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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.5.4.5 $
 */


#define LOG_DOMAIN "thread"
#include "cxxlog.h"

#include "vm_process.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;

#include <signal.h>
#include <stdlib.h>

#if defined (PLATFORM_NT)
#include <direct.h>
#elif defined (PLATFORM_POSIX)
#include <sys/time.h>
#include <unistd.h>
#endif

#include <apr_pools.h>

#include "open/hythread.h"
#include "open/hythread_ext.h"
#include "open/jthread.h"
#include "open/thread_externals.h"

#include "open/types.h"
#include "open/vm_util.h"
#include "open/gc.h"

#include "environment.h"
#include "vm_strings.h"
#include "object_layout.h"
#include "Class.h"
#include "classloader.h"
#include "vm_threads.h"
#include "nogc.h"
#include "ini.h"
#include "m2n.h"
#include "exceptions.h"
#include "jit_intf.h"
#include "exception_filter.h"
#include "vm_threads.h"
#include "jni_utils.h"
#include "object.h"
#include "platform_core_natives.h"
#include "heap.h"
#include "verify_stack_enumeration.h"
#include "sync_bits.h"
#include "vm_stats.h"
#include "native_utils.h"
#include "thread_manager.h"
#include "object_generic.h"
#include "thread_generic.h"
#include "mon_enter_exit.h"
#include "jni_direct.h"
#include "port_malloc.h"

#ifdef _IPF_
#include "java_lang_thread_ipf.h"
#include "../m2n_ipf_internal.h"
#elif defined _EM64T_
#include "java_lang_thread_em64t.h"
#include "../m2n_em64t_internal.h"
#else
#include "java_lang_thread_ia32.h"
#include "../m2n_ia32_internal.h"
#endif

#define IS_FAT_LOCK(lockword) (lockword >> 31)

extern struct JNINativeInterface_ jni_vtable;

/**
 * Runs java.lang.Thread.detach() method.
 */
static jint run_java_detach(jthread java_thread)
{
    static Method *detach = NULL;
    const char *method_name = "detach";
    const char *descriptor = "(Ljava/lang/Throwable;)V";
    jvalue args[2];
    JNIEnv *jni_env;
    Global_Env *vm_env;
    Class *thread_class;

    assert(hythread_is_suspend_enabled());

    jni_env = jthread_get_JNI_env(java_thread);
    vm_env = jni_get_vm_env(jni_env);
    thread_class = vm_env->java_lang_Thread_Class;

    if (detach == NULL) {
        detach = class_lookup_method(thread_class, method_name, descriptor);
        if (detach == NULL) {
            TRACE("Failed to find thread's detach method " << descriptor <<
                  " , exception = " << exn_get());
            return TM_ERROR_INTERNAL;
        }
    }
    // Initialize arguments.
    args[0].l = java_thread;
    if (vm_env->IsVmShutdowning()) {
        args[1].l = NULL;
    } else {
        args[1].l = exn_get();
    }
    exn_clear();

    hythread_suspend_disable();
    vm_execute_java_method_array((jmethodID) detach, 0, args);
    hythread_suspend_enable();

    if (exn_raised()) {
        TRACE
            ("java.lang.Thread.detach(Throwable) method completed with an exception: "
             << exn_get_name());
        return TM_ERROR_INTERNAL;
    }
    return TM_ERROR_NONE;
}

/**
 * Attaches thread current thread to VM.
 */
