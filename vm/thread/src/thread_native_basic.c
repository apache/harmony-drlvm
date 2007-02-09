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
 * @version $Revision: 1.1.2.13 $
 */  

/**
 * @file thread_native_basic.c
 * @brief hythread basic functions
 */

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.native"

#include <open/hythread_ext.h>
#include "thread_private.h"

typedef struct {
    hythread_t thread;
    hythread_group_t group;
    hythread_entrypoint_t start_proc;
    void * start_proc_args;
} thread_start_proc_data;

extern hythread_group_t TM_DEFAULT_GROUP;
extern hythread_library_t TM_LIBRARY;
static void* APR_THREAD_FUNC thread_start_proc(apr_thread_t* thd, void *p_args);
static hythread_t allocate_thread();
static void reset_thread(hythread_t thread);
static IDATA register_to_group(hythread_t thread, hythread_group_t group);
//#define APR_TLS_USE 1

#define NAKED __declspec(naked)

#if !defined (APR_TLS_USE) && !defined (FS14_TLS_USE)
#ifdef PLATFORM_POSIX
__thread hythread_t tm_self_tls = NULL;
#else
__declspec(thread) hythread_t tm_self_tls = NULL;
#endif
#endif

#define MAX_ID 1000000
hythread_t fast_thread_array[MAX_ID];
int next_id = 1;

/*
IDATA add_to_fast_thread_array(hythread_t thread,int id)
{
	if (id>=MAX_ID)
	{
		if (MAX_ID<1000)
	    {
			MAX_ID=1000;
			fast_thread_array=(hythread_t *)malloc(MAX_ID*sizeof(hythread_t));
	    }
		else
		{
			MAX_ID*=2;
			fast_thread_array=(hythread_t *)realloc(fast_thread_array,MAX_ID*sizeof(hythread_t));
		}
		if (fast_thread_array==NULL)
			return TM_ERROR_OUT_OF_MEMORY;
	}
	fast_thread_array[id]=thread;
	return TM_ERROR_NONE;
}*/
static void thread_set_self(hythread_t thread);

/**
 * Creates a new thread.
 *
 * @param[out] new_thread The newly created thread.
 * @param[in] group thread group, or NULL; in case of NULL this thread will go to the default group.
 * @param[in] attr threadattr to use to determine how to create the thread, or NULL for default attributes
 * @param[in] func function to run in the new thread
 * @param[in] data argument to be passed to starting function
 * @sa apr_thread_create()
 */
IDATA VMCALL hythread_create_with_group(hythread_t *ret_thread, hythread_group_t group, UDATA stacksize, UDATA priority, UDATA suspend, hythread_entrypoint_t func, void *data) {
    apr_threadattr_t *apr_attrs;
    hythread_t  new_thread;
    thread_start_proc_data * start_proc_data;
    apr_status_t apr_status;
   
    if (ret_thread) {
        hythread_struct_init(ret_thread);
        new_thread = *ret_thread;
    } else {
        new_thread = allocate_thread();
    }
    
    if (new_thread == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
    }

    new_thread->library = hythread_self()->library;
    if (stacksize) {
        apr_threadattr_create(&apr_attrs, new_thread->pool);
        apr_threadattr_stacksize_set(apr_attrs, stacksize);
        new_thread->apr_attrs  = apr_attrs;
    } else {
        new_thread->apr_attrs = NULL;
    }

    new_thread->priority = priority ? priority : HYTHREAD_PRIORITY_NORMAL;
    //new_thread->suspend_request = suspend ? 1 : 0;
    
    start_proc_data = (thread_start_proc_data *) apr_palloc(new_thread->pool, sizeof(thread_start_proc_data));
    if (start_proc_data == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
    }

    // Set up thread body procedure 
    start_proc_data->thread = new_thread;
    start_proc_data->group = group == NULL ? TM_DEFAULT_GROUP : group;
    start_proc_data->start_proc = func;
    start_proc_data->start_proc_args = data;

    // Create APR thread using the given attributes;
    apr_status = apr_thread_create(&(new_thread->os_handle), // new thread OS handle 
            new_thread->apr_attrs,                           // thread attr created here
            thread_start_proc,                               //
            (void *)start_proc_data,                         //thread_proc attrs 
            new_thread->pool); 
   
    return CONVERT_ERROR(apr_status);
}

