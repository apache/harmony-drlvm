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
#include <limits.h>
#include <unistd.h>
#undef __USE_XOPEN
#include <signal.h>

#include "open/platform_types.h"
#include "port_crash_handler.h"
#include "port_malloc.h"
#include "stack_dump.h"
#include "../linux/include/gdb_crash_handler.h"
#include "signals_internal.h"


port_tls_key_t port_tls_key;

int init_private_tls_data()
{
    return (pthread_key_create(&port_tls_key, NULL) == 0) ? 0 : -1;
}

int free_private_tls_data()
{
    return (pthread_key_delete(port_tls_key) == 0) ? 0 : -1;
}


// Because application can set up some protected area in stack region,
// we need to re-enable access to this area because we need to operate
// with the original stack of the thread
// Application should take care of restoring this protected area after
// signal processing
// FIXME This is workaround only; it also can break crash processing for SIGSEGV
// Ideally all the functionality on guard pages should be in Port.
// Unfortunately, it can involve thread creation in Port, additional
// thread wrapper, and moving general TLS operations to Port, so
// it's postponed
static void clear_stack_protection(Registers* regs, void* fault_addr)
{
    if (!fault_addr) // is not SO
        return;

    size_t fault = (size_t)fault_addr;
    size_t sp = (size_t)regs->get_sp();
    size_t diff = (fault > sp) ? (fault - sp) : (sp - fault);
    size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);

    if (diff > page_size)
        return; // Most probably is not SO

    size_t start = sp & ~(page_size - 1);
    size_t size = page_size;

    if (sp - start < 0x400)
    {
        start -= page_size;
        size += page_size;
    }

    int res = mprotect((void*)start, size, PROT_READ | PROT_WRITE);
}

static void c_handler(Registers* pregs, size_t signum, void* fault_addr)
{ // this exception handler is executed *after* OS signal handler returned
    int result;

    switch ((int)signum)
    {
    case SIGSEGV:
        result = port_process_signal(PORT_SIGNAL_GPF, pregs, fault_addr, FALSE);
        break;
    case SIGFPE:
        result = port_process_signal(PORT_SIGNAL_ARITHMETIC, pregs, fault_addr, FALSE);
        break;
    case SIGTRAP:
        // Correct return address
        pregs->set_ip((void*)((POINTER_SIZE_INT)pregs->get_ip() - 1));
        result = port_process_signal(PORT_SIGNAL_BREAKPOINT, pregs, fault_addr, FALSE);
        break;
    case SIGINT:
        result = port_process_signal(PORT_SIGNAL_CTRL_C, pregs, fault_addr, FALSE);
        break;
    case SIGQUIT:
        result = port_process_signal(PORT_SIGNAL_QUIT, pregs, fault_addr, FALSE);
        break;
    case SIGABRT:
        result = port_process_signal(PORT_SIGNAL_ABORT, NULL, fault_addr, FALSE);
        break;
    default:
        result = port_process_signal(PORT_SIGNAL_UNKNOWN, pregs, fault_addr, TRUE);
    }

    if (result == 0)
        return;

    // We've got a crash
    if (result > 0) // invoke debugger
    { // Prepare second catch of signal to attach GDB from signal handler
        port_tls_data* tlsdata = get_private_tls_data();
        if (!tlsdata)
        {   // STD_MALLOC can be harmful here
            tlsdata = (port_tls_data*)STD_MALLOC(sizeof(port_tls_data));
            memset(tlsdata, 0, sizeof(port_tls_data));
            set_private_tls_data(tlsdata);
        }

        tlsdata->debugger = TRUE;
        return; // To produce signal again
    }

    // result < 0 - exit process
    if ((port_crash_handler_get_flags() & PORT_CRASH_DUMP_PROCESS_CORE) != 0)
    { // Return to the same place to produce the same crash and generate core
        signal(signum, SIG_DFL); // setup default handler
        return;
    }

    // No core needed - simply terminate
    _exit(-1);
}

static void general_signal_handler(int signum, siginfo_t* info, void* context)
{
    Registers regs;

    if (!context)
        return;

    // Convert OS context to Registers
    port_thread_context_to_regs(&regs, (ucontext_t*)context);

    // Check if SIGSEGV is produced by port_read/write_memory
    port_tls_data* tlsdata = get_private_tls_data();
    if (tlsdata && tlsdata->violation_flag)
    {
        tlsdata->violation_flag = 0;
        regs.set_ip(tlsdata->restart_address);
        return;
    }

    if (tlsdata && tlsdata->debugger)
    {
        bool result = gdb_crash_handler(&regs);
        _exit(-1); // Exit process if not sucessful...
    }

    if (signum == SIGABRT && // SIGABRT can't be trown again from c_handler
        (port_crash_handler_get_flags() & PORT_CRASH_CALL_DEBUGGER) != 0)
    { // So attaching GDB right here
        bool result = gdb_crash_handler(&regs);
        _exit(-1); // Exit process if not sucessful...
    }

    if (signum == SIGSEGV)
        clear_stack_protection(&regs, info->si_addr);

    // Prepare registers for transfering control out of signal handler
    void* callback = (void*)&c_handler;
    void* fault_addr = info ? info->si_addr : NULL;

    port_set_longjump_regs(callback, &regs, 3,
                            &regs, (void*)(size_t)signum, fault_addr);

    // Convert prepared Registers back to OS context
    port_thread_regs_to_context((ucontext_t*)context, &regs);
    // Return from signal handler to go to C handler
}


struct sig_reg
{
    int signal;
    port_sigtype port_sig;
    int flags;
    bool set_up;
};

static sig_reg signals_used[] =
{
    { SIGTRAP, PORT_SIGNAL_BREAKPOINT, SA_SIGINFO, false },
    { SIGSEGV, PORT_SIGNAL_GPF,        SA_SIGINFO | SA_ONSTACK, false },
    { SIGFPE,  PORT_SIGNAL_ARITHMETIC, SA_SIGINFO, false },
    { SIGINT,  PORT_SIGNAL_CTRL_C,     SA_SIGINFO, false },
    { SIGQUIT, PORT_SIGNAL_QUIT,       SA_SIGINFO, false },
    { SIGABRT, PORT_SIGNAL_ABORT,      SA_SIGINFO, false }
};

static struct sigaction old_actions[sizeof(signals_used)/sizeof(signals_used[0])];


static void restore_signals()
{
    for (size_t i = 0; i < sizeof(signals_used)/sizeof(signals_used[0]); i++)
    {
        if (!signals_used[i].set_up)
            continue;

        signals_used[i].set_up = false;
        sigaction(signals_used[i].signal, &old_actions[i], NULL);
    }
}

int initialize_signals()
{
    struct sigaction sa;

    for (size_t i = 0; i < sizeof(signals_used)/sizeof(signals_used[0]); i++)
    {
        if (!sd_is_handler_registered(signals_used[i].port_sig))
            continue;

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = signals_used[i].flags;
        sa.sa_sigaction = &general_signal_handler;

        if (0 != sigaction(signals_used[i].signal, &sa, &old_actions[i]))
        {
            restore_signals();
            return -1;
        }

        signals_used[i].set_up = true;
    }

    // Prepare gdb crash handler
    if (!init_gdb_crash_handler())
    {
        restore_signals();
        return -1;
    }

    return 0;

} //initialize_signals

int shutdown_signals()
{
    cleanup_gdb_crash_handler();
    restore_signals();
    return 0;
} //shutdown_signals

void sig_process_crash_flags_change(unsigned added, unsigned removed)
{
// Still empty on Linux
}
