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

typedef vm_thread_t (*vm_thread_accessor)();
VMEXPORT extern vm_thread_accessor get_thread_ptr;

#define p_TLS_vmthread (jthread_self_vm_thread())

/**
 * Gets jvmti_thread pointer from native thread
 */
inline jvmti_thread_t jthread_self_jvmti()
{
    register vm_thread_t vm_thread = jthread_self_vm_thread();
    return vm_thread ? &(vm_thread->jvmti_thread) : NULL;
} // jthread_self_jvmti

/**
 * Gets jvmti_thread pointer from a given native thread
 */
inline jvmti_thread_t jthread_get_jvmti_thread(hythread_t native)
{
    assert(native);
    register vm_thread_t vm_thread = jthread_get_vm_thread(native);
    return vm_thread ? &(vm_thread->jvmti_thread) : NULL;
} // jthread_get_jvmti_thread

/**
 *  Auxiliary function to update thread count
 */
void jthread_start_count();
void jthread_end_count();

jint jthread_allocate_vm_thread_pool(JavaVM * java_vm, vm_thread_t vm_thread);
void jthread_deallocate_vm_thread_pool(vm_thread_t vm_thread);
vm_thread_t jthread_allocate_thread();
void vm_set_jvmti_saved_exception_registers(vm_thread_t vm_thread, Registers & regs);
void vm_set_exception_registers(vm_thread_t vm_thread, Registers & regs);
void *vm_get_ip_from_regs(vm_thread_t vm_thread);
void vm_reset_ip_from_regs(vm_thread_t vm_thread);


#endif //!_VM_THREADS_H_
