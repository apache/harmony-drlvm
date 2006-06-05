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

#define LOG_DOMAIN "enumeration"
#include "cxxlog.h"

#include "exceptions.h"
#include "mon_enter_exit.h"
#include "environment.h"
#include "thread_generic.h"
#include "vm_synch.h"
#include "port_atomic.h"

static void vm_monitor_exit_default(ManagedObject *p_obj);
static void vm_monitor_exit_default_handle(jobject jobj);

void (*vm_monitor_enter)(ManagedObject *p_obj) = 0;
void (*vm_monitor_exit)(ManagedObject *p_obj) = 0;
void (*vm_monitor_exit_handle)(jobject jobj) = 0;

volatile int active_thread_count;

mon_enter_fields mon_enter_array[MAX_VM_THREADS];

static void jvmti_push_monitor(jobject jobj);
static void jvmti_pop_monitor(jobject jobj);

#if defined(USE_TLS_API) || !defined(STACK_BASE_AS_TID)
uint16 get_self_stack_key()
{
    return p_TLS_vmthread->stack_key;
}
#endif //#ifdef USE_TLS_API || !STACK_BASE_AS_TID

void block_on_mon_enter(jobject obj)
{
    assert(!tmn_is_suspend_enabled());
    ManagedObject *p_obj = obj->object;
    VM_thread *p_thr = get_thread_ptr();

    assert(mon_enter_array[p_thr->thread_index].p_obj == 0);

    // set thread state to BLOCKED on monitor
    // RUNNING will be set back before return
    enum java_state old_state =  p_thr->app_status;
    p_thr->app_status = thread_is_blocked;
    
#ifdef _DEBUG
    int loop_count = 0;
#endif

    //since p_obj may be moved by GC in the below loop, we need to reload after gc_enable/disable
    volatile ManagedObject *volatile_p_obj = (ManagedObject *)p_obj;

    while (1) {

        mon_enter_array[p_thr->thread_index].p_obj = (ManagedObject *)volatile_p_obj;

#ifdef _DEBUG
        loop_count++;
        if (loop_count > max_block_on_mon_enter_loops)
            max_block_on_mon_enter_loops = loop_count;
#endif
        if ( STACK_KEY(volatile_p_obj) == FREE_MONITOR)
        {
            mon_enter_array[p_thr->thread_index].p_obj = 0;

            Boolean retval = vm_try_monitor_enter( (ManagedObject *)volatile_p_obj);
            if (retval){
                assert(p_thr->app_status == thread_is_blocked); 
                p_thr->app_status = old_state;
                return;
            }
            else continue;
        }

        tmn_suspend_enable(); 
        p_thr -> is_suspended = true;

        // no one sends this event
        // DWORD stat = vm_wait_for_single_object(p_thr->event_handle_monitor, 1);
        vm_yield();

        p_thr -> is_suspended = false;
        tmn_suspend_disable();

        // by convention, *only* mon_enter_array and mon_wait_array will be enumerated to the GC
        // thus reload from mon_enter_array[] after the waitforsingleobject returns
        ManagedObject* p_obj = (ManagedObject *)
                            mon_enter_array[p_thr->thread_index].p_obj;
        assert(managed_object_is_valid(p_obj));
        volatile_p_obj = p_obj;

    } //while(1)
}


void find_an_interested_thread(ManagedObject *p_obj)
{
    assert(!tmn_is_suspend_enabled());
    if ( ( HASH_CONTENTION(p_obj) & CONTENTION_MASK) == 0)
        return;  // nobody wants this object

    DWORD stat;
    int xx;

    for (xx = 1; xx < next_thread_index; xx++)
    {
        if (mon_enter_array[xx].p_obj == p_obj)
        {
            stat = vm_set_event(mon_enter_array[xx].p_thr->event_handle_monitor);
            assert(stat);

            return;
        }
    }
}


void vm_enumerate_root_set_mon_arrays()
{
    TRACE2("enumeration", "enumerating root set monitor arrays");
    int xx;

    for (xx = 1; xx < next_thread_index; xx++)
    {
        if (mon_enter_array[xx].p_obj){
            assert( (STACK_KEY( mon_enter_array[xx].p_obj)) != mon_enter_array[xx].p_thr->stack_key );
            vm_enumerate_root_reference((void **)&(mon_enter_array[xx].p_obj), FALSE);
        }

        if (mon_wait_array[xx].p_obj){
            assert( mon_wait_array[xx].p_thr->app_status == thread_is_waiting || mon_wait_array[xx].p_thr->app_status == thread_is_timed_waiting);
            vm_enumerate_root_reference((void **)&(mon_wait_array[xx].p_obj), FALSE);
        }
    }
}

void mon_enter_recursion_overflowed(ManagedObject * UNREF p_obj)
{
    ABORT("Not implemented"); //TODO: add the backup algorithm for recursion overflow

}

void vm_monitor_init()
{
    vm_monitor_enter = vm_monitor_enter_slow;
    vm_monitor_exit = vm_monitor_exit_default;
    vm_monitor_exit_handle = vm_monitor_exit_default_handle;
}

