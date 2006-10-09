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

#include <assert.h>
#include "jni.h"
#include "testframe.h"
#include "thread_unit_test_utils.h"
#include <open/jthread.h>
#include <open/hythread.h>
#include <open/hythread_ext.h>
#include <open/ti_thread.h>
#include "apr_time.h"


/*
 * Utilities for thread manager unit tests 
 */

tested_thread_sturct_t * current_thread_tts;
tested_thread_sturct_t dummy_tts_struct;
tested_thread_sturct_t * dummy_tts = &dummy_tts_struct;
tested_thread_sturct_t tested_threads[MAX_TESTED_THREAD_NUMBER];

apr_pool_t *pool = NULL;

void sleep_a_click(void){
    apr_sleep(CLICK_TIME_MSEC * 1000);
}

jthread new_jthread_jobject(JNIEnv * jni_env) {
    const char * name = "<init>";
    const char * sig = "()V";
    jmethodID constructor = NULL;
    jclass thread_class;
    
    thread_class = (*jni_env)->FindClass(jni_env, "java/lang/Thread");
    constructor = (*jni_env)->GetMethodID(jni_env, thread_class, name, sig);
    return (*jni_env)->NewObject(jni_env, thread_class, constructor);
}

jthread new_jobject(){

    apr_status_t status;
    _jobject *obj;
    _jjobject *object;

    if (!pool){
        status = apr_pool_create(&pool, NULL);
        if (status) return NULL; 
    }

    obj = apr_palloc(pool, sizeof(_jobject));
    object = apr_palloc(pool, sizeof(_jjobject));
    assert(obj);
    obj->object = object;
    obj->object->data = NULL;
    obj->object->daemon = 0;
    obj->object->name = NULL;
    return obj;
}

void delete_jobject(jobject obj){
}

void test_java_thread_setup(int argc, char *argv[]) {
    JavaVMInitArgs args;
    JavaVM * java_vm;
    JNIEnv * jni_env;
    int i;

    args.version = JNI_VERSION_1_2;
    args.nOptions = argc;
    args.options = (JavaVMOption *) malloc(args.nOptions * sizeof(JavaVMOption));
    args.options[0].optionString = "-Djava.class.path=.";
    for (i = 1; i < argc; i++) {
        args.options[i].optionString = argv[i];
        args.options[i].extraInfo = NULL;
    }

    log_debug("test_java_thread_init()");

    apr_initialize();
    JNI_CreateJavaVM(&java_vm, &jni_env, &args);
}

void test_java_thread_teardown(void) {

    IDATA status;

    log_debug("test_java_thread_shutdown()");
    //hythread_detach(NULL);

    // status = tm_shutdown(); ??????????????? fix me: 
    // second testcase don't work after tm_shutdown() call
    status = TM_ERROR_NONE; 
        
    /*
     * not required, if there are running threads tm will exit with
     * error TM_ERROR_RUNNING_THREADS
     */
    if(!(status == TM_ERROR_NONE || status == TM_ERROR_RUNNING_THREADS)){
        log_error("test_java_thread_shutdown() FAILED: %d", status);
    }
}

void tested_threads_init(int mode){
        
    tested_thread_sturct_t *tts;
    jobject monitor;
    jrawMonitorID raw_monitor;
    JNIEnv * jni_env;
    IDATA status; 
    int i;
    
    jni_env = jthread_get_JNI_env(jthread_self());
        
    if (mode != TTS_INIT_DIFFERENT_MONITORS){
        monitor = new_jobject();
        status = jthread_monitor_init(monitor);
        tf_assert_same_v(status, TM_ERROR_NONE);
    }
    status = jthread_raw_monitor_create(&raw_monitor);
    tf_assert_same_v(status, TM_ERROR_NONE);

    reset_tested_thread_iterator(&tts);
    for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
        tts = &tested_threads[i];
        tts->my_index = i;
        //tf_assert_null(tts->java_thread);
        tts->java_thread = new_jthread_jobject(jni_env);
        //tf_assert_null(tts->jni_env);
        //tts->attrs.priority = 5;
        tts->jvmti_start_proc_arg = &tts->jvmti_start_proc_arg;
        tts->clicks = 0;
        tts->phase = TT_PHASE_NONE;
        tts->stop = 0;
        if (mode == TTS_INIT_DIFFERENT_MONITORS){
            monitor = new_jobject();
            status = jthread_monitor_init(monitor);
            tf_assert_same_v(status, TM_ERROR_NONE);
        }
        tts->monitor = monitor;
        tts->raw_monitor = raw_monitor;
        tts->excn = NULL;
    }
}

