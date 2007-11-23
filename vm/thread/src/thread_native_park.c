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
 * @file thread_native_park.c
 * @brief Hythread park/unpark related functions
 */

#include <open/hythread_ext.h>
#include "thread_private.h"


/**
 * 'Park' the current thread. 
 * 
 * Stop the current thread from executing until it is unparked, interrupted, or the specified timeout elapses.
 * 
 * Unlike wait or sleep, the interrupted flag is NOT cleared by this API.
 *
 * @param[in] millis
 * @param[in] nanos 
 * 
 * @return 0 if the thread is unparked
 * HYTHREAD_INTERRUPTED if the thread was interrupted while parked<br>
 * HYTHREAD_PRIORITY_INTERRUPTED if the thread was priority interrupted while parked<br>
 * HYTHREAD_TIMED_OUT if the timeout expired<br>
 *
 * @see hythread_unpark
 */
IDATA VMCALL hythread_park(I_64 millis, IDATA nanos) {
     IDATA status = 0;
     hythread_t t = tm_self_tls;     
     assert(t);

     hymutex_lock(&t->mutex);

     if (t->state & TM_THREAD_STATE_UNPARKED) {
        t->state &= ~TM_THREAD_STATE_UNPARKED;
        hymutex_unlock(&t->mutex);
        return (t->state & TM_THREAD_STATE_INTERRUPTED) ? TM_ERROR_INTERRUPT : TM_ERROR_NONE;
     }

     t->state |= TM_THREAD_STATE_PARKED;
     status = hycond_wait_interruptable(&t->condition, &t->mutex, millis, nanos);
     t->state &= ~TM_THREAD_STATE_PARKED;

     if (t->request) {
         int save_count;
         hymutex_unlock(&t->mutex);
         hythread_safe_point();
         hythread_exception_safe_point();
         save_count = hythread_reset_suspend_disable();
         hymutex_lock(&t->mutex);
         hythread_set_suspend_disable(save_count);
     }

     //the status should be restored for j.u.c.LockSupport
     if (status == TM_ERROR_INTERRUPT) {
         t->state |= TM_THREAD_STATE_INTERRUPTED;
     }

     hymutex_unlock(&t->mutex);
     return status;
}

/**
 * 'Unpark' the specified thread. 
 * 
 * If the thread is parked, it will return from park.
 * If the thread is not parked, its 'UNPARKED' flag will be set, and it will return
 * immediately the next time it is parked.
 *
 * Note that unparks are not counted. Unparking a thread once is the same as unparking it n times.
 * 
 * @see hythread_park
 */
void VMCALL hythread_unpark(hythread_t thread) {
    if (thread ==  NULL) {
        return;
    }
    
    hymutex_lock(&thread->mutex);

    if (thread->state & TM_THREAD_STATE_PARKED) {
        thread->state &= ~TM_THREAD_STATE_PARKED;
        hycond_notify_all(&thread->condition);
    } else {
        thread->state |= TM_THREAD_STATE_UNPARKED;
    }

    hymutex_unlock(&thread->mutex);
}
