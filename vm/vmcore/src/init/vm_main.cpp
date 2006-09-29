/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/** 
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.5.4.3 $
 */  

#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

#include "thread_generic.h"
#include "open/vm_util.h"
#include "open/hythread_ext.h"
#include "properties.h"
#include <apr_env.h>
#include "vm_synch.h"
#include "init.h"
#include "compile.h"
#include "method_lookup.h"
#include "nogc.h"
#include "classloader.h"
#include "thread_dump.h"
#include "interpreter.h"
#include "verify_stack_enumeration.h"

// Multiple-JIT support.
#include "component_manager.h"
#include "jit_intf.h"
#include "dll_jit_intf.h"
#include "dll_gc.h"
#include "em_intf.h"
#include "port_filepath.h"

union Scalar_Arg {
    int i;
    unsigned u;
    void *p;
};

#include "m2n.h"
// FIXME Alexei
// There should be no need to expose internal m2n strucutre here
#ifdef _IPF_
#include "../m2n_ipf_internal.h"
#elif defined _EM64T_
#include "../m2n_em64t_internal.h"
#else
#include "../m2n_ia32_internal.h"
#endif

static StaticInitializer vm_initializer;
tl::MemoryPool m;
Properties properties;
Global_Env env(m, properties);

#include "jarfile_util.h"
bool runjarfile = false;

VMEXPORT bool instrumenting = false;

VMEXPORT bool vtune_support = false;

VMEXPORT bool dump_stubs = false;

bool parallel_jit = true;

static bool begin_shutdown_hooks = false;

static M2nFrame* p_m2n = NULL;

typedef union {
    ObjectHandlesOld old;
    ObjectHandlesNew nw;
} HandlesUnion;

static HandlesUnion* p_handles = NULL;

hythread_library_t hythread_lib;

static void initialize_javahome(Global_Env* p_env)
{
    PropertiesHandle ph = (PropertiesHandle)&p_env->properties;
    // Determine java home
    const char* jh = properties_get_string_property(ph, "java.home");
    if (!jh) {
        if (apr_env_get((char**)&jh, "JAVA_HOME", 0) != APR_SUCCESS) {
            DIE("Failed to determine Java home directory");
        }
        p_env->properties.add(strdup("java.home"), new Prop_String(strdup(jh)));
    }
} //initialize_javahome

/**
 * Must be called after vm_arguments parsing.
 */
static void initialize_classpath(Global_Env* p_env)
{
    Properties* props = &p_env->properties;
    apr_pool_t *pool;
    apr_pool_create(&pool, 0);

    const char* class_path = properties_get_string_property((PropertiesHandle)props, "java.class.path");

    // if classpath was not defined by cmd line arguments, check environment
    if (!class_path) 
    {
        apr_env_get((char**) &class_path, "CLASSPATH", pool);
        if (class_path)
            TRACE("No classpath specified, using \"CLASSPATH\" environment setting");
    }

    // if classpath not defined or empty string, use dafault "." value
    if (!class_path || !class_path[0]) {
        class_path = ".";
    }

    add_pair_to_properties(*props, "java.class.path", class_path);
    apr_pool_destroy(pool);
} //initialize_classpath

