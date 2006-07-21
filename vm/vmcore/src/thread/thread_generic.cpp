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
 * @version $Revision: 1.1.2.5.4.5 $
 */  


#define LOG_DOMAIN "thread"
#include "cxxlog.h"

#include "platform.h"
#include "vm_process.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;

#include <signal.h>
#include <stdlib.h>

#if defined (PLATFORM_NT)
#include <direct.h>
#elif defined (PLATFORM_POSIX)
#include <sys/time.h>
#include <unistd.h>
#endif

#include "environment.h"
#include "vm_strings.h"
#include "open/types.h"
#include "open/vm_util.h"
#include "object_layout.h"
#include "Class.h"
#include "classloader.h"
#include "open/gc.h"
#include "vm_threads.h"
#include "nogc.h"
#include "ini.h"
#include "m2n.h"
#include "exceptions.h"
#include "jit_intf.h"
#include "vm_synch.h"
#include "exception_filter.h"
#include "vm_threads.h"
#include "jni_utils.h"
#include "object.h"
#include "open/thread.h"
#include "platform_core_natives.h"
#include "heap.h"
#include "verify_stack_enumeration.h"

#include "sync_bits.h"
#include "vm_stats.h"
#include "native_utils.h"

#ifdef PLATFORM_NT
// wjw -- following lines needs to be generic for all OSs
#include "java_lang_thread_nt.h"
#endif

#ifdef _IPF_
#include "java_lang_thread_ipf.h"
#else
#include "java_lang_thread_ia32.h"
#endif

#include "thread_manager.h"
#include "object_generic.h"
#include "thread_generic.h"
#include "open/thread.h"
#include "mon_enter_exit.h"

#include "jni_direct.h"

#ifdef _IPF_
#include "../m2n_ipf_internal.h"
#elif defined _EM64T_
#include "../m2n_em64t_internal.h"
#else
#include "../m2n_ia32_internal.h"
#endif

void __cdecl call_the_run_method2(void *p_xx);

int next_thread_index = 1;  // never use zero for a thread index

/////////////////////////////////////////////////////////////////////
// Native lib stuff

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

void set_vm_thread_ptr_safe(JNIEnv *jenv, jobject jThreadObj, jlong data)
{
    jclass jthreadclass = jenv->GetObjectClass(jThreadObj);
    jfieldID field_id = jenv->GetFieldID(jthreadclass, "vm_thread", "J");
    jenv->SetLongField(jThreadObj, field_id, data);
}

VM_thread *get_vm_thread_ptr_safe(JNIEnv *jenv, jobject jThreadObj)
{
    jclass jThreadClass = jenv->GetObjectClass(jThreadObj);
    jfieldID field_id = jenv->GetFieldID(jThreadClass, "vm_thread", "J");
    POINTER_SIZE_INT data = (POINTER_SIZE_INT)jenv->GetLongField(jThreadObj, field_id);
    
    return (VM_thread *)data;
}

// ToDo: Remove this function. Use get_vm_thread_ptr_safe() instead.
static VM_thread *
my_get_vm_thread_ptr_safe(JNIEnv *jenv, jobject jThreadObj)
{
    jclass jThreadClass = jenv->GetObjectClass(jThreadObj);
    jfieldID field_id = jenv->GetFieldID(jThreadClass, "vm_thread", "J");
    POINTER_SIZE_INT eetop = (POINTER_SIZE_INT)jenv->GetLongField(jThreadObj, field_id);    
    return (VM_thread *)eetop;
} //my_get_vm_thread_ptr_safe

static boolean is_daemon_thread(JNIEnv *jenv, jobject jThreadSelf)
{
   //FIXME should not use JNI here
    jclass jthreadclass = jenv->GetObjectClass(jThreadSelf);
    jfieldID daemon_id = jenv->GetFieldID(jthreadclass, "daemon", "Z");
    jboolean daemon = jenv->GetBooleanField(jThreadSelf, daemon_id);
    return daemon ? TRUE : FALSE;
} //is_daemon_thread

