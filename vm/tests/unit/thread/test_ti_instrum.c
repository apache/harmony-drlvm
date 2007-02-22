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
#include <open/thread_externals.h>

hysem_t mon_enter;

/*
 * Test jthread_get_all_threads(...)
 */
int test_jthread_get_all_threads(void) {

    tested_thread_sturct_t *tts;
    jint all_threads_count = 99;
    jint thread_count = 99;
    jint initial_thread_count;
    jint initial_all_threads_count;
    jthread *threads = NULL;
    JNIEnv * jni_env;
    int i;

    jni_env = jthread_get_JNI_env(jthread_self());

    // TODO: unsafe .... need to find another way of synchronization
    hythread_sleep(1000);
    
    jthread_get_thread_count(&initial_thread_count);
    jthread_get_all_threads(&threads, &initial_all_threads_count);

    // Initialize tts structures
    tested_threads_init(TTS_INIT_COMMON_MONITOR);

    tf_assert_same(jthread_get_thread_count(&thread_count), TM_ERROR_NONE);
    tf_assert_same(jthread_get_all_threads(&threads, &all_threads_count), TM_ERROR_NONE);
    tf_assert_same(thread_count, initial_thread_count);
    tf_assert_same(all_threads_count, initial_all_threads_count);
    tf_assert_not_null(threads);
    i = 0;
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(jthread_create_with_function(jni_env, tts->java_thread, &tts->attrs, default_run_for_test, tts), TM_ERROR_NONE);
        tested_thread_wait_started(tts);
        tts->native_thread = (hythread_t) vm_jthread_get_tm_data(tts->java_thread);
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);
        tf_assert_same(jthread_get_thread_count(&thread_count), TM_ERROR_NONE);
        tf_assert_same(jthread_get_all_threads(&threads, &all_threads_count), TM_ERROR_NONE);
        i++;
        tf_assert_same(thread_count, i + initial_thread_count);
        tf_assert_same(all_threads_count, i + initial_all_threads_count);
        compare_threads(threads, i, 0);
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_thread_count(...)
 */
int test_jthread_get_thread_count(void) {

    return test_jthread_get_all_threads();
}

/*
 * Test get_blocked_count(...)
 */
void JNICALL run_for_test_jthread_get_blocked_count(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    tested_thread_started(tts);
    status = jthread_monitor_enter(monitor);

    // Begin critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    hysem_set(mon_enter, 1);
    tested_thread_wait_for_stop_request(tts);
    status = jthread_monitor_exit(monitor);
    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_get_blocked_count(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    int waiting_on_monitor_nmb;

    hysem_create(&mon_enter, 0, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_blocked_count);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        int cycles = MAX_TIME_TO_WAIT / CLICK_TIME_MSEC;

        waiting_on_monitor_nmb = 0;
        critical_tts = NULL;

        hysem_wait(mon_enter);

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(critical_tts == NULL);
                critical_tts = tts;
            }
        }
        while ((MAX_TESTED_THREAD_NUMBER - i > waiting_on_monitor_nmb + 1) && (cycles-- > 0)) {
            tf_assert_same(jthread_get_blocked_count(&waiting_on_monitor_nmb), TM_ERROR_NONE);
            sleep_a_click();
        }
        if (cycles < 0){
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
 * Test jthread_get_deadlocked_threads(...)
 */
void JNICALL run_for_test_jthread_get_deadlocked_threads(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    IDATA status = TM_ERROR_NONE;
    
    if (tts->my_index < 2){
        status = jthread_monitor_enter(tts->monitor);
    }

    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);
    tested_thread_wait_for_stop_request(tts);
    if (tts->my_index == 0){
        status = jthread_monitor_enter(get_tts(1)->monitor);
    } else if (tts->my_index == 1){
        status = jthread_monitor_enter(get_tts(0)->monitor);
    }
    tts->phase = status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR;
    tested_thread_ended(tts);
}

int test_jthread_get_deadlocked_threads(void) {

    tested_thread_sturct_t * tts;
    jthread *thread_list;
    int dead_list_count;
    jthread *dead_list;
    int i = 0;
    apr_status_t apr_status;
    apr_pool_t *pool = NULL;

    apr_status = apr_pool_create(&pool, NULL);
    thread_list = apr_palloc(pool, sizeof(int) * MAX_TESTED_THREAD_NUMBER);

    // Initialize tts structures and run all tested threads
    tested_threads_run_with_different_monitors(run_for_test_jthread_get_deadlocked_threads);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        thread_list[i] = tts->java_thread;
        i++;
    }
    tf_assert_same(jthread_get_deadlocked_threads(thread_list, MAX_TESTED_THREAD_NUMBER,
                                                  &dead_list, &dead_list_count), TM_ERROR_NONE);
    tf_assert_same(dead_list_count, 0);

    tested_thread_send_stop_request(get_tts(0));
    tested_thread_send_stop_request(get_tts(1));

    // TODO: unsafe .... need to find another way of synchronization
    hythread_sleep(5000);
    tf_assert_same(jthread_get_deadlocked_threads(thread_list, MAX_TESTED_THREAD_NUMBER,
                                                  &dead_list, &dead_list_count), TM_ERROR_NONE);
    tf_assert_same(dead_list_count, 2);

    tf_assert_same(jthread_monitor_exit(get_tts(0)->java_thread), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_exit(get_tts(1)->java_thread), TM_ERROR_NONE);

     // Terminate all threads and clear tts structures
    tested_threads_destroy();
    
    return TEST_PASSED;
}

/*
 * Test get_wated_count(...)
 */
void JNICALL run_for_test_jthread_get_waited_count(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);
    status = jthread_monitor_enter(monitor);
    tested_thread_wait_for_stop_request(tts);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_WAITING_ON_WAIT : TT_PHASE_ERROR);
    status = jthread_monitor_wait(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    status = jthread_monitor_exit(monitor);

    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_get_waited_count(void) {

    int i;
    int waiting_nmb;
    jobject monitor;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_waited_count);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_nmb = 0;

        tf_assert_same(jthread_get_waited_count(&waiting_nmb), TM_ERROR_NONE);
        if (waiting_nmb != i){
            tf_fail("Wrong number waiting on monitor threads");
        }
        tested_thread_send_stop_request(get_tts(i));
        // TODO: unsafe .... need to find another way of synchronization
        hythread_sleep(1000);
        check_tested_thread_phase(get_tts(i), TT_PHASE_WAITING_ON_WAIT);
    }
    monitor = get_tts(0)->monitor;
    tf_assert_same(jthread_monitor_enter(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_notify_all(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_exit(monitor), TM_ERROR_NONE);

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_jthread_get_all_threads)
    TEST(test_jthread_get_thread_count)
    TEST(test_jthread_get_blocked_count)
    //TEST(test_jthread_get_deadlocked_threads)
    //TEST(test_jthread_get_waited_count)
TEST_LIST_END;