inline void pause_inst(void){
#if defined(PLATFORM_POSIX) && !defined(_IPF_)
    __asm__(
#if !((__GLIBC__ == 2) && (__GLIBC_MINOR__ <= 1))
        "pause"
#else
        "nop;nop;nop"
#endif
        : : : "memory"
    );

#else
#ifdef _IPF_
    // WE WILL DO NOTHING HERE FOR THE TIME BEING...until we can find a suitable & profitable IPF instruction...
#else  // !_IPF_
    _asm{ 
        pause
    }
#endif // !_IPF_

#endif

}


//return the old monitor stack key, so that 0 means success, otherwise failure;
Boolean vm_try_monitor_enter(ManagedObject *p_obj)
{
 
    uint16 current_stack_key = get_self_stack_key();
    uint16 *p_monitor_stack_key = P_STACK_KEY(p_obj);
    uint16 old_stack_key = FREE_MONITOR;
    old_stack_key = port_atomic_cas16(p_monitor_stack_key, current_stack_key, FREE_MONITOR);
    if( old_stack_key == FREE_MONITOR ){    
        
        return TRUE;

    } else if (old_stack_key == current_stack_key) { //recursed
        ++RECURSION(p_obj);
        if (0 == RECURSION(p_obj)) {
            mon_enter_recursion_overflowed(p_obj);
        }
        return TRUE;
    }

    //hold by other thread
    return FALSE;
}

static inline uint8 get_recursion(jobject jobj) {
    assert(tmn_is_suspend_enabled());
    tmn_suspend_disable(); // ----------- vv
    uint8 recursion = RECURSION(jobj->object);
    tmn_suspend_enable(); // ------------ ^^
    return recursion;
}

static inline uint16 get_stack_key(jobject jobj) {
    assert(object_is_valid(jobj));
    tmn_suspend_disable(); // ----------vv
    uint16 *p_stack_key = P_STACK_KEY(jobj->object);
    uint16 result = *p_stack_key;
    tmn_suspend_enable(); // -----------^^
    return result;
}

static inline uint16 compare_swap_stack_key(jobject jobj, uint16 value, uint16 expected) {
    assert(object_is_valid(jobj));
    tmn_suspend_disable(); // ----------vv
    uint16 *p_stack_key = P_STACK_KEY(jobj->object);
    uint16 old = port_atomic_cas16(p_stack_key, value, expected);
    tmn_suspend_enable(); // -----------^^
    return old;
}

// this is a first step of a bigger refactoring
// of VM code to run with gc enabled.
//
// this function should be called instead of vm_monitor_enter_slow
// NOTE, that only ia32 stubs were modified so.
//
// -salikh 2005-05-11
VMEXPORT void vm_monitor_enter_slow_handle (jobject jobj) {
    TRACE2("oh", "vm_monitor_enter_slow_handle()");
    assert(tmn_is_suspend_enabled());
    assert(object_is_valid(jobj));
    
    uint16 current_stack_key = get_self_stack_key();
    uint16 old_stack_key = get_stack_key(jobj);
    
    if (old_stack_key == FREE_MONITOR) { //common case, fastest
        old_stack_key = compare_swap_stack_key(jobj, current_stack_key, FREE_MONITOR);
        if (old_stack_key == FREE_MONITOR) { //ok, got it
            jvmti_push_monitor(jobj);
            return;
        }
        // failed to grab the lock, fall through to contended case
    } else if (old_stack_key == current_stack_key) { //recursed
        tmn_suspend_disable(); // ---------------vv
        ++RECURSION(jobj->object);
        int recursion = RECURSION(jobj->object);
        tmn_suspend_enable(); // ----------------^^
        if (0 == recursion) {
            tmn_suspend_disable(); // -----------vv
            mon_enter_recursion_overflowed(jobj->object);
            tmn_suspend_enable(); // ------------^^
        }
        return;
    }

    //hold by other thread, let's spin look for a short while

    unsigned int i = SPIN_LOOP_COUNT;
    while (i--) {
        pause_inst();
        if (0 == get_stack_key(jobj)) { //monitor is free now, try to get it
            old_stack_key = compare_swap_stack_key(jobj, current_stack_key, FREE_MONITOR);
            if (old_stack_key == FREE_MONITOR) { //ok, got it
                jvmti_push_monitor(jobj);
                return;
            }
        }
    }

    bool isEnter = true;
    jvmti_send_contended_enter_or_entered_monitor_event(jobj, isEnter);
    
    // we have no way to get the lock, then sleep; 
    tmn_suspend_disable();
    block_on_mon_enter(jobj);
    tmn_suspend_enable();

    assert(object_is_valid(jobj));
    if (get_stack_key(jobj) != current_stack_key) {
        DIE("Wrong monitor states after GC: " << jobj << ")");
    }

    return;
}

