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
 * @file thread_java_basic.c
 * @brief Key threading operations like thread creation and pointer conversion.
 */

#include <open/jthread.h>
#include <open/hythread_ext.h>
#include <apr_atomic.h>
#include "open/thread_externals.h"
#include "thread_private.h"
#include "jni.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.java"

void stop_callback(void);
jmethodID getRunMethod(JNIEnv *env, jthread java_thread);
IDATA increase_nondaemon_threads_count(hythread_t self);
IDATA countdown_nondaemon_threads(hythread_t self);

typedef struct  {
    JavaVM * java_vm;
    jboolean daemon;
    jvmtiEnv *tiEnv;
    jvmtiStartFunction tiProc;
    void *tiProcArgs;
    void *vm_thread_dummies;
} wrapper_proc_data;


IDATA associate_native_and_java_thread(JNIEnv* env, jthread java_thread, hythread_t tm_native_thread, jobject thread_ref);

/**
 * Creates new Java thread.
 *
 * The newly created thread will immediately start to execute the <code>run()</code>
 * method of the appropriate <code>thread</code> object.
 *
 * @param[in] jni_env jni environment for the current thread.
 * @param[in] java_thread Java thread object with which new thread must be associated.
 * @param[in] attrs thread attributes.
 * @sa java.lang.Thread.run()
 */
IDATA jthread_create(JNIEnv * jni_env, jthread java_thread, jthread_threadattr_t *attrs) {

    return jthread_create_with_function(jni_env, java_thread, attrs, NULL, NULL);
}

int wrapper_proc(void *arg) {
    IDATA status;
    JNIEnv * jni_env;
    hythread_t native_thread;
    jvmti_thread_t jvmti_thread;
    jthread java_thread;
    wrapper_proc_data *data = (wrapper_proc_data *)arg;
    
    // Association should be already done.
    native_thread = hythread_self();
    jvmti_thread = hythread_get_private_data(native_thread);
    assert(jvmti_thread);
    java_thread = jvmti_thread->thread_object;

    status = vm_attach(data->java_vm, &jni_env, data->vm_thread_dummies);
    assert (status == JNI_OK);

    jvmti_thread->jenv = jni_env;
    jvmti_thread->daemon = data->daemon;

    TRACE(("TM: Java thread started: id=%d OS_handle=%p", native_thread->thread_id, apr_os_thread_current()));

    if (!jvmti_thread->daemon) {
        increase_nondaemon_threads_count(native_thread);
    }

    // Send Thread Start event.
    jvmti_send_thread_start_end_event(1);
    
    thread_start_count();


    if (data->tiProc != NULL) {
        data->tiProc(data->tiEnv, jni_env, data->tiProcArgs);
    } else {
        // for jthread_create();
        (*jni_env) -> CallVoidMethodA(jni_env, java_thread, getRunMethod(jni_env, java_thread), NULL);
    }
    
    status = jthread_detach(java_thread);

    TRACE(("TM: Java thread finished: id=%d OS_handle=%p", native_thread->thread_id, apr_os_thread_current()));

    return status;
}

/**
 * Creates new Java thread with specific execution function.
 *
 * This function differs from <code>jthread_create</code> such that
 * the newly created thread, instead of invoking Java execution engine,
 * would start to directly execute the specific native function pointed by the <code>proc</code>.
 * This method of thread creation would be useful for creating TI agent threads (i.e. Java threads
 * which always execute only native code).
 *
 * @param[in] jni_env jni environment for the current thread.
 * @param[in] java_thread Java thread object with which new thread must be associated.
 * @param[in] attrs thread attributes.
 * @param[in] proc the start function to be executed in this thread.
 * @param[in] arg The argument to the start function. Is passed as an array.
 * @sa JVMTI::RunAgentThread()
 */
