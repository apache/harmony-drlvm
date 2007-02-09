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
 * @file thread_native_thin_monitor.c
 * @brief Hythread thin monitors related functions
 */

#undef LOG_DOMAIN
#define LOG_DOMAIN "tm.locks"

#include <open/hythread_ext.h>
#include <open/thread_externals.h>
#include "thread_private.h"
#include <apr_atomic.h>
#include <port_atomic.h>

/** @name Thin monitors support. Implement thin-fat scheme.
 */
//@{

/*
 * 32bit lock word
 *           |0|------15bit-------|--5bit--|1bit|--10bit------|
 * thin lock -^  threadID (owner)  recursion  ^  hashcode  
 *                                            |
 *                               reservation bit (0 - reserved)
 * inflated lock:
 *           |1|------------- 20bit -------|----11bit-----|
 *  fat lock -^            fat lock id
 */

// lockword operations
#define THREAD_ID(lockword) (lockword >> 16)
#define IS_FAT_LOCK(lockword) (lockword >> 31)
#define FAT_LOCK_ID(lockword) ((lockword << 1) >> 12)
// lock reservation support
#define RESERVED_BITMASK ((1<<10))
#define IS_RESERVED(lockword) (0==(lockword & RESERVED_BITMASK))

#define RECURSION(lockword) ((lockword >> 11) & 31)
#define RECURSION_INC(lockword_ptr, lockword) (*lockword_ptr= lockword + (1<<11))
#define RECURSION_DEC(lockword_ptr, lockword) (*lockword_ptr=lockword - (1<<11))
#define MAX_RECURSION 31

IDATA owns_thin_lock(hythread_t thread, I_32 lockword) {
    IDATA this_id = thread->thread_id;
    assert(!IS_FAT_LOCK(lockword));
#ifdef LOCK_RESERVATION
    return THREAD_ID(lockword) == this_id
        && (!IS_RESERVED(lockword) || RECURSION(lockword) !=0);
#else
    return THREAD_ID(lockword) == this_id;
#endif
}

void set_fat_lock_id(hythread_thin_monitor_t *lockword_ptr, IDATA monitor_id) {
    I_32 lockword = *lockword_ptr;
#ifdef LOCK_RESERVATION
    assert(!IS_RESERVED(lockword));
#endif
    lockword&=0x7FF;
    lockword|=(monitor_id << 11) | 0x80000000;
    *lockword_ptr=lockword;
    apr_memory_rw_barrier();
}

IDATA is_fat_lock(hythread_thin_monitor_t lockword) {
        return IS_FAT_LOCK(lockword);
}

//forward declaration
hythread_monitor_t locktable_get_fat_monitor(IDATA lock_id);
IDATA locktable_put_fat_monitor(hythread_monitor_t fat_monitor);
hythread_monitor_t locktable_delete_entry(int lock_id);
hythread_monitor_t inflate_lock(hythread_thin_monitor_t *lockword_ptr);

//DEBUG INFO BLOCK
//char *vm_get_object_class_name(void* ptr);
int unreserve_count=0;
int inflate_contended=0;
int inflate_waited=0;
int unreserve_count_self=0;
int inflate_count=0;
int fat_lock2_count = 0;
int init_reserve_cout = 0;
int cas_cout = 0;
int res_lock_count = 0;

#ifdef LOCK_RESERVATION

extern hymutex_t TM_LOCK;
/*
 * Unreserves the lock already owned by this thread
 */
