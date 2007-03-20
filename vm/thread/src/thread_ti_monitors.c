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
 * @file thread_ti_monitors.c
 * @brief JVMTI raw monitors related functions
 */  

#include <open/hythread_ext.h>
#include "thread_private.h"

static array_t jvmti_monitor_table = 0;
static hymutex_t jvmti_monitor_table_lock;

//#define jrawMonitorID hythread_monitor_t;
/**
 * Initializes raw monitor. 
 *
 * Raw monitors are a simple combination of mutex and conditional variable which is 
 * not associated with any Java object. This function creates the raw monitor at the 
 * address specified as mon_ptr. 
 * User needs to allocate space equal to sizeof(jrawMonitorID) before doing this call.
 *
 * @param[in] mon_ptr address where monitor needs to be created and initialized.
 */
IDATA VMCALL jthread_raw_monitor_create(jrawMonitorID* mon_ptr) {
    hythread_monitor_t monitor;
    IDATA status;
        status = hythread_monitor_init(&monitor, 0);
    if (status != TM_ERROR_NONE) return status;
    // possibly should be moved to jvmti(environment?) init section
    //// 
    if (!jvmti_monitor_table) {
        status =hythread_global_lock();
        if (status != TM_ERROR_NONE) return status;
        if (!jvmti_monitor_table) {
                        
            if (array_create(&jvmti_monitor_table)) {
                hythread_global_unlock();
                return TM_ERROR_OUT_OF_MEMORY;
            }
            status = hymutex_create(&jvmti_monitor_table_lock, TM_MUTEX_NESTED);
            if (status != TM_ERROR_NONE) {
                hythread_global_unlock();
                return status;
            }
        }
        status =hythread_global_unlock();
        if (status != TM_ERROR_NONE) return status;
    }
    
    status =hymutex_lock(&jvmti_monitor_table_lock);
    if (status != TM_ERROR_NONE) return status;
    *mon_ptr = array_add(jvmti_monitor_table, monitor);

    status =hymutex_unlock(&jvmti_monitor_table_lock);
    if (status != TM_ERROR_NONE) return status;
    if (!(*mon_ptr)) return TM_ERROR_OUT_OF_MEMORY;

    return TM_ERROR_NONE;
        
}

/**
 * Destroys raw monitor. 
 *
 * @param[in] mon_ptr address where monitor needs to be destroyed.
 */
IDATA VMCALL jthread_raw_monitor_destroy(jrawMonitorID mon_ptr) {
    IDATA status = 0;
    hythread_monitor_t monitor;
    
    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }

    while (hythread_monitor_destroy((hythread_monitor_t)monitor) != TM_ERROR_NONE) {
        if ((status = hythread_monitor_exit((hythread_monitor_t)monitor)) != TM_ERROR_NONE)
            return status;
    }
    
    status =hymutex_lock(&jvmti_monitor_table_lock);
    if (status != TM_ERROR_NONE) return status;
    array_delete(jvmti_monitor_table, (UDATA)mon_ptr);
    status =hymutex_unlock(&jvmti_monitor_table_lock);
    return status;
}

/**
 * Gains the ownership over monitor.
 *
 * Current thread blocks if the specified monitor is owned by other thread.
 *
 * @param[in] mon_ptr monitor
 */
IDATA VMCALL jthread_raw_monitor_enter(jrawMonitorID mon_ptr) {
    hythread_monitor_t monitor;
    IDATA stat;
    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }
    stat = hythread_monitor_enter(monitor);
    hythread_safe_point();
    return stat;
}

/**
 * Attempt to gain the ownership over monitor without blocking.
 *
 * @param[in] mon_ptr monitor
 * @return 0 in case of successful attempt.
 */
IDATA VMCALL jthread_raw_monitor_try_enter(jrawMonitorID mon_ptr) {    
    hythread_monitor_t monitor;
    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }
    return hythread_monitor_try_enter((hythread_monitor_t)monitor);
}

/**
 * Releases the ownership over monitor.
 *
 * @param[in] mon_ptr monitor
 */
IDATA VMCALL jthread_raw_monitor_exit(jrawMonitorID mon_ptr) {
    hythread_monitor_t monitor;
    IDATA stat;
    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }
    stat = hythread_monitor_exit(monitor);
    hythread_safe_point();
    return stat;
}

/**
 * Wait on the monitor.
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
 * @param[in] mon_ptr monitor
 * @param[in] millis timeout in milliseconds. Zero timeout is not taken into consideration.
 * @return 
 *      TM_ERROR_NONE               success
 *      TM_ERROR_INTERRUPT          wait was interrupted
 *      TM_ERROR_INVALID_MONITOR    current thread isn't the owner
 */
IDATA VMCALL jthread_raw_monitor_wait(jrawMonitorID mon_ptr, I_64 millis) {
    hythread_monitor_t monitor;

    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }

    return hythread_monitor_wait_interruptable(monitor, millis, 0);
}

/**
 * Notifies one thread waiting on the monitor.
 *
 * Only single thread waiting on the
 * object's monitor is waked up.
 * Nothing happens if no threads are waiting on the monitor.
 *
 * @param[in] mon_ptr monitor
 */
IDATA VMCALL jthread_raw_monitor_notify(jrawMonitorID mon_ptr) {
    hythread_monitor_t monitor;
    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }
    return hythread_monitor_notify(monitor);    
}

/**
 * Notifies all threads which are waiting on the monitor.
 *
 * Each thread from the set of threads waiting on the
 * object's monitor is waked up.
 *
 * @param[in] mon_ptr monitor
 */
IDATA VMCALL jthread_raw_monitor_notify_all(jrawMonitorID mon_ptr) {
    hythread_monitor_t monitor;
    if (!(monitor = (hythread_monitor_t)array_get(jvmti_monitor_table, (UDATA)mon_ptr))) {
        return TM_ERROR_INVALID_MONITOR;
    }
    return hythread_monitor_notify_all(monitor);    
}
