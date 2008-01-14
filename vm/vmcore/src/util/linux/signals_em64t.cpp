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


#include <sys/ucontext.h>
#include <stdio.h>
#include "vm_core_types.h"
#include "signals_common.h"


void ucontext_to_regs(Registers* regs, ucontext_t *uc)
{
    regs->rax = uc->uc_mcontext.gregs[REG_RAX];
    regs->rcx = uc->uc_mcontext.gregs[REG_RCX];
    regs->rdx = uc->uc_mcontext.gregs[REG_RDX];
    regs->rdi = uc->uc_mcontext.gregs[REG_RDI];
    regs->rsi = uc->uc_mcontext.gregs[REG_RSI];
    regs->rbx = uc->uc_mcontext.gregs[REG_RBX];
    regs->rbp = uc->uc_mcontext.gregs[REG_RBP];
    regs->rip = uc->uc_mcontext.gregs[REG_RIP];
    regs->rsp = uc->uc_mcontext.gregs[REG_RSP];
    regs->r8  = uc->uc_mcontext.gregs[REG_R8];
    regs->r9  = uc->uc_mcontext.gregs[REG_R9];
    regs->r10 = uc->uc_mcontext.gregs[REG_R10];
    regs->r11 = uc->uc_mcontext.gregs[REG_R11];
    regs->r12 = uc->uc_mcontext.gregs[REG_R12];
    regs->r13 = uc->uc_mcontext.gregs[REG_R13];
    regs->r14 = uc->uc_mcontext.gregs[REG_R14];
    regs->r15 = uc->uc_mcontext.gregs[REG_R15];
    regs->eflags = uc->uc_mcontext.gregs[REG_EFL];
}

void regs_to_ucontext(ucontext_t *uc, Registers* regs)
{
    uc->uc_mcontext.gregs[REG_RAX] = regs->rax;
    uc->uc_mcontext.gregs[REG_RCX] = regs->rcx;
    uc->uc_mcontext.gregs[REG_RDX] = regs->rdx;
    uc->uc_mcontext.gregs[REG_RDI] = regs->rdi;
    uc->uc_mcontext.gregs[REG_RSI] = regs->rsi;
    uc->uc_mcontext.gregs[REG_RBX] = regs->rbx;
    uc->uc_mcontext.gregs[REG_RBP] = regs->rbp;
    uc->uc_mcontext.gregs[REG_RIP] = regs->rip;
    uc->uc_mcontext.gregs[REG_RSP] = regs->rsp;
    uc->uc_mcontext.gregs[REG_R8]  = regs->r8;
    uc->uc_mcontext.gregs[REG_R9]  = regs->r9;
    uc->uc_mcontext.gregs[REG_R10] = regs->r10;
    uc->uc_mcontext.gregs[REG_R11] = regs->r11;
    uc->uc_mcontext.gregs[REG_R12] = regs->r12;
    uc->uc_mcontext.gregs[REG_R13] = regs->r13;
    uc->uc_mcontext.gregs[REG_R14] = regs->r14;
    uc->uc_mcontext.gregs[REG_R15] = regs->r15;
    uc->uc_mcontext.gregs[REG_EFL] = regs->eflags;
}

// Max. 6 arguments can be set up
void regs_push_param(Registers* pregs, POINTER_SIZE_INT param, int num)
{
    switch (num)
    {
    case 0:
        pregs->rdi = param;
        return;
    case 1:
        pregs->rsi = param;
        return;
    case 2:
        pregs->rdx = param;
        return;
    case 3:
        pregs->rcx = param;
        return;
    case 4:
        pregs->r8 = param;
        return;
    case 5:
        pregs->r9 = param;
        return;
    }
}

void print_state(Registers* regs)
{
    fprintf(stderr, "Registers:\n");
    fprintf(stderr, "    RAX: 0x%016llx, RBX: 0x%016llx\n",
        regs->rax, regs->rbx);
    fprintf(stderr, "    RCX: 0x%016llx, RDX: 0x%016llx\n",
        regs->rcx, regs->rdx);
    fprintf(stderr, "    RSI: 0x%016llx, RDI: 0x%016llx\n",
        regs->rsi, regs->rdi);
    fprintf(stderr, "    RSP: 0x%016llx, RBP: 0x%016llx\n",
        regs->rsp, regs->rbp);
    fprintf(stderr, "    R8 : 0x%016llx, R9 : 0x%016llx\n",
        regs->r8,  regs->r9);
    fprintf(stderr, "    R10: 0x%016llx, R11: 0x%016llx\n",
        regs->r10, regs->r11);
    fprintf(stderr, "    R12: 0x%016llx, R13: 0x%016llx\n",
        regs->r12, regs->r13);
    fprintf(stderr, "    R14: 0x%016llx, R15: 0x%016llx\n",
        regs->r14, regs->r15);
    fprintf(stderr, "    RIP: 0x%016llx\n", regs->rip);
}
