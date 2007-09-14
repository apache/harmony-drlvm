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
#include "open/thread_externals.h"
#include "vm_threads.h"
#include "jni.h"

#define LOG_DOMAIN "tm.java"
#include "cxxlog.h"

static jmethodID jthread_get_run_method(JNIEnv * env, jthread java_thread);
static jmethodID jthread_get_set_alive_method(JNIEnv * env, jthread java_thread);
static IDATA jthread_associate_native_and_java_thread(JNIEnv * env,
    jthread java_thread, hythread_t native_thread, jobject weak_ref);

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
IDATA jthread_create(JNIEnv *jni_env,
                     jthread java_thread,
                     jthread_threadattr_t *attrs)
{
    return jthread_create_with_function(jni_env, java_thread, attrs);
} // jthread_create

static IDATA HYTHREAD_PROC jthread_wrapper_proc(void *arg)
{
    assert(arg);
    jthread_threadattr_t data = *(jthread_threadattr_t*) arg;
    STD_FREE(arg);

    // Association should be already done.
    hythread_t native_thread = hythread_self();
    assert(native_thread);
    vm_thread_t vm_thread = jthread_get_vm_thread_unsafe(native_thread);
    assert(vm_thread);
    jobject java_thread = vm_thread->java_thread;

    JNIEnv *jni_env;
    IDATA status = vm_attach(data.java_vm, &jni_env);
    assert(status == JNI_OK);

    vm_thread->jni_env = jni_env;
    vm_thread->daemon = data.daemon;

    TRACE(("TM: Java thread started: id=%d OS_handle=%p",
           hythread_get_id(native_thread), apr_os_thread_current()));

    if (!vm_thread->daemon) {
        status = hythread_increase_nondaemon_threads_count(native_thread);
        assert(status == TM_ERROR_NONE);
    }

    // Send Thread Start event.
    assert(hythread_is_alive(native_thread));
    jni_env->CallVoidMethod(java_thread,
        jthread_get_set_alive_method(jni_env, java_thread), true);
    jvmti_send_thread_start_end_event(1);
    jthread_start_count();

    if (data.proc != NULL) {
        data.proc(data.jvmti_env, jni_env, (void*)data.arg);
    } else {
        // for jthread_create();
        jni_env->CallVoidMethodA(java_thread,
            jthread_get_run_method(jni_env, java_thread), NULL);
    }

    status = jthread_detach(java_thread);

    TRACE(("TM: Java thread finished: id=%d OS_handle=%p",
           hythread_get_id(native_thread), apr_os_thread_current()));

    return status;
} // jthread_wrapper_proc

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
IDATA jthread_create_with_function(JNIEnv *jni_env,
                                   jthread java_thread,
                                   jthread_threadattr_t *given_attrs)
{
    if (jni_env == NULL || java_thread == NULL || given_attrs == NULL) {
        return TM_ERROR_NULL_POINTER;
    }
    hythread_t native_thread = vm_jthread_get_tm_data(java_thread);
    assert(native_thread);

    vm_thread_t vm_thread = jthread_get_vm_thread_unsafe(native_thread);
    assert(vm_thread);
    vm_thread->java_thread = jni_env->NewGlobalRef(java_thread);

    // prepare args for wrapper_proc
    jthread_threadattr_t *attrs =
        (jthread_threadattr_t*)STD_MALLOC(sizeof(jthread_threadattr_t));
    if (attrs == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
    }
    *attrs = *given_attrs;

    // Get JavaVM 
    IDATA status = jni_env->GetJavaVM(&attrs->java_vm);
    if (status != JNI_OK) {
        return TM_ERROR_INTERNAL;
    }

    static size_t default_stacksize;
    if (0 == default_stacksize) {
        size_t stack_size = get_size_property("thread.stacksize", 0, VM_PROPERTIES);
        default_stacksize = stack_size ? stack_size : TM_DEFAULT_STACKSIZE;
    }

    if (!attrs->stacksize) {
        attrs->stacksize = default_stacksize;
    }

    status = hythread_create_ex(native_thread, NULL, attrs->stacksize,
                        attrs->priority, jthread_wrapper_proc, attrs);

    TRACE(("TM: Created thread: id=%d", hythread_get_id(native_thread)));

    return status;
} // jthread_create_with_function

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
IDATA jthread_attach(JNIEnv *jni_env, jthread java_thread, jboolean daemon)
{
    if (jthread_self() != NULL) {
        // Do nothing if thread already attached.
        return TM_ERROR_NONE;
    }

    hythread_t native_thread = hythread_self();
    assert(native_thread);
    IDATA status = jthread_associate_native_and_java_thread(jni_env, java_thread,
                        native_thread, NULL);
    if (status != TM_ERROR_NONE) {
        return status;
    }

    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    assert(vm_thread);
    vm_thread->java_thread = jni_env->NewGlobalRef(java_thread);
    vm_thread->jni_env = jni_env;
    vm_thread->daemon = daemon;
    if (!daemon) {
        status = hythread_increase_nondaemon_threads_count(native_thread);
        assert(status == TM_ERROR_NONE);
    }

    // Send Thread Start event.
    assert(hythread_is_alive(native_thread));
    jvmti_send_thread_start_end_event(1);
    jthread_start_count();

    TRACE(("TM: Current thread attached to jthread=%p", java_thread));
    return TM_ERROR_NONE;
} // jthread_attach

