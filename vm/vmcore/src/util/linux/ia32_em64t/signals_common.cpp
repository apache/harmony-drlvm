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

#include <sys/mman.h>
#include <sys/ucontext.h>
#undef __USE_XOPEN
#include <signal.h>

#include <pthread.h>
#if defined(FREEBSD)
#include <pthread_np.h>
#endif
#include <sys/time.h>

#define LOG_DOMAIN "signals"
#include "cxxlog.h"
#include "open/platform_types.h"
#include "Class.h"
#include "interpreter.h"
#include "environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "signals_common.h"
#include "signals.h"


/*
 * Information about stack
 */
inline void* find_stack_addr() {
    int err;
    void* stack_addr;
    pthread_attr_t pthread_attr;
    size_t stack_size;

    pthread_t thread = pthread_self();
    err = pthread_attr_init(&pthread_attr);
    assert(!err);
#if defined(FREEBSD)
    err = pthread_attr_get_np(thread, &pthread_attr);
#else
    err = pthread_getattr_np(thread, &pthread_attr);
#endif
    assert(!err);
    err = pthread_attr_getstack(&pthread_attr, &stack_addr, &stack_size);
    assert(!err);
    pthread_attr_destroy(&pthread_attr);

    return (void *)((unsigned char *)stack_addr + stack_size);
}

#if 0
inline size_t find_stack_size() {
    int err;
    size_t stack_size;
    pthread_attr_t pthread_attr;

    pthread_attr_init(&pthread_attr);
    err = pthread_attr_getstacksize(&pthread_attr, &stack_size);
    pthread_attr_destroy(&pthread_attr);
    return stack_size;
}
#endif

static inline size_t find_guard_stack_size() {
    return 64*1024;
}

static inline size_t find_guard_page_size() {
    int err;
    size_t guard_size;
    pthread_attr_t pthread_attr;

    pthread_attr_init(&pthread_attr);
    err = pthread_attr_getguardsize(&pthread_attr, &guard_size);
    pthread_attr_destroy(&pthread_attr);

    return guard_size;
}

static size_t common_guard_stack_size;
static size_t common_guard_page_size;

static inline void* get_stack_addr() {
    return jthread_self_vm_thread_unsafe()->stack_addr;
}

static inline size_t get_stack_size() {
    return jthread_self_vm_thread_unsafe()->stack_size;
}

static inline size_t get_guard_stack_size() {
    return common_guard_stack_size;
}

static inline size_t get_guard_page_size() {
    return common_guard_page_size;
}

#ifdef _IA32_
static void __attribute__ ((cdecl)) stack_holder(char* addr) {
    char buf[1024];

    if (addr > (buf + ((size_t)1024))) {
        return;
    }
    stack_holder(addr);
}
#endif

void init_stack_info() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();

    // find stack parametrs
    char* stack_addr = (char *)find_stack_addr();
    vm_thread->stack_addr = stack_addr;
    size_t stack_size = hythread_get_thread_stacksize(hythread_self());
    assert(stack_size > 0);

    vm_thread->stack_size = stack_size;

    common_guard_stack_size = find_guard_stack_size();
    common_guard_page_size = find_guard_page_size();

    // stack should be mapped so it's result of future mapping
    char* res;
    // begin of the stack can be protected by OS, but this part already mapped
    // found address of current stack page
    char* current_page_addr =
            (char*)(((size_t)&res) & (~(common_guard_page_size-1)));

    // leave place for mmap work
    char* mapping_page_addr = current_page_addr - common_guard_page_size;

#ifdef _IA32_
    // makes sure that stack allocated till mapping_page_addr
    stack_holder(mapping_page_addr);
#endif

    // found size of the stack area which should be maped
    size_t stack_mapping_size = (size_t)mapping_page_addr
            - (size_t)stack_addr + stack_size;

    // maps unmapped part of the stack
    res = (char*) mmap(stack_addr - stack_size, stack_mapping_size,
            PROT_READ | PROT_WRITE, STACK_MMAP_ATTRS, -1, 0);

    // stack should be mapped, checks result
    assert(res == (stack_addr - stack_size));

    // set guard page
    set_guard_stack();
}

bool set_guard_stack() {
    int err;
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    if (((size_t)(&stack_addr) - get_mem_protect_stack_size()) 
            < ((size_t)((char*)stack_addr - stack_size 
            + guard_stack_size + 2 * guard_page_size))) {
        return false;
    }

    err = mprotect(stack_addr - stack_size  + guard_page_size +
            guard_stack_size, guard_page_size, PROT_NONE );

    assert(!err);

    // sets alternative, guard stack
    stack_t sigalt;
    sigalt.ss_sp = stack_addr - stack_size + guard_page_size;
#if defined(FREEBSD)
    sigalt.ss_flags = 0;
#else
    sigalt.ss_flags = SS_ONSTACK;
#endif
    sigalt.ss_size = guard_stack_size;
    err = sigaltstack (&sigalt, NULL);

    assert(!err);

    // notify that stack is OK and there are no needs to restore it
    jthread_self_vm_thread_unsafe()->restore_guard_page = false;

    return true;
}

