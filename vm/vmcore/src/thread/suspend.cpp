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
 * @author Artem Aliev
 * @version $Revision: 1.1.2.3.4.4 $
 */  


#include "platform_lowlevel.h"
#ifdef PLATFORM_POSIX
#endif // PLATFORM_POSIX

#include "open/gc.h"
#include "jit_intf_cpp.h"
#include "method_lookup.h"
#include "vm_stats.h"
#include "vm_threads.h"
#include "thread_generic.h"
#include "open/thread.h"
#include "atomics.h"
#include "root_set_enum_internal.h"
#include "lock_manager.h"
#include "verify_stack_enumeration.h"

#include "open/vm_util.h"
#include "vm_process.h"
#include "cxxlog.h"

#define REDUCE_RWBARRIER 1

#if defined (PLATFORM_POSIX) && defined (_IPF_)
extern "C" void get_rnat_and_bspstore(uint64* res);
extern "C" void do_flushrs();
#endif

#ifdef REDUCE_RWBARRIER
#if defined (PLATFORM_POSIX)
#include <signal.h>
#endif
static void thread_yield_other(VM_thread*);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// The rules for when something can and can't be enabled or disabled:
//
// 1. Only the current thread (p_TLS_vmthread) can disable and enable GC.
// 2. You cannot enable GC from within JITTED code.
// 3. You can only enable if you are disabled.

// tmn_suspend_disable interface() assumes we are in enabled state (otherwise 
// tmn_suspend_disable_and_return_old_value() should be used)
// so it would be better to rename thread_enable_suspend() to vm_try_enabling_gc()
// to break incorrectly intuitive pair
//
// New interface: 
//     tmn_suspend_enable();
//     tmn_suspend_disable();
//     the thread_safe_point();


// this is internal function
// thread param should always be self thread (p_TLS_vmthread).

inline void thread_safe_point_impl(VM_thread *thread) { 
    // must be either suspend enabled (status == 0) or in managed code (status == 1)
    assert(thread->suspend_enabled_status <= 1);
    debug_stack_enumeration();
     if(thread->suspend_request >0) {   
        int old_status = thread->suspend_enabled_status;
        do {
            thread->suspend_enabled_status = 0;
            MemoryReadWriteBarrier();
             // code for Ipf that support StackIterator and immmediate suspend
            // notify suspender
            vm_set_event(thread->suspended_event);
            TRACE2("suspend", "suspended event... " << thread->thread_id);

            // wait for resume event
            vm_wait_for_single_object(thread->resume_event, INFINITE);

           TRACE2("suspend", "resuming after gc resume event" << thread->thread_id);
           thread->suspend_enabled_status = old_status;
           MemoryReadWriteBarrier();
        } while (thread->suspend_request >0);
     }
} // thread_safe_point_impl


void tmn_suspend_enable()
{
    assert(p_TLS_vmthread->suspend_enabled_status == 1);
    tmn_suspend_enable_recursive();
}

void tmn_suspend_enable_recursive()
{
    VM_thread *thread = p_TLS_vmthread;
        //TRACE2("suspend", "enable" << thread->suspend_enabled_status);
    assert(!tmn_is_suspend_enabled());
    debug_stack_enumeration();
    thread->suspend_enabled_status--;
       //assert(tmn_is_suspend_enabled());
    
//     MemoryReadWriteBarrier();
    if ((thread->suspend_request > 0)
        && (0 == thread->suspend_enabled_status)) {
            // notify suspender
            vm_set_event(thread->suspended_event);
            TRACE2("suspend", "suspended " << thread->thread_id);           
    }
}

int suspend_count = 0;
void tmn_suspend_disable()
{   
//  if (!tmn_is_suspend_enabled()) {
//      printf("asserted\n");
//        // the following lines allow MSVC++ "Debug->>>Break" to work
//        while (1) {
//            DWORD stat = vm_wait_for_single_object(NULL, 2000);
////            if (stat == WAIT_OBJECT_0) break;
////            assert(stat != WAIT_FAILED);
//          Sleep(2000);
//        }
//  }

    assert(tmn_is_suspend_enabled());
    tmn_suspend_disable_recursive();
}