/**
 * Associates the Java thread with the native thread.
 *
 * @param[in] env JNI environment that will be associated with the created Java thread
 * @param[in] java_thread the Java thread for the association
 * @param[in] weak_ref java.lang.WeakReference to the <code>java_thread</code> that used for native resource deletion
 * @param[in] dead_thread the native thread for the association
 * @return the native thread
 */
jlong jthread_thread_init(JNIEnv *env,
                          jthread java_thread,
                          jobject weak_ref,
                          hythread_t dead_thread)
{
    hythread_t native_thread = NULL;
    if (dead_thread) {
        native_thread = (hythread_t)((IDATA) dead_thread);
        vm_thread_t vm_thread = jthread_get_vm_thread_unsafe(native_thread);
        assert(vm_thread);
        if (vm_thread->weak_ref) {
            // delete used weak reference
            env->DeleteGlobalRef(vm_thread->weak_ref);
        }
    } else {
        native_thread = (hythread_t)jthread_allocate_thread();
    }
    
    IDATA status = hythread_struct_init(native_thread);
    if (status != TM_ERROR_NONE) {
        return 0;
    }

    status = jthread_associate_native_and_java_thread(env, java_thread,
                    native_thread, weak_ref);
    if (status != TM_ERROR_NONE) {
        return 0;
    }
    return (jlong)((IDATA)native_thread);
} // jthread_thread_init

/**
 * Detaches the selected thread from java VM.
 *
 * This function will release any resources associated with the given thread.
 *
 * @param[in] java_thread Java thread to be detached
 */
IDATA jthread_detach(jthread java_thread)
{
    // Check input arg
    assert(java_thread);
    assert(hythread_is_suspend_enabled());

    TRACE(("TM: jthread_detach %x", hythread_self()));

    hythread_t native_thread = jthread_get_native_thread(java_thread);
    assert(native_thread);
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    assert(vm_thread);
    JNIEnv *jni_env = vm_thread->jni_env;

    IDATA status;
    if (!vm_thread->daemon) {
        status = hythread_decrease_nondaemon_threads_count(native_thread, 1);
        assert(status == TM_ERROR_NONE);
    }

    // Detach from VM.
    status = vm_detach(java_thread);
    if (status != JNI_OK) {
        return TM_ERROR_INTERNAL;
    }

    // Delete global reference to current thread object.
    jni_env->DeleteGlobalRef(vm_thread->java_thread);
    // jthread_self() will return NULL now.
    vm_thread->java_thread = NULL;

    // Decrease alive thread counter
    jthread_end_count();
    assert(hythread_is_suspend_enabled());

    return TM_ERROR_NONE;
} // jthread_detach

static IDATA
jthread_associate_native_and_java_thread(JNIEnv * jni_env,
                                         jthread java_thread,
                                         hythread_t native_thread,
                                         jobject weak_ref)
{
    if ((jni_env == NULL) || (java_thread == NULL)
        || (native_thread == NULL))
    {
        return TM_ERROR_NULL_POINTER;
    }

    vm_thread_t vm_thread = jthread_get_vm_thread_unsafe(native_thread);
    assert(vm_thread);

    vm_thread->weak_ref = 
        (weak_ref) ? jni_env->NewGlobalRef(weak_ref) : NULL;

    // Associate java thread with native thread      
    vm_jthread_set_tm_data(java_thread, native_thread);

    return TM_ERROR_NONE;
} // jthread_associate_native_and_java_thread

