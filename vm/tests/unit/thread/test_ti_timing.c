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

int helper_hythread_cpu_timing(void);

/*
 * CPU timing
 */

int test_jthread_get_thread_cpu_time(void) {

    log_info("NO IMPLEMENTTATION TO TEST");
    return TEST_FAILED;
    return helper_hythread_cpu_timing();
}

int test_jthread_get_thread_user_cpu_time(void) {

    log_info("NO IMPLEMENTTATION TO TEST");
    return TEST_FAILED;
    return helper_hythread_cpu_timing();
}

int test_jthread_get_thread_blocked_time(void) {

    log_info("NO IMPLEMENTTATION TO TEST");
    return TEST_FAILED;
    return helper_hythread_cpu_timing();
}

int test_jthread_get_thread_waited_time(void) {

    log_info("NO IMPLEMENTTATION TO TEST");
    return TEST_FAILED;
    return helper_hythread_cpu_timing();
}

int test_jthread_get_thread_cpu_timer_info(void) {

    log_info("NO IMPLEMENTTATION TO TEST");
    return TEST_FAILED;
    return helper_hythread_cpu_timing();
}

void run_for_helper_hythread_cpu_timing(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    int i;
    int num = 0;
    
    tts->phase = TT_PHASE_RUNNING;
    while(1){
        for (i = 0; i < 1000; i++){
            num = num + 1;
        }
        if (tts->stop) {
            break;
        }
        sleep_a_click();
    }
    tts->phase = TT_PHASE_DEAD;
}

int helper_hythread_cpu_timing(void) {

    tested_thread_sturct_t *tts;
    jlong cpu_time;
    jlong user_cpu_time;
    jlong blocked_time;
    jlong waited_time;
    jvmtiTimerInfo timer_info;

    log_info("NO IMPLEMENTATION TO TEST");
    return TEST_FAILED;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_hythread_cpu_timing);
    
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tts->stop = 1;
        check_tested_thread_phase(tts, TT_PHASE_DEAD);

        tf_assert_same(jthread_get_thread_cpu_time(tts->java_thread, &cpu_time), TM_ERROR_NONE);
        tf_assert_same(jthread_get_thread_user_cpu_time(tts->java_thread, &user_cpu_time), TM_ERROR_NONE);
        tf_assert_same(jthread_get_thread_blocked_time(tts->java_thread, &blocked_time), TM_ERROR_NONE);
        tf_assert_same(jthread_get_thread_waited_time(tts->java_thread, &waited_time), TM_ERROR_NONE);
        tf_assert_same(jthread_get_thread_cpu_timer_info(&timer_info), TM_ERROR_NONE);
        
        tf_assert(user_cpu_time > 0);
        printf("=================================================== %08x\n", cpu_time);
        printf("cpu_time = %i \n", cpu_time);
        printf("user_cpu_time = %i \n", user_cpu_time);
        printf("blocked_time = %i \n", blocked_time);
        printf("waited_time = %i \n", waited_time);
        printf("cpu_time = %i \n", cpu_time);

        printf("jvmtiTimerInfo :\n");
        printf("max_value = %i \n", timer_info.max_value);
        printf("may_skip_forward = %i \n", timer_info.may_skip_forward);
        printf("may_skip_backward = %i \n", timer_info.may_skip_backward);
        printf("kind = %i \n", timer_info.kind);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    //TEST(test_jthread_get_thread_cpu_time)
    //TEST(test_jthread_get_thread_user_cpu_time)
    //TEST(test_jthread_get_thread_waited_time)
    //TEST(test_jthread_get_thread_blocked_time)
    //TEST(test_jthread_get_thread_cpu_timer_info)
TEST_LIST_END;