/**
 * Create a new OS thread.
 * 
 * The created thread is attached to the threading library.<br>
 * <br>
 * Unlike POSIX, this doesn't require an attributes structure.
 * Instead, any interesting attributes (e.g. stacksize) are
 * passed in with the arguments.
 *
 * @param[out] ret_thread a pointer to a hythread_t which will point to the thread (if successfully created)
 * @param[in] stacksize the size of the new thread's stack (bytes)<br>
 *                      0 indicates use default size
 * @param[in] priority priorities range from HYTHREAD_PRIORITY_MIN to HYTHREAD_PRIORITY_MAX (inclusive)
 * @param[in] suspend set to non-zero to create the thread in a suspended state.
 * @param[in] func pointer to the function which the thread will run
 * @param[in] data a value to pass to the entrypoint function
 *
 * @return  0 on success or negative value on failure
 *
 * @see hythread_exit, hythread_resume
 */
IDATA VMCALL hythread_create(hythread_t *ret_thread, UDATA stacksize, UDATA priority, UDATA suspend, hythread_entrypoint_t func, void *data) {
    return hythread_create_with_group(ret_thread, NULL, stacksize, priority, suspend, func, data);
}

/**
 * Registers the current OS thread with the threading subsystem.
 *
 * @param[in] handle thread to register
 * @param[in] lib thread library to attach to
 * @param[in] group thread group, or NULL; in case of NULL this thread will go to the default group
 */
IDATA hythread_attach_to_group(hythread_t * handle, hythread_library_t lib, hythread_group_t group) {
    hythread_t thread;
    apr_thread_t *os_handle = NULL; 
    apr_os_thread_t *os_thread;
    apr_status_t apr_status;

    if (lib == NULL) {
        lib = TM_LIBRARY;
    }

    // Do nothing and return if the thread is already attached
    thread = tm_self_tls;
    if (thread) {
        if (handle) {
            *handle = thread;
        }
        return TM_ERROR_NONE;
    }
    if (handle) {
        hythread_struct_init(handle);
        thread = *handle;
    } else {
        thread = allocate_thread();
    }
    if (thread == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
    }
    thread->library = lib;
    os_thread = apr_palloc(thread->pool, sizeof(apr_os_thread_t));
    if (os_thread == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
    } 
    *os_thread = apr_os_thread_current();
    apr_status = apr_os_thread_put(&os_handle, os_thread, thread->pool);
    if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
    
    thread->os_handle = os_handle;

    TRACE(("TM: native attached: native: %p ",  tm_self_tls));
    
    return register_to_group(thread, group == NULL ? TM_DEFAULT_GROUP : group);
}

/**
 * Attach an OS thread to the threading library.
 *
 * Create a new hythread_t to represent the existing OS thread.
 * Attaching a thread is required when a thread was created
 * outside of the Hy threading library wants to use any of the
 * Hy threading library functionality.
 *
 * If the OS thread is already attached, handle is set to point
 * to the existing hythread_t.
 *
 * @param[out] handle pointer to a hythread_t to be set (will be ignored if null)
 * @return  0 on success or negative value on failure
 *
 * @note (*handle) should be NULL or point to hythread_t structure  
 * @see hythread_detach
 */
IDATA VMCALL hythread_attach(hythread_t *handle) {
    return hythread_attach_to_group(handle, TM_LIBRARY, NULL);
}

/**
 * Attach an OS thread to the threading library.
 *
 * @param[out] handle pointer to a hythread_t to be set (will be ignored if null)
 * @param[in] lib thread library to attach thread to
 * @return  0 on success or negative value on failure
 *
 * @note (*handle) should be NULL or point to hythread_t structure  
 * @see hythread_detach
 */