/**
 * Lets an another thread to pass.
 * @sa java.lang.Thread.yield()
 */
IDATA jthread_yield()
{
    hythread_yield();
    return TM_ERROR_NONE;
} // jthread_yield

/*
 * Callback which is executed in the target thread at safe point 
 * whenever Thread.stop() method is called.
 */
static void stop_callback(void)
{
    hythread_t native_thread = hythread_self();
    assert(native_thread);
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    assert(vm_thread);
    jobject excn = vm_thread->stop_exception;

    // Does not return if the exception could be thrown straight away
    jthread_throw_exception_object(excn);

    // getting here means top stack frame is non-unwindable.
    if (hythread_get_state(native_thread) &
            (TM_THREAD_STATE_SLEEPING | TM_THREAD_STATE_WAITING_WITH_TIMEOUT
                | TM_THREAD_STATE_WAITING | TM_THREAD_STATE_IN_MONITOR_WAIT
                | TM_THREAD_STATE_WAITING_INDEFINITELY | TM_THREAD_STATE_PARKED))
    {
        // This is needed for correct stopping of a thread blocked on monitor_wait.
        // The thread needs some flag to exit its waiting loop.
        // We piggy-back on interrupted status. A correct exception from TLS
        // will be thrown because the check of exception status on leaving
        // JNI frame comes before checking return status in Object.wait().
        // Interrupted status will be cleared by function returning TM_ERROR_INTERRUPT.
        // (though, in case of parked thread, it will not be cleared)
        hythread_interrupt(native_thread);
    }
} // stop_callback

/**
 * Stops the execution of the given <code>thread</code> and forces
 * ThreadDeath exception to be thrown in it.
 *
 * @param[in] java_thread thread to be stopped
 * @sa java.lang.Thread.stop()
 */
