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
 * @version $Revision: 1.1.2.2.4.5 $
 */  


#define LOG_DOMAIN "notify"
#include "cxxlog.h"

#include "platform_lowlevel.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;


#include "lock_manager.h"
#include "open/types.h"
#include "environment.h"
#include "exceptions.h"
#include "jni_utils.h"
#include "native_utils.h"
#include "vm_arrays.h"

#include "thread_generic.h"
#include "open/thread.h"
#include "thread_manager.h"
#include "object.h"
#include "object_generic.h"
#include "mon_enter_exit.h"

#include "jit_runtime_support_common.h"

#include "vm_process.h"
#include "interpreter.h"
#include "vm_log.h"

mon_wait_fields mon_wait_array[MAX_VM_THREADS];

#ifdef _DEBUG
// loose, approximate counts, full of race conditions
int max_notifyAll = 0;
int max_recursion = 0;
int total_sleep_timeouts = 0;
int total_sleep_interrupts = 0;
int total_wait_timeouts = 0;
int total_wait_interrupts = 0;
int total_illegal_mon_state_exceptions = 0;
int iterations_per_thread[MAX_VM_THREADS];
int max_block_on_mon_enter_loops = 0;
#endif

static inline bool object_locked_by_current_thread(jobject jobj) {
    uint16 current_stack_key = get_self_stack_key();
    bool result = false;
    assert(!tmn_is_suspend_enabled());
    result = (STACK_KEY(jobj->object) == current_stack_key);
    TRACE2("stack_key", "self = " << current_stack_key 
        << ", locked by " << STACK_KEY(jobj->object)
        << ", result = " << result);
    return result;
}

void notify_internal (jobject jobj, Boolean all)
{

    tmn_suspend_disable_recursive();
    assert(jobj->object->vt()->clss->vtable->clss->vtable);
 
   if (!object_locked_by_current_thread(jobj)) 
    {
        tmn_suspend_enable_recursive(); 
        exn_raise_by_name("java/lang/IllegalMonitorStateException");
        return;
    }

#ifdef _DEBUG
    int notifyAll_count = 0;
#endif

    DWORD stat;
    int xx;

    for (xx = 1; xx < next_thread_index; xx++)
    {
        if (mon_wait_array[xx].p_obj == jobj->object)
        {
            stat = vm_set_event(mon_wait_array[xx].p_thr->event_handle_notify_or_interrupt);
#ifdef _DEBUG
            if(!stat) {
                 WARN("set notify event failed: " << IJGetLastError());
            }
#endif
            if (!all)
                break;

#ifdef _DEBUG
            notifyAll_count++;
            if (notifyAll_count > max_notifyAll)
                max_notifyAll = notifyAll_count;
#endif
        }
    }
    tmn_suspend_enable_recursive(); 
}

void thread_object_notify (jobject jobj)
{
    java_lang_Object_notify (jobj);
}
void java_lang_Object_notify (jobject jobj)
{
    TRACE2("notify", "Notify " << p_TLS_vmthread->thread_handle << " on " << jobj);
    //assert(tmn_is_suspend_enabled());

    notify_internal(jobj, false);

    //assert(tmn_is_suspend_enabled());
    TRACE2("notify", "Finish Notify " << p_TLS_vmthread->thread_handle << " on " << jobj);
}

void thread_object_notify_all (jobject jobj)
{
    java_lang_Object_notifyAll (jobj);
}

void java_lang_Object_notifyAll (jobject jobj)
{
    TRACE2("notify", "Notify all " << p_TLS_vmthread->thread_handle << " on " << jobj);
 
    notify_internal(jobj, true);

    TRACE2("notify", "Finish notify all " << p_TLS_vmthread->thread_handle << " on " << jobj);
}

void thread_object_wait_nanos(jobject jobj, int64 msec, int nanosec) {
    /*
	 * We have to do something in the case millis == 0 and nanos > 0,
	 * because otherwise we'll wait infinitely, also just return whould 
	 * be incorrect because we have to relese lock for a while:
	 */
    msec += (msec==0 && nanosec>0)?1:0;
    java_lang_Object_wait(jobj, msec);
}

