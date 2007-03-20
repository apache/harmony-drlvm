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

#define THREAD_CPU_TIME_SUPPORTED 1

/*
 *  Thread CPU time enabled flag.
 */
int thread_cpu_time_enabled = 0;

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
    int64 kernel_time;
    assert(nanos_ptr);

    if (NULL == java_thread) {
        tm_native_thread = hythread_self();
    } else {
        tm_native_thread = vm_jthread_get_tm_data(java_thread);
    }

    return hythread_get_thread_times(tm_native_thread, &kernel_time, nanos_ptr);
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
    int64 kernel_time;
    int64 user_time;

    assert(java_thread);
    assert(nanos_ptr);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    hythread_get_thread_times(tm_native_thread, &kernel_time, &user_time);
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

/**
 * Returns number of times the specific thread contending for monitors.
 *
 * @param[in] java_thread
 * @return number of times the specific thread contending for monitors
 */
jlong VMCALL jthread_get_thread_blocked_times_count(jthread java_thread) {
    
    hythread_t tm_native_thread = jthread_get_native_thread(java_thread); 
    jvmti_thread_t tm_java_thread = hythread_get_private_data(tm_native_thread);

    return tm_java_thread->blocked_count;
}

/**
 * Returns number of times the specific thread waiting on monitors for notification.
 *
 * @param[in] java_thread
 * @return number of times the specific thread waiting on monitors for notification
 */
jlong VMCALL jthread_get_thread_waited_times_count(jthread java_thread) {
    
    hythread_t tm_native_thread = jthread_get_native_thread(java_thread); 
    jvmti_thread_t tm_java_thread = hythread_get_private_data(tm_native_thread);

    return tm_java_thread->waited_count;
}

/**
 * Returns true if VM supports current thread CPU and USER time requests
 *
 * @return true if current thread CPU and USER time requests are supported, false otherwise;
 */
jboolean jthread_is_current_thread_cpu_time_supported(){
    return THREAD_CPU_TIME_SUPPORTED;
}

/**
 * Returns true if VM supports thread CPU and USER time requests
 *
 * @return true if thread CPU and USER time requests are supported, false otherwise;
 */
jboolean jthread_is_thread_cpu_time_supported(){
    return THREAD_CPU_TIME_SUPPORTED;
}

/**
 * Returns true if VM supports (current) thread CPU and USER time requests and 
 * this feature is enabled 
 *
 * @return true if thread CPU and USER time requests are enabled, false otherwise;
 */
jboolean jthread_is_thread_cpu_time_enabled(){
    return thread_cpu_time_enabled;
}

/**
 * Enabled or diabled thread CPU and USER time requests
 *
 * @param[in] true or false to enable or disable the feature
 */
void jthread_set_thread_cpu_time_enabled(jboolean flag){
     thread_cpu_time_enabled = THREAD_CPU_TIME_SUPPORTED ? flag : 0;
}
