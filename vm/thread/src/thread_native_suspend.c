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
 * @file thread_native_suspend.c
 * @brief Hythread suspend/resume related functions
 */

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.suspend"

#include <open/hythread_ext.h>
#include <open/thread_externals.h>
#include "thread_private.h"
#include <apr_atomic.h>

static void thread_safe_point_impl(hythread_t thread);

/** @name Safe suspension support
 */
//@{

/**
 * Returns non-zero if thread is suspended.
 */
IDATA VMCALL hythread_is_suspend_enabled() {
    return tm_self_tls->suspend_disable_count == 0;
}


/**
 * Denotes the beginning of the code region where safe suspension is possible.
 *
 * First, this method sets the suspend_enabled state flag to true.
 * If there was a suspension request set for this thread, this method notifies the 
 * requesting thread that a safe region is reached.
 * <p>
 * A thread marks itself with functions tmn_suspend_enable() and tmn_suspend_disable() in order
 * to denote a safe region of code. It may also call safe_point() method to denote a selected 
 * point where safe suspension is possible.
 */
void VMCALL hythread_suspend_enable() {
    register hythread_t thread;
    assert(!hythread_is_suspend_enabled());

#ifdef FS14_TLS_USE
    __asm { 
         mov eax, fs:[0x14] 
         dec [eax]HyThread.suspend_disable_count
         mov eax, [eax]HyThread.suspend_request
         test eax, eax
         jnz suspended
        
    } 
    return;

suspended:
    thread=tm_self_tls;

#else 
    thread=tm_self_tls;
    thread->suspend_disable_count--;

#endif
 //   if (!thread->suspend_request  || thread->suspend_disable_count!=0) {
   //     return;
   // }
        
   // hylatch_count_down(thread->safe_region_event);
}

/**
 * Denotes the end of the code region where safe suspension was possible.
 *
 * This method sets the suspend_enabled state flag to false and then invokes
 * tmn_safe_point().
 * <p>
 * A thread marks itself with functions tmn_suspend_enable() and tmn_suspend_disable() in order
 * to denote a safe region of code. It may also call safe_point() method to denote a selected 
 * point where safe suspension is possible.
 */
void VMCALL hythread_suspend_disable()
{   
    register hythread_t thread;
#ifndef NDEBUG
    // Check that current thread is in default thread group.
    // Justification: GC suspends and enumerates threads from default group only.
    assert(tm_self_tls->group == TM_DEFAULT_GROUP);
#endif

#ifdef FS14_TLS_USE
    __asm { 
         mov eax, fs:[0x14] 
         inc [eax]HyThread.suspend_disable_count
         mov eax, [eax]HyThread.suspend_request
         test eax, eax
         jnz suspended
        
    } 
    return;

suspended:
    thread=tm_self_tls;

#else 
    thread=tm_self_tls;
    thread->suspend_disable_count++;
#endif

    if (!thread->suspend_request  || thread->suspend_disable_count!=1) {
        return;
    }
    thread_safe_point_impl(thread);
}



/**
 * Denotes a single point where safe suspension is possible.
 *
 * If there was a suspension request set for this thread, this method notifies
 * the requesting thread and then blocks until someone calls the tmn_resume() 
 * for this thread.
 * <p>
 * A thread marks itself with functions tmn_suspend_enable() and tmn_suspend_disable() in order
 * to denote a safe region of code. It may also call safe_point() method to denote a selected 
 * point where safe suspension is possible.
 */
void VMCALL hythread_safe_point() {
    thread_safe_point_impl(tm_self_tls);
}

static void thread_safe_point_impl(hythread_t thread) { 
    hythread_event_callback_proc callback_func;
    if (thread->suspend_request >0) {
        
        int old_status = thread->suspend_disable_count;
        do {
            TRACE(("TM: safe point enter: thread: %p count: %d dis count: %d", 
                thread, thread->suspend_request, thread->suspend_disable_count));
            
            if (thread->safepoint_callback) {
                thread->suspend_disable_count = 1;
                // Clear callback (this is one-time event)
                callback_func = thread->safepoint_callback;
                thread->safepoint_callback = NULL;
                
                // since set callback suspended the thread
                // restore its original state
                hythread_resume(tm_self_tls);
                callback_func();
            }
            
            thread->suspend_disable_count = 0;
            
            apr_memory_rw_barrier();
            // code for Ipf that support StackIterator and immediate suspend
            // notify suspender
            //  hylatch_count_down(thread->safe_region_event);

            // wait for resume event
            hysem_wait(thread->resume_event);
            TRACE(("TM: safe point resume: thread: %p count: %d", thread, thread->suspend_request));

            thread->suspend_disable_count = old_status;
            apr_memory_rw_barrier();
        } while (thread->suspend_request >0);
    }
} // thread_safe_point_impl


