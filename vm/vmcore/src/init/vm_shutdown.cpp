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
#include "open/gc.h"

#include "jni.h"
#include "jni_direct.h"
#include "environment.h"
#include "classloader.h"
#include "compile.h"
#include "nogc.h"
#include "jni_utils.h"
#include "vm_stats.h"
#include "thread_dump.h"
#include "interpreter.h"

#define LOG_DOMAIN "vm.core.shutdown"
#include "cxxlog.h"

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
 * Calls java.lang.System.execShutdownSequence() method.
 *
 * @param jni_env JNI environment of the current thread
 */
static jint exec_shutdown_sequence(JNIEnv * jni_env) {
    jclass system_class;
    jmethodID shutdown_method;

    assert(hythread_is_suspend_enabled());

    system_class = jni_env->FindClass("java/lang/System");
    if (jni_env->ExceptionCheck() == JNI_TRUE || system_class == NULL) {
        // This is debug message only. May appear when VM is already in shutdown stage.
        PROCESS_EXCEPTION("can't find java.lang.System class.");
    }
    
    shutdown_method = jni_env->GetStaticMethodID(system_class, "execShutdownSequence", "()V");
    if (jni_env->ExceptionCheck() == JNI_TRUE || shutdown_method == NULL) {
        PROCESS_EXCEPTION("can't find java.lang.System.execShutdownSequence() method.");
    }

    jni_env->CallStaticVoidMethod(system_class, shutdown_method);

    if (jni_env->ExceptionCheck() == JNI_TRUE) {
        PROCESS_EXCEPTION("java.lang.System.execShutdownSequence() method completed with an exception.");
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
    jobject uncaught_exception;
    VM_thread * vm_thread;
    hythread_t native_thread;
    hythread_t current_native_thread;
    hythread_iterator_t it;

    assert(hythread_is_suspend_enabled());

    jni_env = jthread_get_JNI_env(java_thread);
    current_native_thread = hythread_self();

    status = jthread_wait_for_all_nondaemon_threads();
    if (status != TM_ERROR_NONE) {
        TRACE("Failed to wait for all non-daemon threads completion.");
        return JNI_ERR;
    }

    // Print out gathered data.
#ifdef VM_STATS
    ClassLoader::PrintUnloadingStats();
    VM_Statistics::get_vm_stats().print();
#endif

    // Remember thread's uncaught exception if any.
    uncaught_exception = jni_env->ExceptionOccurred();
    jni_env->ExceptionClear();

    // Execute pinding shutdown hooks & finalizers
    status = exec_shutdown_sequence(jni_env);
    if (status != JNI_OK) return status;

    // Send VM_Death event and switch to VM_Death phase.
    // This should be the last event sent by VM.
    // This event should be sent before Agent_OnUnload called.
    jvmti_send_vm_death_event();

    // Raise uncaught exception to current thread.
    // It will be properly processed in jthread_detach().
    if (uncaught_exception) {
        exn_raise_object(uncaught_exception);
    }

    // Detach current thread.
    status = jthread_detach(java_thread);
    if (status != TM_ERROR_NONE) return JNI_ERR;

    // Call Agent_OnUnload() for agents and unload agents.
    java_vm->vm_env->TI->Shutdown(java_vm);

    // Stop all (except current) java and native threads
    // before destroying VM-wide data.

    // TODO: current implementation doesn't stop java threads :-(
    // So, don't perform cleanup if there are running java threads.
    it = hythread_iterator_create(NULL);
    while (native_thread = hythread_iterator_next(&it)) {
        vm_thread = get_vm_thread(native_thread);
        if (vm_thread != NULL && native_thread != current_native_thread) {
            add_pair_to_properties(*java_vm->vm_env->properties, "vm.noCleanupOnExit", "true");
            hythread_iterator_release(&it);
            return JNI_ERR;
        }
    }
    hythread_iterator_release(&it);

    // TODO: ups we don't stop native threads as well :-((
    // We are lucky! Currently, there are no such threads.

    return JNI_OK;
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
    if (interpreter_enabled()) {
            dump_all_java_stacks();
    } else {
            td_dump_all_threads(stderr); 
    }
}

void interrupt_handler(int UNREF x)
{
    static bool begin_shutdown_hooks = false;

    if(!begin_shutdown_hooks){
        begin_shutdown_hooks = true;
        //FIXME: integration should do int another way.
        //vm_set_event(non_daemon_threads_dead_handle);
    }else
        exit(1);
}
