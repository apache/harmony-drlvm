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

extern hythread_group_t TM_DEFAULT_GROUP;
static void* APR_THREAD_FUNC thread_start_proc(apr_thread_t* thd, void *p_args);
static hythread_t init_thread(hythread_group_t group);
static void reset_thread(hythread_t thread);
//#define APR_TLS_USE 1

#define NAKED __declspec( naked )

#if !defined (APR_TLS_USE ) && !defined (FS14_TLS_USE)
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
    apr_status_t apr_status;
   
    if (!ret_thread || !(*ret_thread)) {
        // Allocate & init thread structure
        new_thread = init_thread(group); 
		if(new_thread==NULL){
			return TM_ERROR_OUT_OF_MEMORY;
		}
    } else {
        new_thread = (*ret_thread);
    }

    if (stacksize) {
        apr_threadattr_create(&apr_attrs, new_thread->pool);
        apr_threadattr_stacksize_set(apr_attrs, stacksize);
        new_thread->apr_attrs  = apr_attrs;
    } else {
        new_thread->apr_attrs = NULL;
    }

    new_thread->priority = priority ? priority : HYTHREAD_PRIORITY_NORMAL;
    //new_thread->suspend_request = suspend ? 1 : 0;
    
    // Set up thread body procedure 
    new_thread->start_proc = func;
    new_thread->start_proc_args = data;
    new_thread->state = TM_THREAD_STATE_ALIVE; 
    // Create APR thread using the given attributes;
    apr_status = apr_thread_create(&(new_thread->os_handle),    // new thread OS handle 
            new_thread->apr_attrs,                          // thread attr created here
            thread_start_proc,                              //
            (void *)new_thread,                             //thread_proc attrs 
            new_thread->pool); 
   
    // Store the pointer to the resulting thread 
    if(ret_thread) {
        *ret_thread = new_thread;
    }
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
 * @param[in] group thread group, or NULL; in case of NULL this thread will go to the default group
 */
