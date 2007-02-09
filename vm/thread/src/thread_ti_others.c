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

/*
 *  Peak count
 */
 
/**
 * Resets the thread peak counter.
 */
IDATA  jthread_reset_peak_thread_count () {
    return TM_ERROR_NONE;
} 

/**
 * Returns the peak thread count since the last peak reset. 
 */
IDATA  jthread_get_peak_thread_count (jint *threads_count_ptr) {
    return TM_ERROR_NONE;
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
