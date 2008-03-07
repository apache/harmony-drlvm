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


#include "clog.h"
#include "Environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "interpreter_exports.h"
#include "stack_iterator.h"
#include "stack_dump.h"
#include "jvmti_break_intf.h"
#include "m2n.h"
#include "open/hythread_ext.h"
#include "port_thread.h"
#include "compile.h"

// Windows specific
#include <string>
#include <excpt.h>
#ifndef NO_DBGHELP
#include <dbghelp.h>
#pragma comment(linker, "/defaultlib:dbghelp.lib")
#endif

#include "exception_filter.h"


#if INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
#define JVMTI_EXCEPTION_STATUS STATUS_BREAKPOINT
#elif INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_HLT || INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_CLI
#define JVMTI_EXCEPTION_STATUS STATUS_PRIVILEGED_INSTRUCTION
#else
#error Unknown value of INSTRUMENTATION_BYTE
#endif


#ifndef NO_DBGHELP
typedef BOOL (WINAPI *MiniDumpWriteDump_type)
   (HANDLE          hProcess,
    DWORD           ProcessId,
    HANDLE          hFile,
    MINIDUMP_TYPE   DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION   ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION    CallbackParam);
#endif // #ifndef NO_DBGHELP


static void create_minidump(LPEXCEPTION_POINTERS exp)
{
#ifndef NO_DBGHELP
    MINIDUMP_EXCEPTION_INFORMATION mei = {GetCurrentThreadId(), exp, TRUE};
    MiniDumpWriteDump_type mdwd = NULL;

    HMODULE hdbghelp = ::LoadLibrary("dbghelp");

    if (hdbghelp)
        mdwd = (MiniDumpWriteDump_type)::GetProcAddress(hdbghelp, "MiniDumpWriteDump");

    if (!mdwd)
    {
        fprintf(stderr, "Failed to open DbgHelp library");
        return;
    }

    char filename[24];
    sprintf(filename, "minidump_%d.dmp", GetCurrentProcessId());

    HANDLE file = CreateFile(filename, GENERIC_WRITE, 0, 0,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (file == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to create minidump file: %s", filename);
        return;
    }

    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    BOOL res = mdwd(GetCurrentProcess(), GetCurrentProcessId(),
                                file, MiniDumpNormal, &mei, 0, 0);

    if (!res)
    {
        fprintf(stderr, "Failed to create minidump");
    }
    else
    {
        char dir[_MAX_PATH];
        GetCurrentDirectory(_MAX_PATH, dir);
        fprintf(stderr, "Minidump is generated:\n%s\\%s", dir, filename);
    }

    CloseHandle(file);
#endif // #ifndef NO_DBGHELP
}


static LONG process_crash(Registers* regs, LPEXCEPTION_POINTERS exp, DWORD ExceptionCode)
{
static DWORD saved_eip_index = TlsAlloc();
static BOOL UNREF tmp_init = TlsSetValue(saved_eip_index, (LPVOID)0);

    // Check crash location to prevent infinite recursion
    if (regs->get_ip() == (void*)TlsGetValue(saved_eip_index))
        return EXCEPTION_CONTINUE_SEARCH;
    // Store registers to compare IP in future
    TlsSetValue(saved_eip_index, (LPVOID)regs->get_ip());

    switch (ExceptionCode)
    {
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
        break;

    case EXCEPTION_STACK_OVERFLOW:
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // We can't obtain a value of property if loader_env is NULL
    if (VM_Global_State::loader_env == NULL ||
        get_boolean_property("vm.assert_dialog", TRUE, VM_PROPERTIES))
        return EXCEPTION_CONTINUE_SEARCH;

    // Report crash
    fprintf(stderr, "Windows reported exception: 0x%x\n", ExceptionCode);

    sd_print_stack(regs);
    create_minidump(exp);
    LOGGER_EXIT(-1);
    return EXCEPTION_CONTINUE_EXECUTION;
}


/*
 * Information about stack
 */
inline void* find_stack_addr() {
    void* stack_addr;
    size_t reg_size;
    MEMORY_BASIC_INFORMATION memory_information;

    VirtualQuery(&memory_information, &memory_information, sizeof(memory_information));
    reg_size = memory_information.RegionSize;
    stack_addr =((char*) memory_information.BaseAddress) + reg_size;

    return stack_addr;
}

inline size_t find_guard_page_size() {
    size_t  guard_size;
    SYSTEM_INFO system_info;

    GetSystemInfo(&system_info);
    guard_size = system_info.dwPageSize;

    return guard_size;
}

inline size_t find_guard_stack_size() {
#   ifdef _EM64T_
       //this code in future should be used on both platforms x86-32 and x86-64
        return 64*1024;
#   else
        // guaerded stack size on windows 32 can be equals one page size only :(
        return find_guard_page_size();
#   endif
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


    //this code in future should be used on both platforms x86-32 and x86-64
#   ifdef _EM64T_
    ULONG guard_stack_size_param = (ULONG)common_guard_stack_size;

    if (!SetThreadStackGuarantee(&guard_stack_size_param)) {
        // should be successful always
        assert(0);
    }
#   endif
}

size_t get_mem_protect_stack_size() {
    return 0x0100;
}

size_t get_restore_stack_size() {
    return get_mem_protect_stack_size() + 0x0100;
}

bool set_guard_stack() {
    void* stack_addr = get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t page_size = get_guard_page_size();
    size_t guard_stack_size = get_guard_stack_size();
  
    if (((size_t)(&stack_addr) - get_mem_protect_stack_size())
            < ((size_t)((char*)stack_addr - stack_size 
            + 2 * page_size + guard_stack_size))) {
        return false;
    }

    if (!VirtualFree((char*)stack_addr - stack_size + page_size,
        page_size, MEM_DECOMMIT)) {
        // should be successful always
        assert(0);
    }

    if (!VirtualAlloc( (char*)stack_addr - stack_size + page_size + guard_stack_size,
        page_size, MEM_COMMIT, PAGE_GUARD | PAGE_READWRITE)) {
        // should be successful always
        assert(0);
    }
    jthread_self_vm_thread_unsafe()->restore_guard_page = false;

    return true;
}

void remove_guard_stack() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();
    assert(vm_thread);
    remove_guard_stack(vm_thread);
}

void remove_guard_stack(vm_thread_t vm_thread) {
    void* stack_addr = get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t page_size = get_guard_page_size();
    size_t guard_stack_size = get_guard_stack_size();
    DWORD oldProtect;

    assert(((size_t)(&stack_addr)) > ((size_t)((char*)stack_addr - stack_size + 3 * page_size)));
    vm_thread->restore_guard_page = true;

    if (!VirtualProtect((char*)stack_addr - stack_size + page_size + guard_stack_size,
        page_size, PAGE_READWRITE, &oldProtect)) {
        // should be successful always
        assert(0);
    }
}

size_t get_available_stack_size() {
    char* stack_addr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_addr) - ((size_t)(&stack_addr));
    size_t available_stack_size;

    if (!p_TLS_vmthread->restore_guard_page) {
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

bool check_stack_size_enough_for_exception_catch(void* sp) {
    char* stack_adrr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_adrr) - ((size_t)sp);
    size_t available_stack_size =
            get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    return get_restore_stack_size() < available_stack_size;
}


LONG NTAPI vectored_exception_handler_internal(LPEXCEPTION_POINTERS nt_exception)
{
    DWORD code = nt_exception->ExceptionRecord->ExceptionCode;
    PCONTEXT context = nt_exception->ContextRecord;
    Registers regs;
    bool flag_replaced = false;
    VM_thread* vmthread = p_TLS_vmthread;

    // Convert NT context to Registers
    port_thread_context_to_regs(&regs, context);
    POINTER_SIZE_INT saved_eip = (POINTER_SIZE_INT)regs.get_ip();

    bool in_java = false;

    if (vmthread)
    {
        // If exception is occured in processor instruction previously
        // instrumented by breakpoint, the actual exception address will reside
        // in jvmti_jit_breakpoints_handling_buffer
        // We should replace exception address with saved address of instruction
        POINTER_SIZE_INT break_buf =
            (POINTER_SIZE_INT)vmthread->jvmti_thread.jvmti_jit_breakpoints_handling_buffer;
        if (saved_eip >= break_buf &&
            saved_eip < break_buf + TM_JVMTI_MAX_BUFFER_SIZE)
        {
            flag_replaced = true;
            regs.set_ip(vm_get_ip_from_regs(vmthread));
            port_thread_regs_to_context(context, &regs);
        }

        in_java = (vm_identify_eip(regs.get_ip()) == VM_TYPE_JAVA);
    }

    // the possible reasons for hardware exception are
    //  - segfault or division by zero in java code
    //     => NullPointerException or ArithmeticException
    //
    //  - breakpoint or privileged instruction in java code
    //    => send jvmti breakpoint event
    //
    //  - stack overflow, either in java or in native
    //    => StackOverflowError
    //
    //  - other (internal VM error or debugger breakpoint)
    //    => delegate to default handler

    // Pass exception to NCAI exception handler
    bool is_continuable =
        (nt_exception->ExceptionRecord->ExceptionFlags != EXCEPTION_NONCONTINUABLE);
    bool is_handled = !is_continuable;
    bool is_internal = (code == JVMTI_EXCEPTION_STATUS);
    ncai_process_signal_event((NativeCodePtr)regs.get_ip(),
        (jint)code, is_internal, &is_handled);
    if (is_continuable && is_handled)
        return EXCEPTION_CONTINUE_EXECUTION;

    // delegate "other" cases to crash handler
    // Crash handler shouls be invoked when VM_thread is not attached to VM
    // or exception has occured in native code and it's not STACK_OVERFLOW
    if ((!vmthread ||
        (!in_java && code != STATUS_STACK_OVERFLOW)) &&
        code != JVMTI_EXCEPTION_STATUS)
    {
        LONG result = process_crash(&regs, nt_exception, code);
        regs.set_ip((void*)saved_eip);
        port_thread_regs_to_context(context, &regs);
        return result;
    }

    TRACE2("signals", ("VEH received an exception: code = %x, ip = %p, sp = %p",
        nt_exception->ExceptionRecord->ExceptionCode, regs.get_ip(), regs.get_sp()));

    // if HWE occured in java code, suspension should also have been disabled
    bool ncai_enabled = GlobalNCAI::isEnabled();
    assert(!in_java || !hythread_is_suspend_enabled() || ncai_enabled);

    Global_Env *env = VM_Global_State::loader_env;
    // the actual exception object will be created lazily,
    // we determine only exception class here
    Class *exn_class = 0;

    switch(nt_exception->ExceptionRecord->ExceptionCode)
    {
    case STATUS_STACK_OVERFLOW:
        {
            TRACE2("signals",
                ("StackOverflowError detected at ip = %p, esp = %p",
                 regs.get_ip(), regs.get_sp()));

            vmthread->restore_guard_page = true;
            exn_class = env->java_lang_StackOverflowError_Class;
            if (in_java) {
                // stack overflow occured in java code:
                // nothing special to do
            } else if (is_unwindable()) {
                // stack overflow occured in native code that can be unwound
                // safely.
                // Throwing exception requires suspend disabled status
                if (hythread_is_suspend_enabled())
                    hythread_suspend_disable();
            } else {
                // stack overflow occured in native code that
                // cannot be unwound.
                // Mark raised exception in TLS and resume execution
                exn_raise_by_class(env->java_lang_StackOverflowError_Class);
                regs.set_ip((void*)saved_eip);
                port_thread_regs_to_context(context, &regs);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        break;
    case STATUS_ACCESS_VIOLATION:
        {
            TRACE2("signals",
                ("NullPointerException detected at ip = %p", regs.get_ip()));
            exn_class = env->java_lang_NullPointerException_Class;
        }
        break;

    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        {
            TRACE2("signals",
                ("ArithmeticException detected at ip = %p", regs.get_ip()));
            exn_class = env->java_lang_ArithmeticException_Class;
        }
        break;
    case JVMTI_EXCEPTION_STATUS:
        // JVMTI breakpoint in JITted code
        {
            // Breakpoints should not occur in breakpoint buffer
            assert(!flag_replaced);

            TRACE2("signals",
                ("JVMTI breakpoint detected at ip = %p", regs.get_ip()));
            bool handled = jvmti_jit_breakpoint_handler(&regs);
            if (handled)
            {
                port_thread_regs_to_context(context, &regs);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            else
                return EXCEPTION_CONTINUE_SEARCH;
        }
    default:
        // unexpected hardware exception occured in java code
        LONG result = process_crash(&regs, nt_exception, code);
        regs.set_ip((void*)saved_eip);
        port_thread_regs_to_context(context, &regs);
        return result;
    }

    // we must not call potentially blocking or suspendable code
    // (i.e. java code of exception constructor) from exception
    // handler, because this handler may hold a system-wide lock,
    // and this may result in a deadlock.

    // it was reported that exception handler grabs a system
    // lock on Windows XPsp2 and 2003sp0, but not on a 2003sp1

    // save register context of hardware exception site
    // into thread-local registers snapshot
    vm_set_exception_registers(vmthread, regs);

    assert(exn_class);
    // Set up parameters and registers for C handler
    port_set_longjump_regs(c_exception_handler, &regs,
                            3, &regs, exn_class, (POINTER_SIZE_INT)in_java);
    // Store changes into NT context
    port_thread_regs_to_context(context, &regs);

    // exit NT exception handler and transfer
    // control to VM exception handler
    return EXCEPTION_CONTINUE_EXECUTION;
}

void __cdecl c_exception_handler(Registers* regs, Class* exn_class, bool in_java)
{
    // this exception handler is executed *after* NT exception handler returned
    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    // Create local copy for registers because registers in TLS can be changed
    VM_thread *thread = p_TLS_vmthread;
    assert(thread);
    assert(exn_class);

    TRACE2("signals", ("should throw exception %p at IP=%p, SP=%p",
                exn_class, regs->get_ip(), regs->get_sp()));
    exn_athrow_regs(regs, exn_class, in_java, true);

    assert(!"si_transfer_control should not return");
}
