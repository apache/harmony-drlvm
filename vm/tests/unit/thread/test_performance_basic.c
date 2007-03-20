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

int proc_empty(void *args) {
    return 0;
}

void* APR_THREAD_FUNC proc_apr_empty(apr_thread_t *thread, void *args) {
    return 0;
}

int proc_waiting(void *args) {
    hymutex_lock(&tm_mutex_lock);
    hycond_wait(&tm_condition_lock, &tm_mutex_lock);
    hymutex_unlock(&tm_mutex_lock);
    return 0;
}

void* APR_THREAD_FUNC proc_apr_waiting(apr_thread_t *thread, void *args) {
    apr_thread_mutex_lock(apr_mutex_lock);
    apr_thread_cond_wait(apr_condition_lock, apr_mutex_lock);
    apr_thread_mutex_unlock(apr_mutex_lock);
    return 0;
}

/*
 * Test hythread_self()
 */
int test_hythread_self(void){

    apr_time_t start, end;

    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 1500000;

    hythread_t tm_native_thread;

    apr_status_t statapr;

    /* APR test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            statapr = apr_threadkey_private_get((void **)(&tm_native_thread), TM_THREAD_KEY);
            assert(!statapr);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            tm_native_thread = hythread_self();
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
 * Test test_hymutex_create_destroy()
 */
int test_hymutex_create_destroy(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 15000;

    apr_pool_t *pool;

    IDATA stat;
    apr_status_t statapr;

    /* APR test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            statapr = apr_pool_create(&pool, NULL);
            assert(!statapr);
            statapr = apr_thread_mutex_create(&apr_mutex_lock, APR_THREAD_MUTEX_DEFAULT, pool);
            assert(!statapr);
            statapr = apr_thread_mutex_destroy(apr_mutex_lock);
            assert(!statapr);
            apr_pool_destroy(pool);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    apr_mutex_lock = NULL;
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            stat = hymutex_create(&tm_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
            assert(!stat);
            stat = hymutex_destroy(&tm_mutex_lock);
            assert(!stat);
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
 * Test test_hymutex_lock_unlock()
 */
int test_hymutex_lock_unlock(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 150000;

    apr_pool_t *pool;

    IDATA stat;
    apr_status_t statapr;

    /* APR test */
    tested_threads_run(default_run_for_test);
    statapr = apr_pool_create(&pool, NULL);
    assert(!statapr);
    statapr = apr_thread_mutex_create(&apr_mutex_lock, APR_THREAD_MUTEX_DEFAULT, pool);
    assert(!statapr);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            statapr = apr_thread_mutex_lock(apr_mutex_lock);
            assert(!statapr);
            statapr = apr_thread_mutex_unlock(apr_mutex_lock);                      
            assert(!statapr);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    statapr = apr_thread_mutex_destroy(apr_mutex_lock);
    assert(!statapr);
    apr_pool_destroy(pool);
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    stat = hymutex_create(&tm_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
    assert(!stat);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            stat = hymutex_lock(&tm_mutex_lock);
            assert(!stat);
            stat = hymutex_unlock(&tm_mutex_lock);
            assert(!stat);
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    stat = hymutex_destroy(&tm_mutex_lock);
    assert(!stat);
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
 * Test test_hymutex_trylock_unlock()
 */
int test_hymutex_trylock_unlock(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 150000;

    apr_pool_t *pool;

    IDATA stat;
    apr_status_t statapr;

    /* APR test */
    tested_threads_run(default_run_for_test);
    statapr = apr_pool_create(&pool, NULL);
    assert(!statapr);
    statapr = apr_thread_mutex_create(&apr_mutex_lock, APR_THREAD_MUTEX_DEFAULT, pool);
    assert(!statapr);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            statapr = apr_thread_mutex_trylock(apr_mutex_lock);
            assert(!statapr);
            statapr = apr_thread_mutex_unlock(apr_mutex_lock);                      
            assert(!statapr);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    statapr = apr_thread_mutex_destroy(apr_mutex_lock);
    assert(!statapr);
    apr_pool_destroy(pool);
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    stat = hymutex_create(&tm_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
    assert(!stat);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            stat = hymutex_trylock(&tm_mutex_lock);
            assert(!stat);
            stat = hymutex_unlock(&tm_mutex_lock);
            assert(!stat);
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    stat = hymutex_destroy(&tm_mutex_lock);
    assert(!stat);
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
 * Test test_hythread_create()
 */
int test_hythread_create(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 300;

    int OTHER_FIDELITY;
    int const MAX_THREADS = 1000;

    hythread_t thread = NULL;
    void *args = NULL;
    apr_thread_t *apr_thread = NULL;
    apr_threadattr_t *apr_attrs = NULL;
    apr_pool_t *pool;

    IDATA stat;
    apr_status_t statapr;

    if ((ITERATIONS * PERF_FIDELITY) > MAX_THREADS) {
        OTHER_FIDELITY = (MAX_THREADS / ITERATIONS);
    } else {
        OTHER_FIDELITY = PERF_FIDELITY;
    }

    /* APR test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < OTHER_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            statapr = apr_pool_create(&pool, NULL);
            assert(!statapr);
            statapr = apr_thread_create(&apr_thread, apr_attrs, proc_apr_empty, args, pool);
            apr_thread_join(&statapr, apr_thread);
            assert(!statapr);
            assert(!statapr);
            apr_pool_destroy(pool);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    aprdiff = aprdiff / OTHER_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < OTHER_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            thread = NULL;
            stat = hythread_create(&thread, 0, 0, 0, proc_empty, args);
            assert(!stat);
            stat = hythread_join(thread);
            assert(!stat);
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    difference = difference / OTHER_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
* Test test_hythread_thread_suspend_enable_disable()
*/
int test_hythread_thread_suspend_enable_disable(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 1500000;

    apr_status_t statapr;

    hythread_t tm_native_thread;

    /* APR test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            statapr = apr_threadkey_private_get((void **)(&tm_native_thread), TM_THREAD_KEY);
            assert(!statapr);
            statapr = apr_threadkey_private_set((void *)(tm_native_thread), TM_THREAD_KEY);
            assert(!statapr);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            hythread_suspend_disable();
            hythread_suspend_enable();
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
* Test test_hythread_set_private_data()
*/
int test_hythread_set_private_data(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 1000000;

    hythread_t thread = NULL;
    void *args = NULL;
    void *data = NULL;
    apr_thread_t *apr_thread = NULL;
    apr_threadattr_t *apr_attrs = NULL;

    apr_pool_t *pool;
    apr_pool_t *locks_pool;

    IDATA stat;
    apr_status_t statapr;

    /* APR test */
    tested_threads_run(default_run_for_test);
    // Create pools, locks and thread
    statapr = apr_pool_create(&pool, NULL);
    assert(!statapr);
    statapr = apr_pool_create(&locks_pool, NULL);
    assert(!statapr);
    statapr = apr_thread_cond_create(&apr_condition_lock, locks_pool);
    assert(!statapr);
    statapr = apr_thread_mutex_create(&apr_mutex_lock, APR_THREAD_MUTEX_DEFAULT, locks_pool);
    assert(!statapr);
    statapr = apr_thread_create(&apr_thread, apr_attrs, proc_apr_waiting, args, pool);
    assert(!statapr);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            stat = apr_thread_data_set(data, "DATA", 0, apr_thread);
            assert(!stat);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    statapr = apr_thread_cond_signal(apr_condition_lock);
    assert(!statapr);
    apr_thread_join(&statapr, apr_thread);
    assert(!statapr);
    statapr = apr_thread_mutex_destroy(apr_mutex_lock);
    assert(!statapr);
    statapr = apr_thread_cond_destroy(apr_condition_lock);
    assert(!statapr);
    apr_pool_destroy(locks_pool);
    apr_pool_destroy(pool);
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    // Create locks and thread
    stat = hycond_create(&tm_condition_lock);
    assert(!stat);
    stat = hymutex_create(&tm_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
    assert(!stat);
    stat = hythread_create(&thread, 0, 0, 0, proc_waiting, args);
    assert(!stat);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            stat = hythread_set_private_data(thread, data);
            assert(!stat);
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    stat = hycond_notify(tm_condition_lock);
    assert(!stat);
    stat = hythread_join(thread);
    assert(!stat);
    stat = hymutex_destroy(&tm_mutex_lock);
    assert(!stat);
    stat = hycond_destroy(tm_condition_lock);
    assert(!stat);
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

/*
* Test test_hythread_get_private_data()
*/
int test_hythread_get_private_data(void) {

    apr_time_t start, end;
    long difference = 0, aprdiff = 0;

    int i, j;
    long ITERATIONS = 1000000;

    hythread_t thread = NULL;
    void *args = NULL;
    void *data = NULL;
    apr_thread_t *apr_thread = NULL;
    apr_threadattr_t *apr_attrs = NULL;

    apr_pool_t *pool;
    apr_pool_t *locks_pool;

    IDATA stat;
    apr_status_t statapr;

    /* APR test */
    tested_threads_run(default_run_for_test);
    // Create pools, locks and thread
    statapr = apr_pool_create(&pool, NULL);
    assert(!statapr);
    statapr = apr_pool_create(&locks_pool, NULL);
    assert(!statapr);
    statapr = apr_thread_cond_create(&apr_condition_lock, locks_pool);
    assert(!statapr);
    statapr = apr_thread_mutex_create(&apr_mutex_lock, APR_THREAD_MUTEX_DEFAULT, locks_pool);
    assert(!statapr);
    statapr = apr_thread_create(&apr_thread, apr_attrs, proc_apr_waiting, args, pool);
    assert(!statapr);
    // Set private data
    stat = apr_thread_data_set(data, "DATA", 0, apr_thread);
    assert(!stat);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            stat = apr_thread_data_get(&data, "DATA", apr_thread);
            assert(!stat);
        }
        end = apr_time_now();
        aprdiff = aprdiff + (end - start);
    }
    statapr = apr_thread_cond_signal(apr_condition_lock);
    assert(!statapr);
    apr_thread_join(&statapr, apr_thread);
    assert(!statapr);
    statapr = apr_thread_mutex_destroy(apr_mutex_lock);
    assert(!statapr);
    statapr = apr_thread_cond_destroy(apr_condition_lock);
    assert(!statapr);
    apr_pool_destroy(locks_pool);
    apr_pool_destroy(pool);
    aprdiff = aprdiff / PERF_FIDELITY;
    tested_threads_destroy();

    /* Thread manager test */
    tested_threads_run(default_run_for_test);
    // Create locks and thread
    stat = hycond_create(&tm_condition_lock);
    assert(!stat);
    stat = hymutex_create(&tm_mutex_lock, APR_THREAD_MUTEX_DEFAULT);
    assert(!stat);
    stat = hythread_create(&thread, 0, 0, 0, proc_waiting, args);
    assert(!stat);
    // Set private data
    stat = hythread_set_private_data(thread, data);
    assert(!stat);
    for (j = 0; j < PERF_FIDELITY; j++) {
        start = apr_time_now();
        for (i = 0; i < ITERATIONS; i++) {
            data = hythread_get_private_data(thread);
            assert(!stat);
        }
        end = apr_time_now();
        difference = difference + (end - start);
    }
    stat = hycond_notify(tm_condition_lock);
    assert(!stat);
    stat = hythread_join(thread);
    assert(!stat);
    stat = hymutex_destroy(&tm_mutex_lock);
    assert(!stat);
    stat = hycond_destroy(tm_condition_lock);
    assert(!stat);
    difference = difference / PERF_FIDELITY;
    tested_threads_destroy();

    return check_result(difference, aprdiff);
}

TEST_LIST_START
    TEST(test_hythread_self)
    TEST(test_hymutex_create_destroy)
    TEST(test_hymutex_lock_unlock)
    TEST(test_hymutex_trylock_unlock)
    TEST(test_hythread_create)
    TEST(test_hythread_thread_suspend_enable_disable)
    TEST(test_hythread_set_private_data)
    TEST(test_hythread_get_private_data)
TEST_LIST_END;
