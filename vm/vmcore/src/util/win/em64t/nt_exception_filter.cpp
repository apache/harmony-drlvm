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


void nt_to_vm_context(PCONTEXT pcontext, Registers* regs)
{
    regs->rsp = pcontext->Rsp;
    regs->rbp = pcontext->Rbp;
    regs->rip = pcontext->Rip;

    regs->rbx = pcontext->Rbx;
    regs->r12 = pcontext->R12;
    regs->r13 = pcontext->R13;
    regs->r14 = pcontext->R14;
    regs->r15 = pcontext->R15;

    regs->rax = pcontext->Rax;
    regs->rcx = pcontext->Rcx;
    regs->rdx = pcontext->Rdx;
    regs->rsi = pcontext->Rsi;
    regs->rdi = pcontext->Rdi;
    regs->r8  = pcontext->R8;
    regs->r9  = pcontext->R9;
    regs->r10 = pcontext->R10;
    regs->r11 = pcontext->R11;

    regs->eflags = pcontext->EFlags;
}

void vm_to_nt_context(Registers* regs, PCONTEXT pcontext)
{
    pcontext->Rsp = regs->rsp;
    pcontext->Rbp = regs->rbp;
    pcontext->Rip = regs->rip;

    pcontext->Rbx = regs->rbx;
    pcontext->R12 = regs->r12;
    pcontext->R13 = regs->r13;
    pcontext->R14 = regs->r14;
    pcontext->R15 = regs->r15;

    pcontext->Rax = regs->rax;
    pcontext->Rcx = regs->rcx;
    pcontext->Rdx = regs->rdx;
    pcontext->Rsi = regs->rsi;
    pcontext->Rdi = regs->rdi;
    pcontext->R8  = regs->r8;
    pcontext->R9  = regs->r9;
    pcontext->R10 = regs->r10;
    pcontext->R11 = regs->r11;

    pcontext->EFlags = regs->eflags;
}

void* regs_get_sp(Registers* pregs)
{
    return (void*)pregs->rsp;
}

// Max. 4 arguments can be set up
void regs_push_param(Registers* pregs, POINTER_SIZE_INT param, int num)
{ // RCX, RDX, R8, R9
    switch (num)
    {
    case 0:
        pregs->rcx = param;
        return;
    case 1:
        pregs->rdx = param;
        return;
    case 2:
        pregs->r8 = param;
        return;
    case 3:
        pregs->r9 = param;
        return;
    }
}

void regs_push_return_address(Registers* pregs, void* ret_addr)
{
    pregs->rsp = pregs->rsp - 8;
    *((void**)pregs->rsp) = ret_addr;
}
