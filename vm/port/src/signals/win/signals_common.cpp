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


#include <crtdbg.h>
#include <process.h>
#include <signal.h>
#include "open/platform_types.h"
#include "open/hythread_ext.h"
#include "port_malloc.h"
#include "port_mutex.h"

#include "port_crash_handler.h"
#include "stack_dump.h"
#include "signals_internal.h"


#if INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
#define JVMTI_EXCEPTION_STATUS STATUS_BREAKPOINT
#elif INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_HLT || INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_CLI
#define JVMTI_EXCEPTION_STATUS STATUS_PRIVILEGED_INSTRUCTION
#else
#error Unknown value of INSTRUMENTATION_BYTE
#endif


port_tls_key_t port_tls_key = TLS_OUT_OF_INDEXES;

typedef void (__cdecl *sigh_t)(int); // Signal handler type

static PVOID veh = NULL;
static sigh_t prev_sig = (sigh_t)SIG_ERR;
// Mutex to protect access to the global data
static osmutex_t g_mutex;
// The global data protected by the mutex
static int report_modes[4];
static _HFILE report_files[3];
//--------
static bool asserts_disabled = false;


int init_private_tls_data()
{
    DWORD key = TlsAlloc();

    if (key == TLS_OUT_OF_INDEXES)
        return -1;

    port_tls_key = key;
    return 0;
}

int free_private_tls_data()
{
    return TlsFree(port_tls_key) ? 0 : -1;
}


static void c_handler(Registers* pregs,
                        void* fault_addr, size_t code, size_t flags)
{ // this exception handler is executed *after* VEH handler returned
    int result;
    Registers regs = *pregs;
    Boolean iscrash = (DWORD)flags == EXCEPTION_NONCONTINUABLE;

    switch ((DWORD)code)
    {
    case STATUS_STACK_OVERFLOW:
        result = port_process_signal(PORT_SIGNAL_STACK_OVERFLOW, pregs, fault_addr, iscrash);
        break;
    case STATUS_ACCESS_VIOLATION:
        result = port_process_signal(PORT_SIGNAL_GPF, pregs, fault_addr, iscrash);
        break;
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_OVERFLOW:
        result = port_process_signal(PORT_SIGNAL_ARITHMETIC, pregs, fault_addr, iscrash);
        break;
    case JVMTI_EXCEPTION_STATUS:
        result = port_process_signal(PORT_SIGNAL_BREAKPOINT, pregs, fault_addr, iscrash);
        break;
    default:
        result = port_process_signal(PORT_SIGNAL_UNKNOWN, pregs, fault_addr, TRUE);
    }

    if (result == 0)
        return;

    if (result > 0 || // Assert dialog
        (port_crash_handler_get_flags() & PORT_CRASH_DUMP_PROCESS_CORE) != 0)
    {
        // Prepare second catch of this exception to produce minidump (because
        // we've lost LPEXCEPTION_POINTERS structure) and/or show assert dialog
        // FIXME: This will not work for stack overflow, because guard page
        // is disabled automatically - need to restore it somehow
        port_tls_data* tlsdata = get_private_tls_data();
        if (!tlsdata)
        {   // STD_MALLOC can be harmful here
            tlsdata = (port_tls_data*)STD_MALLOC(sizeof(port_tls_data));
            memset(tlsdata, 0, sizeof(port_tls_data));
            set_private_tls_data(tlsdata);
        }

        if ((port_crash_handler_get_flags() & PORT_CRASH_DUMP_PROCESS_CORE) != 0)
            tlsdata->produce_core = TRUE;
        if (result > 0)
            tlsdata->debugger = TRUE;

        return; // To produce exception again
    }

    _exit(-1);
}

void prepare_assert_dialog(Registers* regs)
{
    shutdown_signals();
}

