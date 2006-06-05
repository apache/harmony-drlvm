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
 * @version $Revision: 1.1.2.1.4.5 $
 */  


#include "platform_lowlevel.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;

#ifndef PLATFORM_POSIX
#include "vm_process.h"
#endif

#include "open/vm_util.h"
#include "nogc.h"
#include "sync_bits.h"
#include "vm_synch.h"

#include "lock_manager.h"
#include "thread_manager.h"
#include "thread_generic.h"
#include "vm_threads.h"
#define LOG_DOMAIN "thread"
#include "cxxlog.h"
#include "tl/memory_pool.h"
#include "open/vm_util.h"
#include "suspend_checker.h"

// wjw -- following lines needs to be generic for all OSs
#ifndef PLATFORM_POSIX
#include "java_lang_thread_nt.h"
#endif

#ifdef _IPF_
#include "java_lang_thread_ipf.h"
#else
#include "java_lang_thread_ia32.h"
#endif

static StaticInitializer thread_runtime_initializer;

static tl::MemoryPool thr_pool;

VM_thread *p_threads_iterator;
VM_thread *p_free_thread_blocks;
VM_thread *p_active_threads_list;

#ifdef USE_TLS_API 
#ifdef PLATFORM_POSIX
__thread VM_thread *p_TLS_vmthread = NULL;
VM_thread *get_thread_ptr()
{
    return p_TLS_vmthread;
}

#else //PLATFORM_POSIX
__declspec( thread ) VM_thread *p_TLS_vmthread;
#endif //PLATFORM_POSIX else
#endif //USE_TLS_API

#ifdef __cplusplus
extern "C" {
#endif

volatile VM_thread *p_the_safepoint_control_thread = 0;  // only set when a gc is happening
volatile safepoint_state global_safepoint_status = nill;
// Incremented at the start and at the end of each GC. The total number
// of GC is this number * 2. This is done so that when a lot of application
// threads discover that they need a GC only on is done.

unsigned non_daemon_thread_count = 0;

VmEventHandle non_daemon_threads_dead_handle = 0;
VmEventHandle new_thread_started_handle = 0;

VmEventHandle non_daemon_threads_are_all_dead;

thread_array quick_thread_id[MAX_VM_THREADS];
POINTER_SIZE_INT hint_free_quick_thread_id;

#ifdef __cplusplus
}
#endif

#ifdef _DEBUG
////////////Object_Handle vm_create_global_object_handle();  // bugbug -- where does this go?
#include "jni.h"
#endif


void vm_thread_init(Global_Env * UNREF p_env___not_used)
{
    new_thread_started_handle = vm_create_event( 
            NULL,   // pointer to security attributes 
            FALSE,  // flag for manual-reset event  -- auto reset mode 
            FALSE,  // flag for initial state 
            N_T_S_H // pointer to event-object name 
        ); 
}


void vm_thread_shutdown()
{
} //vm_thread_shutdown



VM_thread * get_a_thread_block()
{
    VM_thread *p_vmthread;
    int thread_index;

    for( thread_index=1; thread_index < MAX_VM_THREADS; thread_index++ ){
        if (quick_thread_id[thread_index].p_vmthread == NULL )
           break; 
    }
    if (thread_index == MAX_VM_THREADS){
        WARN("Out of java threads, maximum is " << MAX_VM_THREADS);
        return NULL;
    } 
    
    next_thread_index = (thread_index == next_thread_index)? ++next_thread_index : next_thread_index;
    
    if (p_free_thread_blocks) {
        p_vmthread = p_free_thread_blocks;
        p_free_thread_blocks = p_free_thread_blocks->p_free;
        p_vmthread->p_free = 0;
        p_vmthread->app_status = thread_is_birthing;
        TRACE2("thread", "Reusing old thread block " << p_vmthread << " for a new thread");
     }
    else {
        p_vmthread = (VM_thread *)thr_pool.alloc(sizeof(VM_thread));
        memset(p_vmthread, 0, sizeof(VM_thread) );
        TRACE2("thread", "Creating new thread block " << p_vmthread);
        
        //park semaphore initialization
        p_vmthread->park_event = CreateEvent( 
               NULL,   // pointer to security attributes 
               FALSE,  // flag for manual-reset event  -- auto reset mode 
               FALSE,  // flag for initial state 
               NULL    // pointer to event-object name 
           ); 

        // If new thread block is being created, set app_status to "birthing" here too.
        p_vmthread->app_status = thread_is_birthing;

        p_vmthread->event_handle_monitor = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- auto reset mode 
                FALSE,  // flag for initial state 
                E_H_M   // pointer to event-object name 
            ); 
        assert(p_vmthread->event_handle_monitor);

        p_vmthread->event_handle_sleep = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- auto reset mode 
                FALSE,  // flag for initial state 
                E_H_S   // pointer to event-object name 
            ); 
        assert(p_vmthread->event_handle_sleep);

        p_vmthread->event_handle_notify_or_interrupt = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- auto reset mode 
                FALSE,  // flag for initial state 
                E_H_I   // pointer to event-object name 
            ); 
        assert(p_vmthread->event_handle_notify_or_interrupt);

    p_vmthread->jvmti_resume_event_handle = vm_create_event( 
                NULL,   // pointer to security attributes 
                TRUE,  // flag for manual-reset event  -- manual mode
                TRUE,  // flag for initial state 
                J_V_M_T_I_E_H // pointer to event-object name 
            ); 
        assert(p_vmthread->jvmti_resume_event_handle);

    // ============== new SUSPEND related variables setup =====================
    p_vmthread->suspend_request = 0;
    p_vmthread->suspended_event = vm_create_event( 
                NULL,   // pointer to security attributes 
                TRUE,  // flag for manual-reset event  -- manual mode
                FALSE,  // flag for initial state 
                E_H_S0 // pointer to event-object name 
            ); // set when thread is suspended 
            
      assert(p_vmthread->suspended_event);

     p_vmthread->resume_event = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- manual mode
                TRUE,  // flag for initial state 
                NULL // pointer to event-object name 
            ); // set when thread is suspended 
      assert(p_vmthread->resume_event);
   
    }