size_t get_available_stack_size() {
    char* stack_addr = (char*) get_stack_addr();
    size_t used_stack_size = stack_addr - ((char*)&stack_addr);
    size_t available_stack_size;

    if (!(p_TLS_vmthread->restore_guard_page)) {
        available_stack_size = get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    } else {
        available_stack_size = get_stack_size() - used_stack_size - get_guard_page_size();
    }

    if (available_stack_size > 0) {
        return (size_t) available_stack_size;
    }

    return 0;
}

bool check_available_stack_size(size_t required_size) {
    size_t available_stack_size = get_available_stack_size();

    if (available_stack_size < required_size) {
        if (available_stack_size < get_guard_stack_size()) {
            remove_guard_stack(p_TLS_vmthread);
        }
        Global_Env *env = VM_Global_State::loader_env;
        exn_raise_by_class(env->java_lang_StackOverflowError_Class);
        return false;
    } else {
        return true;
    }
}

bool check_stack_size_enough_for_exception_catch(void* sp) {
    char* stack_adrr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_adrr) - ((size_t)sp);
    size_t available_stack_size =
            get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    return get_restore_stack_size() < available_stack_size;
}

void remove_guard_stack(vm_thread_t vm_thread) {
    int err;
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();


    err = mprotect(stack_addr - stack_size + guard_page_size +
    guard_stack_size, guard_page_size, PROT_READ | PROT_WRITE);


    stack_t sigalt;
    sigalt.ss_sp = stack_addr - stack_size + guard_page_size;
    sigalt.ss_flags = SS_DISABLE;
    sigalt.ss_size = guard_stack_size;

    err = sigaltstack (&sigalt, NULL);

    vm_thread->restore_guard_page = true;
}

static bool check_stack_overflow(Registers* regs, void* fault_addr) {
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    char* guard_page_begin = stack_addr - stack_size + guard_page_size + guard_stack_size;
    char* guard_page_end = guard_page_begin + guard_page_size;

    // FIXME: Workaround for main thread
    guard_page_end += guard_page_size;

    return((guard_page_begin <= fault_addr) && (fault_addr < guard_page_end));
}


Boolean stack_overflow_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", ("SOE detected at ip=%p, sp=%p",
                            regs->get_ip(), regs->get_sp()));

    vm_thread_t vmthread = get_thread_ptr();
    Global_Env* env = VM_Global_State::loader_env;
    void* saved_ip = regs->get_ip();
    void* new_ip = NULL;

    if (is_in_ti_handler(vmthread, saved_ip))
    {
        new_ip = vm_get_ip_from_regs(vmthread);
        regs->set_ip(new_ip);
    }

    if (!vmthread || env == NULL)
        return FALSE; // Crash

    remove_guard_stack(vmthread);
    vmthread->restore_guard_page = true;

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    ncai_process_signal_event((NativeCodePtr)regs->get_ip(),
                                (jint)signum, false, &is_handled);
    if (is_handled)
    {
        if (new_ip)
            regs->set_ip(saved_ip);
        return TRUE;
    }

    Class* exn_class = env->java_lang_StackOverflowError_Class;

    if (is_in_java(regs))
    {
        signal_throw_java_exception(regs, exn_class);
    }
    else if (is_unwindable())
    {
        if (hythread_is_suspend_enabled())
            hythread_suspend_disable();
        signal_throw_exception(regs, exn_class);
    } else {
        exn_raise_by_class(exn_class);
    }

    if (new_ip && regs->get_ip() == new_ip)
        regs->set_ip(saved_ip);

    return TRUE;
}

Boolean null_reference_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", "NPE detected at " << regs->get_ip());

    vm_thread_t vmthread = get_thread_ptr();
    Global_Env* env = VM_Global_State::loader_env;
    void* saved_ip = regs->get_ip();
    void* new_ip = NULL;

    if (is_in_ti_handler(vmthread, saved_ip))
    {
        new_ip = vm_get_ip_from_regs(vmthread);
        regs->set_ip(new_ip);
    }

    if (!vmthread || env == NULL)
        return FALSE; // Crash

    // Stack overflow can occur in native code as well as in interpreter
    if (check_stack_overflow(regs, fault_addr))
    {
        Boolean result = stack_overflow_handler(signum, regs, fault_addr);

        if (new_ip && regs->get_ip() == new_ip)
            regs->set_ip(saved_ip);

        return result;
    }

    if (!is_in_java(regs) || interpreter_enabled())
        return FALSE; // Crash

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    ncai_process_signal_event((NativeCodePtr)regs->get_ip(),
                                (jint)signum, false, &is_handled);
    if (is_handled)
    {
        if (new_ip)
            regs->set_ip(saved_ip);
        return TRUE;
    }

    signal_throw_java_exception(regs, env->java_lang_NullPointerException_Class);

    if (new_ip && regs->get_ip() == new_ip)
        regs->set_ip(saved_ip);

    return TRUE;
}
