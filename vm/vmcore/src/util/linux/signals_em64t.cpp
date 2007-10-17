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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
// We use signal handlers to detect null pointer and divide by zero
// exceptions.
// There must be an easier way of locating the context in the signal
// handler than what we do here.

#define LOG_DOMAIN "port.old"
#include "cxxlog.h"

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
#include <sys/mman.h>

#undef __USE_XOPEN
#include <signal.h>

#include <pthread.h>
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

void linux_ucontext_to_regs(Registers* regs, ucontext_t *uc)
{
    regs->rax = uc->uc_mcontext.gregs[REG_RAX];
    regs->rcx = uc->uc_mcontext.gregs[REG_RCX];
    regs->rdx = uc->uc_mcontext.gregs[REG_RDX];
    regs->rdi = uc->uc_mcontext.gregs[REG_RDI];
    regs->rsi = uc->uc_mcontext.gregs[REG_RSI];
    regs->rbx = uc->uc_mcontext.gregs[REG_RBX];
    regs->rbp = uc->uc_mcontext.gregs[REG_RBP];
    regs->rip = uc->uc_mcontext.gregs[REG_RIP];
    regs->rsp = uc->uc_mcontext.gregs[REG_RSP];
    regs->r8 = uc->uc_mcontext.gregs[REG_R8];
    regs->r9 = uc->uc_mcontext.gregs[REG_R9];
    regs->r10 = uc->uc_mcontext.gregs[REG_R10];
    regs->r11 = uc->uc_mcontext.gregs[REG_R11];
    regs->r12 = uc->uc_mcontext.gregs[REG_R12];
    regs->r13 = uc->uc_mcontext.gregs[REG_R13];
    regs->r14 = uc->uc_mcontext.gregs[REG_R14];
    regs->r15 = uc->uc_mcontext.gregs[REG_R15];
    regs->eflags = uc->uc_mcontext.gregs[REG_EFL];
}

void linux_regs_to_ucontext(ucontext_t *uc, Registers* regs)
{
    uc->uc_mcontext.gregs[REG_RAX] = regs->rax;
    uc->uc_mcontext.gregs[REG_RCX] = regs->rcx;
    uc->uc_mcontext.gregs[REG_RDX] = regs->rdx;
    uc->uc_mcontext.gregs[REG_RDI] = regs->rdi;
    uc->uc_mcontext.gregs[REG_RSI] = regs->rsi;
    uc->uc_mcontext.gregs[REG_RBX] = regs->rbx;
    uc->uc_mcontext.gregs[REG_RBP] = regs->rbp;
    uc->uc_mcontext.gregs[REG_RIP] = regs->rip;
    uc->uc_mcontext.gregs[REG_RSP] = regs->rsp;
    uc->uc_mcontext.gregs[REG_R8] = regs->r8;
    uc->uc_mcontext.gregs[REG_R9] = regs->r9;
    uc->uc_mcontext.gregs[REG_R10] = regs->r10;
    uc->uc_mcontext.gregs[REG_R11] = regs->r11;
    uc->uc_mcontext.gregs[REG_R12] = regs->r12;
    uc->uc_mcontext.gregs[REG_R13] = regs->r13;
    uc->uc_mcontext.gregs[REG_R14] = regs->r14;
    uc->uc_mcontext.gregs[REG_R15] = regs->r15;
    uc->uc_mcontext.gregs[REG_EFL] = regs->eflags;
}

// Max. 6 arguments can be set up
void regs_push_param(Registers* pregs, POINTER_SIZE_INT param, int num)
{
    switch (num)
    {
    case 0:
        pregs->rdi = param;
        return;
    case 1:
        pregs->rsi = param;
        return;
    case 2:
        pregs->rdx = param;
        return;
    case 3:
        pregs->rcx = param;
        return;
    case 4:
        pregs->r8 = param;
        return;
    case 5:
        pregs->r9 = param;
        return;
    }
}

void regs_push_return_address(Registers* pregs, void* ret_addr)
{
    pregs->rsp = pregs->rsp - 8;
    *((void**)pregs->rsp) = ret_addr;
}

