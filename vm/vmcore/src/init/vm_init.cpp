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
 * @version $Revision: 1.1.2.4.4.3 $
 */  


#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

#include "classloader.h"
#include "jni_utils.h"
#include "mon_enter_exit.h"
#include "heap.h"

#ifdef PLATFORM_NT
// 20040427 Used to turn on heap checking on every allocation
#include <crtdbg.h>
#endif //PLATFORM_NT

bool vm_is_initialized = false;

void vm_initialize_critical_sections()
{
    p_jit_a_method_lock = new Lock_Manager();
    p_vtable_patch_lock = new Lock_Manager();
    p_meth_addr_table_lock = new Lock_Manager();
    p_handle_lock = new Lock_Manager();

    // 20040224 Support for recording which methods (actually, CodeChunkInfo's) call which other methods.
    p_method_call_lock = new Lock_Manager();
} //vm_initialize_critical_sections

void vm_uninitialize_critical_sections()
{
    delete p_jit_a_method_lock;
    delete p_vtable_patch_lock;
    delete p_meth_addr_table_lock;
    delete p_handle_lock;

    delete p_method_call_lock;
} //vm_uninitialize_critical_sections

Class* preload_class(Global_Env* env, const char* classname)
{
    String* s = env->string_pool.lookup(classname);
    return env->LoadCoreClass(s);
}

Class* preload_class(Global_Env* env, String* s)
{
    return env->LoadCoreClass(s);
}


static Class* preload_primitive_class(Global_Env* env, const char* classname)
{
    String *s = env->string_pool.lookup(classname);
    ClassLoader* cl = env->bootstrap_class_loader;
    Class *clss = cl->NewClass(env, s);
    clss->is_primitive = 1;
    clss->class_loader = cl;
    clss->access_flags = ACC_ABSTRACT | ACC_FINAL | ACC_PUBLIC;
    clss->is_verified = 2;
    cl->InsertClass(clss);

    class_prepare(env, clss);
    return clss;
} //preload_primitive_class




#ifdef LIB_DEPENDENT_OPTS

static Class *class_initialize_by_name(const char *classname)
{
    ASSERT_RAISE_AREA;
    Global_Env* env = VM_Global_State::loader_env;

    String *s = env->string_pool.lookup(classname);
    Class *clss = env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, s);
    if(clss == NULL) {
        DIE("Couldn't load class " << classname);
    } else {
        class_initialize(clss);
    }
    return clss;
} //class_initialize_by_name



void lib_dependent_opts()
{
    ASSERT_RAISE_AREA;
    class_initialize_by_name("java/lang/Math");
} //lib_dependent_opts

#endif // LIB_DEPENDENT_OPTS


// Create the java_lang_Class instance for a struct Class
// and set its "vm_class" field to point back to that structure.
void create_instance_for_class(Global_Env *env, Class *clss) 
{
    clss->class_loader->AllocateAndReportInstance(env, clss);
} //create_instance_for_class


VTable *cached_object_array_vtable_ptr;



static void bootstrap_initial_java_classes(Global_Env *env)
{
    assert(hythread_is_suspend_enabled());
    TRACE2("init", "bootstrapping initial java classes");
    
    /*
     *  Bootstrap java.lang.Class class. This requires also loading the other classes 
     *  it inherits/implements: java.io.Serializable and java.lang.Object, and 
     * j.l.reflect.AnnotatedElement, GenericDeclaration and Type as per Java 5
     */
    env->StartVMBootstrap();
    env->JavaLangObject_Class       = preload_class(env, env->JavaLangObject_String);
    env->java_io_Serializable_Class = preload_class(env, env->Serializable_String);
    env->JavaLangClass_Class        = preload_class(env, env->JavaLangClass_String);
    env->FinishVMBootstrap();

    // Now create the java_lang_Class instance.
    create_instance_for_class(env, env->JavaLangClass_Class);

    ClassTable* table = env->bootstrap_class_loader->GetLoadedClasses();
    unsigned num = 0;
    for (ClassTable::const_iterator it = table->begin(), end = table->end(); 
        it != end; ++it, ++num)
    {
        Class* booted = (*it).second;
        if (booted != env->JavaLangClass_Class) {
            create_instance_for_class(env, booted);
        }
        jvmti_send_class_load_event(env, booted);
        jvmti_send_class_prepare_event(booted);
    }    

  #ifdef VM_STATS
      // Account for the classes loaded before env->JavaLangObject_Class is set.
    env->JavaLangObject_Class->num_allocations += num;
    env->JavaLangObject_Class->num_bytes_allocated += (num * env->JavaLangClass_Class->instance_data_size);
#endif //VM_STATS
    TRACE2("init", "bootstrapping initial java classes complete");
} // bootstrap_initial_java_classes

