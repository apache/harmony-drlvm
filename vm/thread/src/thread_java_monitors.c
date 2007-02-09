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
 * @file thread_java_monitors.c
 * @brief Java thread monitors related functions
 */

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.monitor"
#include <open/jthread.h>
#include <open/hythread_ext.h>
#include <open/thread_externals.h>
#include "thread_private.h"
#include "jni.h"

void add_owned_monitor(jobject monitor);
void remove_owned_monitor(jobject monitor);
void set_contended_monitor(jobject monitor);
void set_wait_monitor(jobject monitor);

/**
 *  Initializes Java monitor.
 *
 *  Monitor is a recursive lock with one conditional variable associated with it.
 *  Implementation may use the knowledge of internal object layout in order to allocate lock
 *  and conditional variable in the most efficient manner.
 *
 *  @param[in] monitor object where monitor needs to be initialized.
 */
IDATA VMCALL jthread_monitor_init(jobject monitor) {        
    
    hythread_thin_monitor_t *lockword;
    IDATA status;
    assert(monitor);
    
    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    status = hythread_thin_monitor_create(lockword); 
    hythread_suspend_enable();
    return status;
}



/**
 * Gains the ownership over monitor.
 *
 * Current thread blocks if the specified monitor is owned by other thread.
 *
 * @param[in] monitor object where monitor is located
 * @sa JNI::MonitorEnter()
 */
IDATA VMCALL jthread_monitor_enter(jobject monitor) {        
    hythread_thin_monitor_t *lockword;
    IDATA status;
    // should be moved to event handler
    apr_time_t enter_begin;
    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;
    int disable_count;

    assert(monitor);
    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    status = hythread_thin_monitor_try_enter(lockword);
    if (status != TM_ERROR_EBUSY) {
        goto entered;
    }

    /////////
#ifdef LOCK_RESERVATION
    // busy unreserve lock before blocking and inflating
    while (TM_ERROR_NONE !=unreserve_lock(lockword)) {
        hythread_yield();
        hythread_safe_point();
        lockword = vm_object_get_lockword_addr(monitor);
    }
    status = hythread_thin_monitor_try_enter(lockword);
    if (status != TM_ERROR_EBUSY) {
        goto entered;
    }
#endif //LOCK_RESERVATION
    tm_native_thread = hythread_self();
    tm_native_thread->state &= ~TM_THREAD_STATE_RUNNABLE;
    tm_native_thread->state |= TM_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;

    // should be moved to event handler
    if (ti_is_enabled()) {
        enter_begin = apr_time_now();
        disable_count =  reset_suspend_disable();
        set_contended_monitor(monitor);
        jvmti_send_contended_enter_or_entered_monitor_event(monitor, 1);
        set_suspend_disable(disable_count);

    }
    
    // busy wait and inflate
    // reload pointer after safepoints
    
    lockword = vm_object_get_lockword_addr(monitor);
    while ((status = hythread_thin_monitor_try_enter(lockword)) == TM_ERROR_EBUSY) {
        hythread_safe_point();
        lockword = vm_object_get_lockword_addr(monitor);
 
        if (is_fat_lock(*lockword)) {
            status = hythread_thin_monitor_enter(lockword);
             if (status != TM_ERROR_NONE) {
                 hythread_suspend_enable();
                 assert(0);
                 return status;
             }
             goto contended_entered; 
        }
        hythread_yield();
    }
    assert(status == TM_ERROR_NONE);
    if (!is_fat_lock(*lockword)) {
        inflate_lock(lockword);
    }
// do all ti staff here
contended_entered:
    if (ti_is_enabled()) {
        disable_count =  reset_suspend_disable();
        jvmti_send_contended_enter_or_entered_monitor_event(monitor, 0);
        set_suspend_disable(disable_count);
        // should be moved to event handler
        tm_java_thread = hythread_get_private_data(hythread_self());
        tm_java_thread->blocked_time += apr_time_now()- enter_begin;
        /////////
    }
    tm_native_thread->state &= ~TM_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;
    tm_native_thread->state |= TM_THREAD_STATE_RUNNABLE;

entered:
   if (ti_is_enabled()) { 
      add_owned_monitor(monitor);
   }
    hythread_suspend_enable();
    return TM_ERROR_NONE;
}

/**
 * Attempt to gain the ownership over monitor without blocking.
 *
 * @param[in] monitor object where monitor is located
 */
IDATA VMCALL jthread_monitor_try_enter(jobject monitor) {        
    hythread_thin_monitor_t *lockword;
    IDATA status;
    assert(monitor);

    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    status = hythread_thin_monitor_try_enter(lockword);
    
    hythread_suspend_enable();
    if (status == TM_ERROR_NONE && ti_is_enabled()) {
        add_owned_monitor(monitor);
    }
    return status;
}

/**
 * Releases the ownership over monitor.
 *
 * @param[in] monitor monitor
 * @sa JNI::MonitorExit()
 */