void unreserve_self_lock(hythread_thin_monitor_t *lockword_ptr) {
    I_32 lockword = *lockword_ptr;
    I_32 lockword_new;
    TRACE(("unreserve self_id %d lock owner %d", hythread_get_id(hythread_self()), THREAD_ID(lockword)));
    assert(hythread_get_id(hythread_self()) == THREAD_ID(lockword));
    assert(!IS_FAT_LOCK(*lockword_ptr));
    assert(IS_RESERVED(lockword));
    TRACE(("Unreserved self %d \n", ++unreserve_count_self/*, vm_get_object_class_name(lockword_ptr-1)*/));  
       
    // Set reservation bit to 1 and reduce recursion count
    lockword_new = (lockword | RESERVED_BITMASK);
    if (RECURSION(lockword_new) != 0) {
        RECURSION_DEC(lockword_ptr, lockword_new);
    } else {
        lockword_new  = lockword_new & 0x0000ffff;
        *lockword_ptr = lockword_new;
    }
    assert(!IS_RESERVED(*lockword_ptr));
    TRACE(("unreserved self"));
}

/*
 * Used lockword
 */
IDATA unreserve_lock(hythread_thin_monitor_t *lockword_ptr) {
    I_32 lockword = *lockword_ptr;
    I_32 lockword_new;
    uint16 lock_id;
    hythread_t owner;
    IDATA status;
    // trylock used to prevent cyclic suspend deadlock
    // the java_monitor_enter calls safe_point between attempts.
    /*status = hymutex_trylock(TM_LOCK);
    if (status !=TM_ERROR_NONE) {
        return status;
    }*/
    
    if (IS_FAT_LOCK(lockword)) {
        return TM_ERROR_NONE;
    }
    lock_id = THREAD_ID(lockword);
    owner = hythread_get_thread(lock_id);
    TRACE(("Unreserved other %d \n", ++unreserve_count/*, vm_get_object_class_name(lockword_ptr-1)*/));
    if (!IS_RESERVED(lockword) || IS_FAT_LOCK(lockword)) {
        // hymutex_unlock(TM_LOCK);
        return TM_ERROR_NONE;
    }
    // suspend owner 
    if (owner) {
        assert(owner);
        assert(hythread_get_id(owner) == lock_id);
        assert(owner != hythread_self());
        status=hythread_suspend_other(owner);
        if (status !=TM_ERROR_NONE) {
             return status;
        }
    }
    // prepare new unreserved lockword and try to CAS it with old one.
    while (IS_RESERVED(lockword)) {
        assert(!IS_FAT_LOCK(lockword));
        TRACE(("unreserving lock"));
        lockword_new = (lockword | RESERVED_BITMASK);
        if (RECURSION(lockword) != 0) {
            assert(RECURSION(lockword) > 0);
            assert(RECURSION(lockword_new) > 0);
            RECURSION_DEC(&lockword_new, lockword_new);
        } else {
            lockword_new =  lockword_new & 0x0000ffff; 
        }
        if (lockword == apr_atomic_cas32 (((volatile apr_uint32_t*) lockword_ptr), 
                (apr_uint32_t) lockword_new, lockword)) {
            TRACE(("unreserved lock"));
            break;
        }
        lockword = *lockword_ptr;
    }

    // resume owner
    if (owner) {
        apr_thread_yield_other(owner->os_handle);
        hythread_resume(owner);
    }

    /* status = hymutex_unlock(TM_LOCK);*/

    // Gregory - This lock, right after it was unreserved, may be
    // inflated by another thread and therefore instead of recursion
    // count and reserved flag it will have the fat monitor ID. The
    // assertion !IS_RESERVED(lockword) fails in this case. So it is
    // necessary to check first that monitor is not fat.
    // To avoid race condition between checking two different
    // conditions inside of assert, the lockword contents has to be
    // loaded before checking.
    lockword = *lockword_ptr;
    assert(IS_FAT_LOCK(lockword) || !IS_RESERVED(lockword));
    return TM_ERROR_NONE;
}
#else
IDATA unreserve_lock(I_32* lockword_ptr) {
    return TM_ERROR_NONE;
}
#endif 

/**
 * Initializes a thin monitor at the given address.
 * Thin monitor is a version of monitor which is optimized for space and
 * single-threaded usage.
 *
 * @param[in] lockword_ptr monitor addr 
 */
IDATA hythread_thin_monitor_create(hythread_thin_monitor_t *lockword_ptr) {
    //clear anything but hashcode
    // 000000000000000000011111111111
    *lockword_ptr = *lockword_ptr & 0x3FF; 
    return TM_ERROR_NONE;
}