/** Calls java.lang.ClassLoader.getSystemClassLoader() to obtein system class loader object
 * and pass it to class_loader_set_system_class_loader(..) function
 * @return success status
 */
static bool initialize_system_class_loader(JNIEnv* jenv)
{
    jclass cl = jenv->FindClass("java/lang/ClassLoader");
    if (! cl) 
        return false;

    jmethodID gcl = jenv->GetStaticMethodID(cl, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    if (! gcl) 
        return false;

    jobject scl = jenv->CallStaticObjectMethod(cl, gcl);
    if (exn_raised())
        return false;

    if(scl) {
        tmn_suspend_disable();
        class_loader_set_system_class_loader(ClassLoader::LookupLoader(((ObjectHandle)scl)->object));
        tmn_suspend_enable();
    }

    return true;
} //initialize_system_class_loader

static bool init_thread_object(JNIEnv *);

bool vm_init(Global_Env *env)
{
    ASSERT_RAISE_AREA;

    if(vm_is_initialized)
        return false;
    assert(hythread_is_suspend_enabled());

    vm_is_initialized = true;
    TRACE2("init","Initializing VM");

    vm_monitor_init();

    env->bootstrap_class_loader = new BootstrapClassLoader(env); // !!! use proper MM
    env->bootstrap_class_loader->Initialize();

/////////////// Start bootstrap of initial classes ////////////////

    bootstrap_initial_java_classes(env);

/////////////// End bootstrap of initial classes ////////////////

    TRACE2("init", "preloading primitive type classes");
    env->Boolean_Class = preload_primitive_class(env, "boolean");
    env->Char_Class    = preload_primitive_class(env, "char");
    env->Float_Class   = preload_primitive_class(env, "float");
    env->Double_Class  = preload_primitive_class(env, "double");
    env->Byte_Class    = preload_primitive_class(env, "byte");
    env->Short_Class   = preload_primitive_class(env, "short");
    env->Int_Class     = preload_primitive_class(env, "int");
    env->Long_Class    = preload_primitive_class(env, "long");

    env->Void_Class    = preload_primitive_class(env, "void");

    env->ArrayOfBoolean_Class   = preload_class(env, "[Z");
    env->ArrayOfByte_Class      = preload_class(env, "[B");
    env->ArrayOfChar_Class      = preload_class(env, "[C");
    env->ArrayOfShort_Class     = preload_class(env, "[S");
    env->ArrayOfInt_Class       = preload_class(env, "[I");
    env->ArrayOfLong_Class      = preload_class(env, "[J");
    env->ArrayOfFloat_Class     = preload_class(env, "[F");
    env->ArrayOfDouble_Class    = preload_class(env, "[D");

#ifndef POINTER64
    // In IA32, Arrays of Doubles need to be eight byte aligned to improve 
    // performance. In IPF all objects (arrays, class data structures, heap objects)
    // get aligned on eight byte boundaries. So, this special code is not needed.
    env->ArrayOfDouble_Class->alignment = ((GC_OBJECT_ALIGNMENT<8)?8:GC_OBJECT_ALIGNMENT);
    // The alignment is either 4 or it is a multiple of 8. Things like 12 aren't allowed.
    assert ((GC_OBJECT_ALIGNMENT==4) || ((GC_OBJECT_ALIGNMENT % 8) == 0)); 
    // align doubles on 8, clear alignment field and put in 8.
    set_prop_alignment_mask (env->ArrayOfDouble_Class, 8);
    // Set high bit in size so that gc knows there are constraints
#endif // POINTER64

    TRACE2("init", "preloading string class");
    env->JavaLangString_Class = preload_class(env, env->JavaLangString_String);
    env->strings_are_compressed =
        (class_lookup_field_recursive(env->JavaLangString_Class, "bvalue", "[B") != NULL);
    env->JavaLangString_VTable = env->JavaLangString_Class->vtable;
    env->JavaLangString_allocation_handle = env->JavaLangString_Class->allocation_handle;

    TRACE2("init", "preloading exceptions");
    env->java_lang_Throwable_Class =
        preload_class(env, env->JavaLangThrowable_String);
    env->java_lang_StackTraceElement_Class = 
        preload_class(env, "java/lang/StackTraceElement");
    env->java_lang_Error_Class =
        preload_class(env, "java/lang/Error");
    env->java_lang_ExceptionInInitializerError_Class =
        preload_class(env, "java/lang/ExceptionInInitializerError");
    env->java_lang_NoClassDefFoundError_Class =
        preload_class(env, "java/lang/NoClassDefFoundError");
    env->java_lang_ClassNotFoundException_Class =
        preload_class(env, "java/lang/ClassNotFoundException");
    env->java_lang_NullPointerException_Class =
        preload_class(env, env->JavaLangNullPointerException_String);
    env->java_lang_StackOverflowError_Class =
        preload_class(env, "java/lang/StackOverflowError");
    env->java_lang_ArrayIndexOutOfBoundsException_Class =
        preload_class(env, env->JavaLangArrayIndexOutOfBoundsException_String);
    env->java_lang_ArrayStoreException_Class =
        preload_class(env, "java/lang/ArrayStoreException");
    env->java_lang_ArithmeticException_Class =
        preload_class(env, "java/lang/ArithmeticException");
    env->java_lang_ClassCastException_Class =
        preload_class(env, "java/lang/ClassCastException");
    env->java_lang_OutOfMemoryError_Class = 
        preload_class(env, "java/lang/OutOfMemoryError");

    env->java_lang_OutOfMemoryError = oh_allocate_global_handle();
    env->popFrameException = oh_allocate_global_handle();

    tmn_suspend_disable();
    // precompile StackOverflowError
    class_alloc_new_object_and_run_default_constructor(env->java_lang_StackOverflowError_Class);
    env->java_lang_OutOfMemoryError->object = 
        class_alloc_new_object(env->java_lang_OutOfMemoryError_Class);
    env->popFrameException->object =
        class_alloc_new_object(env->java_lang_Error_Class);
    tmn_suspend_enable();

    env->java_lang_Cloneable_Class =
        preload_class(env, env->Clonable_String);
    env->java_lang_Thread_Class =
        preload_class(env, "java/lang/Thread");
    env->java_lang_ThreadGroup_Class =
        preload_class(env, "java/lang/ThreadGroup");
    env->java_util_Date_Class = 
        preload_class(env, "java/util/Date");
    env->java_util_Properties_Class = 
        preload_class(env, "java/util/Properties");
    env->java_lang_Runtime_Class = 
        preload_class(env, "java/lang/Runtime");

    env->java_lang_reflect_Constructor_Class = 
        preload_class(env, env->JavaLangReflectConstructor_String);
    env->java_lang_reflect_Field_Class = 
        preload_class(env, env->JavaLangReflectField_String);
    env->java_lang_reflect_Method_Class = 
        preload_class(env, env->JavaLangReflectMethod_String);

    Method *m = class_lookup_method(env->java_lang_Throwable_Class, 
        env->Init_String, env->VoidVoidDescriptor_String);
    assert(m);
    assert(hythread_is_suspend_enabled());
    m->set_side_effects(MSE_False);

    m = class_lookup_method(env->java_lang_Throwable_Class,
        env->Init_String, env->FromStringConstructorDescriptor_String);
    assert(m);
    m->set_side_effects(MSE_False);


    void global_object_handles_init();
    global_object_handles_init();
    Class *aoObjectArray = preload_class(env, "[Ljava/lang/Object;");
    cached_object_array_vtable_ptr = aoObjectArray->vtable;

    // the following is required for creating exceptions
    preload_class(env, "[Ljava/lang/VMClassRegistry;");
    extern unsigned resolve_const_pool(Global_Env& env, Class *clss);
    unsigned fail_idx = resolve_const_pool(*env, env->java_lang_Throwable_Class);
    if(fail_idx != 0xFFFFFFFF)
    {
        WARN("Failed to resolve class java/lang/Throwable");
        return false;
    }

    // We assume, that at this point VM supports exception objects creation.
    env->ReadyForExceptions();

    TRACE2("init", "initializing thread group");
    assert(hythread_is_suspend_enabled());

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
    if (! init_thread_object(jni_env))
        return false;


    TRACE2("init", "Invoking the java.lang.Class constructor");
    Class *jlc = env->JavaLangClass_Class;
    jobject jlo = struct_Class_to_java_lang_Class_Handle(jlc);

    jmethodID java_lang_class_init = GetMethodID(jni_env, jlo, "<init>", "()V");
    jvalue args[1];
    args[0].l = jlo;
    tmn_suspend_disable();
    vm_execute_java_method_array(java_lang_class_init, 0, args);

    assert(!exn_raised());

    void unsafe_global_object_handles_init();
    unsafe_global_object_handles_init();

    tmn_suspend_enable();

    if (vm_get_boolean_property_value_with_default("vm.finalize")) {
        // load and initialize finalizer thread
        env->finalizer_thread = preload_class(env, "java/lang/FinalizerThread");
        assert(env->finalizer_thread);

        Field* finalizer_shutdown_field = class_lookup_field_recursive(env->finalizer_thread, 
                "shutdown", "Z");
        Field* finalizer_on_exit_field = class_lookup_field_recursive(env->finalizer_thread, 
                "onExit", "Z");
        assert(finalizer_shutdown_field);
        assert(finalizer_on_exit_field);
        env->finalizer_shutdown = (jboolean*) finalizer_shutdown_field->get_address();
        env->finalizer_on_exit = (jboolean*) finalizer_on_exit_field->get_address();
        assert(env->finalizer_shutdown);
        assert(env->finalizer_on_exit);
        class_initialize_from_jni(env->finalizer_thread);
    } else {
        env->finalizer_thread = NULL;
    }

    TRACE2("init", "initialization of system classes completed");

#ifdef WIN32
    // Code to start up Networking on Win32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err; 
    wVersionRequested = MAKEWORD( 2, 2 ); 
    err = WSAStartup( wVersionRequested, &wsaData );

    if ( err != 0 ) {
        // Tell the user that we could not find a usable WinSock DLL.                                      
        WARN("Couldn't startup Winsock 2.0 dll ");
    }
#endif // WIN32

#ifdef LIB_DEPENDENT_OPTS
    lib_dependent_opts();
#endif

    TRACE2("init", "initializing system class loader");
    //XXX NativeObjectHandles lhs;
    bool res = initialize_system_class_loader(jni_env);
    if(!res) {
        WARN("Fail to initialize system class loader.");
    }
    if(exn_raised()) {
        print_uncaught_exception_message(stderr, 
                "system class loader initialisation", exn_get());
    }
    exn_clear(); // Ignore any exception that might have occured
    TRACE2("init", "system class loader initialized");

    jvmti_send_vm_start_event(env, jni_env);

    assert(!exn_raised());
    TRACE2("init", "VM initialization completed");

    return true;
} //vm_init

static bool init_thread_object(JNIEnv *jenv)
{
    Global_Env *env = VM_Global_State::loader_env;

    assert(hythread_is_suspend_enabled());

    // Load, prepare and initialize the "Thread class"
    String *ss = env->string_pool.lookup("java/lang/VMStart");
    Class *thread_clss = env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, ss);
    assert(thread_clss);
    assert(hythread_is_suspend_enabled());
    tmn_suspend_disable();
    class_initialize(thread_clss);
    assert(!hythread_is_suspend_enabled());

    ObjectHandle jThreadClass = oh_allocate_local_handle();
    jThreadClass->object = struct_Class_to_java_lang_Class(thread_clss);
    tmn_suspend_enable();

    jmethodID main_method = jenv->GetStaticMethodID(jThreadClass, "mainThreadInit", "()V");
    if (ExceptionOccurred(jenv) || !main_method) {
        WARN("*** Error: exception occured in main Thread constructor.");
        ExceptionDescribe(jenv);
        ExceptionClear(jenv);
        return false;
    }

    jenv->CallStaticVoidMethod(jThreadClass, main_method);
    
    if (ExceptionOccurred(jenv)) {
        WARN("*** Error: exception occured in main Thread constructor.");
        ExceptionDescribe(jenv);
        ExceptionClear(jenv);
        return false;
    }

    return true;
} //init_thread_object
