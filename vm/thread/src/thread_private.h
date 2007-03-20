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
#ifndef THREAD_PRIVATE_H
#define THREAD_PRIVATE_H

#include <assert.h>
//#include <open/thread_types.h>
#include <stdlib.h>
#include <open/types.h>
#include <open/hythread_ext.h>
#include <open/ti_thread.h>
#include <open/thread_externals.h>
#include <apr_pools.h>
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_thread_rwlock.h>
#include <apr_portable.h>

#include <assert.h>
#include "apr_thread_ext.h"

#ifdef __linux__
#include <pthread.h>
#endif // __linux__

// temporary remove logging
#define TRACE(a) //printf a; printf("\n")
//#include "clog.h"

// FIXME move to the global header, add error converter 
#define RET_ON_ERROR(stat) if (stat) { return -1; }
#define CONVERT_ERROR(stat)	(stat)

#define MAX_OWNED_MONITOR_NUMBER 200 //FIXME: switch to dynamic resize
#define FAST_LOCAL_STORAGE_SIZE 10

#if !defined (_IPF_)
//use lock reservation
#define LOCK_RESERVATION
// spin with try_lock SPIN_COUNT times
#define SPIN_COUNT 5

#endif // !defined (_IPF_)

#if defined(WIN32) && !defined (_EM64T_)
//use optimized asm monitor enter and exit helpers 
#define ASM_MONITOR_HELPER
// FS14_TLS_USE define turns on windows specific TLS access optimization 
// We use free TIB slot with 14 offset, see following article for details 
// http://www.microsoft.com/msj/archive/S2CE.aspx
#define FS14_TLS_USE
#endif

/*
#ifdef _EM64T_
#define APR_TLS_USE
#endif
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
// optimization code
#if !defined (APR_TLS_USE ) && !defined (FS14_TLS_USE) 

#ifdef PLATFORM_POSIX
extern __thread hythread_t tm_self_tls;
#else
extern __declspec(thread) hythread_t tm_self_tls;
#endif //PLATFORM_POSIX

#else
#if defined (WIN32) && defined (FS14_TLS_USE)

__forceinline hythread_t tmn_self_macro() {
    register hythread_t t;
    _asm { mov eax, fs:[0x14]
           mov t, eax;
    }
    return t;
}


#define store_tm_self(self)  (__asm(mov self, fs:[0x14]))
#define tm_self_tls (tmn_self_macro())
#endif 

#endif

#ifdef APR_TLS_USE
#define tm_self_tls (hythread_self())
#endif


#ifdef __linux__
#define osthread_t pthread_t
#elif _WIN32
#define osthread_t HANDLE
#else // !_WIN32 && !__linux__
#error "threading is only supported on __linux__ or _WIN32"
#endif // !_WIN32 && !__linux__


extern hythread_group_t TM_DEFAULT_GROUP;
/**
 * current capacity of the thread local storage
 */
extern int16 tm_tls_capacity;

/**
 * current capacity of the thread local storage
 */
extern int16 tm_tls_size;


typedef struct HyThreadLibrary {
    IDATA a;
    hymutex_t TM_LOCK;
    IDATA     nondaemon_thread_count;
    hycond_t  nondaemon_thread_cond;
} HyThreadLibrary;



/**
 * Native thread control structure.
 */
