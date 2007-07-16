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
 * @file thread_java_suspend.c
 * @brief Java thread suspend/resume functions
 */

#include <open/jthread.h>
#include <open/hythread_ext.h>
#include <open/thread_externals.h>
#include "vm_threads.h"

/**
 * Resumes the suspended thread <code>thread</code> execution.
 *
 * This function cancels the effect of all previous
 * <code>thread_suspend(thread)</code> calls such that
 * the thread <code>thread</code> may proceed with execution.
 *
 * @param[in] java_thread thread to be resumed
 * @sa java.lang.Thread.resume(), JVMTI::ResumeThread()
 */
IDATA VMCALL jthread_resume(jthread java_thread)
{
    hythread_t native_thread = vm_jthread_get_tm_data(java_thread);
    hythread_resume(native_thread);
    return TM_ERROR_NONE;
} // jthread_resume

/**
 * Resumes the suspended threads from the list.
 *
 * @param[out] results list of error codes for resume result
 * @param[in] count number of threads in the list
 * @param[in] thread_list list of threads to be resumed
 * @sa JVMTI::ResumeThreadList()
 */
IDATA VMCALL
jthread_resume_all(jvmtiError *results,
                   jint count,
                   const jthread *thread_list)
{
    for (jint i = 0; i < count; i++) {
        results[i] = (jvmtiError)jthread_resume(thread_list[i]);
    }
    return TM_ERROR_NONE;
} // jthread_resume_all

/**
 * Suspends the <code>thread</code> execution.
 *
 * The execution of <code>java_thread</code> is suspended till
 * <code>jthread_resume(java_thread)</code> is called from another thread.
 *
 * Note: this function implements safe suspend based on the safe points and
 * regions. This means that:
 * <ul>
 *   <li>If the thread is in Java code,
 *        it runs till the safe point and puts itself into a wait state.
 *   <li>If the thread is running native code, it runs till
 *        the end of safe region and then puts itself into a wait state,
 * </ul>
 *
 * @param[in] java_thread thread to be suspended
 * @sa java.lang.Thread.suspend(), JVMTI::SuspendThread()
 */
IDATA VMCALL jthread_suspend(jthread java_thread)
{
    hythread_t native_thread = vm_jthread_get_tm_data(java_thread);
    return hythread_suspend_other(native_thread);
} // jthread_suspend

/**
 * Suspends the threads from the list.
 *
 * The <code>thread</code> thread's execution is suspended till
 * <code>thread_resume(thread)</code> is called from another thread.
 *
 * Note: this function implements safe suspend based on the safe points and
 * regions. This means that:
 * <ul>
 *   <li>If the thread is in Java code,
 *        it runs till the safe point and puts itself into wait state.
 *   <li>If the thread is running native code, it runs till
 *        the end of safe region and then puts itself into a wait state,
 * </ul>
 *
 * @param[out] results list of error codes for suspension result
 * @param[in] count number of threads in the list
 * @param[in] thread_list list of threads to be suspended
 * @sa JVMTI::SuspendThreadList()
 */
IDATA VMCALL
jthread_suspend_all(jvmtiError *results,
                    jint count,
                    const jthread *thread_list)
{
    for (jint i = 0; i < count; i++) {
        results[i] = (jvmtiError)jthread_suspend(thread_list[i]);
    }
    return TM_ERROR_NONE;
} // jthread_suspend_all
