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

/**
 * @author Alexander Shipilov
 * @version $Revision: 1.1.2.3 $
 */

#include "test_performance.h"

int test_hythread_cuncurrent_mutex_apr(apr_thread_t* apr_threads_array[],
                                       int THREADS_NUMBER, apr_thread_start_t func);

int test_hythread_cuncurrent_mutex_tm(hythread_t threads_array[],
                                      int THREADS_NUMBER, hythread_entrypoint_t func);

int check_result_few_threads(int difference, int otherdiff);

int test_hythread_cuncurrent_mutex_run_test_few_threads(int THREADS_NUMBER, 
                                                        apr_thread_t* apr_threads_array[],
                                                        apr_thread_start_t apr_func,
                                                        hythread_t threads_array[],
                                                        hythread_entrypoint_t hythread_func);

int test_hythread_cuncurrent_mutex_run_test(int THREADS_NUMBER, 
                                            apr_thread_t* apr_threads_array[],
                                            apr_thread_start_t apr_func,
                                            hythread_t threads_array[],
                                            hythread_entrypoint_t hythread_func);

int iterations;

/*
 * Concurrent mutex functions
 */

int proc_concurrent(void *args) {

    int j = 0;

    hymutex_lock(&tm_mutex_lock);
    hycond_wait(&tm_condition_lock, &tm_mutex_lock);
    hymutex_unlock(&tm_mutex_lock);

    for (j = 0; j < iterations; j++) {
        hymutex_lock(tm_concurrent_mutex_lock);
        concurrent_mutex_data = 1;
        hymutex_unlock(tm_concurrent_mutex_lock);
    }

    return 0;
}

void* APR_THREAD_FUNC proc_apr_concurrent(apr_thread_t *thread, void *args) {

    int j = 0;

    apr_thread_mutex_lock(apr_mutex_lock);
    apr_thread_cond_wait(apr_condition_lock, apr_mutex_lock);
    apr_thread_mutex_unlock(apr_mutex_lock);

    for (j = 0; j < iterations; j++) {
        apr_thread_mutex_lock(apr_concurrent_mutex_lock);
        concurrent_mutex_data = 1;
        apr_thread_mutex_unlock(apr_concurrent_mutex_lock);
    }
    return 0;
}

/*
 * Concurrent mutex tests
 */

int test_hythread_cuncurrent_mutex1(void){

    const int THREADS_NUMBER = 1;
    hythread_t threads_array[1];
    apr_thread_t* apr_threads_array[1];

    return test_hythread_cuncurrent_mutex_run_test_few_threads(THREADS_NUMBER,
                                                               apr_threads_array,
                                                               proc_apr_concurrent,
                                                               threads_array,
                                                               proc_concurrent);
}