IDATA VMCALL hythread_attach_ex(hythread_t *handle, hythread_library_t lib) {
    return hythread_attach_to_group(handle, lib, NULL);
}

/**
 * Detaches a thread from the threading library.
 * 
 * @note Assumes that the thread being detached is already attached.<br>
 * 
 * If the thread is an attached thread, then detach should only be called by the thread
 * itself. Internal resources associated with the thread are freed.
 * 
 * If the thread is already dead, this call will destroy it.
 * 
 * @param[in] thread a hythread_t representing the thread to be detached.
 * If this is NULL, the current thread is detached.
 * @return none
 * 
 * @see hythread_attach
 */
void VMCALL hythread_detach(hythread_t thread) {
    IDATA status;

    if (thread == NULL) {
        thread = hythread_self();
    }
    
    // Acquire global TM lock to prevent concurrent access to thread list
    status = hythread_global_lock(NULL);
    assert(status == TM_ERROR_NONE);

    // No actions required in case the specified thread is detached already.
    if (thread->group != NULL) {
        assert(thread == tm_self_tls);
        
        thread_set_self(NULL);
        fast_thread_array[thread->thread_id] = NULL;
        
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;
        thread->group->threads_count--;
        thread->group = NULL;
    }
    
    hythread_global_unlock(NULL);
    assert(status == TM_ERROR_NONE);
}

/**
 * Waits until the selected thread finishes execution.
 *
 * @param[in] t thread to join
 */
IDATA VMCALL hythread_join(hythread_t t) { 
    return hylatch_wait(t->join_event);
}
/**
 * Waits until the selected thread finishes with specific timeout.
 *
 * @param[in] t a thread to wait for
 * @param[in] millis timeout in milliseconds to wait
 * @param[in] nanos timeout in nanoseconds to wait
 * @return TM_THREAD_TIMEOUT or 0 in case thread
 * was successfully joined.
 */
IDATA VMCALL hythread_join_timed(hythread_t t, I_64 millis, IDATA nanos) { 
    return hylatch_wait_timed(t->join_event, millis, nanos);
}

/**
 * Waits until the selected thread finishes with specific timeout.
 *
 * @param[in] t a thread to wait for
 * @param[in] millis timeout in milliseconds to wait
 * @param[in] nanos timeout in nanoseconds to wait
 * @return TM_THREAD_TIMEOUT or TM_THREAD_INTERRUPTED or 0 in case thread
 * was successfully joined.
 */
IDATA VMCALL hythread_join_interruptable(hythread_t t, I_64 millis, IDATA nanos) { 
    return hylatch_wait_interruptable(t->join_event, millis, nanos);
}

/** 
 * Yield the processor.
 * 
 * @return none
 */
void VMCALL hythread_yield() {
    //apr_thread_yield returns void 
    apr_thread_yield();
}
/** 
 * Return the hythread_t for the current thread.
 * 
 * @note Must be called only by an attached thread
 * 
 * @return hythread_t for the current thread
 *
 * @see hythread_attach
 * 
 */
#ifdef APR_TLS_USE
/**
 * Return the hythread_t for the current thread.
 *
 * @note Must be called only by an attached thread
 *
 * @return hythread_t for the current thread
 *
 * @see hythread_attach
 *
 */
hythread_t hythread_self() {
    hythread_t  thread;
    apr_status_t UNUSED apr_status;
    
    // Extract hythread_t from TLS
    apr_status = apr_threadkey_private_get((void **)(&thread), TM_THREAD_KEY);            
    assert(apr_status == APR_SUCCESS);
    
    return thread;
}

static void thread_set_self(hythread_t  thread) {
    apr_threadkey_private_set(thread, TM_THREAD_KEY);
}
#else 
#ifdef FS14_TLS_USE
/**
 * Return the hythread_t for the current thread.
 *
 * @note Must be called only by an attached thread
 *
 * @return hythread_t for the current thread
 *
 * @see hythread_attach
 *
 */
