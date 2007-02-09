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
 * @file thread_java_iterator.c
 * @brief Java thread iterator related functions
 */

#include <open/ti_thread.h>
#include <open/hythread_ext.h>
#include <open/jthread.h>
#include "thread_private.h"


/**
 * Creates the iterator that can be used to walk over java threads.
 */
jthread_iterator_t VMCALL jthread_iterator_create(void) {
    hythread_group_t java_thread_group = get_java_thread_group();
    return (jthread_iterator_t)hythread_iterator_create(java_thread_group);
}

/**
 * Releases the iterator.
 * 
 * @param[in] it iterator
 */
IDATA VMCALL jthread_iterator_release(jthread_iterator_t *it) {
    return hythread_iterator_release((hythread_iterator_t *)it);
}

/**
 * Resets the iterator such that it will start from the beginning.
 * 
 * @param[in] it iterator
 */
IDATA VMCALL jthread_iterator_reset(jthread_iterator_t *it) {
    return hythread_iterator_reset((hythread_iterator_t *)it);   
}

/**
 * Returns the next jthread using the given iterator.
 * 
 * @param[in] it iterator
 */
jthread VMCALL jthread_iterator_next(jthread_iterator_t *it) {
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    tm_native_thread = hythread_iterator_next((hythread_iterator_t *)it);
    while (tm_native_thread!=NULL)
    {
        if (hythread_is_alive(tm_native_thread)) {
            tm_java_thread = hythread_get_private_data(tm_native_thread);
            if (tm_java_thread) {
                return (jthread)tm_java_thread->thread_object;
            }
        }
        tm_native_thread = hythread_iterator_next((hythread_iterator_t *)it);
    }

    return NULL;
} 

/**
 * Returns the the number of Java threads.
 * 
 * @param[in] iterator
 */
IDATA VMCALL jthread_iterator_size(jthread_iterator_t iterator) {
    jthread res;
    IDATA status;
    int count=0;
    status=jthread_iterator_reset(&iterator);
    assert(status == TM_ERROR_NONE);
    res = jthread_iterator_next(&iterator);
    while (res!=NULL) {
        count++;        
        res = jthread_iterator_next(&iterator);
    }
    status=jthread_iterator_reset(&iterator);
    assert(status == TM_ERROR_NONE);
    return count;
}