static void process_properties_dlls(Global_Env* p_env)
{
    post_initialize_ee_dlls((PropertiesHandle)&p_env->properties);

    const char* dll = properties_get_string_property((PropertiesHandle)&p_env->properties, "vm.em_dll");
    TRACE2("init", "analyzing em dll " << dll);

    int ret = CmLoadComponent(dll, "EmInitialize");
    if (JNI_OK != ret) {
        WARN("Cannot load EM component from " << dll << ", error " << ret);
        LOGGER_EXIT(1);
    }

    ret = p_env->cm->CreateInstance(&(p_env->em_instance), "em");
    if (JNI_OK != ret) {
        WARN("Cannot instantiate EM, error " << ret);
        LOGGER_EXIT(1);
    }

    ret = p_env->em_instance->intf->GetInterface((OpenInterfaceHandle*) &(p_env->em_interface),
        OPEN_INTF_EM_VM);
    if (JNI_OK != ret) {
        WARN("Cannot get EM_VM interface, error " << ret);
        LOGGER_EXIT(1);
    }

    const char* dlls = properties_get_string_property((PropertiesHandle)&p_env->properties, "vm.dlls");
    if (!dlls) {
        return;
    }
    dlls = strdup(dlls);
    assert(dlls);
    static const char delimiters[] = { PORT_PATH_SEPARATOR, 0 };
    char* tok = strtok((char*)dlls, delimiters);
    while (tok) {
        TRACE2("init", "analyzing dll " << tok);
#ifndef USE_GC_STATIC
        if (vm_is_a_gc_dll(tok))
        {
            vm_add_gc(tok);
        }
#endif // !USE_GC_STATIC
#ifdef USE_DISEM
        else if (vm_is_a_disem_dll(tok)) {
            vm_add_disem(tok);
        }
#endif
        else
        {
            WARN("Mandatory library cannot be loaded: " << tok);
            LOGGER_EXIT(1);
        }
        tok = strtok(NULL, delimiters);
    }
    STD_FREE((void*)dlls);

} //process_properties_dlls

#define PROCESS_EXCEPTION(message) \
{ \
    ECHO("Error occured while running starter class: " << message); \
\
    if (jenv->ExceptionOccurred()) \
    { \
        jenv->ExceptionDescribe(); \
        jenv->ExceptionClear(); \
    } \
\
    return 1; \
} \

static int run_java_main(char *class_name, char **java_args, int java_args_num)
{
    assert(hythread_is_suspend_enabled());

    JNIEnv* jenv = (JNIEnv*) jni_native_intf;

    jclass start_class = jenv->FindClass("java/lang/VMStart");
    if (jenv->ExceptionOccurred() || !start_class)
        PROCESS_EXCEPTION("can't find starter class: java/lang/VMStart");

    jmethodID main_method = jenv->GetStaticMethodID(start_class, "start", "(Ljava/lang/String;[Ljava/lang/String;)V");
    if (jenv->ExceptionOccurred() || !main_method)
        PROCESS_EXCEPTION("can't find start method in class java/lang/VMStart");
  
    jclass string_class = jenv->FindClass("java/lang/String");
    if (jenv->ExceptionOccurred() || !string_class)
        PROCESS_EXCEPTION("can't find java.lang.String class");
    
    jarray args = jenv->NewObjectArray(java_args_num, string_class, NULL);
    if (jenv->ExceptionOccurred() || !args)
        PROCESS_EXCEPTION("can't create arguments array: java.lang.String[" << java_args_num <<"]");

    for (int i = 0; i < java_args_num; i++)
    {
        jstring arg = jenv->NewStringUTF(java_args[i]);
        if (jenv->ExceptionOccurred() || !arg)
            PROCESS_EXCEPTION("can't create java.lang.String object for \"" << java_args[i] << "\"");
    
        jenv->SetObjectArrayElement(args, i, arg);
        if (jenv->ExceptionOccurred())
            PROCESS_EXCEPTION("can't set array element for index " << i);
    }

    //create Main class name string
    jstring jclassname = jenv->NewStringUTF(class_name);

    jenv->CallStaticVoidMethod(start_class, main_method, jclassname, args);
    if (jenv->ExceptionOccurred())
        PROCESS_EXCEPTION("error during start(...) method execution");

    return 0;
} //run_java_main

static int run_java_init()
{
    assert(hythread_is_suspend_enabled());

    JNIEnv* jenv = (JNIEnv*) jni_native_intf;

    jclass start_class = jenv->FindClass("java/lang/VMStart");
    if (jenv->ExceptionOccurred() || !start_class)
        PROCESS_EXCEPTION("can't find starter class: java/lang/VMStart");

    jmethodID init_method = jenv->GetStaticMethodID(start_class, "initialize", "()V");
    if (jenv->ExceptionOccurred() || !init_method)
        PROCESS_EXCEPTION("can't find initialize method in class java/lang/VMStart");

    jenv->CallStaticVoidMethod(start_class, init_method);
    if (jenv->ExceptionOccurred())
        PROCESS_EXCEPTION("error during initialize() method execution");

    return 0;
} //run_java_init

