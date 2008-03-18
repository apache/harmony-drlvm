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

#include <unistd.h>
#undef __USE_XOPEN
#include <signal.h>

#include "open/platform_types.h"
#include "port_crash_handler.h"
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


static void c_handler(Registers* pregs, size_t signum, void* fault_addr)
{ // this exception handler is executed *after* VEH handler returned
    int result;
    Registers regs = *pregs;

    // Check if SIGSEGV is produced by port_read/write_memory
    port_tls_data* tlsdata = get_private_tls_data();
    if (tlsdata && tlsdata->violation_flag)
    {
        tlsdata->violation_flag = 0;
        pregs->set_ip(tlsdata->restart_address);
        return;
    }

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
    default:
        result = port_process_signal(PORT_SIGNAL_UNKNOWN, pregs, fault_addr, TRUE);
    }

    if (result == 0)
        return;

    // We've got a crash
    if (result > 0)
    { // result > 0 - invoke debugger
        bool result = gdb_crash_handler(&regs);
        // Continue with exiting process if not sucessful...
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
    // Convert OS context to Registers
    port_thread_context_to_regs(&regs, (ucontext_t*)context);
    // Prepare registers for transfering control out of signal handler
    void* callback = (void*)&c_handler;
    port_set_longjump_regs(callback, &regs, 3,
                            &regs, (void*)(size_t)signum, info->si_addr);
    // Convert prepared Registers back to OS context
    port_thread_regs_to_context((ucontext_t*)context, &regs);
    // Return from signal handler to go to C handler
}


struct sig_reg
{
    int signal;
    int flags;
    bool set_up;
};

static sig_reg signals_used[] =
{
    { SIGTRAP, SA_SIGINFO, false },
    { SIGSEGV, SA_SIGINFO | SA_ONSTACK, false },
    { SIGFPE,  SA_SIGINFO, false },
    { SIGINT,  SA_SIGINFO, false },
    { SIGQUIT, SA_SIGINFO, false },
    { SIGABRT, SA_SIGINFO, false }
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
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = signals_used[i].flags;
        sa.sa_sigaction = &general_signal_handler;

        if (0 != sigaction(signals_used[i].signal, &sa, &old_actions[i]))
        {
            restore_signals();
            return -1;
        }
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