// Call default constructor for java.lang.Thread object (should be called for main thread)
static bool init_thread_object(JNIEnv *jenv, jobject jthread)
{
    // DRL uses this function to initialize main thread's java.lang.Thread object.
    jclass tclazz = FindClass(jenv, "java/lang/Thread");
    assert(tclazz);

    jmethodID tconstr = GetMethodID(jenv, tclazz, "<init>", "()V");
    assert(tconstr);

    CallVoidMethod(jenv, jthread, tconstr);

    if (ExceptionOccurred(jenv)) {
        WARN("*** Error: exception occured in main Thread constructor.");
        ExceptionDescribe(jenv);
        ExceptionClear(jenv);
        return false;
    }

    return true;
} //init_thread_object

void thread_sleep(jthread UNREF thread, long millis, int UNREF nanos){
    Java_java_lang_Thread_sleep_generic(jni_native_intf, get_thread_ptr(), millis);
} 

//JNI implementation of Thread.sleep()
void Java_java_lang_Thread_sleep_generic(JNIEnv *jenv, VM_thread * p_thr, int64 msec)
{
    assert(!((unsigned int)(msec >> 32))); // We currently ignore high 32 bits of msec.

    tm_acquire_tm_lock(); 

    jint stat;

    stat = vm_reset_event(p_thr->event_handle_notify_or_interrupt);  // throw away old vm_set_event()s
    assert(stat);

    assert(p_thr->app_status == thread_is_running);
    p_thr->app_status = thread_is_sleeping;

    tm_release_tm_lock(); 
 
    assert(tmn_is_suspend_enabled());
    p_thr -> is_suspended = true;

    while (p_thr->interrupt_a_waiting_thread == false) {
        stat = vm_wait_for_single_object(p_thr->event_handle_notify_or_interrupt, (int)msec);

        if (stat == WAIT_TIMEOUT)
        {
#ifdef _DEBUG
            total_sleep_timeouts++;
#endif
            break;
        }

        // the sleep did not time out and nobody sent us an interrupt
        // the only other possibility is that this is a stale notify/notifyAll()
        //
        // if other threads did a notify()/notifyAll() and between
        // the time they selected the current thread and the time they 
        // do the notifyAll's vm_set_event(), they get preempted for a long enough period of 
        // time such that the current thread gets to the above WaitForSingleEvent()
        // then we ignore this unblocking and loop back to the waitforsingleevent()
        // note that we re-wait for the same amount of msec.  This should not cause a problem
        // since pre-emptive multitasking guarantees a sleep() will be at least
        // mseconds or longer.  If the system is lightly loaded, the stale notify()s will
        // get flushed out quickly.  If the system is heavily loaded, the run queues
        // are long meaning a sleep will wakeup and go onto a long run queue anyway
    }
    p_thr -> is_suspended = false;
    tmn_suspend_disable(); // to suspend thread if needed
    tmn_suspend_enable();

    tm_acquire_tm_lock(); 
    assert(p_thr->app_status == thread_is_sleeping);
    p_thr->app_status = thread_is_running;

    if (p_thr->interrupt_a_waiting_thread == true)
    {
        // this can only happen if someone really did send us an interrupt
        p_thr->interrupt_a_waiting_thread = false;
#ifdef _DEBUG
        total_sleep_interrupts++;
#endif
        tm_release_tm_lock(); 
        throw_exception_from_jni(jenv, "java/lang/InterruptedException", 0);
        return; // gregory - added return here because otherwise we execute
                // unlock_enum twice which results in error and failed assert on
                // error code from pthread_mutex_unlock that thread doesn't own
                // the mutex. The thread doesn't own mutex because it was unlocked in
                // this if block
    }

    tm_release_tm_lock(); 
}

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

void Java_java_lang_Thread_interrupt_generic(VM_thread *p_thr)
{
    if (!p_thr) {
        printf("bad interrupt, current thread_index = %d\n", p_TLS_vmthread->thread_index);
        return;
    }

    p_thr->interrupt_a_waiting_thread = true;
    unpark(p_thr);

    jint UNUSED stat = vm_set_event(p_thr->event_handle_notify_or_interrupt);
    assert(stat);
}

