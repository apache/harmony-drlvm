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

#include <stdio.h>
#include "platform_lowlevel.h"
#include "vm_core_types.h"
#include "exceptions_jit.h"
#include "exception_filter.h"


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

void __declspec(naked) asm_c_exception_handler(Class *exn_class, bool in_java)
{
    __asm {
    push    ebp
    mov     ebp,esp
    pushfd
    cld
    mov     eax, [ebp + 12]
    push    eax
    mov     eax, [ebp + 8]
    push    eax
    call    c_exception_handler
    add     esp, 8
    popfd
    pop     ebp
    ret
    }
}

void* regs_get_sp(Registers* pregs)
{
    return (void*)pregs->esp;
}

void regs_push_param(Registers* pregs, POINTER_SIZE_INT param, int UNREF num)
{
    pregs->esp = pregs->esp - 4;
    *((uint32*)pregs->esp) = param;
}

void regs_push_return_address(Registers* pregs, void* ret_addr)
{
    pregs->esp = pregs->esp - 4;
    *((void**)pregs->esp) = ret_addr;
}