void tmn_suspend_disable_recursive()
{   
#ifdef _DEBUG
    suspend_count++;
    if(suspend_count % 100000 == 0) 
        TRACE2("suspend", "suspend disable count: " << suspend_count);
#endif      
    VM_thread *thread = p_TLS_vmthread;
        //TRACE2("suspend", "disable: " << thread->suspend_enabled_status);
    thread->suspend_enabled_status++;
    debug_stack_enumeration();
#ifndef REDUCE_RWBARRIER
    MemoryReadWriteBarrier();
#endif
    if (1 == thread->suspend_enabled_status)
        thread_safe_point_impl(thread);
}


//FIXME replace through the code to tmn_suspend_disable();
bool tmn_suspend_disable_and_return_old_value() {
    /*if(tmn_is_suspend_enabled()) {
        tmn_suspend_disable();
        return true;
    }
    return false;*/
        //TRACE2("suspend", "disable if enable: " << p_TLS_vmthread->suspend_enabled_status);
    
    p_TLS_vmthread->suspend_enabled_status++;
    return  true;
}
 
VMEXPORT bool tmn_is_suspend_enabled() {
    assert(p_TLS_vmthread->suspend_enabled_status >=0);
    return (p_TLS_vmthread->suspend_enabled_status == 0);
}

// temporary for debug purpose
VMEXPORT int tmn_suspend_disable_count() {
    return (p_TLS_vmthread->suspend_enabled_status);
}

void tmn_safe_point() {
    thread_safe_point_impl(p_TLS_vmthread);
}

void rse_thread_resume(VM_thread *thread)
{  
    if(thread == p_TLS_vmthread) {
        return;
    }

    if(thread->suspend_request <=0) {
        LOG2("suspend", "resume allived thread: "  << thread->thread_id);
        return;
    }

   //decrement suspend_request
   if(--thread->suspend_request > 0) {
        return;
    }
    
    vm_set_event(thread->resume_event);  

    TRACE2("suspend", "resume " << thread->thread_id);
    thread -> jvmti_thread_state ^= JVMTI_THREAD_STATE_SUSPENDED;

} // rse_thread_resume

// the function start suspension.
// call wait_supend_respose() should be called to wait for safe region or safe point.
// the function do not suspend self.
static void send_suspend_request(VM_thread *t) {

        assert(t->suspend_request >=0);
        // already suspended?
        if(t->suspend_request > 0) {
            t->suspend_request++;
            return;
        }           
        
        //we realy need to suspend thread.

        vm_reset_event(t->resume_event);
        
        t->suspend_request++;
        MemoryReadWriteBarrier();
#ifdef REDUCE_RWBARRIER
       // ping other thread to do MemoryReadWriteBarrier()
       // this is done by sending signal on linux
       // or by SuspendThread();ResumeThread() on windows
       thread_yield_other(t);
#endif
        TRACE2("suspend", "suspend request " << t->thread_id);
}


// the second part of suspention
// blocked in case was selfsuspended.
static void wait_supend_respose(VM_thread *t) {
        if(t->suspend_request > 1) {
            return;
        }           
        if(t == p_TLS_vmthread) {
                TRACE2("suspend", "suspend self... skiped");
                return;
        }   

        // we need to wait for notification only in case the thread is in the unsafe/disable region
        while (t->suspend_enabled_status != 0) {
            // wait for the notification
            TRACE2("suspend", "wait for suspended_event " << t->thread_id);
            vm_wait_for_single_object(t->suspended_event, 50);
                // this is auto reset event 
                        //vm_reset_event(t->suspended_event);
        }
        TRACE2("suspend", "suspended " << t->thread_id);
        t -> jvmti_thread_state |= JVMTI_THREAD_STATE_SUSPENDED;
}


