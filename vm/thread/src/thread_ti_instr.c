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
 * @file thread_ti_instr.c
 * @brief JVMTI basic related functions
 */  

#include <open/ti_thread.h>
#include <open/hythread_ext.h>
#include <open/jthread.h>
#include "thread_private.h"

/**
 * Returns the list of all Java threads.
 *
 * @param[out] threads resulting threads list
 * @param[out] count_ptr number of threads in the resulting list
 */
IDATA VMCALL jthread_get_all_threads(jthread** threads, jint *count_ptr) {

    hythread_group_t  java_thread_group = get_java_thread_group();
    hythread_iterator_t  iterator;
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    jthread* java_threads;
    int i;
    int count = 0;
    int java_thread_count = 0;
    IDATA status;
    //apr_status_t apr_status; 
    //apr_pool_t *pool;

    assert(java_thread_group);
    iterator = hythread_iterator_create(java_thread_group);
    count = hythread_iterator_size (iterator);
    for (i = 0; i < count; i++) {
        tm_native_thread = hythread_iterator_next(&iterator);
        tm_java_thread = hythread_get_private_data(tm_native_thread);
        if (tm_java_thread) {
            java_thread_count++;
        }
    }
    /*apr_status = apr_pool_create(&pool, 0);
        if (apr_status != APR_SUCCESS) {
                hythread_iterator_release(&iterator);
                return CONVERT_ERROR(apr_status);
        }
    java_threads = apr_palloc(pool, sizeof(jthread)* java_thread_count);*/
    java_threads = (jthread*)malloc(sizeof(jthread)* java_thread_count);
    if (!java_threads) {
        hythread_iterator_release(&iterator);
        return TM_ERROR_OUT_OF_MEMORY;
    }
    hythread_iterator_reset(&iterator);
    java_thread_count = 0;
    for (i = 0; i < count; i++) {
        tm_native_thread = hythread_iterator_next(&iterator);
        tm_java_thread = hythread_get_private_data(tm_native_thread);
        if (tm_java_thread) {
            java_threads[java_thread_count] = tm_java_thread->thread_object;
            java_thread_count++;
        }
    }
    *threads = java_threads;
    *count_ptr = java_thread_count;
    status = hythread_iterator_release(&iterator);

    return status;
}
/*
 */
IDATA deads_expand(jthread **deads, int deads_size) {

    jthread *new_deads;
        int i;
    new_deads = (jthread *)malloc(sizeof(jthread) * deads_size * 2);
    if (!new_deads) return TM_ERROR_OUT_OF_MEMORY;

    for (i = 0; i < deads_size; i++) {
        new_deads[i] = (*deads)[i];
    }
    *deads = new_deads;
    return TM_ERROR_NONE;
}
/*
 */
int deads_find(jobject thread, jobject *deads, int base, int top, int deads_size) {
    int i;

    for (i = 0; i < top; i++) {
        if (vm_objects_are_equal(thread, deads[i])) {
            return 1;
        }
    }
    return 0;
}
// FIXME: synchronization and maybe thread suspension needed
/**
 * Checks for the deadlock conditions within the specified thread list.
 *
 * @param[in] thread_list thread list where to search for deadlock conditions
 * @param[in] thread_count number of threads in the thread list
 * @param[out] dead_list deadlocked threads
 * @param[out] dead_count number of deadlocked threads
 */