IDATA jthread_stop(jthread java_thread)
{
    assert(java_thread);
    hythread_t native_thread = vm_jthread_get_tm_data(java_thread);
    assert(native_thread);
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    assert(vm_thread);
    JNIEnv *env = vm_thread->jni_env;
    assert(env);
    jclass clazz = env->FindClass("java/lang/ThreadDeath");
    jmethodID excn_constr = env->GetMethodID(clazz, "<init>", "()V");
    jobject excen_obj = env->NewObject(clazz, excn_constr);

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
IDATA jthread_exception_stop(jthread java_thread, jobject excn)
{
    assert(java_thread);
    hythread_t native_thread = vm_jthread_get_tm_data(java_thread);
    assert(native_thread);
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    assert(vm_thread);

    // Install safepoint callback that would throw exception
    JNIEnv *env = vm_thread->jni_env;
    assert(env);
    vm_thread->stop_exception = env->NewGlobalRef(excn);

    return hythread_set_thread_stop_callback(native_thread, stop_callback);
} // jthread_exception_stop

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
IDATA jthread_sleep(jlong millis, jint nanos)
{
    hythread_t native_thread = hythread_self();
    hythread_thread_lock(native_thread);
    IDATA state = hythread_get_state(native_thread);
    state &= ~TM_THREAD_STATE_RUNNABLE;
    state |= TM_THREAD_STATE_WAITING | TM_THREAD_STATE_SLEEPING
                | TM_THREAD_STATE_WAITING_WITH_TIMEOUT;
    IDATA status = hythread_set_state(native_thread, state);
    assert(status == TM_ERROR_NONE);
    hythread_thread_unlock(native_thread);

    status = hythread_sleep_interruptable(millis, nanos);
#ifndef NDEBUG
    if (status == TM_ERROR_INTERRUPT) {
        TRACE(("TM: sleep interrupted status received, thread: %p",
               hythread_self()));
    }
#endif

    hythread_thread_lock(native_thread);
    state = hythread_get_state(native_thread);
    state &= ~(TM_THREAD_STATE_WAITING | TM_THREAD_STATE_SLEEPING
                    | TM_THREAD_STATE_WAITING_WITH_TIMEOUT);
    state |= TM_THREAD_STATE_RUNNABLE;
    hythread_set_state(native_thread, state);
    hythread_thread_unlock(native_thread);

    return status;
} // jthread_sleep

/**
 * Returns JNI environment associated with the given jthread, or NULL if there is none.
 *
 * The NULL value means the jthread object is not yet associated with native thread,
 * or appropriate native thread has already died and deattached.
 * 
 * @param[in] java_thread java.lang.Thread object
 */
JNIEnv * jthread_get_JNI_env(jthread java_thread)
{
    if (java_thread == NULL) {
        return NULL;
    }
    hythread_t native_thread = jthread_get_native_thread(java_thread);
    if (native_thread == NULL) {
        return NULL;
    }
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    if (vm_thread == NULL) {
        return NULL;
    }
    return vm_thread->jni_env;
} // jthread_get_JNI_env

/**
 * Returns thread ID for the given <code>thread</code>.
 *
 * Thread ID must be unique for all Java threads.
 * Can be reused after thread is finished.
 *
 * @return thread ID
 * @sa java.lang.Thread.getId()
 */
jlong jthread_get_id(jthread java_thread)
{
    hythread_t native_thread = jthread_get_native_thread(java_thread);
    assert(native_thread);
    return hythread_get_id(native_thread);
} // jthread_get_id

/**
 * Returns jthread given the thread ID.
 *
 * @param[in] thread_id thread ID
 * @return jthread for the given ID, or NULL if there are no such.
 */
jthread jthread_get_thread(jlong thread_id)
{
    hythread_t native_thread = hythread_get_thread((IDATA)thread_id);
    if (native_thread == NULL) {
        return NULL;
    }
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    assert(vm_thread);
    jobject java_thread = vm_thread->java_thread;
    assert(java_thread);
    return java_thread;
} // jthread_get_thread

/**
 * Returns native thread associated with the given Java <code>thread</code>.
 *
 * @return native thread
 */
hythread_t jthread_get_native_thread(jthread thread)
{
    assert(thread);
    return vm_jthread_get_tm_data(thread);
} // jthread_get_native_thread

/**
 * Returns Java thread associated with the given native <code>thread</code>.
 *
 * @return Java thread
 */
jthread jthread_get_java_thread(hythread_t native_thread)
{
    if (native_thread == NULL) {
        TRACE(("TM: native thread is NULL"));
        return NULL;
    }
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    if (vm_thread == NULL) {
        TRACE(("TM: vm_thread_t thread is NULL"));
        return NULL;
    }
    return vm_thread->java_thread;
} // jthread_get_java_thread

/**
 * Returns jthread associated with the current thread.
 *
 * @return jthread associated with the current thread, 
 * or NULL if the current native thread is not attached to JVM.
 */
jthread jthread_self(void)
{
    return jthread_get_java_thread(hythread_self());
} // jthread_self

/**
 * Cancels all java threads. This method being used at VM shutdown
 * to terminate all java threads.
 */
IDATA jthread_cancel_all()
{
    return hythread_cancel_all(NULL);
} // jthread_cancel_all

/**
 * waiting all nondaemon thread's
 * 
 */
IDATA VMCALL jthread_wait_for_all_nondaemon_threads()
{
    hythread_t native_thread = hythread_self();
    assert(native_thread);
    vm_thread_t vm_thread = jthread_get_vm_thread(native_thread);
    return hythread_wait_for_nondaemon_threads(native_thread, 
                                               (vm_thread->daemon ? 0 : 1));
} // jthread_wait_for_all_nondaemon_threads

/*
 *  Auxiliary function to throw java.lang.InterruptedException
 */
void throw_interrupted_exception(void)
{
    TRACE(("interrupted_exception thrown"));
    vm_thread_t vm_thread = p_TLS_vmthread;
    assert(vm_thread);
    JNIEnv *env = vm_thread->jni_env;
    assert(env);
    jclass clazz = env->FindClass("java/lang/InterruptedException");
    env->ThrowNew(clazz, "Park() is interrupted");
} // throw_interrupted_exception

static jmethodID jthread_get_run_method(JNIEnv * env, jthread java_thread)
{
    static jmethodID run_method = NULL;

    TRACE("run method find enter");
    if (!run_method) {
        jclass clazz = env->GetObjectClass(java_thread);
        run_method = env->GetMethodID(clazz, "runImpl", "()V");
    }
    TRACE("run method find exit");
    return run_method;
} // jthread_get_run_method

static jmethodID jthread_get_set_alive_method(JNIEnv * env, jthread java_thread)
{
    static jmethodID set_alive_method = NULL;

    TRACE("run method find enter");
    if (!set_alive_method) {
        jclass clazz = env->GetObjectClass(java_thread);
        set_alive_method = env->GetMethodID(clazz, "setAlive", "(Z)V");
    }
    TRACE("run method find exit");
    return set_alive_method;
} // jthread_get_run_method