// GC suspend function
void suspend_all_threads_except_current() {
    VM_thread *self = p_TLS_vmthread;
    TRACE2("suspend", "suspend_all_threads_except_current() in thread: " << p_TLS_vmthread->thread_id);
    tm_acquire_tm_lock();
   // unlock mutex in case self was suspended by other thread
    // this will prevent us from cyclic dead-lock 
    if(self != NULL) { 
        while (self ->suspend_request > 0) {
                tm_release_tm_lock();
                TRACE2("suspend", "generic suspend safe_point " << self->thread_id);            
                thread_safe_point_impl(self);
                tm_acquire_tm_lock();   
        }
    }
    // Run through list of active threads and suspend each one of them.
    // We can do this safely because we hold the global thread lock
    VM_thread *t = p_active_threads_list;
    assert(t);

     for (;t;t = t->p_active) {
         if(t == self) {
                TRACE2("suspend", "skip enumerate self");
                continue;
        }   
        send_suspend_request(t);
     }
    
     for (t = p_active_threads_list;t;t = t->p_active) {
        if(t == self) {
                TRACE2("suspend", "skip enumerate self");
                continue;
        }   
        wait_supend_respose(t);
        t->gc_status = gc_at_safepoint;  
 
      }
     tm_release_tm_lock();
      
    TRACE2("suspend", "suspend_all_threads_except_current() complete");
}

void suspend_all_threads_except_current_generic() {
    TRACE2("suspend", "suspend_all_threads_except_current() in thread: " << p_TLS_vmthread->thread_id);
    p_thread_lock->_lock_enum();
   // unlock mutex in case self was suspended by other thread
    // this will prevent us from cyclic dead-lock 
    while (p_TLS_vmthread ->suspend_request > 0) {
                p_thread_lock->_unlock_enum();
                TRACE2("suspend", "generic suspend safe_point " << p_TLS_vmthread->thread_id);          
                thread_safe_point_impl(p_TLS_vmthread);
                p_thread_lock->_lock_enum();    
    }
    // Run through list of active threads and suspend each one of them.
    // We can do this safely because we hold the global thread lock
    VM_thread *t = p_active_threads_list;
    assert(t);

     for (;t;t = t->p_active) {
         if(t == p_TLS_vmthread) {
                TRACE2("suspend", "skip enumerate self");
                continue;
        }   
        send_suspend_request(t);
     }
    
     for (t = p_active_threads_list;t;t = t->p_active) {
        if(t == p_TLS_vmthread) {
                TRACE2("suspend", "skip enumerate self");
                continue;
        }   
        wait_supend_respose(t);
 
      }
     p_thread_lock->_unlock_enum();
      
    TRACE2("suspend", "suspend_all_threads_except_current() complete");
}

// GC resume function
void resume_all_threads() {
  VM_thread *self = p_TLS_vmthread;
  tm_acquire_tm_lock();
  TRACE2("suspend", " ====== resume all threads ==========" );
    
    VM_thread *t = p_active_threads_list;
    assert(t);


     for (;t;t = t->p_active) {
         if(t == self) {
                assert(t->gc_status == gc_enumeration_done);   
                t->gc_status = zero;
                continue;   
         }
        rse_thread_resume(t);
        assert(t->gc_status == gc_enumeration_done);   
        t->gc_status = zero;
        
    }
    tm_release_tm_lock();
    
}

void resume_all_threads_generic() {
  p_thread_lock->_lock_enum();
  TRACE2("suspend", " ====== resume all threads ==========" );
    
    VM_thread *t = p_active_threads_list;
    assert(t);


     for (;t;t = t->p_active) {
         if(t == p_TLS_vmthread) {
                continue;   
         }

         if ((t->suspend_request == 1 && t->gc_status != zero) // do no resume stoped by GC thread
                || t->suspend_request <=0) { // do not resume working thread 

            TRACE2("suspend", "generic resume failed " << t->thread_id << " gc " <<t->gc_status 
                        << " suspend_request " << t->suspend_request );
            continue;
        }
        
        rse_thread_resume(t);
    }
    p_thread_lock->_unlock_enum();
}

void 
thread_suspend_self() {
     TRACE2("suspend", "suspend_self called" );
    VM_thread *thread = p_TLS_vmthread;
    assert(thread);
    tm_acquire_tm_lock();
    send_suspend_request(thread);
    tm_release_tm_lock();
    
    assert(tmn_is_suspend_enabled());
    assert(thread->suspend_request > 0);
    thread_safe_point_impl(thread);
}