//void tested_threads_shutdown(void){
//      
//      tested_thread_sturct_t *tts;
//      int i;
//      
//      reset_tested_thread_iterator(&tts);
//      for (i = 0; i < MAX_TESTED_THREAD_NUMBER; i++){
//              tts = &tested_threads[i];
//        delete_jobject(tts->java_thread);
//        delete_JNIEnv(tts->jni_env);
//      }
//}

tested_thread_sturct_t *get_tts(int tts_index){
    
    if (tts_index >= 0 && tts_index < MAX_TESTED_THREAD_NUMBER){
        return &tested_threads[tts_index];
    }
    return (void *)NULL;
}

int next_tested_thread(tested_thread_sturct_t **tts_ptr){

    tested_thread_sturct_t *tts = *tts_ptr;
    int tts_index;

    if (! tts){
        tts = &tested_threads[0];
    } else {
        tts_index = tts->my_index;
        if (tts_index >= 0 && tts_index < MAX_TESTED_THREAD_NUMBER - 1){
            tts = &tested_threads[tts_index + 1];
        } else {
            tts = NULL;
        }
    }
    *tts_ptr = tts;
    return tts != NULL;
}

int prev_tested_thread(tested_thread_sturct_t **tts_ptr){
    
    tested_thread_sturct_t *tts = *tts_ptr;
    int tts_index;

    if (! tts){
        tts = &tested_threads[MAX_TESTED_THREAD_NUMBER - 1];
    } else {
        tts_index = tts->my_index;
        if (tts_index > 0 && tts_index < MAX_TESTED_THREAD_NUMBER){
            tts = &tested_threads[tts_index - 1];
        } else {
            tts = NULL;
        }
    }
    *tts_ptr = tts;
    return tts != NULL;
}

void reset_tested_thread_iterator(tested_thread_sturct_t **tts){
    *tts = (void *)NULL;
}

void tested_threads_run_common(jvmtiStartFunction run_method_param){
    tested_thread_sturct_t *tts;
    JNIEnv * jni_env;

    jni_env = jthread_get_JNI_env(jthread_self());

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        current_thread_tts = tts;
        tf_assert_same_v(jthread_create_with_function(jni_env, tts->java_thread, &tts->attrs, run_method_param, NULL), TM_ERROR_NONE);
        check_tested_thread_phase(tts, TT_PHASE_ANY);
        check_tested_thread_structures(tts);
    }
}

void tested_threads_run(jvmtiStartFunction run_method_param){

    tested_threads_init(TTS_INIT_COMMON_MONITOR);
    tested_threads_run_common(run_method_param);
}

void tested_threads_run_with_different_monitors(jvmtiStartFunction run_method_param){

    tested_threads_init(TTS_INIT_DIFFERENT_MONITORS);
    tested_threads_run_common(run_method_param);
}

void tested_threads_run_with_jvmti_start_proc(jvmtiStartFunction jvmti_start_proc){
    tested_thread_sturct_t *tts;
    JNIEnv * jni_env;

    jni_env = jthread_get_JNI_env(jthread_self());
    
    tested_threads_init(TTS_INIT_COMMON_MONITOR);
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        current_thread_tts = tts;
        tf_assert_same_v(jthread_create_with_function(jni_env, tts->java_thread, &tts->attrs,
                                                      jvmti_start_proc, tts->jvmti_start_proc_arg), TM_ERROR_NONE);
        check_tested_thread_phase(tts, TT_PHASE_ANY);
    }
}

void tested_os_threads_run(apr_thread_start_t run_method_param){

    tested_thread_sturct_t *tts;
    apr_thread_t *apr_thread;
    apr_threadattr_t *apr_attrs = NULL;
    apr_status_t status;
    apr_pool_t *pool;

    status = apr_pool_create(&pool, NULL);
    tf_assert_v(status == APR_SUCCESS);
    tested_threads_init(TTS_INIT_COMMON_MONITOR);
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        current_thread_tts = tts;
        apr_thread = NULL;
        // Create APR thread
        status = apr_thread_create(
                                   &apr_thread,      // new thread OS handle 
                                   apr_attrs,
                                   run_method_param, // start proc
                                   NULL,             // start proc arg 
                                   pool
                                   ); 
        tf_assert_v(status == APR_SUCCESS);
        check_tested_thread_phase(tts, TT_PHASE_ANY);
    }
}

