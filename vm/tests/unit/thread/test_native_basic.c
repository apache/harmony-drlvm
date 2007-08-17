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
#include <apr_pools.h>
#include <apr_time.h>
#include <open/hythread_ext.h>
#include "testframe.h"
#include "thread_manager.h"

int start_proc(void *);
int start_proc_empty(void *);

int test_hythread_self_base(void) {
    hythread_t thread;
    //check that this thread is attached(tm_init called)
    ////
    tf_assert(thread = hythread_self());
   
    return 0; 
}

/*
 * Test tm_create(..)
 */
int test_hythread_create(void){
    apr_pool_t *pool;
    void **args; 
    hythread_t thread;
    IDATA r;
    
    apr_pool_create(&pool, NULL);

    args = (void**) apr_palloc(pool, sizeof(void *) *2); 
    
    hythread_group_create((hythread_group_t *)&args[0]); 
    
    args[1] = apr_palloc(pool, sizeof(jthread_threadattr_t));
    ((jthread_threadattr_t *)args[1])->stacksize = 1024000;
    ((jthread_threadattr_t *)args[1])->priority  = 1;
    
    thread = (hythread_t)calloc(1, hythread_get_struct_size());
    assert(thread);
    r = hythread_create_with_group(thread, args[0], 1024000, 1,
        (hythread_entrypoint_t)start_proc, args);
    tf_assert(r == 0 && "thread creation failed");

    r = hythread_join(thread);
    tf_assert(r == 0 && "thread join failed");
    return TEST_PASSED;
}

// Waits until count of running threads in specified group reaches 'count' or less
static void wait_for_all_treads_are_terminated(hythread_group_t group, int count)
{
    int max_tries = 1000; // Maximum count of iterations

    while (max_tries--)
    {
        int n = 0;
        hythread_t thread;

        hythread_iterator_t iterator = hythread_iterator_create(group);

        while(hythread_iterator_has_next(iterator))
        {
            thread = hythread_iterator_next(&iterator);

            if (!hythread_is_terminated(thread))
                ++n;
        }

        hythread_iterator_release(&iterator);

        if (n <= count)
            break;

        apr_sleep(1000); // 1ms
    }

    apr_sleep(100000);// 0.1s to let system threads finish their work
}

hylatch_t start;
hylatch_t end;

int test_hythread_iterator(void) {
    hythread_group_t group = NULL;
    hythread_t thread = NULL;
    hythread_iterator_t iterator;
    const int n = 100;
    int i;

    hythread_group_create(&group);
    hylatch_create(&start, n);
    hylatch_create(&end, 1);

    for (i = 0; i < n; i++) {
        thread = (hythread_t)calloc(1, hythread_get_struct_size());
        assert(thread);
        hythread_create_with_group(thread, group, 0, 0,
            (hythread_entrypoint_t)start_proc_empty, NULL);
    }

    // Wait util all threads have started.
    hylatch_wait(start);
    iterator = hythread_iterator_create(group);
    // Notify all threads
    hylatch_count_down(end);

    printf ("iterator size: %d\n", (int)hythread_iterator_size(iterator));
    tf_assert(hythread_iterator_size(iterator) == n);
    i = 0;
    while(hythread_iterator_has_next(iterator)) {
        i++;
        thread = hythread_iterator_next(&iterator);
        tf_assert(hythread_is_alive(thread));
    }

    tf_assert(i == n);

    hythread_iterator_release(&iterator);

    wait_for_all_treads_are_terminated(group, i - n);

    return 0;
}

int test_hythread_iterator_default(void) {
    hythread_t thread = NULL;
    hythread_iterator_t iterator;
    const int n = 100;
    int i;

    hylatch_create(&start, n);
    hylatch_create(&end, 1);

    for (i = 0; i < n; i++) {
        thread = (hythread_t)calloc(1, hythread_get_struct_size());
        assert(thread);
        hythread_create_with_group(thread, NULL, 0, 0,
            (hythread_entrypoint_t)start_proc_empty, NULL);
    }

    // Wait util all threads have started.
    hylatch_wait(start);
    iterator = hythread_iterator_create(NULL);
    // Notify all threads
    hylatch_count_down(end);

    printf("default group iterator: %d\n", (int)hythread_iterator_size(iterator));
    tf_assert(hythread_iterator_size(iterator) >= n);
    i = 0;
    while(hythread_iterator_has_next(iterator)) {
        i++;
        thread = hythread_iterator_next(&iterator);
    }

    tf_assert(i >= n);

    hythread_iterator_release(&iterator);

    wait_for_all_treads_are_terminated(NULL, i - n);

    return 0;
}



/*
 * Test tm_create(..)
 */
int test_hythread_create_many(void){
    apr_pool_t *pool;
    void **args; 
    hythread_t thread = NULL;
    hythread_group_t group = NULL;
    IDATA r;
    
    char *buf;
    int i = 10;
    
    hythread_group_create(&group);
    while(i--) {
        apr_pool_create(&pool, NULL);

        args = (void**) apr_palloc(pool, sizeof(void *) *2); 
    
        args[0] = group; 
        
        args[1] = apr_palloc(pool, sizeof(jthread_threadattr_t));
        ((jthread_threadattr_t *)args[1])->stacksize = 1024000;
        ((jthread_threadattr_t *)args[1])->priority  = 1;
        
        thread = (hythread_t)calloc(1, hythread_get_struct_size());
        assert(thread);
        r = hythread_create_with_group(thread, group, 1024000, 1,
            (hythread_entrypoint_t)start_proc, args);
        tf_assert(r == 0 && "thread creation failed");
        buf = (char *)apr_pcalloc(pool, sizeof(char)*12);

        /*
          sprintf(buf, "Thread %d\0", i);
          hythread_set_name(thread, buf);
        */
        r = hythread_join(thread);
        tf_assert(r == 0 && "thread join failed");
    }

    //check thread structures:
    //1. thread get group
    //2. check that group contains 10 threads
    //NOTE: native structures should not be freed untill tm_thread_destroy method
    ////
    
    //1.group
    ////
    tf_assert(group);
    //tf_assert(group->threads_count == 0);

    return 0;
}

int start_proc(void *args) {
    void** attrs = (void **)args; 
    tf_assert_same(hythread_get_priority(hythread_self()), ((jthread_threadattr_t *)attrs[1])->priority);
    tf_assert_same(((HyThread_public*)hythread_self())->group, attrs[0]);
    return 0;
}

int start_proc_empty(void *args) {
    hylatch_count_down(start);
    hylatch_wait(end);
    return 0;
}

TEST_LIST_START
    TEST(test_hythread_self_base)
    TEST(test_hythread_create)
    TEST(test_hythread_create_many)
    TEST(test_hythread_iterator)
    TEST(test_hythread_iterator_default)
TEST_LIST_END;
