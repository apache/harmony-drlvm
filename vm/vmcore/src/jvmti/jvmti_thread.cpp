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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.5 $
 */  
/*
 * JVMTI thread API
 */

#define LOG_DOMAIN "jvmti.thread"
#include "cxxlog.h"

#include "jvmti_utils.h"
#include "vm_threads.h"
#include "thread_generic.h"
#include "open/thread.h"
#include "thread_manager.h"
#include "object_handles.h"
#include "open/vm_util.h"
#include "platform_lowlevel.h"
#include "mon_enter_exit.h"
#include "mon_enter_exit.h"
#include "interpreter_exports.h"
#include "environment.h"
#include "suspend_checker.h"

#define MAX_JVMTI_ENV_NUMBER 10
// jvmti_local_storage_static is used to store local storage for thread == NULL
static JVMTILocalStorage jvmti_local_storage_static[MAX_JVMTI_ENV_NUMBER];
static JNIEnv * jvmti_test_jenv = jni_native_intf;
extern VM_thread * get_vm_thread_ptr_safe(JNIEnv * env, jobject thread);
jobject get_jobject(VM_thread * thread);

Boolean is_valid_thread_object(jthread thread)
{
    if (NULL == thread)
        return false;

    tmn_suspend_disable();       //---------------------------------v
    ObjectHandle h = (ObjectHandle)thread;
    ManagedObject *mo = h->object;

    // Check that reference pointer points to the heap
    if (mo < (ManagedObject *)Class::heap_base ||
        mo > (ManagedObject *)Class::heap_end)
    {
        tmn_suspend_enable();
        return false;
    }

    // Check that object is an instance of java.lang.Thread or extends it
    if (mo->vt() == NULL)
    {
        tmn_suspend_enable();
        return false;
    }

    Class *object_clss = mo->vt()->clss;
    Class *thread_class = VM_Global_State::loader_env->java_lang_Thread_Class;
    Boolean result = class_is_subtype_fast(object_clss->vtable, thread_class);
    tmn_suspend_enable();        //---------------------------------^

    return result;
}