int tested_threads_stop(){

    tested_thread_sturct_t *tts;
    
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        if (check_tested_thread_phase(tts, TT_PHASE_DEAD) != TEST_PASSED){
            tts->stop = 1;
            check_tested_thread_phase(tts, TT_PHASE_DEAD);
            //Sleep(1000);
        }
    }
    return TEST_PASSED;
}

int tested_threads_destroy(){

    tested_thread_sturct_t *tts;
    
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tts->stop = 1;
        check_tested_thread_phase(tts, TT_PHASE_DEAD);
    }
    return TEST_PASSED;
}

int check_tested_thread_structures(tested_thread_sturct_t *tts){

    jthread java_thread = tts->java_thread;
    jvmti_thread_t jvmti_thread;
    hythread_t hythread;

    hythread = vm_jthread_get_tm_data(java_thread);
    tf_assert(hythread);
    jvmti_thread = hythread_get_private_data(hythread);
    tf_assert(jvmti_thread);
    /*
      tf_assert_same(jvmti_thread->thread_object->object->data, 
      java_thread->object->data);
      tf_assert_same(jvmti_thread->jenv, tts->jni_env);
    */

    //if(jvmti_thread->stop_exception != stop_exception){
    //      return TEST_FAILED; ????????????????????????????????????????????
    //}

    return TEST_PASSED;
}

int check_tested_thread_phase(tested_thread_sturct_t *tts, int phase){

    int i;
    for (i = 0; i < MAX_CLICKS_TO_WAIT; i++){
        sleep_a_click(); // must be here to give tested thread to change phase

        tf_assert(tts->phase != TT_PHASE_ERROR);
        if (phase == tts->phase){
            return 0;
        } 
        if (phase == TT_PHASE_ANY && tts->phase != TT_PHASE_NONE){
            return 0;
        }
    }

    //    tf_assert_same(phase, tts->phase);
    return 0;
}

int tested_thread_is_running(tested_thread_sturct_t *tts){

    int clicks = tts->clicks;
    int i;

    for (i = 0; i < MAX_CLICKS_TO_WAIT; i++){
        sleep_a_click();
        if (clicks != tts->clicks){
            return 1;
        }
    }
    return 0;
}

int compare_threads(jthread *threads, int thread_nmb, int compare_from_end) {

    int i;
    int j;
    int found;
    int tested_thread_start;
    jthread java_thread;

    // Check that all thread_nmb threads are different

    //printf("----------------------------------------------- %i %i\n", thread_nmb, compare_from_end);
    //for (j = 0; j < MAX_TESTED_THREAD_NUMBER; j++){
    //      printf("[%i] %p\n",j,tested_threads[j].java_thread->object);
    //}
    //printf("\n");
    //for (i = 0; i < thread_nmb; i++){
    //      printf("!!! %p\n", (*(threads + i))->object);
    //}
    for (i = 0; i < thread_nmb - 1; i++){
        java_thread = *(threads + i);
        for (j = i + 1; j < thread_nmb; j++){
            if (*(threads + j) == java_thread){
                return TM_ERROR_INTERNAL;
            }
        }
    }

    // Check that the set of threads are equal to the set of the first 
    // or the last thread_nmb tested threads

    tested_thread_start = compare_from_end ? MAX_TESTED_THREAD_NUMBER - thread_nmb : 0;
    for (i = 0; i < thread_nmb; i++){
        java_thread = *(threads + i);
        found = 0;
        for (j = tested_thread_start; j < thread_nmb + tested_thread_start; j++){
            if (tested_threads[j].java_thread->object == java_thread->object){
                found = 1;
                break;
            }
        }
        tf_assert(found);
    }
    return TM_ERROR_NONE;
}

int compare_pointer_sets(void ** set_a, void ** set_b, int nmb){
    return TEST_FAILED;
}

int check_exception(jobject excn){

    return TM_ERROR_INTERNAL;
    //return TM_ERROR_NONE;
}

void JNICALL default_run_for_test(jvmtiEnv * jvmti_env, JNIEnv * jni_env, void *arg) {

    tested_thread_sturct_t * tts = current_thread_tts;
    
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

