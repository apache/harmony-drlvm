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
 * @file thread_native_condvar.c
 * @brief Hythread condvar related functions
 */

#include "thread_private.h"
#include <open/hythread_ext.h>


/** @name Conditional variable
 */
//@{

/**
 * Creates and initializes condition variable.
 *
 * @param[out] cond the memory address where the newly created condition variable
 * will be stored.
 * @sa apr_thread_cond_create()
 */
IDATA VMCALL hycond_create (hycond_t *cond) {
    apr_pool_t *pool = get_local_pool(); 
    apr_status_t apr_status = apr_thread_cond_create((apr_thread_cond_t**)cond, pool);
    if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
    return TM_ERROR_NONE;
}

IDATA condvar_wait_impl(hycond_t cond, hymutex_t mutex, I_64 ms, IDATA nano, IDATA interruptable) {
    apr_status_t apr_status;
    int disable_count;
    hythread_t this_thread;
    
    this_thread = tm_self_tls;    
    
    // Store provided cond into current thread cond
    this_thread->current_condition = interruptable ? cond : NULL;

    // check interrupted flag
    if (interruptable && (this_thread->state & TM_THREAD_STATE_INTERRUPTED)) {
        // clean interrupted flag
        this_thread->state &= (~TM_THREAD_STATE_INTERRUPTED);
                return TM_ERROR_INTERRUPT;
    }

    disable_count = reset_suspend_disable(); 
    // Delegate to OS wait
    apr_status = (!ms && !nano)?
        apr_thread_cond_wait((apr_thread_cond_t*)cond, (apr_thread_mutex_t*)mutex):
	apr_thread_cond_timedwait ((apr_thread_cond_t*)cond, (apr_thread_mutex_t*)mutex, ms*1000 + ((nano < 1000) ? 1 : (nano / 1000)));
        
    set_suspend_disable(disable_count);

    this_thread->current_condition = NULL;
   
    // check interrupted flag
    if (interruptable &&  (this_thread->state & TM_THREAD_STATE_INTERRUPTED)) {
        // clean interrupted flag
        this_thread->state &= (~TM_THREAD_STATE_INTERRUPTED);
        return TM_ERROR_INTERRUPT;
    }

    return CONVERT_ERROR(apr_status);
}

/**
 * Instructs the current thread to wait until it is signaled to wake up.
 * 
 * @param[in] cond the condition variable on which to block
 * @param[in] mutex the mutex that must be locked upon entering this function
 * @sa apr_thread_cond_wait()
 * @return  
 *      TM_NO_ERROR on success 
 */
IDATA VMCALL hycond_wait(hycond_t cond, hymutex_t mutex) {
    return condvar_wait_impl(cond, mutex, 0, 0, 0);
}

/**
 * Instructs the current thread to wait until signaled to wake up or
 * the specified timeout is elapsed.
 *
 * @param[in] cond the condition variable on which to block.
 * @param[in] mutex the mutex that must be locked upon entering this function
 * @param[in] ms amount of time in milliseconds to wait
 * @param[in] nano amount of time in nanoseconds to wait
 * @sa apr_thread_cond_timedwait()
 * @return  
 *      TM_NO_ERROR on success 
 */
IDATA VMCALL hycond_wait_timed(hycond_t cond, hymutex_t mutex, I_64 ms, IDATA nano) {
    return condvar_wait_impl(cond, mutex, ms, nano, 0);
}

/**
 * Instructs the current thread to wait until signaled to wake up or
 * the specified timeout is elapsed.
 *
 * @param[in] cond the condition variable on which to block.
 * @param[in] mutex the mutex that must be locked upon entering this function
 * @param[in] ms amount of time in milliseconds to wait
 * @param[in] nano amount of time in nanoseconds to wait
 * @sa apr_thread_cond_timedwait()
 * @return  
 *      TM_NO_ERROR on success 
 *      TM_THREAD_INTERRUPTED in case thread was interrupted during wait.
 */
IDATA VMCALL hycond_wait_interruptable(hycond_t cond, hymutex_t mutex, I_64 ms, IDATA nano) {
    return condvar_wait_impl(cond, mutex, ms, nano, WAIT_INTERRUPTABLE);
}

/**
 * Signals a single thread that is blocking on the given condition variable to wake up. 
 *
 * @param[in] cond the condition variable on which to produce the signal.
 * @sa apr_thread_cond_signal()
 */
IDATA VMCALL hycond_notify (hycond_t cond) {
    apr_status_t apr_status = apr_thread_cond_signal((apr_thread_cond_t*)cond);
    if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
    return TM_ERROR_NONE;
}

/**
 * Signals all threads blocking on the given condition variable.
 * 
 * @param[in] cond the condition variable on which to produce the broadcast.
 * @sa apr_thread_cond_broadcast()
 */
IDATA VMCALL hycond_notify_all (hycond_t cond) {
    apr_status_t apr_status = apr_thread_cond_broadcast((apr_thread_cond_t*)cond);
    if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
    return TM_ERROR_NONE;   
}

/**
 * Destroys the condition variable and releases the associated memory.
 *
 * @param[in] cond the condition variable to destroy
 * @sa apr_thread_cond_destroy()
 */
IDATA VMCALL hycond_destroy (hycond_t cond) {
    apr_status_t apr_status;
    apr_pool_t *pool = apr_thread_cond_pool_get ((apr_thread_cond_t*)cond);
    if (pool != get_local_pool()) {
          return local_pool_cleanup_register(hycond_destroy, cond);
    }
    apr_status=apr_thread_cond_destroy((apr_thread_cond_t*)cond);
    return CONVERT_ERROR(apr_status);
}

//@}
