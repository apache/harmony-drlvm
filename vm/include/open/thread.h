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
/** 
 * @author Sergey Petrovsky
 * @version $Revision: 1.1.2.1.4.3 $
 */  
#ifndef _OPEN_THREAD_H_
#define _OPEN_THREAD_H_

#include "open/types.h"
#include "jvmti_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file thread_generic.h
 * 
 * This module describes VM interfaces to work with Java threads.
 * 
 */

//---------------------- Object -------------------------------------

/**
 * Just one thread from the set of threads 
 * waiting on the <code>object</code> object's monitor is waked up.
 *
 * Nothing happens if the set is empty.
 *
 * @param object
 */
void thread_object_notify(jobject object); 

/**
 * Each thread from the set of threads waiting on the 
 * <code>object</code> object's monitor is waked up.
 *
 * Nothing happens if the set is empty.
 *
 * @param object
 */
void thread_object_notify_all(jobject object); 

/**
 * Current thread gets waiting on the <code>object</code> object's monitor.
 * 
 * The thread <code>object</code> waiting on <code>object</code>
 * object's monitor is waked up if:
 * <UL>
 * <LI>another thread invokes <code>thread_notify(object)</code> 
 * and VM chooses this thread to wake up;
 * <LI>another thread invokes <code>thread_notifyAll(object);</code>
 * <LI>another thread invokes <code>thread_interrupt(thread);</code> 
 * <LI>real time elapsed from the wating begin is 
 * greater or equal the time specified. 
 * </UL>
 *
 * @param object
 * @param millis
 */
void thread_object_wait (jobject object, int64 millis);

/**
 * Current thread gets waiting on the <code>object</code> object's monitor.
 * 
 * The thread <code>object</code> waiting on <code>object</code>
 * object's monitor is waked up if:
 * <UL>
 * <LI>another thread invokes <code>thread_notify(object)</code> 
 * and VM chooses this thread to wake up;
 * <LI>another thread invokes <code>thread_notifyAll(object);</code>
 * <LI>another thread invokes <code>thread_interrupt(thread);</code> 
 * <LI>real time elapsed from the wating begin is 
 * greater or equal the time specified. 
 * </UL>
 *
 * @param object
 * @param millis
 * @param nanos
 */
void thread_object_wait_nanos (jobject object, int64 millis, int nanos);

//---------------------- Thread -------------------------------------
/**
 * Returns the monitor the thread <code>thread</code> is contended for. 
 *
 * Returns <code>NULL</code> if there's no such monitor.
 *
 * @param thread
 * @return monitor the thread <code>thread</code> is contended for.
 */
jobject thread_contends_for_lock(jobject thread); 

/**
 * Returns current thread.
 */
jobject thread_current_thread();

/**
 * Safe point.
 */
VMEXPORT void tmn_safe_point();

/**
 * Disables suspend for the current thread.
 */
VMEXPORT void tmn_suspend_disable();

/**
 * Disables suspend for the current thread and increments the recursion counter.
 */
void tmn_suspend_disable_recursive();

/**
 * Checks if suspend enabled, if so calls thread_disable suspend and returns true;
 * otherwise returns false.
 */
//VMEXPORT bool tmn_suspend_disable_and_return_old_value();

/**
 * Notify that it's safe to suspend thread.
 * 
 * After a call of this function and till <code>thread_disable_suspend()</code>
 * is called any call of <code>thread_is_suspend_enabled()</code> 
 * returns <code>true</code>.
 */
VMEXPORT void tmn_suspend_enable();

/**
 * Decrements a recursion counter. 
 * 
 * If the counter value becomes zero, 
 * after a call of this function and till <code>thread_disable_suspend()</code>
 * is called any call of <code>thread_is_suspend_enabled()</code> 
 * returns <code>true</code>.
 */
void tmn_suspend_enable_recursive();

/**
 * Returns <code>true</code> if it's safe to suspend the thread <code>thread</code>.
 *
 * Otherwise returns <code>false</code>. 
 *
 * @return <code>true</code> if it's safe to suspend the thread <code>thread</code>;
 * <code>false</code> otherwise.
 */
VMEXPORT bool tmn_is_suspend_enabled();

/**
 * Returns a set of of all existig alive threads.
 *
 * The set is returned as an array of <code>jthread</code> pointers.
 * The value returned is the number of alive threads.
 * No array is returned if <code>threads == NULL</code>.
 *
 * @param threads
 * @return the number of alive threads.
 */
int thread_get_all_threads(jthread** threads); 

/**
 * Gets <code>thread</code> thread's local storage. 
 *
 * @param env
 * @param thread
 * @param data_ptr
 */
int thread_get_jvmti_thread_local_storage(jvmtiEnv* env,
                                      jthread thread, void** data_ptr);
/**
 * Gets all monitors owned by the thread <code>thread</code>. 
 *
 * @param thread
 * @param monitors
 */
int thread_get_owned_monitor_info(jthread thread, jobject ** monitors);

/**
 * Returns the <code>thread</code> thread's state according 
 * JVM TI specification.
 *
 * @param thread
 * @return the <code>thread</code> thread's state.
 */
int thread_get_thread_state(jthread thread); 