/*
 * Get Thread State
 *
 * Get the state of a thread.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetThreadState(jvmtiEnv* env,
                    jthread thread,
                    jint* thread_state_ptr)
{
    TRACE2("jvmti.thread", "GetThreadState called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL != thread)
    {
        if (!is_valid_thread_object(thread))
            return JVMTI_ERROR_INVALID_THREAD;
    }
    else
        thread = thread_current_thread();

    if (thread_state_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    int state = thread_get_thread_state(thread);

    if (state == -1)
    {
        return JVMTI_ERROR_THREAD_NOT_ALIVE; // non-existant thread
    }

    * thread_state_ptr = state;

    return JVMTI_ERROR_NONE;
}

/*
 * Get All Threads
 *
 * Get all live threads. The threads are Java programming language
 * threads; that is, threads that are attached to the VM. A thread
 * is live if java.lang.Thread.isAlive() would return true, that
 * is, the thread has been started and has not yet died. The
 * universe of threads is determined by the context of the JVMTI
 * environment, which typically is all threads attached to the VM.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetAllThreads(jvmtiEnv* env,
                   jint* threads_count_ptr,
                   jthread** threads_ptr)
{
    TRACE2("jvmti.thread", "GetAllThreads called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (threads_count_ptr == NULL || threads_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    *threads_count_ptr = thread_get_all_threads(threads_ptr);

    return JVMTI_ERROR_NONE;
}

/*
 * Suspend Thread
 *
 * Suspend the specified thread. If the calling thread is specified,
 * this function will not return until some other thread calls
 * ResumeThread. If the thread is currently suspended, this
 * function does nothing and returns an error.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiSuspendThread(jvmtiEnv* env,
                   jthread thread)
{
    TRACE2("jvmti.thread", "SuspendThread called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_suspend == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (NULL != thread)
    {
        if (!is_valid_thread_object(thread))
            return JVMTI_ERROR_INVALID_THREAD;
    }
    else
        thread = thread_current_thread();

    jint state;
    err = jvmtiGetThreadState(env, thread, &state);

    if (err != JVMTI_ERROR_NONE)
        return err;

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0)
        return JVMTI_ERROR_THREAD_NOT_ALIVE;

    if ((state & JVMTI_THREAD_STATE_SUSPENDED) != 0)
        return JVMTI_ERROR_THREAD_SUSPENDED;

    thread_suspend(thread);

    return JVMTI_ERROR_NONE;
}

/*
 * Suspend Thread List
 *
 * Suspend the request_count threads specified in the request_list
 * array. Threads may be resumed with ResumeThreadList or
 * ResumeThread. If the calling thread is specified in the
 * request_list array, this function will not return until some
 * other thread resumes it. Errors encountered in the suspension
 * of a thread are returned in the results array, not in the
 * return value of this function. Threads that are currently
 * suspended are not suspended.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiSuspendThreadList(jvmtiEnv* env,
                       jint request_count,
                       const jthread* request_list,
                       jvmtiError* results)
{
    TRACE2("jvmti.thread", "SuspendThreadList called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_suspend == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (request_count < 0)
    {
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }

    if (request_list == NULL || results == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    for (int i = 0; i < request_count; i++)
    {
        results[i] = jvmtiSuspendThread(env, request_list[i]);
    }

    return JVMTI_ERROR_NONE;
}

/*
 * Resume Thread
 *
 * Resume a suspended thread. Any threads currently suspended
 * through a JVMTI suspend function (eg. SuspendThread) or
 * java.lang.Thread.suspend() will resume execution; all other
 * threads are unaffected.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiResumeThread(jvmtiEnv* env,
                  jthread thread)
{
    TRACE2("jvmti.thread", "ResumeThread called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = jvmtiGetCapabilities(env, &capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_suspend == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (false)
    { // TBD
        return JVMTI_ERROR_INVALID_TYPESTATE;
    }

    if (NULL == thread)
        return JVMTI_ERROR_INVALID_THREAD;

    jint state;
    // check error condition: JVMTI_ERROR_INVALID_THREAD
    err = jvmtiGetThreadState(env, thread, &state);

    if (err != JVMTI_ERROR_NONE)
        return err;

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0)
        return JVMTI_ERROR_THREAD_NOT_ALIVE;

    if ((state & JVMTI_THREAD_STATE_SUSPENDED) == 0)
        return JVMTI_ERROR_THREAD_NOT_SUSPENDED;

    thread_resume(thread);

    return JVMTI_ERROR_NONE;
}

/*
 * Resume Thread List
 *
 * Resume the request_count threads specified in the request_list
 * array. Any thread suspended through a JVMTI suspend function
 * (eg. SuspendThreadList) or java.lang.Thread.suspend() will
 * resume execution.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiResumeThreadList(jvmtiEnv* env,
                      jint request_count,
                      const jthread* request_list,
                      jvmtiError* results)
{
    TRACE2("jvmti.thread", "ResumeThreadList called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_suspend == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (request_count < 0)
    {
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }

    if (request_list == NULL || results == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    for (int i = 0; i < request_count; i++)
    {
        results[i] = jvmtiResumeThread(env, request_list[i]);
    }

    return JVMTI_ERROR_NONE;
}

/*
 * Stop Thread
 *
 * Send the specified asynchronous exception to the specified
 * thread (similar to java.lang.Thread.stop). Normally, this
 * function is used to kill the specified thread with an instance
 * of the exception ThreadDeath.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiStopThread(jvmtiEnv* env,
                jthread thread,
                jobject UNREF exception)
{
    TRACE2("jvmti.thread", "StopThread called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_signal_thread == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (!is_valid_thread_object(thread))
        return JVMTI_ERROR_INVALID_THREAD;

    thread_stop(thread, NULL);

    return JVMTI_NYI;
}

/*
 * Interrupt Thread
 *
 * Interrupt the specified thread (similar to
 * java.lang.Thread.interrupt).
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiInterruptThread(jvmtiEnv* env,
                     jthread thread)
{
    TRACE2("jvmti.thread", "InterruptThread called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_signal_thread == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (!is_valid_thread_object(thread))
        return JVMTI_ERROR_INVALID_THREAD;

    VM_thread * vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    if (!vm_thread)
    {
        return JVMTI_ERROR_THREAD_NOT_ALIVE; // non-existant thread
    }

    thread_interrupt(thread);

    return JVMTI_ERROR_NONE;
}

/*
 * Get Thread Info
 *
 * Get thread information. The fields of the jvmtiThreadInfo
 * structure are filled in with details of the specified thread.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetThreadInfo(jvmtiEnv* env,
                   jthread thread,
                   jvmtiThreadInfo* info_ptr)
{
    TRACE2("jvmti.thread", "GetThreadInfo called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (info_ptr == NULL)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL != thread)
    {
        if (!is_valid_thread_object(thread))
            return JVMTI_ERROR_INVALID_THREAD;
    }
    else
        thread = thread_current_thread();

    jclass cl = GetObjectClass(jvmti_test_jenv, thread);
    jmethodID id = jvmti_test_jenv -> GetMethodID(cl, "getName","()Ljava/lang/String;");
    jstring  name = jvmti_test_jenv -> CallObjectMethod (thread, id);
    info_ptr -> name = (char *)jvmti_test_jenv -> GetStringUTFChars (name, false);

    id = jvmti_test_jenv -> GetMethodID(cl, "getPriority","()I");
    info_ptr -> priority = jvmti_test_jenv -> CallIntMethod (thread, id);

    id = jvmti_test_jenv -> GetMethodID(cl, "isDaemon","()Z");
    info_ptr -> is_daemon = jvmti_test_jenv -> CallBooleanMethod (thread, id);

    id = jvmti_test_jenv -> GetMethodID(cl, "getThreadGroup","()Ljava/lang/ThreadGroup;");
    info_ptr -> thread_group = jvmti_test_jenv -> CallObjectMethod (thread, id);

    id = jvmti_test_jenv -> GetMethodID(cl, "getContextClassLoader","()Ljava/lang/ClassLoader;");
    info_ptr -> context_class_loader = jvmti_test_jenv -> CallObjectMethod (thread, id);

    return JVMTI_ERROR_NONE;
}

/*
 * Get Owned Monitor Info
 *
 * Get information about the monitors owned by the specified thread.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiGetOwnedMonitorInfo(jvmtiEnv* env,
                         jthread thread,
                         jint* owned_monitor_count_ptr,
                         jobject** owned_monitors_ptr)
{
    TRACE2("jvmti.thread", "GetOwnedMonitorInfo called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_get_owned_monitor_info == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (NULL != thread)
    {
        if (!is_valid_thread_object(thread))
            return JVMTI_ERROR_INVALID_THREAD;
    }
    else
        thread = thread_current_thread();

    VM_thread * vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    if (!vm_thread)
    {
        return JVMTI_ERROR_THREAD_NOT_ALIVE; // non-existant thread
    }

    if (owned_monitor_count_ptr == NULL || owned_monitors_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    int count = thread_get_owned_monitor_info(thread, owned_monitors_ptr);

    if (count == -1){
        return JVMTI_ERROR_NULL_POINTER;
    }

    *owned_monitor_count_ptr = count;

    return JVMTI_ERROR_NONE;
}

/*
 * Get Current Contended Monitor
 *
 * Get the object, if any, whose monitor the specified thread is
 * waiting to enter or waiting to regain through java.lang.Object.wait.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetCurrentContendedMonitor(jvmtiEnv* env,
                                jthread thread,
                                jobject* monitor_ptr)
{
    TRACE2("jvmti.thread", "GetCurrentContendedMonitor called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    jvmtiCapabilities capa;

    jvmtiError err = env -> GetCapabilities(&capa);

    if (err != JVMTI_ERROR_NONE) 
    {
       return err; 
    } 
    if (capa.can_get_current_contended_monitor == 0)
    {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    if (monitor_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    if (NULL == thread)
        thread = thread_current_thread();

    jint state;
    // check error condition: JVMTI_ERROR_INVALID_THREAD
    err = jvmtiGetThreadState(env, thread, &state);

    if (err != JVMTI_ERROR_NONE)
        return err;

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0)
        return JVMTI_ERROR_THREAD_NOT_ALIVE;

    *monitor_ptr = thread_contends_for_lock(thread);

    return JVMTI_ERROR_NONE;
}

/*
 * Run Agent Thread
 *
 * Starts the execution of an agent thread. with the specified
 * native function.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiRunAgentThread(jvmtiEnv* env,
                    jthread thread,
                    jvmtiStartFunction proc,
                    const void* arg,
                    jint priority)
{
    TRACE2("jvmti.thread", "RunAgentThread called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (priority < JVMTI_THREAD_MIN_PRIORITY || priority > JVMTI_THREAD_MAX_PRIORITY)
    {
        return JVMTI_ERROR_INVALID_PRIORITY;
    }

    if (!is_valid_thread_object(thread))
        return JVMTI_ERROR_INVALID_THREAD;

    if (proc == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    // Set daemon flag for the thread
    jclass thread_class = GetObjectClass(jvmti_test_jenv, thread);
    assert(thread_class);
    jmethodID set_daemon = GetMethodID(jvmti_test_jenv, thread_class, "setDaemon", "(Z)V");
    assert(set_daemon);
    CallVoidMethod(jvmti_test_jenv, thread, set_daemon, JNI_TRUE);
    // Run new thread
    Java_java_lang_Thread_start_generic(jvmti_test_jenv, thread, env, proc, arg, priority);

    return JVMTI_ERROR_NONE;
}

/*
 * Set Thread Local Storage
 *
 * The VM stores a pointer value associated with each
 * environment-thread pair. This pointer value is called
 * thread-local storage. This value is NULL unless set with this
 * function. Agents can allocate memory in which they store thread
 * specific information. By setting thread-local storage it can
 * then be accessed with GetThreadLocalStorage.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiSetThreadLocalStorage(jvmtiEnv* env,
                           jthread thread,
                           const void* data)
{
    TRACE2("jvmti.thread", "SetThreadLocalStorage called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL != thread)
    {
        if (!is_valid_thread_object(thread))
            return JVMTI_ERROR_INVALID_THREAD;
    }
    else
        thread = thread_current_thread();

    VM_thread* vm_thread = thread != NULL ? 
        get_vm_thread_ptr_safe(jvmti_test_jenv, thread) : p_TLS_vmthread;

    if (!vm_thread)
    {
        return JVMTI_ERROR_THREAD_NOT_ALIVE; // non-existant thread
    }
    JVMTILocalStorage* aa = NULL;
    JVMTILocalStorage* lstg = &vm_thread -> jvmti_local_storage;
    if (lstg -> env == NULL) {
        if (lstg -> data == NULL) {
            // we have no records stored;
            // so, we put our first record into vm_thread -> jvmti_local_storage
            vm_thread -> jvmti_local_storage.env = (data == NULL) ? NULL : env;
            vm_thread -> jvmti_local_storage.data = (void *)data;
            return JVMTI_ERROR_NONE;
        } else {
            // we have more than one record stored;
            // so, they are stored in array which is pointed at by 
            // vm_thread -> jvmti_local_storage -> data  
            aa = (JVMTILocalStorage*)vm_thread -> jvmti_local_storage.data;
        }
    } else {
        // we have just one record stored;
        // so, it's stored in vm_thread -> jvmti_local_storage 
        if (lstg -> env == env) {
            // override data in this record
            lstg -> data = (void *)data;
            return JVMTI_ERROR_NONE;
        } else if (data != NULL){
            // we have just one record stored and we have to add another one; 
            // so, array is created and record is copied there 
            aa = (JVMTILocalStorage*)STD_MALLOC(sizeof(JVMTILocalStorage)*
                                                           MAX_JVMTI_ENV_NUMBER);
            for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
                aa[0].env = NULL;
                aa[0].data = NULL;
            }
            aa[0].env = vm_thread -> jvmti_local_storage.env;
            aa[0].data = vm_thread -> jvmti_local_storage.data;
            vm_thread -> jvmti_local_storage.env = NULL;
            vm_thread -> jvmti_local_storage.data = (void *) aa;
        }
    }
    // array look up for existing env or for free record
    for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
        if (aa[i].env == env || aa[i].env == NULL ) {
            aa[i].env = (data == NULL) ? NULL : env;
            aa[i].data = (void *)data;
            return JVMTI_ERROR_NONE;
        }
    }
    ASSERT(0, "Array is full");

    return JVMTI_ERROR_NONE;
}

/*
 * Get Thread Local Storage
 *
 * Called by the agent to get the value of the JVMTI thread-local
 * storage.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetThreadLocalStorage(jvmtiEnv* env,
                           jthread thread,
                           void** data_ptr)
{
    TRACE2("jvmti.thread", "GetThreadLocalStorage called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (data_ptr == NULL)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL != thread)
    {
        if (!is_valid_thread_object(thread))
            return JVMTI_ERROR_INVALID_THREAD;
    }
    else
        thread = thread_current_thread();

    *data_ptr = NULL;

    VM_thread* vm_thread = thread != NULL ? 
        get_vm_thread_ptr_safe(jvmti_test_jenv, thread) : p_TLS_vmthread;

    if (!vm_thread)
        return JVMTI_ERROR_THREAD_NOT_ALIVE; // non-existant thread

    JVMTILocalStorage* lstg = &vm_thread -> jvmti_local_storage;
    if (lstg -> env == NULL) {
        if (lstg -> data != NULL) {
            // we have more than one record stored;
            // so, they are stored in array which is pointed at by 
            // vm_thread -> jvmti_local_storage -> data  
            JVMTILocalStorage* aa = (JVMTILocalStorage* )lstg -> data;
            for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
                if (aa[i].env == env) {
                    *data_ptr = aa[i].data;
                    break;
                }
            }
        }
    } else {
        // we have just one record stored;
        // so, it's stored in vm_thread -> jvmti_local_storage 
        if (lstg -> env == env) {
            *data_ptr = lstg -> data;
        }
    }

    return JVMTI_ERROR_NONE;
}

/*
 * -------------------------------------------------------------------------------
 * -------------------------------------------------------------------------------
 */

