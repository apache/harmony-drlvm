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


#ifndef MON_ENTER_EXIT_HEADER
#define MON_ENTER_EXIT_HEADER

#include "object_layout.h"
#include "vm_threads.h"

#include "open/vm_gc.h"

#ifdef __cplusplus
extern "C" {
#endif


//      object header bits layout
//
//      31..............16      15......8           7......0
//      stack_key               recursion_count      ^^^^^^^7-bit hash
//                                                  ^contention_bit 

#define FREE_MONITOR 0

#define P_HASH_CONTENTION_BYTE(x) ( (uint8 *)(x->get_obj_info_addr()) )
#define P_STACK_KEY_SHORT(x)      ( (uint16 *)((((ManagedObject*)x)->get_obj_info_addr())) + 1 )

#define P_HASH_CONTENTION(x)      P_HASH_CONTENTION_BYTE(x)
#define P_RECURSION(x)            P_RECURSION_BYTE(x)
#define P_STACK_KEY(x)            P_STACK_KEY_SHORT(x)

#define HASH_CONTENTION(x)        ( *P_HASH_CONTENTION(x) )
#define RECURSION(x)              ( *P_RECURSION(x) )
#define STACK_KEY(x)              ( *P_STACK_KEY(x) )



// This is called once at startup, before any classes are loaded,
// and after arguments are parsed.  It should set function pointers
// to the appropriate values.
void vm_monitor_init();

// Tries to acquire the lock, but will not block or allow GC to
// happen if it is already locked by another thread.
// It is only called from block_on_mon_enter() which is called
// from vm_monitor_enter_slow() and thread_object_wait(),
// which basically means that it is not part of the general
// locking interface.
Boolean vm_try_monitor_enter(ManagedObject *p_obj);

// Does a monitorexit operation.
extern void (*vm_monitor_exit)(ManagedObject *p_obj);
extern void (*vm_monitor_exit_handle)(jobject jobj);

// Does a monitorenter, possibly blocking if the object is already
// locked.
void vm_monitor_enter_slow(ManagedObject *p_obj);
VMEXPORT void vm_monitor_enter_slow_handle (jobject jobj);

extern void (*vm_monitor_enter)(ManagedObject *p_obj);

// Called only from vm_monitor_enter_slow() and thread_object_wait(),
// which basically means that it is not part of the general
// locking interface.
void block_on_mon_enter(jobject obj);

// Tries to find a thread that is waiting on the lock for p_obj.
// If one is found, it is unblocked so that it can try again to
// acquire the lock.
void find_an_interested_thread(ManagedObject *p_obj);

#define RECURSION_OFFSET 1
#define STACK_KEY_OFFSET 2
#define INPUT_ARG_OFFSET 4
#define HASH_CONTENTION_AND_RECURSION_OFFSET 0
#define CONTENTION_MASK 0x80
#define HASH_MASK 0x7e

#define SPIN_LOOP_COUNT 0x2

void set_hash_bits(ManagedObject *p_obj);

typedef struct mon_enter_fields {
    ManagedObject *p_obj;
    VM_thread *p_thr;
} mon_enter_fields;

extern mon_enter_fields mon_enter_array[];
extern ManagedObject *obj_array[];

typedef struct mon_wait_fields {
    ManagedObject *p_obj;
    VM_thread *p_thr;
} mon_wait_fields;

extern mon_wait_fields mon_wait_array[];

#ifdef _DEBUG
extern VM_thread *thread_array_for_debugging_only[];
extern int max_notifyAll;
extern int max_recursion;
extern int total_sleep_timeouts;
extern int total_sleep_interrupts;
extern int total_wait_timeouts;
extern int total_wait_interrupts;
extern int total_illegal_mon_state_exceptions;
extern int iterations_per_thread[];
extern int max_block_on_mon_enter_loops;
#endif

extern volatile int active_thread_count;


#ifdef __cplusplus
}
#endif

#endif // MON_ENTER_EXIT_HEADER
