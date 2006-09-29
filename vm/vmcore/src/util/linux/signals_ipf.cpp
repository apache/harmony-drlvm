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

#include "platform.h"
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
 
#include "exceptions.h"
#include "vm_synch.h"
#include "vm_threads.h"
#include "open/vm_util.h"
#include "compile.h"
#include "vm_synch.h"
#include "vm_stats.h"
#include "sync_bits.h"

#include "object_generic.h"
#include "thread_manager.h"

#include "exception_filter.h"
#include "crash_handler.h"


#include <semaphore.h>

static sigset_t signal_set;

extern "C" void get_rnat_and_bspstore(uint64* res);
extern "C" void do_flushrs();

void get_context_handler(int signal, siginfo_t* info, void* context) {
    VM_thread* thread = p_active_threads_list;
    pthread_t self = GetCurrentThreadId();
    TRACE2("SIGNALLING", "get_context_handler, try to find pthread_t " << self);

    while (thread) {

        TRACE2("SIGNALLING", "get_context_handler, finding pthread_t " << self);

        if (thread->thread_id != self) {
        thread = thread->p_active;
        continue;
    }

        assert(thread->suspend_request >0);
        TRACE2("SIGNALLING", "get_context_handler, found pthread_t " << self);

    ucontext_t* c = (ucontext_t*) context;

    uint64 ip = c->uc_mcontext.sc_ip;
    thread->regs.ip = ip;

    TRACE2("SIGNALLING", "get_context_handler, ip is  " << ip);

    uint64* bsp = (uint64*)c->uc_mcontext.sc_ar_bsp;
    uint64 rnat = c->uc_mcontext.sc_ar_rnat;
    thread->t[0] = rnat;
    thread->t[1] = (uint64)bsp;
    thread->suspended_state = SUSPENDED_IN_SIGNAL_HANDLER;
    do_flushrs();
    
        TRACE2("SIGNALLING", "HANDLER, before POST thread = " << self);
        sem_post(&thread->suspend_self);
        TRACE2("SIGNALLING", "HANDLER, after POST thread = " << self);

        //vm_wait_for_single_object(thread->resume_event, INFINITE);
        
        TRACE2("SIGNALLING", "get_context_handler, continue pthread_t " << self);
        
    //thread->suspended_state = NOT_SUSPENDED;
    do_flushrs();
    return;
    }

    DIE("Cannot find Java thread using signal context");
}

void initialize_signals() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = get_context_handler;
    int res = sigaction(SIGUSR2, &sa, NULL);

    if (res != 0) {
        DIE("sigaction fail: SIGUSR2: " << strerror(errno) << "\nDRL VM will exit with error\n");
    }

    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGCONT);
    sigprocmask(SIG_BLOCK, &signal_set, NULL);

    extern void interrupt_handler(int);
    signal(SIGINT, (void (*)(int)) interrupt_handler);
    extern void quit_handler(int);
    signal(SIGQUIT, (void (*)(int)) quit_handler);

    if (vm_get_boolean_property_value_with_default("vm.crash_handler")) {
        init_crash_handler();
        install_crash_handler(SIGABRT);
        install_crash_handler(SIGSEGV); 
    }
}

// Variables used to locate the context from the signal handler
static int sc_nest = -1;
static bool use_ucontext = false;
static uint32 exam_point;

bool SuspendThread(unsigned xx){ return 0; }
bool ResumeThread(unsigned xx){ return 1; }

void linux_ucontext_to_regs(Registers* regs, ucontext_t* uc)
{
//TODO: ADD Copying of IPF registers here like it was done on ia32!!
}

void linux_regs_to_ucontext(ucontext_t* uc, Registers* regs)
{
//TODO: ADD Copying of IPF registers here like it was done on ia32!!
}

static void linux_sigcontext_to_regs(Registers* regs, sigcontext* sc)
{
//TODO: ADD Copying of IPF registers here like it was done on ia32!!
}

static void linux_regs_to_sigcontext(sigcontext* sc, Registers* regs)
{
//TODO: ADD Copying of IPF registers here like it was done on ia32!!
}