jobject get_jobject(VM_thread * thread) {
    ObjectHandle hThread = oh_allocate_global_handle();
    tmn_suspend_disable();
    hThread->object = (struct ManagedObject *)thread->p_java_lang_thread;
    tmn_suspend_enable();
    return (jthread)hThread;
}

int thread_get_thread_state(jthread thread) {

    VM_thread * vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    if ( !vm_thread)
    {
        jclass cl =  GetObjectClass(jvmti_test_jenv, thread);
        jfieldID id = jvmti_test_jenv -> GetFieldID(cl, "started", "Z");
        jboolean started  = jvmti_test_jenv -> GetBooleanField(thread, id);
        return started  ? JVMTI_THREAD_STATE_TERMINATED : 0; // 0 - New thread
    }
    return vm_thread -> get_jvmti_thread_state(thread);
}

jint VM_thread::get_jvmti_thread_state(jthread thread){

    // see: thread_is_alive(jobject jThreadObj)
    jint jvmti_thread_state = 0;
    if ( app_status == zip) {
        return JVMTI_ERROR_THREAD_NOT_ALIVE; // non-existant thread
    }
    java_state as = this->app_status;  
    switch (as) {
        case thread_is_sleeping:
            jvmti_thread_state |= JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_SLEEPING |
                                  JVMTI_THREAD_STATE_WAITING; 
            break;
        case thread_is_waiting:
            jvmti_thread_state |= JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING |
                                  JVMTI_THREAD_STATE_IN_OBJECT_WAIT |
                                  JVMTI_THREAD_STATE_WAITING_INDEFINITELY; 
            break;
        case thread_is_timed_waiting:
            jvmti_thread_state |= JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING |
                                  JVMTI_THREAD_STATE_IN_OBJECT_WAIT |
                                  JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT; 
            break;
        case thread_is_blocked:
            jvmti_thread_state |= JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER; 
            break;
        case thread_is_birthing:
        case thread_is_running:
            jvmti_thread_state |= JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_RUNNABLE; 
            break;
        case thread_is_dying:
            jvmti_thread_state |= JVMTI_THREAD_STATE_TERMINATED; 
            break;
        default:
            ABORT("Unexpected thread state");
            break;
    }
    // end see
    jvmti_thread_state |= this -> jvmti_thread_state & JVMTI_THREAD_STATE_WAITING_INDEFINITELY;
    jvmti_thread_state |= this -> jvmti_thread_state & JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;
    jvmti_thread_state |= this -> jvmti_thread_state & JVMTI_THREAD_STATE_SUSPENDED;
    if (thread_is_alive(thread)){
        jvmti_thread_state |= JVMTI_THREAD_STATE_ALIVE; 
    }
    if (thread_is_interrupted(thread, JNI_FALSE)){
        jvmti_thread_state |= JVMTI_THREAD_STATE_INTERRUPTED; 
    }
    unsigned nnn = interpreter.interpreter_st_get_interrupted_method_native_bit(this);
    if (nnn) {
        jvmti_thread_state |= JVMTI_THREAD_STATE_IN_NATIVE; 
    }
    return jvmti_thread_state;
}

