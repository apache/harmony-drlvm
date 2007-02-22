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
 * @version $Revision: 1.1.2.1.4.5 $
 */  

#undef LOG_DOMAIN
#define LOG_DOMAIN "nt_exception_filter"

#include "platform_lowlevel.h"
#include "Class.h"
#include "Environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "method_lookup.h"
#include "vm_strings.h"
#include "vm_threads.h"
#include "compile.h"
#include "ini.h"
#include "cxxlog.h"

#include "exception_filter.h"

#include "thread_generic.h"



// Afremov Pavel 20050117
#include "../m2n_em64t_internal.h"

void nt_to_vm_context(PCONTEXT pcontext, Registers* regs)
{
    regs->rax = pcontext->Rax;
    regs->rcx = pcontext->Rcx;
    regs->rdx = pcontext->Rdx;
    regs->rdi = pcontext->Rdi;
    regs->rsi = pcontext->Rsi;
    regs->rbx = pcontext->Rbx;
    regs->rbp = pcontext->Rbp;
    regs->rip = pcontext->Rip;
    regs->rsp = pcontext->Rsp;
}

void vm_to_nt_context(Registers* regs, PCONTEXT pcontext)
{
    pcontext->Rsp = regs->rsp;
    pcontext->Rip = regs->rip;
    pcontext->Rbp = regs->rbp;
    pcontext->Rbx = regs->rbx;
    pcontext->Rsi = regs->rsi;
    pcontext->Rdi = regs->rdi;
    pcontext->Rax = regs->rax;
    pcontext->Rcx = regs->rcx;
    pcontext->Rdx = regs->rdx;
}

int NT_exception_filter(LPEXCEPTION_POINTERS p_NT_exception) 
{

    // this filter catches _all_ null ptr exceptions including those caused by
    // VM internal code.  To elimate confusion over what caused the null ptr
    // exception, we first make sure the exception was thrown inside a Java
    // method else assert(0); <--- means it was thrown by VM C/C++ code.

    Global_Env *env = VM_Global_State::loader_env;

    VM_Code_Type vmct =
        vm_identify_eip((void *)p_NT_exception->ContextRecord->Rip);
    if(vmct != VM_TYPE_JAVA) {
        if (!get_boolean_property("vm.assert_dialog", TRUE, VM_PROPERTIES)) {
            LWARN(43, "Fatal exception, terminating");
            return EXCEPTION_EXECUTE_HANDLER;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // since we are now sure NPE occured in java code, gc should also have been disabled
    assert(!hythread_is_suspend_enabled());

    
    volatile ManagedObject *exc = 0;
    Class *exc_clss = 0;
    switch(p_NT_exception->ExceptionRecord->ExceptionCode) {
    case STATUS_ACCESS_VIOLATION:
        // null pointer exception -- see ...\vc\include\winnt.h
        {
            // Lazy exception object creation
            exc_clss = env->java_lang_NullPointerException_Class;
        }
        break;

    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        // divide by zero exception  -- see ...\vc\include\winnt.h
        {
            // Lazy exception object creation
            exc_clss = env->java_lang_ArithmeticException_Class;
        }
        break;

    case STATUS_PRIVILEGED_INSTRUCTION:
        {
            LDIE(36, "Unexpected exception code");
        }
        break;

    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    Registers regs;

    nt_to_vm_context(p_NT_exception->ContextRecord, &regs);

    bool java_code = (vm_identify_eip((void *)regs.rip) == VM_TYPE_JAVA);
    exn_athrow_regs(&regs, exc_clss, java_code);

    vm_to_nt_context(&regs, p_NT_exception->ContextRecord);

    return EXCEPTION_CONTINUE_EXECUTION;
} //NT_exception_filter

int call_the_run_method3( void * p_xx ){
    LPEXCEPTION_POINTERS p_NT_exception;
    int NT_exception_filter(LPEXCEPTION_POINTERS p_NT_exception);

    // NT null pointer exception support
    __try {
        // TODO: couldn't find where call_the_run_method() body is
        //call_the_run_method(p_xx); 
        assert(0);
        return 0;
    }
    __except ( p_NT_exception = GetExceptionInformation(), 
        NT_exception_filter(p_NT_exception) ) {

        ABORT("Uncaught exception");  // get here only if NT_null_ptr_filter() screws up

        return 0;
    }  // NT null pointer exception support

}

// TODO: the functions below need an implementation
static void asm_exception_catch_callback() {
assert(0);
}

void asm_jvmti_exception_catch_callback() {
assert(0);
}

LONG NTAPI vectored_exception_handler(LPEXCEPTION_POINTERS nt_exception)
{
assert(0);
return 0;
}

void init_stack_info() {
assert(0);
}

size_t get_available_stack_size() { 
assert(0);
return 0;
}

