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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#define LOG_DOMAIN "kernel"
#include "cxxlog.h"

#include "thread_generic.h"
#include "object_handles.h"
#include "mon_enter_exit.h"
#include "open/thread.h"

#include "java_lang_VMThreadManager.h"

/*
 * Class:     java_lang_VMThreadManager
 * Method:    currentThread
 * Signature: ()Ljava/lang/Thread;
 */
JNIEXPORT jobject JNICALL Java_java_lang_VMThreadManager_currentThread
  (JNIEnv * UNREF jenv, jclass)
{
    return thread_current_thread();
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    holdsLock
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMThreadManager_holdsLock
  (JNIEnv * UNREF jenv, jclass, jobject lock)
{
    //ToDo: the following code will be used.
    //return thread_holds_lock(thread_current_thread(), lock);

    VM_thread * vm_thread = get_thread_ptr();
    tmn_suspend_disable();       //---------------------------------v

    ManagedObject *p_obj = (ManagedObject *)(((ObjectHandle)lock)->object);
    uint16 stack_key = STACK_KEY(p_obj);

    tmn_suspend_enable();        //---------------------------------^

    return (jboolean)((vm_thread -> thread_index == stack_key) ? JNI_TRUE : JNI_FALSE);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    interrupt
 * Signature: (Ljava/lang/Thread;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_interrupt
  (JNIEnv * UNREF jenv, jclass, jobject jthread)
{
    thread_interrupt(jthread);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    isInterrupted
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMThreadManager_isInterrupted__
  (JNIEnv * UNREF jenv, jclass)
{
    jobject thread = thread_current_thread();
    return thread_is_interrupted(thread, JNI_TRUE);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    isInterrupted
 * Signature: (Ljava/lang/Thread;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMThreadManager_isInterrupted__Ljava_lang_Thread_2
  (JNIEnv * UNREF jenv, jclass, jobject jthread)
{
    return thread_is_interrupted(jthread, JNI_FALSE);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    notify
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_notify
  (JNIEnv * UNREF jenv, jclass, jobject obj)
{
    thread_object_notify(obj);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    notifyAll
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_notifyAll
  (JNIEnv * UNREF jenv, jclass, jobject obj)
{
    thread_object_notify_all(obj);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    resume
 * Signature: (Ljava/lang/Thread;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_resume
  (JNIEnv * UNREF jenv, jclass, jobject jthread)
{
    thread_resume(jthread);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    setPriority
 * Signature: (Ljava/lang/Thread;I)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_setPriority
  (JNIEnv * UNREF jenv, jclass, jobject UNREF jthread, jint UNREF priority)
{
    //ToDo: the following code will be used.
    //thread_set_priority(jthread, priority);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    sleep
 * Signature: (JI)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_sleep
  (JNIEnv * UNREF jenv, jclass, jlong millis, jint nanos)
{
#if defined (__INTEL_COMPILER)   // intel compiler
  #pragma warning( push )
  #pragma warning (disable:1682) // explicit conversion of a 64-bit integral type to a smaller integral type
#elif defined (_MSC_VER)  // MS compiler
  #pragma warning( push )
  #pragma warning (disable:4244) // conversion from 'jlong' to 'long', possible loss of data
#endif

    thread_sleep(thread_current_thread(), millis, nanos); 

#if defined (__INTEL_COMPILER) || defined (_MSC_VER)
 #pragma warning( pop )
#endif
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    start
 * Signature: (Ljava/lang/Thread;J)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_start
  (JNIEnv *jenv, jclass, jobject thread, jlong UNREF stackSize)
{
    thread_start(jenv, thread);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    stop
 * Signature: (Ljava/lang/Thread;Ljava/lang/Throwable;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_stop
  (JNIEnv *, jclass, jobject UNREF thread, jthrowable UNREF threadDeathException)
{
    //ToDo: the following code will be used.
    //thread_stop(thread, threadDeathException);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    suspend
 * Signature: (Ljava/lang/Thread;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_suspend
  (JNIEnv * UNREF jenv, jclass, jobject jthread)
{
    thread_suspend(jthread);
}


/*
 * Class:     java_lang_VMThreadManager
 * Method:    wait
 * Signature: (Ljava/lang/Object;JI)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_wait
  (JNIEnv *, jclass, jobject object, jlong millis, jint nanos)
{
    thread_object_wait_nanos (object, millis, nanos);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    yield
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_yield
  (JNIEnv * UNREF jenv, jclass)
{
    //ToDo: the following code will be used.
    //thread_yield(thread_current_thread());

    SleepEx(0, false);
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    isDead
 * Signature: (Ljava/lang/Thread;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMThreadManager_isDead
  (JNIEnv *jenv, jclass, jobject thread)
{
    //return ! thread_is_alive(thread);

    VM_thread *p_vmthread = get_vm_thread_ptr_safe(jenv, thread);
    if ( !p_vmthread ) {
        return 1; // don't try to isAlive() non-existant thread
    }   
    
    java_state as = p_vmthread->app_status;  
    // According to JAVA spec, this method should return true, if and
    // only if the thread has been died. 
    switch (as) {
        case thread_is_sleeping:
        case thread_is_waiting:
        case thread_is_timed_waiting:
        case thread_is_blocked:
        case thread_is_running:
        case thread_is_birthing:
            return 0; 
            //break;
        case thread_is_dying:
        case zip:
            return 1; 
            //break;
        default:
            ABORT("Unexpected thread state");
            return 1;
            //break;
    }
    // must return from inside the switch statement //remark #111: statement is unreachable
}

/*
 * Class:     java_lang_VMThreadManager
 * Method:    join
 * Signature: (Ljava/lang/Thread;JI)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThreadManager_join
  (JNIEnv * UNREF jenv, jclass, jobject thread, jlong millis, jint nanos)
{
#if defined (__INTEL_COMPILER)   // intel compiler
  #pragma warning( push )
  #pragma warning (disable:1682) // explicit conversion of a 64-bit integral type to a smaller integral type
#elif defined (_MSC_VER)  // MS compiler
  #pragma warning( push )
  #pragma warning (disable:4244) // conversion from 'jlong' to 'long', possible loss of data
#endif

    thread_join(thread, millis, nanos);

#if defined (__INTEL_COMPILER) || defined (_MSC_VER)
#pragma warning( pop )
#endif
}

