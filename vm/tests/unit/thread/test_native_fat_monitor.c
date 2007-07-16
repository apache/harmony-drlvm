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

#include "thread_manager.h"
#include "testframe.h"
#include "open/jthread.h"
#include <open/hythread_ext.h>

#define NMB 5

hythread_monitor_t monitor;
apr_thread_mutex_t *mutex;
apr_thread_cond_t  *condvar;
int waiting_count;

int run_for_test_wait_signal(void *args) {

    IDATA status;

    status = hythread_monitor_enter(monitor);
    tf_assert_same(status, TM_ERROR_NONE);
    
    waiting_count++;

    status = hythread_monitor_wait(monitor);
    tf_assert_same(status, TM_ERROR_NONE);

    waiting_count--;

    status = hythread_monitor_exit(monitor);
    tf_assert_same(status, TM_ERROR_NONE);

    return TEST_PASSED;
}

int test_wait_signal(void){

    IDATA status;
    hythread_t threads[NMB];
    int i;

    //log_error("BUG IN APR");
    //tf_assert(0);
    status = hythread_monitor_init(&monitor, 0);
    tf_assert_same(status, TM_ERROR_NONE);
    waiting_count = 0;

    for (i = 0; i < NMB; i++){
        threads[i] = NULL;
        hythread_create(&threads[i], 0, 0, 0, run_for_test_wait_signal, NULL);
    }

    // Wait till all tested threads call wait() 
    while (1){
        status = hythread_monitor_enter(monitor);
        tf_assert_same(status, TM_ERROR_NONE);

        if (waiting_count == NMB) break;

        status = hythread_monitor_exit(monitor);
        tf_assert_same(status, TM_ERROR_NONE);

        jthread_sleep(100, 0);
    }
    status = hythread_monitor_exit(monitor);


    // Send one signal per tested thread
    for (i = 0; i < NMB; i++){
        jthread_sleep(100, 0);

        status = hythread_monitor_enter(monitor);
        tf_assert_same(status, TM_ERROR_NONE);
            
        //hythread_monitor_notify_all(monitor);
        hythread_monitor_notify(monitor);

        status = hythread_monitor_exit(monitor);
        tf_assert_same(status, TM_ERROR_NONE);
    }
    for (i = 0; i < NMB; i++){
        jthread_sleep(100, 0);
        hythread_join(threads[i]);
    }
    return 0;
}

void * APR_THREAD_FUNC run_for_test_apr_wait_signal(apr_thread_t *thread, void *args) {

    apr_status_t status;

    status = apr_thread_mutex_lock(mutex);
    waiting_count ++;

    status = apr_thread_cond_wait(condvar, mutex);

    waiting_count --;
    status = apr_thread_mutex_unlock(mutex);

    printf("------ waiting_count = %i\n", waiting_count);

    return NULL;
}

int test_apr_wait_signal(void){

    apr_thread_t *threads[NMB];
    apr_threadattr_t *apr_attrs = NULL;
    apr_status_t status;
    apr_pool_t *pool;
    int i;
    int all_are_waiting;

    log_error("BUG IN APR");
    tf_assert(0);
    waiting_count = 0;
    status = apr_pool_create(&pool, NULL);
    status = apr_thread_mutex_create(&mutex, TM_MUTEX_NESTED, pool);
    status = apr_thread_cond_create(&condvar, pool);

    for (i = 0; i < NMB; i++){
        threads[i] = NULL;
        // Create APR thread
        status = apr_thread_create(
                                   &threads[i],// new thread OS handle 
                                   apr_attrs,
                                   run_for_test_apr_wait_signal, // start proc
                                   NULL,                         //start proc arg 
                                   pool
                                   ); 
    }
    all_are_waiting = 0;
    while (!all_are_waiting){
        status = apr_thread_mutex_lock(mutex);
        if (waiting_count == NMB) all_are_waiting = 1;
        status = apr_thread_mutex_unlock(mutex);
        jthread_sleep(100, 0);
    }
    for (i = 0; i < NMB; i++){
        jthread_sleep(100, 0);
        status = apr_thread_mutex_lock(mutex);
        apr_thread_cond_signal(condvar);
        status = apr_thread_mutex_unlock(mutex);
    }

    //status = apr_thread_mutex_lock(mutex);
    //apr_thread_cond_broadcast(condvar);
    //status = apr_thread_mutex_unlock(mutex);

    for (i = 0; i < NMB; i++){
        jthread_sleep(100, 0);
        apr_thread_join(&status, threads[i]);
    }
    return 0;
}

TEST_LIST_START
    TEST(test_wait_signal)
    //TEST(test_apr_wait_signal)
TEST_LIST_END;
