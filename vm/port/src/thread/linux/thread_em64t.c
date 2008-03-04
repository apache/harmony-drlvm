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


#define  _GNU_SOURCE
#include <sys/ucontext.h>
#include "port_thread.h"


void port_thread_context_to_regs(Registers* regs, ucontext_t *uc)
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

void port_thread_regs_to_context(ucontext_t *uc, Registers* regs)
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


void port_longjump_stub(void);
#define DIR_FLAG ((uint32)0x00000400)

void port_set_longjump_regs(void* fn, Registers* regs, int num, ...)
{
    void** sp;
    va_list ap;
    int i;
    size_t align;
    void** p_pregs;
    size_t rcount =
        (sizeof(Registers) + sizeof(void*) - 1) / sizeof(void*);

    if (!regs)
        return;

    sp = (void**)regs->rsp - 16 - 1; /* preserve 128-bytes 'red zone' */
    *sp = (void*)regs->rip;
    align = !((rcount & 1) ^ (((uint64)sp & sizeof(void*)) != 0));
    p_pregs = sp - rcount - align - 1;
    sp = sp - rcount;
    *((Registers*)sp) = *regs;
    *p_pregs = (void*)sp;

//    sp = p_pregs - 16 - 1; /* preserve 128-bytes 'red zone' */
    sp = p_pregs - 1; /* set sp to return address */

    va_start(ap, num);

    if (num > 0)
    {
        void* arg = va_arg(ap, void*);
        if (arg == regs)
            regs->rdi = (uint64)(*p_pregs); /* Replace 1st arg */
        else
            regs->rdi = (uint64)arg;
    }

    if (num > 1)
        regs->rsi = (uint64)va_arg(ap, void*);

    if (num > 2)
        regs->rdx = (uint64)va_arg(ap, void*);

    if (num > 3)
        regs->rcx = (uint64)va_arg(ap, void*);

    if (num > 4)
        regs->r8 = (uint64)va_arg(ap, void*);

    if (num > 5)
        regs->r9 = (uint64)va_arg(ap, void*);

    *sp = (void*)&port_longjump_stub;
    regs->rsp = (uint64)sp;
    regs->rip = (uint64)fn;
    regs->eflags = regs->eflags & ~DIR_FLAG;
}

void port_transfer_to_function(void* fn, Registers* pregs, int num, ...)
{
    void** sp;
    va_list ap;
    int i;
    size_t align;
    void** p_pregs;
    size_t rcount =
        (sizeof(Registers) + sizeof(void*) - 1) / sizeof(void*);
    Registers regs;

    if (!pregs)
        return;

    regs = *pregs;

    sp = (void**)regs.rsp - 16 - 1; /* preserve 128-bytes 'red zone' */
    *sp = (void*)regs.rip;
    align = !((rcount & 1) ^ (((uint64)sp & sizeof(void*)) != 0));
    p_pregs = sp - rcount - align - 1;
    sp = sp - rcount;
    *((Registers*)sp) = regs;
    *p_pregs = (void*)sp;

//    sp = p_pregs - 16 - 1; /* preserve 128-bytes 'red zone' */
    sp = p_pregs - 1; /* set sp to return address */

    va_start(ap, num);

    if (num > 0)
    {
        void* arg = va_arg(ap, void*);
        if (arg == pregs)
            regs.rdi = (uint64)(*p_pregs); /* Replace 1st arg */
        else
            regs.rdi = (uint64)arg;
    }

    if (num > 1)
        regs.rsi = (uint64)va_arg(ap, void*);

    if (num > 2)
        regs.rdx = (uint64)va_arg(ap, void*);

    if (num > 3)
        regs.rcx = (uint64)va_arg(ap, void*);

    if (num > 4)
        regs.r8 = (uint64)va_arg(ap, void*);

    if (num > 5)
        regs.r9 = (uint64)va_arg(ap, void*);

    *sp = (void*)&port_longjump_stub;
    regs.rsp = (uint64)sp;
    regs.rip = (uint64)fn;
    regs.eflags = regs.eflags & ~DIR_FLAG;

    port_transfer_to_regs(&regs);
}
