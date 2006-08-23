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
#include <open/hythread_ext.h>

int helper_jthread_monitor_enter_exit(void);
int helper_jthread_monitor_wait_notify(void);
 
int test_jthread_monitor_init (void){

    return helper_jthread_monitor_enter_exit();
} 

int test_jthread_monitor_enter (void){

    return helper_jthread_monitor_enter_exit();
} 

/*
 * Test jthread_monitor_try_enter()
 */
void run_for_test_jthread_monitor_try_enter(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_try_enter(monitor);
    while (status == TM_ERROR_EBUSY){
        status = jthread_monitor_try_enter(monitor);
        sleep_a_click();
    }
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

int test_jthread_monitor_try_enter(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    int waiting_on_monitor_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_try_enter);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_monitor_nmb = 0;
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
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
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

int test_jthread_monitor_exit (void){

    return helper_jthread_monitor_enter_exit();
} 

int test_jthread_monitor_notify (void){

    return helper_jthread_monitor_wait_notify();
} 

/*
 * Test jthread_monitor_notify_all(...)
 */
void run_for_test_jthread_monitor_notify_all(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    // Begin critical section
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_wait(monitor);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_monitor_notify_all(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    jobject monitor;
    int i;
    int waiting_on_wait_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_notify_all);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->monitor; // the same for all tts
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    tf_assert_same(jthread_monitor_enter(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_notify_all(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_exit(monitor), TM_ERROR_NONE);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_wait_nmb = 0;
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
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
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_monitor_wait()
 */
void run_for_test_jthread_monitor_wait(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_wait(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_monitor_wait (void){

    tested_thread_sturct_t *tts;
    jobject monitor;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_wait);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->monitor; // the same for all tts
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    tf_assert_same(jthread_monitor_enter(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_notify_all(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_exit(monitor), TM_ERROR_NONE);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
} 

/*
 * Test jthread_monitor_wait_interrupt()
 */
void run_for_test_jthread_monitor_wait_interrupt(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_wait(monitor);
    if (status != TM_ERROR_INTERRUPT){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_monitor_wait_interrupt(void){

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *waiting_tts;
    int i;
    int waiting_on_wait_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_wait_interrupt);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER + 1; i++){

        waiting_on_wait_nmb = 0;
        waiting_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_WAITING_ON_WAIT){
                waiting_tts = tts;
                waiting_on_wait_nmb++;
            } else {
                check_tested_thread_phase(tts, TT_PHASE_DEAD);
            }
        }
        tf_assert_same(MAX_TESTED_THREAD_NUMBER - waiting_on_wait_nmb - i, 0);
        if (waiting_tts){
            tf_assert_same(jthread_interrupt(waiting_tts->java_thread), TM_ERROR_NONE);
            check_tested_thread_phase(waiting_tts, TT_PHASE_DEAD);
        }
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
} 

/*
 * Test jthread_monitor_timed_wait()
 */
void run_for_test_jthread_monitor_timed_wait(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_timed_wait(monitor, 10 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER,0);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_monitor_timed_wait(void) {

    tested_thread_sturct_t *tts;
    jobject monitor;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_timed_wait);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->monitor; // the same for all tts
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    tf_assert_same(jthread_monitor_enter(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_notify_all(monitor), TM_ERROR_NONE);
    tf_assert_same(jthread_monitor_exit(monitor), TM_ERROR_NONE);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
} 

/*
 * Test jthread_monitor_timed_wait()
 */
void run_for_test_jthread_monitor_timed_wait_timeout(void){


    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_timed_wait(monitor, 10 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);
    if (status != TM_ERROR_TIMEOUT){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_monitor_timed_wait_timeout(void) {

    tested_thread_sturct_t *tts;
    jobject monitor;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_timed_wait_timeout);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->monitor; // the same for all tts
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }

    // Wait for all threads wait timeout
    jthread_sleep(20 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
} 

/*
 * Test jthread_monitor_timed_wait()
 */
void run_for_test_jthread_monitor_timed_wait_interrupt(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_timed_wait(monitor, 100 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);
    if (status != TM_ERROR_INTERRUPT){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_monitor_timed_wait_interrupt(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *waiting_tts;
    int i;
    int waiting_on_wait_nmb;

    log_info("!!!!!!!!!!!!!! CRASHES");
    tf_assert(0);
    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_monitor_timed_wait_interrupt);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER + 1; i++){

        waiting_on_wait_nmb = 0;
        waiting_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_WAITING_ON_WAIT){
                waiting_tts = tts;
                waiting_on_wait_nmb++;
            } else {
                check_tested_thread_phase(waiting_tts, TT_PHASE_DEAD);
            }
        }
        tf_assert_same(MAX_TESTED_THREAD_NUMBER - waiting_on_wait_nmb - i, 0);
        if (waiting_tts){
            tf_assert_same(jthread_interrupt(waiting_tts->java_thread), TM_ERROR_NONE);
        }
        check_tested_thread_phase(waiting_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
} 

/*
 * ------------------------ HELPERS -----------------------
 */

/*
 * Test jthread_monitor_enter(...)
 * Test jthread_monitor_exit(...)
 */
//?????????????????????????????? jthread_monitor_init and not init
//?????????????????????????????? jthread_monitor_exit without enter

void run_for_helper_jthread_monitor_enter_exit(void){

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

int helper_jthread_monitor_enter_exit(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    int waiting_on_monitor_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_jthread_monitor_enter_exit);

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        waiting_on_monitor_nmb = 0;
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
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
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_monitor_wait(...)
 * Test jthread_monitor_notify(...)
 */
void run_for_helper_jthread_monitor_wait_notify(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;

    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    if (status != TM_ERROR_NONE){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    // Begin critical section
    tts->phase = TT_PHASE_WAITING_ON_WAIT;
    status = jthread_monitor_wait(monitor);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    status = jthread_monitor_exit(monitor);
    // Exit critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int helper_jthread_monitor_wait_notify(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    jobject monitor;
    int i;
    int waiting_on_wait_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_helper_jthread_monitor_wait_notify);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        monitor = tts->monitor; // the same for all tts
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_WAIT);       
    }

    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){

        waiting_on_wait_nmb = 0;
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            tf_assert(tts->phase != TT_PHASE_IN_CRITICAL_SECTON);
        }
        tf_assert_same(jthread_monitor_notify(monitor), TM_ERROR_NONE);
        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
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
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_jthread_monitor_init)
    TEST(test_jthread_monitor_enter)
    TEST(test_jthread_monitor_try_enter)
    TEST(test_jthread_monitor_exit)
    //TEST(test_jthread_monitor_notify)
    TEST(test_jthread_monitor_notify_all)
    TEST(test_jthread_monitor_wait)
    //TEST(test_jthread_monitor_wait_interrupt)
    TEST(test_jthread_monitor_timed_wait)
    TEST(test_jthread_monitor_timed_wait_timeout)
    //TEST(test_jthread_monitor_timed_wait_interrupt)
TEST_LIST_END;