IDATA VMCALL jthread_monitor_exit(jobject monitor) {
    hythread_thin_monitor_t *lockword;
    IDATA status;
    assert(monitor);

    hythread_suspend_disable();

    lockword = vm_object_get_lockword_addr(monitor);
    status =  hythread_thin_monitor_exit(lockword);
    hythread_suspend_enable();
    if (status == TM_ERROR_NONE && ti_is_enabled()) {
        remove_owned_monitor(monitor);
    }
    if (status == TM_ERROR_ILLEGAL_STATE) {
        jthread_throw_exception("java/lang/IllegalMonitorStateException", "Illegal monitor state");
    }
    return status;
}

/**
 * Notifies one thread waiting on the monitor.
 *
 * Only single thread waiting on the
 * object's monitor is waked up.
 * Nothing happens if no threads are waiting on the monitor.
 *
 * @param[in] monitor object where monitor is located
 * @sa java.lang.Object.notify() 
 */
IDATA VMCALL jthread_monitor_notify(jobject monitor) {    
    hythread_thin_monitor_t *lockword;
    IDATA status;
    assert(monitor);

    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    status = hythread_thin_monitor_notify(lockword);
    hythread_suspend_enable();

    return status;
}

/**
 * Notifies all threads which are waiting on the monitor.
 *
 * Each thread from the set of threads waiting on the
 * object's monitor is waked up.
 *
 * @param[in] monitor object where monitor is located
 * @sa java.lang.Object.notifyAll() 
 */
IDATA VMCALL jthread_monitor_notify_all(jobject monitor) {    
    hythread_thin_monitor_t *lockword;
    IDATA status;
    assert(monitor);

    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    status = hythread_thin_monitor_notify_all(lockword);
    hythread_suspend_enable();

    return status;
}

/**
 * Wait on the <code>object</code>'s monitor.
 *
 * This function instructs the current thread to be scheduled off 
 * the processor and wait on the monitor until the following occurs: 
 * <UL>
 * <LI>another thread invokes <code>thread_notify(object)</code>
 * and VM chooses this thread to wake up;
 * <LI>another thread invokes <code>thread_notifyAll(object);</code>
 * <LI>another thread invokes <code>thread_interrupt(thread);</code>
 * </UL>
 *
 * @param[in] monitor object where monitor is located
 * @sa java.lang.Object.wait()
 * @return 
 */
IDATA VMCALL jthread_monitor_wait(jobject monitor) {
    return jthread_monitor_timed_wait(monitor, 0, 0);
}

/**
 * Wait on the <code>object</code>'s monitor with the specified timeout.
 *
 * This function instructs the current thread to be scheduled off 
 * the processor and wait on the monitor until the following occurs: 
 * <UL>
 * <LI>another thread invokes <code>thread_notify(object)</code>
 * and VM chooses this thread to wake up;
 * <LI>another thread invokes <code>thread_notifyAll(object);</code>
 * <LI>another thread invokes <code>thread_interrupt(thread);</code>
 * <LI>real time elapsed from the waiting begin is
 * greater or equal the timeout specified.
 * </UL>
 *
 * @param[in] monitor object where monitor is located
 * @param[in] millis time to wait (in milliseconds)
 * @param[in] nanos time to wait (in nanoseconds)
 * @sa java.lang.Object.wait()
 */
IDATA VMCALL jthread_monitor_timed_wait(jobject monitor, jlong millis, jint nanos) {
    hythread_thin_monitor_t *lockword;
    IDATA status;
    hythread_t tm_native_thread;
    apr_time_t wait_begin;
    jvmti_thread_t tm_java_thread;
    int disable_count;
    ///////

    assert(monitor);

    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    if (!is_fat_lock(*lockword)) {
        if (!owns_thin_lock(hythread_self(), *lockword)) {
            TRACE(("ILLEGAL_STATE wait %x\n", lockword));
            hythread_suspend_enable();
            return TM_ERROR_ILLEGAL_STATE;  
        }    
        inflate_lock(lockword);
    }

    if (ti_is_enabled()) {
        disable_count =  reset_suspend_disable();
        set_wait_monitor(monitor);
        set_contended_monitor(monitor);
        jvmti_send_wait_monitor_event(monitor, (jlong)millis);
        jvmti_send_contended_enter_or_entered_monitor_event(monitor, 1);
        set_suspend_disable(disable_count);

        // should be moved to event handler
        wait_begin = apr_time_now();
        ////////
        remove_owned_monitor(monitor);
    }

    tm_native_thread = hythread_self();
    tm_native_thread->state &= ~TM_THREAD_STATE_RUNNABLE;
    tm_native_thread->state |= TM_THREAD_STATE_WAITING |
            TM_THREAD_STATE_IN_MONITOR_WAIT;

    if ((millis > 0) || (nanos > 0)) {
       tm_native_thread->state |= TM_THREAD_STATE_WAITING_WITH_TIMEOUT;
    } else {
       tm_native_thread->state |= TM_THREAD_STATE_WAITING_INDEFINITELY;
    }

    status = hythread_thin_monitor_wait_interruptable(lockword, millis, nanos);

    tm_native_thread->state &= ~(TM_THREAD_STATE_WAITING | 
			                         TM_THREAD_STATE_IN_MONITOR_WAIT);
    if ((millis > 0) || (nanos > 0)) {
       tm_native_thread->state &= ~TM_THREAD_STATE_WAITING_WITH_TIMEOUT;
    } else {
       tm_native_thread->state &= ~TM_THREAD_STATE_WAITING_INDEFINITELY;
    }
    tm_native_thread->state |= TM_THREAD_STATE_RUNNABLE;

    hythread_suspend_enable();
    if (ti_is_enabled()) {
        add_owned_monitor(monitor);
        disable_count =  reset_suspend_disable();
        jvmti_send_contended_enter_or_entered_monitor_event(monitor, 0);
        jvmti_send_waited_monitor_event(monitor, (status == APR_TIMEUP)?(jboolean)1:(jboolean)0);
        // should be moved to event handler
        set_suspend_disable(disable_count);
        tm_java_thread = hythread_get_private_data(hythread_self());
        tm_java_thread->waited_time += apr_time_now()- wait_begin;
        /////////
    }
    return status;
}

