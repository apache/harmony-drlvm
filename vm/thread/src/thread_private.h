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
 * @version $Revision: 1.1.2.14 $
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

// temporary remove logging
#define TRACE(a) //printf a; printf("\n")
//#include "clog.h"

// FIXME move to the global header, add error converter 
#define RET_ON_ERROR(stat) if(stat) { return -1; }
#define CONVERT_ERROR(stat)	(stat)

#define MAX_OWNED_MONITOR_NUMBER 200 //FIXME: switch to dynamic resize
#define FAST_LOCAL_STORAGE_SIZE 10

//use lock reservation
#define LOCK_RESERVATION
// spin with try_lock SPIN_COUNT times
#define SPIN_COUNT 5

//use optimized asm monitor enter and exit 
#define ASM_MONITOR_HELPER

// TLS access options
#ifdef WIN32
#define FS14_TLS_USE
#endif
//#define APR_TLS_USE



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
// optimization code
#if !defined (APR_TLS_USE ) && !defined (FS14_TLS_USE) 
#ifdef PLATFORM_POSIX
extern __thread hythread_t tm_self_tls;
#else
extern __declspec(thread) hythread_t tm_self_tls;
#endif
#else

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


/**
  * get_local_pool() function return apr pool asociated with the current thread.
  * the memory could be allocated without lock using this pool
  * dealocation should be done in the same thread, otherwise 
  * local_pool_cleanup_register() should be called
  */
 apr_pool_t* get_local_pool();
 
/**
  * local_pool_cleanup_register() synchroniously register the cleanup function.
  * It shold be called to request cleaunp in thread local pool, from other thread
  * Usage scenario:
  * IDATA hymutex_destroy (tm_mutex_t *mutex) {
  *        apr_pool_t *pool = apr_thread_mutex_pool_get ((apr_thread_mutex_t*)mutex);
  *        if(pool != get_local_pool()) {
  *              return local_pool_cleanup_register(hymutex_destroy, mutex);
  *      }
  *      apr_thread_mutex_destroy(mutex);
  *  return TM_ERROR_NONE;
  * }
  *  
  */
IDATA local_pool_cleanup_register(void* func, void* data); 



// Direct mappings to porting layer / APR 
#define HyCond apr_thread_cond_t
#define HyMutex apr_thread_mutex_t 
//#define tm_rwlock_t apr_rwlock_t 
//#define _tm_threadkey_t apr_threadkey_t 


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
} HyThreadLibrary;



/**
 * Native thread control structure.
 */
typedef struct HyThread {
// Suspension 
    
    /**
     * Number of suspend requests made for this thread.
     */
    IDATA suspend_request;

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
    apr_thread_t *os_handle;    
        
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

    /**
     * Event reserved for parking threads.
     */
    hysem_t park_event;
    
    /**
     * Event reserved for sleeping threads.
     */
    hysem_t sleep_event;    

    /**
     * Event reserved for threads that invoke join.
     */
    hylatch_t join_event;
    
    /**
     * Current conditional variable thread is waiting on (used for interrupting)
     */
    hycond_t current_condition;
           
              
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

    /**
     * Is this thread daemon?
     */
    IDATA daemon;
    

// Monitors
    
    /**
     * ID for this thread. The maximum number of threads is governed by the size of lockword record.
     */
    IDATA thread_id;

       

    /**
     * Memory pool in with this thread allocated
     */
    apr_pool_t *pool;

    /**
     * APR thread attributes
     */
    apr_threadattr_t *apr_attrs;

    
    /**
     * Procedure that describes thread body to be executed.
     */
    hythread_entrypoint_t start_proc;

    /**
     * Arguments to be passed to the thread body.
     */
    void *start_proc_args;

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
     * Blocked on monitor time in nanoseconds
     */
     jlong blocked_time;
       
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
      * weak reference to corresponding java.lang.Thread instace
      */
     jobject thread_ref;

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
     * Group index or key for search perposes
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
         * monitor sub pool
         * will be destroyed by monitor_destroy()
         */
        apr_pool_t *pool;
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
        /**
          * semaphore sub pool
          * will be destroyed by sem_destroy()
         */
        apr_pool_t *pool;     
} HySemaphore;

// Global variables 

extern hythread_group_t group_list; // list of thread groups
extern int groups_count; // number of thread groups

extern apr_pool_t *TM_POOL;           //global APR pool for thread manager

extern apr_threadkey_t *TM_THREAD_KEY; // Key used to store tm_thread_t structure in TLS

extern int max_group_index;     // max number of groups


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
IDATA owns_thin_lock(hythread_t thread, IDATA lockword);
hythread_monitor_t inflate_lock(hythread_thin_monitor_t *lockword_ptr);
IDATA unreserve_lock(IDATA *lockword_ptr);


/**
 *  Auxiliary function to throw java.lang.InterruptedException
 */

void throw_interrupted_exception(void);

hythread_group_t  get_java_thread_group(void);

/**
 * Thread cancelation, being used at VM shutdown through
 * tmj_cancel_all_threads() method call to terminate all java 
 * threads at shutdown.
 */

IDATA countdown_nondaemon_threads();
IDATA increase_nondaemon_threads_count();

IDATA VMCALL hythread_create_with_group(hythread_t *ret_thread, hythread_group_t group, UDATA stacksize, UDATA priority, UDATA suspend, hythread_entrypoint_t func, void *data);

typedef void (*tm_thread_event_callback_proc)(void);
IDATA VMCALL set_safepoint_callback(hythread_t thread, tm_thread_event_callback_proc callback);

IDATA acquire_start_lock(void);
IDATA release_start_lock(void);

IDATA thread_destroy(hythread_t thread);
IDATA VMCALL hythread_get_group(hythread_group_t *group, hythread_t thread);
IDATA thread_sleep_impl(I_64 millis, IDATA nanos, IDATA interruptable);
IDATA condvar_wait_impl(hycond_t cond, hymutex_t mutex, I_64 ms, IDATA nano, IDATA interruptable);
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



#ifdef __cplusplus
}
#endif

#endif  /* THREAD_PRIVATE_H */