IDATA VMCALL jthread_get_deadlocked_threads(jthread *thread_list, jint thread_count, jthread **dead_list, jint *dead_count) {
    jthread *deads;
    int deads_size = 1;
    int deads_base = 0;
    int deads_top = 0;
    jthread *output;
    int output_top = 0;
    jobject monitor;
    jthread thread;
    //apr_pool_t *pool;
    /*apr_pool_t *pool_out;
    apr_status_t apr_status;*/
    IDATA status;
    int i;

    /*apr_status = apr_pool_create(&pool, NULL);
    if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);

    deads = apr_palloc(pool, sizeof(jthread) * deads_size);
    output = apr_palloc(pool, sizeof(jthread) * thread_count);
    if (!deads || !output) return TM_ERROR_OUT_OF_MEMORY;*/

    deads = (jthread *)malloc(sizeof(jthread) * deads_size);
    output = (jthread *)malloc(sizeof(jthread) * thread_count);
    if ((deads==NULL)||(output==NULL)) {
        return TM_ERROR_OUT_OF_MEMORY;
    }
    for (i = 0; i < thread_count; i++) {
        thread = thread_list[i];
        while (1) {
            status=jthread_get_contended_monitor(thread, &monitor);
            if (status != TM_ERROR_NONE) return status;
            if (! monitor) {
                deads_top = deads_base; // remove frame
                break;
            }
            if (deads_find(thread, deads, deads_base, deads_top, deads_size)) {
                output[output_top] = thread;
                output_top++;
                deads_base = deads_top; // add frame
                break;
            }
            if (deads_top == deads_size) {
                status = deads_expand(&deads, deads_size);
                if (status != TM_ERROR_NONE) return status;
            }
            deads[deads_top] = thread;
            deads_top++;
            status = jthread_get_lock_owner(monitor, &thread);
            if (status != TM_ERROR_NONE) return status;
        }
    }

    if (output_top > 0) {
        /* apr_status = apr_pool_create(&pool_out, NULL);
        if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);*/
        *dead_list = (jthread *)malloc(sizeof(jthread) * output_top);
        if (! *dead_list) return TM_ERROR_OUT_OF_MEMORY;

        for (i = 0; i < output_top; i++) {
            (*dead_list)[i] = output[i];
        }
    } else {
        *dead_list = NULL;
    }
    *dead_count = output_top;

    return TM_ERROR_NONE;
}

/**
 * Returns the number of all Java threads.
 *
 * @param[out] count_ptr number of threads.
 */
IDATA VMCALL jthread_get_thread_count(jint *count_ptr) {

    hythread_group_t  java_thread_group = get_java_thread_group();
    hythread_iterator_t  iterator;
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    int i;
    int count = 0;
    int java_thread_count = 0;
    IDATA status;

    assert(java_thread_group);
    iterator = hythread_iterator_create(java_thread_group);
    count = hythread_iterator_size (iterator);
    for (i = 0; i < count; i++) {
        tm_native_thread = hythread_iterator_next(&iterator);
        tm_java_thread = hythread_get_private_data(tm_native_thread);
        if (tm_java_thread) {
            java_thread_count++;
        }
    }
    *count_ptr = java_thread_count;
    status = hythread_iterator_release(&iterator);

    return status;
}

/**
 * Returns the number of total started Java threads.
 *
 * @param[out] count_ptr number of started threads.
 */
IDATA VMCALL jthread_get_total_started_thread_count(jint *count_ptr) {
    *count_ptr = total_started_thread_count;
    return TM_ERROR_NONE;
}

/**
 * Returns the number of blocked threads.
 *
 * @param[out] count_ptr number of threads.
 */
IDATA VMCALL jthread_get_blocked_count(jint* count_ptr) {

    hythread_group_t  java_thread_group = get_java_thread_group();
    hythread_iterator_t  iterator;
    hythread_t tm_native_thread;
    int nmb = 0;
    int count;
    IDATA status;

    assert(java_thread_group);
    iterator = hythread_iterator_create(java_thread_group);
    count = hythread_iterator_size (iterator);

    while (hythread_iterator_has_next(iterator)) {
        tm_native_thread = hythread_iterator_next(&iterator);
        if (tm_native_thread && hythread_is_blocked_on_monitor_enter(tm_native_thread)) {
            nmb++;
        }
    }
    *count_ptr = nmb;
    status = hythread_iterator_release(&iterator);

        return status;
}

/**
 * Returns the number of waiting threads.
 *
 * @param[out] count number of threads.
 */
