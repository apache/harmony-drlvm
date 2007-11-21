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

#if defined(LINUX)
void linux_ucontext_to_regs(Registers* regs, ucontext_t *uc)
{
    regs->eax = uc->uc_mcontext.gregs[REG_EAX];
    regs->ecx = uc->uc_mcontext.gregs[REG_ECX];
    regs->edx = uc->uc_mcontext.gregs[REG_EDX];
    regs->edi = uc->uc_mcontext.gregs[REG_EDI];
    regs->esi = uc->uc_mcontext.gregs[REG_ESI];
    regs->ebx = uc->uc_mcontext.gregs[REG_EBX];
    regs->ebp = uc->uc_mcontext.gregs[REG_EBP];
    regs->eip = uc->uc_mcontext.gregs[REG_EIP];
    regs->esp = uc->uc_mcontext.gregs[REG_ESP];
    regs->eflags = uc->uc_mcontext.gregs[REG_EFL];
}

void linux_regs_to_ucontext(ucontext_t *uc, Registers* regs)
{
    uc->uc_mcontext.gregs[REG_EAX] = regs->eax;
    uc->uc_mcontext.gregs[REG_ECX] = regs->ecx;
    uc->uc_mcontext.gregs[REG_EDX] = regs->edx;
    uc->uc_mcontext.gregs[REG_EDI] = regs->edi;
    uc->uc_mcontext.gregs[REG_ESI] = regs->esi;
    uc->uc_mcontext.gregs[REG_EBX] = regs->ebx;
    uc->uc_mcontext.gregs[REG_EBP] = regs->ebp;
    uc->uc_mcontext.gregs[REG_EIP] = regs->eip;
    uc->uc_mcontext.gregs[REG_ESP] = regs->esp;
    uc->uc_mcontext.gregs[REG_EFL] = regs->eflags;
}

#define UCONTEXT_TO_REGS linux_ucontext_to_regs
#define REGS_TO_UCONTEXT linux_regs_to_ucontext
#define REGISTER_EIP uc->uc_mcontext.gregs[REG_EIP]

#else
#if defined(FREEBSD)
void freebsd_ucontext_to_regs(Registers* regs, ucontext_t *uc)
{
    regs->eax = uc->uc_mcontext.mc_eax;
    regs->ecx = uc->uc_mcontext.mc_ecx;
    regs->edx = uc->uc_mcontext.mc_edx;
    regs->edi = uc->uc_mcontext.mc_edi;
    regs->esi = uc->uc_mcontext.mc_esi;
    regs->ebx = uc->uc_mcontext.mc_ebx;
    regs->ebp = uc->uc_mcontext.mc_ebp;
    regs->eip = uc->uc_mcontext.mc_eip;
    regs->esp = uc->uc_mcontext.mc_esp;
    regs->eflags = uc->uc_mcontext.mc_eflags;
}

void freebsd_regs_to_ucontext(ucontext_t *uc, Registers* regs)
{
    uc->uc_mcontext.mc_eax = regs->eax;
    uc->uc_mcontext.mc_ecx = regs->ecx;
    uc->uc_mcontext.mc_edx = regs->edx;
    uc->uc_mcontext.mc_edi = regs->edi;
    uc->uc_mcontext.mc_esi = regs->esi;
    uc->uc_mcontext.mc_ebx = regs->ebx;
    uc->uc_mcontext.mc_ebp = regs->ebp;
    uc->uc_mcontext.mc_eip = regs->eip;
    uc->uc_mcontext.mc_esp = regs->esp;
    uc->uc_mcontext.mc_eflags = regs->eflags;
}

#define UCONTEXT_TO_REGS freebsd_ucontext_to_regs
#define REGS_TO_UCONTEXT freebsd_regs_to_ucontext
#define REGISTER_EIP uc->uc_mcontext.mc_eip
#else
#error need to add correct mcontext_t lookup for registers
#endif
#endif


void regs_push_param(Registers* pregs, POINTER_SIZE_INT param, int UNREF num)
{
    pregs->esp = pregs->esp - 4;
    *((uint32*)pregs->esp) = param;
}

void regs_push_return_address(Registers* pregs, void* ret_addr)
{
    pregs->esp = pregs->esp - 4;
    *((void**)pregs->esp) = ret_addr;
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
    UCONTEXT_TO_REGS(&regs, uc);

    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    bool java_code = (vm_identify_eip((void *)regs.eip) == VM_TYPE_JAVA);
    VM_thread* vmthread = p_TLS_vmthread;
    NativeCodePtr callback = (NativeCodePtr) c_exception_handler;

    vm_set_exception_registers(vmthread, regs);
    regs_push_param(&regs, java_code, 1/*2nd arg */);
    assert(exc_clss);
    regs_push_param(&regs, (POINTER_SIZE_INT)exc_clss, 0/* 1st arg */);
    // imitate return IP on stack
    regs_push_return_address(&regs, NULL);

    // set up the real exception handler address
    regs.set_ip(callback);
    REGS_TO_UCONTEXT(uc, &regs);
}

static bool java_throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    ASSERT_NO_INTERPRETER;
    unsigned *eip = (unsigned *) REGISTER_EIP;
    VM_Code_Type vmct = vm_identify_eip((void *)eip);
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

        execle("/usr/bin/addr2line", "addr2line", "-e", executable, "-C", "-s", NULL, env);

    } else { // parent
        close(pipes[0]);        // close unneeded read pipe

        write(pipes[1],buf,strlen(buf));
        close(pipes[1]);        // close write pipe

        int status;
        wait(&status); // wait for the child to complete
    }
}

