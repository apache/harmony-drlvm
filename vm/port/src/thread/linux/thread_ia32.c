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
#include <stdarg.h>
#include "port_thread.h"


#if defined(LINUX)
void port_thread_context_to_regs(Registers* regs, ucontext_t *uc)
{
    regs->eax = uc->uc_mcontext.gregs[REG_EAX];
    regs->ecx = uc->uc_mcontext.gregs[REG_ECX];
    regs->edx = uc->uc_mcontext.gregs[REG_EDX];
    regs->edi = uc->uc_mcontext.gregs[REG_EDI];
    regs->esi = uc->uc_mcontext.gregs[REG_ESI];
    regs->ebx = uc->uc_mcontext.gregs[REG_EBX];
    regs->ebp = uc->uc_mcontext.gregs[REG_EBP];
    regs->eip = uc->uc_mcontext.gregs[REG_EIP];
    regs->esp = uc->uc_mcontext.gregs[REG_ESP];
    regs->eflags = uc->uc_mcontext.gregs[REG_EFL];
}

void port_thread_regs_to_context(ucontext_t *uc, Registers* regs)
{
    uc->uc_mcontext.gregs[REG_EAX] = regs->eax;
    uc->uc_mcontext.gregs[REG_ECX] = regs->ecx;
    uc->uc_mcontext.gregs[REG_EDX] = regs->edx;
    uc->uc_mcontext.gregs[REG_EDI] = regs->edi;
    uc->uc_mcontext.gregs[REG_ESI] = regs->esi;
    uc->uc_mcontext.gregs[REG_EBX] = regs->ebx;
    uc->uc_mcontext.gregs[REG_EBP] = regs->ebp;
    uc->uc_mcontext.gregs[REG_EIP] = regs->eip;
    uc->uc_mcontext.gregs[REG_ESP] = regs->esp;
    uc->uc_mcontext.gregs[REG_EFL] = regs->eflags;
}

#elif defined(FREEBSD)
void port_thread_context_to_regs(Registers* regs, ucontext_t *uc)
{
    regs->eax = uc->uc_mcontext.mc_eax;
    regs->ecx = uc->uc_mcontext.mc_ecx;
    regs->edx = uc->uc_mcontext.mc_edx;
    regs->edi = uc->uc_mcontext.mc_edi;
    regs->esi = uc->uc_mcontext.mc_esi;
    regs->ebx = uc->uc_mcontext.mc_ebx;
    regs->ebp = uc->uc_mcontext.mc_ebp;
    regs->eip = uc->uc_mcontext.mc_eip;
    regs->esp = uc->uc_mcontext.mc_esp;
    regs->eflags = uc->uc_mcontext.mc_eflags;
}

void port_thread_regs_to_context(ucontext_t *uc, Registers* regs)
{
    uc->uc_mcontext.mc_eax = regs->eax;
    uc->uc_mcontext.mc_ecx = regs->ecx;
    uc->uc_mcontext.mc_edx = regs->edx;
    uc->uc_mcontext.mc_edi = regs->edi;
    uc->uc_mcontext.mc_esi = regs->esi;
    uc->uc_mcontext.mc_ebx = regs->ebx;
    uc->uc_mcontext.mc_ebp = regs->ebp;
    uc->uc_mcontext.mc_eip = regs->eip;
    uc->uc_mcontext.mc_esp = regs->esp;
    uc->uc_mcontext.mc_eflags = regs->eflags;
}

#else
#error need to add correct mcontext_t lookup for registers
#endif


void port_longjump_stub(void);
#define DIR_FLAG ((uint32)0x00000400)

void port_set_longjump_regs(void* fn, Registers* regs, int num, ...)
{
    void** sp;
    va_list ap;
    int i;
    size_t rcount =
        (sizeof(Registers) + sizeof(void*) - 1) / sizeof(void*);

    if (!regs)
        return;

    sp = (void**)regs->esp - 1;
    *sp = (void*)regs->eip;
    sp = sp - rcount - 1;
    *((Registers*)(sp + 1)) = *regs;
    *sp = (void*)(sp + 1);
    regs->ebp = (uint32)sp;

    sp = sp - num - 1;

    va_start(ap, num);

    for (i = 1; i <= num; i = i + 1)
    {
        void* arg = va_arg(ap, void*);

        if (i == 1 && arg == regs)
            sp[i] = *((void**)regs->ebp); /* Replace 1st arg */
        else
            sp[i] = arg;
    }

    *sp = (void*)&port_longjump_stub;
    regs->esp = (uint32)sp;
    regs->eip = (uint32)fn;
    regs->eflags = regs->eflags & ~DIR_FLAG;
}

void port_transfer_to_function(void* fn, Registers* pregs, int num, ...)
{
    void** sp;
    va_list ap;
    int i;
    size_t rcount =
        (sizeof(Registers) + sizeof(void*) - 1) / sizeof(void*);
    Registers regs;

    if (!pregs)
        return;

    regs = *pregs;

    sp = (void**)regs.esp - 1;
    *sp = (void*)regs.eip;
    sp = sp - rcount - 1;
    *((Registers*)(sp + 1)) = regs;
    *sp = (void*)(sp + 1);
    regs.ebp = (uint32)sp;

    sp = sp - num - 1;

    va_start(ap, num);

    for (i = 1; i <= num; i = i + 1)
    {
        void* arg = va_arg(ap, void*);

        if (i == 1 && arg == pregs)
            sp[i] = *((void**)regs.ebp); /* Replace 1st arg */
        else
            sp[i] = arg;
    }

    *sp = (void*)&port_longjump_stub;
    regs.esp = (uint32)sp;
    regs.eip = (uint32)fn;
    regs.eflags = regs.eflags & ~DIR_FLAG;

    port_transfer_to_regs(&regs);
}