/**
 * Attempts to lock thin monitor.
 * If the monitor is already locked, this call returns immediately with TM_BUSY.  
 * 
 * @param[in] lockword_ptr monitor addr 
 */
IDATA hythread_thin_monitor_try_enter(hythread_thin_monitor_t *lockword_ptr) {
    I_32 lockword;
    // warkaround strange intel compiler bug 
    #if defined (__INTEL_COMPILER) && defined (LINUX)
    volatile
    #endif
    IDATA this_id = tm_self_tls->thread_id;
    IDATA lock_id;
    IDATA status;
    hythread_monitor_t fat_monitor;
    int UNUSED i;
    assert(!hythread_is_suspend_enabled());
    assert((UDATA)lockword_ptr > 4);    
    assert(tm_self_tls);
    lockword = *lockword_ptr;       
    lock_id = THREAD_ID(lockword);
    //TRACE(("try lock %x %d", this_id, RECURSION(lockword)));

    // Check if the lock is already reserved or owned by this thread
    if (lock_id == this_id) {    
        if (RECURSION(lockword) == MAX_RECURSION) {
            //inflate lock in case of recursion overflow
            fat_monitor =inflate_lock(lockword_ptr);
            return hythread_monitor_try_enter(fat_monitor);
            //break FAT_LOCK;
        } else {
            TRACE(("try lock %x count:%d", this_id, res_lock_count++)); 
            // increase recursion
            RECURSION_INC(lockword_ptr, lockword);
            return TM_ERROR_NONE;
        }        
    } 

    // Fast path didn't work, someoneelse is holding the monitor (or it isn't reserved yet):   

    // DO SPIN FOR A WHILE, this will decrease the number of fat locks.
#ifdef SPIN_COUNT
    for (i = SPIN_COUNT; i >=0; i--, lockword = *lockword_ptr, lock_id = THREAD_ID(lockword)) { 
#endif

        // Check if monitor is free and thin
        if (lock_id == 0) {
            // Monitor is free
            assert( RECURSION(lockword) < 1);
            assert(this_id > 0 && this_id < 0x8000); 
            // Acquire monitor
            if (0 != port_atomic_cas16 (((volatile apr_uint16_t*) lockword_ptr)+1, 
                    (apr_uint16_t) this_id, 0)) {

#ifdef SPIN_COUNT
                continue; 
#else
                return TM_ERROR_EBUSY;
#endif
            }

#ifdef LOCK_RESERVATION
            //lockword = *lockword_ptr; // this reloading of lockword may be odd, need to investigate;
            if (IS_RESERVED(lockword)) {
                    TRACE(("initially reserve lock %x count: %d ", *lockword_ptr, init_reserve_cout++));
                    RECURSION_INC(lockword_ptr, *lockword_ptr);
            }
#endif
            TRACE(("CAS lock %x count: %d ", *lockword_ptr, cas_cout++));
            return TM_ERROR_NONE;
        } else 

        // Fat monitor
        if (IS_FAT_LOCK(lockword)) {
            TRACE(("FAT MONITOR %d \n", ++fat_lock2_count/*, vm_get_object_class_name(lockword_ptr-1)*/));  
            fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword)); //  find fat_monitor in lock table

            status = hythread_monitor_try_enter(fat_monitor);
#ifdef SPIN_COUNT
            if (status == TM_ERROR_EBUSY) {
                continue; 
            }
#endif
            return status;
        }

#ifdef LOCK_RESERVATION
        // unreserved busy lock
        else if (IS_RESERVED(lockword)) {
            status = unreserve_lock(lockword_ptr);
            if (status != TM_ERROR_NONE) {
#ifdef SPIN_COUNT
                if (status == TM_ERROR_EBUSY) {
                    continue;
                }
#endif //SPIN_COUNT
                return status;
            }
            assert(!IS_RESERVED(*lockword_ptr));
            return hythread_thin_monitor_try_enter(lockword_ptr);
        }
