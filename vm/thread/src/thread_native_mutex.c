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
 * @file thread_native_mutex.c
 * @brief Hythread mutex related functions
 */

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.locks"

#include "thread_private.h"
#include <open/hythread_ext.h>


/** @name Mutex
 *  
 */
//@{

/**
 * Creates and initializes a mutex.
 *
 * Mutex is a fat lock which implies parking of threads in case of contention.
 * @param[in] mutex the memory address where the newly created mutex will be
 *        stored.
 * @param[in] flags Or'ed value of:
 * <PRE>
 *           HYTHREAD_MUTEX_DEFAULT   platform-optimal lock behavior.
 *           HYTHREAD_MUTEX_NESTED    enable nested (recursive) locks.
 *           HYTHREAD_MUTEX_UNNESTED  disable nested locks (non-recursive).
 * </PRE>
 * @sa apr_thread_mutex_create()
 */
IDATA VMCALL hymutex_create (hymutex_t *mutex, UDATA flags) {
    apr_pool_t *pool = get_local_pool(); 
    apr_status_t apr_status;
        
    apr_status = apr_thread_mutex_create((apr_thread_mutex_t**)mutex, flags, pool);
    return CONVERT_ERROR(apr_status);
}

/**
 * Acquires the lock for the given mutex. If the mutex is already locked,
 * the current thread will be put to sleep until the lock becomes available.
 *
 * @param[in] mutex the mutex on which to acquire the lock.
 * @sa apr_thread_mutex_lock()
 */
IDATA VMCALL hymutex_lock(hymutex_t mutex) {
    apr_status_t apr_status;
    apr_status = apr_thread_mutex_lock((apr_thread_mutex_t*)mutex);

    return CONVERT_ERROR(apr_status);
}

/**
 * Attempts to acquire the lock for the given mutex. 
 *
 * @param[in] mutex the mutex on which to attempt the lock acquiring.
 * @sa apr_thread_mutex_trylock()
 */
IDATA VMCALL hymutex_trylock (hymutex_t mutex) {
    return CONVERT_ERROR(apr_thread_mutex_trylock((apr_thread_mutex_t*)mutex));
}

/**
 * Releases the lock for the given mutex.
 *
 * @param[in] mutex the mutex from which to release the lock.
 * @sa apr_thread_mutex_unlock()
 */
IDATA VMCALL hymutex_unlock (hymutex_t mutex) {
    apr_status_t apr_status = apr_thread_mutex_unlock((apr_thread_mutex_t*)mutex);
    assert(apr_status == APR_SUCCESS);
    return CONVERT_ERROR(apr_status);
}

/**
 * Destroys the mutex and releases the memory associated with the lock.
 *
 * @param[in] mutex the mutex to destroy.
 * @sa apr_thread_mutex_destroy()
 */
IDATA VMCALL hymutex_destroy (hymutex_t mutex) {
    apr_pool_t *pool = apr_thread_mutex_pool_get ((apr_thread_mutex_t*)mutex);
    apr_status_t apr_status;
    if (pool != get_local_pool()) {
        return local_pool_cleanup_register(hymutex_destroy, mutex);
    }
    apr_status=apr_thread_mutex_destroy((apr_thread_mutex_t*)mutex);
    return CONVERT_ERROR(apr_status);
}


//@}