void vm_monitor_enter_slow(ManagedObject *p_obj)
{
    assert(!tmn_is_suspend_enabled());
    assert(p_obj);
    jobject jobj = oh_allocate_local_handle();
    jobj->object = p_obj;
    tmn_suspend_enable();    // we've protected the argument
    vm_monitor_enter_slow_handle(jobj);
    tmn_suspend_disable();   // restore suspend enabled state
}

static void vm_monitor_exit_default_handle(jobject jobj) {
    assert(object_is_valid(jobj));

    uint16 current_stack_key = get_self_stack_key();

    uint16 monitor_stack_key = get_stack_key(jobj);
    if (monitor_stack_key == current_stack_key) { //common case, fastest path
        if (!get_recursion(jobj)) { //no recursion
            tmn_suspend_disable(); // ----------------------vv
            STACK_KEY(jobj->object) = FREE_MONITOR; //release the lock
            if (HASH_CONTENTION(jobj->object) & CONTENTION_MASK) { //contented
                find_an_interested_thread(jobj->object);
            }
            tmn_suspend_enable(); // -----------------------^^
            jvmti_pop_monitor(jobj);
        } else { //recursed
            tmn_suspend_disable(); // ----------------------vv
            RECURSION(jobj->object)--; 
            tmn_suspend_enable(); // -----------------------^^
        }
        
    } else { //illegal monitor state
        exn_throw_by_name("java/lang/IllegalMonitorStateException");
    }

    assert(tmn_is_suspend_enabled());
}



static void vm_monitor_exit_default(ManagedObject *p_obj)
{
    assert(managed_object_is_valid(p_obj));
    jobject jobj = oh_allocate_local_handle();
    jobj->object = p_obj;
    tmn_suspend_enable();
    vm_monitor_exit_default_handle(jobj);
    tmn_suspend_disable();
}

//#endif // !_IPF_

void set_hash_bits(ManagedObject *p_obj)
{
    uint8 hb = (uint8) (((POINTER_SIZE_INT)p_obj >> 3) & HASH_MASK)  ;
    // lowest 3 bits are not random enough so get rid of them

    if (hb == 0)
        hb = (23 & HASH_MASK);  // NO hash = zero allowed, thus hard map hb = 0 to a fixed prime number

    // don't care if the cmpxchg fails -- just means someone else already set the hash
    port_atomic_cas8(P_HASH_CONTENTION(p_obj),hb, 0);
}


// returns true if the object has its monitor taken...
// asserts if the object header is ill-formed.
// returns false if object's monitor is free.
//
// ASSUMPTION -- CAVEAT -- Should be called only during stop the world GC...
//
//

Boolean verify_object_header(void *ptr)
{
    ManagedObject *p_obj = (ManagedObject *) ptr;
    if (p_obj->get_obj_info() == 0) {
        // ZERO header is valid.
        return FALSE;
    }

    uint16 stack_key = STACK_KEY(p_obj);
    // Recursion can be any 8-bit value...no way to check integrity.
    // Contention bit can remain ON in our current implemenation at arbitrary times.
    // Hash can be zero or non-zero at any given time, though it doesnt change after it goes non-zero

    // Not much I can check here....can I????

    // So I will check only if the monitor lock is held and if so by a valid thread.

    if (stack_key == 0) {
        // No thread owns the object lock
        return FALSE;
    }

    // Object monitor is held.

    Boolean res = TRUE;
    tmn_suspend_enable();
    tm_iterator_t * iterator = tm_iterator_create();
    tmn_suspend_disable();
    VM_thread *thread = tm_iterator_next(iterator);
    assert(thread);
    
    while (thread) {
        if (thread->thread_index == stack_key) {
            res = TRUE;
            break;
        }
        thread = tm_iterator_next(iterator);
    }
    tm_iterator_release(iterator);
    // What should the return value be?
    return res;
}

static void jvmti_push_monitor(jobject jobj)
{
    assert(tmn_is_suspend_enabled());
    if (VM_Global_State::loader_env->TI->isEnabled()) {
        VM_thread *thread = p_TLS_vmthread;
        assert(thread -> jvmti_owned_monitor_count < MAX_OWNED_MONITORS);
        ObjectHandle oh = oh_allocate_global_handle();
        tmn_suspend_disable(); // ----------- vv
        oh -> object = jobj -> object;
        tmn_suspend_enable(); // ------------ ^^
        thread -> jvmti_owned_monitors[thread -> jvmti_owned_monitor_count++] = (jobject) oh;
    }
}

static void jvmti_pop_monitor(jobject UNREF jobj)
{
    assert(tmn_is_suspend_enabled());
    if (VM_Global_State::loader_env->TI->isEnabled()) {
        VM_thread *thread = p_TLS_vmthread;
        if (thread -> jvmti_owned_monitor_count <= 0) return;
        assert(thread -> jvmti_owned_monitor_count > 0);

        ObjectHandle oh = (ObjectHandle)
            thread -> jvmti_owned_monitors[--thread -> jvmti_owned_monitor_count];
        oh_deallocate_global_handle(oh);
    }
}