LONG NTAPI vectored_exception_handler_internal(LPEXCEPTION_POINTERS nt_exception)
{
    Registers regs;
    // Convert NT context to Registers
    port_thread_context_to_regs(&regs, nt_exception->ContextRecord);

    // Check if TLS structure is set - probably we should produce minidump
    port_tls_data* tlsdata = get_private_tls_data();

    if (tlsdata)
    {
        if (tlsdata->produce_core)
        {
            tlsdata->produce_core = FALSE;
            create_minidump(nt_exception);
            if (!tlsdata->debugger)
                _exit(-1);
        }

        if (tlsdata->debugger)
        {
            // Go to handler to restore CRT/VEH settings and crash once again
            port_set_longjump_regs(&prepare_assert_dialog, &regs, 1, &regs);
            port_thread_regs_to_context(nt_exception->ContextRecord, &regs);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    switch (nt_exception->ExceptionRecord->ExceptionCode)
    {
    case STATUS_STACK_OVERFLOW:
        if (!sd_is_handler_registered(PORT_SIGNAL_STACK_OVERFLOW))
            return EXCEPTION_CONTINUE_SEARCH;
        break;
    case STATUS_ACCESS_VIOLATION:
        if (!sd_is_handler_registered(PORT_SIGNAL_GPF))
            return EXCEPTION_CONTINUE_SEARCH;
        break;
    case JVMTI_EXCEPTION_STATUS:
        if (!sd_is_handler_registered(PORT_SIGNAL_BREAKPOINT))
            return EXCEPTION_CONTINUE_SEARCH;
        break;
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_OVERFLOW:
        if (!sd_is_handler_registered(PORT_SIGNAL_ARITHMETIC))
            return EXCEPTION_CONTINUE_SEARCH;
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Prepare to transfering control out of VEH handler
    port_set_longjump_regs(&c_handler, &regs, 4, &regs,
        nt_exception->ExceptionRecord->ExceptionAddress,
        (void*)(size_t)nt_exception->ExceptionRecord->ExceptionCode,
        (void*)(size_t)nt_exception->ExceptionRecord->ExceptionFlags);
    // Convert prepared Registers back to NT context
    port_thread_regs_to_context(nt_exception->ContextRecord, &regs);
    // Return from VEH - presumably continue execution
    return EXCEPTION_CONTINUE_EXECUTION;
}

BOOL ctrl_handler(DWORD ctrlType)
{
    int result = 0;

    switch (ctrlType)
    {
    case CTRL_BREAK_EVENT:
        if (!sd_is_handler_registered(PORT_SIGNAL_CTRL_BREAK))
            return FALSE;

        result = port_process_signal(PORT_SIGNAL_CTRL_BREAK, NULL, NULL, FALSE);
        if (result == 0)
            return TRUE;
        else
            return FALSE;

    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (!sd_is_handler_registered(PORT_SIGNAL_CTRL_C))
            return FALSE;

        result = port_process_signal(PORT_SIGNAL_CTRL_C, NULL, NULL, FALSE);
        if (result == 0)
            return TRUE;
        else
            return FALSE;
    }

    return FALSE;
}


static void disable_assert_dialogs()
{
#ifdef _DEBUG
    report_modes[0] = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    report_files[0] = _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    report_modes[1] = _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
    report_files[1] = _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
    report_modes[2] = _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
    report_files[2] = _CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
    report_modes[3] = _set_error_mode(_OUT_TO_STDERR);
#endif // _DEBUG
}

static void restore_assert_dialogs()
{
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, report_modes[0]);
    _CrtSetReportFile(_CRT_ASSERT, report_files[0]);
    _CrtSetReportMode(_CRT_ERROR,  report_modes[1]);
    _CrtSetReportFile(_CRT_ERROR,  report_files[1]);
    _CrtSetReportMode(_CRT_WARN,   report_modes[2]);
    _CrtSetReportFile(_CRT_WARN,   report_files[2]);
    _set_error_mode(report_modes[3]);
#endif // _DEBUG
}

static void show_debugger_dialog()
{
    int result = MessageBox(NULL,
                    "ABORT handler has requested to call the debugger\n\n"
                    "Press Retry to attach to the debugger\n"
                    "Press Cancel to terminate the application",
                    "Crash Handler",
                    MB_RETRYCANCEL | MB_ICONHAND | MB_SETFOREGROUND | MB_TASKMODAL);

    if (result == IDCANCEL)
    {
        _exit(3);
        return;
    }

    port_win_dbg_break(); // Call the debugger
}

static void __cdecl sigabrt_handler(int signum)
{
    int result = port_process_signal(PORT_SIGNAL_ABORT, NULL, NULL, FALSE);
    // There no reason for checking for 0 - abort() will do _exit(3) anyway
//    if (result == 0)
//        return;

    shutdown_signals(); // Remove handlers

    if (result > 0) // Assert dialog
        show_debugger_dialog();

    _exit(3);
}

static void __cdecl final_sigabrt_handler(int signum)
{
    _exit(3);
}

void sig_process_crash_flags_change(unsigned added, unsigned removed)
{
    apr_status_t aprarr = port_mutex_lock(&g_mutex);
    if (aprarr != APR_SUCCESS)
        return;

    if ((added & PORT_CRASH_CALL_DEBUGGER) != 0 && asserts_disabled)
    {
        restore_assert_dialogs();
        asserts_disabled = false;
        signal(SIGABRT, (sigh_t)final_sigabrt_handler);
    }

    if ((removed & PORT_CRASH_CALL_DEBUGGER) != 0 && !asserts_disabled)
    {
        disable_assert_dialogs();
        asserts_disabled = true;
        signal(SIGABRT, (sigh_t)sigabrt_handler);
    }

    port_mutex_unlock(&g_mutex);
}

int initialize_signals()
{
    apr_status_t aprerr = port_mutex_create(&g_mutex, APR_THREAD_MUTEX_NESTED);

    if (aprerr != APR_SUCCESS)
        return -1;

    BOOL ok = SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, TRUE);

    if (!ok)
        return -1;

    // Adding vectored exception handler
    veh = AddVectoredExceptionHandler(0, vectored_exception_handler);

    if (!veh)
        return -1;

    prev_sig = signal(SIGABRT, (sigh_t)sigabrt_handler);

    if (prev_sig == SIG_ERR)
        return -1;

    disable_assert_dialogs();
    asserts_disabled = true;

    return 0;
}

int shutdown_signals()
{
    if (asserts_disabled)
    {
        restore_assert_dialogs();
        asserts_disabled = false;
    }

    signal(SIGABRT, prev_sig);

    ULONG res = RemoveVectoredExceptionHandler(veh);

    if (!res)
        return -1;

    BOOL ok = SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, FALSE);

    if (!ok)
        return -1;

    port_mutex_destroy(&g_mutex);
    return 0;
} //shutdown_signals
