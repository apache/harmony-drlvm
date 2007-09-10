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
#include <open/hythread_ext.h>

#define NMB 5

hythread_monitor_t monitor;
hymutex_t *mutex;
hycond_t  *condvar;
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

    status = hythread_monitor_init(&monitor, 0);
    tf_assert_same(status, TM_ERROR_NONE);
    waiting_count = 0;

    for (i = 0; i < NMB; i++) {
        threads[i] = NULL;
        status = hythread_create(&threads[i], 0, 0, 0,
            (hythread_entrypoint_t)run_for_test_wait_signal, NULL);
        tf_assert_same(status, TM_ERROR_NONE);
    }

    // Wait till all tested threads call wait() 
    while (1){
        status = hythread_monitor_enter(monitor);
        tf_assert_same(status, TM_ERROR_NONE);

        if (waiting_count == NMB) break;

        status = hythread_monitor_exit(monitor);
        tf_assert_same(status, TM_ERROR_NONE);

        hythread_sleep(100);
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
        hythread_sleep(100);
        hythread_join(threads[i]);
    }
    return 0;
}

TEST_LIST_START
    TEST(test_wait_signal)
TEST_LIST_END;