void Java_java_lang_Thread_setPriority_generic(VM_thread *p_vm_thread, int pri)
{

    tm_acquire_tm_lock();
    tmn_suspend_disable();

    ASSERT(0, "Is not expected to be called"); 

    // NOTE: setPriority is actually called before the thread is started
    // in which case, p_vm_thread is null... Thread_start will then set the priority
    
    if(p_vm_thread == 0) {
        tmn_suspend_enable();
        tm_release_tm_lock();
        return;
    }
    if (p_vm_thread->app_status == zip) {
        tmn_suspend_enable();
        tm_release_tm_lock();
        return;
    }    
    
    int status = p_vm_thread->setPriority(THREAD_PRIORITY_NORMAL + pri - 5);
    if(status == 0) {
        jint error = IJGetLastError();
        printf ("java_lang_Thread_setPriority error = 0x%x\n", error);   
    }

    tmn_suspend_enable();
    tm_release_tm_lock();
} //Java_java_lang_Thread_setPriority_generic

static void
my_clear_vm_thread_ptr_safe(JNIEnv *jenv, jobject jThreadObj)
{
    assert(tmn_is_suspend_enabled());
    jclass jThreadClass = jenv->GetObjectClass(jThreadObj);
    jfieldID field_id = jenv->GetFieldID(jThreadClass, "vm_thread", "J");
    jenv->SetLongField(jThreadObj, field_id, (jlong) NULL);
    assert(tmn_is_suspend_enabled());
} //my_clear_vm_thread_ptr_safe

// This method should be called when Thread.run() execution ended with an exception.
static void process_uncaught_exception(VM_thread *p_vm_thread, ManagedObject *exn) {
    // create handle for exception object
    ObjectHandle exn_handle = oh_allocate_local_handle();
    exn_handle->object = exn;

    // create handle for java.lang.Thread object
    ObjectHandle thread_handle = oh_allocate_local_handle();
    thread_handle->object = (ManagedObject*) p_vm_thread->p_java_lang_thread;

    // lookup Thread.getThreadGroup() method
    String *name   = VM_Global_State::loader_env->string_pool.lookup("getThreadGroup");
    String *descr  = VM_Global_State::loader_env->string_pool.lookup("()Ljava/lang/ThreadGroup;");

    VTable *p_vtable = (((ManagedObject *)thread_handle->object)->vt());
    Method *getThreadGroup_method = class_lookup_method_recursive(p_vtable->clss, name, descr);
    assert(getThreadGroup_method);

    // execute Thread.getThreadGroup() method in order to get threadGroup
    jvalue getThreadGroup_args[1];
    jvalue threadGroup;
    //tmn_suspend_enable();
    getThreadGroup_args[0].l = (jobject) thread_handle;    // this
    vm_execute_java_method_array((jmethodID) getThreadGroup_method, &threadGroup, getThreadGroup_args);
    //tmn_suspend_disable();

    // check for exception
    ManagedObject* cte = get_current_thread_exception();
    clear_current_thread_exception();
    if (cte || ! threadGroup.l) {
        print_uncaught_exception_message(stderr, "thread execution", exn_handle->object);
        return;
    }

    // lookup ThreadGroup.uncaughtException() method
    name   = VM_Global_State::loader_env->string_pool.lookup("uncaughtException");
    descr  = VM_Global_State::loader_env->string_pool.lookup("(Ljava/lang/Thread;Ljava/lang/Throwable;)V");

    p_vtable = (((ManagedObject *)threadGroup.l->object)->vt());
    Method *uncaughtException_method = class_lookup_method_recursive(p_vtable->clss, name, descr);
    assert(uncaughtException_method);

    // execute ThreadGroup.uncaughtException() method
    jvalue uncaughtException_args[3];
    //tmn_suspend_enable();
    uncaughtException_args[0].l = (jobject) threadGroup.l;    // this
    uncaughtException_args[1].l = (jobject) thread_handle;    // Thread
    uncaughtException_args[2].l = (jobject) exn_handle;       // Throwable
    vm_execute_java_method_array((jmethodID) uncaughtException_method, NULL, uncaughtException_args);
    //tmn_suspend_disable();

    // check for exception
    cte = get_current_thread_exception();
    clear_current_thread_exception();
    if (cte) {
        print_uncaught_exception_message(stderr, "thread execution", exn_handle->object);
        return;
    }
}

