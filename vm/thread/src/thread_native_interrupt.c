/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Artem Aliev
 * @version $Revision: 1.1.2.7 $
 */  

/**
 * @file thread_native_interrupt.c
 * @brief Hythread interruption related functions
 */

#include "thread_private.h"
#include <open/hythread_ext.h>

/** 
 * Interrupt a thread.
 * 
 * If the thread is currently blocked (i.e. waiting on a monitor_wait or sleeping)
 * resume the thread and cause it to return from the blocking function with
 * HYTHREAD_INTERRUPTED.
 * 
 * @param[in] thread a thead to be interrupted
 * @return none
 */
void VMCALL hythread_interrupt(hythread_t thread) {
    IDATA status;
    thread->state |= TM_THREAD_STATE_INTERRUPTED;
    
    // If thread was doing any kind of wait, notify it.
    if (thread->current_condition) {
            status=hycond_notify(thread->current_condition);   
                        assert (status == TM_ERROR_NONE);
    }
}

/** 
 *  Returns interrupted status and clear interrupted flag.
 *
 * @param[in] thread where to clear interrupt flag
 * @returns TM_ERROR_INTERRUPT if thread was interruped, TM_ERROR_NONE otherwise
 */
UDATA VMCALL hythread_clear_interrupted_other(hythread_t thread) {
        int interrupted = thread->state & TM_THREAD_STATE_INTERRUPTED;
    thread->state &= ~TM_THREAD_STATE_INTERRUPTED;    
        return interrupted?TM_ERROR_INTERRUPT:TM_ERROR_NONE;
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