#endif 
#ifdef SPIN_COUNT
        hythread_yield();
    }
#endif
    return TM_ERROR_EBUSY;
}


/**
 * Locks thin monitor.
 * 
 * @param[in] lockword_ptr monitor addr 
 */
IDATA hythread_thin_monitor_enter(hythread_thin_monitor_t *lockword_ptr) {
    hythread_monitor_t fat_monitor;
    IDATA status; 
    int saved_disable_count;

    assert(lockword_ptr);    

    if (hythread_thin_monitor_try_enter(lockword_ptr) == TM_ERROR_NONE) {
        return TM_ERROR_NONE;
    }

    while (hythread_thin_monitor_try_enter(lockword_ptr) == TM_ERROR_EBUSY) {
        if (IS_FAT_LOCK(*lockword_ptr)) {
            fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(*lockword_ptr)); //  find fat_monitor in lock table
            TRACE((" lock %d\n", FAT_LOCK_ID(*lockword_ptr)));
            saved_disable_count=reset_suspend_disable();
            status = hythread_monitor_enter(fat_monitor);
            set_suspend_disable(saved_disable_count);
            return status; // lock fat_monitor
        } 
        //hythread_safe_point();
        hythread_yield();
    }
    if (IS_FAT_LOCK(*lockword_ptr)) {
        // lock already inflated
        return TM_ERROR_NONE;
    }
    TRACE(("inflate_contended  thin_lcok%d\n", ++inflate_contended));   
    inflate_lock(lockword_ptr);
    return TM_ERROR_NONE;
}

/**
 * Unlocks thin monitor.
 * 
 * @param[in] lockword_ptr monitor addr 
 */
IDATA VMCALL hythread_thin_monitor_exit(hythread_thin_monitor_t *lockword_ptr) {
    I_32 lockword = *lockword_ptr;
    hythread_monitor_t fat_monitor;
    IDATA this_id = tm_self_tls->thread_id; // obtain current thread id   
    assert(this_id > 0 && this_id < 0xffff);
    assert(!hythread_is_suspend_enabled());

    if (THREAD_ID(lockword) == this_id) {
        if (RECURSION(lockword)==0) {
#ifdef LOCK_RESERVATION
            if (IS_RESERVED(lockword)) {
                TRACE(("ILLEGAL_STATE %x\n", lockword));
                return TM_ERROR_ILLEGAL_STATE;
            }
#endif
            *lockword_ptr = lockword & 0xffff;
        } else {
            RECURSION_DEC(lockword_ptr, lockword);
            //TRACE(("recursion_dec: 0x%x", *lockword_ptr)); 
        }
        //TRACE(("unlocked: 0x%x id: %d\n", *lockword_ptr, THREAD_ID(*lockword_ptr)));
                //hythread_safe_point();
        return TM_ERROR_NONE;     
    }  else if (IS_FAT_LOCK(lockword)) {
        TRACE(("exit fat monitor %d thread: %d\n", FAT_LOCK_ID(lockword), tm_self_tls->thread_id));
        fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword)); // find fat_monitor
        return hythread_monitor_exit(fat_monitor); // unlock fat_monitor
    }
    TRACE(("ILLEGAL_STATE %d\n", FAT_LOCK_ID(lockword)));
    return TM_ERROR_ILLEGAL_STATE;
}


IDATA thin_monitor_wait_impl(hythread_thin_monitor_t *lockword_ptr, I_64 ms, IDATA nano, IDATA interruptable) {
    // get this thread
    hythread_t this_thread = tm_self_tls;    
    I_32 lockword = *lockword_ptr;
    hythread_monitor_t fat_monitor;
  
    if (!IS_FAT_LOCK(lockword)) {
        // check if the current thread owns lock
        if (!owns_thin_lock(this_thread, lockword)) {
            TRACE(("ILLEGAL_STATE %wait d\n", FAT_LOCK_ID(lockword)));
            return TM_ERROR_ILLEGAL_STATE;  
        }    
        TRACE(("inflate_wait%d\n", ++inflate_waited));  
        // if it is not a thin lock, inflate it
        fat_monitor = inflate_lock(lockword_ptr);
    } else {
        // otherwise, get the appropriate fat lock
        fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword));
    }
    assert(fat_monitor == locktable_get_fat_monitor(FAT_LOCK_ID(*lockword_ptr)));
    return monitor_wait_impl(fat_monitor,ms,nano, interruptable);    
}