IDATA VMCALL jthread_get_waited_count(jint* count) {

    hythread_group_t  java_thread_group = get_java_thread_group();
    hythread_iterator_t  iterator;
    hythread_t tm_native_thread;
    int nmb = 0;
    IDATA status;

    assert(java_thread_group);
    iterator = hythread_iterator_create(java_thread_group);

    while (hythread_iterator_has_next(iterator)) {
        tm_native_thread = hythread_iterator_next(&iterator);
        //if (hythread_is_in_monitor_wait(tm_native_thread)) { ???????????????????????
        if (hythread_is_waiting(tm_native_thread)) {
            nmb++;
        }
    }
    *count = nmb;
    status = hythread_iterator_release(&iterator);

    return status;
}

/**
 * Returns the <code>thread</code>'s state according
 * to JVMTI specification. See <a href=http://java.sun.com/j2se/1.5.0/docs/guide/jvmti/jvmti.html#GetThreadState> 
 * JVMTI Specification </a> for more details.
 *
 * @param[in] java_thread thread those state is to be queried
 * @param[out] state resulting thread state
 * 
 */
IDATA VMCALL jthread_get_state(jthread java_thread, jint *state) {
    hythread_t tm_native_thread;

    assert(java_thread);
    assert(state);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);

    *state = 0;
    if (! tm_native_thread) return TM_ERROR_NONE; // Not started yet

    if (hythread_is_alive(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_ALIVE;}
    if (hythread_is_runnable(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_RUNNABLE;}
    if (hythread_is_blocked_on_monitor_enter(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;}
    if (hythread_is_waiting(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_WAITING;}
    if (hythread_is_waiting_indefinitely(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_WAITING_INDEFINITELY;}
    if (hythread_is_waiting_with_timeout(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;}
    if (hythread_is_sleeping(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_SLEEPING;}
    if (hythread_is_in_monitor_wait(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_IN_OBJECT_WAIT;}
    if (hythread_is_parked(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_PARKED;}
    if (hythread_is_suspended(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_SUSPENDED;}
    if (hythread_interrupted(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_INTERRUPTED;}
    if (hythread_is_in_native(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_IN_NATIVE;}
    if (hythread_is_terminated(tm_native_thread)) {*state |= JVMTI_THREAD_STATE_TERMINATED;}

    return TM_ERROR_NONE;
}
 
/**
 * Puts the data into thread local storage.
 *
 * @param[in] java_thread thread where to put the data
 * @param[in] data data to be put
 */
IDATA VMCALL jthread_set_local_storage(jthread java_thread, const void* data) {
    hythread_t tm_native_thread;

    assert(java_thread);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    assert(tm_native_thread);

    return hythread_set_private_data(tm_native_thread, (void *)data);
}

/**
 * Extracts the data from the thread local storage.
 *
 * @param[in] java_thread thread where to get the data
 * @param[out] data_ptr pointer to the data
 */
IDATA VMCALL jthread_get_local_storage(jthread java_thread, void** data_ptr) {
    hythread_t tm_native_thread;

    assert(java_thread);
    assert(data_ptr);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    assert(tm_native_thread);
    *data_ptr = hythread_get_private_data (tm_native_thread);

    return TM_ERROR_NONE;
}

/**
 * Returns true if specified thread holds the lock associated with the given monitor.
 *
 * @param[in] thread thread which may hold the lock
 * @param[in] monitor object those monitor is possibly locked
 * @return true if thread holds the lock, false otherwise;
 */
jboolean VMCALL jthread_holds_lock(jthread thread, jobject monitor) {

   jthread lock_owner;
   IDATA status;
   jboolean res;

   status = jthread_get_lock_owner(monitor, &lock_owner);
   assert(status == TM_ERROR_NONE);

   hythread_suspend_disable();
   res = vm_objects_are_equal(thread, lock_owner);
   hythread_suspend_enable();

   return res;
}

/**
 * Returns the monitor the specific thread is currently contending for.
 *
 * @param[in] java_thread thread to be explored for contention
 * @param[out] monitor monitor the thread <code>thread</code> is currently contending for, 
 * or NULL if thread doesn't contend for any monitor.
 */
IDATA VMCALL jthread_get_contended_monitor(jthread java_thread, jobject* monitor) {

    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;

    assert(java_thread);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    assert(tm_java_thread);
    *monitor = tm_java_thread->contended_monitor;
    return TM_ERROR_NONE;
}

/**
 * Returns the monitor the specific thread is currently waiting on.
 *
 * @param[in] java_thread thread to be explored for contention
 * @param[out] monitor monitor the thread <code>thread</code> is currently contending for, 
 * or NULL if thread doesn't contend for any monitor.
 */
IDATA VMCALL jthread_get_wait_monitor(jthread java_thread, jobject* monitor) {

    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;

    assert(java_thread);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    assert(tm_java_thread);
    *monitor = tm_java_thread->wait_monitor;
    return TM_ERROR_NONE;
}

/**
 * Returns the owner of the lock associated with the given monitor.
 *
 * If the given monitor is not owned by any thread, NULL is returned.
 *
 * @param[in] monitor monitor those owner needs to be determined
 * @param[out] lock_owner thread which owns the monitor
 */
IDATA VMCALL jthread_get_lock_owner(jobject monitor, jthread* lock_owner) {

    hythread_t tm_native_thread = NULL;
    jvmti_thread_t tm_java_thread = NULL;
    hythread_thin_monitor_t *lockword;

    assert(monitor);
    hythread_suspend_disable();
    lockword = vm_object_get_lockword_addr(monitor);
    tm_native_thread = hythread_thin_monitor_get_owner(lockword);
    if (!tm_native_thread) {
        *lock_owner = NULL;
    } else {
        tm_java_thread = hythread_get_private_data(tm_native_thread);
        *lock_owner = tm_java_thread->thread_object;
    }
    hythread_suspend_enable();

    return TM_ERROR_NONE;
}

/**
 * Returns the number of times given thread have entered given monitor;
 *
 * If the given monitor is not owned by this thread, 0 is returned.
 *
 * @param[in] monitor monitor those owner needs to be determined
 * @param[in] owner thread which owns the monitor
 */
IDATA VMCALL jthread_get_lock_recursion(jobject monitor, jthread owner) {

    hythread_t given_thread, lock_owner = NULL;
    jvmti_thread_t tm_java_thread = NULL;
    hythread_thin_monitor_t *lockword;
    IDATA recursion = 0;

    assert(monitor);
    given_thread = owner?vm_jthread_get_tm_data(owner):NULL;
    hythread_suspend_disable();
    
    lockword = vm_object_get_lockword_addr(monitor);
    lock_owner = hythread_thin_monitor_get_owner(lockword);
        
    if (lock_owner && (!given_thread || lock_owner->thread_id == given_thread->thread_id))
        recursion = hythread_thin_monitor_get_recursion(lockword);
    
    hythread_suspend_enable();

    return recursion;
}

/**
 * Returns all monitors owned by the specific thread.
 *
 * @param[in] java_thread thread which owns monitors
 * @param[out] monitor_count_ptr number of owned monitors 
 * @param[out] monitors_ptr array of owned monitors
 */
IDATA VMCALL jthread_get_owned_monitors(jthread java_thread, 
                       jint* monitor_count_ptr, jobject** monitors_ptr) {

    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    // apr_pool_t* pool;
    // apr_status_t apr_status;
    jobject* monitors;
    int i;
    IDATA status;

    status =hythread_global_lock();
    if (status != TM_ERROR_NONE) return status;
    assert(java_thread);
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    assert(tm_java_thread);
    /* apr_status = apr_pool_create(&pool, 0);
    if (apr_status != APR_SUCCESS) {
        hythread_global_unlock();
        return TM_ERROR_INTERNAL;
    }
    monitors = apr_palloc(pool, sizeof(jobject*) * tm_java_thread->owned_monitors_nmb); */
    monitors = (jobject *)malloc(sizeof(jobject*) * tm_java_thread->owned_monitors_nmb);
    if (!monitors) {
        hythread_global_unlock();
        return TM_ERROR_OUT_OF_MEMORY;
    }
    for (i = 0; i < tm_java_thread->owned_monitors_nmb; i++) {
        monitors[i] = tm_java_thread->owned_monitors[i];
    }
    *monitors_ptr = monitors;
    *monitor_count_ptr = tm_java_thread->owned_monitors_nmb;

    status = hythread_global_unlock();
    return status;
}
