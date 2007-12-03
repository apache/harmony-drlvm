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
 * @version $Revision: 1.1.2.2.4.4 $
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
#include <iostream.h>
#include <fstream.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/ucontext.h>
#include <sys/wait.h>

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
#include "jvmti_break_intf.h"


#include <semaphore.h>

void linux_ucontext_to_regs(Registers* regs, ucontext_t* uc)
{
    memcpy(regs->gr, uc->uc_mcontext.sc_gr, sizeof(regs->gr));
    memcpy(regs->fp, uc->uc_mcontext.sc_fr, sizeof(regs->fp));
    memcpy(regs->br, uc->uc_mcontext.sc_br, sizeof(regs->br));

    regs->preds = uc->uc_mcontext.sc_pr;
    regs->nats  = uc->uc_mcontext.sc_ar_rnat;
    regs->pfs   = uc->uc_mcontext.sc_ar_pfs;
    regs->bsp   = (uint64*)uc->uc_mcontext.sc_ar_bsp;
    regs->ip    = uc->uc_mcontext.sc_ip;
}

void linux_regs_to_ucontext(ucontext_t* uc, Registers* regs)
{
    memcpy(uc->uc_mcontext.sc_gr, regs->gr, sizeof(regs->gr));
    memcpy(uc->uc_mcontext.sc_fr, regs->fp, sizeof(regs->fp));
    memcpy(uc->uc_mcontext.sc_br, regs->br, sizeof(regs->br));

    uc->uc_mcontext.sc_pr      = regs->preds;
    uc->uc_mcontext.sc_ar_rnat = regs->nats;
    uc->uc_mcontext.sc_ar_pfs  = regs->pfs;
    uc->uc_mcontext.sc_ar_bsp  = (uint64)regs->bsp;
    uc->uc_mcontext.sc_ip      = regs->ip;
}


void asm_exception_catch_callback() {
    // FIXME: not implemented
    fprintf(stderr, "FIXME: asm_jvmti_exception_catch_callback: not implemented\n");
    assert(0);
    abort();
}

void asm_jvmti_exception_catch_callback() {
    // FIXME: not implemented
    fprintf(stderr, "FIXME: asm_jvmti_exception_catch_callback: not implemented\n");
    assert(0);
    abort();
}

static bool java_throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    // FIXME: not implemented
    fprintf(stderr, "FIXME: java_throw_from_sigcontext: not implemented\n");
    assert(0);
    abort();
    return false;
}

void abort_handler (int signum, siginfo_t* info, void* context) {
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


static void throw_from_sigcontext(ucontext_t *uc, Class* exc_clss) {
    // FIXME: not implemented
    fprintf(stderr, "FIXME: throw_from_sigcontext: not implemented\n");
    assert(0);
    abort();
}

void null_java_divide_by_zero_handler(int signum, siginfo_t* UNREF info, void* context)
{
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;

    /* Will not compile on IPF:
     * TRACE2("signals", "ArithmeticException detected at " <<
        (void *)uc->uc_mcontext.gregs[REG_EIP]);
     */

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

}

bool check_stack_overflow(siginfo_t *info, ucontext_t *uc) {
    char* stack_addr = (char*) get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t guard_stack_size = get_guard_stack_size();
    size_t guard_page_size = get_guard_page_size();

    char* guard_page_begin = stack_addr - stack_size + guard_page_size + guard_stack_size;
    char* guard_page_end = guard_page_begin + guard_page_size;

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
            vm_thread_t vm_thread = p_TLS_vmthread;
            remove_guard_stack(vm_thread);
            exn_raise_by_name("java/lang/StackOverflowError");
            vm_thread->restore_guard_page = true;
        }
    }
}

