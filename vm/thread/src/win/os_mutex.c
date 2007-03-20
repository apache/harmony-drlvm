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
 * @brief hymutex binding to Win32 critical sections
 */

#include "thread_private.h"
#include <open/hythread_ext.h>


/** @name Mutex
 *
 */
//@{

/**
 * Initializes a mutex.
 *
 * A memory for mutex must be preallocated.
 *
 * @param[in] mutex the address of the mutex to be initialized
 * @param[in] flags Or'ed value of:
 * <PRE>
 *           APR_THREAD_MUTEX_DEFAULT   platform-optimal lock behavior.
 *           APR_THREAD_MUTEX_NESTED    enable nested (recursive) locks.
 *           APR_THREAD_MUTEX_UNNESTED  disable nested locks (non-recursive).
 * </PRE>
 */
IDATA VMCALL hymutex_create (hymutex_t *mutex, UDATA flags) {
    int r = 0;
    if (flags & APR_THREAD_MUTEX_UNNESTED) {
        assert(!"not implemented");
        return -1;
    }
    InitializeCriticalSection(mutex);
    return r;
}

/**
 * Acquires the lock for the given mutex. If the mutex is already locked,
 * the current thread will be put to sleep until the lock becomes available.
 *
 * @param[in] mutex the mutex on which to acquire the lock.
 * @sa apr_thread_mutex_lock()
 */
IDATA VMCALL hymutex_lock(hymutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

/**
 * Attempts to acquire the lock for the given mutex.
 *
 * @param[in] mutex the mutex on which to attempt the lock acquiring.
 * @sa apr_thread_mutex_trylock()
 */
IDATA VMCALL hymutex_trylock (hymutex_t *mutex) {
    int r;
    r = TryEnterCriticalSection(mutex);
    // Return code is non-zero on success
    if (r == 0) return TM_ERROR_EBUSY;
    return 0;
}

/**
 * Releases the lock for the given mutex.
 *
 * @param[in] mutex the mutex from which to release the lock.
 * @sa apr_thread_mutex_unlock()
 */
IDATA VMCALL hymutex_unlock (hymutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

/**
 * Destroys the mutex.
 *
 * @param[in] mutex the mutex to destroy.
 * @sa apr_thread_mutex_destroy()
 */
IDATA VMCALL hymutex_destroy (hymutex_t *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

//@}