extern "C" {
void __attribute__ ((used, cdecl)) c_exception_handler(Class* exn_class, bool java_code) {
    // this exception handler is executed *after* NT exception handler returned
    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    // Create local copy for registers because registers in TLS can be changed
    Registers regs = {0};
    VM_thread *thread = p_TLS_vmthread;
    assert(thread);
    assert(exn_class);

    if (thread->regs) {
        regs = *(Registers*)thread->regs;
    }

    exn_athrow_regs(&regs, exn_class, java_code, true);
}
}

static void throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    Registers regs;
    linux_ucontext_to_regs(&regs, uc);

    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    bool java_code = (vm_identify_eip((void *)regs.rip) == VM_TYPE_JAVA);
    VM_thread* vmthread = p_TLS_vmthread;
    NativeCodePtr callback = (NativeCodePtr) c_exception_handler;
    const static uint64 red_zone_size = 0x80;

    vm_set_exception_registers(vmthread, regs);
    regs.rsp -= red_zone_size;
    regs_push_param(&regs, java_code, 1);
    assert(exc_clss);
    regs_push_param(&regs, (POINTER_SIZE_INT)exc_clss, 0);
    // imitate return IP on stack
    regs_push_return_address(&regs, NULL);
    regs_push_return_address(&regs, NULL);

    // set up the real exception handler address
    regs.set_ip(callback);
    linux_regs_to_ucontext(uc, &regs);
}

static bool java_throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    ASSERT_NO_INTERPRETER;
    unsigned *rip = (unsigned *) uc->uc_mcontext.gregs[REG_RIP];
    VM_Code_Type vmct = vm_identify_eip((void *)rip);
    if(vmct != VM_TYPE_JAVA) {
        return false;
    }

    throw_from_sigcontext(uc, exc_clss);
    return true;
}

/**
 * the saved copy of the executable name.
 */
static char executable[1024];

/**
 * invokes addr2line to decode stack.
 */
void addr2line (char *buf) {

    if ('\0' == executable[0]) {
        // no executable name is available, degrade gracefully
        LWARN(41, "Execution stack follows, consider using addr2line\n{0}" << buf);
        return;
    }

    //
    // NOTE: this function is called from signal handler,
    //       so it should use only limited list 
    //       of async signal-safe system calls
    //
    // Currently used list:
    //          pipe, fork, close, dup2, execle, write, wait
    //
    int pipes[2];               
    pipe(pipes);                // create pipe
    if (0 == fork()) { // child

        close(pipes[1]);        // close unneeded write pipe
        close(0);               // close stdin
        dup2(pipes[0],0);       // replicate read pipe as stdin

        close(1);               // close stdout
        dup2(2,1);              // replicate stderr as stdout

        char *env[] = {NULL};

        execle("/usr/bin/addr2line", "addr2line", "-e", executable, "-C", "-s", "-f", NULL, env);

    } else { // parent
        close(pipes[0]);        // close unneeded read pipe

        write(pipes[1],buf,strlen(buf));
        close(pipes[1]);        // close write pipe

        int status;
        wait(&status); // wait for the child to complete
    }
}

/*
 * Information about stack
 */
inline void* find_stack_addr() {
    int err;
    void* stack_addr;
    size_t stack_size;
    pthread_attr_t pthread_attr;

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

void set_guard_stack();

void init_stack_info() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();
    char* stack_addr = (char *)find_stack_addr();
    unsigned int stack_size = hythread_get_thread_stacksize(hythread_self());
    vm_thread->stack_addr = stack_addr;
    vm_thread->stack_size = stack_size;
    common_guard_stack_size = find_guard_stack_size();
    common_guard_page_size =find_guard_page_size();

    // stack should be mapped so it's result of future mapping
    char* res;

    // begin of the stack can be protected by OS, but this part already mapped
    // found address of current stack page
    char* current_page_addr =
            (char*)(((size_t)&res) & (~(common_guard_page_size-1)));

    // leave place for mmap work
    char* mapping_page_addr = current_page_addr - common_guard_page_size;

    // makes sure that stack allocated till mapping_page_addr
    //stack_holder(mapping_page_addr);

    // found size of the stack area which should be maped
    size_t stack_mapping_size = (size_t)mapping_page_addr
            - (size_t)stack_addr + stack_size;

    // maps unmapped part of the stack
    res = (char*) mmap(stack_addr - stack_size,
            stack_mapping_size,
            PROT_READ | PROT_WRITE,
            MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN,
            -1,
            0);
    // stack should be mapped, checks result
    assert(res == (stack_addr - stack_size));

    // set guard page
    set_guard_stack();
}