static int run_java_shutdown()
{
    assert(hythread_is_suspend_enabled());

    JNIEnv* jenv = (JNIEnv*) jni_native_intf;

    /*
     * Make shutdown resistant to possible exceptions left in JNI code
     */
    if (jenv->ExceptionOccurred()) {
        PROCESS_EXCEPTION("Exception left unhandled before destroying VM");
    }

    jclass start_class = jenv->FindClass("java/lang/VMStart");
    if (jenv->ExceptionOccurred() || !start_class) {
        PROCESS_EXCEPTION("can't find starter class: java/lang/VMStart");
    }

    jmethodID shutdown_method = jenv->GetStaticMethodID(start_class, "shutdown", "()V");
    if (jenv->ExceptionOccurred() || !shutdown_method) {
        PROCESS_EXCEPTION("can't find initialize method in class java/lang/VMStart");
    }

    jenv->CallStaticVoidMethod(start_class, shutdown_method);
    if (jenv->ExceptionOccurred()) {
        PROCESS_EXCEPTION("error during shutdown() method execution");
    }
    
    return 0;
} //run_java_shutdown

void create_vm(Global_Env *p_env, JavaVMInitArgs* vm_arguments) 
{
    
#if defined(PLATFORM_NT)
    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    BOOL ok = GetVersionEx(&osvi);
    if(!ok) {
        DWORD e = GetLastError();
        printf("Windows error: %d\n", e);
        exit(1);
    }
    if((osvi.dwMajorVersion != 4 || osvi.dwMinorVersion != 0) &&  // NT 4.0
       (osvi.dwMajorVersion != 5 || osvi.dwMinorVersion != 0) &&  // Windows 2000
       (osvi.dwMajorVersion != 5 || osvi.dwMinorVersion != 1) &&  // Windows XP
       (osvi.dwMajorVersion != 5 || osvi.dwMinorVersion != 2)) {  // Windows.NET
        printf("Windows %d.%d is not supported\n", osvi.dwMajorVersion, osvi.dwMinorVersion);
        exit(1);
    }
#endif
    hythread_init(hythread_lib);
    vm_thread_init(&env);

    VM_Global_State::loader_env = &env;
    hythread_attach(NULL);
    // should be removed
    vm_thread_attach();
    MARK_STACK_END

    p_env->TI = new DebugUtilsTI;

    assert(hythread_is_suspend_enabled());

    vm_initialize_critical_sections();
    initialize_vm_cmd_state(p_env, vm_arguments); //PASS vm_arguments to p_env->vm_arguments and free vm_arguments 
    set_log_levels_from_cmd(&p_env->vm_arguments);

    if (JNI_OK != CmAcquire(&p_env->cm)) {
        WARN("Cannot initialize a component manager");
        LOGGER_EXIT(1);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    // Begin property processing
    //
    // 20030407 Note: property initialization must follow initialization of the default JITs to allow 
    // the command line to override those default JITs.

    initialize_properties(p_env, env.properties);

    // Enable interpreter by default if TI should be enabled
    p_env->TI->setExecutionMode(p_env);

    parse_vm_arguments(p_env);

    process_properties_dlls(p_env);
    VERIFY(p_env->em_instance != NULL, "EM init failed");
 
    parse_jit_arguments(&p_env->vm_arguments);

     // The following must go after getting propertes and parsing arguments
    initialize_classpath(p_env);
    initialize_javahome(p_env);

    VM_Global_State::loader_env->pin_interned_strings = 
        (bool)vm_get_property_value_boolean("vm.pin_interned_strings", FALSE);

    initialize_verify_stack_enumeration();

    //
    // End property processing
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS
    if (!vm_get_boolean_property_value_with_default("vm.assert_dialog"))
    {
        TRACE2("init", "disabling assertion dialogs");
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
        _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDOUT);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
        _set_error_mode(_OUT_TO_STDERR);
    }
#endif // _WINDOWS

    // Initialize the VM's sorted list of code chunks (sorted by starting code address)
    void *p = p_env->mem_pool.alloc(sizeof(Method_Lookup_Table));
    vm_methods = new (p) Method_Lookup_Table();

    // Initialize memory allocation
    vm_init_mem_alloc();
    gc_init();
   // gc_thread_init(&p_TLS_vmthread->_gc_private_information);

    // Prepares to load natives
    bool UNREF status = natives_init();
    assert( status );
    // Preloading system native libraries
    // initialize_natives((PropertiesHandle)&env.properties);

    jni_init();
    /*
     * Valentin Al. Sitnick
     * Initialisation of TI.
     */
    if (0 != p_env->TI->Init()) {
        WARN("JVMTI initialization failed. Exiting.");
        p_env->TI->setDisabled();
        vm_exit(1);
    }

    // Moved after Method_Lookup_Table initialization
    extern void initialize_signals();
    initialize_signals();

    Class::heap_base = (Byte *)gc_heap_base_address();
    Class::heap_end  = (Byte *)gc_heap_ceiling_address();
    Class::managed_null = (vm_references_are_compressed() ? Class::heap_base : NULL);

    // 20030404 This handshaking protocol isn't quite correct. It doesn't work at the moment because JIT has not yet been
    // modified to support compressed references, so it never answers "true" to supports_compressed_references().

    // Check for a mismatch between whether the various VM components all compress references or not.
    Boolean vm_compression = vm_references_are_compressed();
    Boolean gc_compression = gc_supports_compressed_references();
    if (vm_compression) {
        if (!gc_compression) {
            WARN("VM component mismatch: the VM compresses references but the GC doesn't.");
            vm_exit(1);
        }
        
        // We actually check the first element in the jit_compilers array, as current JIT
        // always returns FALSE to the supports_compressed_references() call. 
        JIT **jit = &jit_compilers[0];
        if (!interpreter_enabled()) {
            Boolean jit_compression = (*jit)->supports_compressed_references();
            if (!jit_compression) {
                WARN("VM component mismatch: the VM compresses references but a JIT doesn't");
                vm_exit(1);
            }
        }
    } else {
        if (gc_compression) {
            WARN("VM component mismatch: the VM doesn't compress references but the GC does.");
            vm_exit(1);
        }
        JIT **jit = &jit_compilers[0];
        if (!interpreter_enabled()) {
            Boolean jit_compression = (*jit)->supports_compressed_references();
            if (jit_compression) {
                WARN("VM component mismatch: the VM doesn't compress references but a JIT does");
                vm_exit(1);
            }
        }
    }

    // create first m2n frame
    assert(hythread_is_suspend_enabled());
    tmn_suspend_disable();
    p_m2n = (M2nFrame*) p_env->mem_pool.alloc(sizeof(M2nFrame));
    p_handles = (HandlesUnion*) p_env->mem_pool.alloc(sizeof(HandlesUnion));

    m2n_null_init(p_m2n);
    m2n_set_last_frame(p_m2n);

    oh_null_init_handles((ObjectHandles*)p_handles);
    m2n_set_local_handles(p_m2n, (ObjectHandles*)p_handles);
    m2n_set_frame_type(p_m2n, FRAME_NON_UNWINDABLE);
    tmn_suspend_enable();
 
    if (! vm_init(p_env))
    {
        WARN("Failed to initialize VM.");
        vm_exit(1);
    }

    // Send VM init event
    jvmti_send_vm_init_event(p_env);


    int result = run_java_init();

    if (result != 0)
    {
        WARN("Failed execute starter class initialize() method.");
        vm_exit(1);
    }
} //create_vm

