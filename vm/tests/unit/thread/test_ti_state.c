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
#include <open/hythread_ext.h>
#include <open/ti_thread.h>

/*
 * Test jthread_get_jvmti_state(...)
 *
 *  called function          tested state
 *
 *                           Not Alive, New (state == 0)
 *  hythread_create()             ALIVE | RUNNABLE
 *  jthread_interrupt()          ALIVE | RUNNABLE | INTERRUPTED
 *  jthread_clear_interrupted()  ALIVE | RUNNABLE
 *                           DEAD
 */
void JNICALL run_for_test_jthread_get_jvmti_state_1(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;

    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);
    while(tested_thread_wait_for_stop_request_timed(tts, SLEEP_TIME) == TM_ERROR_TIMEOUT){
        hythread_safe_point();
        hythread_yield();
    }
    tts->phase = TT_PHASE_DEAD;
    tested_thread_ended(tts);
}

int test_jthread_get_jvmti_state_1(void) {

    tested_thread_sturct_t *tts;
    int state;
    int ref_state;

    // Initialize tts structures and run all tested threads
    tested_threads_init(TTS_INIT_COMMON_MONITOR);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        tf_assert_same(state, ref_state);
    }

    // Run all tested threads
    tested_threads_run_common(run_for_test_jthread_get_jvmti_state_1);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tested_thread_wait_running(tts);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x5;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);

        tf_assert_same(jthread_interrupt(tts->java_thread), TM_ERROR_NONE);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x200005;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);

        tf_assert_same(jthread_clear_interrupted(tts->java_thread), TM_ERROR_INTERRUPT);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x5;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);

        tested_thread_send_stop_request(tts);
        tested_thread_wait_ended(tts);
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x2;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}
/*
 * Test jthread_get_jvmti_state(...)
 */
void JNICALL run_for_test_jthread_get_jvmti_state_2(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    tested_thread_started(tts);
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    // Begin critical section
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_wait(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_RUNNING : TT_PHASE_ERROR);
    while(tested_thread_wait_for_stop_request_timed(tts, SLEEP_TIME) == TM_ERROR_TIMEOUT){
        hythread_safe_point();
        hythread_yield();
    }
    tts->phase = TT_PHASE_DEAD;
    tested_thread_ended(tts);
}

int test_jthread_get_jvmti_state_2(void) {

    tested_thread_sturct_t *tts;
    jobject monitor;
    IDATA status;
    int state;
    int ref_state;

    // Initialize tts structures and run all tested threads
    tested_threads_init(TTS_INIT_COMMON_MONITOR);

    // Lock monitor
    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    monitor = tts->monitor;
    status = jthread_monitor_enter(monitor);

    // Run all tested threads
    tested_threads_run_common(run_for_test_jthread_get_jvmti_state_2);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_MONITOR);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x401;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);
    }
    // Release monitor
    status = jthread_monitor_exit(monitor);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x191;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);
    }

    // Lock monitor
    status = jthread_monitor_enter(monitor);
    status = jthread_monitor_notify_all(monitor);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x401;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);
    }

    // Release monitor
    status = jthread_monitor_exit(monitor);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x5;
        if(tts->my_index == 0){
            printf("state = %08x (%08x)\n", state, ref_state);
        }
        //tf_assert_same(state, ref_state);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_jvmti_state(...)
 */
void JNICALL run_for_test_jthread_get_jvmti_state_3(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    tested_thread_started(tts);
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    // Begin critical section
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_timed_wait(monitor, 1000000, 100);
    if (status != TM_ERROR_INTERRUPT){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_get_jvmti_state_3(void) {

    tested_thread_sturct_t *tts;
    int state;
    int ref_state;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_jvmti_state_3);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x1a1;
        printf("state = %08x (%08x)\n", state, ref_state);
        //tf_assert_same(state, ref_state);
        tf_assert_same(jthread_interrupt(tts->java_thread), TM_ERROR_NONE);
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x2;
        printf("state = %08x (%08x)\n", state, ref_state);
        //tf_assert_same(state, ref_state);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_jvmti_state(...)
 */
void JNICALL run_for_test_jthread_get_jvmti_state_4(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    tested_thread_started(tts);
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    // Begin critical section
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_timed_wait(monitor, CLICK_TIME_MSEC, 0);
    if (status != TM_ERROR_TIMEOUT){
        tts->phase = TT_PHASE_ERROR;
        tested_thread_ended(tts);
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_get_jvmti_state_4(void) {

    tested_thread_sturct_t *tts;
    int state;
    int ref_state;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_jvmti_state_4);

    // Wait for all threads wait timeout
    jthread_sleep(20 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x2;
        printf("state = %08x (%08x)\n", state, ref_state);
        //tf_assert_same(state, ref_state);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_jvmti_state(...)
 */
void JNICALL run_for_test_jthread_get_jvmti_state_5(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    IDATA status;

    tts->phase = TT_PHASE_SLEEPING;
    tested_thread_started(tts);
    status = hythread_sleep(1000000);
    tts->phase = (status == TM_ERROR_INTERRUPT ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_get_jvmti_state_5(void) {

    tested_thread_sturct_t *tts;
    int state;
    int ref_state;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_jvmti_state_5);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_SLEEPING);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0;
        printf("state = %08x (%08x)\n", state, ref_state);
        //tf_assert_same(state, ref_state);
        tf_assert_same(jthread_interrupt(tts->java_thread), TM_ERROR_NONE);
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0;
        printf("state = %08x (%08x)\n", state, ref_state);
        //tf_assert_same(state, ref_state);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_jvmti_state(...)
 */
void JNICALL run_for_test_jthread_get_jvmti_state_6(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    IDATA status;

    tts->phase = TT_PHASE_SLEEPING;
    tested_thread_started(tts);
    status = hythread_sleep(CLICK_TIME_MSEC);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_get_jvmti_state_6(void) {

    tested_thread_sturct_t *tts;
    int state;
    int ref_state;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_jvmti_state_6);

    // Wait for all threads wait timeout
    jthread_sleep(20 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
        tf_assert_same(jthread_get_jvmti_state(tts->java_thread, &state), TM_ERROR_NONE);
        ref_state = 0x2;
        printf("state = %08x (%08x)\n", state, ref_state);
        //tf_assert_same(state, ref_state);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    //TEST(test_jthread_get_jvmti_state_1)
    //TEST(test_jthread_get_jvmti_state_2)
    //TEST(test_jthread_get_jvmti_state_3)
    //TEST(test_jthread_get_jvmti_state_4)
    //TEST(test_jthread_get_jvmti_state_5)
    TEST(test_jthread_get_jvmti_state_6)
TEST_LIST_END;