//JNI implementation
void __cdecl call_the_run_method( void * p_args )
{
#ifdef _IA32_
    init_stack_info();
#ifdef PLATFORM_POSIX
    set_guard_stack();
#endif // PLATFORM_POSIX
#endif // _IA32_

    //when a new thread created, gc is disabled, because VM thread structure 
    //was set 0 initially, then gc_enabled_status kept 0 till now
    VM_thread *p_vm_thread=(VM_thread *)(((void **)p_args)[0]); 
    JNIEnv *jenv=(JNIEnv *)(((void **)p_args)[1]);
    ObjectHandle jThreadSelf = ((ObjectHandle*)p_args)[2];

    jvmtiEnv * jvmti_Env = (jvmtiEnv *)((void **)p_args)[3];
    jvmtiStartFunction jvmtiStartProc = (jvmtiStartFunction)((void **)p_args)[4]; 
    void* jvmtiStartProcArg = ((void **)p_args)[5];

    set_TLS_data(p_vm_thread);
    MARK_STACK_END

    tmn_suspend_disable();
    
    m2n_set_last_frame(NULL);
    
    p_vm_thread->thread_id = GetCurrentThreadId();

    // Invoke Thread.run()
    // BUGBUG its an interface method but we cheat by looking up "run" in the instance instead of going thru interface lookup
    String *name   = VM_Global_State::loader_env->string_pool.lookup("run");
    String *descr  = VM_Global_State::loader_env->string_pool.lookup("()V");

    VTable *p_vtable = (((ManagedObject *)p_vm_thread->p_java_lang_thread)->vt());
    Method *start_method = class_lookup_method_recursive(p_vtable->clss, name, descr);
    assert(start_method);

    SET_THREAD_DATA_MACRO();

    int old_floating_point_state = 0;
    void setup_floating_point_state(int *);
    setup_floating_point_state(&old_floating_point_state);
    // Set the app_status to "running" just before entering Java code. It is "birthing" till now.

    p_vm_thread->app_status = thread_is_running;
    active_thread_count ++;

    // the thread that originally called java_lang_Thread_start() will wait for this event 
    jint UNUSED stat = vm_set_event(new_thread_started_handle);
    assert(stat);

     // The scope of lhs must be very precise to make it work properly
    { NativeObjectHandles lhs;

    gc_thread_init(&p_vm_thread->_gc_private_information);

    assert(!tmn_is_suspend_enabled());
    tmn_suspend_enable();
    jvmti_send_thread_start_end_event(1);
    tmn_suspend_disable();

    jvalue args[1];
    ObjectHandle h = oh_allocate_local_handle();
    h->object = (ManagedObject*) p_vm_thread->p_java_lang_thread;
    args[0].l = (jobject) h;

    if (jvmtiStartProc){
        // ppervov: all JVM TI agent functions must be run with gc enabled
        tmn_suspend_enable();
        jvmtiStartProc(jvmti_Env, jenv, jvmtiStartProcArg);
        tmn_suspend_disable();
    } else {
        //tmn_suspend_enable();
        
        vm_execute_java_method_array((jmethodID) start_method, 0, args);
        //tmn_suspend_disable();
    }

    assert(!tmn_is_suspend_enabled());

    // If an exception resulted from calling the run method, call 
    // ThreadGroup.uncaughtException()
    ManagedObject* exn = get_current_thread_exception();
    clear_current_thread_exception();
    if (exn)
        process_uncaught_exception(p_vm_thread, exn);

    void cleanup_floating_point_state(int);
    cleanup_floating_point_state(old_floating_point_state);

    clear_current_thread_exception();
    tmn_suspend_enable();
    jvmti_send_thread_start_end_event(0);
    tmn_suspend_disable();

    assert(p_vm_thread->app_status == thread_is_running);

    Method *kill_method = class_lookup_method_recursive(p_vtable->clss, "kill", "()V");
    assert(kill_method);
    vm_execute_java_method_array((jmethodID) kill_method, 0, args);
    assert(!exn_raised());

    p_vm_thread->app_status = thread_is_dying;

    gc_thread_kill(&p_vm_thread->_gc_private_information);
    
    tmn_suspend_enable();
    vm_monitor_enter_slow_handle(jThreadSelf);  

    thread_object_notify_all(jThreadSelf);
    vm_monitor_exit_handle(jThreadSelf);
    
    tm_acquire_tm_lock();

    if (! is_daemon_thread(jenv, (jobject)jThreadSelf)) {
        non_daemon_thread_count--;   
        TRACE2("thread", "non daemon threads removed: " << non_daemon_thread_count);
 
    }
    //--------------------------------------------------------- fix 979
    // pointer to VM_thread structure wasn't cleared and was
    // wrongly used after correasponding thread was terminated and 
    // VM_thread structure was reused for another thread
    my_clear_vm_thread_ptr_safe(jenv, (jobject)jThreadSelf); 
    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ fix 979
   } // end of scope for lhs

    // We should remove those useless global handles
    oh_deallocate_global_handle(jThreadSelf);
    tmn_suspend_disable();

    /*
        wgs: I did meet with the assertion failure here. The scenario is:
        A Thread.run() begins to compile a method, but fails in loading 
        some exception classes which are necessary for compiling the method, 
        thus ClassNotFoundException is throwed. Thread.run() has a handler 
        for all exception, so it catches and exits, but note that the compilation
        of the method never finishes.
        That might prevent further compilation because we have global compile lock.

        Should we use a defensive way:
            if (p_vm_thread->gc_frames != 0)
                p_jit_a_method_lock->_unlock();                
    */
    assert(p_vm_thread->gc_frames == 0);  

    free_this_thread_block( p_vm_thread );
    set_TLS_data(NULL);
    THREAD_CLEANUP();

    TRACE2("thread", "non daemon threads count: " << non_daemon_thread_count); 
    if (!non_daemon_thread_count) {
        TRACE2("thread", "non_daemon_threads_dead_handle set" ); 
        jint UNUSED stat = vm_set_event(non_daemon_threads_dead_handle);
        assert(stat);
    }

    active_thread_count --;
    tm_release_tm_lock();
    
    vm_endthread();
}

 
void thread_start(JNIEnv* jenv, jobject jthread)
{
    Java_java_lang_Thread_start_generic(jenv, jthread, NULL, NULL, NULL, 5);
} //thread_start