/**
 * Print out the call stack.
 */
void print_native_stack (unsigned *ebp) {
    int depth = 17;
    LWARN(42, "Fatal error");
    char buf[1024];
    unsigned int n = 0;
    while (ebp && ebp[1] && --depth >= 0 && (n<sizeof(buf)-20)) {
        n += sprintf(buf+n,"%08x\n",ebp[1]);
        ebp = (unsigned *)ebp[0];
    }
    addr2line(buf);
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

static void __attribute__ ((cdecl)) stack_holder(char* addr) {
    char buf[1024];
    
    if (addr > (buf + ((size_t)1024))) {
        return;
    }
    stack_holder(addr);
}

void init_stack_info() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();

    // find stack parametrs
    char* stack_addr = (char *)find_stack_addr();
    vm_thread->stack_addr = stack_addr;
    unsigned int stack_size = hythread_get_thread_stacksize(hythread_self());
    
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

    // makes sure that stack allocated till mapping_page_addr
    stack_holder(mapping_page_addr);

    // found size of the stack area which should be maped
    size_t stack_mapping_size = (size_t)mapping_page_addr
            - (size_t)stack_addr + stack_size;

    // maps unmapped part of the stack
    res = (char*) mmap(stack_addr - stack_size,
            stack_mapping_size,
            PROT_READ | PROT_WRITE,
            MAP_FIXED | MAP_PRIVATE |
#if defined(FREEBSD)
              MAP_ANON | MAP_STACK, 
#else
              MAP_ANONYMOUS | MAP_GROWSDOWN,
#endif
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
size_t get_default_stack_size() {
    size_t default_stack_size = get_stack_size();
    return default_stack_size;
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


    err = mprotect(stack_addr - stack_size + guard_page_size +
    guard_stack_size, guard_page_size, PROT_READ | PROT_WRITE);


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

void null_java_reference_handler(int signum, siginfo_t* UNREF info, void* context)
{ 
    ucontext_t *uc = (ucontext_t *)context;
    VM_thread *vm_thread = p_TLS_vmthread;
    Registers regs;
    UCONTEXT_TO_REGS(&regs, uc);

    if (vm_thread && vm_thread->jvmti_thread.violation_flag)
    {
        vm_thread->jvmti_thread.violation_flag = 0;
        regs.set_ip(vm_thread->jvmti_thread.violation_restart_address);
        linux_regs_to_ucontext(uc, &regs);
        return;
    }

    Global_Env *env = VM_Global_State::loader_env;

    TRACE2("signals", "NPE or SOE detected at " << (void*)REGISTER_EIP);

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

    // setup default handler
    signal(signum, SIG_DFL);

    if (!is_gdb_crash_handler_enabled() ||
        !gdb_crash_handler())
    {
        // print stack trace
        st_print_stack(&regs);
    }
}


void null_java_divide_by_zero_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;

    TRACE2("signals",
           "ArithmeticException detected at " << (void*)REGISTER_EIP);

    if (!interpreter_enabled()) {
        if (java_throw_from_sigcontext(
                    uc, env->java_lang_ArithmeticException_Class)) {
            return;
        }
    }

    fprintf(stderr, "SIGFPE in VM code.\n");
    Registers regs;
    UCONTEXT_TO_REGS(&regs, uc);

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

    UCONTEXT_TO_REGS(&regs, uc);
    TRACE2("signals", "JVMTI breakpoint detected at " <<
        (void *)regs.eip);

    bool handled = jvmti_jit_breakpoint_handler(&regs);
    if (handled)
    {
        REGS_TO_UCONTEXT(uc, &regs);
        return;
    }

    fprintf(stderr, "SIGTRAP in VM code.\n");
    UCONTEXT_TO_REGS(&regs, uc);

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
    UCONTEXT_TO_REGS(&regs, uc);
    
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
    uint32 saved_eip = (uint32)REGISTER_EIP;
    uint32 new_eip = saved_eip;
    VM_thread* vm_thread = p_TLS_vmthread;
    bool violation =
        (signum == SIGSEGV) && vm_thread && vm_thread->jvmti_thread.violation_flag;

    // If exception is occured in processor instruction previously
    // instrumented by breakpoint, the actual exception address will reside
    // in jvmti_jit_breakpoints_handling_buffer
    // We should replace exception address with saved address of instruction
    uint32 break_buf =
        (uint32)vm_thread->jvmti_thread.jvmti_jit_breakpoints_handling_buffer;
    if (saved_eip >= break_buf &&
        saved_eip < break_buf + TM_JVMTI_MAX_BUFFER_SIZE)
    {
        // Breakpoints should not occur in breakpoint buffer
        assert(signum != SIGTRAP);

        replaced = true;
        new_eip = (uint32)vm_get_ip_from_regs(vm_thread);
        REGISTER_EIP = new_eip;
    }

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    bool is_internal = (signum == SIGTRAP) || violation;
    ncai_process_signal_event((NativeCodePtr)uc->uc_mcontext.gregs[REG_EIP],
        (jint)signum, is_internal, &is_handled);
    if (is_handled)
        return;

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

    // If EIP was not changed in specific handler to start another handler,
    // we should restore original EIP, if it's nesessary
    if (replaced &&
        (uint32)REGISTER_EIP == new_eip)
    {
        REGISTER_EIP = saved_eip;
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

} //initialize_signals

void shutdown_signals() {
    //FIXME: should be defined in future
} //shutdown_signals
