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
#include "open/thread_externals.h"

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

static void vm_shutdown_callback() {
    hythread_t   self_native;
    jthread      self_java;
    VM_thread *  self_vm;
    JNIEnv     * jni_env;
    Global_Env * vm_env;
        
    self_native = hythread_self();
    self_java = jthread_get_java_thread(self_native);
    self_vm = get_vm_thread(self_native);

    // Make sure the thread is still attached to the VM.
    if (self_java == NULL || self_vm == NULL) {
        return;
    }
        
    jni_env = jthread_get_JNI_env(self_java);
    vm_env = jni_get_vm_env(jni_env);

    // We need to be in suspend disabled state to be able to throw an exception.
    assert(!hythread_is_suspend_enabled());

    //hythread_suspend_disable();

    // Raise an exception. In shutdown stage it should cause entire stack unwinding.
    jthread_throw_exception_object(vm_env->java_lang_ThreadDeath);

    //hythread_suspend_enable();
}

/**
 * Stops running java threads by throwing an exception
 * up to the first native frame.
 *
 * @param[in] vm_env a global VM environment
 */
static void vm_shutdown_stop_java_threads(Global_Env * vm_env) {
    hythread_t self;
    hythread_t * running_threads;
    hythread_t native_thread;
    hythread_iterator_t it;
    VM_thread * vm_thread;
    int size;
    bool changed;
    int left;
    int prev;
   
    self = hythread_self();

    // Collect running java threads.
    size = 0;
    it = hythread_iterator_create(NULL);
    running_threads = (hythread_t *)apr_palloc(vm_env->mem_pool,
        hythread_iterator_size(it) * sizeof(hythread_t));
    while(native_thread = hythread_iterator_next(&it)) {
        vm_thread = get_vm_thread(native_thread);
        if (vm_thread != NULL && native_thread != self) {
            running_threads[size] = native_thread;
            ++size;
        }
    }
    hythread_iterator_release(&it);

    // Interrupt running threads.
    for (int i = 0; i < size; i++) {
        hythread_interrupt(running_threads[i]);
    }

    left = size;
    prev = -1;
    // Spin until there is no running threads.
    while (left > 0) {
        do {
            left = 0;
            changed = false;
            // Join or Interrupt.
            for (int i = 0; i < size; i++) {
                if (running_threads[i] == NULL) continue;
                if (hythread_join_timed(running_threads[i], 100, 0) == TM_ERROR_NONE) {
                    changed = true;
                    running_threads[i] = NULL;
                } else {
                    hythread_interrupt(running_threads[i]);
                    ++left;
                }
            }
        } while(left > 0 && changed);

        // We are stuck :-( Need to throw asynchronous exception.
        if (left > 0) {
            for (int i = 0; i < size; i++) {
                if (running_threads[i] == NULL) continue;
                if (left > 1 && i == prev) { 
                    continue;
                } else {
                    prev = i;
                    hythread_set_safepoint_callback(running_threads[i], vm_shutdown_callback);
                }
            }
        }
    }               
}

/**
 * Waits until all non-daemon threads finish their execution,
 * initiates VM shutdown sequence and stops running threads if any.
 *
 * @param[in] java_vm JVM that should be destroyed
 * @param[in] java_thread current java thread
 */
jint vm_destroy(JavaVM_Internal * java_vm, jthread java_thread)
{
    jint status;
    JNIEnv * jni_env;
    jobject uncaught_exception;

    assert(hythread_is_suspend_enabled());

    jni_env = jthread_get_JNI_env(java_thread);

    // Wait until all non-daemon threads finish their execution.
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

    // Execute pending shutdown hooks & finalizers
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

    // Block thread creation.
    // TODO: investigate how to achieve that with ThreadManager

    // Starting this moment any exception occurred in java thread will cause
    // entire java stack unwinding to the most recent native frame.
    // JNI is not available as well.
    assert(java_vm->vm_env->vm_state == Global_Env::VM_RUNNING);
    java_vm->vm_env->vm_state = Global_Env::VM_SHUTDOWNING;

    // Stop all (except current) java and native threads
    // before destroying VM-wide data.

    // Stop java threads
    vm_shutdown_stop_java_threads(java_vm->vm_env);

    // TODO: ups we don't stop native threads as well :-((
    // We are lucky! Currently, there are no such threads.

    return JNI_OK;
}