IDATA jthread_create_with_function(JNIEnv * jni_env, jthread java_thread, jthread_threadattr_t *attrs, jvmtiStartFunction proc, const void* arg)
{
    hythread_t tm_native_thread = NULL;
    jvmti_thread_t tm_java_thread;
    void *vm_thread_dummies;
    
    wrapper_proc_data * data;
    IDATA status;
    
    if (jni_env == NULL || java_thread == NULL || attrs == NULL) {
        return TM_ERROR_NULL_POINTER;
    }
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    
    // This is for irregular use. In ordinary live valid jthread instance
    // contains weak reference associated with it and native thread to reuse. 
    if (tm_native_thread == NULL) {
        if ( !jthread_thread_init(NULL, jni_env, java_thread, NULL, 0)) {
            return TM_ERROR_OUT_OF_MEMORY;
        }
        tm_native_thread = vm_jthread_get_tm_data(java_thread);
    }

    tm_java_thread = hythread_get_private_data(tm_native_thread);
    assert(tm_java_thread);
    tm_java_thread->thread_object = (*jni_env)->NewGlobalRef(jni_env, java_thread);

    data = apr_palloc(tm_java_thread->pool, sizeof(wrapper_proc_data));
    if (data == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
    }

    // Get JavaVM 
    status = (*jni_env) -> GetJavaVM(jni_env, &data->java_vm);
    if (status != JNI_OK) return TM_ERROR_INTERNAL;

    // Allocate memory needed by soon to be born thread
    vm_thread_dummies = vm_allocate_thread_dummies(data->java_vm);

    if (vm_thread_dummies == NULL) {
	return TM_ERROR_OUT_OF_MEMORY;
    }

    // prepare args for wrapper_proc
    data->daemon = attrs->daemon;
    data->tiEnv  = attrs->jvmti_env;
    data->tiProc = proc;
    data->tiProcArgs = (void *)arg;
    data->vm_thread_dummies = vm_thread_dummies;

    status = hythread_create(&tm_native_thread, (attrs->stacksize)?attrs->stacksize:1024000,
                               attrs->priority, 0, wrapper_proc, data);

    TRACE(("TM: Created thread: id=%d", tm_native_thread->thread_id));

    return status;
}

/**
 * Attaches the current native thread to Java VM.
 *
 * This function will create a control structure for Java thread
 * and associate it with the current native thread. Nothing happens
 * if this thread is already attached.
 *
 * @param[in] jni_env JNI environment for current thread
 * @param[in] java_thread j.l.Thread instance to associate with current thread
 * @param[in] daemon JNI_TRUE if attaching thread is a daemon thread, JNI_FALSE otherwise
 * @sa JNI::AttachCurrentThread ()
 */
IDATA jthread_attach(JNIEnv * jni_env, jthread java_thread, jboolean daemon) {
    hythread_t tm_native_thread;
    jvmti_thread_t jvmti_thread;
    IDATA status;

    // Do nothing if thread already attached.
    if (jthread_self() != NULL) return TM_ERROR_NONE;

    tm_native_thread = hythread_self();
    assert(tm_native_thread);    

    status = associate_native_and_java_thread(jni_env, java_thread, tm_native_thread, NULL);
    if (status != TM_ERROR_NONE) return status;

    jvmti_thread = hythread_get_private_data(tm_native_thread);
    assert(jvmti_thread);
    jvmti_thread->thread_object = (*jni_env)->NewGlobalRef(jni_env, java_thread);
    jvmti_thread->jenv = jni_env;
    jvmti_thread->daemon = daemon;

    if (!jvmti_thread->daemon) {
        increase_nondaemon_threads_count(tm_native_thread);
    }

    // Send Thread Start event.
    jvmti_send_thread_start_end_event(1);

    thread_start_count();

    TRACE(("TM: Current thread attached to jthread=%p", java_thread));
    return TM_ERROR_NONE;
}

/**
 * Associates the Java thread with the native thread.
 *
 * @param[in] ret_thread not used
 * @param[in] env JNI environment that will be associated with the created Java thread
 * @param[in] java_thread the Java thread for the association
 * @param[in] weak_ref java.lang.WeakReference to the <code>java_thread</code> that used for native resource deletion
 * @param[in] old_thread the native thread for the association
 * @return the native thread
 */
