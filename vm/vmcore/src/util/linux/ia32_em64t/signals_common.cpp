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

#define LOG_DOMAIN "signals"
#include "cxxlog.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/ucontext.h>
#include <sys/wait.h>


#undef __USE_XOPEN
#include <signal.h>

#include <pthread.h>
#if defined(FREEBSD)
#include <pthread_np.h>
#endif
#include <sys/time.h>
#include "method_lookup.h"

#include "Class.h"
#include "environment.h"

#include "open/gc.h"

#include "init.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "vm_threads.h"
#include "open/vm_util.h"
#include "compile.h"
#include "vm_stats.h"
#include "sync_bits.h"

#include "object_generic.h"
#include "thread_manager.h"

#include "exception_filter.h"
#include "interpreter.h"
#include "crash_handler.h"
#include "stack_dump.h"
#include "jvmti_break_intf.h"

#include "signals_common.h"


static void general_crash_handler(int signum, Registers* regs);


extern "C" {
static void DECL_CHANDLER c_exception_handler(Class* exn_class, bool java_code) {
    // this exception handler is executed *after* NT exception handler returned
    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    // Create local copy for registers because registers in TLS can be changed
    Registers regs = {0};
    VM_thread *thread = p_TLS_vmthread;
    assert(thread);
    assert(exn_class);

    if (thread->regs) {
        regs = *thread->regs;
    }

    exn_athrow_regs(&regs, exn_class, java_code, true);
}
}

static void throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    Registers regs;
    ucontext_to_regs(&regs, uc);

    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    bool java_code = (vm_identify_eip((void *)regs.get_ip()) == VM_TYPE_JAVA);
    VM_thread* vmthread = p_TLS_vmthread;
    NativeCodePtr callback = (NativeCodePtr) c_exception_handler;

    vm_set_exception_registers(vmthread, regs);
    add_red_zone(&regs);
    regs_push_param(&regs, java_code, 1/*2nd arg */);
    assert(exc_clss);
    regs_push_param(&regs, (POINTER_SIZE_INT)exc_clss, 0/* 1st arg */);
    // To get proper stack alignment on x86_64
    regs_align_stack(&regs);
    // imitate return IP on stack
    regs_push_return_address(&regs, NULL);

    // set up the real exception handler address
    regs.set_ip(callback);
    regs_to_ucontext(uc, &regs);
}

static bool java_throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    ASSERT_NO_INTERPRETER;
    void* ip = (void*)UC_IP(uc);
    VM_Code_Type vmct = vm_identify_eip(ip);
    if(vmct != VM_TYPE_JAVA) {
        return false;
    }

    throw_from_sigcontext(uc, exc_clss);
    return true;
}

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

inline size_t find_guard_stack_size() {
    return 64*1024;
}

inline size_t find_guard_page_size() {
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

inline void* get_stack_addr() {
    return jthread_self_vm_thread_unsafe()->stack_addr;
}

inline size_t get_stack_size() {
    return jthread_self_vm_thread_unsafe()->stack_size;
}

inline size_t get_guard_stack_size() {
    return common_guard_stack_size;
}

inline size_t get_guard_page_size() {
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

static bool check_stack_overflow(siginfo_t *info, ucontext_t *uc) {
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    char* guard_page_begin = stack_addr - stack_size + guard_page_size + guard_stack_size;
    char* guard_page_end = guard_page_begin + guard_page_size;

    // FIXME: Workaround for main thread
    guard_page_end += guard_page_size;

    char* fault_addr = (char*)(info->si_addr);
    //char* esp_value = (char*)(uc->uc_mcontext.gregs[REG_ESP]);

    return((guard_page_begin <= fault_addr) && (fault_addr < guard_page_end));
}



static void stack_overflow_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;

    vm_thread_t vm_thread = p_TLS_vmthread;
    remove_guard_stack(vm_thread);
    vm_thread->restore_guard_page = true;

    if (java_throw_from_sigcontext(
                uc, env->java_lang_StackOverflowError_Class)) {
        return;
    } else {
        if (is_unwindable()) {
            if (hythread_is_suspend_enabled()) {
                tmn_suspend_disable();
            }
            throw_from_sigcontext(
                uc, env->java_lang_StackOverflowError_Class);
        } else {
            exn_raise_by_class(env->java_lang_StackOverflowError_Class);
        }
    }
}


static void null_java_reference_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    VM_thread *vm_thread = p_TLS_vmthread;
    Registers regs;
    ucontext_to_regs(&regs, uc);

    if (vm_thread && vm_thread->jvmti_thread.violation_flag)
    {
        vm_thread->jvmti_thread.violation_flag = 0;
        regs.set_ip(vm_thread->jvmti_thread.violation_restart_address);
        regs_to_ucontext(uc, &regs);
        return;
    }

    Global_Env *env = VM_Global_State::loader_env;

    TRACE2("signals", "NPE or SOE detected at " << (void*)UC_IP(uc));

    if (check_stack_overflow(info, uc)) {
        stack_overflow_handler(signum, info, context);
        return;
    }

    if (!interpreter_enabled()) {
        if (java_throw_from_sigcontext(
                    uc, env->java_lang_NullPointerException_Class)) {
            return;
        }
    }

    general_crash_handler(signum, &regs);
}


static void null_java_divide_by_zero_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;

    TRACE2("signals",
           "ArithmeticException detected at " << (void*)UC_IP(uc));

    if (!interpreter_enabled()) {
        if (java_throw_from_sigcontext(
                    uc, env->java_lang_ArithmeticException_Class)) {
            return;
        }
    }

    Registers regs;
    ucontext_to_regs(&regs, uc);
    general_crash_handler(signum, &regs);
}

