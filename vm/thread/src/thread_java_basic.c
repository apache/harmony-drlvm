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
 * @author Nikolay Kuznetsov
 * @version $Revision: 1.1.2.21 $
 */  

/**
 * @file thread_java_basic.c
 * @brief Key threading operations like thread creation and pointer conversion.
 */

#include <open/jthread.h>
#include <open/hythread_ext.h>
#include "thread_private.h"
#include "jni.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.java"

void stop_callback(void);
jmethodID getRunMethod(JNIEnv *env);

typedef struct  {
    JNIEnv *jenv;
    jthread thread;
    jboolean daemon;
        jvmtiEnv *tiEnv;
        jvmtiStartFunction tiProc;
        void *tiProcArgs;
} wrapper_proc_data;


IDATA associate_native_and_java_thread(JNIEnv* env, jthread java_thread, hythread_t  tm_native_thread, jobject thread_ref);

/**
 * Creates new Java thread.
 *
 * The newly created thread will immediately start to execute the <code>run()</code>
 * method of the appropriate <code>thread</code> object.
 *
 * @param[in] env JNI environment that will be associated with the created Java thread
 * @param[in] java_thread Java thread object with which new thread must be associated.
 * @param[in] attrs thread attributes.
 * @sa java.lang.Thread.run()
 */
IDATA jthread_create(JNIEnv* env, jthread java_thread, jthread_threadattr_t *attrs) {

        IDATA status;
        status = jthread_create_with_function(env,java_thread,attrs,NULL,NULL);
        return status;
}

