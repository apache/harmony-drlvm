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
 * @author Ilya Berezhniuk
 * @version $Revision: 1.1.2.1 $
 */

#include "dec_base.h"
#include "native_modules.h"
#include "native_stack.h"


bool native_is_frame_exists(WalkContext* context, Registers* regs)
{
    // Check for frame layout and stack values
    if ((regs->ebp < regs->esp) || !native_is_in_stack(context, (void*)regs->ebp))
        return false; // Invalid frame

    void** frame_ptr = (void**)regs->ebp;
    void* eip = frame_ptr[1]; // Return address

    // Check return address for meaning
    return (native_is_in_code(context, eip) || native_is_ip_stub(eip));
}

bool native_unwind_stack_frame(WalkContext* context, Registers* regs)
{
    void** frame = (void**)regs->ebp;

    void* ebp = frame[0];
    void* eip = frame[1];
//    void* esp = (void*)(frame + 2);
    void* esp = &frame[2];


    if (native_is_in_stack(context, esp) &&
        (native_is_in_code(context, eip) || native_is_ip_stub(eip)))
    {
        regs->ebp = (uint32)ebp;
        regs->esp = (uint32)esp;
        regs->eip = (uint32)eip;
        return true;
    }

    return false;
}

void native_get_regs_from_jit_context(JitFrameContext* jfc, Registers* regs)
{
    regs->eip = *jfc->p_eip;
    regs->ebp = *jfc->p_ebp;
    regs->esp = jfc->esp;
}

static bool fill_regs_from_sp(WalkContext* context, Registers* regs, void** sp)
{
    regs->esp = (uint32)(sp + 1);
    regs->eip = (uint32)*sp;
    regs->ebp = native_is_in_stack(context, sp[-1]) ? (uint32)sp[-1] : regs->esp;
    return true;
}

static unsigned native_dec_instr(WalkContext* context, void* addr, void** target)
{
    Inst inst;

    if (!native_is_in_code(context, addr))
        return 0;

    uint32 len = DecoderBase::decode(addr, &inst);

    if (len == 0 ||
        inst.mn != Mnemonic_CALL ||
        inst.argc != 1)
        return 0;

    if (target && inst.operands[0].is_imm())
        *target = (void*)((uint32)addr + len + inst.operands[0].imm());

    return len;
}

static bool native_check_caller(WalkContext* context, Registers* regs, void** sp)
{
    void* target = NULL;
    char* ptr = (char*)*sp;
    
    if (native_dec_instr(context, ptr - 2, &target) == 2 || // CALL r/m32 w/o SIB w/o disp
        native_dec_instr(context, ptr - 3, &target) == 3 || // CALL r/m32 w/ SIB w/o disp
        native_dec_instr(context, ptr - 4, &target) == 4 || // CALL r/m32 w/ SIB w/ disp8
        native_dec_instr(context, ptr - 5, &target) == 5 || // CALL rel32
        native_dec_instr(context, ptr - 6, &target) == 6 || // CALL r/m32 w/o SIB w/ disp32
        native_dec_instr(context, ptr - 7, &target) == 7 || // CALL r/m32 w/ SIB w/ disp32
        native_dec_instr(context, ptr - 8, &target) == 8)   // CALL r/m32 w/ SIB w/ disp32 + Seg prefix
    {
        if (!target)
            return true;

        native_module_t* cur_module =
            port_find_module(context->modules, regs->get_ip());
        native_module_t* found_module =
            port_find_module(context->modules, target);

        return (cur_module == found_module);
    }

    return false;
}


// Max search depth for return address
#define MAX_SPECIAL_DEPTH 0x400
#define NATIVE_STRICT_UNWINDING 1

bool native_unwind_special(WalkContext* context, Registers* regs)
{
    for (void** cur_sp = (void**)regs->esp;
         (char*)cur_sp < ((char*)regs->esp + MAX_SPECIAL_DEPTH) && native_is_in_stack(context, cur_sp);
         ++cur_sp)
    {
        if (!native_is_in_code(context, *cur_sp))
            continue;

#if (!NATIVE_STRICT_UNWINDING)
        return fill_regs_from_sp(context, regs, cur_sp);
#else
        if (native_check_caller(context, regs, cur_sp))
            return fill_regs_from_sp(context, regs, cur_sp);
#endif
    }

    return false;
}

void native_fill_frame_info(Registers* regs, native_frame_t* frame, jint jdepth)
{
    frame->java_depth = jdepth;

    if (!regs)
        return;

    frame->ip = (void*)regs->eip;
    frame->frame = (void*)regs->ebp;
    frame->stack = (void*)regs->esp;
}
