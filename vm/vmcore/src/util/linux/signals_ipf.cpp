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

#define LOG_DOMAIN "signals"
#include "cxxlog.h"
#include "open/platform_types.h"
#include "Class.h"
#include "interpreter.h"
#include "environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
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
    err = pthread_getattr_np(thread, &pthread_attr);
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


void init_stack_info() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();
    vm_thread->stack_addr = find_stack_addr();
    vm_thread->stack_size = hythread_get_thread_stacksize(hythread_self());
    common_guard_stack_size = find_guard_stack_size();
    common_guard_page_size = find_guard_page_size();

    /* FIXME: doesn't work, BTW, move this code to common file for all linuxes
     *        to avoid code duplication
     *
     * set_guard_stack();
     */
}

bool set_guard_stack() {
    int err;
    
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    assert(((size_t)(&stack_addr)) > ((size_t)((char*)stack_addr - stack_size
        + guard_stack_size + 2 * guard_page_size)));

    // map the guard page and protect it
    void UNUSED *res = mmap(stack_addr - stack_size + guard_page_size +
    guard_stack_size, guard_page_size,  PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    assert(res!=MAP_FAILED);

    err = mprotect(stack_addr - stack_size  + guard_page_size +  
    guard_stack_size, guard_page_size, PROT_NONE );
   
    assert(!err);

    //map the alternate stack on which we want to handle the signal
    void UNUSED *res2 = mmap(stack_addr - stack_size + guard_page_size,
    guard_stack_size,  PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    assert(res2!=MAP_FAILED);


    stack_t sigalt;
    sigalt.ss_sp = stack_addr - stack_size + guard_page_size;
    sigalt.ss_flags = SS_ONSTACK;
    sigalt.ss_size = guard_stack_size;

    err = sigaltstack (&sigalt, NULL);
    assert(!err);

    return true; //FIXME HARMONY-5157
}

size_t get_available_stack_size() {
    char* stack_addr = (char*) get_stack_addr();
    size_t used_stack_size = stack_addr - ((char*)&stack_addr);
    int available_stack_size;

    if (((char*)&stack_addr) > (stack_addr - get_stack_size() + get_guard_page_size() + get_guard_stack_size())) {
        available_stack_size = get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    } else {
        available_stack_size = get_stack_size() - used_stack_size - get_guard_page_size();
    }

    if (available_stack_size > 0) {
        return (size_t) available_stack_size;
    } else {
        return 0;
    }
}

bool check_available_stack_size(size_t required_size) {
    size_t available_stack_size = get_available_stack_size();

    if (available_stack_size < required_size) {
        if (available_stack_size < get_guard_stack_size()) {
            remove_guard_stack(p_TLS_vmthread);
        }
        exn_raise_by_name("java/lang/StackOverflowError");
        return false;
    } else {
        return true;
    }
}

size_t get_restore_stack_size() {
    return 0x0200;
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

}

static bool check_stack_overflow(Registers* regs, void* fault_addr) {
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    char* guard_page_begin = stack_addr - stack_size + guard_page_size + guard_stack_size;
    char* guard_page_end = guard_page_begin + guard_page_size;

    return((guard_page_begin <= fault_addr) && (fault_addr < guard_page_end));
}


Boolean stack_overflow_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", ("SOE detected at ip=%p, sp=%p",
                            regs->get_ip(), regs->get_sp()));

    assert(0); // Not implemented
    abort();

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
        remove_guard_stack(vmthread);
        vmthread->restore_guard_page = true;
        exn_raise_by_class(exn_class);
    }

    if (new_ip && regs->get_ip() == new_ip)
        regs->set_ip(saved_ip);

    return TRUE;
}

Boolean null_reference_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", "NPE detected at " << regs->get_ip());

    assert(0); // Not implemented
    abort();

    vm_thread_t vmthread = get_thread_ptr();
    Global_Env* env = VM_Global_State::loader_env;
    void* saved_ip = regs->get_ip();
    void* new_ip = NULL;

    if (is_in_ti_handler(vmthread, saved_ip))
    {
        new_ip = vm_get_ip_from_regs(vmthread);
        regs->set_ip(new_ip);
    }

    if (check_stack_overflow(regs, fault_addr))
    {
        Boolean result = stack_overflow_handler(signum, regs, fault_addr);

        if (new_ip && regs->get_ip() == new_ip)
            regs->set_ip(saved_ip);

        return result;
    }

    if (!vmthread || env == NULL ||
        !is_in_java(regs) || interpreter_enabled())
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