jint vm_attach(JavaVM * java_vm, JNIEnv ** p_jni_env)
{
    // It seems to be reasonable to have suspend enabled state here.
    // It is unsafe to perform operations which require suspend disabled
    // mode until current thread is not attaced to VM.
    assert(hythread_is_suspend_enabled());

    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();
    assert(vm_thread);

    // if the assertion is false we cannot notify the parent thread
    // that we started and it would hang in waitloop
    assert(vm_thread);

    jint status = jthread_allocate_vm_thread_pool(java_vm, vm_thread);
    if (status != JNI_OK) {
        return status;
    }

    // Create top level M2N frame.
    M2nFrame *p_m2n = (M2nFrame *) apr_palloc(vm_thread->pool, sizeof(M2nFrame));
    if (!p_m2n) {
        return JNI_ENOMEM;
    }

    // Create local handles.
    ObjectHandles *p_handles = (ObjectHandles *) apr_palloc(vm_thread->pool,
                        sizeof(ObjectHandlesNew));
    if (!p_handles) {
        return JNI_ENOMEM;
    }

    vm_thread->jni_env =
        (JNIEnv *) apr_palloc(vm_thread->pool, sizeof(JNIEnv_Internal));
    if (!vm_thread->jni_env) {
        return JNI_ENOMEM;
    }

    // Initialize JNI environment.
    JNIEnv_Internal *jni_env = (JNIEnv_Internal *) vm_thread->jni_env;
    jni_env->functions = &jni_vtable;
    jni_env->vm = (JavaVM_Internal *) java_vm;
    jni_env->reserved0 = (void *) 0x1234abcd;
    *p_jni_env = jni_env;

    init_stack_info();

    m2n_null_init(p_m2n);
    m2n_set_last_frame(p_m2n);

    oh_null_init_handles(p_handles);

    m2n_set_local_handles(p_m2n, p_handles);
    m2n_set_frame_type(p_m2n, FRAME_NON_UNWINDABLE);
    gc_thread_init(&vm_thread->_gc_private_information);

    if (ti_is_enabled()) {
        vm_thread->jvmti_thread.owned_monitors_size = TM_INITIAL_OWNED_MONITOR_SIZE;
        vm_thread->jvmti_thread.owned_monitors = (jobject*)apr_palloc(vm_thread->pool,
                TM_INITIAL_OWNED_MONITOR_SIZE * sizeof(jobject));

        vm_thread->jvmti_thread.jvmti_jit_breakpoints_handling_buffer =
            (jbyte*)apr_palloc(vm_thread->pool,
                sizeof(jbyte) * TM_JVMTI_MAX_BUFFER_SIZE);
    }
    ((hythread_t)vm_thread)->java_status = TM_STATUS_INITIALIZED;

    assert(hythread_is_suspend_enabled());
    return JNI_OK;
}

/**
 * Detaches current thread from VM.
 */
jint vm_detach(jthread java_thread)
{
    VM_thread *p_vm_thread;

    assert(hythread_is_suspend_enabled());

    // could return error if detach throws an exception,
    // keep exception and ignore an error
    run_java_detach(java_thread);

    // Send Thread End event
    jvmti_send_thread_start_end_event(0);

    hythread_suspend_disable();

    p_vm_thread = get_thread_ptr();

    // Notify GC about thread detaching.
    gc_thread_kill(&p_vm_thread->_gc_private_information);
    assert(p_vm_thread->gc_frames == 0);

#ifdef PLATFORM_POSIX
    // Remove guard page on the stack on linux
    remove_guard_stack();
#endif

    // Destroy current VM_thread pool.
    jthread_deallocate_vm_thread_pool(p_vm_thread);

    hythread_suspend_enable();

    return JNI_OK;

/** TODO: Check if we need these actions!!!
#ifndef NDEBUG
    hythread_t tm_native_thread = jthread_get_native_thread();
    assert(tm_native_thread);
    assert(tm_native_thread == hythread_self());
#endif

    // 3) Remove tm_thread_t pointer from java.lang.Thread object.
    vm_jthread_set_tm_data(jthread java_thread, NULL);
*/
}

void vm_notify_obj_alive(void *p_obj)
{
    uint32 obj_info = ((ManagedObject*)p_obj)->get_obj_info();
    if (hythread_is_fat_lock(obj_info)) {
        hythread_native_resource_is_live(obj_info);
    }
}

void vm_reclaim_native_objs()
{
    hythread_reclaim_resources();
}
