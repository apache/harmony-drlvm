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
 * @file thread_ti_others.c
 * @brief JVMTI peak related functions
 */  

#include <open/jthread.h>
#include <open/hythread_ext.h>
#include <open/ti_thread.h>
#include "thread_private.h"

#define THREAD_CONTENTION_MONITORING_SUPPORTED 1

/*
 *  Monitors contentions requests enabled flag.
 */
int thread_contention_monitoring_enabled = 0;

/*
 *  Total started thread counter.
 */
int total_started_thread_count = 0;

/*
 *  Alive thread counter.
 */
int alive_thread_count = 0;

/*
 *  Peak count
 */
int peak_thread_count = 0;
 
/**
 * Resets the thread peak counter to current value.
 */
IDATA  jthread_reset_peak_thread_count () {

    peak_thread_count = alive_thread_count;
    return TM_ERROR_NONE;
} 

/**
 * Returns the peak thread count since the last peak reset. 
 */
IDATA  jthread_get_peak_thread_count (jint *threads_count_ptr) {
    *threads_count_ptr = peak_thread_count;
    return TM_ERROR_NONE;
}
 
/**
 * Returns true if VM supports monitors contention requests and 
 * this feature is enabled 
 *
 * @return true if monitors contention requests are enabled, false otherwise;
 */
jboolean jthread_is_thread_contention_monitoring_enabled(){
    return thread_contention_monitoring_enabled;
}

/**
 * Returns true if VM supports monitors contention requests
 *
 * @return true if monitors contention requests are supported, false otherwise;
 */
jboolean jthread_is_thread_contention_monitoring_supported(){

    return THREAD_CONTENTION_MONITORING_SUPPORTED;
}

/**
 * Enabled or diabled thread monitors contention requests
 *
 * @param[in] true or false to enable or disable the feature
 */
void jthread_set_thread_contention_monitoring_enabled(jboolean flag){

    thread_contention_monitoring_enabled = THREAD_CONTENTION_MONITORING_SUPPORTED ? flag : 0;
}

/**
 * Returns JVMTILocalStorage pointer.
 *
 * @param[in] java_thread
 */
JVMTILocalStorage* jthread_get_jvmti_local_storage(jthread java_thread) {

    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;

    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);

    return &tm_java_thread->jvmti_local_storage;

}

/**
 * Increase thread counters.
 */
void thread_start_count(){
    alive_thread_count++;
    total_started_thread_count++;
    if (peak_thread_count < alive_thread_count) {
        peak_thread_count = alive_thread_count;
    }
}

/**
 * Decrease alive thread counter.
 */
void thread_end_count(){
    alive_thread_count--;
}