// the function start suspension.
// call wait_safe_region_event() should be called to wait for safe region or safe point.
// the function do not suspend self.
static void send_suspend_request(hythread_t thread) {

    assert(thread->suspend_request >=0);
    // already suspended?
    if (thread->suspend_request > 0) {
        apr_atomic_inc32((apr_uint32_t *)&(thread->suspend_request));
        return;
    }               
                
    //we really need to suspend thread.

    hysem_set(thread->resume_event, 0);
                
    apr_atomic_inc32((apr_uint32_t *)&(thread->suspend_request));

    apr_thread_yield_other(thread->os_handle);

    TRACE(("TM: suspend request sent: %p request count: %d",thread , thread->suspend_request));
}


// the second part of suspension
// blocked in case was selfsuspended.
static IDATA wait_safe_region_event(hythread_t thread) {
    assert(thread->suspend_request >= 1);
    if(thread == tm_self_tls) {
        TRACE(("TM: suspend wait self exit thread: %p request count: %d",thread , thread->suspend_request));
        return TM_ERROR_NONE;
    }               
                // we need to wait for notification only in case the thread is in the unsafe/disable region
    while (thread->suspend_disable_count) {
        // HIT cyclic suspend
        if(tm_self_tls->suspend_request > 0) {
             return TM_ERROR_EBUSY; 
        }
        hythread_yield();
    }
    TRACE(("TM: suspend wait exit safe region thread: %p request count: %d",thread , thread->suspend_request));
    thread->state |= TM_THREAD_STATE_SUSPENDED;
    return TM_ERROR_NONE; 
}

/**
 * Suspends the current thread. 
 * 
 * Stop the current thread from executing until it is resumed.
 * 
 * @return none
 *
 * @see hythread_resume
 */
void VMCALL hythread_suspend() {
    hythread_t thread = tm_self_tls;

    apr_atomic_inc32((apr_uint32_t *)&(thread->suspend_request));

    hythread_safe_point();
}


/**
 * Safely suspends the <code>thread</code> execution.
 *
 * This method is a SAFE_POINT
 *
 * The safe suspension acts as follows:
 * <ul>
 * <li>
 * If the <code>thread</code> is currently running in safe code region, this
 * method immediately returns back.
 * The <code>thread</code> itself runs until it reaches the end of safe region
 * and then blocks until someone calls tmn_resume() for it.
 * <li>
 * If the <code>thread</code> is currently in unsafe region, this
 * method blocks until the <code>thread</code> either reaches the beginning 
 * of a safe region, or reaches a safe point. 
 * Once reached safe point or end of safe region, the<code>thread</code> blocks 
 * until someone calls tmn_resume() for it.
 * </ul>
 * A thread marks itself with functions tmn_suspend_enable() and tmn_suspend_disable() in order
 * to denote a safe region of code. It may also call safe_point() method to denote a selected 
 * point where safe suspension is possible.
 *
 * @param[in] thread thread to be suspended
 * @return TM_ERROR_EBUSY if deadlock, TM_ERROR_NONE if OK  
 */
IDATA VMCALL hythread_suspend_other(hythread_t thread) {
    hythread_t self;
    self = tm_self_tls;
    TRACE(("TM: suspend one enter thread: %p self: %p request count: %d",thread , tm_self_tls, thread->suspend_request));
    if (self == thread) {
        hythread_suspend();
        return TM_ERROR_NONE; 
    }

    send_suspend_request(thread);
    while (wait_safe_region_event(thread)!=TM_ERROR_NONE) {
        if (self->suspend_request>0) {
            hythread_resume(thread);
            return TM_ERROR_EBUSY;
        }
    }
    TRACE(("TM: suspend one exit thread: %p request count: %d",thread , thread->suspend_request));
        
    return TM_ERROR_NONE;
}

/**
 * Resume a thread.
 *
 * Take a threads out of the suspended state.
 *
 * If the thread is not suspended, no action is taken.
 *
 * @param[in] thread a thread to be resumed
 * @return none
 *
 * @see hythread_create, hythread_suspend
 */
void VMCALL hythread_resume(hythread_t thread) {
    TRACE(("TM: start resuming: %p request count: %d",thread , thread->suspend_request));
    // If there was request for suspension, decrease the request counter
    //printf("resume other now lock %d  %d  %d  %d\n",tm_self_tls->thread_id,tm_self_tls->suspend_disable_count,thread->thread_id,thread->suspend_disable_count);
    if (thread->suspend_request > 0) {
        if (thread->safepoint_callback && thread->suspend_request < 2) return;
        apr_atomic_dec32((apr_uint32_t *)&(thread->suspend_request));
        if (thread->suspend_request == 0) {
            // Notify the thread that it may wake up now
            hysem_post(thread->resume_event);            
            TRACE(("TM: resume one thread: %p request count: %d",thread , thread->suspend_request));
            thread->state &= ~TM_THREAD_STATE_SUSPENDED;
        }
    }
    //printf("resume other now lock-compl %d  %d  %d  %d\n",tm_self_tls->thread_id,tm_self_tls->suspend_disable_count,thread->thread_id,thread->suspend_disable_count);
}