NAKED hythread_t hythread_self() {
    _asm { mov eax, fs:[0x14]
           ret;
    }
    //return tm_self_tls;
}

static void thread_set_self(hythread_t  thread) {
    //tm_self_tls = thread;
    _asm{
        mov eax, thread
        mov fs:[0x14], eax
    }
}
#else
/**
 * Return the hythread_t for the current thread.
 *
 * @note Must be called only by an attached thread
 *
 * @return hythread_t for the current thread
 *
 * @see hythread_attach
 *
 */
hythread_t hythread_self() {
    return tm_self_tls;
}

static void thread_set_self(hythread_t  thread) {
    tm_self_tls = thread;
}
#endif
#endif

IDATA thread_sleep_impl(I_64 millis, IDATA nanos, IDATA interruptable) {
    IDATA status;
    
    hythread_t thread = tm_self_tls;
    
    if (nanos == 0 && millis == 0) {
        hythread_yield();
        return TM_ERROR_NONE;
    }         
    // Report error in case current thread is not attached
    if (!thread) return TM_ERROR_UNATTACHED_THREAD;
    
    hymutex_lock(thread->mutex);
    thread->state |= TM_THREAD_STATE_SLEEPING;
    status = condvar_wait_impl(thread->condition, thread->mutex, millis, nanos, interruptable);
    thread->state &= ~TM_THREAD_STATE_SLEEPING;
    hymutex_unlock(thread->mutex);

    return (status == TM_ERROR_INTERRUPT && interruptable) ? TM_ERROR_INTERRUPT : TM_ERROR_NONE;
}

/** 
 * Suspend the current thread from executing 
 * for at least the specified time.
 *
 * @param[in] millis
 * @param[in] nanos 
 * @return  0 on success<br>
 *    HYTHREAD_INVALID_ARGUMENT if the arguments are invalid<br>
 *    HYTHREAD_INTERRUPTED if the sleep was interrupted
 *
 * @see hythread_sleep
 */
IDATA VMCALL hythread_sleep_interruptable(I_64 millis, IDATA nanos) {    
    return thread_sleep_impl(millis, nanos, WAIT_INTERRUPTABLE);
}

/** 
 * Suspend the current thread from executing 
 * for at least the specified time.
 *
 * @param[in] millis minimum number of milliseconds to sleep
 * @return  0 on success<br> HYTHREAD_INVALID_ARGUMENT if millis < 0
 *
 * @see hythread_sleep_interruptable
 */
IDATA VMCALL hythread_sleep(I_64 millis) {
    return thread_sleep_impl(millis, 0, WAIT_NONINTERRUPTABLE);
}

/**
 * Returns the id of the specific thread.
 * 
 * @return  0 on success
 */
IDATA VMCALL hythread_get_id(hythread_t t) {
    assert(t);
    return (IDATA)t->thread_id;
}

/**
 * Returns the id of the current thread.
 * @return  0 on success
 */
IDATA VMCALL hythread_get_self_id() {
    return (IDATA)tm_self_tls->thread_id;
}
/**
 * Returns the thread given the specific id.
 */
hythread_t VMCALL hythread_get_thread(IDATA id) {
    return fast_thread_array[id];
}

/**
 * Returns thread private data.
 * 
 * @param[in] t thread those private data to get
 * @return pointer to thread private data
 */
void* VMCALL hythread_get_private_data(hythread_t t) {
    assert(t);
    return t->private_data;
}
/**
 * Sets the thread private data. 
 *
 * @param t thread
 * @param data pointer to private data
 */
IDATA VMCALL hythread_set_private_data(hythread_t t, void* data) {
    assert(t);
    t->private_data = data;
    return TM_ERROR_NONE;
}

/**
 * Get thread group. 
 *
 * @param[out] group hythread_group_t* pointer to group
 * @param[in] thread hythread_t thread
 * @return  0 on success
 */
IDATA VMCALL hythread_get_group(hythread_group_t *group, hythread_t thread) {
    (*group) = thread->group;
    return TM_ERROR_NONE;
}

