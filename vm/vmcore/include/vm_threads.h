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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.2.4.4 $
 */  


#ifndef _VM_THREADS_H_
#define _VM_THREADS_H_


#ifdef PLATFORM_POSIX
#include <semaphore.h>
#include "platform_lowlevel.h"
#else
#include "vm_process.h"
#endif

#include <apr_pools.h>

#include "open/types.h"
//#include "open/hythread.h"
#include <open/hythread_ext.h>
#include "open/ti_thread.h"

#include "vm_core_types.h"
#include "object_layout.h"
#include "open/vm_gc.h"
#include "exceptions_type.h"
#include "jvmti.h"
#include "jni_direct.h"


// 
#define tmn_suspend_disable assert(hythread_is_suspend_enabled());hythread_suspend_disable
#define tmn_suspend_enable assert(!hythread_is_suspend_enabled());hythread_suspend_enable
#define tmn_suspend_disable_recursive hythread_suspend_disable
#define tmn_suspend_enable_recursive hythread_suspend_enable

#define GC_BYTES_IN_THREAD_LOCAL (20 * sizeof(void *))

// These are thread level gc states. 
enum gc_state {
    zero = 0,
    gc_moving_to_safepoint,
    gc_at_safepoint,
    gc_enumeration_done
};


struct jvmti_frame_pop_listener;
struct JVMTISingleStepState;

class VmRegisterContext;

class VM_thread {

public:
    /**
     * Memory pool where this structure is allocated.
     * This pool should be used by current thread for memory allocations.
     */
    apr_pool_t * pool;

    /**
     * JNI environment associated with this thread.
     */
    JNIEnv_Internal * jni_env;

    /**
     * Class loader which loads native library and calls to its JNI_OnLoad
     */
    ClassLoader* onload_caller;

    /**
    * Flag to detect if a class is not found on bootclasspath,
    * as opposed to linkage errors.
    * Used for implementing default delegation model.
    */
    bool class_not_found;

    // In case exception is thrown, Exception object is put here
    // TODO: Needs to be replaced with jobject!
    //volatile ManagedObject*           p_exception_object;
    volatile Exception thread_exception;

    // For support of JVMTI events: EXCEPTION, EXCEPTION_CATCH
    // If p_exception_object is set and p_exception_object_ti is not
    //    - EXCEPTION event should be generated
    // If p_exception_object_ti is set and p_exception_object is not
    //    - EXCEPTION_CATCH even should be generated
    volatile ManagedObject*           p_exception_object_ti;

    // flag which indicate that guard page on the stak should be restored
    bool restore_guard_page;

    // thread stack address
    void* stack_addr;

    // Should JVMTI code be notified about exception in p_exception_object
    bool                              ti_exception_callback_pending;

    // ??
    gc_state                          gc_status; 
    int                               finalize_thread_flags;
    // Flag indicating whether thread stopped. Duplicate of thread_state?
    bool                              is_stoped;

    JVMTILocalStorage                 jvmti_local_storage;
    // Buffer used to create instructions instead of original instruction
    // to transfer execution control back to the code after breakpoint
    // has been processed
    jbyte                             jvmti_jit_breakpoints_handling_buffer[50];
    jvmti_frame_pop_listener          *frame_pop_listener;
    JVMTISingleStepState              *ss_state;
    Registers                         jvmti_saved_exception_registers;

    // CPU registers.
    Registers                         regs;

    // This field is private the to M2nFrame module, init code should set it to NULL
    // Informational frame - created when native is called from Java,
    // used to store local handles (jobjects) + registers.
    // =0 if there is no m2n frame.
    M2nFrame*                         last_m2n_frame;

    // GC Information
    Byte                              _gc_private_information[GC_BYTES_IN_THREAD_LOCAL];
    NativeObjectHandles*              native_handles;
    GcFrame*                          gc_frames;
    //gc_enable_disable_state           gc_enabled_status;
    bool                              gc_wait_for_enumeration;
    bool                              restore_context_after_gc_and_resume;

#if defined(PLATFORM_POSIX) && defined(_IPF_)
    // Linux/IPF
    sem_t                             suspend_sem;     // To suspend thread in signal handler
    sem_t                             suspend_self;    // To suspend current thread for signal handler
    uint64                            suspended_state; // Flag to indicate how the one thread is suspended
                                                       // Possible values:
                               // NOT_SUSPENDED, 
                               // SUSPENDED_IN_SIGNAL_HANDLER,
                               // SUSPENDED_IN_DISABLE_GC_FOR_THREAD
    uint64                            t[2]; // t[0] <= rnat, t[1] <= bspstore for current thread context
                                            // t[0] <= rnat, t[1] <= bsp      for other   thread context
#endif

    void *lastFrame;
    void *firstFrame;
    int interpreter_state;
    void** stack_end;       /// The upper boundary of the stack to scan when verifying stack enumeration

    void* get_ip_from_regs() { return regs.get_ip(); }
    void reset_regs_ip() { return regs.reset_ip(); }


};

typedef  VM_thread *vm_thread_accessor();
VMEXPORT extern vm_thread_accessor *get_thread_ptr;

//VMEXPORT VM_thread *get_vm_thread(hythread_t thr);
//VMEXPORT VM_thread *get_vm_thread_self();

inline VM_thread *get_vm_thread_fast_self() {
	register hythread_t thr = hythread_self();

    return thr ? ((VM_thread *)hythread_tls_get(thr, TM_THREAD_VM_TLS_KEY)) : NULL;
}

inline VM_thread *get_vm_thread(hythread_t thr) {
    if (thr == NULL) {
        return NULL;
    }
    return (VM_thread *)hythread_tls_get(thr, TM_THREAD_VM_TLS_KEY);
}

VMEXPORT void init_TLS_data();

VMEXPORT void set_TLS_data(VM_thread *thread) ;
uint16 get_self_stack_key();

#define p_TLS_vmthread (get_vm_thread_fast_self())

Registers *thread_gc_get_context(VM_thread *, VmRegisterContext &);
void thread_gc_set_context(VM_thread *);

struct Global_Env;

#endif //!_VM_THREADS_H_
