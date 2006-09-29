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
#include "thread_private.h"

/*
 * Time for jthread_timed_join() to wait
 * set in test_jthread_timed_join() and used in run_for_test_jthread_timed_join()
 */
int timed_join_wait_time;

/*
 * Test jthread_attach()
 */
void * APR_THREAD_FUNC run_for_test_jthread_attach(apr_thread_t *thread, void *args){

    tested_thread_sturct_t * tts = current_thread_tts;
    //JNIEnv * new_env = new_JNIEnv();
    IDATA status;
    
    tts->jni_env = new_JNIEnv();
    status = jthread_attach(tts->jni_env, tts->java_thread);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_ATTACHED : TT_PHASE_ERROR);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }

    tts->phase = TT_PHASE_DEAD;
    return 0;
}

int test_jthread_attach(void) {

    tested_thread_sturct_t * tts;

    // Initialize tts structures and run all tested threads
    tested_os_threads_run(run_for_test_jthread_attach);
    
    // Make second attach to the same jthread.
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_ATTACHED);
        check_tested_thread_structures(tts);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();
    return TEST_PASSED;
}

/*
 * Test jthread_detach()
 */
int test_jthread_detach (void){

    tested_thread_sturct_t * tts;
    jthread *thread;
    hythread_t hythread;

    // Initialize tts structures and run all tested threads
    tested_threads_run(default_run_for_test);
    
    // Make second attach to the same jthread.
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);
        hythread = vm_jthread_get_tm_data(tts->java_thread);
        thread = hythread_get_private_data(hythread);
        tf_assert_same(jthread_detach(tts->java_thread), TM_ERROR_NONE);
        tf_assert_null(vm_jthread_get_tm_data(tts->java_thread));
        //tf_assert_null(hythread_get_private_data(hythread));
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();
    return TEST_PASSED;
}

/*
 * Test hythread_create(...)
 */
int test_hythread_create(void) {

    tested_thread_sturct_t *tts;

    // Initialize tts structures and run all tested threads
    tested_threads_run(default_run_for_test);
    
    // Test that all threads are running and have associated structures valid
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);
        check_tested_thread_structures(tts);
        tf_assert(tested_thread_is_running(tts));
    }
    // Terminate all tested threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test hythread_create_with_function(...)
 */
