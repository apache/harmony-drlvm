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

hysem_t start;

void JNICALL run_for_test_jthread_get_all_threads(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg) {

    tested_thread_sturct_t * tts = current_thread_tts;
    
    tts->phase = TT_PHASE_RUNNING;
    hysem_set(start, 1);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    tts->phase = TT_PHASE_DEAD;
}

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
    hysem_create(&start, 0, 1);

    sleep_a_click();
    
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
        current_thread_tts = tts;
        tf_assert_same(jthread_create_with_function(jni_env, tts->java_thread, &tts->attrs, run_for_test_jthread_get_all_threads, NULL), TM_ERROR_NONE);
        hysem_wait(start);
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
void JNICALL run_for_test_jthread_get_blocked_count(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);

    // Begin critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    status = jthread_monitor_exit(monitor);
    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_get_blocked_count(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    int waiting_on_monitor_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_blocked_count);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_monitor_nmb = 0;
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                critical_tts = tts;
            }
        }
        tf_assert_same(jthread_get_blocked_count(&waiting_on_monitor_nmb), TM_ERROR_NONE);
        if (MAX_TESTED_THREAD_NUMBER - waiting_on_monitor_nmb - i != 1){
            tf_fail("Wrong number waiting on monitor threads");
        }
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_deadlocked_threads(...)
 */
void JNICALL run_for_test_jthread_get_deadlocked_threads(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

    tested_thread_sturct_t * tts = current_thread_tts;
    IDATA status;
    
    if (tts->my_index < 2){
        status = jthread_monitor_enter(tts->monitor);
    }

    tts->phase = TT_PHASE_RUNNING;
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    if (tts->my_index == 0){
        status = jthread_monitor_enter(get_tts(1)->monitor);
    } else if (tts->my_index == 1){
        status = jthread_monitor_enter(get_tts(0)->monitor);
    }
    tts->phase = TT_PHASE_DEAD;
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

    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    tts->stop = 1;
    check_tested_thread_phase(tts, TT_PHASE_ANY);
    next_tested_thread(&tts);
    tts->stop = 1;
    check_tested_thread_phase(tts, TT_PHASE_ANY);
    tf_assert_same(jthread_get_deadlocked_threads(thread_list, MAX_TESTED_THREAD_NUMBER,
                                                  &dead_list, &dead_list_count), TM_ERROR_NONE);
    tf_assert_same(dead_list_count, 2);

    return TEST_PASSED;
}

/*
 * Test get_wated_count(...)
 */
void JNICALL run_for_test_jthread_get_waited_count(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_RUNNING;
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    status = jthread_monitor_enter(monitor);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_WAITING_ON_WAIT : TT_PHASE_ERROR);
    status = jthread_monitor_wait(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);

    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_get_waited_count(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *running_tts;
    int i;
    int waiting_nmb;
    jobject monitor;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_waited_count);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_nmb = 0;
        running_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_RUNNING){
                running_tts = tts;
            }
        }
        tf_assert_same(jthread_get_waited_count(&waiting_nmb), TM_ERROR_NONE);
        if (waiting_nmb != i){
            tf_fail("Wrong number waiting on monitor threads");
        }
        if (running_tts){
            running_tts->stop = 1;
        }
        check_tested_thread_phase(running_tts, TT_PHASE_WAITING_ON_WAIT);
    }
    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    monitor = tts->monitor;
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
