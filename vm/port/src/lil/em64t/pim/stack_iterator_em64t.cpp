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
 * @author Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */

#include <string.h>

#include "environment.h"
#include "stack_iterator.h"
#include "vm_threads.h"
#include "method_lookup.h"
#include "jit_intf_cpp.h"
#include "encoder.h"
#include "m2n.h"
#include "m2n_em64t_internal.h"
#include "nogc.h"
#include "interpreter.h" // for ASSERT_NO_INTERPRETER
#include "cci.h"

#include "dump.h"
#include "vm_stats.h"

#include "cxxlog.h"

// see stack_iterator_ia32.cpp
struct StackIterator {
    CodeChunkInfo *   cci;
    JitFrameContext   jit_frame_context;
    M2nFrame *        m2n_frame;
    uint64            ip;
};

//////////////////////////////////////////////////////////////////////////
// Utilities

static void si_copy(StackIterator * dst, const StackIterator * src) {
    memcpy(dst, src, sizeof(StackIterator));
    // If src uses itself for ip then the dst should also do
    // to avoid problems if src is deallocated first.
    if (src->jit_frame_context.p_rip == &src->ip) {
        dst->jit_frame_context.p_rip = &dst->ip;
    }
}

static void init_context_from_registers(JitFrameContext & context,
                                        Registers & regs, bool is_ip_past) {
    context.rsp   = regs.rsp;
    context.p_rbp = &regs.rbp;
    context.p_rip = &regs.rip;

    context.p_rbx = &regs.rbx;
    context.p_r12 = &regs.r12;
    context.p_r13 = &regs.r13;
    context.p_r14 = &regs.r14;
    context.p_r15 = &regs.r15;
    
    context.p_rax = &regs.rax;
    context.p_rcx = &regs.rcx;
    context.p_rdx = &regs.rdx;
    context.p_rsi = &regs.rsi;
    context.p_rdi = &regs.rdi;
    context.p_r8  = &regs.r8;
    context.p_r9  = &regs.r9;
    context.p_r10 = &regs.r10;
    context.p_r11 = &regs.r11;
    
    context.is_ip_past = is_ip_past;
}


// Goto the managed frame immediately prior to m2nfl
static void si_unwind_from_m2n(StackIterator * si) {
#ifdef VM_STATS
    VM_Statistics::get_vm_stats().num_unwind_native_frames_all++;
#endif

    M2nFrame * current_m2n_frame = si->m2n_frame;
    assert(current_m2n_frame);

    si->m2n_frame = m2n_get_previous_frame(current_m2n_frame);

    TRACE2("si", "si_unwind_from_m2n, ip = " 
        << (void*)current_m2n_frame->rip);

    // Is it a normal M2nFrame or one for suspended managed code?
    if (m2n_is_suspended_frame(current_m2n_frame)) {
        // Suspended managed code, rip is at instruction,
        // rsp & registers are in regs structure
        TRACE2("si", "si_unwind_from_m2n from suspended managed code, ip = " 
            << (void*)current_m2n_frame->regs->rip);
        init_context_from_registers(si->jit_frame_context, *current_m2n_frame->regs, false);
    } else {
        // Normal M2nFrame, rip is past instruction,
        // rsp is implicitly address just beyond the frame,
        // callee saves registers in M2nFrame
        
        si->jit_frame_context.rsp = (uint64)((uint64*) m2n_get_frame_base(current_m2n_frame) + 1);
        
        si->jit_frame_context.p_rbp = &current_m2n_frame->rbp;
        si->jit_frame_context.p_rip = &current_m2n_frame->rip;

        si->jit_frame_context.p_rbx = &current_m2n_frame->rbx;
        si->jit_frame_context.p_r12 = &current_m2n_frame->r12;
        si->jit_frame_context.p_r13 = &current_m2n_frame->r13;
        si->jit_frame_context.p_r14 = &current_m2n_frame->r14;
        si->jit_frame_context.p_r15 = &current_m2n_frame->r15;
        si->jit_frame_context.is_ip_past = true;
    }
}

static inline char * get_reg(char * ss, const R_Opnd & dst, Reg_No base, int64 offset) {
    assert(Imm_Opnd(offset).get_size() <= size_32);
    ss = mov(ss, dst,  M_Base_Opnd(base, (int32)offset));
    return mov(ss, dst,  M_Base_Opnd(dst.reg_no(), 0));
}

typedef void (* transfer_control_stub_type)(StackIterator *);