void set_guard_stack() {
    int err;
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    assert(((size_t)(&stack_addr)) > ((size_t)((char*)stack_addr - stack_size
        + guard_stack_size + 2 * guard_page_size)));

    err = mprotect(stack_addr - stack_size + guard_page_size + guard_stack_size,
        guard_page_size, PROT_NONE);

    stack_t sigalt;
    sigalt.ss_sp = stack_addr - stack_size + guard_page_size;
    sigalt.ss_flags = SS_ONSTACK;
    sigalt.ss_size = guard_stack_size;

    err = sigaltstack (&sigalt, NULL);

    jthread_self_vm_thread_unsafe()->restore_guard_page = false;
}

size_t get_available_stack_size() {
    char* stack_addr = (char*) get_stack_addr();
    size_t used_stack_size = stack_addr - ((char*)&stack_addr);
    int available_stack_size;

    if (!(p_TLS_vmthread->restore_guard_page)) {
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
        Global_Env *env = VM_Global_State::loader_env;
        exn_raise_by_class(env->java_lang_StackOverflowError_Class);
        return false;
    } else {
        return true;
    }
}

size_t get_restore_stack_size() {
    return 0x0800;
}

bool check_stack_size_enough_for_exception_catch(void* sp) {
    char* stack_adrr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_adrr) - ((size_t)sp);
    size_t available_stack_size =
            get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    return get_restore_stack_size() < available_stack_size;
}

void remove_guard_stack() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();
    assert(vm_thread);
    remove_guard_stack(vm_thread);
}

void remove_guard_stack(vm_thread_t vm_thread) {
    int err;
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    err = mprotect(stack_addr - stack_size + guard_page_size + guard_stack_size,
        guard_page_size, PROT_READ | PROT_WRITE);

    stack_t sigalt;
    sigalt.ss_sp = stack_addr - stack_size + guard_page_size;
    sigalt.ss_flags = SS_DISABLE;
    sigalt.ss_size = guard_stack_size;

    err = sigaltstack (&sigalt, NULL);

    vm_thread->restore_guard_page = true;
}

bool check_stack_overflow(siginfo_t *info, ucontext_t *uc) {
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


/*
 * We find the true signal stack frame set-up by kernel,which is located
 * by locate_sigcontext() below; then change its content according to 
 * exception handling semantics, so that when the signal handler is 
 * returned, application can continue its execution in Java exception handler.
 */
void stack_overflow_handler(int signum, siginfo_t* UNREF info, void* context)
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


void null_java_reference_handler(int signum, siginfo_t* info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;

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

    fprintf(stderr, "SIGSEGV in VM code.\n");
    Registers regs;
    linux_ucontext_to_regs(&regs, uc);

    // setup default handler
    signal(signum, SIG_DFL);

    if (!is_gdb_crash_handler_enabled() ||
        !gdb_crash_handler())
    {
        // print stack trace
        st_print_stack(&regs);
    }
}


void null_java_divide_by_zero_handler(int signum, siginfo_t* info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;

    if (!interpreter_enabled()) {
        if (java_throw_from_sigcontext(
                    uc, env->java_lang_ArithmeticException_Class)) {
            return;
        }
    }
    
    fprintf(stderr, "SIGFPE in VM code.\n");
    Registers regs;
    linux_ucontext_to_regs(&regs, uc);

    // setup default handler
    signal(signum, SIG_DFL);

    if (!is_gdb_crash_handler_enabled() ||
        !gdb_crash_handler())
    {
        // print stack trace
        st_print_stack(&regs);
    }
}

void jvmti_jit_breakpoint_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Registers regs;

    linux_ucontext_to_regs(&regs, uc);
    TRACE2("signals", "JVMTI breakpoint detected at " <<
        (void *)regs.rip);
    assert(!interpreter_enabled());

    bool handled = jvmti_jit_breakpoint_handler(&regs);
    if (handled)
    {
        linux_regs_to_ucontext(uc, &regs);
        return;
    }

    fprintf(stderr, "SIGTRAP in VM code.\n");
    linux_ucontext_to_regs(&regs, uc);

    // setup default handler
    signal(signum, SIG_DFL);

    if (!is_gdb_crash_handler_enabled() ||
        !gdb_crash_handler())
    {
        // print stack trace
        st_print_stack(&regs);
    }
}