/**
 * Atomically releases a thin monitor and causes the calling 
 * thread to wait on the given condition variable.
 *
 * Calling thread must own the monitor. After notification is received, the monitor
 * is locked again, restoring the original recursion.
 *
 * @param[in] lockword_ptr monitor addr 
 * @return  
 *      TM_NO_ERROR on success 
 */
IDATA VMCALL hythread_thin_monitor_wait(hythread_thin_monitor_t *lockword_ptr) {
    return thin_monitor_wait_impl(lockword_ptr, 0, 0, WAIT_NONINTERRUPTABLE);    
}       

/**
 * Atomically releases a thin monitor and causes the calling 
 * thread to wait for signal.
 *
 * Calling thread must own the monitor. After notification is received or time out, the monitor
 * is locked again, restoring the original recursion.
 *
 * @param[in] lockword_ptr monitor addr
 * @param[in] ms timeout millis
 * @param[in] nano timeout nanos
 * @return  
 *      TM_NO_ERROR on success 
 *      TM_ERROR_TIMEOUT in case of time out.
 */
IDATA VMCALL hythread_thin_monitor_wait_timed(hythread_thin_monitor_t *lockword_ptr, I_64 ms, IDATA nano) {
    return thin_monitor_wait_impl(lockword_ptr, ms, nano, WAIT_NONINTERRUPTABLE);    
}

/**
 * Atomically releases a thin monitor and causes the calling 
 * thread to wait for signal.
 *
 * Calling thread must own the monitor. After notification is received or time out, the monitor
 * is locked again, restoring the original recursion.
 *
 * @param[in] lockword_ptr monitor addr
 * @param[in] ms timeout millis
 * @param[in] nano timeout nanos
 * @return  
 *      TM_NO_ERROR on success 
 *      TM_THREAD_INTERRUPTED in case thread was interrupted during wait.
 *      TM_ERROR_TIMEOUT in case of time out.
 */
IDATA VMCALL hythread_thin_monitor_wait_interruptable(hythread_thin_monitor_t *lockword_ptr, I_64 ms, IDATA nano) {
    return thin_monitor_wait_impl(lockword_ptr, ms, nano, WAIT_INTERRUPTABLE);    
}


/**
 * Signals a single thread that is blocking on the given monitor to wake up. 
 *
 * @param[in] lockword_ptr monitor addr
 */
IDATA hythread_thin_monitor_notify(hythread_thin_monitor_t *lockword_ptr) {
    hythread_monitor_t fat_monitor;
    hythread_thin_monitor_t lockword = *lockword_ptr;
    if (IS_FAT_LOCK(lockword)) {
         fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword));
         assert(fat_monitor);
         return hythread_monitor_notify(fat_monitor); 
    }
    // check if the current thread owns lock
    if (!owns_thin_lock(tm_self_tls, lockword)) {
        return TM_ERROR_ILLEGAL_STATE;  
    }    
  
    return TM_ERROR_NONE;
}

/**
 * Signals all threads blocking on the given thin monitor.
 * 
 * @param[in] lockword_ptr monitor addr
 */
IDATA hythread_thin_monitor_notify_all(hythread_thin_monitor_t *lockword_ptr) {
    hythread_monitor_t fat_monitor;
    hythread_thin_monitor_t lockword = *lockword_ptr;
    if (IS_FAT_LOCK(lockword)) {
         fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword));
         assert(fat_monitor);
         return hythread_monitor_notify_all(fat_monitor); 
    }
    // check if the current thread owns lock
    if (!owns_thin_lock(tm_self_tls, lockword)) {
        return TM_ERROR_ILLEGAL_STATE;  
    }    
    return TM_ERROR_NONE;
}