static transfer_control_stub_type gen_transfer_control_stub()
{
    static transfer_control_stub_type addr = NULL;

    if (addr) {
        return addr;
    }

    const int STUB_SIZE = 68;
    char * stub = (char *)malloc_fixed_code_for_jit(STUB_SIZE,
        DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_COLD, CAA_Allocate);
    char * ss = stub;
#ifndef NDEBUG
    memset(stub, 0xcc /*int 3*/, STUB_SIZE);
#endif

    //
    // ************* LOW LEVEL DEPENDENCY! ***************
    // This code sequence must be atomic.  The "atomicity" effect is achieved by
    // changing the rsp at the very end of the sequence.

    // rdx holds the pointer to the stack iterator (skip return ip)
    ss = mov(ss, rdx_opnd, rdi_opnd); //M_Base_Opnd(rsp_reg, GR_STACK_SIZE));

    // restore eax (return value)
    ss = get_reg(ss, rax_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_rax);
    // Restore callee saves registers
    ss = get_reg(ss, r15_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_r15);
    ss = get_reg(ss, r14_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_r14);
    ss = get_reg(ss, r13_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_r13);
    ss = get_reg(ss, r12_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_r12);
    ss = get_reg(ss, rbx_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_rbx);
    ss = get_reg(ss, rbp_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_rbp);
    // cut the stack
    ss = mov(ss,  rsp_opnd,  M_Base_Opnd(rdx_reg,
        (int64)&((StackIterator *)0)->jit_frame_context.rsp));
    // grab the new ip
    ss = get_reg(ss, rdx_opnd, rdx_reg,
        (int64)&((StackIterator*)0)->jit_frame_context.p_rip);
    // jump
    ss = jump(ss,  rdx_opnd);

    addr = (transfer_control_stub_type)stub;
    assert(ss-stub <= STUB_SIZE);

    DUMP_STUB(stub, "getaddress__transfer_control", ss-stub);

    return addr;
}

//////////////////////////////////////////////////////////////////////////
// Stack Iterator Interface

StackIterator * si_create_from_native() {
    ASSERT_NO_INTERPRETER
    return si_create_from_native(p_TLS_vmthread);
}

StackIterator * si_create_from_native(VM_thread * thread) {
    ASSERT_NO_INTERPRETER
    // Allocate iterator
    StackIterator * si = (StackIterator *)STD_MALLOC(sizeof(StackIterator));
    memset(si, 0, sizeof(StackIterator));

    si->cci = NULL;
    si->jit_frame_context.p_rip = &si->ip;
    si->m2n_frame = m2n_get_last_frame(thread);
    si->ip = 0;

    return si;
}

StackIterator * si_create_from_registers(Registers * regs, bool is_ip_past,
                                        M2nFrame * lm2nf) {
    ASSERT_NO_INTERPRETER
    // Allocate iterator
    StackIterator * si = (StackIterator *)STD_MALLOC(sizeof(StackIterator));
    memset(si, 0, sizeof(StackIterator));

    Global_Env *env = VM_Global_State::loader_env;
    // Setup current frame
    si->cci = env->vm_methods->find((NativeCodePtr)regs->rip, is_ip_past);
    assert(si->cci);

    init_context_from_registers(si->jit_frame_context, *regs, is_ip_past);
    
    si->m2n_frame = lm2nf;
    si->ip = regs->rip;

    return si;
}

// On EM64T all registers are preserved automatically, so this is a nop.
void si_transfer_all_preserved_registers(StackIterator *) {
    ASSERT_NO_INTERPRETER
    // Do nothing
}

bool si_is_past_end(StackIterator * si) {
    ASSERT_NO_INTERPRETER
    // check if current position neither corresponds
    // to jit frame nor to m2n frame
    return si->cci == NULL && si->m2n_frame == NULL;
}

void si_goto_previous(StackIterator * si, bool over_popped) {
    ASSERT_NO_INTERPRETER
    if (si_is_native(si)) {
        TRACE2("si", "si_goto_previous from ip = " 
            << (void*)si_get_ip(si) << " (M2N)");
        if (si->m2n_frame == NULL) return;
        si_unwind_from_m2n(si);
    } else {
        assert(si->cci->get_jit() && si->cci->get_method());
        TRACE2("si", "si_goto_previous from ip = "
            << (void*)si_get_ip(si) << " ("
            << method_get_name(si->cci->get_method())
            << method_get_descriptor(si->cci->get_method()) << ")");
        si->cci->get_jit()->unwind_stack_frame(si->cci->get_method(), si_get_jit_context(si));
        si->jit_frame_context.is_ip_past = TRUE;
    }

    Global_Env *vm_env = VM_Global_State::loader_env;
    si->cci = vm_env->vm_methods->find(si_get_ip(si), si_get_jit_context(si)->is_ip_past);
#ifndef NDEBUG
    if (si_is_native(si)) {
        TRACE2("si", "si_goto_previous to ip = " << (void*)si_get_ip(si)
            << " (M2N)");
    } else {
        TRACE2("si", "si_goto_previous to ip = " << (void*)si_get_ip(si)
            << " (" << method_get_name(si->cci->get_method())
            << method_get_descriptor(si->cci->get_method()) << ")");
    }
#endif
}

