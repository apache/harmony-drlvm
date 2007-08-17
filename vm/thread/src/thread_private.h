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
//#define TRACE(a) //printf a; printf("\n")

#ifdef __linux__
#include "clog.h"
#else
#define TRACE(a) //printf a; printf("\n")
#define DIE(A) //exit(55);
#endif

// FIXME move to the global header, add error converter 
#define RET_ON_ERROR(stat) if (stat) { return -1; }
#define CONVERT_ERROR(stat)     (stat)

#define FAST_LOCAL_STORAGE_SIZE 10

#define HY_FAT_LOCK_ID_OFFSET 11     // fat lock ID offset within lockword
#define HY_FAT_LOCK_ID_MASK 0xFFFFF  // nonzero bits (starting from 0 bit) to mask fat lock ID

#define HY_FAT_TABLE_ENTRIES (16*1024)   // fat lock table is exapandible by adding new table
#define HY_MAX_FAT_LOCKS (HY_FAT_LOCK_ID_MASK + 1)// max total count of fat locks
// max count of tables for exapansion
#define HY_MAX_FAT_TABLES ((HY_MAX_FAT_LOCKS + HY_FAT_TABLE_ENTRIES - 1)/HY_FAT_TABLE_ENTRIES)


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


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

// Public fields exported by HyThread_public. If you change these fields,
// please, check fields in hythread.h/HyThread_public

    /**
     * Number of requests made for this thread, it includes both
     * suspend requests and safe point callback requests.
     * The field is modified by atomic operations.
     *
     * Increment in functions:
     *    1. send_suspend_request()
     *          - sets suspend request for a given thread
     *    2. hythread_set_safepoint_callback()
     *          - sets safe point callback request for a given thread
     *
     * Decrement in functions:
     *    1. hythread_resume()
     *          - removes suspend request for a given thread
     *    2. hythread_exception_safe_point()
     *          - removes safe point callback request for current thread
     */
    uint32 request;

    /**
     * Field indicating that thread can safely be suspended.
     * Safe suspension is enabled on value 0.
     *
     * The disable_count is increased/decreaded in
     * hythread_suspend_disable()/hythread_suspend_enable() function
     * for current thread only.
     *
     * Also disable_count could be reset to value 0 and restored in
     * hythread_set_suspend_disable()/hythread_set_suspend_disable() function
     * for current thread only.
     *
     * Function hythread_exception_safe_point() sets disable_count to
     * value 1 before safe point callback function calling and restores
     * it after the call.
     *
     * Function thread_safe_point_impl() sets disable_count to
     * value 0 before entering to the safe point and restores it
     * after exitting.
     */
    int16 disable_count;

    /**
     * Group for this thread. Different groups are needed in order 
     * to be able to quickly iterate over the specific group.
     * Examples are: Java threads, GC private threads.
     * Equal to the address of the head of the list of threads for this group.
     */
    hythread_group_t  group; 

    /**
     * Array representing thread local storage
     */
    void *thread_local_storage[10];


// Private fields

    /**
     * Each thread keeps a pointer to the library it belongs to.
     */
    HyThreadLibrary * library;

// Suspension 
    
    /**
     * Number of suspend requests made for this thread.
     * The field is modified by atomic operations.
     *
     * After increment/decrement of suspend_count, request field
     * should be incremented/decremented too.
     */
    uint32 suspend_count;
        
    /**
     * Function to be executed at safepoint upon thread resume.
     *
     * Field is set in hythread_set_safepoint_callback() function
     * and reset hythread_exception_safe_point() function.
     *
     * After set/reset of safepoint_callback, request field
     * should be incremented/decremented too.
     */
    hythread_event_callback_proc safepoint_callback;

    /**
     * Event used to notify suspended thread that it needs to wake up.
     */
    hysem_t resume_event;

// Basic manipulation fields
    
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
    * Hint for scheduler about thread priority
    */
    IDATA priority;    

    /**
     * Size of thread's stack, set on creation
     */

    UDATA stacksize;

// Monitors
    
    /**
     *  Monitor this thread is waiting on now.
     */
    hythread_monitor_t waited_monitor;

    /**
     * ID for this thread. The maximum number of threads is governed by the size of lockword record.
     */
    IDATA thread_id;

} HyThread;

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
    
    /// Monitor mutex.
    hymutex_t mutex;

    /// Monitor condition varibale.
    hycond_t condition;
    
    /// Mutex recurtion count.
    IDATA recursion_count;

    /// Current mutex owner.
    hythread_t owner;

    /// Number of threads waiting on a condition variable
    /// or queued to acquire a monitor mutex after wakeup
    int wait_count;

    /// Number of notify events sent by the user,
    /// it is bounded by the wait_count
    int notify_count;

    /// Owner thread ID. 
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

  
/*
 * Lock table which holds the mapping between LockID and fat lock 
 * (OS fat_monitor) pointer.
 */

typedef enum hythread_locktable_state { 
    HYTHREAD_LOCKTABLE_IDLE, 
    HYTHREAD_LOCKTABLE_READING, 
    HYTHREAD_LOCKTABLE_WRITING 
} hythread_locktable_state_t;
    
typedef struct HyFatLockTable {
    // locktable itself
    hythread_monitor_t* tables[HY_MAX_FAT_TABLES];
    
    // mutex guarding locktable
    hymutex_t mutex;
    hycond_t read;
    hycond_t write;
    
    int readers_reading;
    int readers_waiting;
    int writers_waiting;
    
    hythread_locktable_state_t state;
  
    U_32 read_count;
    
    // table of live objects (updated during each major GC)
    unsigned char *live_objs;
    
    // size of locktable
    U_32 size;

    // used to scan the lock table for the next available entry
    U_32 array_cursor;
    
} HyFatLockTable;


// Global variables 

extern hythread_group_t group_list; // list of thread groups
extern IDATA groups_count; // number of thread groups

extern apr_pool_t *TM_POOL;           //global APR pool for thread manager

extern apr_threadkey_t *TM_THREAD_KEY; // Key used to store tm_thread_t structure in TLS

extern int max_group_index;     // max number of groups

extern HyFatLockTable *lock_table;

#define THREAD_ID_SIZE 16  //size of thread ID in bits. Also defines max number of threads



/**
* Internal TM functions
*/
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

/*
 * portability functions, private for thread module
 */
int os_thread_create(osthread_t* phandle, UDATA stacksize, UDATA priority,
        int (VMAPICALL *func)(void*), void *data);
int os_thread_set_priority(osthread_t thread, int priority);
osthread_t os_thread_current();
int os_thread_cancel(osthread_t);
void os_thread_exit(IDATA status);
void os_thread_yield_other(osthread_t);
int os_get_thread_times(osthread_t os_thread, int64* pkernel, int64* puser);

int os_cond_timedwait(hycond_t *cond, hymutex_t *mutex, I_64 ms, IDATA nano);
UDATA os_get_foreign_thread_stack_size();

#ifdef __cplusplus
}
#endif

#endif  /* THREAD_PRIVATE_H */