/**
 * Print out the call stack of the aborted thread.
 * @note call stacks may be used for debugging
 */
void abort_handler (int signum, siginfo_t* UNREF info, void* context) {
    fprintf(stderr, "SIGABRT in VM code.\n");
    Registers regs;
    ucontext_t *uc = (ucontext_t *)context;
    linux_ucontext_to_regs(&regs, uc);

    // setup default handler
    signal(signum, SIG_DFL);

    if (!is_gdb_crash_handler_enabled() ||
        !gdb_crash_handler())
    {
        // print stack trace
        st_print_stack(&regs);
    }
}

void general_signal_handler(int signum, siginfo_t* info, void* context)
{
    bool replaced = false;
    ucontext_t* uc = (ucontext_t *)context;
    POINTER_SIZE_INT saved_ip = (POINTER_SIZE_INT)uc->uc_mcontext.gregs[REG_RIP];
    POINTER_SIZE_INT new_ip = 0;

    // If exception is occured in processor instruction previously
    // instrumented by breakpoint, the actual exception address will reside
    // in jvmti_jit_breakpoints_handling_buffer
    // We should replace exception address with saved address of instruction
    POINTER_SIZE_INT break_buf =
        (POINTER_SIZE_INT)p_TLS_vmthread->jvmti_thread.jvmti_jit_breakpoints_handling_buffer;
    if (saved_ip >= break_buf &&
        saved_ip < break_buf + TM_JVMTI_MAX_BUFFER_SIZE)
    {
        // Breakpoints should not occur in breakpoint buffer
        assert(signum != SIGTRAP);

        replaced = true;
        new_ip = (POINTER_SIZE_INT)vm_get_ip_from_regs(p_TLS_vmthread);
        uc->uc_mcontext.gregs[REG_RIP] = (greg_t)new_ip;
    }

    // FIXME: Should unify all signals here as it is done on ia32
    jvmti_jit_breakpoint_handler(signum, info, context);

    // If EIP was not changed in specific handler to start another handler,
    // we should restore original EIP, if it's nesessary
    if (replaced &&
        (POINTER_SIZE_INT)uc->uc_mcontext.gregs[REG_RIP] == new_ip)
    {
        uc->uc_mcontext.gregs[REG_RIP] = (greg_t)saved_ip;
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
    sa.sa_sigaction = &null_java_reference_handler;
    sigaction(SIGSEGV, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &null_java_divide_by_zero_handler;
    sigaction(SIGFPE, &sa, NULL);
    
    signal(SIGINT, (void (*)(int)) vm_interrupt_handler);
    signal(SIGQUIT, (void (*)(int)) vm_dump_handler);

    /* install abort_handler to print out call stack on assertion failures */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &abort_handler;
    sigaction( SIGABRT, &sa, NULL);
    /* abort_handler installed */

    // Prepare gdb crash handler
    init_gdb_crash_handler();

} //initialize_signals

void shutdown_signals() {
    //FIXME: should be defined in future
} //shutdown_signals

