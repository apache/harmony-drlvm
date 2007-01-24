/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements. See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


#ifndef _OPEN_THREAD_GENERIC_H
#define _OPEN_THREAD_GENERIC_H

/**
 * @file
 * @brief Java threading interface
 * @details
 * Java threading interface - contains functions to work with Java threads. 
 * The generic part od Java thrading interface is mostly targeted to address 
 * the needs of <code>java.lang.Object</code> and <code>java.lang.Thread</code> 
 * classes implementations.
 * All functions in this interface start with <code><>jthread_*</code> prefix.
 * The implemnentation of this layer provides the mapping of Java thrads onto 
 * native/OS threads.
 * 
 * For more detailes, see thread manager component documentation located at 
 * <code>vm/thread/doc/ThreadManager.htm</code>
 */

#include "open/types.h"
#include "open/hythread_ext.h"
#include <jni.h>
#include <jvmti.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/** 
 * @name Basic manipulation
 */
//@{

typedef struct JVMTIThread *jvmti_thread_t;

/**
 * Java thread creation attributes.
 */
typedef struct {
  
   /**
    * Thread scheduling priority.
    */  
    jint priority;

   /**
    * Thread stack size.
    */  
    jint stacksize;

   /**
    * Denotes whether Java thread is daemon.  
    * JVM exits when the only threads running are daemon threads.
    */  
    jboolean daemon;

   /**
    * JVM TI Environment.
    */  
    jvmtiEnv * jvmti_env;

} jthread_threadattr_t;

jlong jthread_thread_init(jvmti_thread_t *ret_thread, JNIEnv* env, jthread java_thread, jobject weak_ref, jlong old_thread);
IDATA jthread_create(JNIEnv * jni_env, jthread thread, jthread_threadattr_t *attrs);
IDATA jthread_create_with_function(JNIEnv * jni_env, jthread thread, jthread_threadattr_t *attrs, jvmtiStartFunction proc, const void* arg);
IDATA jthread_attach(JNIEnv * jni_env, jthread thread, jboolean daemon);
 IDATA jthread_detach(jthread thread);
IDATA jthread_join(jthread thread);
IDATA jthread_timed_join(jthread thread, jlong millis, jint nanos);
IDATA jthread_yield();
IDATA jthread_stop(jthread thread);
IDATA jthread_exception_stop(jthread thread, jobject throwable);
IDATA jthread_sleep(jlong millis, jint nanos);
JNIEnv *jthread_get_JNI_env(jthread thread);
IDATA jthread_wait_for_all_nondaemon_threads();



//@}
/** @name Identification
 */
//@{

jthread jthread_self(void);
jlong jthread_get_id(jthread thread);
jthread jthread_get_thread(jlong thread_id);



//@}
/** @name Top&lt;-&gt;middle pointer conversion
 */
//@{


hythread_t  jthread_get_native_thread(jthread thread);
jthread jthread_get_java_thread(hythread_t thread);


//@}
/** @name Attributes access
 */
//@{


IDATA jthread_set_priority(jthread thread, int priority);
int jthread_get_priority(jthread thread);
jboolean jthread_is_daemon(jthread thread);

/**
 * Sets the name for the <code>thread</code>.
 *
 * @param[in] thread those attribute is set
 * @param[in] name thread name
 *
 * @sa <code>java.lang.Thread.setName()</code>
 */
IDATA jthread_set_name(jthread thread, jstring name);

/**
 * Returns the name for the <code>thread</code>.
 *
 * @param[in] - thread those attribute is read
 *
 * @sa <code>java.lang.Thread.getName()</code>
 */
jstring jthread_get_name(jthread thread);


//@}
/** @name Interruption
 */
//@{

IDATA jthread_interrupt(jthread thread);
jboolean jthread_is_interrupted(jthread thread);
IDATA jthread_clear_interrupted(jthread thread);


//@}
/** @name Monitors
 */
//@{

 IDATA jthread_monitor_init(jobject mon);
VMEXPORT IDATA jthread_monitor_enter(jobject mon);
 IDATA jthread_monitor_try_enter(jobject mon);
VMEXPORT IDATA jthread_monitor_exit(jobject mon);
 IDATA jthread_monitor_notify(jobject mon);
 IDATA jthread_monitor_notify_all(jobject mon);
 IDATA jthread_monitor_wait(jobject mon);
 IDATA jthread_monitor_timed_wait(jobject mon, jlong millis, jint nanos);

//@}
/** @name Parking
 */
//@{

 IDATA jthread_park();
 IDATA jthread_timed_park(jlong millis, jint nanos);
 IDATA jthread_unpark(jthread thread);
 IDATA jthread_park_until(jlong milis);

//@}
/** @name Suspension
 */
//@{

 IDATA jthread_suspend(jthread thread);
 IDATA jthread_suspend_all(jvmtiError* results, jint count, const jthread* thread_list);
 IDATA jthread_resume(jthread thread);
 IDATA jthread_resume_all(jvmtiError* results, jint count, const jthread* thread_list);
 IDATA jthread_cancel_all();


#ifdef __cplusplus
}
#endif

#endif  /* _OPEN_THREAD_GENERIC_H */
