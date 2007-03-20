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
 * @version $Revision$
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "testframe.h"
#include "thread_unit_test_utils.h"
#include "apr_time.h"
#include "apr_pools.h"
#include "apr_thread_cond.h"
#include "apr_thread_proc.h"
#include "apr_thread_mutex.h"
#include "thread_private.h"

/* 
 * Number of iterations for each test
 * PERF_FIDELITY ~ measurement_fidelity ~ 1/running_time
 * Make sure that PERF_FIDELITY * ITERATIONS don't exceed threads limit
 */
int const PERF_FIDELITY = 10;

/*
 * Coefficient shows how much TMH performance could be worse than APR
 */
float const PERF_COEFFICIENT = 3;

/*
 * Locks for waiting
 */
hymutex_t tm_mutex_lock;
hycond_t tm_condition_lock = NULL;
apr_thread_mutex_t* apr_mutex_lock = NULL;
apr_thread_cond_t* apr_condition_lock = NULL;

/*
* Locks for concurrent mutex tests
*/
hymutex_t tm_concurrent_mutex_lock;
apr_thread_mutex_t* apr_concurrent_mutex_lock = NULL;

/*
* Data variable and iterations constant for concurrent mutex tests
*/
int concurrent_mutex_data = 1;
int const concurrent_mutex_iterations = 1000;
int const concurrent_mutex_iterations_few_threads = 150000;

int check_result(int difference, int otherdiff) {
    log_info("TMN result is: %i", difference/concurrent_mutex_iterations);
    log_info("APR result is: %i", otherdiff/concurrent_mutex_iterations);
    if (difference > (otherdiff * PERF_COEFFICIENT)) {
        if (!(difference == 0 || otherdiff == 0)) {
            return TEST_FAILED;
        }
    }
    return TEST_PASSED;
}
