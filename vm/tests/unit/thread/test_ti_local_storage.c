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

#include "testframe.h"
#include "thread_unit_test_utils.h"
#include <open/ti_thread.h>

/*
 * Test jthread_set_local_storage(...)
 */
int test_jthread_set_local_storage(void) {


    tested_thread_sturct_t *tts;
    void * data;

    // Initialize tts structures and run all tested threads
    tested_threads_run(default_run_for_test);
    
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(jthread_set_local_storage(tts->java_thread, (const void*)tts), TM_ERROR_NONE);
    }
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        tf_assert_same(jthread_get_local_storage(tts->java_thread, &data), TM_ERROR_NONE);
        tf_assert_same(data, tts);
    }

    // Terminate all threads and clear tts structures
    tested_threads_destroy();

    return TEST_PASSED;
}

/*
 * Test jthread_get_local_storage(...)
 */
int test_jthread_get_local_storage(void) {

    return test_jthread_set_local_storage();
}

TEST_LIST_START
    TEST(test_jthread_set_local_storage)
    TEST(test_jthread_get_local_storage)
TEST_LIST_END;