static void linux_throw_from_sigcontext(uint32* top_ebp, Class* exc_clss)
{
    sigcontext *sc, *top_sc = NULL;
    uint32 *ebp;
    int i;

    ebp = top_ebp;

    for (i = 0; i < sc_nest; i++) {
        ebp = (uint32 *)((uint64)ebp[0]);
    }
    if (!use_ucontext) {
        sc = (sigcontext *)(ebp + 3);
        top_sc = (sigcontext *)(top_ebp + 3);
    } else {
        sc = (sigcontext *) &((struct ucontext *)((uint64)ebp[4]))->uc_mcontext;
    }

    assert(vm_identify_eip((void *)sc->sc_ip) == VM_TYPE_JAVA);

    Registers regs;
//    linux_sigcontext_to_regs(&regs, sc); // FIXME: transfer to linux_ucontext_to_regs
    exn_athrow_regs(&regs, exc_clss);
//    linux_regs_to_sigcontext(sc, &regs); // FIXME: transfer to linux_regs_to_ucontext
//    linux_regs_to_sigcontext(top_sc, &regs); // FIXME: transfer to linux_regs_to_ucontext
}

/*
 * We find the true signal stack frame set-up by kernel,which is located
 * by locate_sigcontext() below; then change its content according to 
 * exception handling semantics, so that when the signal handler is 
 * returned, application can continue its execution in Java exception handler.
 */

void null_java_reference_handler(int signum)
{
    if (VM_Global_State::loader_env->shutting_down != 0) {
        fprintf(stderr, "null_java_reference_handler(): called in shutdown stage\n");

        // crash with default handler
        signal(signum, 0);
    }

    uint32* top_ebp = NULL;
//TODO: ADD correct stack handling here!!
    Global_Env *env = VM_Global_State::loader_env;
    linux_throw_from_sigcontext(top_ebp, env->java_lang_NullPointerException_Class);
}


void null_java_divide_by_zero_handler(int signum)
{
    if (VM_Global_State::loader_env->shutting_down != 0) {
        fprintf(stderr, "null_java_divide_by_zero_handler(): called in shutdown stage\n");

        // crash with default handler
        signal(signum, 0);
    }

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



void initialize_signals_old()
{
    // First figure out how to locate the context in the
    // signal handler.  Apparently you have to do it differently
    // on different versions of Linux.

    //attach signal handler locate_sigcontext() with SIGTRAP

    signal(SIGTRAP, (void (*)(int))locate_sigcontext);

    //this asm code sequence is to cause a signal SIGTRAP
    //delivered to itself by operating system

//TODO: ADD correct signal handling here!!
/*    asm(
        ".align 16\n\t"
        ".byte 0xe8\n\t.long 0\n\t" // call 0

        //this is "call 0", which does nothing at runtime
        //but pushing the return address, i.e. instruction
        //pointer to next instruction, i.e. "popl %%eax"

        "popl  %%eax\n\t"   // 1 byte

        //pop the return address into eax for arithmetic

        "addb  $9,%%al\n\t" // 2 byte

        //add the return address with 9, which is the address
        //pointing to instruction after the "int $0x3" below.
        //The instructions length in byte are shown on the
        //rightside of each.

        "movl  %%eax,%0\n\t"    // 5 byte

        //move eax content into static memory variable "exam_point",
        //which is used for sigcontext locating, because when "int 0x3"
        //happens, operating system will save the return address
        //into signal context for process execution resumption;
        //the trick here is that we also save that address in exam_point,
        //so that we can compare the return EIP saved in signal context
        //with exam_point to identify the correct stack frame


        "int   $0x3" : "=m" (exam_point)

        //trigger int3 into operating system kernel, which sends a
        //signal SIGTRAP to current process, setups the signal stack
        //frame on user stack, then returns to user mode execution,
        //i.e. the signal handler locate_sigcontext()

    ); */

    //when execution goes here, the signal handler locate_sigcontext()
    //has successfully found the signal context setup method on current
    //system; resume SIGTRAP handler back to default

    signal(SIGTRAP, SIG_DFL);
    
    //Now register the real signal handlers. signal() function in Linux
    //kernel has SYSV semantics, i.e. the handler is a kind of ONE SHOT
    //behaviour, which is different from BSD. But glibc2 in Linux
    //implements BSD semantics.

    signal(SIGSEGV, null_java_reference_handler);
    signal(SIGFPE, null_java_divide_by_zero_handler);
    
} //initialize_signals