typedef struct HyThread {

#ifndef POSIX
    // This is dummy pointer for Microsoft Visual Studio debugging
    // If this is removed, Visual Studio, when attached to VM, will show
    // no symbolic information
    void* reserved;
#endif

    /**
     * Each thread keeps a pointer to the library it belongs to.
     */
    HyThreadLibrary * library;

// Suspension 
    
    /**
     * Number of suspend requests made for this thread.
     */
    int32 suspend_request;
    
    /**
     * Flag indicating that thread can safely be suspended.
     */
    int16 suspend_disable_count;
        
    /**
     * Event used to notify interested threads whenever thread enters the safe region.
     */
    hylatch_t safe_region_event;

    /**
     * Event used to notify suspended thread that it needs to wake up.
     */   
    hysem_t resume_event;

    /**
     * Function to be executed at safepoint upon thread resume.
     */
    hythread_event_callback_proc safepoint_callback;


// Basic manipulation fields

    /**
     * Group for this thread. Different groups are needed in order 
     * to be able to quickly iterate over the specific group.
     * Examples are: Java threads, GC private threads.
     * Equal to the address of the head of the list of threads for this group.
     */
    hythread_group_t  group; 
    
    /**
     * Points to the next thread within the group.
     */    
    hythread_t next;

    /**
     * Points to the last thread within the group.
     */
    hythread_t prev;
    
    /**
     * Handle to OS thread.
     */
    osthread_t os_handle;
        
    /**
     * Placeholder for any data to be associated with this thread.
     * Java layer is using it to keep java-specific context.
     */
    void *private_data;

    /**
     * Flag indicating there was request to exit
     */
    Boolean exit_request; 
    
    /**
     * Exit value of this thread
     */
    IDATA exit_value; 


// Synchronization stuff

    /*
     * Thread local lock, used to serialize thread state;
     */
    hymutex_t mutex;

    /*
     * Conditional variable used to implement wait function for sleep/park;
     */
    hycond_t condition;

    /**
     * Event reserved for threads that invoke join.
     */
    hylatch_t join_event;
    
    /**
     * Current conditional variable thread is waiting on (used for interrupting)
     */
    hycond_t *current_condition;

// State

    /**
     * Thread state. Holds thread state flags as defined in JVMTI specification, plus some additional
     * flags. See <a href=http://java.sun.com/j2se/1.5.0/docs/guide/jvmti/jvmti.html#GetThreadState> 
     * JVMTI Specification </a> for more details.
     */
    IDATA state;


// Attributes

    /**
     * name of the thread (useful for debugging purposes)
     */
    char* name;

   /**
    * Hint for scheduler about thread priority
    */
    IDATA priority;    

// Monitors
    
    /**
     *  Monitor this thread is waiting on now.
     **/
    hythread_monitor_t waited_monitor;

    /**
     * ID for this thread. The maximum number of threads is governed by the size of lockword record.
     */
    IDATA thread_id;

    /**
     * APR thread attributes
     */
    apr_threadattr_t *apr_attrs;

    /**
     * Array representing thread local storage
     */
    void *thread_local_storage[10];

    /**
     * Extension to the standard local storage slot.
     */
    void **big_local_storage;
       
} HyThread;


/**
 * Java-specific context that is attached to tm_thread control structure by Java layer
 */
typedef struct JVMTIThread {
    
    /**
     * JNI env associated with this Java thread
     */
    JNIEnv *jenv;
       
    /**
     * jthread object which is associated with tm_thread
     */
    jthread thread_object;
       
    /**
     * Conditional variable which is used to wait/notify on java monitors.
     */
    hycond_t monitor_condition;

    /**
     * Exception that has to be thrown in stopped thread
     */
    jthrowable stop_exception;

    /**
     * Blocked on monitor times count
     */
     jlong blocked_count;
       
    /**
     * Blocked on monitor time in nanoseconds
     */
     jlong blocked_time;
       
    /**
     * Waited on monitor times count
     */
     jlong waited_count;
       
    /**
     * Waited on monitor time in nanoseconds
     */
     jlong waited_time;
       
    /**
     * JVM TI local storage
     */
     JVMTILocalStorage jvmti_local_storage;

    /**
     * Monitor this thread is blocked on.
     */
     jobject contended_monitor;

    /**
     * Monitor this thread waits on.
     */
     jobject wait_monitor;

    /**
     * Monitors for which this thread is owner.
     */
     jobject *owned_monitors;

    /**
     * owned monitors count.
     */
     int owned_monitors_nmb;

    /**
     * APR pool for this structure
     */
     apr_pool_t *pool;

     /**
      * weak reference to corresponding java.lang.Thread instance
      */
     jobject thread_ref;

     /**
      * Is this thread daemon?
      */
     IDATA daemon;

} JVMTIThread;



/** 
  * hythread_group_t pointer to the first element in the thread group
  */
typedef struct HyThreadGroup {
    
    /**
     * Pointer to the first thread in the list of threads 
     * contained in this group
     */
    hythread_t  thread_list;


    /**
     * Pointer to the first thread in the list of threads 
     * contained in this group
     */
    hythread_t  thread_list_tail;
        
    /**
     * Number of threads in this group
     */
    int threads_count;

    /**
     * Group index or key for search purposes
     */
    int group_index;
    
    /**
     * Memory pool to place created threads into.
     */
    apr_pool_t* pool;

    /**
     *
     */
    hythread_group_t next;

    /**
     *
     */
    hythread_group_t prev;

} HyThreadGroup;


/**
 * Fat monitor structure.
 *
 * A simple combination of conditional variable and fat lock.
 */
typedef struct HyThreadMonitor {
    
    /**
     * Mutex
     */
    hymutex_t mutex;

    /**
     * Condition variable
     */
    hycond_t condition;
    
    /**
     * Recursion count 
     */
    IDATA recursion_count;
    hythread_t owner; 
    hythread_t inflate_owner;
    hythread_t last_wait;
    int inflate_count;
    int wait_count;
    int notify_flag;

    /**
     * Owner thread ID. 
     */
    IDATA thread_id;

    UDATA flags;

    char *name;

} HyThreadMonitor;

/**
 * Count down latch
 */