static int run_main(Global_Env *p_env, char* class_name, char* jar_file,
                    int java_args_num, char* java_args[])
{
    if (jar_file)
    {
        // check if file is archive
        if(!file_is_archive(jar_file)) {
            ECHO("You must specify a jar file to run.");
            LOGGER_EXIT(1);
        }

        // create archive file structure
        void *mem_jar = STD_ALLOCA(sizeof(JarFile));
        JarFile* jarfl = new (mem_jar) JarFile();
        if(!jarfl || !(jarfl->Parse(jar_file)) ) {
            ECHO("VM can't find the jar file you want to run.");
            LOGGER_EXIT(1);
        }

        // extract main class name from jar's manifest
        class_name = strdup(archive_get_main_class_name(jarfl));
        if(!class_name) {
            ECHO("Your jar file hasn't specified Main-Class manifest attribute.");
            LOGGER_EXIT(1);
        }

        // close archive file
        jarfl->~JarFile();

    } else if (class_name) {
        // convert class name: change '.' to '/'
        for (char* pointer = class_name; *pointer; pointer++) {
            if (*pointer == '/') {
                *pointer = '.';
            }
        }
    }

    // if class_name unknown print help and exit
    if (!class_name) {
        p_env->TI->setDisabled();
        print_generic_help();
        vm_exit(1);
    }

    // run a given class
    int result = run_java_main(class_name, java_args, java_args_num);

    return result;
} //run_main