jlong jthread_thread_init(jvmti_thread_t *ret_thread, JNIEnv* env, jthread java_thread, jobject weak_ref, jlong old_thread) {
    hythread_t tm_native_thread = NULL;
    jvmti_thread_t tmj_thread;
    IDATA status;

    if (old_thread) {
        tm_native_thread = (hythread_t)((IDATA)old_thread);
        tmj_thread = (jvmti_thread_t)hythread_get_private_data(tm_native_thread);
        //delete used weak reference
        if (tmj_thread->thread_ref) (*env)->DeleteGlobalRef(env, tmj_thread->thread_ref);       
    }
    
    status = hythread_struct_init(&tm_native_thread);
    if (status != TM_ERROR_NONE) {
        return 0;
    }
    
    status = associate_native_and_java_thread(env, java_thread, tm_native_thread, weak_ref);
    if (status != TM_ERROR_NONE) {
        return 0;
    }
    return (jlong)((IDATA)tm_native_thread);
}

/**
 * Detaches the selected thread from java VM.
 *
 * This function will release any resources associated with the given thread.
 *
 * @param[in] java_thread Java thread to be detached
 */
IDATA jthread_detach(jthread java_thread) {
    IDATA status;
    hythread_t tm_native_thread;
    jvmti_thread_t tm_jvmti_thread;
    JNIEnv * jni_env;

    assert(hythread_is_suspend_enabled());
     
    // Check input arg
    assert(java_thread);
    TRACE(("TM: jthread_detach %x", hythread_self()));

    tm_native_thread = jthread_get_native_thread(java_thread);
    tm_jvmti_thread = hythread_get_private_data(tm_native_thread);
    jni_env = tm_jvmti_thread->jenv;

    if (!tm_jvmti_thread->daemon) {
        countdown_nondaemon_threads(tm_native_thread);
    }

    // Detach from VM.
    status = vm_detach(java_thread);
    if (status != JNI_OK) return TM_ERROR_INTERNAL;

    // Delete global reference to current thread object.
    (*jni_env)->DeleteGlobalRef(jni_env, tm_jvmti_thread->thread_object);
    // jthread_self() will return NULL now.
    tm_jvmti_thread->thread_object = NULL;

    // Decrease alive thread counter
    thread_end_count();

    // Deallocate tm_jvmti_thread 
    //apr_pool_destroy(tm_jvmti_thread->pool);

    assert(hythread_is_suspend_enabled());
    return TM_ERROR_NONE;    
}

IDATA associate_native_and_java_thread(JNIEnv * jni_env, jthread java_thread, hythread_t tm_native_thread, jobject thread_ref)
{
    IDATA status;
    apr_status_t apr_status;
    apr_pool_t *pool;
    jvmti_thread_t tm_java_thread;
    
    if ((jni_env == NULL) || (java_thread == NULL) || (tm_native_thread == NULL)) {
        return TM_ERROR_NULL_POINTER;
    }
    
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    if (!tm_java_thread) {
        apr_status = apr_pool_create(&pool, 0);
        if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
        if (pool == NULL) return TM_ERROR_OUT_OF_MEMORY;
        tm_java_thread = apr_palloc(pool, sizeof(JVMTIThread));
        if (tm_java_thread == NULL) return TM_ERROR_OUT_OF_MEMORY;
    
        tm_java_thread->pool = pool;

        status = hythread_set_private_data(tm_native_thread, tm_java_thread);
        if (status != TM_ERROR_NONE) return status;
    }
    // JNI environment is created when this thread attaches to VM.
    tm_java_thread->jenv = NULL;
    tm_java_thread->thread_ref    = (thread_ref) ? (*jni_env)->NewGlobalRef(jni_env, thread_ref) : NULL; 
    tm_java_thread->contended_monitor = 0;
    tm_java_thread->wait_monitor = 0;
    tm_java_thread->blocked_count = 0;
    tm_java_thread->blocked_time = 0;
    tm_java_thread->waited_count = 0;
    tm_java_thread->waited_time = 0;
    tm_java_thread->owned_monitors = 0;
    tm_java_thread->owned_monitors_nmb = 0;
    tm_java_thread->jvmti_local_storage.env = 0;
    tm_java_thread->jvmti_local_storage.data = 0;

    // Associate java_thread with tm_thread      
    vm_jthread_set_tm_data(java_thread, tm_native_thread);
    
    return TM_ERROR_NONE;
}

/**
 * Waits till the <code>thread</code> is finished.
 *
 * @param[in] java_thread a thread to wait for
 * @return TM_THREAD_TIMEOUT or TM_THREAD_INTERRUPTED or 0 in case thread
 * was successfully joined.
 * @sa java.lang.Thread.join()
 */
