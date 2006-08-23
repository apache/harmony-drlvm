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
 * @author Nikolay Kuznetsov
 * @version $Revision: 1.1.2.7 $
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
     hythread_t t = tm_self_tls;     
     assert(t);
     return hysem_wait_interruptable(t->park_event, millis, nanos);
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
    IDATA UNUSED status;
    if(thread ==  NULL) {
        return;
    }
    
    status = hysem_post(thread->park_event);
        assert(status == TM_ERROR_NONE);
}