#ifdef PLATFORM_POSIX
    sem_init(&p_vmthread->yield_other_sem,0,0);
#endif
    p_vmthread->p_active = p_active_threads_list;
    p_active_threads_list = p_vmthread;
    p_vmthread->thread_index = thread_index;
    quick_thread_id[thread_index].p_vmthread = p_vmthread;
    mon_enter_array[thread_index].p_thr = p_vmthread; 
    mon_wait_array[thread_index].p_thr = p_vmthread; 
    p_vmthread->native_handles = 0;

    return p_vmthread;
}

#ifdef RECOMP_THREAD
VM_thread* new_a_thread_block()
{ //::
    VM_thread* p_vmthread = (VM_thread *)thr_pool.alloc(sizeof(VM_thread));
    TRACE2("thread", "new_a_thread_block() created new thread block " << p_vmthread);
        memset(p_vmthread, 0, sizeof(VM_thread) );

        p_vmthread->park_event = CreateEvent( 
               NULL,   // pointer to security attributes 
               FALSE,  // flag for manual-reset event  -- auto reset mode 
               FALSE,  // flag for initial state 
               NULL    // pointer to event-object name 
           ); 

        // If new thread block is being created, set app_status to "birthing" here too.
        p_vmthread->app_status = thread_is_birthing;

        p_vmthread->event_handle_monitor = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- auto reset mode 
                FALSE,  // flag for initial state 
                E_H_M   // pointer to event-object name 
            ); 
        assert(p_vmthread->event_handle_monitor);

        p_vmthread->event_handle_sleep = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- auto reset mode 
                FALSE,  // flag for initial state 
                E_H_S   // pointer to event-object name 
            ); 
        assert(p_vmthread->event_handle_sleep);

        p_vmthread->event_handle_notify_or_interrupt = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- auto reset mode 
                FALSE,  // flag for initial state 
                E_H_I   // pointer to event-object name 
            ); 
        assert(p_vmthread->event_handle_notify_or_interrupt);

        p_vmthread->jvmti_resume_event_handle = vm_create_event( 
                NULL,   // pointer to security attributes 
                TRUE,  // flag for manual-reset event  -- manual mode
                TRUE,  // flag for initial state 
                J_V_M_T_I_E_H // pointer to event-object name 
            ); 
        assert(p_vmthread->jvmti_resume_event_handle);

    //park semaphore initialization
    
    // ============== new SUSPEND related variables setup =====================
    p_vmthread->suspend_request = 0;
    p_vmthread->suspended_event = vm_create_event( 
                NULL,   // pointer to security attributes 
                TRUE,  // flag for manual-reset event  -- manual mode
                FALSE,  // flag for initial state 
                E_H_S0 // pointer to event-object name 
            ); // set when thread is suspended 
            
      assert(p_vmthread->suspended_event);

     p_vmthread->resume_event = vm_create_event( 
                NULL,   // pointer to security attributes 
                FALSE,  // flag for manual-reset event  -- manual mode
                TRUE,  // flag for initial state 
                NULL // pointer to event-object name 
            ); // set when thread is suspended 
      assert(p_vmthread->resume_event);
#ifdef PLATFORM_POSIX
    sem_init(&p_vmthread->yield_other_sem,0,0);
#endif
    return p_vmthread;
}
#endif