int thread_get_all_threads(jthread** threads_ptr){

    jthread * all_jthreads = NULL;
    int num_active_threads = 0;

    tm_iterator_t * iterator = tm_iterator_create();
    VM_thread *thread = tm_iterator_next(iterator);
    assert(thread);
    while (thread)
    {
        num_active_threads++;
        thread = tm_iterator_next(iterator);
    }

    jvmtiError jvmti_error = _allocate(sizeof(jthread*) *
            num_active_threads, (unsigned char **)&all_jthreads);

    if (JVMTI_ERROR_NONE != jvmti_error)
    {
        tm_iterator_release(iterator);
        return jvmti_error;
    }

    int ii = 0;

    tm_iterator_reset(iterator);
    thread = tm_iterator_next(iterator);
    while (thread)
    {
        all_jthreads[ii] = get_jobject(thread);
        ii++;
        thread = tm_iterator_next(iterator);
    }
    tm_iterator_release(iterator);

    *threads_ptr = all_jthreads;
    return num_active_threads;
}

jobject thread_contends_for_lock(jthread thread)
{
    SuspendEnabledChecker sec;
    VM_thread *vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    assert(vm_thread);

    tmn_suspend_disable();
    ManagedObject *p_obj = mon_enter_array[vm_thread->thread_index].p_obj;
    if (NULL == p_obj)
    {
        tmn_suspend_enable();
        return NULL;
    }

    ObjectHandle oh = oh_allocate_local_handle();
    oh->object = p_obj;
    tmn_suspend_enable();

    return (jobject)oh;
}