void add_owned_monitor(jobject monitor) {
    hythread_t tm_native_thread = hythread_self();
    jvmti_thread_t tm_java_thread = hythread_get_private_data(tm_native_thread);
    int disable_status;
    TRACE(("TM: add owned monitor: %x", monitor));
    if (!tm_java_thread) return;
    
    disable_status = reset_suspend_disable();
    if (tm_java_thread->contended_monitor) {
        (*(tm_java_thread->jenv))->DeleteGlobalRef(tm_java_thread->jenv, tm_java_thread->contended_monitor);
        tm_java_thread->contended_monitor = NULL;
    }

    if (tm_java_thread->wait_monitor) {
        (*(tm_java_thread->jenv))->DeleteGlobalRef(tm_java_thread->jenv, tm_java_thread->wait_monitor);
        tm_java_thread->wait_monitor = NULL;
    }
    
    
    if (!tm_java_thread->owned_monitors) {
         tm_java_thread->owned_monitors = apr_pcalloc(tm_java_thread->pool,
                            MAX_OWNED_MONITOR_NUMBER * sizeof(jobject));
         assert(tm_java_thread->owned_monitors);
         tm_java_thread->owned_monitors_nmb = 0;
    }
    assert(tm_java_thread->owned_monitors_nmb < MAX_OWNED_MONITOR_NUMBER);
  
    tm_java_thread->owned_monitors[tm_java_thread->owned_monitors_nmb] 
        = (*(tm_java_thread->jenv))->NewGlobalRef(tm_java_thread->jenv, monitor);
    set_suspend_disable(disable_status);
    tm_java_thread->owned_monitors_nmb++;
}

void remove_owned_monitor(jobject monitor) {
    int i,j, disable_status;
    hythread_t tm_native_thread = hythread_self();
    jvmti_thread_t tm_java_thread = hythread_get_private_data(tm_native_thread);
    
    TRACE(("TM: remove owned monitor: %x", monitor));
    
    if (!tm_java_thread) return;
    for (i = tm_java_thread->owned_monitors_nmb - 1; i >= 0; i--) {
        if (vm_objects_are_equal(tm_java_thread->owned_monitors[i], monitor)) {
            disable_status = reset_suspend_disable();
            (*(tm_java_thread->jenv))->DeleteGlobalRef(tm_java_thread->jenv, tm_java_thread->owned_monitors[i]);
            set_suspend_disable(disable_status);
            for (j = i; j < tm_java_thread->owned_monitors_nmb - 1; j++) {
                tm_java_thread->owned_monitors[j] = tm_java_thread->owned_monitors[j + 1];
            }
            tm_java_thread->owned_monitors_nmb--;
            return;
        }
    }
    //assert(0); monitor - it is no valid monitor
}

void set_contended_monitor(jobject monitor) {
    hythread_t tm_native_thread = hythread_self();
    IDATA suspend_status;
    
    jvmti_thread_t tm_java_thread = hythread_get_private_data(tm_native_thread);

    if (!tm_java_thread) return;
    
    suspend_status = reset_suspend_disable();
    tm_java_thread->contended_monitor = (*(tm_java_thread->jenv))->NewGlobalRef(tm_java_thread->jenv, monitor);

    set_suspend_disable(suspend_status);
}

void set_wait_monitor(jobject monitor) {
    hythread_t tm_native_thread = hythread_self();
    IDATA suspend_status;
    
    jvmti_thread_t tm_java_thread = hythread_get_private_data(tm_native_thread);

    if (!tm_java_thread) return;

    suspend_status = reset_suspend_disable();
    assert(suspend_status == TM_ERROR_NONE);
    tm_java_thread->wait_monitor = (*(tm_java_thread->jenv))->NewGlobalRef(tm_java_thread->jenv, monitor);
    set_suspend_disable(suspend_status);
}