//JNI implementation
void Java_java_lang_Thread_start_generic(JNIEnv *jenv, jobject jThreadSelf,
                                         jvmtiEnv * jvmtiEnv, jvmtiStartFunction proc, 
                                         const void* arg, jint priority)
{
    //GC is enabled in JNI invocation of Java_java_lang_Thread_start

    // get a nursery for child thread. this invocation previously is 
    // by child thread itself in call_the_run_method()

    //BUGBUG this should throw IllegalThreadStateException  
    // because field in use means thread is already running     
    assert(! get_vm_thread_ptr_safe(jenv, jThreadSelf)); 

    tm_acquire_tm_lock();
    TRACE2("thread", "starting a new thread");

    VM_thread * p_vm_thread = get_a_thread_block();
    if (NULL == p_vm_thread) {
        tm_release_tm_lock();
        TRACE2("thread", "can't get a thread block for a new thread");
        exn_raise_by_name("java/lang/OutOfMemoryError",
                "too many threads created");
        return;
    }

    p_vm_thread->p_java_lang_thread = ((ObjectHandle)jThreadSelf)->object;

    set_vm_thread_ptr_safe(jenv, jThreadSelf, (jlong) POINTER_SIZE_INT(p_vm_thread));

    assert(get_vm_thread_ptr_safe(jenv, jThreadSelf)); 

    if (! is_daemon_thread(jenv, (jobject)jThreadSelf)) {
            non_daemon_thread_count++;
            TRACE2("thread", "non daemon threads total: " << non_daemon_thread_count);
    }
    ObjectHandle hThreadSelf = oh_allocate_global_handle();
    tmn_suspend_disable();
    hThreadSelf->object = ((ObjectHandle)jThreadSelf)->object;

    void *p_args[7]={(void*)p_vm_thread, (void*)jenv, (void *)hThreadSelf,
                     (void*)jvmtiEnv, (void*)proc, (void*)arg, (void*)((POINTER_SIZE_INT)priority)};


VmThreadHandle stat =  vm_beginthread( 
                       (void(__cdecl *)(void *))call_the_run_method2, 
                       (unsigned)(64*1024), 
                       (void *)p_args);

    if (!stat) {
        TRACE2("thread", "thread failed to start, raising OOME");
        free_this_thread_block(p_vm_thread);
        tmn_suspend_enable();
        set_vm_thread_ptr_safe(jenv, jThreadSelf, (jlong) 0);
        assert(get_vm_thread_ptr_safe(jenv, jThreadSelf) == NULL); 
        tm_release_tm_lock();
        exn_raise_by_name("java/lang/OutOfMemoryError",
                "can't start new thread");
        return;
    }
    
    p_vm_thread->thread_handle = stat;

    Sleep(0);

    // the new thread just resumed above will set new_thread_started_handle
    int status;
    //FIXME we should release thread_Lock before wait.
    status = vm_wait_for_single_object(new_thread_started_handle, INFINITE);

    ASSERT(status != (int)WAIT_FAILED, "Unexpected error while starting new thread: " << IJGetLastError());
    
    tmn_suspend_enable();
    assert(thread_is_birthing != p_vm_thread->app_status);
    tm_release_tm_lock();
} //Java_java_lang_Thread_start_generic


