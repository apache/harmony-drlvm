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
#include <apr_atomic.h>
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
    IDATA status;
    IDATA result = TM_ERROR_NONE;
    hythread_t self = hythread_self();
    hythread_monitor_t mon;
    assert(self);

    // Grab thread monitor
    mon = self->monitor;
    status = hythread_monitor_enter(mon);
    assert(status == TM_ERROR_NONE);
    assert(mon->recursion_count == 0);
    mon->owner = NULL;
    mon->wait_count++;

    // Set thread state
    status = hymutex_lock(&self->mutex);
    assert(status == TM_ERROR_NONE);
    self->waited_monitor = mon;
    if (!(self->state & TM_THREAD_STATE_UNPARKED)) {
        // if thread is not unparked stop the current thread from executing
        self->state |= TM_THREAD_STATE_PARKED;
        status = hymutex_unlock(&self->mutex);
        assert(status == TM_ERROR_NONE);

        do {
            result = condvar_wait_impl(&mon->condition, &mon->mutex,
                millis, nanos, WAIT_INTERRUPTABLE);
            if (result != TM_ERROR_NONE
                || (self->state & TM_THREAD_STATE_PARKED) == 0)
            {
                break;
            }
        } while (1);

        // Restore thread state
        status = hymutex_lock(&self->mutex);
        assert(status == TM_ERROR_NONE);
    }
    self->state &= ~TM_THREAD_STATE_PARKED;
    self->waited_monitor = NULL;
    status = hymutex_unlock(&self->mutex);
    assert(status == TM_ERROR_NONE);

    // Release thread monitor
    mon->wait_count--;
    mon->owner = self;
    assert(mon->notify_count <= mon->wait_count);
    status = hythread_monitor_exit(mon);
    assert(status == TM_ERROR_NONE);

    if (self->request) {
        hythread_safe_point();
        hythread_exception_safe_point();
    }

    // the status should be restored for j.u.c.LockSupport
    if (result == TM_ERROR_INTERRUPT) {
        apr_atomic_set32(&self->interrupted, TRUE);
    }

    return result;
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
    IDATA status;
    hythread_monitor_t mon;
    if (thread == NULL) {
        return;
    }

    status = hymutex_lock(&thread->mutex);
    assert(status == TM_ERROR_NONE);

    if (thread->state & TM_THREAD_STATE_PARKED) {
        mon = thread->waited_monitor;
        assert(mon);
        // Notify parked thread
        status = hymutex_lock(&mon->mutex);
        assert(status == TM_ERROR_NONE);
        status = hycond_notify_all(&mon->condition);
        assert(status == TM_ERROR_NONE);
        status = hymutex_unlock(&mon->mutex);
        assert(status == TM_ERROR_NONE);
    } else {
        thread->state |= TM_THREAD_STATE_UNPARKED;
    }

    thread->state &= ~TM_THREAD_STATE_PARKED;
    status = hymutex_unlock(&thread->mutex);
    assert(status == TM_ERROR_NONE);
}