void free_this_thread_block(VM_thread *p_vmthread)
{
    assert(quick_thread_id[p_vmthread->thread_index].p_vmthread == p_vmthread);

    VM_thread *p_thr = p_active_threads_list;
    VM_thread *p_old_thr = p_active_threads_list;

    // pull thread out of active threads list
    if (p_thr == p_vmthread) 
        p_active_threads_list = p_thr->p_active;

    else {
        while (1) {
            if(p_thr == p_vmthread) break;
            p_old_thr = p_thr;
            p_thr = p_thr->p_active;
        }
        assert(p_thr);
        assert(p_old_thr);
        p_old_thr->p_active = p_thr->p_active;
    }

    // put it back in the free threads pool
    quick_thread_id[p_vmthread->thread_index].p_vmthread = 0;
    mon_enter_array[p_vmthread->thread_index].p_thr = NULL;
    mon_wait_array[p_vmthread->thread_index].p_thr = NULL;

    // copy the handles into a temporary place 
    // and put them back after zeroing whole data struct
    VmEventHandle aa = p_vmthread->event_handle_monitor;
    VmEventHandle bb = p_vmthread->event_handle_sleep;
    VmEventHandle cc = p_vmthread->event_handle_notify_or_interrupt;
    VmEventHandle ff = p_vmthread->jvmti_resume_event_handle;
    VmEventHandle jj = p_vmthread->suspended_event; 
    VmEventHandle hh = p_vmthread->resume_event;
    VmEventHandle gg = p_vmthread->park_event;
 #ifdef PLATFORM_POSIX
    sem_destroy(&p_vmthread->yield_other_sem);
#endif 
  
    memset(p_vmthread, 0, sizeof(VM_thread) );
  
    vm_reset_event(gg);
    p_vmthread->park_event = gg;

    vm_reset_event(aa);
    p_vmthread->event_handle_monitor = aa;
    vm_reset_event(bb);
    p_vmthread->event_handle_sleep = bb;
    vm_reset_event(cc);
    p_vmthread->event_handle_notify_or_interrupt = cc;
    vm_reset_event(ff);
    vm_set_event(ff);
    p_vmthread->jvmti_resume_event_handle = ff;
    vm_reset_event(jj);
    p_vmthread->suspended_event = jj; 
    vm_reset_event(hh);
    vm_set_event(hh);
    p_vmthread->resume_event = hh;

#ifdef PLATFORM_POSIX
    sem_init(&p_vmthread->yield_other_sem,0,0);
#endif 

    p_vmthread->p_free = p_free_thread_blocks;
    p_free_thread_blocks = p_vmthread;

}

void tmn_thread_attach()
{
    VM_thread *p_vmthread;
    p_vmthread = get_a_thread_block();
    VERIFY(p_vmthread, "Thread attach failure: can't get a thread block");
// FIXME: following code should be incapsulated into thread manager component. 
#ifdef PLATFORM_POSIX 
    p_vmthread->thread_handle = GetCurrentThreadId();
    p_vmthread->thread_id = GetCurrentThreadId();
#else //PLATFORM_POSIX
   int UNUSED stat =
        DuplicateHandle(GetCurrentProcess(),        // handle to process with handle to duplicate
                GetCurrentThread(),                 // handle to duplicate
                GetCurrentProcess(),                // handle to process to duplicate to
               // gashiman - Duplicate handle does not do anything on linux
               // so it is safe to put type convertion here
                (VmEventHandle*)(&(p_vmthread->thread_handle)),   // pointer to duplicate handle
                0,                              // access for duplicate handle
                false,                              // handle inheritance flag
                DUPLICATE_SAME_ACCESS               // optional actions
        );
    assert(stat);
#endif //!PLATFORM_POSIX

    set_TLS_data(p_vmthread);
} //init_thread_block
void tm_acquire_tm_lock(){
    p_thread_lock->_lock();
}

void tm_release_tm_lock(){
    p_thread_lock->_unlock();
}

bool tm_try_acquire_tm_lock(){
    return p_thread_lock->_tryLock();
}

tm_iterator_t * tm_iterator_create()
{
    assert(tmn_is_suspend_enabled());
    tm_acquire_tm_lock();
    tm_iterator_t * iterator = (tm_iterator_t *) malloc(sizeof(tm_iterator_t));
    assert(iterator); 
    iterator->current = p_active_threads_list;
    iterator->init = true;
    return iterator;
}

int tm_iterator_release(tm_iterator_t * iterator)
{
    free(iterator);
    tm_release_tm_lock();
    return 0;
}

int tm_iterator_reset(tm_iterator_t * iterator)
{
    iterator->current = p_active_threads_list;
    iterator->init = true;
    return 0;
}

VM_thread * tm_iterator_next(tm_iterator_t * iterator)
{
    if (iterator->init) {
        iterator->init = false;
    } else if (iterator->current) {
        iterator->current = iterator->current->p_active;
    }
    return iterator->current;
}