//
//  For IPF configuration this method is defined in nt_exception_filter.cpp
//

#if !defined(_IPF_) || defined(PLATFORM_POSIX)
#ifdef __INTEL_COMPILER
#pragma optimize("", off)
#endif
void __cdecl call_the_run_method2(void *p_args)
{ 
    VM_thread *p_vm_thread;
    p_vm_thread = (VM_thread *)(((void **)p_args)[0]);  

    set_TLS_data(p_vm_thread);
    
    // Hyperthreading (HT) optimization.  We allocate extra space on the stack
    // so that each thread's stack is "misaligned" with other threads' stacks.
    // The offset is 256*thead_index, capped at 16KB.
    if (VM_Global_State::loader_env->is_hyperthreading_enabled)
    {
        STD_ALLOCA(((p_TLS_vmthread->thread_index - 1) % 64) * 256);
    }

#ifdef PLATFORM_POSIX
    call_the_run_method( p_args );
#else // !PLATFORM_POSIX

      // A temporary patch needed to support j9 class libraries.
      // TODO: remove it after the needed subset of j9thread functions is implemented.
 
      {
          static HINSTANCE hDLL;           // Handle to j9thr23.dll
          typedef void* (*lpFunc)(void*);  //
          static lpFunc func;              // j9thread_attach proc address
          static boolean firstAttempt = true;
 
          if (firstAttempt) {
              hDLL = GetModuleHandle("j9thr23.dll");
              // hDLL = LoadLibrary("j9thr23");
 
              if (hDLL != NULL) {
                   func = (lpFunc)GetProcAddress(hDLL, "j9thread_attach");
              }
          }
 
          if (func != NULL) {
              func(NULL); // Attach to j9thread
          }
      } 

    call_the_run_method( p_args );
#endif //#ifdef PLATFORM_POSIX

}
#ifdef __INTEL_COMPILER
#pragma optimize("", on)
#endif
#endif // !_IPF_



////////////////////////////////////////////////////////////////////////////////////////////
//////// CALLED by vm_init() to initialialize thread groups and create the main thread ////
////////////////////////////////////////////////////////////////////////////////////////////


