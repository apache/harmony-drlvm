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
int HYTHREAD_PROC run_for_test_jthread_attach(void *args){
    tested_thread_sturct_t * tts;
    hythread_t native_thread;
    JNIEnv * jni_env;
    IDATA status;

    tts = (tested_thread_sturct_t *) args;
    native_thread = hythread_self();
    if (!native_thread) {
        status = hythread_attach(&native_thread);
        if (status != JNI_OK) {
            tts->phase = TT_PHASE_ERROR;
            return 0;
        }
    }

    status = vm_attach(GLOBAL_VM, &jni_env, NULL);
    if (status != JNI_OK) {
        tts->phase = TT_PHASE_ERROR;
        return 0;
    }

    status = jthread_attach(jni_env, tts->java_thread, JNI_FALSE);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_ATTACHED : TT_PHASE_ERROR);
    tested_thread_started(tts);
    tested_thread_wait_for_stop_request(tts);
    status = jthread_detach(tts->java_thread);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
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
    return test_jthread_attach();
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
        tested_thread_wait_running(tts);
    }
    // Terminate all tested threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test hythread_create_with_function(...)
 */
void JNICALL jvmti_start_proc(jvmtiEnv *jvmti_env, JNIEnv *jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t * ) args;
    
    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);
    tested_thread_wait_for_stop_request(tts);
    tts->phase = TT_PHASE_DEAD;
    tested_thread_ended(tts);
}

int test_hythread_create_with_function(void) {

    tested_thread_sturct_t *tts;
    void * args = &args;

    // Initialize tts structures and run all tested threads
    tested_threads_run(jvmti_start_proc);
    
    // Test that all threads are running and have associated structures valid
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        check_tested_thread_phase(tts, TT_PHASE_RUNNING);
        check_tested_thread_structures(tts);
        tested_thread_wait_running(tts);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_join()
 */
void JNICALL run_for_test_jthread_join(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    tested_thread_sturct_t * prev_tts = tts; 
    IDATA status;

    prev_tested_thread(&prev_tts);
    if (prev_tts == NULL){
        // its the first tested thread
        tts->phase = TT_PHASE_RUNNING;
        tested_thread_started(tts);
        tested_thread_wait_for_stop_request(tts);
        tts->phase = TT_PHASE_DEAD;
        tested_thread_ended(tts);
    } else {
        // wait until previous thread ends 
        tts->phase = TT_PHASE_WAITING_ON_JOIN;
        tested_thread_started(tts);
        status = jthread_join(prev_tts->java_thread);
        tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
        tested_thread_ended(tts);
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
    tested_thread_send_stop_request(tts);
    tested_thread_wait_ended(tts);
    while(next_tested_thread(&tts)){
        tested_thread_wait_ended(tts);
    }

    // Terminate all threads (not needed here) and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_timed_join()
 */
void JNICALL run_for_test_jthread_timed_join(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    tested_thread_sturct_t * prev_tts = tts;
    IDATA status;

    prev_tested_thread(&prev_tts);
    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);
    if (prev_tts == NULL){
        tested_thread_wait_for_stop_request(tts);
        tts->phase = TT_PHASE_DEAD;
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
    tested_thread_ended(tts);
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
    tested_thread_send_stop_request(tts);
    tested_thread_wait_ended(tts);
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
void JNICALL run_for_test_jthread_exception_stop(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    
    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);
    while(tested_thread_wait_for_stop_request_timed(tts, CLICK_TIME_MSEC) == TM_ERROR_TIMEOUT){
        hythread_safe_point();
    }
    tts->phase = TT_PHASE_DEAD;
    tested_thread_ended(tts);
}

int test_jthread_exception_stop (void){

    tested_thread_sturct_t * tts;
    jobject excn;
    hythread_t hythread;
    jvmti_thread_t jvmti_thread;
    JNIEnv * jni_env;

    jni_env = jthread_get_JNI_env(jthread_self());
    excn = new_jobject_thread_death(jni_env);

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

    env = jthread_get_JNI_env(jthread_self());
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
void JNICALL run_for_test_jthread_sleep(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;
    IDATA status;
    
    tts->phase = TT_PHASE_SLEEPING;
    tested_thread_started(tts);
    status = jthread_sleep(1000000, 0);
    tts->phase = (status == TM_ERROR_INTERRUPT ? TT_PHASE_DEAD : TT_PHASE_ERROR);
    tested_thread_ended(tts);
}

int test_jthread_sleep(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *sleeping_tts;
    int i;
    int sleeping_nmb;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_sleep);

    for (i = 0; i <= MAX_TESTED_THREAD_NUMBER; i++){
        sleeping_nmb = 0;
        sleeping_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            if (tts->phase == TT_PHASE_SLEEPING){
                sleeping_nmb++;
                sleeping_tts = tts;
            } else {
                check_tested_thread_phase(tts, TT_PHASE_DEAD);
            }
        }
        if (MAX_TESTED_THREAD_NUMBER - i != sleeping_nmb){            
            tf_fail("Wrong number of sleeping threads");
        }
        if (sleeping_nmb > 0){
            tf_assert_same(jthread_interrupt(sleeping_tts->java_thread), TM_ERROR_NONE);
            tested_thread_wait_ended(sleeping_tts);
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

    tf_assert(jthread_get_JNI_env(jthread_self()) != 0);
    return TEST_PASSED;
}

/*
 * Test hythread_yield()
 */
void JNICALL run_for_test_hythread_yield(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *args){

    tested_thread_sturct_t * tts = (tested_thread_sturct_t *) args;

    tts->phase = TT_PHASE_RUNNING;
    tested_thread_started(tts);

    while(tested_thread_wait_for_stop_request_timed(tts, CLICK_TIME_MSEC) == TM_ERROR_TIMEOUT){
        hythread_yield();
    }

    tts->phase = TT_PHASE_DEAD;
    tested_thread_ended(tts);

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
        tested_thread_send_stop_request(tts);
        tested_thread_wait_ended(tts);  
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
    TEST(test_jthread_timed_join)
    TEST(test_jthread_exception_stop)
    //TEST(test_jthread_stop)
    TEST(test_jthread_sleep)
    TEST(test_hythread_yield)
TEST_LIST_END;
    
     