void null_java_reference_handler(int signum, siginfo_t* UNREF info, void* context)
{ 
    ucontext_t *uc = (ucontext_t *)context;
    Global_Env *env = VM_Global_State::loader_env;


    /* Will not compile on IPF:
     TRACE2("signals", "NPE or SOE detected at " <<
        (void *)uc->uc_mcontext.gregs[REG_EIP]); */

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

void initialize_signals() {
    struct sigaction sa;
    
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

#if 0

static sigset_t signal_set;

extern "C" void get_rnat_and_bspstore(uint64* res);
extern "C" void do_flushrs();

// Variables used to locate the context from the signal handler
static int sc_nest = -1;
static bool use_ucontext = false;
static uint32 exam_point;

bool SuspendThread(unsigned xx){ return 0; }
bool ResumeThread(unsigned xx){ return 1; }

static bool linux_throw_from_sigcontext(ucontext_t *uc, Class* exc_clss)
{
    // FIXME: not implemented
    assert(0);
    abort();
    return false;
}

static void throw_from_sigcontext(ucontext_t *uc, Class* exc_clss) {
    // FIXME: not implemented
    assert(0);
    abort();
    return false;
}


/*
 * We find the true signal stack frame set-up by kernel,which is located
 * by locate_sigcontext() below; then change its content according to 
 * exception handling semantics, so that when the signal handler is 
 * returned, application can continue its execution in Java exception handler.
 */

void null_java_reference_handler(int signum)
{
    uint32* top_ebp = NULL;
//TODO: ADD correct stack handling here!!
    Global_Env *env = VM_Global_State::loader_env;
    linux_throw_from_sigcontext(top_ebp, env->java_lang_NullPointerException_Class);
}


void null_java_divide_by_zero_handler(int signum)
{
    uint32* top_ebp = NULL;
//TODO: ADD correct stack handling here!!
    Global_Env *env = VM_Global_State::loader_env;
    linux_throw_from_sigcontext(top_ebp, env->java_lang_ArithmeticException_Class);
}

/*
See function initialize_signals() below first please.

Two kinds of signal contexts (and stack frames) are tried in
locate_sigcontext(), one is sigcontext, which is the way of
Linux kernel implements( see Linux kernel source
arch/i386/kernel/signal.c for detail); the other is ucontext,
used in some other other platforms.

The sigcontext locating in Linux is not so simple as expected,
because it involves not only the kernel, but also glibc/linuxthreads,
which VM is linked against. In glibc/linuxthreads, user-provided
signal handler is wrapped by linuxthreads signal handler, i.e.
the signal handler really registered in system is not the one
provided by user. So when Linux kernel finishes setting up signal
stack frame and returns to user mode for singal handler execution,
locate_sigcontext() is not the one being invoked immediately. It's 
called by linuxthreads function. That means the user stack viewed by
locate_sigcontext() is NOT NECESSARILY the signal frame set-up by
kernel, we need find the true one according to glibc/linuxthreads
specific signal implementation in different versions.

Because locate_sigcontext() uses IA32 physical register epb for
call stack frame, compilation option `-fomit-frame-pointer' MUST
not be used when gcc compiles it; and as gcc info, `-O2' will do
`-fomit-frame-pointer' by default, although we haven't seen that
in our experiments.
*/

volatile void locate_sigcontext(int signum)
{
    sigcontext *sc, *found_sc;
    uint32 *ebp = NULL;
    int i;

//TODO: ADD correct stack handling here!!

#define SC_SEARCH_WIDTH 3
    for (i = 0; i < SC_SEARCH_WIDTH; i++) {
        sc = (sigcontext *)(ebp + 3 );
        if (sc->sc_ip == ((uint32)exam_point)) {    // found
            sc_nest = i;
            use_ucontext = false;
            found_sc = sc;
        // we will try to find the real sigcontext setup by Linux kernel,
        // because if we want to change the execution path after the signal
        // handling, we must modify the sigcontext used by kernel. 
        // LinuxThreads in glibc 2.1.2 setups a sigcontext for our singal
        // handler, but we should not modify it, because kernel doesn't use 
        // it when resumes the application. Then we must find the sigcontext
        // setup by kernel, and modify it in singal handler. 
        // but, with glibc 2.2.3, it's useless to modify only the sigcontext
        // setup by Linux kernel, because LinuxThreads does a interesting 
        // copy after our signal handler returns, which destroys the 
        // modification we have just done in the handler. So with glibc 2.2.3,
        // what we need do is simply to modify the sigcontext setup by 
        // LinuxThreads, which will be copied to overwrite the one setup by 
        // kernel. Really complicated..., not really. We use a simple trick
        // to overcome the changes in glibc from version to version, that is,
        // we modify both sigcontexts setup by kernel and LinuxThreads. Then
        // it will always work. 

        } else {                    // not found
            struct ucontext *uc;
            uc = (struct ucontext *)((uint64)ebp[4]);
            if ((ebp < (uint32 *)uc) && ((uint32 *)uc < ebp + 0x100)) {
                sc = (sigcontext *)&uc->uc_mcontext;
                if (sc->sc_ip == ((uint32)exam_point)) {    // found
                    sc_nest = i;
                    use_ucontext = true;
                    found_sc = sc;
                    break; 
                }
            }
        }

        ebp = (uint32 *)((uint64)ebp[0]);
    }

    if (sc_nest < 0) {
        printf("cannot locate sigcontext.\n");
        printf("Please add or remove any irrelevant statement(e.g. add a null printf) in VM source code, then rebuild it. If problem remains, please submit a bug report. Thank you very much\n");
        exit(1);
    }
}



#endif
