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


#include <stdio.h>
#include "testframe.h"
#include "thread_unit_test_utils.h"
#include <open/jthread.h>
#include <open/ti_thread.h>

int test_jthread_raw_monitor_destroy(void);
int helper_jthread_raw_monitor_enter_exit(void);
int helper_jthread_raw_monitor_try_enter(void);
int helper_jthread_raw_wait_notify(void);
int helper_jthread_raw_wait_notify_all(void);

/*
 * Raw monitors 
 */

int test_jthread_raw_monitor_create(void) {

    return test_jthread_raw_monitor_destroy();
}

int test_jthread_raw_monitor_destroy(void) {

    jrawMonitorID raw_monitor;
    IDATA status; 

    status = jthread_raw_monitor_create(&raw_monitor);
    if (status != TM_ERROR_NONE){
        return TEST_FAILED;
    }
    status = jthread_raw_monitor_destroy(raw_monitor);
    if (status != TM_ERROR_NONE){
        return TEST_FAILED;
    }
    return TEST_PASSED;
}

int test_jthread_raw_monitor_enter(void) {

    return helper_jthread_raw_monitor_enter_exit();
}

int test_jthread_raw_monitor_try_enter(void) {

    return helper_jthread_raw_monitor_try_enter();
}

int test_jthread_raw_monitor_exit(void) {

    return helper_jthread_raw_monitor_enter_exit();
}

int test_jthread_raw_notify(void) {

    return helper_jthread_raw_wait_notify();
}

int test_jthread_raw_notify_all(void) {

    return helper_jthread_raw_wait_notify_all();
}

int test_jthread_raw_wait(void) {

    return helper_jthread_raw_wait_notify();
}

/*
 * ------------------------ HELPERS -----------------------
 */

hysem_t mon_enter;
/*
 * Test jthread_raw_monitor_enter(...)
 * Test jthread_raw_monitor_exit(...)
 */
//?????????????????????????????? jthread_raw_monitor_init and not init
//?????????????????????????????? jthread_raw_monitor_exit without enter
void JNICALL run_for_helper_jthread_raw_monitor_enter_exit(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jrawMonitorID monitor = tts->raw_monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    tested_thread_started(tts);
    status = jthread_raw_monitor_enter(monitor);

    // Begin critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    hysem_set(mon_enter, 1);
    tested_thread_wait_for_stop_request(tts);
    status = jthread_raw_monitor_exit(monitor);
    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int helper_jthread_raw_monitor_enter_exit(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    int waiting_on_monitor_nmb;

    hysem_create(&mon_enter, 0, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_jthread_raw_monitor_enter_exit);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_monitor_nmb = 0;
        critical_tts = NULL;

        hysem_wait(mon_enter);

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(critical_tts == NULL); // error if two threads in critical section
                critical_tts = tts;
            } else if (tts->phase == TT_PHASE_WAITING_ON_MONITOR){
                waiting_on_monitor_nmb++;
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        if (MAX_TESTED_THREAD_NUMBER - waiting_on_monitor_nmb - i != 1){
            tf_fail("Wrong number waiting on monitor threads");
        }
        tested_thread_send_stop_request(critical_tts);
        tested_thread_wait_ended(critical_tts);
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_raw_wait(...)
 * Test jthread_raw_notify(...)
 */
void JNICALL run_for_helper_jthread_raw_wait_notify(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jrawMonitorID monitor = tts->raw_monitor;
    IDATA status;
    int64 msec = 1000000;
    
    status = jthread_raw_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    // Begin critical section
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    tested_thread_started(tts);
    status = jthread_raw_monitor_wait(monitor, msec);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    hysem_set(mon_enter, 1);
    tested_thread_wait_for_stop_request(tts);
    status = jthread_raw_monitor_exit(monitor);
    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int helper_jthread_raw_wait_notify(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    jrawMonitorID monitor;
    int i;
    int waiting_on_wait_nmb;

    hysem_create(&mon_enter, 0, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_jthread_raw_wait_notify);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->raw_monitor; // the same for all tts
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_wait_nmb = 0;
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            tf_assert(tts->phase != TT_PHASE_IN_CRITICAL_SECTON);
        }
        tf_assert_same(jthread_raw_monitor_notify(monitor), TM_ERROR_NONE);
        hysem_wait(mon_enter);
        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(critical_tts == NULL); // error if two threads in critical section
                critical_tts = tts;
            } else if (tts->phase == TT_PHASE_WAITING_ON_WAIT){
                waiting_on_wait_nmb++;
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        if (MAX_TESTED_THREAD_NUMBER - waiting_on_wait_nmb - i != 1){
            tf_fail("Wrong number waiting on monitor threads");
        }
        tested_thread_send_stop_request(critical_tts);
        tested_thread_wait_ended(critical_tts);
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_raw_wait(...)
 * Test jthread_raw_notify_all(...)
 */

int helper_jthread_raw_wait_notify_all(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    jrawMonitorID monitor;
    int i;
    int waiting_on_wait_nmb;

    hysem_create(&mon_enter, 0, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_jthread_raw_wait_notify);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->raw_monitor;
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    tf_assert_same(jthread_raw_monitor_enter(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_raw_monitor_notify_all(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_raw_monitor_enter(monitor), TM_ERROR_NONE);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_wait_nmb = 0;
        critical_tts = NULL;

        hysem_wait(mon_enter);

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(critical_tts == NULL); // error if two threads in critical section
                critical_tts = tts;
            } else if (tts->phase == TT_PHASE_WAITING_ON_WAIT){
                waiting_on_wait_nmb++;
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        if (MAX_TESTED_THREAD_NUMBER - waiting_on_wait_nmb - i != 1){
            tf_fail("Wrong number waiting on monitor threads");
        }
        tested_thread_send_stop_request(critical_tts);
        tested_thread_wait_ended(critical_tts);
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

void JNICALL run_for_helper_jthread_raw_monitor_try_enter(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jrawMonitorID monitor = tts->raw_monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    tested_thread_started(tts);
    status = jthread_raw_monitor_try_enter(monitor);
    while (status == TM_ERROR_EBUSY){
        status = jthread_raw_monitor_try_enter(monitor);
        sleep_a_click();
    }
    // Begin critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    hysem_set(mon_enter, 1);
    tested_thread_wait_for_stop_request(tts);
    status = jthread_raw_monitor_exit(monitor);
    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int helper_jthread_raw_monitor_try_enter(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    int waiting_on_monitor_nmb;

    hysem_create(&mon_enter, 0, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_jthread_raw_monitor_try_enter);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_monitor_nmb = 0;
        critical_tts = NULL;

        hysem_wait(mon_enter);

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(critical_tts == NULL); // error if two threads in critical section
                critical_tts = tts;
            } else if (tts->phase == TT_PHASE_WAITING_ON_MONITOR){
                waiting_on_monitor_nmb++;
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        if (MAX_TESTED_THREAD_NUMBER - waiting_on_monitor_nmb - i != 1){
            tf_fail("Wrong number waiting on monitor threads");
        }
        tested_thread_send_stop_request(critical_tts);
        tested_thread_wait_ended(critical_tts);
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_jthread_raw_monitor_create)
    TEST(test_jthread_raw_monitor_destroy)
    TEST(test_jthread_raw_monitor_enter)
    TEST(test_jthread_raw_monitor_try_enter)
    TEST(test_jthread_raw_monitor_exit)
    //TEST(test_jthread_raw_notify)
    //TEST(test_jthread_raw_notify_all)
    //TEST(test_jthread_raw_wait)
TEST_LIST_END;