/** 
 * Terminates a running thread.
 * 
 * @note This should only be used as a last resort.  The system may be in
 * an unpredictable state once a thread is cancelled.  In addition, the thread
 * may not even stop running if it refuses to cancel.
 * 
 * @param[in] thread a thread to be terminated 
 * @return none
 */
void VMCALL hythread_cancel(hythread_t thread) {
    apr_thread_cancel(thread->os_handle);
}

/** 
 * Terminates all running threads in the given group.
 * 
 * @param[in] group thread group
 * @see hythread_cancel
 */
IDATA VMCALL hythread_cancel_all(hythread_group_t group) {
    hythread_iterator_t iter;
    hythread_t next;
    hythread_t self = tm_self_tls;

    if (!group) {
        group = TM_DEFAULT_GROUP;
    }
    
    iter = hythread_iterator_create(group);
    while ((next = hythread_iterator_next (&iter)) != NULL) {
        if (next != self) {
            hythread_cancel(next);
            //since this method being used at shutdown it does not
            //make any sense to exit on error, but continue terminating threads
        }       
    }

    return TM_ERROR_NONE;
}

/**
 * Allocates and initializes a new thread_t structure.
 *
 */
IDATA VMCALL hythread_struct_init(hythread_t *ret_thread) {
    if (*ret_thread) {
        reset_thread(*ret_thread);
        return TM_ERROR_NONE;
    }
    (*ret_thread) = allocate_thread();
    return (*ret_thread) == NULL ? TM_ERROR_OUT_OF_MEMORY : TM_ERROR_NONE;
}
//==============================================================================
// Private functions

/*
 */
static IDATA register_to_group(hythread_t thread, hythread_group_t group) {
    IDATA status;
    hythread_t cur, prev;

    assert(thread);
    assert(group);
    
    // Acquire global TM lock to prevent concurrent access to thread list
    status = hythread_global_lock(NULL);
    if (status != TM_ERROR_NONE) return status;

    thread_set_self(thread);
    assert(thread == tm_self_tls);

    thread->state |= TM_THREAD_STATE_ALIVE | TM_THREAD_STATE_RUNNABLE;
    
    if (!thread->thread_id) {
        ++next_id;
        thread->thread_id = next_id;
        if (next_id >= MAX_ID) {
            hythread_global_unlock(NULL);
            return TM_ERROR_OUT_OF_MEMORY;
        }
    }
    
    fast_thread_array[thread->thread_id] = thread;

    thread->group = group;
    group->threads_count++;
    cur  = group->thread_list->next;
    prev = cur->prev;
    thread->next = cur;
    thread->prev = prev;
    prev->next = cur->prev = thread;
    return hythread_global_unlock(NULL);    
}

/*
 * Allocates and initializes a new thread_t structure 
 *
 * @return created and initialized thread_t structure
 */
static hythread_t allocate_thread() {
    apr_pool_t *pool;
    apr_status_t apr_status;
    hythread_t ptr;
    IDATA status;

    apr_status = apr_pool_create(&pool, TM_POOL);
    if ((apr_status != APR_SUCCESS) || (pool == NULL)) return NULL;

    ptr = (hythread_t )apr_pcalloc(pool, sizeof(HyThread));
    if (ptr == NULL) return NULL;

    ptr->pool       = pool;
    ptr->os_handle  = NULL;
    ptr->priority   = HYTHREAD_PRIORITY_NORMAL;
    // not implemented
    //ptr->big_thread_local_storage = (void **)apr_pcalloc(pool, sizeof(void*)*tm_tls_capacity);
    
    // Suspension
    ptr->suspend_request = 0;
    ptr->suspend_disable_count = 0;
    status = hylatch_create(&ptr->join_event, 1);
    assert(status == TM_ERROR_NONE);
    status = hylatch_create(&ptr->safe_region_event, 1);
    assert(status == TM_ERROR_NONE);
    status = hysem_create(&ptr->resume_event, 0, 1);
    assert(status == TM_ERROR_NONE);
    status = hymutex_create(&ptr->mutex, TM_MUTEX_NESTED);
    assert(status == TM_ERROR_NONE);
    status = hycond_create(&ptr->condition);
    assert(status == TM_ERROR_NONE);
    
    ptr->state = TM_THREAD_STATE_ALLOCATED;
    return ptr;
}

