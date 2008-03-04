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

#include "port_thread.h"
#define ANSI
#include <stdarg.h>


void port_thread_context_to_regs(Registers* regs, PCONTEXT context)
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

void port_thread_regs_to_context(PCONTEXT context, Registers* regs)
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
