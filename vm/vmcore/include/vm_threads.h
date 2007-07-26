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
#include "open/hythread.h"
#include "open/hythread_ext.h"
#include "open/ti_thread.h"
#include "open/jthread.h"
#include "open/vm_gc.h"

#include "jvmti.h"
#include "jni_direct.h"
#include "thread_manager.h"
#include "vm_core_types.h"
#include "object_layout.h"

#define tmn_suspend_disable assert(hythread_is_suspend_enabled());hythread_suspend_disable
#define tmn_suspend_enable assert(!hythread_is_suspend_enabled());hythread_suspend_enable
#define tmn_suspend_disable_recursive hythread_suspend_disable
#define tmn_suspend_enable_recursive hythread_suspend_enable

typedef VM_thread *vm_thread_accessor();
VMEXPORT extern vm_thread_accessor *get_thread_ptr;

inline void vm_set_exception_registers(VM_thread * thread, Registers & regs)
{
    if (!thread->regs) {
        thread->regs = malloc(sizeof(Registers));
        assert(thread->regs);
    }
    *(Registers *) thread->regs = regs;
}

inline void *vm_get_ip_from_regs(VM_thread * thread)
{
    return ((Registers *) thread->regs)->get_ip();
}

inline void vm_reset_ip_from_regs(VM_thread * thread)
{
    ((Registers *) thread->regs)->reset_ip();
}

inline jvmti_thread_t jthread_get_jvmti_thread(hythread_t native_thread)
{
    assert(native_thread);
    vm_thread_t vm_thread =
        (vm_thread_t)hythread_tls_get(native_thread, TM_THREAD_VM_TLS_KEY);
    if (!vm_thread) {
        return NULL;
    }
    return &(vm_thread->jvmti_thread);
}

inline jvmti_thread_t jthread_self_jvmti()
{
    return jthread_get_jvmti_thread(hythread_self());
}

inline void vm_set_jvmti_saved_exception_registers(VM_thread * thread,
                                                   Registers & regs)
{
    assert(thread);
    assert(&thread->jvmti_thread);
    jvmti_thread_t jvmti_thread = &thread->jvmti_thread;
    if (!jvmti_thread->jvmti_saved_exception_registers) {
        jvmti_thread->jvmti_saved_exception_registers = malloc(sizeof(Registers));
        assert(jvmti_thread->jvmti_saved_exception_registers);
    }
    *(Registers *) jvmti_thread->jvmti_saved_exception_registers = regs;
}

inline VM_thread *get_vm_thread_fast_self()
{
    register hythread_t thr = hythread_self();
    return thr ? ((VM_thread *) hythread_tls_get(thr, TM_THREAD_VM_TLS_KEY)) :
        NULL;
}

inline VM_thread *get_vm_thread(hythread_t thr)
{
    if (thr == NULL) {
        return NULL;
    }
    return (VM_thread *) hythread_tls_get(thr, TM_THREAD_VM_TLS_KEY);
}

VMEXPORT void init_TLS_data();

VMEXPORT void set_TLS_data(VM_thread * thread);
uint16 get_self_stack_key();

#define p_TLS_vmthread (get_vm_thread_fast_self())

//Registers *thread_gc_get_context(VM_thread *, VmRegisterContext &);
void thread_gc_set_context(VM_thread *);

/**
 *  Auxiliary function to update thread count
 */
void jthread_start_count();
void jthread_end_count();

vm_thread_t jthread_allocate_vm_thread(hythread_t native_thread);
jint jthread_allocate_vm_thread_pool(JavaVM * java_vm, vm_thread_t vm_thread);
void jthread_deallocate_vm_thread_pool(vm_thread_t vm_thread);

#endif //!_VM_THREADS_H_
