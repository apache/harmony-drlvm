/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1 $
 */  

#include <stdlib.h>
#include <apr_thread_mutex.h>

#include "open/hythread.h"
#include "open/jthread.h"

#include "jni.h"
#include "jni_direct.h"
#include "environment.h"
#include "classloader.h"
#include "compile.h"
#include "component_manager.h"
#include "nogc.h"
#include "jni_utils.h"
#include "vm_synch.h"
#include "vm_stats.h"
#include "thread_dump.h"
#include "interpreter.h"

#define LOG_DOMAIN "vm.core.shutdown"
#include "cxxlog.h"


/**
 * TODO:
 */
void vm_cleanup_internal_data() {
    // Print out gathered data.
#ifdef VM_STATS
    ClassLoader::PrintUnloadingStats();
    VM_Statistics::get_vm_stats().print();
#endif

    // Unload jit instances.
    vm_delete_all_jits();

    // Unload component manager and all registered components.
    // TODO: why do we need to unload "em" explicitly?
    CmFreeComponent("em");
    CmRelease();

    // Unload all system native libraries.
    natives_cleanup();

    // TODO: it seems we don't need to do it!!! At least here!!!
    gc_wrapup();

    // Release global data.
    // TODO: move these data to VM space.
    vm_uninitialize_critical_sections();
    vm_mem_dealloc();
}

/**
 * TODO:
 */
void vm_exit(int exit_code)
{
    jthread java_thread;
    JNIEnv * jni_env;
    Global_Env * vm_env;

    assert(hythread_is_suspend_enabled());

    java_thread = jthread_self();
    jni_env = jthread_get_JNI_env(java_thread);
    vm_env = jni_get_vm_env(jni_env);

    // Send VM_Death event and switch phase to VM_Death
    jvmti_send_vm_death_event();

    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME *
     * gregory - JVMTI shutdown should be part of DestroyVM after current VM shutdown      *
     * problems are fixed                                                                  *
     * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
    // call Agent_OnUnload() for agents and unload agents
    vm_env->TI->Shutdown();

    // Raise uncaught exception to current thread.
    // It will be properly processed in jthread_detach().
    if (vm_env->uncaught_exception) {
        exn_raise_object(vm_env->uncaught_exception);
    }

    // Detach current thread.
    IDATA UNREF status = jthread_detach(java_thread);
    assert(status == TM_ERROR_NONE);

    // Cleanup internal data. Disabled by default due to violation access exception.
    if (vm_get_boolean_property_value_with_default("vm.cleanupOnExit")) {
        jthread_cancel_all();
        vm_cleanup_internal_data();
        LOGGER_EXIT(exit_code);
    }

    _exit(exit_code);
}

#define PROCESS_EXCEPTION(message) \
{ \
    ECHO("Internal error: " << message); \
\
    if (jni_env->ExceptionCheck()== JNI_TRUE) \
    { \
        jni_env->ExceptionDescribe(); \
        jni_env->ExceptionClear(); \
    } \
\
    return JNI_ERR; \
} \

/**
 * Calls java.lang.System.exit() method.
 * Current thread should be attached to VM.
 *
 * @param exit_status VM exit code
 */
static jint run_system_exit(JNIEnv * jni_env, jint exit_status) {
    jvalue args[1];

    assert(hythread_is_suspend_enabled());

    args[0].i = exit_status;

    jclass system_class = jni_env->FindClass("java/lang/System");
    if (jni_env->ExceptionCheck() == JNI_TRUE || system_class == NULL) {
        // This is debug message only. May appear when VM is already in shutdown stage.
        PROCESS_EXCEPTION("can't find java.lang.System class.");
    }
    
    jmethodID exit_method = jni_env->GetStaticMethodID(system_class, "exit", "(I)V");
    if (jni_env->ExceptionCheck() == JNI_TRUE || exit_method == NULL) {
        PROCESS_EXCEPTION("can't find java.lang.System.exit(int) method.");
    }

    jni_env->CallStaticVoidMethodA(system_class, exit_method, args);

    if (jni_env->ExceptionCheck() == JNI_TRUE) {
        PROCESS_EXCEPTION("java.lang.System.exit(int) method completed with an exception.");
    }
    return JNI_OK;
}

/**
 * TODO:
 */
jint vm_destroy(JavaVM_Internal * java_vm, jthread java_thread)
{
    jint status;
    JNIEnv * jni_env;

    assert(hythread_is_suspend_enabled());

    jni_env = jthread_get_JNI_env(java_thread);

    status = jthread_wait_for_all_nondaemon_threads();
    if (status != TM_ERROR_NONE) {
        TRACE("Failed to wait for all non-daemon threads completion.");
        return JNI_ERR;
    }

    // Remember thread's uncaught exception if any.
    java_vm->vm_env->uncaught_exception = jni_env->ExceptionOccurred();
    jni_env->ExceptionClear();

    status = java_vm->vm_env->uncaught_exception  == NULL ? 0 : 1;

    return run_system_exit(jni_env, status);
}

static inline void dump_all_java_stacks()
{
    hythread_t native_thread;
    hythread_iterator_t  iterator;
    VM_thread * vm_thread;

    INFO("****** BEGIN OF JAVA STACKS *****\n");

    hythread_suspend_all(&iterator, NULL);
    native_thread ;
    while(native_thread = hythread_iterator_next(&iterator)) {
        vm_thread = get_vm_thread(native_thread);
        assert(vm_thread);
        interpreter.stack_dump(vm_thread);
    }
    hythread_resume_all(NULL);
    
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
    static bool begin_shutdown_hooks = false;
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