/**
 * Destroys the thin monitor and releases any associated resources.
 *
 * @param[in] lockword_ptr monitor addr 
 */
IDATA hythread_thin_monitor_destroy(hythread_thin_monitor_t *lockword_ptr) {
    hythread_monitor_t fat_monitor;
    hythread_thin_monitor_t lockword = *lockword_ptr;
    
    if (IS_FAT_LOCK(lockword)) {
         fat_monitor = locktable_delete_entry(FAT_LOCK_ID(lockword));
         assert(fat_monitor);
         return hythread_monitor_destroy(fat_monitor); 
    }
    return TM_ERROR_NONE;
}

extern hymutex_t FAT_MONITOR_TABLE_LOCK;

/*
 * Inflates the compressed lockword into fat fat_monitor
 */
hythread_monitor_t VMCALL inflate_lock(hythread_thin_monitor_t *lockword_ptr) {
    hythread_monitor_t fat_monitor;
    IDATA status;
    IDATA fat_monitor_id;
    I_32 lockword;
    int i;
    status=hymutex_lock(FAT_MONITOR_TABLE_LOCK);
    assert(status == TM_ERROR_NONE);
    TRACE(("inflate tmj%d\n", ++inflate_count));
    lockword = *lockword_ptr;
    if (IS_FAT_LOCK (lockword)) {
        status = hymutex_unlock(FAT_MONITOR_TABLE_LOCK);
        assert(status == TM_ERROR_NONE);
        return locktable_get_fat_monitor(FAT_LOCK_ID(lockword));
    }
#ifdef LOCK_RESERVATION
    // unreserve lock first
    if (IS_RESERVED(lockword)) {
        unreserve_self_lock(lockword_ptr);
        lockword = *lockword_ptr;
    }
    assert(!IS_RESERVED(lockword));
#endif 

    assert(owns_thin_lock(tm_self_tls, lockword));
    assert(!hythread_is_suspend_enabled());

    TRACE (("inflation begin for %x thread: %d", lockword, tm_self_tls->thread_id));
    status = hythread_monitor_init(&fat_monitor, 0); // allocate fat fat_monitor    
    assert(status == TM_ERROR_NONE);  
    status = hythread_monitor_enter(fat_monitor);
    if (status != TM_ERROR_NONE) {
        hymutex_unlock(FAT_MONITOR_TABLE_LOCK);
        return NULL;
    } 
    
    for (i = RECURSION(lockword); i > 0; i--) {
        TRACE( ("inflate recursion monitor"));
        status = hythread_monitor_enter(fat_monitor);  // transfer recursion count to fat fat_monitor   
        assert(status == TM_ERROR_NONE);     
    }     
    fat_monitor_id = locktable_put_fat_monitor(fat_monitor); // put fat_monitor into lock table
    set_fat_lock_id(lockword_ptr, fat_monitor_id);
    TRACE(("inflate_lock  %d thread: %d\n", FAT_LOCK_ID(*lockword_ptr), tm_self_tls->thread_id));
    //assert(FAT_LOCK_ID(*lockword_ptr) != 2);
    TRACE(("FAT ID : 0x%x", *lockword_ptr));
    fat_monitor->inflate_count++;
    fat_monitor->inflate_owner=tm_self_tls;
    status=hymutex_unlock(FAT_MONITOR_TABLE_LOCK);
    assert(status == TM_ERROR_NONE);
#ifdef LOCK_RESERVATION
    assert(!IS_RESERVED(*lockword_ptr));
#endif
    return fat_monitor;
}


/*
 * Deflates the fat lock back into compressed lock word.
 * Not yet implemented.
 */
