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

#include "clog.h"
#include "method_lookup.h"
#include "Environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "interpreter_exports.h"
#include "stack_iterator.h"
#include "stack_dump.h"
#include "jvmti_break_intf.h"
#include "m2n.h"

// Windows specific
#include <string>
#include <excpt.h>

#if INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
#define JVMTI_EXCEPTION_STATUS STATUS_BREAKPOINT
#elif INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_HLT || INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_CLI
#define JVMTI_EXCEPTION_STATUS STATUS_PRIVILEGED_INSTRUCTION
#else
#error Unknown value of INSTRUMENTATION_BYTE
#endif

void nt_to_vm_context(PCONTEXT context, Registers* regs)
{
    regs->eax = context->Eax;
    regs->ecx = context->Ecx;
    regs->edx = context->Edx;
    regs->edi = context->Edi;
    regs->esi = context->Esi;
    regs->ebx = context->Ebx;
    regs->ebp = context->Ebp;
    regs->eip = context->Eip;
    regs->esp = context->Esp;
    regs->eflags = context->EFlags;
}

void vm_to_nt_context(Registers* regs, PCONTEXT context)
{
    context->Esp = regs->esp;
    context->Eip = regs->eip;
    context->Ebp = regs->ebp;
    context->Ebx = regs->ebx;
    context->Esi = regs->esi;
    context->Edi = regs->edi;
    context->Eax = regs->eax;
    context->Ecx = regs->ecx;
    context->Edx = regs->edx;
    context->EFlags = regs->eflags;
}

static void print_state(LPEXCEPTION_POINTERS nt_exception, const char *msg)
{
    fprintf(stderr, "...VM Crashed!\n");
    if (msg != 0) 
    {
        fprintf(stderr, "Windows reported exception: %s\n", msg);
    }
    else 
    {
        fprintf(stderr, "Windows reported exception: 0x%x\n", nt_exception->ExceptionRecord->ExceptionCode);
    }

    fprintf(stderr, "Registers:\n");
    fprintf(stderr, "    EAX: 0x%08x, EBX: 0x%08x, ECX: 0x%08x, EDX=0x%08x\n",
            nt_exception->ContextRecord->Eax,
            nt_exception->ContextRecord->Ebx,
            nt_exception->ContextRecord->Ecx,
            nt_exception->ContextRecord->Edx);
    fprintf(stderr, "    ESI: 0x%08x, EDI: 0x%08x, ESP: 0x%08x, EBP=0x%08x\n",
            nt_exception->ContextRecord->Esi,
            nt_exception->ContextRecord->Edi,
            nt_exception->ContextRecord->Esp,
            nt_exception->ContextRecord->Ebp);
    fprintf(stderr, "    EIP: 0x%08x\n", nt_exception->ContextRecord->Eip);
}


static void print_callstack(LPEXCEPTION_POINTERS nt_exception) {
    PCONTEXT context = nt_exception->ContextRecord;
    Registers regs;
    nt_to_vm_context(context, &regs);
    st_print_stack(&regs);
    fflush(stderr);
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

inline size_t find_stack_size() {
   void* stack_addr;
    size_t stack_size;
    size_t reg_size;
    MEMORY_BASIC_INFORMATION memory_information;

    VirtualQuery(&memory_information, &memory_information, sizeof(memory_information));
    reg_size = memory_information.RegionSize;
    stack_addr = ((char*) memory_information.BaseAddress) + reg_size;
    stack_size = ((char*) stack_addr) - ((char*) memory_information.AllocationBase);

    return stack_size;
}

inline size_t find_guard_page_size() {
    size_t  guard_size;
    SYSTEM_INFO system_info;

    GetSystemInfo(&system_info);
    guard_size = system_info.dwPageSize;

    return guard_size;
}

inline size_t find_guard_stack_size() {
    // guaerded stack size on windows can be equals one page size only :(
    return find_guard_page_size();
}

static size_t common_stack_size;
static size_t common_guard_stack_size;
static size_t common_guard_page_size;

inline void* get_stack_addr() {
    return p_TLS_vmthread->stack_addr;
}

inline size_t get_stack_size() {
    return common_stack_size;
}

inline size_t get_guard_stack_size() {
    return common_guard_stack_size;
}

inline size_t get_guard_page_size() {
    return common_guard_page_size;
}


void init_stack_info() {
    p_TLS_vmthread->stack_addr = find_stack_addr();
    common_stack_size = find_stack_size();
    common_guard_stack_size = find_guard_stack_size();
    common_guard_page_size =find_guard_page_size();
}

void set_guard_stack() {
    void* stack_addr = get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t page_size = get_guard_page_size();

    if (!VirtualFree((char*)stack_addr - stack_size + page_size,
        page_size, MEM_DECOMMIT)) {
        // should be successful always
        assert(0);
    }

    DWORD oldProtect;

    if (!VirtualProtect((char*)stack_addr - stack_size + page_size + page_size,
        page_size, PAGE_GUARD | PAGE_READWRITE, &oldProtect)) {
        // should be successful always
        assert(0);
    }

    p_TLS_vmthread->restore_guard_page = false;
}

size_t get_available_stack_size() {
    char* stack_adrr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_adrr) - ((size_t)(&stack_adrr));
    size_t available_stack_size =
            get_stack_size() - used_stack_size
            - get_guard_page_size() - get_guard_stack_size();
    return available_stack_size;
}
size_t get_default_stack_size() {
    size_t default_stack_size = get_stack_size();
    return default_stack_size;
}
bool check_available_stack_size(size_t required_size) {
    if (get_available_stack_size() < required_size) {
        Global_Env *env = VM_Global_State::loader_env;
        exn_raise_by_class(env->java_lang_StackOverflowError_Class);
        return false;
    } else {
        return true;
    }
}

