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

/*
 * Test jthread_suspend(...)
 * Test jthread_resume(...)
  */
void run_for_test_jthread_suspend_resume(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject mon = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_RUNNING;
    while(1){
        hythread_safe_point();
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    tts->phase = TT_PHASE_DEAD;
}

int test_jthread_suspend_resume(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *switch_tts;
    int i;
    int suspended_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_suspend_resume);

    for (i = 0; i <= MAX_TESTED_THREAD_NUMBER; i++){

        suspended_nmb = 0;
        switch_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_RUNNING);
            if (tested_thread_is_running(tts)){
                switch_tts = tts;
            } else {
                suspended_nmb++;
            }
        }
        if (suspended_nmb != i){
            tf_fail("Wrong number of suspended threads");
        }
        if (switch_tts != NULL){
            tf_assert_same(jthread_suspend(switch_tts->java_thread), TM_ERROR_NONE);
        }
    }
    for (i = 0; i <= MAX_TESTED_THREAD_NUMBER; i++){

        suspended_nmb = 0;
        switch_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_RUNNING);
            if (!tested_thread_is_running(tts)){
                suspended_nmb++;
                switch_tts = tts;
            }
        }
        if (suspended_nmb != MAX_TESTED_THREAD_NUMBER - i){
            tf_fail("Wrong number of suspended threads");
        }
        if (switch_tts != NULL){
            tf_assert_same(jthread_resume(switch_tts->java_thread), TM_ERROR_NONE);
        }
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}
/*
 * Test jthread_suspend_all(...)
 * Test jthread_resume_all(...)
 */

int test_jthread_suspend_all_resume_all(void) {

    tested_thread_sturct_t * tts;
    jthread all_threads[MAX_TESTED_THREAD_NUMBER];
    jvmtiError results[MAX_TESTED_THREAD_NUMBER];
    int i = 0;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_suspend_resume);

    // Test that all threads are running
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        all_threads[i] = tts->java_thread;
        results[i] = (jvmtiError)(TM_ERROR_NONE + 1);
        i++;
        tf_assert(tested_thread_is_running(tts));
    }
    tf_assert_same(jthread_suspend_all(results, MAX_TESTED_THREAD_NUMBER, all_threads), TM_ERROR_NONE);
    // Test that all threads are suspended
    i = 0;
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert(!tested_thread_is_running(tts));
        tf_assert_same(results[i], TM_ERROR_NONE);
        results[i] = (jvmtiError)(TM_ERROR_NONE + 1);
        i++;
    }
    tf_assert_same(jthread_resume_all(results, MAX_TESTED_THREAD_NUMBER, all_threads), TM_ERROR_NONE);
    // Test that all threads are running
    i = 0;
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert(tested_thread_is_running(tts));
        tf_assert_same(results[i], TM_ERROR_NONE);
        i++;
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();
    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_jthread_suspend_resume)
    TEST(test_jthread_suspend_all_resume_all)
TEST_LIST_END;
