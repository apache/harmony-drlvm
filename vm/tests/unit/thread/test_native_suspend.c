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
#include <apr_atomic.h>
#include <open/hythread_ext.h>
#include "testframe.h"
#include "thread_manager.h"

int start_proc(void *);

int start_proc(void *args);
int test_hythread_thread_suspend(void){
    apr_pool_t *pool;
    void **args; 
    hythread_t thread = NULL;
    hythread_thin_monitor_t lockword_ptr;
    IDATA status;
    int i;
    apr_pool_create(&pool, NULL);

    args = (void**) apr_palloc(pool, sizeof(void *) *3); 
    
    status = hythread_thin_monitor_create(&lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);

    args[0] = &lockword_ptr;
    args[1] = 0;
    hythread_create(&thread, 0, 0, 0, start_proc, args);
    hythread_sleep(500); 
    hythread_suspend_other(thread);
    hythread_suspend_disable();
    status = hythread_thin_monitor_enter(&lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    status = hythread_thin_monitor_notify_all(&lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    status = hythread_thin_monitor_exit(&lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    hythread_suspend_enable();
    tf_assert_same(status, TM_ERROR_NONE);


    for(i = 0; i < 100000; i++) {
        tf_assert_same(args[1], 0);
    }
    
    hythread_resume(thread);
    
    hythread_join(thread);
    
    tf_assert_same((int)args[1], 1);

    return 0;
}


int test_hythread_thread_suspend_all(void){
    apr_pool_t *pool;
    void **args; 
    hythread_t thread = NULL;
    hythread_thin_monitor_t *lockword_ptr;
    IDATA status;
    int i;
    apr_pool_create(&pool, NULL);

    args = (void**) apr_palloc(pool, sizeof(void *) *3); 
    lockword_ptr = (hythread_thin_monitor_t *) apr_palloc(pool, sizeof(hythread_thin_monitor_t)); 
    status = hythread_thin_monitor_create(lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);

    args[0] = lockword_ptr;
    args[1] = 0;
    for(i = 0; i < 10; i++) {
        thread = NULL;
        hythread_create(&thread, 0, 0, 0, start_proc, args);
    } 
    hythread_sleep(500); 
    hythread_suspend_all(NULL, ((HyThread_public*)hythread_self())->group);
    hythread_suspend_disable();
    status = hythread_thin_monitor_enter(lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    status = hythread_thin_monitor_notify_all(lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    status = hythread_thin_monitor_exit(lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    hythread_suspend_enable();
    tf_assert_same(status, TM_ERROR_NONE);


    for(i = 0; i < 100000; i++) {
        tf_assert_same(args[1], 0);
    }
    
    hythread_resume_all(((HyThread_public*)hythread_self())->group);
    
    hythread_join(thread);
    
    tf_assert_same((IDATA)args[1], 1);

    return 0;
}

int start_proc(void *args) {
    hythread_thin_monitor_t *lockword_ptr = (hythread_thin_monitor_t*)((void**)args)[0];
    IDATA *ret =  (IDATA*)args+1;
    IDATA status;
        
    //wait to start
       hythread_suspend_disable();
    status = hythread_thin_monitor_enter(lockword_ptr);
    hythread_thin_monitor_wait(lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    status = hythread_thin_monitor_exit(lockword_ptr);
    tf_assert_same(status, TM_ERROR_NONE);
    *ret =1;
    hythread_suspend_enable();

    return 0;
}

TEST_LIST_START
    TEST(test_hythread_thread_suspend)
    TEST(test_hythread_thread_suspend_all)
TEST_LIST_END;