// exception catch callback to restore stack after Stack Overflow Error
static void __cdecl exception_catch_callback_wrapper(){
    exception_catch_callback();
}

// exception catch support for JVMTI
static void __cdecl jvmti_exception_catch_callback_wrapper(Registers regs){
    jvmti_exception_catch_callback(&regs);
}

static void __declspec(naked) asm_exception_catch_callback() {
    __asm {
        push ebp
        mov ebp, esp
        push eax
        push ebx
        push ecx
        push edx
        call exception_catch_callback_wrapper
        pop edx
        pop ecx
        pop ebx
        pop eax
        leave
        ret
    }
}

void __declspec(naked) asm_jvmti_exception_catch_callback() {
    __asm {
        push ebp
        mov ebp, esp
        add esp, -36
        mov [ebp-36], eax
        mov [ebp-32], ebx
        mov [ebp-28], ecx
        mov [ebp-24], edx
        mov eax, esp
        mov ebx, [ebp]
        mov ecx, [ebp+4]
        add eax, 44
        mov [ebp-20], edi
        mov [ebp-16], esi
        mov [ebp-12], ebx
        mov [ebp-8], eax
        mov [ebp-4], ecx
        call jvmti_exception_catch_callback_wrapper
        mov eax, [ebp-36]
        mov ebx, [ebp-32]
        mov ecx, [ebp-28]
        mov edx, [ebp-24]
        add esp, 36
        leave
        ret
    }
}

static LONG NTAPI vectored_exception_handler_internal(LPEXCEPTION_POINTERS nt_exception);
static void __cdecl c_exception_handler(Class*, bool);

LONG __declspec(naked) NTAPI vectored_exception_handler(LPEXCEPTION_POINTERS nt_exception)
{
    __asm {
    push    ebp
    mov     ebp,esp
    pushfd
    cld
    mov     eax, [ebp + 8]
    push    eax
    call    vectored_exception_handler_internal
    popfd
    pop     ebp
    ret     4    
    }
}