StackIterator * si_dup(StackIterator * si) {
    ASSERT_NO_INTERPRETER
    StackIterator * dup_si = (StackIterator *)STD_MALLOC(sizeof(StackIterator));
    si_copy(dup_si, si);
    return dup_si;
}

void si_free(StackIterator * si) {
    STD_FREE(si);
}

NativeCodePtr si_get_ip(StackIterator * si) {
    ASSERT_NO_INTERPRETER
    return (NativeCodePtr)(*si->jit_frame_context.p_rip);
}

void si_set_ip(StackIterator * si, NativeCodePtr ip, bool also_update_stack_itself) {
    if (also_update_stack_itself) {
        *(si->jit_frame_context.p_rip) = (uint64)ip;
    } else {
        si->ip = (uint64)ip;
        si->jit_frame_context.p_rip = &si->ip;
    }
}

// 20040713 Experimental: set the code chunk in the stack iterator
void si_set_code_chunk_info(StackIterator * si, CodeChunkInfo * cci) {
    ASSERT_NO_INTERPRETER
    assert(si);
    si->cci = cci;
}

CodeChunkInfo * si_get_code_chunk_info(StackIterator * si) {
    return si->cci;
}

JitFrameContext * si_get_jit_context(StackIterator * si) {
    return &si->jit_frame_context;
}

bool si_is_native(StackIterator * si) {
    ASSERT_NO_INTERPRETER
    return si->cci == NULL;
}

M2nFrame * si_get_m2n(StackIterator * si) {
    ASSERT_NO_INTERPRETER
    return si->m2n_frame;
}

void si_set_return_pointer(StackIterator * si, void ** return_value) {
    // TODO: check if it is needed to dereference return_value
    si->jit_frame_context.p_rax = (uint64 *)return_value;
}

void si_transfer_control(StackIterator * si) {
    // !!! NO LOGGER IS ALLOWED IN THIS FUNCTION !!!
    // !!! RELEASE BUILD WILL BE BROKEN          !!!
    // !!! NO TRACE2, INFO, WARN, ECHO, ASSERT,  ...
    
    // 1. Copy si to stack
    StackIterator local_si;
    si_copy(&local_si, si);
    si_free(si);

    // 2. Set the M2nFrame list
    m2n_set_last_frame(local_si.m2n_frame);
    
    // 3. Call the stub
    transfer_control_stub_type tcs = gen_transfer_control_stub();
    tcs(&local_si);
}

void si_copy_to_registers(StackIterator * si, Registers * regs) {
    ASSERT_NO_INTERPRETER    

    regs->rsp = si->jit_frame_context.rsp;
    regs->rbp = *si->jit_frame_context.p_rbp;
    regs->rip = *si->jit_frame_context.p_rip;

    regs->rbx = *si->jit_frame_context.p_rbx;
    regs->r12 = *si->jit_frame_context.p_r12;
    regs->r13 = *si->jit_frame_context.p_r13;
    regs->r14 = *si->jit_frame_context.p_r14;
    regs->r15 = *si->jit_frame_context.p_r15;

    regs->rax = *si->jit_frame_context.p_rax;
    regs->rcx = *si->jit_frame_context.p_rcx;
    regs->rdx = *si->jit_frame_context.p_rdx;
    regs->rsi = *si->jit_frame_context.p_rsi;
    regs->rdi = *si->jit_frame_context.p_rdi;
    regs->r8 = *si->jit_frame_context.p_r8;
    regs->r9 = *si->jit_frame_context.p_r9;
    regs->r10 = *si->jit_frame_context.p_r10;
    regs->r11 = *si->jit_frame_context.p_r11;
}

void si_set_callback(StackIterator* si, NativeCodePtr* callback) {
    si->jit_frame_context.rsp = si->jit_frame_context.rsp - 4;
    *((uint64*) si->jit_frame_context.rsp) = *(si->jit_frame_context.p_rip);
    si->jit_frame_context.p_rip = ((uint64*)callback);
}

void si_reload_registers() {
    // Nothing to do
}