void thread_object_wait(jobject jobj, int64 msec)
{
    java_lang_Object_wait(jobj, msec);
}
int java_lang_Object_wait(jobject jobj, int64 msec)
{
    assert(tmn_is_suspend_enabled());
 
    // ? 2003-06-23.  The lock needs to be unreserved.
    if (msec < 0) 
    {
        exn_raise_by_name("java/lang/IllegalArgumentException");
        return -1;
    }

    assert(object_is_valid(jobj));
    tmn_suspend_disable();

    if (!object_locked_by_current_thread(jobj)) 
    {
        tmn_suspend_enable(); 
        exn_raise_by_name("java/lang/IllegalMonitorStateException");
        return -1;
    }
 
    VM_thread *p_thr = get_thread_ptr();
    DWORD stat;
    // toss stale notifies
    stat = vm_reset_event(p_thr->event_handle_notify_or_interrupt);
 
    // check prior interrupts.
    if (p_thr->interrupt_a_waiting_thread)
    {
        p_thr->interrupt_a_waiting_thread = false;
        tmn_suspend_enable();
        exn_raise_by_name("java/lang/InterruptedException");

        return -1;
    }
    
    tmn_suspend_enable(); 
    assert(tmn_is_suspend_enabled());
    tm_acquire_tm_lock(); 
    assert(p_thr->app_status == thread_is_running);
    if(msec == 0 /*&& nanos == 0 // added for nanos support*/) {
        p_thr->app_status = thread_is_waiting;
    } else {
        p_thr->app_status = thread_is_timed_waiting;
    }
    tm_release_tm_lock();
    tmn_suspend_disable(); 
    assert(p_thr->notify_recursion_count == 0);
    assert(mon_wait_array[p_thr->thread_index].p_obj == 0);
    mon_wait_array[p_thr->thread_index].p_obj = jobj->object;
    p_thr->notify_recursion_count = RECURSION(jobj->object);
    RECURSION(jobj->object) = 0;  // leave the recursion at zero for the next lock owner

#ifdef _DEBUG
    if (p_thr->notify_recursion_count > max_recursion)
        max_recursion = p_thr->notify_recursion_count;
#endif

     STACK_KEY(jobj->object) = FREE_MONITOR; // finally, give up the lock


    find_an_interested_thread(jobj->object);
    tmn_suspend_enable();

    TRACE2("wait", "thread going to wait " << p_TLS_vmthread->thread_handle);

    assert(tmn_is_suspend_enabled());
    p_thr -> is_suspended = true;
    if (msec == 0)
    {
        stat = vm_wait_for_single_object(p_thr->event_handle_notify_or_interrupt, INFINITE);
        assert(stat == WAIT_OBJECT_0);
    }
    else
    {
#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif
        stat = vm_wait_for_single_object(p_thr->event_handle_notify_or_interrupt, (int)msec); 

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

        assert( (stat == WAIT_OBJECT_0) || (stat == WAIT_TIMEOUT) );
    }
    p_thr -> is_suspended = false;

    TRACE2("wait", "thread finished waiting " << p_TLS_vmthread->thread_handle);

#ifdef _DEBUG
    if (stat == WAIT_OBJECT_0)
        total_wait_interrupts++;
    else
        total_wait_timeouts++;
#endif

    assert(object_is_valid(jobj));
    tmn_suspend_disable();
    //GC may have moved the object sitting in mon_wait_array[], thus reload it
    assert(mon_wait_array[p_thr->thread_index].p_obj->vt()->clss->vtable->clss->vtable);
    assert(mon_wait_array[p_thr->thread_index].p_obj == jobj->object); // jobj should have been updated too -salikh
    
    mon_wait_array[p_thr->thread_index].p_obj = 0;
    //do we need to do a p4 sfence here?
    // lock will do sfence.
    tmn_suspend_enable();
    
    assert(tmn_is_suspend_enabled());
    tm_acquire_tm_lock();

    assert(p_thr->app_status == thread_is_waiting || p_thr->app_status == thread_is_timed_waiting);
    p_thr->app_status = thread_is_running;

    // now we need to re-acquire the object lock
    // since block_on_mon_enter() can block, reload p_obj
    tm_release_tm_lock(); //with gc_disabled xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    tmn_suspend_disable();

    block_on_mon_enter(jobj);
    assert(jobj->object->vt()->clss->vtable->clss->vtable);

    assert(object_locked_by_current_thread(jobj));

    // re-install the recursion count
    assert(RECURSION(jobj->object) == 0);
    RECURSION(jobj->object) = (uint8)p_thr->notify_recursion_count;

    p_thr->notify_recursion_count = 0;

    tmn_suspend_enable(); 
    if ( (stat == WAIT_OBJECT_0) && (p_thr->interrupt_a_waiting_thread == true) )
    {
        p_thr->interrupt_a_waiting_thread = false;
        exn_raise_by_name("java/lang/InterruptedException");
    }

    return stat;

    // ready to return to java code 
}