static void jvmti_jit_breakpoint_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t*)context;
    Registers regs;

    ucontext_to_regs(&regs, uc);
    TRACE2("signals", "JVMTI breakpoint detected at " << (void*)regs.get_ip());

    if (!interpreter_enabled())
    {
        bool handled = jvmti_jit_breakpoint_handler(&regs);
        if (handled)
        {
            regs_to_ucontext(uc, &regs);
            return;
        }
    }

    general_crash_handler(signum, &regs);
}

/**
 * Print out the call stack of the aborted thread.
 * @note call stacks may be used for debugging
 */
static void abort_handler (int signum, siginfo_t* UNREF info, void* context)
{
    Registers regs;
    ucontext_to_regs(&regs, (ucontext_t *)context);
    general_crash_handler(signum, &regs);
}

static void process_crash(Registers* regs)
{
    // print stack trace
    sd_print_stack(regs);
}

struct sig_name_t
{
    int    num;
    char*  name;
};

static sig_name_t sig_names[] =
{
    {SIGTRAP, "SIGTRAP"},
    {SIGSEGV, "SIGSEGV"},
    {SIGFPE,  "SIGFPE" },
    {SIGABRT, "SIGABRT"},
    {SIGINT,  "SIGINT" },
    {SIGQUIT, "SIGQUIT"}
};

static const char* get_sig_name(int signum)
{
    for (int i = 0; i < sizeof(sig_names)/sizeof(sig_names[0]); i++)
    {
        if (signum == sig_names[i].num)
            return sig_names[i].name;
    }

    return "unregistered";
}

static void general_crash_handler(int signum, Registers* regs)
{
    // setup default handler
    signal(signum, SIG_DFL);
    // Print message
    fprintf(stderr, "Signal %d is reported: %s\n", signum, get_sig_name(signum));

    if (!is_gdb_crash_handler_enabled() ||
        !gdb_crash_handler(regs))
    {
        process_crash(regs);
    }
}

static void general_signal_handler(int signum, siginfo_t* info, void* context)
{
    bool replaced = false;
    ucontext_t* uc = (ucontext_t *)context;
    VM_thread* vm_thread = p_TLS_vmthread;
    bool violation =
        (signum == SIGSEGV) && vm_thread && vm_thread->jvmti_thread.violation_flag;

    Registers regs;
    ucontext_to_regs(&regs, uc);
    void* saved_ip = regs.get_ip();
    void* new_ip = saved_ip;
    bool in_java = false;

    if (vm_thread)
    {
        // If exception is occured in processor instruction previously
        // instrumented by breakpoint, the actual exception address will reside
        // in jvmti_jit_breakpoints_handling_buffer
        // We should replace exception address with saved address of instruction
        POINTER_SIZE_INT break_buf =
            (POINTER_SIZE_INT)vm_thread->jvmti_thread.jvmti_jit_breakpoints_handling_buffer;
        if ((POINTER_SIZE_INT)saved_ip >= break_buf &&
            (POINTER_SIZE_INT)saved_ip < break_buf + TM_JVMTI_MAX_BUFFER_SIZE)
        {
            // Breakpoints should not occur in breakpoint buffer
            assert(signum != SIGTRAP);

            replaced = true;
            new_ip = vm_get_ip_from_regs(vm_thread);
            regs.set_ip(new_ip);
            regs_to_ucontext(uc, &regs);
        }

        in_java = (vm_identify_eip(regs.get_ip()) == VM_TYPE_JAVA);
    }

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    bool is_internal = (signum == SIGTRAP) || violation;
    ncai_process_signal_event((NativeCodePtr)regs.get_ip(),
                                (jint)signum, is_internal, &is_handled);
    if (is_handled)
        return;

    // delegate evident cases to crash handler
    if ((!vm_thread ||
        (!in_java && signum != SIGSEGV)) &&
        signum != SIGTRAP && signum != SIGINT && signum != SIGQUIT)
    {
        general_crash_handler(signum, &regs);
        regs.set_ip(saved_ip);
        regs_to_ucontext(uc, &regs);
        return;
    }

    switch (signum)
    {
    case SIGTRAP:
        jvmti_jit_breakpoint_handler(signum, info, context);
        break;
    case SIGSEGV:
        null_java_reference_handler(signum, info, context);
        break;
    case SIGFPE:
        null_java_divide_by_zero_handler(signum, info, context);
        break;
    case SIGABRT:
        abort_handler(signum, info, context);
        break;
    case SIGINT:
        vm_interrupt_handler(signum);
        break;
    case SIGQUIT:
        vm_dump_handler(signum);
        break;
    default:
        // Unknown signal
        assert(0);
        break;
    }

    ucontext_to_regs(&regs, uc);

    // If IP was not changed in specific handler to start another handler,
    // we should restore original IP, if it's nesessary
    if (replaced && regs.get_ip() == new_ip)
    {
        regs.set_ip((void*)saved_ip);
        regs_to_ucontext(uc, &regs);
    }
}

void initialize_signals()
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &general_signal_handler;
    sigaction(SIGTRAP, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;;
    sa.sa_sigaction = &general_signal_handler;
    sigaction(SIGSEGV, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &general_signal_handler;
    sigaction(SIGFPE, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &general_signal_handler;
    sigaction(SIGINT, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &general_signal_handler;
    sigaction(SIGQUIT, &sa, NULL);

    /* install abort_handler to print out call stack on assertion failures */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &general_signal_handler;
    sigaction( SIGABRT, &sa, NULL);
    /* abort_handler installed */

    // Prepare gdb crash handler
    init_gdb_crash_handler();

    // Prepare general crash handler
    sd_init_crash_handler();

} //initialize_signals

void shutdown_signals() {
    //FIXME: should be defined in future
} //shutdown_signals