/**
 * Returns <code>true</code> if the thread <code>thread</code> 
 * holds monitor <code>monitor</code>.
 *
 * Otherwise returns <code>false</code>.
 *
 * @param thread
 * @param monitor
 * @return <code>true</code> if the thread <code>thread</code>
 * holds monitor <code>monitor</code>; <code>false</code> otherwise.
 */
bool thread_holds_lock(jthread thread, jobject monitor); 

/**
 * Returns a set of monitors the thread <code>thread</code> holds.
 *
 * The set is returned as an array of <code>jobject</code> pointers.
 * The value returned is the number of monitors the thread holds.
 * No array is returned if <code>monitors == NULL</code>.
 *
 * @param thread
 * @param monitors
 * @return the number of monitors the thread <code>thread</code> holds.
 */
int thread_holds_locks(jthread thread, jobject* monitors); 

/**
 * Wakes up the thread <code>thread</code> or marks it as 'interrupted'. 
 *
 * If the thread <code>thread</code> is wating due to 
 * <code>thread_wait(...)</code> or <code>thread_sleep(...)</code> 
 * function call it's waked up.
 * 
 * Else <code>thread</code> thread's state is changed so that 
 * <code>thread.isInterrupted()</code> returns <code>true</code>. 
 *
 * @param thread
 */
void thread_interrupt(jthread thread); 

/**
 * Returns <code>true</code> if the thread <code>thread</code> is alive.
 *
 * Otherwise returns <code>false</code>. 
 * A thread gets alive just after it has been started 
 * and is alive till it has died. 
 *
 * @param thread
 * @return <code>true</code> if the thread <code>thread</code>
 * is alive; <code>false</code> otherwise.
 */
bool thread_is_alive(jthread thread); 

/**
 * Returns <code>true</code> if the thread <code>thread</code> is interrupted.
 *
 * Otherwise returns <code>false</code>. 
 *
 * @param thread
 * @param clear
 * @return <code>true</code> if the thread <code>thread</code>
 * is interrupted; <code>false</code> otherwise.
 */
bool thread_is_interrupted(jthread thread, bool clear);

/**
 * Waits till the thread <code>thread</code> is dead or time specified is expired.
 *
 * Precisely, the function is returned if the thread <code>thread</code> 
 * is dead or real time elapsed from wating begin 
 * is greater or equal the time specified.
 *
 * @param thread
 * @param millis
 * @param nanos
 */
void thread_join(jthread thread, long millis, int nanos); 

/**
 * Resumes suspended thread <code>thread</code> execution.
 *
 * Precisely, cancels the effect of all previous <
 * code>thread_suspend(thread)</code> calls. 
 * <P>So, the thread <code>thread</code> may proceed execution.
 *
 * @param thread
 */
void thread_resume(jthread thread); 

/**
 * Sets <code>thread</code> thread's local storage. 
 *
 * @param env
 * @param thread
 * @param data
 */
int thread_set_jvmt_thread_local_storage(jvmtiEnv* env,
                                      jthread thread, const void* data);
/**
 * Sets <code>thread</code> thread's priority. 
 *
 * The <code>thread</code> thread's priority gets 
 * equal to <code>newPriority</code>. 
 *
 * @param thread
 * @param newPriority
 */
void thread_set_priority(jthread thread, int newPriority); 

/**
 * Makes thread <code>thread</code> to sleep for specified time. 
 *
 * Thread <code>thread</code> gets sleeping till another thread
 * invokes the <code>thread_interrupt(thread)</code> method or real time 
 * elapsed from sleeping begin is greater or equal the time specified. 
 *
 * @param thread
 * @param millis
 * @param nanos
 */
void thread_sleep(jthread thread, long millis, int nanos); 

/**
 * Thread <code>thread</code> execution begins. 
 *
 * The <code>thread</code> thread's <code>run()</code> method is called. 
 *
 * @param thread
 */
VMEXPORT void  thread_start(JNIEnv*, jthread thread);

/**
 * Forces thread <code>thread</code> termination.
 *
 * Precisely, makes the thread <code>thread</code> to throw 
 * <code>threadDeathException</code> from the current execution point.
 *
 * @param thread
 * @param threadDeathException
 */
void thread_stop(jthread thread, jobject threadDeathException); 

// GC can only be disabled in native code.  In jitted code GC is always
// "enabled."  Of course it doesn't mean that GC can happen at any point
// in jitted code -- only at GC-safe points.

/**
 * Suspends the thread <code>thread</code> execution. 
 *
 * The <code>thread</code> thread's execution is suspended till 
 * <code>thread_resume(thread)</code> is called from another thread.
 *
 * @param thread
 */
void thread_suspend(jthread thread); 

/**
 * Suspends the thread <code>thread</code> execution for a while. 
 *
 * So, the other threads have possibility to be executed.
 *
 * @param thread
 */
void thread_yield(jthread thread);
//------------------------------------------------------------
//------------------------------------------------------------

/**
 * Get the current thread's interrupt event. 
 */
VMEXPORT void* thread_get_interrupt_event();

#ifdef __cplusplus
}
#endif
#endif // _OPEN_THREAD_H_
