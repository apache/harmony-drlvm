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
#include "open/thread.h"
#include "Environment.h"
#include "exceptions.h"

// Windows specific
#include <string>
#include <excpt.h>
#include <dbghelp.h>
#include <windows.h> 
#pragma comment(linker, "/defaultlib:dbghelp.lib")

static inline void nt_to_vm_context(PCONTEXT context, Registers* regs)
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

static inline void vm_to_nt_context(Registers* regs, PCONTEXT context)
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

// CallStack print
#define CALLSTACK_DEPTH_LIMIT 100 // max stack length is 100 to prevent getting into loop

static void print_callstack(LPEXCEPTION_POINTERS nt_exception)
{
    PCONTEXT context = nt_exception->ContextRecord;
    STACKFRAME StackFrm;
    // initialize STACKFRAME 
    memset(&StackFrm, 0, sizeof(STACKFRAME));
    StackFrm.AddrPC.Offset       = context->Eip;
    StackFrm.AddrPC.Mode         = AddrModeFlat;
    StackFrm.AddrStack.Offset    = context->Esp;
    StackFrm.AddrStack.Mode      = AddrModeFlat;
    StackFrm.AddrFrame.Offset    = context->Ebp;
    StackFrm.AddrFrame.Mode      = AddrModeFlat;

    // init sym handler for GetCurrentProcess() process
    if (!SymInitialize(GetCurrentProcess(), NULL, true))
    {
        fprintf(stderr, "CallStack is inaccessible due to dbghelp library initialization failed.\n");
        return;
    }
    fprintf(stderr, "CallStack:\n");

    // try to go throuh the stack
    for (int counter = 0; counter < CALLSTACK_DEPTH_LIMIT; counter++)
    {
        if (!StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(),  &StackFrm,  
                                context, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL)) {
            // no STACKFRAME found any more
            break;
        }

        // try to get function name
        BYTE smBuf[sizeof(SYMBOL_INFO) + 2048];
        PSYMBOL_INFO pSymb = (PSYMBOL_INFO)smBuf;
        pSymb->SizeOfStruct = sizeof(smBuf);
        pSymb->MaxNameLen = 2048;

        DWORD64 funcDispl;
        if (SymFromAddr(GetCurrentProcess(), StackFrm.AddrPC.Offset, &funcDispl, pSymb)) {
            fprintf(stderr, "    %s ", pSymb->Name);
        }
        else {
            fprintf(stderr, "    No name ");
        }

        // try to get file name and line number
        DWORD lineOffset;
        IMAGEHLP_LINE lineInfo;
        if (SymGetLineFromAddr(GetCurrentProcess(), StackFrm.AddrPC.Offset, &lineOffset, &lineInfo))
        {
            fprintf(stderr, " (File: %s Line: %d )\n", lineInfo.FileName, lineInfo.LineNumber);
        }
        else
        {
            // skip functions w/o file name and line number
            fprintf(stderr, " (File: ?? Line: ?? )");
        }
    }

    fflush(stderr);
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
        if ((code == STATUS_ACCESS_VIOLATION
                || code == STATUS_INTEGER_DIVIDE_BY_ZERO)
                && vm_identify_eip((void *)context->Eip) == VM_TYPE_JAVA)
            run_default_handler = false;

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
    assert(!tmn_is_suspend_enabled());
    
    Global_Env *env = VM_Global_State::loader_env;
    Class *exc_clss = 0;

    switch(nt_exception->ExceptionRecord->ExceptionCode) 
    {
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
    default: assert(false);
    }

    Registers regs;
    nt_to_vm_context(context, &regs);

    exn_athrow_regs(&regs, exc_clss);

    vm_to_nt_context(&regs, context);

    return EXCEPTION_CONTINUE_EXECUTION;
} //vectored_exception_handler