int wrapper_proc(void *arg) {

    IDATA status,status1;
    wrapper_proc_data *data = (wrapper_proc_data *)arg;
    JNIEnv *env = data->jenv;
    jvmtiEnv *tiEnv = data->tiEnv;
    jvmtiStartFunction tiProc  = data->tiProc;
    void *tiProcArgs =  data->tiProcArgs;

    TRACE(("TM: Java thread started: id=%d OS_handle=%p", hythread_self()->thread_id, apr_os_thread_current()));
    //status = hythread_global_lock();
    //assert (status == TM_ERROR_NONE);
    status=vm_attach();
    if(status!=TM_ERROR_NONE)
    {
        if (!data->daemon){
            status1 = countdown_nondaemon_threads();
            assert (status1 == TM_ERROR_NONE);
        }
        return status;
    }
    jvmti_send_thread_start_end_event(1);
    //status = hythread_global_unlock();
    //  assert (status == TM_ERROR_NONE);
        if(tiProc!=NULL)
        {
                tiProc(tiEnv, env, tiProcArgs);
        }
        else
        {
                (*env) -> CallVoidMethodA(env, data->thread, getRunMethod(env), NULL);//for jthread_create();
        }

        jvmti_send_thread_start_end_event(0);
    (*env) -> DeleteGlobalRef(env, data->thread);
    TRACE(("TM: Java thread finished: id=%d OS_handle=%p", hythread_self()->thread_id, apr_os_thread_current()));
        assert(hythread_is_suspend_enabled());
    status=vm_detach();
    if(status!=TM_ERROR_NONE)
    {
        if (!data->daemon){
            status1 = countdown_nondaemon_threads();
            assert (status1 == TM_ERROR_NONE);
        }
        return status;
    }
    assert(hythread_is_suspend_enabled());
        if (!data->daemon){
                status = countdown_nondaemon_threads();
                assert (status == TM_ERROR_NONE);
        }
    return TM_ERROR_NONE;
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
 * @param[in] env JNI environment that will be associated with the created Java thread
 * @param[in] java_thread Java thread object with which new thread must be associated.
 * @param[in] attrs thread attributes.
 * @param[in] proc the start function to be executed in this thread.
 * @param[in] arg The argument to the start function. Is passed as an array.
 * @sa JVMTI::RunAgentThread()
 */
IDATA jthread_create_with_function(JNIEnv *env, jthread java_thread, jthread_threadattr_t *attrs,jvmtiStartFunction proc, const void* arg)
{
        hythread_t tm_native_thread = NULL;
    jvmti_thread_t tm_java_thread = NULL;
    wrapper_proc_data *data;
        IDATA status;
        apr_status_t apr_status;
        apr_pool_t * pool;

        if (env == NULL || java_thread == NULL || attrs == NULL){
            return TM_ERROR_NULL_POINTER;
        }
    tm_native_thread = vm_jthread_get_tm_data(java_thread);
        
    //This is for irregular use. In ordinary live valid jthread instance
    //contains weak reference associated with it and native thread to reuse 
    //if any
    ////

    if (tm_native_thread==NULL)
        {
       if(!jthread_thread_init(NULL,env,java_thread, NULL, 0))
		 {
			 return TM_ERROR_OUT_OF_MEMORY;
		 }
                tm_native_thread = vm_jthread_get_tm_data(java_thread);
        }
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    assert(tm_java_thread);    
    apr_status = apr_pool_create(&pool, 0);
	if (apr_status != APR_SUCCESS) {
		return CONVERT_ERROR(apr_status);
	}
    data = apr_palloc(pool, sizeof(wrapper_proc_data));
        if(data == NULL) {
                return TM_ERROR_OUT_OF_MEMORY;
        }

    // Prepare argumets for wrapper proc
    data->jenv = env;
    data->thread = tm_java_thread->thread_object; 
    data->daemon = attrs->daemon;
        data->tiEnv  = attrs->jvmti_env;
    data->tiProc = proc;
    data->tiProcArgs = (void *)arg;
    
    // create native thread with wrapper_proc
        if (!attrs->daemon){
         status = increase_nondaemon_threads_count();
                 if (status != TM_ERROR_NONE) return status;
        }
        status = hythread_create(&tm_native_thread, (attrs->stacksize)?attrs->stacksize:1024000,
                              attrs->priority, 0, wrapper_proc, data);
    if ((!attrs->daemon)&&(status != TM_ERROR_NONE)){
        countdown_nondaemon_threads();
    }
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
 * @param[in] env JNI environment that will be associated with the attached Java thread
 * @param[in] java_thread Java thread object with which the current native thread must be associated.
 * @sa JNI::AttachCurrentThread ()
 */
IDATA jthread_attach(JNIEnv* env, jthread java_thread) {        

    hythread_t tm_native_thread;
    IDATA status;
        status = hythread_attach(NULL);
    if (status != TM_ERROR_NONE){
        return status;
    }
        tm_native_thread = hythread_self();
        //I wonder if we need it.
    //since jthread already created, thus association is already done,
    //see jthread_init
    //VVVVVVVVVVVVVVVVVVVVVV
    status=associate_native_and_java_thread(env, java_thread, tm_native_thread, NULL);
    if (status != TM_ERROR_NONE){
        return status;
    }
    TRACE(("TM: Current thread attached to jthread=%p", java_thread));
    status=vm_attach();
        return status;
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
    hythread_t     tm_native_thread = NULL;
    jvmti_thread_t tmj_thread;
        IDATA status;

    if (old_thread) {
        tm_native_thread = (hythread_t)((IDATA)old_thread);
        tmj_thread = (jvmti_thread_t)hythread_get_private_data(tm_native_thread);
        //delete used weak reference
        if (tmj_thread->thread_ref) (*env)->DeleteGlobalRef(env, tmj_thread->thread_ref);
        
    }
        status = hythread_struct_init(&tm_native_thread, NULL);
        if(status != TM_ERROR_NONE)
		{
			return 0;
		}
        status=associate_native_and_java_thread(env, java_thread, tm_native_thread, weak_ref);
        if(status != TM_ERROR_NONE)
		{
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
    jvmti_thread_t tm_java_thread;
    hythread_t tm_native_thread;
     
        // Check input arg
    assert(java_thread);
        TRACE(("TM: jthread_detach %x", hythread_self()));

        tm_native_thread = vm_jthread_get_tm_data(java_thread);
        tm_java_thread = hythread_get_private_data(tm_native_thread);

        // Remove tm_thread_t pointer from java.lang.Thread object
    vm_jthread_set_tm_data(java_thread, NULL);
 
        vm_detach();     

    // Deallocate tm_java_thread 
    apr_pool_destroy(tm_java_thread->pool);

    // Remove tm_jthread_t pointer from *tm_native_thread      
    /*
    status = hythread_set_private_data(tm_native_thread, NULL);
    if (status != TM_ERROR_NONE){
        return status;
    }*/
        assert(hythread_is_suspend_enabled());
    return TM_ERROR_NONE;    
}

IDATA associate_native_and_java_thread(JNIEnv* env, jthread java_thread, hythread_t tm_native_thread, jobject thread_ref)
{
        IDATA status;
    apr_status_t apr_status;
    apr_pool_t *pool;
        jvmti_thread_t tm_java_thread;
        if ((env == NULL) || (java_thread == NULL)||(tm_native_thread==NULL)){
        return TM_ERROR_NULL_POINTER;
    }
    

    tm_java_thread = hythread_get_private_data(tm_native_thread);
    if (!tm_java_thread) {
        apr_status = apr_pool_create(&pool, 0);
            if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
        if (pool==NULL) return TM_ERROR_OUT_OF_MEMORY;
        tm_java_thread = apr_palloc(pool, sizeof(JVMTIThread));
            if(tm_java_thread == NULL) {
                    return TM_ERROR_OUT_OF_MEMORY;
            }
    
        tm_java_thread->pool = pool;

        status = hythread_set_private_data(tm_native_thread, tm_java_thread);
        if (status != TM_ERROR_NONE){
            return status;
        }
    }
    
    tm_java_thread->jenv = env;
    tm_java_thread->thread_object = (*env)->NewGlobalRef(env,java_thread);
    tm_java_thread->thread_ref    = (thread_ref)?(*env)->NewGlobalRef(env, thread_ref):NULL; 
    tm_java_thread->contended_monitor = 0;
    tm_java_thread->wait_monitor = 0;
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

    hythread_t  tm_native_thread;
        IDATA status;

        if (java_thread == NULL){
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

        if (java_thread == NULL){
            return TM_ERROR_NULL_POINTER;
        }
    tm_native_thread = jthread_get_native_thread(java_thread);
        if(!tm_native_thread) {
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
IDATA jthread_yield(){
    hythread_yield();
    return TM_ERROR_NONE;
}

/*
 * Callback which is executed in the target thread at safe point 
 * whenever Thread.stop() method is called.
 */
void stop_callback() {  
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    jobject excn;
    
    tm_native_thread = hythread_self();
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    excn = tm_java_thread->stop_exception;
    tm_native_thread->suspend_request = 0;
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

    tm_native_thread = jthread_get_native_thread(java_thread);
    tm_java_thread = hythread_get_private_data(tm_native_thread);
        
    // Install safepoint callback that would throw exception
    env = tm_java_thread->jenv;
    tm_java_thread->stop_exception = (*env)->NewGlobalRef(env,excn);
    
    return hythread_set_safepoint_callback(tm_native_thread, stop_callback);
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

        tm_native_thread->state &= ~(TM_THREAD_STATE_WAITING | TM_THREAD_STATE_SLEEPING |
                                     TM_THREAD_STATE_WAITING_WITH_TIMEOUT);
    tm_native_thread->state |= TM_THREAD_STATE_RUNNABLE;
        if (status == TM_ERROR_INTERRUPT) {
        TRACE(("TM: sleep interrupted status received, thread: %p", hythread_self()));
    }    
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

    if (java_thread == NULL){
        return NULL;
        }
        tm_native_thread = jthread_get_native_thread(java_thread);
    if (tm_native_thread == NULL){
        return NULL;
        }
        tm_java_thread = hythread_get_private_data(tm_native_thread);
    if (tm_java_thread == NULL){
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
    if (tm_native_thread == NULL){
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

    if (tm_native_thread == NULL){
        TRACE(("TM: native thread is NULL"));
        return NULL;
        }
        tm_java_thread = hythread_get_private_data(tm_native_thread);

    if (tm_java_thread == NULL){
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

/*
 *  Auxiliary function to throw java.lang.InterruptedException
 */

void throw_interrupted_exception(void){

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

jmethodID getRunMethod(JNIEnv *env) {
    jclass clazz;
    static jmethodID run_method = NULL;
        IDATA status;
    
    status=acquire_start_lock();
        assert (status == TM_ERROR_NONE);
    //printf("run method find enter\n");
    if (!run_method) {
        clazz = (*env) -> FindClass(env, "java/lang/Thread");
        run_method = (*env) -> GetMethodID(env, clazz, "runImpl", "()V");
    }
    status=release_start_lock();
    //printf("run method find exit\n");
    assert (status == TM_ERROR_NONE);
    return run_method;
}