int thread_get_owned_monitor_info(jthread thread, jobject ** owned_monitors_ptr) {

    VM_thread * vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    //assert(vm_thread);
    if (!vm_thread)
    {
        return -1; // non-existant thread
    }

    jint count = vm_thread -> jvmti_owned_monitor_count;
    jobject* pp = NULL;
    jvmtiError jvmti_error = _allocate(sizeof(jobject*) * count, (unsigned char **)&pp);

    if (jvmti_error != JVMTI_ERROR_NONE)
    {
        return -1;
    }

    int i;
    for(i = 0; i < count; i++)
    {
        pp[i] = vm_thread -> jvmti_owned_monitors[i];
    }
    *owned_monitors_ptr = pp;
    return count;
}


void thread_stop(jthread thread, jobject UNREF threadDeathException) {

    VM_thread * vm_thread = get_vm_thread_ptr_safe(jni_native_intf, thread);

    vm_thread -> is_stoped = true;
}


int thread_set_jvmt_thread_local_storage(jvmtiEnv* env,
                           jthread thread,
                           const void* data)
{
    if (thread == NULL)
    {
        for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
            if (jvmti_local_storage_static[i].env == env || 
                                     jvmti_local_storage_static[i].env == NULL ) {

                jvmti_local_storage_static[i].env = (data == NULL) ? NULL : env;
                jvmti_local_storage_static[i].data = (void *)data;
                return 0;
            }
        }
        ABORT("Can't find appropriate local storage for the thread");
        return -1;
    }

    VM_thread* vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    if (!vm_thread)
    {
        return -1; // non-existant thread
    }
    JVMTILocalStorage* aa = NULL;
    JVMTILocalStorage* lstg = &vm_thread -> jvmti_local_storage;
    if (lstg -> env == NULL) {
        if (lstg -> data == NULL) {
            // we have no records stored;
            // so, we put our first record into vm_thread -> jvmti_local_storage
            vm_thread -> jvmti_local_storage.env = (data == NULL) ? NULL : env;
            vm_thread -> jvmti_local_storage.data = (void *)data;
            return JVMTI_ERROR_NONE;
        } else {
            // we have more than one record stored;
            // so, they are stored in array which is pointed at by 
            // vm_thread -> jvmti_local_storage -> data  
            aa = (JVMTILocalStorage*)vm_thread -> jvmti_local_storage.data;
        }
    } else {
        // we have just one record stored;
        // so, it's stored in vm_thread -> jvmti_local_storage 
        if (lstg -> env == env) {
            // override data in this record
            lstg -> data = (void *)data;
            return 0;
        } else if (data != NULL){
            // we have just one record stored and we have to add another one; 
            // so, array is created and record is copied there 
            aa = (JVMTILocalStorage*)STD_MALLOC(sizeof(JVMTILocalStorage)*
                                                           MAX_JVMTI_ENV_NUMBER);
            for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
                aa[0].env = NULL;
                aa[0].data = NULL;
            }
            aa[0].env = vm_thread -> jvmti_local_storage.env;
            aa[0].data = vm_thread -> jvmti_local_storage.data;
            vm_thread -> jvmti_local_storage.env = NULL;
            vm_thread -> jvmti_local_storage.data = (void *) aa;
        }
    }
    // array look up for existing env or for free record
    for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
        if (aa[i].env == env || aa[i].env == NULL ) {
            aa[i].env = (data == NULL) ? NULL : env;
            aa[i].data = (void *)data;
            return 0;
        }
    }
    ABORT("Array is full");

    return 0;
}

