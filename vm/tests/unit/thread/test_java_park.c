/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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

/*
 * Test jthread_park(...)
 * Test jthread_unpark(...)
 */
void run_for_test_jthread_park_unpark(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    IDATA status;
    
    tts->phase = TT_PHASE_PARKED;
    status = jthread_park();
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_RUNNING : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    tts->phase = TT_PHASE_DEAD;
}

int test_jthread_park_unpark(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *parked_tts;
    int i;
    int parked_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_park_unpark);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_PARKED);        
    }
    for (i = 0; i <= MAX_TESTED_THREAD_NUMBER; i++){

        parked_nmb = 0;
        parked_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_PARKED){
                parked_nmb++;
                parked_tts = tts;
            } else {
                tf_assert_same(tts->phase, TT_PHASE_RUNNING);
            }
        }
        if (MAX_TESTED_THREAD_NUMBER - parked_nmb - i != 0){
            tf_fail("Wrong number of parked threads");
        }
        if (parked_nmb > 0){
            tf_assert_same(jthread_unpark(parked_tts->java_thread), TM_ERROR_NONE);
            check_tested_thread_phase(parked_tts, TT_PHASE_RUNNING);
        }
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_park(...)
 */
void run_for_test_jthread_park_interrupt(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    IDATA status;
    
    tts->phase = TT_PHASE_PARKED;
    status = jthread_park();
    tts->phase = (status == TM_ERROR_INTERRUPT ? TT_PHASE_RUNNING : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    tts->phase = TT_PHASE_DEAD;
}

int test_jthread_park_interrupt(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *parked_tts;
    int i;
    int parked_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_park_interrupt);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_PARKED);
    }
    for (i = 0; i <= MAX_TESTED_THREAD_NUMBER; i++){

        parked_nmb = 0;
        parked_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_PARKED){
                parked_nmb++;
                parked_tts = tts;
            } else {
                tf_assert_same(tts->phase, TT_PHASE_RUNNING);
            }
        }
        if (MAX_TESTED_THREAD_NUMBER - parked_nmb - i != 0){
            tf_fail("Wrong number of parked threads");
        }
        if (parked_nmb > 0){
            tf_assert_same(jthread_interrupt(parked_tts->java_thread), TM_ERROR_NONE);
            check_tested_thread_phase(parked_tts, TT_PHASE_RUNNING);
        }
    }
    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_timed_park(...)
 */
void run_for_test_jthread_timed_park(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    IDATA status;
    
    tts->phase = TT_PHASE_PARKED;
    status = jthread_timed_park(50, 0);
    tts->phase = (status == TM_ERROR_TIMEOUT ? TT_PHASE_RUNNING : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    tts->phase = TT_PHASE_DEAD;
}

int test_jthread_timed_park(void) {

    tested_thread_sturct_t *tts;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_timed_park);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);       
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_jthread_park_unpark)
    TEST(test_jthread_park_interrupt)
    TEST(test_jthread_timed_park)
TEST_LIST_END;