void JNICALL jvmti_start_proc(jvmtiEnv *jvmti_env, JNIEnv *jni_env, void *arg){

    tested_thread_sturct_t * tts = current_thread_tts;
    
    if (tts->jni_env != jni_env){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    if (tts->jvmti_start_proc_arg != arg){
        tts->phase = TT_PHASE_ERROR;
        return;
    }
    tts->phase = TT_PHASE_RUNNING;
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    tts->phase = TT_PHASE_DEAD;
}

int test_hythread_create_with_function(void) {

    tested_thread_sturct_t *tts;
    void * args = &args;

    // Initialize tts structures and run all tested threads
    tested_threads_run_with_jvmti_start_proc(jvmti_start_proc);
    
    // Test that all threads are running and have associated structures valid
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);
        check_tested_thread_structures(tts);
        tf_assert(tested_thread_is_running(tts));
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_join()
 */
void run_for_test_jthread_join(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    tested_thread_sturct_t * prev_tts = tts; 
    IDATA status;

    prev_tested_thread(&prev_tts);
    if (prev_tts == NULL){
        // its the first tested thread
        tts->phase = TT_PHASE_RUNNING;

        while(1){
            tts->clicks++;
            sleep_a_click();
            if (tts->stop) {
                tts->phase = TT_PHASE_DEAD;
                break;
            }
        }
    } else {
        // wait until previous thread ends 
        tts->phase = TT_PHASE_WAITING_ON_JOIN;
        status = jthread_join(prev_tts->java_thread);
        tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    }
}

int test_jthread_join(void) {

    tested_thread_sturct_t * tts;
    
    tf_assert(MAX_TESTED_THREAD_NUMBER > 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_join);

    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    check_tested_thread_phase(tts, TT_PHASE_RUNNING);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_JOIN);
    }

    // make the first thread terminated and test that all threads are terminated
    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    tts->stop = 1;
    check_tested_thread_phase(tts, TT_PHASE_DEAD);
    jthread_sleep(20 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_timed_join()
 */
void run_for_test_jthread_timed_join(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    tested_thread_sturct_t * prev_tts = tts;
    IDATA status;

    prev_tested_thread(&prev_tts);
    tts->phase = TT_PHASE_RUNNING;
    if (prev_tts == NULL){
        // its the first tested thread
        while(1){
            tts->clicks++;
            sleep_a_click();
            if (tts->stop) {
                tts->phase = TT_PHASE_DEAD;
                break;
            }
        }
    } else {
        // wait until timeout or previous thread ends 
        status = jthread_timed_join(prev_tts->java_thread, timed_join_wait_time, 0);
        printf("-------- status = %08x (%i) %i\n", status,  status, timed_join_wait_time);
        tts->phase = TT_PHASE_DEAD;
        if (timed_join_wait_time > CLICK_TIME_MSEC * 10){
            // must be thread end
            //tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
        } else {
            // must be timeout
           //tts->phase = (status == TM_ERROR_TIMEOUT ? TT_PHASE_DEAD : TT_PHASE_ERROR);
        }
    }
}

int test_jthread_timed_join(void) {

    tested_thread_sturct_t * tts;
    
    tf_assert(MAX_TESTED_THREAD_NUMBER > 1);

    /*
     * Test for jthread_timed_join() exit due to timeout
     */
    timed_join_wait_time = 10 * CLICK_TIME_MSEC;
    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_timed_join);

    // skip the first thread
    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    jthread_sleep(20 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    /*
     * Test for jthread_timed_join() before timeout
     */
    timed_join_wait_time = 1000 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER;
    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_timed_join);

    // make the first thread terminated and test that all threads are terminated
    reset_tested_thread_iterator(&tts);
    next_tested_thread(&tts);
    tts->stop = 1;
    check_tested_thread_phase(tts, TT_PHASE_DEAD);
    jthread_sleep(20 * CLICK_TIME_MSEC * MAX_TESTED_THREAD_NUMBER, 0);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_exception_stop()
 */
void run_for_test_jthread_exception_stop(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    
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

int test_jthread_exception_stop (void){

    tested_thread_sturct_t * tts;
    jobject excn = new_jobject();
    hythread_t hythread;
    jvmti_thread_t jvmti_thread;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_exception_stop);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(jthread_exception_stop(tts->java_thread, excn), TM_ERROR_NONE);
        check_tested_thread_phase(tts, TT_PHASE_ANY);
        hythread = vm_jthread_get_tm_data(tts->java_thread);
        jvmti_thread = hythread_get_private_data(hythread);
        tf_assert(vm_objects_are_equal(excn, jvmti_thread->stop_exception));
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_stop()
 */

int test_jthread_stop (void){

    tested_thread_sturct_t * tts;
    JNIEnv * env;
    jclass excn;
    hythread_t hythread;
    jvmti_thread_t jvmti_thread;

    env = new_JNIEnv();
    excn = (*env) -> FindClass(env, "java/lang/ThreadDeath");

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_exception_stop);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(tts->excn, NULL);
        tf_assert_same(jthread_stop(tts->java_thread), TM_ERROR_NONE);
        //check_tested_thread_phase(tts, TT_PHASE_ANY);
        //tf_assert_same(tts->excn, excn);
        hythread = vm_jthread_get_tm_data(tts->java_thread);
        jvmti_thread = hythread_get_private_data(hythread);
        //tf_assert_same(excn, jvmti_thread->stop_exception);

        //tf_assert_same(jthread_stop(tts->java_thread), TM_ERROR_NONE);
        //tf_assert_same(thread_deaf_excn, thread->stop_exception);
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_sleep(...)
 */
void run_for_test_jthread_sleep(void){

    tested_thread_sturct_t * tts = current_thread_tts;
    IDATA status;
    
    tts->phase = TT_PHASE_SLEEPING;
    status = jthread_sleep(1000000, 0);
    tts->phase = (status == TM_ERROR_INTERRUPT ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}
int test_jthread_sleep(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *sleeping_tts;
    int i;
    int sleeping_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_sleep);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_SLEEPING);      
    }
    for (i = 0; i <= MAX_TESTED_THREAD_NUMBER; i++){
        sleeping_nmb = 0;
        sleeping_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_SLEEPING){
                sleeping_nmb++;
                sleeping_tts = tts;
            } else {
                check_tested_thread_phase(tts, TT_PHASE_DEAD);
            }
        }
        if (MAX_TESTED_THREAD_NUMBER - sleeping_nmb - i != 0){
            tf_fail("Wrong number of sleeping threads");
        }
        if (sleeping_nmb > 0){
            tf_assert_same(jthread_interrupt(sleeping_tts->java_thread), TM_ERROR_NONE);
            check_tested_thread_phase(sleeping_tts, TT_PHASE_DEAD);
        }
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_JNI_env(...)
 */
int test_jthread_get_JNI_env(void) {

    tested_thread_sturct_t *tts;

    // Initialize tts structures and run all tested threads
    tested_threads_run(default_run_for_test);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(jthread_get_JNI_env(tts->java_thread), tts->jni_env);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test hythread_yield()
 */
void run_for_test_hythread_yield(void){

    tested_thread_sturct_t * tts = current_thread_tts;

    /*
      IDATA status;
      tts->phase = TT_PHASE_RUNNING;
    */

    while(1){
        tts->clicks++;
        hythread_yield();
        if (tts->stop) {
            break;
        }
    }

    /*
      tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    */
}
int test_hythread_yield (void){

    tested_thread_sturct_t *tts;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_hythread_yield);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tts->stop = 1;
        check_tested_thread_phase(tts, TT_PHASE_DEAD);  
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
} 

TEST_LIST_START
    TEST(test_jthread_attach)
    TEST(test_jthread_detach)
    TEST(test_hythread_create)
    TEST(test_hythread_create_with_function)
    TEST(test_jthread_get_JNI_env)
    TEST(test_jthread_join)
    //TEST(test_jthread_timed_join)
    TEST(test_jthread_exception_stop)
    //TEST(test_jthread_stop)
    TEST(test_jthread_sleep)
    TEST(test_hythread_yield)
TEST_LIST_END;
    
     