int thread_get_jvmti_thread_local_storage(jvmtiEnv* env,
                           jthread thread,
                           void** data_ptr)
{
    *data_ptr = NULL;

    if (thread == NULL) // for compatibility with other Java VM
    {
        for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
            if (jvmti_local_storage_static[i].env == env) {
                *data_ptr = jvmti_local_storage_static[i].data;
                break;
            }
        }
        return 0;
    }

    VM_thread* vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);

    if (!vm_thread)
    {
        return -1; // non-existant thread
    }

    JVMTILocalStorage* lstg = &vm_thread -> jvmti_local_storage;
    if (lstg -> env == NULL) {
        if (lstg -> data != NULL) {
            // we have more than one record stored;
            // so, they are stored in array which is pointed at by 
            // vm_thread -> jvmti_local_storage -> data  
            JVMTILocalStorage* aa = (JVMTILocalStorage* )lstg -> data;
            for (int i = 0; i < MAX_JVMTI_ENV_NUMBER; i++){
                if (aa[i].env == env) {
                    *data_ptr = aa[i].data;
                    break;
                }
            }
        }
    } else {
        // we have just one record stored;
        // so, it's stored in vm_thread -> jvmti_local_storage 
        if (lstg -> env == env) {
            *data_ptr = lstg -> data;
        }
    }
    return 0;
}