IDATA jthread_join(jthread java_thread) {
    IDATA status;
    hythread_t  tm_native_thread;
    
    if (java_thread == NULL) {
        return TM_ERROR_NULL_POINTER;
    }
    tm_native_thread = jthread_get_native_thread(java_thread); 
    status = hythread_join_interruptable(tm_native_thread, 0, 0);
    TRACE(("TM: jthread %d joined %d", hythread_self()->thread_id, tm_native_thread->thread_id));
    
    return status;
}

/**
 * Waits till the <code>thread</code> is finished with specific timeout.
 *
 * @param[in] java_thread a thread to wait for
 * @param[in] millis timeout in milliseconds to wait
 * @param[in] nanos timeout in nanoseconds to wait
 * @return TM_THREAD_TIMEOUT or TM_THREAD_INTERRUPTED or 0 in case thread
 * was successfully joined.
 * @sa java.lang.Thread.join()
 */
IDATA jthread_timed_join(jthread java_thread, jlong millis, jint nanos) {
        
    hythread_t  tm_native_thread;
    IDATA status;

    if (java_thread == NULL) {
        return TM_ERROR_NULL_POINTER;
    }
    tm_native_thread = jthread_get_native_thread(java_thread);
    if (!tm_native_thread) {
            return TM_ERROR_NONE;
    }
    status = hythread_join_interruptable(tm_native_thread, millis, nanos);
    TRACE(("TM: jthread %d joined %d", hythread_self()->thread_id, tm_native_thread->thread_id));

    return status;
}

/**
 * Lets an another thread to pass.
 * @sa java.lang.Thread.yield()
 */
IDATA jthread_yield() {
    hythread_yield();
    return TM_ERROR_NONE;
}

/*
 * Callback which is executed in the target thread at safe point 
 * whenever Thread.stop() method is called.
 */
void stop_callback(void) {  
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    jobject excn;
    
    tm_native_thread = hythread_self();
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    excn = tm_java_thread->stop_exception;
    
    jthread_throw_exception_object(excn);
}

/**
 * Stops the execution of the given <code>thread</code> and forces
 * ThreadDeath exception to be thrown in it.
 *
 * @param[in] java_thread thread to be stopped
 * @sa java.lang.Thread.stop()
 */