static LONG NTAPI vectored_exception_handler_internal(LPEXCEPTION_POINTERS nt_exception)
{
    DWORD code = nt_exception->ExceptionRecord->ExceptionCode;
    PCONTEXT context = nt_exception->ContextRecord;
    bool flag_replaced = false;
    uint32 saved_eip = context->Eip;

    // If exception is occured in processor instruction previously
    // instrumented by breakpoint, the actual exception address will reside
    // in jvmti_jit_breakpoints_handling_buffer
    // We should replace exception address with saved address of instruction
    uint32 break_buf = (uint32)p_TLS_vmthread->jvmti_jit_breakpoints_handling_buffer;
    if (saved_eip >= break_buf &&
        saved_eip < break_buf + 50)
    {
        flag_replaced = true;
        context->Eip = (uint32)p_TLS_vmthread->jvmti_saved_exception_registers.eip;
    }

    TRACE2("signals", ("VEH received an exception: code = %x, eip = %p, esp = %p",
        nt_exception->ExceptionRecord->ExceptionCode,
        context->Eip, context->Esp));

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

    bool in_java = (vm_identify_eip((void*)context->Eip) == VM_TYPE_JAVA);

    // delegate "other" cases to default handler
    if (!in_java && code != STATUS_STACK_OVERFLOW)
    {
        context->Eip = saved_eip;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // if HWE occured in java code, suspension should also have been disabled
    assert(!in_java || !hythread_is_suspend_enabled());

    Global_Env *env = VM_Global_State::loader_env;
    // the actual exception object will be created lazily,
    // we determine only exception class here
    Class *exn_class = 0;

    switch(nt_exception->ExceptionRecord->ExceptionCode) 
    {
    case STATUS_STACK_OVERFLOW:
        {
            TRACE2("signals",
                ("StackOverflowError detected at eip = %p, esp = %p",
                 context->Eip,context->Esp));

            p_TLS_vmthread->restore_guard_page = true;
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
                context->Eip = saved_eip;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        break;
    case STATUS_ACCESS_VIOLATION:
        {
            TRACE2("signals",
                ("NullPointerException detected at eip = %p", context->Eip));
            exn_class = env->java_lang_NullPointerException_Class;
        }
        break;

    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        {
            TRACE2("signals",
                ("ArithmeticException detected at eip = %p", context->Eip));
            exn_class = env->java_lang_ArithmeticException_Class;
        }
        break;
    case JVMTI_EXCEPTION_STATUS:
        // JVMTI breakpoint in JITted code
        {
            // Breakpoints should not occur in breakpoint buffer
            assert(!flag_replaced);

            Registers regs;
            nt_to_vm_context(context, &regs);
            TRACE2("signals",
                ("JVMTI breakpoint detected at eip = %p", regs.eip));
            bool handled = jvmti_jit_breakpoint_handler(&regs);
            if (handled)
            {
                vm_to_nt_context(&regs, context);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            else
                return EXCEPTION_CONTINUE_SEARCH;
        }
    default:
        // unexpected hardware exception occured in java code
        context->Eip = saved_eip;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // we must not call potentially blocking or suspendable code
    // (i.e. java code of exception constructor) from exception
    // handler, because this handler may hold a system-wide lock,
    // and this may result in a deadlock.

    // it was reported that exception handler grabs a system
    // lock on Windows XPsp2 and 2003sp0, but not on a 2003sp1

    // save register context of hardware exception site
    // into thread-local registers snapshot
    assert(p_TLS_vmthread);
    nt_to_vm_context(context, &p_TLS_vmthread->regs);

    // __cdecl <=> push parameters in the reversed order
    // push in_java argument onto stack
    context->Esp -= 4;
    *((uint32*) context->Esp) = (uint32)in_java;
    // push the exn_class argument onto stack
    context->Esp -= 4;
    assert(exn_class);
    *((uint32*) context->Esp) = (uint32)exn_class;
    // imitate return IP on stack
    context->Esp -= 4;

    // set up the real exception handler address
    context->Eip = (uint32)c_exception_handler;

    // exit NT exception handler and transfer
    // control to VM exception handler
    return EXCEPTION_CONTINUE_EXECUTION;
}


static void __cdecl c_exception_handler(Class *exn_class, bool in_java)
{
    // this exception handler is executed *after* NT exception handler returned
    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;
    // Create local copy for registers because registers in TLS can be changed
    Registers regs = p_TLS_vmthread->regs;

    M2nFrame* prev_m2n = m2n_get_last_frame();
    M2nFrame* m2n = NULL;
    if (in_java)
        m2n = m2n_push_suspended_frame(&regs);

    TRACE2("signals", ("should throw exception %p at EIP=%p, ESP=%p",
                exn_class, regs.eip, regs.esp));
    exn_athrow_regs(&regs, exn_class, false);

    if (ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT)) {
        regs.esp = regs.esp - 4;
        *((uint32*) regs.esp) = regs.eip;
        regs.eip = ((uint32)asm_jvmti_exception_catch_callback);
    } else if (p_TLS_vmthread->restore_guard_page) {
        regs.esp = regs.esp - 4;
        *((uint32*) regs.esp) = regs.eip;
        regs.eip = ((uint32)asm_exception_catch_callback);
    }

    StackIterator *si =
        si_create_from_registers(&regs, false, prev_m2n);
    if (m2n)
        STD_FREE(m2n);
    si_transfer_control(si);
    assert(!"si_transfer_control should not return");
}