IDATA VMCALL hythread_attach_to_group(hythread_t *handle, hythread_group_t group) {
    hythread_t thread;
    apr_thread_t *os_handle = NULL; 
    apr_os_thread_t *os_thread;
    apr_status_t apr_status; 

    // Do nothing and return if the thread is already attached
    thread = tm_self_tls;
    if (thread) {
        if (handle) {
            *handle = thread;
        }
        return TM_ERROR_NONE;
    }
    thread = init_thread(group); // allocate & init thread structure
	if (thread==NULL){
		return TM_ERROR_OUT_OF_MEMORY;
	}
    os_thread = apr_palloc(thread->pool, sizeof(apr_os_thread_t));
	if(os_thread == NULL) {
        return TM_ERROR_OUT_OF_MEMORY;
	} 
    *os_thread = apr_os_thread_current();
    apr_status = apr_os_thread_put(&os_handle, os_thread, thread->pool);
        if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
    
    thread->os_handle = os_handle;
    thread_set_self(thread);
    
    thread->state = TM_THREAD_STATE_ALIVE;
    assert(thread == tm_self_tls);
    TRACE(("TM: native attached: native: %p ",  tm_self_tls));

    if (handle) {
        *handle = thread;
    }
    
    return TM_ERROR_NONE;
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
 * @see hythread_detach
 */
IDATA VMCALL hythread_attach(hythread_t *handle) {
    return hythread_attach_to_group(handle, NULL);
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
    // Acquire global TM lock to prevent concurrent acccess to thread list
        IDATA status;
    status = hythread_global_lock(NULL);
        assert (status == TM_ERROR_NONE);
    
    if (!(thread = tm_self_tls)) {
        status = hythread_global_unlock(NULL);
                assert (status == TM_ERROR_NONE);
        return;
    }
    
        status = thread_destroy(thread);       // Remove thread from the list of thread 
    assert (status == TM_ERROR_NONE);
        thread_set_self(NULL); 
    status = hythread_global_unlock(NULL); 
        assert (status == TM_ERROR_NONE);
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
extern "C"
hythread_t hythread_self() {
    hythread_t  thread;
    apr_status_t UNUSED apr_status;
    
    // Extract hythread_t from TLS
        apr_status = apr_threadkey_private_get((void **)(&thread), TM_THREAD_KEY);            
    assert(apr_status == APR_SUCCESS);
    
    return thread;
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

/*
 */
IDATA thread_sleep_impl(I_64 millis, IDATA nanos, IDATA interruptable) {
    IDATA status;
    
    hythread_t thread = tm_self_tls;
    
    // Report error in case current thread is not attached
    if (!thread) return TM_ERROR_UNATTACHED_THREAD;
    
    status = sem_wait_impl(thread->sleep_event, millis, nanos, interruptable);
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
IDATA VMCALL hythread_sleep(int64 millis) {
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
void* VMCALL hythread_get_private_data(hythread_t  t){
    assert(t);
    return t->private_data;
}
/**
 * Sets the thread private data. 
 *
 * @param t thread
 * @param data pointer to private data
 */
IDATA VMCALL hythread_set_private_data(hythread_t  t, void* data) {
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

//==============================================================================
// Private functions
/*
 */
IDATA thread_destroy(hythread_t thread) {
    
    // Acquire global TM lock to prevent concurrent acccess to thread list    
    IDATA status;
    status =hythread_global_lock(NULL);
    if (status != TM_ERROR_NONE) return status;
    thread->prev->next = thread->next;
    thread->next->prev = thread->prev;
    thread->group->threads_count--;

    fast_thread_array[thread->thread_id] = NULL;

    status =hythread_global_unlock(NULL);
    if (status != TM_ERROR_NONE) return status;
    return TM_ERROR_NONE;
}
/*
 */
IDATA allocate_thread(hythread_t thread, hythread_group_t group) {
    int id = 0;
    hythread_t cur, prev;
        IDATA status;
    //thread_list points to the dummy thread, which is not actual thread, just 
    //a auxiliary thread structure to maintain the thread list
    /////
    
    // Acquire global TM lock to prevent concurrent acccess to thread list
    status =hythread_global_lock(NULL); 
        if (status != TM_ERROR_NONE) return status;
    
    // Append the thread to the list of threads
    cur  = group->thread_list->next;
    if (thread->thread_id) {
        id   = thread->thread_id;
    } else {
        id   = ++next_id;
    }
	if (id>=MAX_ID){
		hythread_global_unlock(NULL);
		return TM_ERROR_OUT_OF_MEMORY;
    }
    prev = cur->prev;
    
    thread->next = cur;
    thread->prev = prev;
    prev->next = cur->prev = thread;
    thread->thread_id = id;
    fast_thread_array[id] = thread;
    /*status=add_to_fast_thread_array(thread,id);
    if (status != TM_ERROR_NONE) return status;*/
    status=hythread_global_unlock(NULL);
    if (status != TM_ERROR_NONE) return status;
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
 * @see hythread_cancell
 */
IDATA VMCALL hythread_cancel_all(hythread_group_t group) {
    hythread_iterator_t iter;
    hythread_t next;
    hythread_t self = tm_self_tls;

    if (!group) {
        group = TM_DEFAULT_GROUP;
    }
    
    iter = hythread_iterator_create (group);
    while((next = hythread_iterator_next (&iter)) != NULL) {
                if(next != self) {
                        hythread_cancel(next);
            //since this method being used at shutdown it does not
            //males any sence to exit on error, but continue terminating threads
                }       
        }

    return TM_ERROR_NONE;
}

/**
 * Allocates and initializes a new thread_t structure.
 *
 */
IDATA VMCALL hythread_struct_init(hythread_t *ret_thread, hythread_group_t group) {
    if (*ret_thread) {
        reset_thread(*ret_thread);
    } else {
        (*ret_thread) = init_thread(group);
    }
    if ((*ret_thread)==NULL)
	{
		return TM_ERROR_OUT_OF_MEMORY;
	}
    return TM_ERROR_NONE;
}

/*
 * Allocates and initializes a new thread_t structure 
 *
 * @return created and initialized thread_t structure
 */
static hythread_t init_thread(hythread_group_t group) {
    apr_pool_t *pool;
        apr_status_t apr_status;
    hythread_t ptr;
        IDATA status;

    if (!group) {
        group = TM_DEFAULT_GROUP;
    }

    apr_status = apr_pool_create(&pool, group->pool);
    if((apr_status!=APR_SUCCESS)||(pool==NULL))
	    return NULL;
    ptr = (hythread_t )apr_pcalloc(pool, sizeof(HyThread));
	if (ptr==NULL)
		return NULL;
    ptr->pool       = pool;
    ptr->group      = group;
    ptr->os_handle  = NULL;
    ptr->next       = ptr->prev = ptr;
    ptr->priority   = HYTHREAD_PRIORITY_NORMAL;
    // not implemented
    //ptr->big_thread_local_storage = (void **)apr_pcalloc(pool, sizeof(void*)*tm_tls_capacity);
    
    // Suspension
    ptr->suspend_request = 0;
    ptr->suspend_disable_count = 0;
    status = hylatch_create(&ptr->join_event, 1);
        assert (status == TM_ERROR_NONE);
    status = hylatch_create(&ptr->safe_region_event, 1);        
        assert (status == TM_ERROR_NONE);
    status = hysem_create(&ptr->resume_event, 0, 1);
        assert (status == TM_ERROR_NONE);
    status = hysem_create(&ptr->park_event, 0, 1);
        assert (status == TM_ERROR_NONE);
    status = hysem_create(&ptr->sleep_event, 0, 1);
        assert (status == TM_ERROR_NONE);
    
    ptr->state = TM_THREAD_STATE_ALLOCATED;
    status = allocate_thread(ptr, group);
	if (status !=TM_ERROR_NONE)
	{
		return NULL;
	}
    group->threads_count++;

    return ptr;
}

static void reset_thread(hythread_t thread) {
    apr_status_t apr_status;
        IDATA status;
    apr_thread_join(&apr_status, thread->os_handle);
        assert(!apr_status);
    thread->os_handle  = NULL;
    thread->priority   = HYTHREAD_PRIORITY_NORMAL;
    // not implemented
    //ptr->big_thread_local_storage = (void **)apr_pcalloc(pool, sizeof(void*)*tm_tls_capacity);
    thread->next       = thread->prev = thread;
    // Suspension
    thread->suspend_request = 0;
    thread->suspend_disable_count = 0;
    status = hylatch_set(thread->join_event, 1);
        assert (status == TM_ERROR_NONE);
    status = hylatch_set(thread->safe_region_event, 1); 
        assert (status == TM_ERROR_NONE);
    status = hysem_set(thread->resume_event, 0);
        assert (status == TM_ERROR_NONE);
    status = hysem_set(thread->park_event, 0);
        assert (status == TM_ERROR_NONE);
    status = hysem_set(thread->sleep_event, 0);
        assert (status == TM_ERROR_NONE);
    
    thread->state = TM_THREAD_STATE_ALLOCATED;
    status =allocate_thread(thread, thread->group);
	if (status!=TM_ERROR_NONE)
	{
		thread=NULL;
	}
    thread->group->threads_count++;
}

// Wrapper around user thread start proc. Used to perform some duty jobs 
// right after thread is started.
//////
static void* APR_THREAD_FUNC thread_start_proc(apr_thread_t* thd, void *p_args) {
        IDATA status;
    hythread_t thread = (hythread_t )p_args;

    TRACE(("TM: native thread started: native: %p tm: %p", apr_os_thread_current(), thread));
    // Also, should it be executed under TM global lock?
    thread->os_handle = thd; // DELETE?
    thread_set_self(thread);
    status = hythread_set_priority(thread, thread->priority);
        //assert (status == TM_ERROR_NONE);//now we down - fixme
    thread->state |= TM_THREAD_STATE_RUNNABLE;

    // Do actual call of the thread body supplied by the user
    thread->start_proc(thread->start_proc_args);
    
    thread->state = TM_THREAD_STATE_TERMINATED;
    // Send join event to those threads who called join on this thread
    hylatch_count_down(thread->join_event);
    status = thread_destroy(thread);       // Remove thread from the list of thread 
    assert (status == TM_ERROR_NONE);
    
    // Cleanup TLS after thread completes
    thread_set_self(NULL); 
    return (void *)apr_thread_exit(thd, APR_SUCCESS);
}

apr_pool_t* get_local_pool() {
  hythread_t self = tm_self_tls;
  if(self == NULL) {
      return TM_POOL;
  } else {
      return self->pool;
  }
}
 
/**
  * TODO: implement this function to reduce memory leaks.
  */
IDATA local_pool_cleanup_register(void* func, void* data) {
   return TM_ERROR_NONE;
}