void deflate_lock(hythread_monitor_t fat_monitor, hythread_thin_monitor_t *lockword) {
/*
    ;;// put thread_id into lockword
    //... TODO
    RECURSION_COUNT(lockword) = fat_monitor->recursion_count; // Transfer recursion count from fat lock back to thin lock
    IS_FAT_LOCK(lockword) = 0; // Set contention bit indicating that the lock is now thin
    hythread_monitor_destroy(fat_monitor); // Deallocate fat_monitor
    locktable_delete_entry(lock_id); // delete entry in lock able

*/
}



/*
 * Lock table which holds the mapping between LockID and fat lock (OS fat_monitor) pointer.
 */

//FIXME: make table resizable, implement delete
 
extern int table_size;
extern hythread_monitor_t *lock_table;
int fat_monitor_count = 1;
// Lock table implementation



/*
 * Returns the OS fat_monitor associated with the given lock id.
 */
hythread_monitor_t locktable_get_fat_monitor(IDATA lock_id) {
    hythread_monitor_t fat_monitor;
    TRACE(("LOCK ID in table %x\n", lock_id));
    assert(lock_id >=0 && lock_id < table_size);
    //hythread_global_lock();
    fat_monitor = lock_table[lock_id];
    //hythread_global_unlock();
    return fat_monitor;
}

/*
 * Sets the value of the specific entry in the lock table
 */
IDATA locktable_put_fat_monitor(hythread_monitor_t fat_monitor) {
    int id;
    //hythread_global_lock();
    id = fat_monitor_count++;
    if (id >= table_size) {
        hythread_suspend_all(NULL, NULL); 
        table_size = table_size*2;
        lock_table = (hythread_monitor_t *)realloc(lock_table, sizeof(hythread_monitor_t)*table_size);
        assert(lock_table);
        apr_memory_rw_barrier();
        hythread_resume_all(NULL);
    }

    lock_table[id] = fat_monitor;
    //hythread_global_unlock();
    return id;
} 

/*
 * Deletes the entry in the lock table with the given lock_id
 */
hythread_monitor_t  locktable_delete_entry(int lock_id) {  
    hythread_monitor_t  m;
    assert(lock_id >=0 && lock_id < table_size);
    m = lock_table[lock_id];
    lock_table[lock_id] = NULL;
    return m;
}

/**
 * Returns the owner of the given thin monitor.
 *
 * @param[in] lockword_ptr monitor addr 
 */
hythread_t VMCALL hythread_thin_monitor_get_owner(hythread_thin_monitor_t *lockword_ptr) {
    I_32 lockword;
    hythread_monitor_t fat_monitor;
    
    assert(lockword_ptr);    
    lockword = *lockword_ptr;       
    if (IS_FAT_LOCK(lockword)) {
        fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword)); //  find fat_monitor in lock table
        return fat_monitor->owner;
    }

    if (THREAD_ID(lockword)== 0) {
         return NULL;
    }

#ifdef LOCK_RESERVATION
    if (RECURSION(lockword)==0 && IS_RESERVED(lockword)) {
         return NULL;
    }
#endif
    return hythread_get_thread(THREAD_ID(lockword));
}

/**
 * Returns the recursion count of the given monitor.
 *
 * @param[in] lockword_ptr monitor addr 
 */
IDATA VMCALL hythread_thin_monitor_get_recursion(hythread_thin_monitor_t *lockword_ptr) {
    I_32 lockword;
    hythread_monitor_t fat_monitor;
    
    assert(lockword_ptr);    
    lockword = *lockword_ptr;       
    if (IS_FAT_LOCK(lockword)) {
        fat_monitor = locktable_get_fat_monitor(FAT_LOCK_ID(lockword)); //  find fat_monitor in lock table
        return fat_monitor->recursion_count+1;
    }
    if (THREAD_ID(lockword) == 0) {
        return 0;
    }
#ifdef LOCK_RESERVATION
    if (IS_RESERVED(lockword)) {
         return RECURSION(lockword);
    }
#endif
    return RECURSION(lockword)+1;
}

//@}