static void reset_thread(hythread_t thread) {
    apr_status_t apr_status;
    IDATA status;
    if (thread->os_handle) {
        apr_thread_join(&apr_status, thread->os_handle);
        assert(!apr_status);
    }

    thread->os_handle  = NULL;
    thread->priority   = HYTHREAD_PRIORITY_NORMAL;
    // not implemented
    //ptr->big_thread_local_storage = (void **)apr_pcalloc(pool, sizeof(void*)*tm_tls_capacity);

    // Suspension
    thread->suspend_request = 0;
    thread->suspend_disable_count = 0;
    thread->safepoint_callback = NULL;
    status = hylatch_set(thread->join_event, 1);
    assert(status == TM_ERROR_NONE);
    status = hylatch_set(thread->safe_region_event, 1);
    assert(status == TM_ERROR_NONE);
    status = hysem_set(thread->resume_event, 0);
    assert(status == TM_ERROR_NONE);
    
    thread->state = TM_THREAD_STATE_ALLOCATED;
}

// Wrapper around user thread start proc. Used to perform some duty jobs 
// right after thread is started.
//////
static void* APR_THREAD_FUNC thread_start_proc(apr_thread_t* thd, void *p_args) {
    IDATA status;
    hythread_t thread;
    thread_start_proc_data * start_proc_data;
    
    start_proc_data = (thread_start_proc_data *) p_args;
    thread = start_proc_data->thread;

    TRACE(("TM: native thread started: native: %p tm: %p", apr_os_thread_current(), thread));

    status = register_to_group(thread, start_proc_data->group);
    if (status != TM_ERROR_NONE) {
        thread->exit_value = status;
        return &thread->exit_value;
    }

    // Also, should it be executed under TM global lock?
    thread->os_handle = thd; // DELETE?
    status = hythread_set_priority(thread, thread->priority);
    //assert(status == TM_ERROR_NONE);//now we down - fixme
    thread->state |= TM_THREAD_STATE_RUNNABLE;

    // Do actual call of the thread body supplied by the user.
    start_proc_data->start_proc(start_proc_data->start_proc_args);

    // Shutdown sequence.
    status = hythread_global_lock(NULL);
    assert(status == TM_ERROR_NONE);
    assert(hythread_is_suspend_enabled()); 
    thread->state = TM_THREAD_STATE_TERMINATED | (TM_THREAD_STATE_INTERRUPTED  & thread->state);
    thread->exit_value = 0;

    hythread_detach(thread);
    // Send join event to those threads who called join on this thread.
    hylatch_count_down(thread->join_event);

    status = hythread_global_unlock(NULL);
    assert(status == TM_ERROR_NONE);    
    
    // TODO: It seems it is there is no need to call apr_thread_exit.
    // Current thread should automatically exit upon returning from this function.
    return (void *)(IDATA)apr_thread_exit(thd, APR_SUCCESS);
}

extern HY_CFUNC void VMCALL 
    hythread_exit (hythread_monitor_t monitor) {
   
    if (monitor !=NULL && monitor->owner == hythread_self()) {
        monitor->recursion_count = 0;
        hythread_monitor_exit(monitor);
    }
    apr_thread_exit(hythread_self()->os_handle, APR_SUCCESS);     
    // unreachable statement
    abort();
}

apr_pool_t* get_local_pool() {
    hythread_t self = tm_self_tls;
    return self == NULL ? TM_POOL : self->pool;
}
 
/**
  * TODO: implement this function to reduce memory leaks.
  */
IDATA local_pool_cleanup_register(void* func, void* data) {
   return TM_ERROR_NONE;
}

