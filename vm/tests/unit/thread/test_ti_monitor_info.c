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

hylatch_t mon_enter;

int ti_is_enabled() {
    return 1;
}

/*
 * Test jthread_get_contended_monitor(...)
 */
void JNICALL run_for_test_jthread_get_contended_monitor(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);
    // Begin critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    hylatch_count_down(mon_enter);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    hylatch_set(mon_enter, 1);
    // End critical section
    status = jthread_monitor_exit(monitor);
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_get_contended_monitor(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts = NULL;
    jobject contended_monitor;
    int i;

    hylatch_create(&mon_enter, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_contended_monitor);
    
    reset_tested_thread_iterator(&tts);
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        critical_tts = NULL;
        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            hylatch_wait(mon_enter);
            tf_assert_same(jthread_get_contended_monitor(tts->java_thread, &contended_monitor), TM_ERROR_NONE);
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert_null(contended_monitor);
                tf_assert_null(critical_tts);
                critical_tts = tts;
            } else if (tts->phase != TT_PHASE_DEAD){
                check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_MONITOR);
                // This can't be guaranteed
                //tf_assert(vm_objects_are_equal(contended_monitor, tts->monitor));
            }
        }
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_holds_lock(...)
 */
void JNICALL run_for_test_jthread_holds_lock(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

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

int test_jthread_holds_lock(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts = NULL;
    int blocked_count;
    int i;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_holds_lock);
    
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        blocked_count = 0;
        critical_tts = NULL;
        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(jthread_holds_lock(tts->java_thread, tts->monitor) > 0);
                tf_assert_null(critical_tts);
                critical_tts = tts;
            } else if (tts->phase != TT_PHASE_DEAD){
                check_tested_thread_phase(tts, TT_PHASE_WAITING_ON_MONITOR);
                tf_assert(jthread_holds_lock(tts->java_thread, tts->monitor) == 0);
                if (tts->phase == TT_PHASE_WAITING_ON_MONITOR){
                    blocked_count++;
                }
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        tf_assert_same(blocked_count, MAX_TESTED_THREAD_NUMBER - i - 1);
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_lock_owner(...)
 */
void JNICALL run_for_test_jthread_get_lock_owner(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

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

int test_jthread_get_lock_owner(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts = NULL;
    jthread lock_owner = NULL;
    int blocked_count;
    int i;

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_lock_owner);
    
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        blocked_count = 0;
        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                critical_tts = tts;
            } else if (tts->phase == TT_PHASE_WAITING_ON_MONITOR){
                blocked_count++;
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        tf_assert_same(blocked_count, MAX_TESTED_THREAD_NUMBER - i - 1);
        tf_assert_same(jthread_get_lock_owner(critical_tts->monitor, &lock_owner), TM_ERROR_NONE);
        tf_assert(lock_owner);
        tf_assert_same(critical_tts->java_thread->object, lock_owner->object);
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_owned_monitors(...)
 */
void JNICALL run_for_test_jthread_get_owned_monitors(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg){

    tested_thread_sturct_t * tts = current_thread_tts;
    jobject monitor = tts->monitor;
    IDATA status;
    
    tts->phase = TT_PHASE_WAITING_ON_MONITOR;
    status = jthread_monitor_enter(monitor);

    // Begin critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_IN_CRITICAL_SECTON : TT_PHASE_ERROR);
    hylatch_count_down(mon_enter);
    while(1){
        tts->clicks++;
        sleep_a_click();
        if (tts->stop) {
            break;
        }
    }
    hylatch_set(mon_enter, 1);
    status = jthread_monitor_exit(monitor);
    // End critical section
    tts->phase = (status == TM_ERROR_NONE ? TT_PHASE_DEAD : TT_PHASE_ERROR);
}

int test_jthread_get_owned_monitors(void) {

    tested_thread_sturct_t *tts;
    tested_thread_sturct_t *critical_tts;
    int i;
    jint owned_monitors_count;
    jobject *owned_monitors = NULL;

    hylatch_create(&mon_enter, 1);

    // Initialize tts structures and run all tested threads
    tested_threads_run(run_for_test_jthread_get_owned_monitors);
    
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        critical_tts = NULL;

        reset_tested_thread_iterator(&tts);
        while(next_tested_thread(&tts)){
            check_tested_thread_phase(tts, TT_PHASE_ANY); // to make thread running
            hylatch_wait(mon_enter);
            tf_assert_same(jthread_get_owned_monitors (tts->java_thread, 
                                                       &owned_monitors_count, &owned_monitors), TM_ERROR_NONE);
            if (tts->phase == TT_PHASE_IN_CRITICAL_SECTON){
                tf_assert(critical_tts == NULL); // error if two threads in critical section
                critical_tts = tts;
                tf_assert_same(owned_monitors_count, 1);
                tf_assert_same(owned_monitors[0]->object, tts->monitor->object);
            } else if (tts->phase == TT_PHASE_WAITING_ON_MONITOR){
                tf_assert_same(owned_monitors_count, 0);
            }
        }
        tf_assert(critical_tts); // thread in critical section found
        critical_tts->stop = 1;
        check_tested_thread_phase(critical_tts, TT_PHASE_DEAD);
    }
    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_jthread_get_contended_monitor)
    TEST(test_jthread_holds_lock)
    TEST(test_jthread_get_lock_owner)
    TEST(test_jthread_get_owned_monitors)
TEST_LIST_END;