/**
 * Sets safepoint callback function.
 * 
 * Callback function is executed at safepoint in case there was a suspension request.
 *  
 * @param[in] thread thread where callback needs to be executed
 * @param[in] callback callback function
 */
IDATA hythread_set_safepoint_callback(hythread_t thread, tm_thread_event_callback_proc callback) {
    IDATA status;
    while (apr_atomic_casptr((volatile void **)&thread->safepoint_callback, (void *)callback, (void *)NULL) != NULL);
    if (tm_self_tls == thread) {
        int old_status = thread->suspend_disable_count;
        thread->suspend_disable_count = 1;
        hythread_suspend();
        thread->suspend_disable_count = old_status;
    } else {
        //we will not have notification from the
        //target thread if safe_point call back will not exit
        ////
        send_suspend_request(thread);
        //let the thread execute safe point in the case it's already suspended
        ////
        status = hysem_post(thread->resume_event);
        
    }
    
    if (thread->current_condition) {
        status=hycond_notify_all(thread->current_condition);   
        assert(status == TM_ERROR_NONE);
    }

    return TM_ERROR_NONE;
}

/**
 * Returns safepoint callback function.
 * 
 * @param[in] t thread where callback needs to be executed
 * @return callback function currently installed, or NULL if there was none
 */
hythread_event_callback_proc VMCALL hythread_get_safepoint_callback(hythread_t t) {
    return t->safepoint_callback;
}

/**
 * Helps to safely suspend the threads in the selected group.
 *
 * This method sets a suspend request for the every thread in the group 
 * and then returns the iterator that can be used to traverse through the suspended threads.
 * Each invocation of the tmn_iterator_next() method on the iterator will return the next 
 * suspended thread.
 *
 * @param[out] t iterator 
 * @param[in] group thread group to be suspended
 */
IDATA VMCALL hythread_suspend_all(hythread_iterator_t *t, hythread_group_t group) {
    hythread_t self = tm_self_tls;
    hythread_t next;
    hythread_iterator_t iter;
    TRACE(("TM: suspend all"));
    
    self = tm_self_tls;
    // try to prevent cyclic suspend dead-lock
    while (self->suspend_request > 0) {
        thread_safe_point_impl(self);
    }

    iter = hythread_iterator_create(group);
    // send suspend requests to all threads
    TRACE(("TM: send suspend requests"));
    while ((next = hythread_iterator_next(&iter)) != NULL) {
        if (next != self) {
            send_suspend_request(next);
        }       
    }
    hythread_iterator_reset(&iter);
    // all threads should be stopped in safepoints or be in safe region.
    TRACE(("TM: wait suspend responses"));
    while ((next = hythread_iterator_next(&iter)) != NULL) {
        if (next != self) {
            while (wait_safe_region_event(next)!=TM_ERROR_NONE) {
                thread_safe_point_impl(tm_self_tls);
                hythread_yield();
            }
        }       
    }
    
    hythread_iterator_reset(&iter);
    hythread_iterator_release(&iter);
    if (t) {
        *t=iter;
    }
       
    return TM_ERROR_NONE;
}



/**
 * Resumes all threads in the selected group.
 *
 * @param[in] group thread group to be resumed
 */
IDATA VMCALL hythread_resume_all(hythread_group_t  group) {
    hythread_t self = tm_self_tls;
    hythread_t next;
    hythread_iterator_t iter;
    iter = hythread_iterator_create(group);
    TRACE(("TM: resume all"));
    // send suspend requests to all threads
    while ((next = hythread_iterator_next(&iter)) != NULL) {
        if (next != self) {
            hythread_resume(next);
        }       
    }
        
    hythread_iterator_release(&iter); 
    return TM_ERROR_NONE;
}


// Private functionality

int reset_suspend_disable() {
    hythread_t self = tm_self_tls;
    int dis = self->suspend_disable_count;
    self->suspend_disable_count = 0;
    if (self->suspend_request >0) {
        // notify suspender
        hylatch_count_down(self->safe_region_event);
    }
    return dis;
}

void set_suspend_disable(int count) {
    hythread_t self = tm_self_tls;
    assert(count>=0);
    self->suspend_disable_count = count;
    if (count) {
        thread_safe_point_impl(self);
    }
}

//@}