static int vm_interrupt_process(void * data) {
    hythread_t * threadBuf;
    int i;

    threadBuf = (hythread_t *)data;
    i = 0;
    // Join all threads.
    while (threadBuf[i] != NULL) {
        hythread_join(threadBuf[i]);
        i++;
    }

    // Return 130 to be compatible with RI.
    exit(130);
}

/**
 * Initiates VM shutdown sequence.
 */
static int vm_interrupt_entry_point(void * data) {
    JNIEnv * jni_env;
    JavaVMAttachArgs args;
    JavaVM * java_vm;
    jint status;

    java_vm = (JavaVM *)data;
    args.version = JNI_VERSION_1_2;
    args.group = NULL;
    args.name = "InterruptionHandler";

    status = AttachCurrentThread(java_vm, (void **)&jni_env, &args);
    if (status == JNI_OK) {
        exec_shutdown_sequence(jni_env);
        DetachCurrentThread(java_vm);
    }
    return status;
}

/**
 * Dumps all java stacks.
 */
static int vm_dump_entry_point(void * data) {
    JNIEnv * jni_env;
    JavaVMAttachArgs args;
    JavaVM * java_vm;
    jint status;

    java_vm = (JavaVM *)data;
    args.version = JNI_VERSION_1_2;
    args.group = NULL;
    args.name = "DumpHandler";

    status = AttachCurrentThread(java_vm, (void **)&jni_env, &args);
    if (status == JNI_OK) {
        // TODO: specify particular VM to notify.
        jvmti_notify_data_dump_request();
        st_print_all(stdout);
        DetachCurrentThread(java_vm);
    }
    return status;
}

/**
 * Current process received an interruption signal (Ctrl+C pressed).
 * Shutdown all running VMs and terminate the process.
 */
void vm_interrupt_handler(int UNREF x) {
    JavaVM ** vmBuf;
    hythread_t * threadBuf;
    int nVMs;
    jint status;

    status = JNI_GetCreatedJavaVMs(NULL, 0, &nVMs);
    assert(nVMs <= 1);
    if (status != JNI_OK) return;

    vmBuf = (JavaVM **) STD_MALLOC(nVMs * sizeof(JavaVM *));
    status = JNI_GetCreatedJavaVMs(vmBuf, nVMs, &nVMs);
    assert(nVMs <= 1);
    if (status != JNI_OK) goto cleanup;

    status = hythread_attach(NULL);
    if (status != TM_ERROR_NONE) goto cleanup;


    threadBuf = (hythread_t *) STD_MALLOC((nVMs + 1) * sizeof(hythread_t));
    threadBuf[nVMs] = NULL;

    // Create a new thread for each VM to avoid scalability and deadlock problems.
    for (int i = 0; i < nVMs; i++) {
        threadBuf[i] = NULL;
        hythread_create((threadBuf + i), 0, HYTHREAD_PRIORITY_NORMAL, 0, vm_interrupt_entry_point, (void *)vmBuf[i]);
    }

    // spawn a new thread which will terminate the process.
    hythread_create(NULL, 0, HYTHREAD_PRIORITY_NORMAL, 0, vm_interrupt_process, (void *)threadBuf);

cleanup:
    STD_FREE(vmBuf);
}

/**
 * Current process received an ??? signal (Ctrl+Break pressed).
 * Prints java stack traces for each VM running in the current process.
 */
void vm_dump_handler(int UNREF x) {
    JavaVM ** vmBuf;
    int nVMs;
    jint status;

    status = JNI_GetCreatedJavaVMs(NULL, 0, &nVMs);
    assert(nVMs <= 1);
    if (status != JNI_OK) return;

    vmBuf = (JavaVM **) STD_MALLOC(nVMs * sizeof(JavaVM *));
    status = JNI_GetCreatedJavaVMs(vmBuf, nVMs, &nVMs);
    assert(nVMs <= 1);

    if (status != JNI_OK) goto cleanup;

    status = hythread_attach(NULL);
    if (status != TM_ERROR_NONE) goto cleanup;

    // Create a new thread for each VM to avoid scalability and deadlock problems.
    for (int i = 0; i < nVMs; i++) {
        hythread_create(NULL, 0, HYTHREAD_PRIORITY_NORMAL, 0, vm_dump_entry_point, (void *)vmBuf[i]);
    }

cleanup:
    STD_FREE(vmBuf);
}
