/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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

#include "cxxlog.h"
#include "method_lookup.h"
#include "Environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "interpreter_exports.h"
#include "stack_dump.h"

// Windows specific
#include <string>
#include <excpt.h>

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
    
    /*
     * have the stack parameters been initialized?
     * 
     * TODO - fix this - this probably should be elsewhere
     */

    if(!p_TLS_vmthread->stack_addr) {
        init_stack_info();
    }
  
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
        exn_raise_by_name("java/lang/StackOverflowError");
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

LONG NTAPI vectored_exception_handler(LPEXCEPTION_POINTERS nt_exception)
{
    DWORD code = nt_exception->ExceptionRecord->ExceptionCode;
    
    bool run_default_handler = true;
    PCONTEXT context = nt_exception->ContextRecord;

    if (VM_Global_State::loader_env->shutting_down == 0) {

        TRACE2("signals", "VEH received an exception: code = 0x" <<
                ((void*)nt_exception->ExceptionRecord->ExceptionCode) <<
                " location IP = 0x" << ((void*)context->Eip));

        // this filter catches _all_ hardware exceptions including those caused by
        // VM internal code.  To elimate confusion over what caused the
        // exception, we first make sure the exception was thrown inside a Java
        // method else crash handler or default handler is executed, this means that
        // it was thrown by VM C/C++ code.
        if (((code == STATUS_ACCESS_VIOLATION ||
                code == STATUS_INTEGER_DIVIDE_BY_ZERO ||
                code == STATUS_STACK_OVERFLOW) &&
                vm_identify_eip((void *)context->Eip) == VM_TYPE_JAVA) ||
            code == STATUS_BREAKPOINT)
        {
            run_default_handler = false;
        } else if (code == STATUS_STACK_OVERFLOW) {
            if (is_unwindable()) {
                if (hythread_is_suspend_enabled()) {
                    tmn_suspend_disable();
                }
                run_default_handler = false;
            } else {
                exn_raise_by_name("java/lang/StackOverflowError");
                p_TLS_vmthread->restore_guard_page = true;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }

    } else {
        if (VM_Global_State::loader_env->shutting_down > 1) {
            // Deadly errors in shutdown.
            fprintf(stderr, "SEH handler: too many shutdown errors");
            return EXCEPTION_CONTINUE_SEARCH;
        } else {
            fprintf(stderr, "SEH handler: shutdown error");
        }
    }

    if (run_default_handler) {
        const char *msg = 0;
        switch (code) {
            // list of errors we can handle:
            case STATUS_ACCESS_VIOLATION:         msg = "ACCESS_VIOLATION"; break;
            case STATUS_INTEGER_DIVIDE_BY_ZERO:   msg = "INTEGER_DIVIDE_BY_ZERO"; break;
            case STATUS_PRIVILEGED_INSTRUCTION:   msg = "PRIVILEGED_INSTRUCTION"; break;
            case STATUS_SINGLE_STEP:              msg = "SINGLE_STEP"; break;
            case STATUS_BREAKPOINT:               msg = "BREAKPOINT"; break;
            case STATUS_ILLEGAL_INSTRUCTION:      msg = "ILLEGAL_INSTRUCTION"; break;
            case STATUS_GUARD_PAGE_VIOLATION:     msg = "GUARD_PAGE_VIOLATION"; break;
            case STATUS_INVALID_HANDLE:           msg = "INVALID_HANDLE"; break;
            case STATUS_DATATYPE_MISALIGNMENT:    msg = "DATATYPE_MISALIGNMENT"; break;
            case STATUS_FLOAT_INVALID_OPERATION:  msg = "FLOAT_INVALID_OPERATION"; break;
            case STATUS_NONCONTINUABLE_EXCEPTION: msg = "NONCONTINUABLE_EXCEPTION"; break;
            case STATUS_STACK_OVERFLOW:           msg = "STACK_OVERFLOW"; break;
            case STATUS_CONTROL_C_EXIT:           msg = "CONTROL_C_EXIT"; break;
            case STATUS_ARRAY_BOUNDS_EXCEEDED:    msg = "ARRAY_BOUNDS_EXCEEDED"; break;
            case STATUS_FLOAT_DENORMAL_OPERAND:   msg = "FLOAT_DENORMAL_OPERAND"; break;
            case STATUS_FLOAT_INEXACT_RESULT:     msg = "FLOAT_INEXACT_RESULT"; break;
            case STATUS_FLOAT_OVERFLOW:           msg = "FLOAT_OVERFLOW"; break;
            case STATUS_FLOAT_STACK_CHECK:        msg = "FLOAT_STACK_CHECK"; break;
            case STATUS_FLOAT_UNDERFLOW:          msg = "FLOAT_UNDERFLOW"; break;
            case STATUS_INTEGER_OVERFLOW:         msg = "INTEGER_OVERFLOW"; break;
            case STATUS_IN_PAGE_ERROR:            msg = "IN_PAGE_ERROR"; break;
            case STATUS_INVALID_DISPOSITION:      msg = "INVALID_DISPOSITION"; break;

            default:
                return EXCEPTION_CONTINUE_SEARCH;
        }



        VM_Global_State::loader_env->shutting_down++;

        if (!vm_get_boolean_property_value_with_default("vm.assert_dialog")) {
            print_state(nt_exception, msg);

            print_callstack(nt_exception);
            LOGGER_EXIT(-1);

        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // since we are now sure HWE occured in java code, gc should also have been disabled

    // gregory - this is not true since for debugging we may use int3
    // in VM code which produces BREAKPOINT exception. JVMTI has
    // assertions for breakpoints which it has set in Java inside of
    // breakpoint handling function. Otherwise this assert should not
    // fail in case _CrtDbgBreak() was added somewhere in VM.
    assert(!hythread_is_suspend_enabled() || code == STATUS_BREAKPOINT);
    
    Global_Env *env = VM_Global_State::loader_env;
    Class *exc_clss = 0;

    switch(nt_exception->ExceptionRecord->ExceptionCode) 
    {
    case STATUS_STACK_OVERFLOW:
        // stack overflow exception -- see ...\vc\include\winnt.h
        {
            TRACE2("signals", "StackOverflowError detected at "
                << (void *) context->Eip << " on the stack at "
                << (void *) context->Esp);
            // Lazy exception object creation
            exc_clss = env->java_lang_StackOverflowError_Class;
            p_TLS_vmthread->restore_guard_page = true;
        }
        break;
    case STATUS_ACCESS_VIOLATION:
        // null pointer exception -- see ...\vc\include\winnt.h
        {
            TRACE2("signals", "NullPointerException detected at " 
                << (void *) context->Eip);
            // Lazy exception object creation
            exc_clss = env->java_lang_NullPointerException_Class;
        }
        break;

    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        // divide by zero exception  -- see ...\vc\include\winnt.h
        {
            TRACE2("signals", "ArithmeticException detected at "
                << (void *) context->Eip);
            // Lazy exception object creation
            exc_clss = env->java_lang_ArithmeticException_Class;
        }
        break;
    case STATUS_BREAKPOINT:
        // JVMTI breakpoint in JITted code
        {
            Registers regs;
            nt_to_vm_context(context, &regs);
            TRACE2("signals", "JVMTI breakpoint detected at " <<
                (void *)regs.eip);
            bool handled = jvmti_jit_breakpoint_handler(&regs);
            if (handled)
            {
                vm_to_nt_context(&regs, context);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            else
                return EXCEPTION_CONTINUE_SEARCH;
        }
    default: assert(false);
    }

    Registers regs;

    nt_to_vm_context(context, &regs);

    uint32 exception_esp = regs.esp;
    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;

    exn_athrow_regs(&regs, exc_clss);

    assert(exception_esp <= regs.esp);
    if (ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT)) {
        regs.esp = regs.esp - 4;
        *((uint32*) regs.esp) = regs.eip;
        regs.eip = ((uint32)asm_jvmti_exception_catch_callback);
    } else if (p_TLS_vmthread->restore_guard_page) {
        regs.esp = regs.esp - 4;
        *((uint32*) regs.esp) = regs.eip;
        regs.eip = ((uint32)asm_exception_catch_callback);
    }

    vm_to_nt_context(&regs, context);

    return EXCEPTION_CONTINUE_EXECUTION;
} //vectored_exception_handler
