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

#include "open/platform_types.h"
#include "port_crash_handler.h"
#include "init.h"
#include "vm_threads.h"
#include "environment.h"
#include "signals.h"


Boolean abort_handler(port_sigtype UNREF signum, Registers* UNREF regs, void* fault_addr)
{
    TRACE2("signals", "Abort detected at " << regs->get_ip());
    // Crash always
    return FALSE;
}

Boolean ctrl_backslash_handler(port_sigtype UNREF signum, Registers* UNREF regs, void* UNREF fault_addr)
{
    TRACE2("signals", "Ctrl+\\ pressed");
    vm_dump_handler();
    return TRUE;
}

Boolean ctrl_break_handler(port_sigtype UNREF signum, Registers* UNREF regs, void* UNREF fault_addr)
{
    TRACE2("signals", "Ctrl+Break pressed");
    vm_dump_handler();
    return TRUE;
}

Boolean ctrl_c_handler(port_sigtype UNREF signum, Registers* UNREF regs, void* UNREF fault_addr)
{
    TRACE2("signals", "Ctrl+C pressed");
    vm_interrupt_handler();
    // Continue execution - the process will be terminated from
    // the separate thread started in vm_interrupt_handler()
    return TRUE;
}

Boolean native_breakpoint_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", "Native breakpoint detected at " << regs->get_ip());
#ifdef _IPF_
    assert(0); // Not implemented
    abort();
#endif

    if (VM_Global_State::loader_env == NULL || interpreter_enabled())
        return FALSE; // Crash

    vm_thread_t vmthread = get_thread_ptr();

    if (is_in_ti_handler(vmthread, regs->get_ip()))
        // Breakpoints should not occur in breakpoint buffer
        return FALSE; // Crash

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    ncai_process_signal_event((NativeCodePtr)regs->get_ip(),
                                (jint)signum, true, &is_handled);
    if (is_handled)
        return TRUE;

    return (Boolean)jvmti_jit_breakpoint_handler(regs);
}

Boolean arithmetic_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", "ArithmeticException detected at " << regs->get_ip());
#ifdef _IPF_
    assert(0); // Not implemented
    abort();
#endif

    vm_thread_t vmthread = get_thread_ptr();
    void* saved_ip = regs->get_ip();
    void* new_ip = NULL;

    if (is_in_ti_handler(vmthread, saved_ip))
    {
        new_ip = vm_get_ip_from_regs(vmthread);
        regs->set_ip(new_ip);
    }

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

    if (!vmthread || VM_Global_State::loader_env == NULL ||
        !is_in_java(regs) || interpreter_enabled())
        return FALSE; // Crash

    signal_throw_java_exception(regs, VM_Global_State::loader_env->java_lang_ArithmeticException_Class);

    if (new_ip && regs->get_ip() == new_ip)
        regs->set_ip(saved_ip);

    return TRUE;
}