long generic_hashcode(ManagedObject * p_obj)
{
    if (!p_obj) return 0L;
    if ( HASH_CONTENTION(p_obj) & HASH_MASK)
        return HASH_CONTENTION(p_obj) & HASH_MASK;

    set_hash_bits(p_obj);

    if ( HASH_CONTENTION(p_obj) & HASH_MASK)
        return HASH_CONTENTION(p_obj) & HASH_MASK;    
    
    ASSERT(0, "All the possible cases are supposed to be covered before");
    return 0xff;
}


jint object_get_generic_hashcode(JNIEnv*, jobject jobj)
{
    tmn_suspend_disable();
    ManagedObject* p_obj;
    if (jobj != NULL) {
        p_obj = ((ObjectHandle)jobj)->object;
    } else {
        p_obj = NULL;
    }
    jint hash = generic_hashcode(p_obj);
    tmn_suspend_enable(); 
    return hash;
}

#define NEW_ARRAY(X) new_arr = jenv->New##X##Array(length); assert(new_arr);        

#define GET_ARRAY_ELEMENTS(X, Y) src_bytes = jenv->Get##X##ArrayElements((j##Y##Array)jobj, &is_copy); \
    dst_bytes = jenv->Get##X##ArrayElements((j##Y##Array)new_arr, &is_copy);

#define MEMCPY(X) tmn_suspend_disable(); memcpy(dst_bytes, src_bytes, sizeof(j##X) * length); tmn_suspend_enable();

#define RELEASE_ARRAY_ELEMENTS(X, Y) jenv->Release##X##ArrayElements((j##Y##Array)new_arr, (j##Y *) dst_bytes, 0); \
    jenv->Release##X##ArrayElements((j##Y##Array)jobj, (j##Y *)src_bytes, JNI_ABORT);

#define HANDLE_TYPE_COPY(A, B) NEW_ARRAY(A) GET_ARRAY_ELEMENTS(A, B) MEMCPY(B) RELEASE_ARRAY_ELEMENTS(A, B)

jobject object_clone(JNIEnv *jenv, jobject jobj)
{
    ManagedObject *result;
    assert(tmn_is_suspend_enabled());
    if(!jobj) {
        // Throw NullPointerException.
        throw_exception_from_jni(jenv, "java/lang/NullPointerException", 0);
        return NULL;
    }
    tmn_suspend_disable();
    ObjectHandle h = (ObjectHandle) jobj;
    VTable *vt = h->object->vt();
    unsigned size;
    if (get_prop_array(vt->class_properties))
    {
        // clone an array
        int32 length = get_vector_length((Vector_Handle) h->object);
        size = vm_array_size(vt, length);
        result = (ManagedObject *) 
            vm_new_vector_using_vtable_and_thread_pointer(
                vt->clss->allocation_handle, length, vm_get_gc_thread_local());
    }
    else
    {
        // clone an object
        Global_Env *global_env = VM_Global_State::loader_env;
        if (!class_is_subtype_fast(h->object->vt(), global_env->java_lang_Cloneable_Class))
        {
            tmn_suspend_enable(); 
            throw_exception_from_jni(jenv, "java/lang/CloneNotSupportedException", 0);
            return NULL;
        }
        size = vt->allocated_size;
        result = (ManagedObject *) gc_alloc(size, vt->clss->allocation_handle, vm_get_gc_thread_local());
    }
    if (result == NULL) {
        tmn_suspend_enable(); 
        exn_throw(VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return NULL;
    }
    memcpy(result, h->object, size);
    result->set_obj_info(0);
    ObjectHandle new_handle = oh_allocate_local_handle();
    new_handle->object = result;
    tmn_suspend_enable(); 
    return (jobject) new_handle;
}