typedef struct HyLatch {
    
    /**
     * Latch count
     */
    int count;

    /**
     * Condition event used to signal threads which are waiting on the latch.
     */
    hycond_t condition; 
    
    /**
     * Mutex associated with the latch data.
     */
    hymutex_t mutex;  
    /**
      * latch sub pool
      * will be destroyed by latch_destroy()
     */
    apr_pool_t *pool;       
    
} HyLatch;


/**
 * Semaphore
 */
typedef struct HySemaphore {
    
    /**
     * Semaphore count
     */
    int count;

    /**
     * Semaphore max count
     */
    int max_count;

    /**
     * Condition event used to signal threads which are waiting on the semaphore.
     */
    hycond_t condition; 
    
    /**
     * Mutex associated with the semaphore data.
     */
    hymutex_t mutex;         
} HySemaphore;

// Global variables 

extern hythread_group_t group_list; // list of thread groups
extern IDATA groups_count; // number of thread groups

extern apr_pool_t *TM_POOL;           //global APR pool for thread manager

extern apr_threadkey_t *TM_THREAD_KEY; // Key used to store tm_thread_t structure in TLS

extern int max_group_index;     // max number of groups

extern int total_started_thread_count; // Total started thread counter.

#define THREAD_ID_SIZE 16  //size of thread ID in bits. Also defines max number of threads



/**
* Internal TM functions
*/

/**
* tm_reset_suspend_disable() reset <code>suspend_disable</code> to 0, and return old value. 
* It should be used with tm_set_suspend_disable() to implement safe points
* Will be used in tm_safe_point(), tm_mutex_lock(), tm_cond_wait().
*/

int reset_suspend_disable();
void set_suspend_disable(int count);

/* thin monitor functions used java monitor
 */
IDATA is_fat_lock(hythread_thin_monitor_t lockword);
IDATA owns_thin_lock(hythread_t thread, I_32 lockword);
hythread_monitor_t inflate_lock(hythread_thin_monitor_t *lockword_ptr);
IDATA unreserve_lock(hythread_thin_monitor_t *lockword_ptr);

IDATA VMCALL hythread_get_group(hythread_group_t *group, hythread_t thread);
/**
 *  Auxiliary function to throw java.lang.InterruptedException
 */

void throw_interrupted_exception(void);

hythread_group_t  get_java_thread_group(void);

/**
 * Thread cancellation, being used at VM shutdown through
 * tmj_cancel_all_threads() method call to terminate all java 
 * threads at shutdown.
 */

typedef void (*tm_thread_event_callback_proc)(void);
IDATA VMCALL set_safepoint_callback(hythread_t thread, tm_thread_event_callback_proc callback);

IDATA acquire_start_lock(void);
IDATA release_start_lock(void);

IDATA thread_sleep_impl(I_64 millis, IDATA nanos, IDATA interruptable);
IDATA condvar_wait_impl(hycond_t *cond, hymutex_t *mutex, I_64 ms, IDATA nano, IDATA interruptable);
IDATA monitor_wait_impl(hythread_monitor_t mon_ptr, I_64 ms, IDATA nano, IDATA interruptable);
IDATA thin_monitor_wait_impl(hythread_thin_monitor_t *lockword_ptr, I_64 ms, IDATA nano, IDATA interruptable);
IDATA sem_wait_impl(hysem_t sem, I_64 ms, IDATA nano, IDATA interruptable);

typedef struct ResizableArrayEntry {
    void *entry;
    UDATA next_free;
} ResizableArrayEntry;

typedef struct ResizableArrayEntry *array_entry_t; 

typedef struct ResizableArrayType {
    UDATA size;
    UDATA capacity;
    UDATA next_index;
    array_entry_t entries;
} ResizableArrayType;
typedef struct ResizableArrayType *array_t;


IDATA array_create(array_t *array);
IDATA array_destroy(array_t array);
UDATA array_add(array_t array, void *value);
void *array_delete(array_t array, UDATA index);
void *array_get(array_t array, UDATA index);

/**
 *  Auxiliary function to update thread count
 */
void thread_start_count();
void thread_end_count();

/*
 * portability functions, private for thread module
 */
int os_thread_create(osthread_t* phandle, UDATA stacksize, UDATA priority,
        int (VMAPICALL *func)(void*), void *data);
int os_thread_set_priority(osthread_t thread, int priority);
osthread_t os_thread_current();
int os_thread_cancel(osthread_t);
int os_thread_join(osthread_t);
void os_thread_exit(int status);
void os_thread_yield_other(osthread_t);
int os_get_thread_times(osthread_t os_thread, int64* pkernel, int64* puser);

int os_cond_timedwait(hycond_t *cond, hymutex_t *mutex, I_64 ms, IDATA nano);

#ifdef __cplusplus
}
#endif

#endif  /* THREAD_PRIVATE_H */