bool
thread_suspend_generic(VM_thread *thread)
{  
   VM_thread *self = p_TLS_vmthread;
    // suspend self
    if(thread == NULL || thread == self) {
        TRACE2("suspend", "suspend self " << (thread == NULL?0:thread->thread_id));     
        thread_suspend_self();
        return true;
    }
    
    assert(tmn_is_suspend_enabled());
    tm_acquire_tm_lock();   
 
   // unlock mutex in case self was suspended by other thread
    // this will prevent us from cyclic dead-lock 
    if(self != NULL) {
        while (self ->suspend_request > 0) {
                tm_release_tm_lock();
                TRACE2("suspend", "generic suspend safe_point " << self->thread_id);            
                thread_safe_point_impl(self);
                tm_acquire_tm_lock();   
        }
    }
    TRACE2("suspend", "generic suspend " << thread->thread_id);

    send_suspend_request(thread);
    wait_supend_respose(thread);
    tm_release_tm_lock();
  return true;
} // thread_suspend_generic


bool
thread_resume_generic(VM_thread *t)
{ 
  if(t == NULL || t == p_TLS_vmthread) {
         TRACE2("suspend", "generic resume self failed ");
        return false;
  }
  assert(tmn_is_suspend_enabled());
  TRACE2("suspend", "generic resume " << t->thread_id);
  
  tm_acquire_tm_lock();
  assert(t);
  assert(t->suspend_request >=0);
  if ((t->suspend_request == 1 && t->gc_status != zero) // do no resume stoped by GC thread
            || t->suspend_request <=0) { // do not resume working thread 

      TRACE2("suspend", "generic resume failed " << t->thread_id << " gc " <<t->gc_status 
                        << " suspend_request " << t->suspend_request );
      tm_release_tm_lock();
      return false;
  } 

  rse_thread_resume(t);
  tm_release_tm_lock();
  return true;
} // thread_resume_generic

void jvmti_thread_resume(VM_thread *thread)
{ 
    thread_resume_generic(thread);
} // jvmti_thread_resume


#ifdef REDUCE_RWBARRIER
// touch thread to flash memory
static void thread_yield_other(VM_thread* thread) {
    assert(thread);
    TRACE2("suspend", "yield from thread: " << p_TLS_vmthread->thread_id << "to: " << thread->thread_id);
    // use signals on linux
    #ifdef PLATFORM_POSIX
        // IPF compiler generate  st4.rel ld4.acq for valatile variables
        // so nothing need to do.
        #ifndef _IPF_
            assert(thread->thread_id);
            pthread_kill(thread->thread_id, SIGUSR2);
            sem_wait(&thread->yield_other_sem);
        #endif
    // use SuspendThread on windows
    #else
       SuspendThread(thread->thread_handle);
       ResumeThread(thread->thread_handle);
    #endif
}
#endif

//  depricated functions
uint32 thread_gc_number_of_threads()
{
    uint32 xx = 0;

    tmn_suspend_enable(); // to make tm_iterator_create()happy: it uses assert(tmn_is_suspend_enabled());
    tm_iterator_t * iterator = tm_iterator_create();
    tmn_suspend_disable();

    VM_thread *thread = tm_iterator_next(iterator);
    while (thread != NULL) {
        xx++;
        thread = tm_iterator_next(iterator);
    }
    tm_iterator_release(iterator);

    // do NOT enumerate the current thread thus its
    // return xx - 1; instead of return xx;
    // setup iterator for thread_gc_enumerate_one();
    p_threads_iterator = p_active_threads_list; 

    return xx - 1;
}

//TODO change to iterator
VM_thread *thread_gc_enumerate_one()
{
        //depricated
 
    assert(p_threads_iterator);

    // skip over the current thread which is already at
    // a gc safepoint.  Also, doing a Get/SetThreadContext()
    // on the current thread will cause a crash (for good reason)

    if(p_TLS_vmthread == p_threads_iterator)
        p_threads_iterator = p_threads_iterator->p_active;

    VM_thread *p_cookie = p_threads_iterator;
    if(p_threads_iterator)
        p_threads_iterator = p_threads_iterator->p_active;

    return p_cookie;
}