bool init_threadgroup()
{
    NativeObjectHandles lhs;    // allows us to create handles in native code not called from JITed code

    Global_Env *env = VM_Global_State::loader_env;
    JNIEnv *jenv = (JNIEnv *)jni_native_intf;

    assert(tmn_is_suspend_enabled());

    // Load, prepare and initialize the "Thread class"
    String *ss = env->string_pool.lookup("java/lang/Thread");
    Class *thread_clss = env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, ss);
    assert(tmn_is_suspend_enabled());
    tmn_suspend_disable();
    class_initialize(thread_clss);
    assert(!tmn_is_suspend_enabled());

    ObjectHandle jThreadClass = oh_allocate_local_handle();
    ObjectHandle jname = oh_allocate_local_handle();
    ObjectHandle jMainThreadObj = oh_allocate_local_handle();
    jThreadClass->object = struct_Class_to_java_lang_Class(thread_clss);
    jname->object = string_create_from_utf8("init", 4);

    // Allocate the Thread object for ... public static void main(String args[])
    // Volatile needed here since GC is enabled by JNI functions below and the use of 
    // "main_thread_obj" after the JNI calls shouldnt become stale.
    volatile ManagedObject *main_thread_obj = class_alloc_new_object(thread_clss);
    assert(main_thread_obj);

    jMainThreadObj->object = (ManagedObject *) main_thread_obj;

    p_TLS_vmthread->thread_id = GetCurrentThreadId();
    p_TLS_vmthread->p_java_lang_thread = jMainThreadObj->object;
    tmn_suspend_enable();
    assert(tmn_is_suspend_enabled());
 
    // Set all all attributes for "main" thread
    set_vm_thread_ptr_safe(jenv, (jobject)jMainThreadObj, (jlong) POINTER_SIZE_INT(p_TLS_vmthread));

    // Call constructor for java.lang.Thread object of "main" thread
    if (! init_thread_object(jenv, (jobject) jMainThreadObj))
        return false;
 
    int UNUSED stat =
        DuplicateHandle(GetCurrentProcess(),        // handle to process with handle to duplicate
                GetCurrentThread(),                 // handle to duplicate
                GetCurrentProcess(),                // handle to process to duplicate to
               // gashiman - Duplicate handle does not do anything on linux
               // so it is safe to put type convertion here
                (VmEventHandle*)&(p_TLS_vmthread->thread_handle),   // pointer to duplicate handle
                0,                              // access for duplicate handle
                false,                              // handle inheritance flag
                DUPLICATE_SAME_ACCESS               // optional actions
                );
    assert(stat);

    p_TLS_vmthread->thread_id = GetCurrentThreadId();

#ifdef PLATFORM_POSIX
#else
    assert(p_TLS_vmthread->event_handle_monitor);
#endif

    return true;
} //init_threadgroup

// Alexei
// migrating to C interfaces for Linux
#if (defined __cplusplus) && (defined PLATFORM_POSIX)
extern "C" {
#endif

jobject thread_current_thread()
{
    void *thread_ptr = get_thread_ptr();
    tmn_suspend_disable(); //------------v-----------
    ObjectHandle hThread = NULL;
    ManagedObject* object = (struct ManagedObject *)((VM_thread*)thread_ptr)->p_java_lang_thread;
    if (object)
    {
        hThread = oh_allocate_local_handle();
        hThread->object = object;
    }
    tmn_suspend_enable();  //------------^----------- 
    return (jobject)hThread; 
} //thread_current_thread

bool thread_is_alive(jthread thread)
{
    VM_thread *p_vmthread = my_get_vm_thread_ptr_safe(jni_native_intf, thread);
    if ( !p_vmthread ) {
        return 0; // don't try to isAlive() non-existant thread
    }

   java_state as = p_vmthread->app_status;  

    // According to JAVA spec, this method should return true, if and
    // only if the thread has been started and not yet died. 
    switch (as) {
        case thread_is_sleeping:
        case thread_is_waiting:
        case thread_is_timed_waiting:
        case thread_is_blocked:
        case thread_is_running:
        case thread_is_birthing:   
            return 1; 
            //break; // exclude remark #111: statement is unreachable
        case thread_is_dying:
        case zip:
            return 0; 
            //break; // exclude remark #111: statement is unreachable
        default:
            DIE("big problem in java_lang_Thread_isAlive(), p_vmthread->appstatus == " << ((int)as));
            return 0;
            //break; // exclude remark #111: statement is unreachable
    }
    // must return from inside the switch statement // remark #111: statement is unreachable
} //thread_is_alive

void thread_suspend(jobject jthread) 
{
     tm_acquire_tm_lock(); 
    // if thread is not alived do nothing
    if(!thread_is_alive(jthread)) {
        tm_release_tm_lock(); 
        return;
    }
    VM_thread * vm_thread = get_vm_thread_ptr_safe(jni_native_intf, jthread);
    thread_suspend_generic(vm_thread);
    tm_release_tm_lock(); 
}

void thread_resume(jobject jthread) 
{
     tm_acquire_tm_lock(); 
   // if thread is not alived do nothing
    if(!thread_is_alive(jthread)) {
        tm_release_tm_lock(); 
        return;
    }
    VM_thread * vm_thread = get_vm_thread_ptr_safe(jni_native_intf, jthread);
    thread_resume_generic(vm_thread);
    tm_release_tm_lock(); 
}

void thread_join(jthread thread, long millis, int UNREF nanos)
{
    assert(thread);
    VM_thread* self = get_thread_ptr();

    vm_monitor_enter_slow_handle(thread);
    while (thread_is_alive(thread)  
           && WAIT_TIMEOUT != java_lang_Object_wait(thread, millis)
           && false == self->interrupt_a_waiting_thread ) {
        // propogate not our notify
        // in any case spurious notify allowed by spec
        thread_object_notify(thread);
    }    
    vm_monitor_exit_handle(thread);
} //thread_join

void thread_interrupt(jobject jthread)
{
    VM_thread *p_vmthread = my_get_vm_thread_ptr_safe(jni_native_intf, jthread);
    //XXX this is a temporary solution
    // the upcoming threading design should eliminate the cases no object 
    // representing a thread exists 
    if (p_vmthread) {
        Java_java_lang_Thread_interrupt_generic(p_vmthread);
    }
}

bool thread_is_interrupted(jobject thread, bool clear)
{
    VM_thread *p_thr = my_get_vm_thread_ptr_safe(jni_native_intf, thread);
    //XXX this is a temporary solution
    // the upcoming threading design should eliminate the cases no object 
    // representing a thread exists
    if (p_thr){
        jboolean res = (jboolean)(p_thr->interrupt_a_waiting_thread ? TRUE : FALSE);
        if (clear) p_thr->interrupt_a_waiting_thread = false;
        return res;
    }
    return FALSE; 
}

void* thread_get_interrupt_event()
{
    VM_thread* thr = get_thread_ptr();
    return (void*)thr->event_handle_notify_or_interrupt;
}

#if (defined __cplusplus) && (defined PLATFORM_POSIX)
}
#endif

