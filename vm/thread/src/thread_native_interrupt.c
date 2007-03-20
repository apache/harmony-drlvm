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
 * @file thread_native_interrupt.c
 * @brief Hythread interruption related functions
 */

#include "thread_private.h"
#include <open/hythread_ext.h>

static int interrupter_thread_function(void *args);

/** 
 * Interrupt a thread.
 * 
 * If the thread is currently blocked (i.e. waiting on a monitor_wait or sleeping)
 * resume the thread and cause it to return from the blocking function with
 * HYTHREAD_INTERRUPTED.
 * 
 * @param[in] thread a thread to be interrupted
 * @return none
 */
void VMCALL hythread_interrupt(hythread_t thread) {
    IDATA status;
    hythread_t thr = NULL;
    hymutex_lock(&thread->mutex);
    thread->state |= TM_THREAD_STATE_INTERRUPTED;
    
    if (thread == tm_self_tls) {
        hymutex_unlock(&thread->mutex);
        return;
    }

    // If thread was doing any kind of wait, notify it.
    if (thread->state & (TM_THREAD_STATE_PARKED | TM_THREAD_STATE_SLEEPING)) {
        if (thread->current_condition) {
	    status = hycond_notify_all(thread->current_condition);
	    assert(status == TM_ERROR_NONE);
	}
    } else if (thread->state & TM_THREAD_STATE_IN_MONITOR_WAIT) {
        if (thread->current_condition && (hythread_monitor_try_enter(thread->waited_monitor) == TM_ERROR_NONE)) {
            hythread_monitor_notify_all(thread->waited_monitor);
            hythread_monitor_exit(thread->waited_monitor);
        } else {
            status = hythread_create(&thr, 0, 0, 0, interrupter_thread_function, (void *)thread);
            assert (status == TM_ERROR_NONE);
	}
    }

    hymutex_unlock(&thread->mutex);
}

static int interrupter_thread_function(void *args) {
    hythread_t thread = (hythread_t)args; 
    hythread_monitor_t monitor = NULL;
    hymutex_lock(&thread->mutex);

    if (thread->waited_monitor) {
        monitor = thread->waited_monitor;
    } else {
        hymutex_unlock(&thread->mutex);
        hythread_exit(NULL);
        return 0; 
    } 

    hymutex_unlock(&thread->mutex);

    hythread_monitor_enter(monitor);
    hythread_monitor_notify_all(monitor);

    hythread_exit(monitor);
    return 0;
}

/** 
 *  Returns interrupted status and clear interrupted flag.
 *
 * @param[in] thread where to clear interrupt flag
 * @returns TM_ERROR_INTERRUPT if thread was interrupted, TM_ERROR_NONE otherwise
 */
UDATA VMCALL hythread_clear_interrupted_other(hythread_t thread) {
    int interrupted;
    hymutex_lock(&thread->mutex);
    interrupted = thread->state & TM_THREAD_STATE_INTERRUPTED;
    thread->state &= ~TM_THREAD_STATE_INTERRUPTED;
    hymutex_unlock(&thread->mutex);
    return interrupted ? TM_ERROR_INTERRUPT : TM_ERROR_NONE;
}

/**
 * Clear the interrupted flag of the current thread and return its previous value.
 * 
 * @return  previous value of interrupted flag: non-zero if the thread had been interrupted.
 */
UDATA VMCALL hythread_clear_interrupted() {
    return hythread_clear_interrupted_other(tm_self_tls);
}

/**
 * Return the value of a thread's interrupted flag.
 * 
 * @param[in] thread thread to be queried
 * @return 0 if not interrupted, non-zero if interrupted
 */
UDATA VMCALL hythread_interrupted(hythread_t thread) {
    int interrupted = thread->state & TM_THREAD_STATE_INTERRUPTED;
    return interrupted?TM_ERROR_INTERRUPT:TM_ERROR_NONE;
}

