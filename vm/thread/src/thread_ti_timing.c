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
 * @file thread_ti_timing.c
 * @brief JVMTI timing related functions
 */

#include <open/ti_thread.h>
#include <open/hythread_ext.h>
#include <open/jthread.h>
#include "thread_private.h"
#include "apr_thread_ext.h"

/**
 * Returns time spent by the specific thread while contending for monitors.
 *
 * @param[in] java_thread
 * @param[out] nanos_ptr CPU time in nanoseconds
 */
IDATA VMCALL jthread_get_thread_blocked_time(jthread java_thread, jlong *nanos_ptr) {

    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;

    assert(java_thread);
    assert(nanos_ptr);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    *nanos_ptr = tm_java_thread->blocked_time;

    return TM_ERROR_NONE;
}

/**
 * Returns time utilized by given thread.
 *
 * @param[in] java_thread
 * @param[out] nanos_ptr CPU time in nanoseconds
 */
IDATA VMCALL jthread_get_thread_cpu_time(jthread java_thread, jlong *nanos_ptr) {

    hythread_t tm_native_thread;
    assert(nanos_ptr);

    if (NULL == java_thread) {
        tm_native_thread = hythread_self();
    } else {
        tm_native_thread = vm_jthread_get_tm_data(java_thread);
    }

    return CONVERT_ERROR(apr_get_thread_time(tm_native_thread->os_handle,
        (apr_int64_t*) nanos_ptr));
}

/**
 * Returns information about the system timer.
 *
 * @param[out] info_ptr timer info
 */
IDATA VMCALL jthread_get_thread_cpu_timer_info(jvmtiTimerInfo* info_ptr) {
    return TM_ERROR_NONE;
}

/**
 * Returns time utilized by the given thread in user mode.
 *
 * @param[in] java_thread 
 * @param[out] nanos_ptr CPU time in nanoseconds
 */
IDATA VMCALL jthread_get_thread_user_cpu_time(jthread java_thread, jlong *nanos_ptr) {

    hythread_t tm_native_thread;
    apr_time_t kernel_time;
    apr_time_t user_time;

    assert(java_thread);
    assert(nanos_ptr);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    apr_thread_times(tm_native_thread->os_handle, &user_time, &kernel_time);
    *nanos_ptr = user_time;

    return TM_ERROR_NONE;
}

/**
 * Returns time spent by the specific thread while waiting for monitors.
 *
 * @param[in] java_thread 
 * @param[out] nanos_ptr CPU time in nanoseconds
 */
IDATA VMCALL jthread_get_thread_waited_time(jthread java_thread, jlong *nanos_ptr) {

    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;

    assert(java_thread);
    assert(nanos_ptr);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    *nanos_ptr = tm_java_thread->waited_time;

    return TM_ERROR_NONE;
}