int parktimed(jlong milis, jint nanos) {
    assert(tmn_is_suspend_enabled());
    int ret_val;
   if (milis<=0 && nanos <=0) return 0;    

    ret_val = WaitForSingleObject(p_TLS_vmthread->park_event,(DWORD)milis);
       
    return ret_val;
}

int parkuntil(jlong milis, jint UNREF nanos) {
    assert(tmn_is_suspend_enabled());
    int ret_val;
    jlong delta = milis - get_current_time();
    if (delta <= 0) return 0;
    
    ret_val =  WaitForSingleObject(p_TLS_vmthread->park_event, (DWORD)delta);
   
    return ret_val;
}

int park(void) {
    assert(tmn_is_suspend_enabled());
    int ret_val;
    ret_val = WaitForSingleObject(p_TLS_vmthread->park_event, INFINITE);
   
    return ret_val;
}

int unpark(VM_thread *thread) {
    return SetEvent(thread->park_event);
}

void wait_until_non_daemon_threads_are_dead() {
        tm_acquire_tm_lock();
    
        if(!non_daemon_thread_count) {
            tm_release_tm_lock();           
            return;
        }   
        tm_release_tm_lock();   
        
        // the following lines allow MSVC++ "Debug->>>Break" to work
        while (1) {
            DWORD stat = vm_wait_for_single_object(non_daemon_threads_dead_handle, 2000);
            if (stat == WAIT_OBJECT_0) {
                INFO("VM is signalled to shutdown");
                break;
            }
            assert(stat != WAIT_FAILED);

        }

} //wait_until_non_daemon_threads_are_dead

void terminate_all_threads() {
        tm_iterator_t * iterator = tm_iterator_create();
        VM_thread *self = p_TLS_vmthread;
        volatile VM_thread *p_scan = tm_iterator_next(iterator);
        while (p_scan) {
            if(p_scan != self) {
                vm_terminate_thread (p_scan->thread_handle);
            }
            p_scan = tm_iterator_next(iterator);
        }
        tm_iterator_release(iterator);    
}