int test_hythread_cuncurrent_mutex2(void){

    const int THREADS_NUMBER = 2;
    hythread_t threads_array[2];
    apr_thread_t* apr_threads_array[2];

    return test_hythread_cuncurrent_mutex_run_test_few_threads(THREADS_NUMBER,
                                                               apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex4(void){

    const int THREADS_NUMBER = 4;
    hythread_t threads_array[4];
    apr_thread_t* apr_threads_array[4];

    return test_hythread_cuncurrent_mutex_run_test_few_threads(THREADS_NUMBER,
                                                               apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex8(void){

    const int THREADS_NUMBER = 8;
    hythread_t threads_array[8];
    apr_thread_t* apr_threads_array[8];

    return test_hythread_cuncurrent_mutex_run_test_few_threads(THREADS_NUMBER,
                                                               apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex16(void){

    const int THREADS_NUMBER = 16;
    hythread_t threads_array[16];
    apr_thread_t* apr_threads_array[16];

    return test_hythread_cuncurrent_mutex_run_test(THREADS_NUMBER,
                                                   apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex32(void){

    const int THREADS_NUMBER = 32;
    hythread_t threads_array[32];
    apr_thread_t* apr_threads_array[32];

    return test_hythread_cuncurrent_mutex_run_test(THREADS_NUMBER,
                                                   apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex64(void){

    const int THREADS_NUMBER = 64;
    hythread_t threads_array[64];
    apr_thread_t* apr_threads_array[64];

    return test_hythread_cuncurrent_mutex_run_test(THREADS_NUMBER,
                                                   apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex128(void){

    const int THREADS_NUMBER = 128;
    hythread_t threads_array[128];
    apr_thread_t* apr_threads_array[128];

    return test_hythread_cuncurrent_mutex_run_test(THREADS_NUMBER,
                                                   apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex256(void){

    const int THREADS_NUMBER = 256;
    hythread_t threads_array[256];
    apr_thread_t* apr_threads_array[256];

    return test_hythread_cuncurrent_mutex_run_test(THREADS_NUMBER,
                                                   apr_threads_array, proc_apr_concurrent, threads_array, proc_concurrent);
}

int test_hythread_cuncurrent_mutex_run_test(int THREADS_NUMBER, 
                                            apr_thread_t* apr_threads_array[],
                                            apr_thread_start_t apr_func,
                                            hythread_t threads_array[],
                                            hythread_entrypoint_t hythread_func)
{
    long difference = 0, aprdiff = 0;

    iterations = concurrent_mutex_iterations;

    /* APR test */
    tested_threads_run(default_run_for_test);
    aprdiff = test_hythread_cuncurrent_mutex_apr(apr_threads_array, THREADS_NUMBER, apr_func);
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    difference = test_hythread_cuncurrent_mutex_tm(threads_array, THREADS_NUMBER, hythread_func);
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

int test_hythread_cuncurrent_mutex_run_test_few_threads(int THREADS_NUMBER, 
                                                        apr_thread_t* apr_threads_array[],
                                                        apr_thread_start_t apr_func,
                                                        hythread_t threads_array[],
                                                        hythread_entrypoint_t hythread_func)
{
    long difference = 0, aprdiff = 0;

    iterations = concurrent_mutex_iterations_few_threads;

    /* APR test */
    tested_threads_run(default_run_for_test);
    aprdiff = test_hythread_cuncurrent_mutex_apr(apr_threads_array, THREADS_NUMBER, apr_func);
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    difference = test_hythread_cuncurrent_mutex_tm(threads_array, THREADS_NUMBER, hythread_func);
    tested_threads_destroy();

    return check_result_few_threads(difference, aprdiff);
}

int test_hythread_cuncurrent_mutex_apr(apr_thread_t* apr_threads_array[],
                                       int THREADS_NUMBER, apr_thread_start_t func)
{
    int i;

    apr_time_t start, end;

    void *args = NULL;
    apr_threadattr_t *apr_attrs = NULL;

    apr_pool_t* pool;
    apr_pool_t* locks_pool;

    apr_status_t statapr;

    for (i = 0; i < THREADS_NUMBER; i++) {
        apr_threads_array[i] = NULL;
    }
    // Create pools and locks
    statapr = apr_pool_create(&pool, NULL);
    assert(!statapr);
    statapr = apr_pool_create(&locks_pool, NULL);
    assert(!statapr);
    statapr = apr_thread_cond_create(&apr_condition_lock, locks_pool);
    assert(!statapr);
    statapr = apr_thread_mutex_create(&apr_mutex_lock, APR_THREAD_MUTEX_DEFAULT, locks_pool);
    assert(!statapr);
    statapr = apr_thread_mutex_create(&apr_concurrent_mutex_lock, APR_THREAD_MUTEX_DEFAULT, locks_pool);
    assert(!statapr);
    // Create threads
    for (i = 0; i < THREADS_NUMBER; i++) {
        statapr = apr_thread_create(&apr_threads_array[i], apr_attrs, func, args, pool);
        assert(!statapr);
    }
    jthread_sleep(1000, 1);
    start = apr_time_now();
    statapr = apr_thread_cond_broadcast(apr_condition_lock);
    assert(!statapr);
    for (i = 0; i < THREADS_NUMBER; i++) {
        apr_thread_join(&statapr, apr_threads_array[i]);
        assert(!statapr);
    }
    end = apr_time_now();
    statapr = apr_thread_mutex_destroy(apr_concurrent_mutex_lock);
    assert(!statapr);
    statapr = apr_thread_mutex_destroy(apr_mutex_lock);
    assert(!statapr);
    statapr = apr_thread_cond_destroy(apr_condition_lock);
    assert(!statapr);
    apr_pool_destroy(locks_pool);
    apr_pool_destroy(pool);
    return (end - start);
}

int test_hythread_cuncurrent_mutex_tm(hythread_t threads_array[],
                                      int THREADS_NUMBER, hythread_entrypoint_t func)
{
    int i;

    apr_time_t start, end;

    void *args = NULL;

    IDATA stat;

    for (i = 0; i < THREADS_NUMBER; i++) {
        threads_array[i] = NULL;
    }
    stat = hycond_create(&tm_condition_lock);
    assert(!stat);
    stat = hymutex_create(&tm_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
    assert(!stat);
    stat = hymutex_create(&tm_concurrent_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
    assert(!stat);
    for (i = 0; i < THREADS_NUMBER; i++) {
        hythread_create(&threads_array[i], 0, 0, 0, func, args);
    }
    jthread_sleep(1000, 1);
    start = apr_time_now();
    hycond_notify_all(tm_condition_lock);
    for (i = 0; i < THREADS_NUMBER; i++) {
        hythread_join(threads_array[i]);
    }       
    end = apr_time_now();
    stat = hymutex_destroy(&tm_concurrent_mutex_lock);
    assert(!stat);
    stat = hymutex_destroy(&tm_mutex_lock);
    assert(!stat);
    stat = hycond_destroy(&tm_condition_lock);
    assert(!stat);
    return (end - start);
}

int check_result_few_threads(int difference, int otherdiff) {
    float base, fraction;

    base = difference / concurrent_mutex_iterations_few_threads;
    fraction = difference % concurrent_mutex_iterations_few_threads;
    fraction = fraction / concurrent_mutex_iterations_few_threads;
    base = base + fraction;
    log_info("TMN result is: %4.2f", base);

    base = otherdiff / concurrent_mutex_iterations_few_threads;
    fraction = otherdiff % concurrent_mutex_iterations_few_threads;
    fraction = fraction / concurrent_mutex_iterations_few_threads;
    base = base + fraction;
    log_info("APR result is: %4.2f", base);

    if (difference > (otherdiff * PERF_COEFFICIENT)) {
        if (!(difference == 0 || otherdiff == 0)) {
            return TEST_FAILED;
        }
    }
    return TEST_PASSED;
}

TEST_LIST_START
    TEST(test_hythread_cuncurrent_mutex1)
    TEST(test_hythread_cuncurrent_mutex2)
    TEST(test_hythread_cuncurrent_mutex4)
    TEST(test_hythread_cuncurrent_mutex8)
    TEST(test_hythread_cuncurrent_mutex16)
    TEST(test_hythread_cuncurrent_mutex32)
    TEST(test_hythread_cuncurrent_mutex64)
    TEST(test_hythread_cuncurrent_mutex128)
    TEST(test_hythread_cuncurrent_mutex256)
TEST_LIST_END;