void destroy_vm(Global_Env *p_env) 
{

    run_java_shutdown();

    // usually shutdown hooks do vm_exit().
    // so we do not reach this point
    // but in case ...

    WARN("Error occured in starter class shutdown() method.");
    vm_exit(-1);
} //destroy_vm


VMEXPORT int vm_main(int argc, char *argv[]) 
{
    init_log_system();

    char** java_args;
    int java_args_num;
    char* class_name;
    char* jar_file;

    JavaVMInitArgs* vm_arguments = 
        parse_cmd_arguments(argc, argv, &class_name, &jar_file, &java_args_num);

    java_args = argv + argc - java_args_num;

    create_vm(&env, vm_arguments);

    clear_vm_arguments(vm_arguments);

    run_main(&env, class_name, jar_file, java_args_num, java_args);

    destroy_vm(&env);

    return 33;
} //vm_main

static inline
void dump_all_java_stacks() {
    hythread_iterator_t  iterator;
    hythread_suspend_all(&iterator, NULL);
    VM_thread *thread = get_vm_thread (hythread_iterator_next(&iterator));
    while(thread) {
        interpreter.stack_dump(thread);
        thread = get_vm_thread (hythread_iterator_next(&iterator));
    }
    hythread_resume_all( NULL);
    INFO("****** END OF JAVA STACKS *****\n");
}

void quit_handler(int UNREF x) {
    if (VM_Global_State::loader_env->shutting_down != 0) {
        // too late for quit handler
        // required infrastructure can be missing.
        fprintf(stderr, "quit_handler(): called in shut down stage\n");
        return;
    }

    if (interpreter_enabled()) {
            dump_all_java_stacks();
    } else {
            td_dump_all_threads(stderr); 
    }
}

void interrupt_handler(int UNREF x)
{
    if (VM_Global_State::loader_env->shutting_down != 0) {
        // too late for quit handler
        // required infrastructure can be missing.
        fprintf(stderr, "interrupt_handler(): called in shutdown stage\n");
        return;
    }

    if(!begin_shutdown_hooks){
        begin_shutdown_hooks = true;
        //FIXME: integration should do int another way.
        //vm_set_event(non_daemon_threads_dead_handle);
    }else
        exit(1); //vm_exit(1);
}

JIT_Handle vm_load_jit(const char* file_name, apr_dso_handle_t** handle) {
    //if (vm_is_a_jit_dll(file_name)) {
        Dll_JIT* jit = new Dll_JIT(file_name);
        handle[0]=jit->get_lib_handle();
        vm_add_jit(jit);
        return (JIT_Handle)jit;
        
    /*}
    printf("not a jit\n");
    handle[0]=NULL;
    return 0;*/
}
