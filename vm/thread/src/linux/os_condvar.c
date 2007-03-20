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
 * @file os_condvar.c
 * @brief Binding of hycond to condition variables provided by Pthreads
 */

#include "thread_private.h"
#include <open/hythread_ext.h>


/** @name Conditional variable
 */
//@{

/**
 * waits on a condition variable, directly using OS interfaces.
 *
 * This function does not implement interruptability and thread state
 * functionality, thus the caller of this function have to handle it.
 */
int os_cond_timedwait(hycond_t *cond, hymutex_t *mutex, I_64 ms, IDATA nano)
{
    int r = 0;
    if (!ms && !nano) {
        r = pthread_cond_wait(cond, mutex);
    } else {
        struct timespec abstime;
        apr_time_t then = apr_time_now() + ms*1000 + nano/1000;
        abstime.tv_sec = apr_time_sec(then);
        abstime.tv_nsec = apr_time_usec(then)*1000 + nano%1000;
        r = pthread_cond_timedwait(cond, mutex, &abstime);
    }
    if (r == ETIMEDOUT)
        r = TM_ERROR_TIMEOUT;
    else if (r == EINTR)
        r = TM_ERROR_INTERRUPT;
    return r;
}


/**
 * Creates and initializes condition variable.
 *
 * @param[in] cond the address of the condition variable.
 * @return 0 on success, non-zero otherwise.
 */
IDATA VMCALL hycond_create (hycond_t *cond) {
    return pthread_cond_init(cond, NULL);
}

/**
 * Signals a single thread that is blocking on the given condition variable
 * to wake up.
 *
 * @param[in] cond the condition variable on which to produce the signal.
 * @sa apr_thread_cond_signal()
 */
IDATA VMCALL hycond_notify (hycond_t *cond) {
    return pthread_cond_signal(cond);
}

/**
 * Signals all threads blocking on the given condition variable.
 *
 * @param[in] cond the condition variable on which to produce the broadcast.
 * @sa apr_thread_cond_broadcast()
 */
IDATA VMCALL hycond_notify_all (hycond_t *cond) {
    return pthread_cond_broadcast(cond);
}

/**
 * Destroys the condition variable and releases the associated memory.
 *
 * @param[in] cond the condition variable to destroy
 * @sa apr_thread_cond_destroy()
 */
IDATA VMCALL hycond_destroy (hycond_t *cond) {
    return pthread_cond_destroy(cond);
}

//@}