IDATA jthread_stop(jthread java_thread) {
    jclass clazz;
    jmethodID excn_constr;
    jobject excen_obj;
    JNIEnv *env;
    jvmti_thread_t tm_java_thread; 
    hythread_t tm_native_thread;

    tm_native_thread = vm_jthread_get_tm_data(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    env = tm_java_thread->jenv;
    clazz = (*env)->FindClass(env, "java/lang/ThreadDeath");
    excn_constr = (*env)->GetMethodID(env, clazz, "<init>", "()V");
    excen_obj = (*env)->NewObject(env, clazz, excn_constr);
    
    return jthread_exception_stop(java_thread, excen_obj);
}

/**
 * Stops the execution of the given <code>thread</code> and forces
 * the <code>throwable</code> exception to be thrown in it.
 *
 * @param[in] java_thread thread to be stopped
 * @param[in] excn exception to be thrown
 * @sa java.lang.Thread.stop()
 */
IDATA jthread_exception_stop(jthread java_thread, jobject excn) {

    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;
    JNIEnv* env;
    IDATA res;

    tm_native_thread = jthread_get_native_thread(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
        
    // Install safepoint callback that would throw exception
    env = jthread_get_JNI_env(jthread_self());
    tm_java_thread->stop_exception = (*env)->NewGlobalRef(env,excn);
    
    res = hythread_set_safepoint_callback(tm_native_thread, stop_callback);

    while (tm_native_thread->suspend_count > 0) {
        apr_atomic_dec32(&tm_native_thread->suspend_count);
        apr_atomic_dec32(&tm_native_thread->request);
    }

    // if there is no competition, it would be 1, but if someone else is
    // suspending the same thread simultaneously, it could be greater than 1
    // if safepoint callback isn't set it could be equal to 0.
    //
    // The following assertion may be false because at each time
    // one of the conditions is true, and the other is false, but
    // when checking the whole condition it may be failse in the result.
    // assert(tm_native_thread->request > 0 || tm_native_thread->safepoint_callback == NULL);

    // notify the thread that it may wake up now,
    // so that it would eventually reach exception safepoint
    // and execute callback
    hysem_post(tm_native_thread->resume_event);

    return res;
}

/**
 * Causes the current <code>thread</code> to sleep for at least the specified time.
 * This call doesn't clear the interrupted flag.
 *
 * @param[in] millis timeout in milliseconds
 * @param[in] nanos timeout in nanoseconds
 *
 * @return  returns 0 on success or negative value on failure,
 * or TM_THREAD_INTERRUPTED in case thread was interrupted during sleep.
 * @sa java.lang.Thread.sleep()
 */
IDATA jthread_sleep(jlong millis, jint nanos) {

    hythread_t tm_native_thread = hythread_self();
    IDATA status;

    tm_native_thread->state &= ~TM_THREAD_STATE_RUNNABLE;
    tm_native_thread->state |= TM_THREAD_STATE_WAITING | TM_THREAD_STATE_SLEEPING |
        TM_THREAD_STATE_WAITING_WITH_TIMEOUT;

    status = hythread_sleep_interruptable(millis, nanos); 
#ifndef NDEBUG
    if (status == TM_ERROR_INTERRUPT) {
        TRACE(("TM: sleep interrupted status received, thread: %p", hythread_self()));
    }
#endif
    
    tm_native_thread->state &= ~(TM_THREAD_STATE_WAITING | TM_THREAD_STATE_SLEEPING |
        TM_THREAD_STATE_WAITING_WITH_TIMEOUT);

    tm_native_thread->state |= TM_THREAD_STATE_RUNNABLE;
    return status;
}

/**
 * Returns JNI environment associated with the given jthread, or NULL if there is none.
 *
 * The NULL value means the jthread object is not yet associated with native thread,
 * or appropriate native thread has already died and deattached.
 * 
 * @param[in] java_thread java.lang.Thread object
 */
JNIEnv *jthread_get_JNI_env(jthread java_thread) {
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;

    if (java_thread == NULL) {
        return NULL;
    }
    tm_native_thread = jthread_get_native_thread(java_thread);
    if (tm_native_thread == NULL) {
        return NULL;
    }
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    if (tm_java_thread == NULL) {
        return NULL;
    }
    return tm_java_thread->jenv;
}
/**
 * Returns thread Id for the given <code>thread</code>.
 *
 * Thread ID must be unique for all Java threads.
 * Can be reused after thread is finished.
 *
 * @return thread ID
 * @sa java.lang.Thread.getId()
 */
jlong jthread_get_id(jthread java_thread) {

    hythread_t tm_native_thread;
    
    tm_native_thread = jthread_get_native_thread(java_thread);
    assert(tm_native_thread);
    
    return hythread_get_id(tm_native_thread);
}

/**
 * Returns jthread given the thread ID.
 *
 * @param[in] thread_id thread ID
 * @return jthread for the given ID, or NULL if there are no such.
 */
jthread jthread_get_thread(jlong thread_id) {
    
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    jthread java_thread;
    
    tm_native_thread = hythread_get_thread((jint)thread_id);
    if (tm_native_thread == NULL) {
        return NULL;
    }
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    java_thread = tm_java_thread->thread_object;
    assert(java_thread);
    return java_thread;
}

/**
 * Returns native thread associated with the given Java <code>thread</code>.
 *
 * @return native thread
 */
hythread_t  jthread_get_native_thread(jthread thread) {
    
    assert(thread);
    return vm_jthread_get_tm_data(thread);        
}

/**
 * Returns Java thread associated with the given native <code>thread</code>.
 *
 * @return Java thread
 */
jthread jthread_get_java_thread(hythread_t tm_native_thread) {
    
    jvmti_thread_t tm_java_thread;

    if (tm_native_thread == NULL) {
        TRACE(("TM: native thread is NULL"));
        return NULL;
    }
    tm_java_thread = hythread_get_private_data(tm_native_thread);

    if (tm_java_thread == NULL) {
        TRACE(("TM: tmj thread is NULL"));
        return NULL;
    }

    return tm_java_thread->thread_object;
}
/**
 * Returns jthread associated with the current thread.
 *
 * @return jthread associated with the current thread, 
 * or NULL if the current native thread is not attached to JVM.
 */
jthread jthread_self(void) {
    return jthread_get_java_thread(hythread_self());
}

/**
 * Cancels all java threads. This method being used at VM shutdown
 * to terminate all java threads.
 */ 
IDATA jthread_cancel_all() {
    return hythread_cancel_all(NULL);
}

/**
 * waiting all nondaemon thread's
 * 
 */
IDATA VMCALL jthread_wait_for_all_nondaemon_threads() {
    hythread_t native_thread;
    jvmti_thread_t jvmti_thread;
    hythread_library_t lib;
    IDATA status;
    
    native_thread = hythread_self();
    jvmti_thread = hythread_get_private_data(native_thread);
    lib = native_thread->library;

    status = hymutex_lock(&lib->TM_LOCK);
    if (status != TM_ERROR_NONE) return status;    

    if (lib->nondaemon_thread_count == 1 && !jvmti_thread->daemon) {
        status = hymutex_unlock(&lib->TM_LOCK);
        return status;
    }

    while ((!jvmti_thread->daemon && lib->nondaemon_thread_count > 1)
            || (jvmti_thread->daemon && lib->nondaemon_thread_count > 0)) {
        status = hycond_wait(&lib->nondaemon_thread_cond, &lib->TM_LOCK);
        //check interruption and other problems
        TRACE(("TM wait for nondaemons notified, count: %d", lib->nondaemon_thread_count));
        if (status != TM_ERROR_NONE) {
            hymutex_unlock(&lib->TM_LOCK);
            return status;
        }
    }
    
    status = hymutex_unlock(&lib->TM_LOCK);
    return status;
}

/*
 *  Auxiliary function to throw java.lang.InterruptedException
 */

void throw_interrupted_exception(void) {

    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;
    jclass clazz;
    JNIEnv *env;
    
    TRACE(("interrupted_exception thrown"));
    tm_native_thread = hythread_self();
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    env = tm_java_thread->jenv;
    clazz = (*env) -> FindClass(env, "java/lang/InterruptedException");
    (*env) -> ThrowNew(env, clazz, "Park() is interrupted");
}

jmethodID getRunMethod(JNIEnv *env, jthread java_thread) {
    jclass clazz;
    static jmethodID run_method = NULL;
    IDATA status;
    
    status=acquire_start_lock();
    assert(status == TM_ERROR_NONE);
    //printf("run method find enter\n");
    if (!run_method) {
        clazz = (*env) -> GetObjectClass(env, java_thread);
        run_method = (*env) -> GetMethodID(env, clazz, "runImpl", "()V");
    }
    status=release_start_lock();
    //printf("run method find exit\n");
    assert(status == TM_ERROR_NONE);
    return run_method;
}

IDATA increase_nondaemon_threads_count(hythread_t self) {
    hythread_library_t lib;
    IDATA status;
    
    lib = self->library;

    status = hymutex_lock(&lib->TM_LOCK);
    if (status != TM_ERROR_NONE) return status;
    
    lib->nondaemon_thread_count++;
    status = hymutex_unlock(&lib->TM_LOCK);
    return status;
}

IDATA countdown_nondaemon_threads(hythread_t self) {
    hythread_library_t lib;
    IDATA status;

    lib = self->library;
    
    status = hymutex_lock(&lib->TM_LOCK);
    if (status != TM_ERROR_NONE) return status;
    
    if (lib->nondaemon_thread_count <= 0) {
        status = hymutex_unlock(&lib->TM_LOCK);
        if (status != TM_ERROR_NONE) return status;
        return TM_ERROR_ILLEGAL_STATE;
    }
    
    TRACE(("TM: nondaemons decreased, thread: %p count: %d\n", self, lib->nondaemon_thread_count));
    lib->nondaemon_thread_count--;
    if (lib->nondaemon_thread_count <= 1) {
        status = hycond_notify_all(&lib->nondaemon_thread_cond); 
        TRACE(("TM: nondaemons all dead, thread: %p count: %d\n", self, lib->nondaemon_thread_count));
        if (status != TM_ERROR_NONE) {
            hymutex_unlock(&lib->TM_LOCK);
            return status;
        }
    }
    
    status = hymutex_unlock(&lib->TM_LOCK);
    return status;
}
