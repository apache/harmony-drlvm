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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.6.12.3.4.4 $
 */
#include "compiler.h"
#include "arith_rt.h"
#include "trace.h"
#include "cg_ia32.h"
#include "stats.h"

#include <limits.h>
#include <malloc.h>

#include <open/vm.h>

#include <jit_runtime_support.h>
#include <jit_intf.h>

#include <stdarg.h>
#include "jni_types.h"

/**
 * @file
 * @brief Codegenerator's routines specific for IA32/EM64T.
 */
 
namespace Jitrino {
namespace Jet {

const TypeInfo typeInfo[num_jtypes] =
{
    { OpndKind_GPReg, OpndSize_8, 1, Mnemonic_MOV, },   // i8
    { OpndKind_GPReg, OpndSize_16, 2, Mnemonic_MOV },   // i16
    { OpndKind_GPReg, OpndSize_16, 2, Mnemonic_MOV },   // u16
    { OpndKind_GPReg, OpndSize_32, 4, Mnemonic_MOV },   // i32
    { OpndKind_GPReg, OpndSize_32, 8, Mnemonic_MOV },   // i64
    { OpndKind_XMMReg, OpndSize_32, 4, Mnemonic_MOVSS}, // flt32
    { OpndKind_XMMReg, OpndSize_64, 8, Mnemonic_MOVQ},  // dbl64
    { OpndKind_GPReg, OpndSize_32, 4, Mnemonic_MOV},    // jobj
    { OpndKind_Null, OpndSize_Null, 0, Mnemonic_NOP},   // jvoid
    { OpndKind_GPReg, OpndSize_32, 4, Mnemonic_MOV},    // jretAddr
};

/**
 * @brief 32-bit integer constant, with value of zero.
 */
static const EncoderBase::Operand Imm32_0(OpndSize_32, 0);
/**
 * @brief 32-bit integer constant, with value of minus one.
 */
static const EncoderBase::Operand Imm32_m1(OpndSize_32, -1);
/**
 * @brief 8-bit integer constant, specifies size of one stack slot.
 */
static const EncoderBase::Operand Imm8_StackSlot(OpndSize_8,
                                                 STACK_SLOT_SIZE);
/**
 * @brief 8-bit integer constant, specifies size of 2 stack slots.
 */
static const EncoderBase::Operand Imm8_StackSlotX2(OpndSize_8, 
                                                   STACK_SLOT_SIZE*2);
/**
 * @brief 8-bit integer constant, specifies size of 3 stack slots.
 */
static const EncoderBase::Operand Imm8_StackSlotX3(OpndSize_8, 
                                                   STACK_SLOT_SIZE*3);
/**
 * @brief Points to the top of native stack, '[ESP]'.
 */
static const EncoderBase::Operand StackTop32(OpndSize_32, RegName_ESP, 0);
/**
 * @brief Points to an integer item next to native stack top, 
 *        '[ESP + stack_slot_size]'.
 */
static const EncoderBase::Operand Stack32_1(OpndSize_32, RegName_ESP, 
                                            STACK_SLOT_SIZE);
/**
 * @brief Handy definition of 32bit integer as Encoder's operand.
 */
#define MK_IMM32(value) EncoderBase::Operand(OpndSize_32, (int)(value))
/**
 * @brief Handy definition of 16bit integer as Encoder's operand.
 */
#define MK_IMM16(value) EncoderBase::Operand(OpndSize_16, (short)(value))
/**
 * @brief Handy definition of 32bit memory location as Encoder's operand.
 */
#define MK_MEM32(base, off) EncoderBase::Operand(OpndSize_32, (base), (off))

/**
 * @brief Constant value to be used for CMOVcc operations.
 */
static const unsigned g_1 = 1;

void Compiler::gen_dbg_rt_out(const char *frmt, ...)
{
    char buf[1024 * 5];
    va_list valist;
    va_start(valist, frmt);
    int len = vsnprintf(buf, sizeof(buf)-1, frmt, valist);
    // The pointer get lost and can not be freed - this is intentional,
    // as the method's lifetime usually is the same as VM's lifetime.
    char *lost = new char[len + 1];
    strcpy(lost, buf);
    const int esp_idx = getRegIndex(RegName_ESP);
    const int num_gp_regs = 8;
    for (int i=0; i<num_gp_regs; i++) {
        if (i == esp_idx) {
            continue;
        }
        voper(Mnemonic_PUSH, getRegName(OpndKind_GPReg, OpndSize_32, i));
    }
    voper(Mnemonic_PUSH, MK_IMM32(lost));
    voper(Mnemonic_MOV, RegName_EAX, MK_IMM32(&rt_dbg));
    voper(Mnemonic_CALL, RegName_EAX);
    for (int i=num_gp_regs-1; i>=0; i--) {
        if (i == esp_idx) {
            continue;
        }
        voper(Mnemonic_POP, getRegName(OpndKind_GPReg, OpndSize_32, i));
    }
}

/**
 * @brief Maps conditional branches JavaByteCodes to appropriate 
 *        ConditionMnemonic.
 *
 * Accepts only opcode in ranges: [OPCODE_IFEQ; OPCODE_IFLE] and 
 * [OPCODE_IF_ICMPEQ; OPCODE_IF_ICMPLE].
 *
 * @param opcod - a byte code
 * @return appropriate ConditionMnemonic 
 */
static ConditionMnemonic map_cc(JavaByteCodes opcod)
{
    unsigned cod;
    if (OPCODE_IFEQ <= opcod && opcod <= OPCODE_IFLE) {
        cod = opcod - OPCODE_IFEQ;
    }
    else {
        assert(OPCODE_IF_ICMPEQ <= opcod && opcod <= OPCODE_IF_ICMPLE);
        cod = opcod - OPCODE_IF_ICMPEQ;
    }
    
    switch (cod) {
    case 0:
        return ConditionMnemonic_E;
    case 1:
        return ConditionMnemonic_NE;
    case 2:
        return ConditionMnemonic_L;
    case 3:
        return ConditionMnemonic_GE;
    case 4:
        return ConditionMnemonic_G;
    case 5:
        return ConditionMnemonic_LE;
    default:
        break;
    }
    assert(false);
    return (ConditionMnemonic) -1;
}

void Compiler::gen_prolog(const::std::vector < jtype > &args)
{
    if (m_infoBlock.get_flags() & DBG_TRACE_EE) {
        gen_dbg_rt_out("entering: %s", m_fname);
    }
    if (m_infoBlock.get_bc_size() == 1 && m_bc[0] == OPCODE_RETURN) {
        // Empty method, nothing to do;  the same is in gen_return();
        
        // So even don't try to do root set enum, regardless of number of 
        // locals and stack size
        m_infoBlock.set_flags(m_infoBlock.get_flags() | JMF_EMPTY_RS);
        if (m_infoBlock.get_flags() & DBG_BRK) {
            gen_dbg_brk();
        }
        return;
    }

    if (m_infoBlock.get_flags() & JMF_PROF_ENTRY_BE) {
        voper(Mnemonic_INC, MK_MEM32(RegName_Null, (int)m_p_methentry_counter));
    }
    
    voper(Mnemonic_PUSH, RegName_EBX);
    voper(Mnemonic_PUSH, RegName_ESI);
    voper(Mnemonic_PUSH, RegName_EDI);
    voper(Mnemonic_PUSH, RegName_EBP);
    voper(Mnemonic_MOV,  REG_BASE, RegName_ESP);
    voper(Mnemonic_MOV,  RegName_EDX, RegName_ESP);

    if (m_infoBlock.get_flags() & JMF_ALIGN_STACK) {
        voper(Mnemonic_SUB,  RegName_ESP, EncoderBase::Operand(OpndSize_32, 7));
        voper(Mnemonic_AND,  RegName_ESP, EncoderBase::Operand(OpndSize_32, ~7));
        voper(Mnemonic_PUSH, REG_BASE);
        voper(Mnemonic_MOV,  REG_BASE, RegName_ESP);
    }

    //
    // @ this point, EDX holds an 'old' ESP which point to the saved EBP
    //
    
    //
    // allocate stack frame
    //
    unsigned frameSize = m_stack.size() * SLOT_SIZE;
    const unsigned PAGE_SIZE = 0x1000;
    unsigned pages = frameSize / PAGE_SIZE;
    // Here is pretty rare case, though still need to be proceeded:
    // When we allocate a stack frame of size more than one page
    // then the memory page(s) may not be accessible and even not 
    // allocated.
    // A direct access to such page during 'REP STOS' raises 'access 
    // violation'. To avoid the problem we need simply probe (make read 
    // access) to the pages sequentially. In response on read-access to 
    // inaccessible page, the OS grows up the stack, so pages become 
    // accessible.
    //
    if (pages>5) {
        /*
        MOV EAX, pages
        XOR ECX, ECX
        _tryit:
        TEST [esp+ecx], eax
        SUB ECX, PAGE_SIZE
        DEC EAX
        JAE _tryit
        */
        // The size of block with cycle about 30 bytes, and the size of 
        // single TEST is about 7 bytes. So it worths to use loop block
        // for pages>5
        voper(Mnemonic_MOV, RegName_EAX, MK_IMM32(pages));
        voper(Mnemonic_XOR, RegName_ECX, RegName_ECX);
        char * addr = ip();
        voper(Mnemonic_TEST, 
              EncoderBase::Operand(OpndSize_32, RegName_ESP, RegName_ECX, 1, 0),
              RegName_EAX);
        voper(Mnemonic_SUB, RegName_ECX, MK_IMM32(PAGE_SIZE));
        voper(Mnemonic_DEC, RegName_EAX);
        unsigned pid = vjcc(ConditionMnemonic_NZ);
        patch_set_target(pid, addr);
    }
    else {
        for(unsigned i=0; i<pages; i++) {
            voper(Mnemonic_TEST, MK_MEM32(RegName_ESP, -(int)i*PAGE_SIZE),
                  RegName_EAX);
        }
    }
    voper(Mnemonic_SUB, RegName_ESP, MK_IMM32(frameSize));

    /* xor eax, eax       */ voper(Mnemonic_XOR, RegName_EAX, RegName_EAX);
    /* mov ecx, num.slots */
    voper(Mnemonic_MOV, RegName_ECX, MK_IMM32(m_stack.size()));
    /* mov edi, esp       */ 
    voper(Mnemonic_MOV, RegName_EDI, RegName_ESP);
    /* rep stosd          */
    vprefix(InstPrefix_REP);
    voper(Mnemonic_STOS);
    //
    // reload input args into local vars
    //
    // an initial GC map, basing on the input args
    ::std::vector < long > initial_map;
    initial_map.resize(words(args.size()));

    unsigned offset = 0;
    for (unsigned i = 0; i < args.size(); i++) {
        const jtype jt = args[i];
        if (is_wide(jt)) {
            if (is_big(jt)) {
                EncoderBase::Operand inarg(OpndSize_32, RegName_EDX,
                                       m_stack.in_slot(offset+1) * SLOT_SIZE);
                EncoderBase::Operand local(OpndSize_32, REG_BASE,
                                            m_stack.local(offset)* SLOT_SIZE);
                voper(Mnemonic_MOV, RegName_EAX, inarg);
                voper(Mnemonic_MOV, local, RegName_EAX);
                
                EncoderBase::Operand inarg_hi(OpndSize_32, RegName_EDX,
                                    m_stack.in_slot(offset) * SLOT_SIZE);
                EncoderBase::Operand local_hi(OpndSize_32, REG_BASE,
                                    m_stack.local(offset+1)* SLOT_SIZE);
                voper(Mnemonic_MOV, RegName_EAX, inarg_hi);
                voper(Mnemonic_MOV, local_hi, RegName_EAX);
            }
            else {
                EncoderBase::Operand inarg(OpndSize_64, RegName_EDX,
                                      m_stack.in_slot(offset+1) * SLOT_SIZE);
                EncoderBase::Operand local(OpndSize_64, REG_BASE,
                                      m_stack.local(offset)* SLOT_SIZE);
                voper(Mnemonic_MOVQ, RegName_XMM0D, inarg);
                voper(Mnemonic_MOVQ, local, RegName_XMM0D);
            }
            offset += 2;
        }
        else {
            EncoderBase::Operand inarg(OpndSize_32, RegName_EDX, 
                                       m_stack.in_slot(offset) * SLOT_SIZE);
            EncoderBase::Operand local(OpndSize_32, REG_BASE,
                                        m_stack.local(offset)* SLOT_SIZE);
            voper(Mnemonic_MOV, RegName_EAX, inarg);
            voper(Mnemonic_MOV, local, RegName_EAX);
            if (jt == jobj) {
                initial_map[word_no(offset)] = 
                    initial_map[word_no(offset)] | (1 <<bit_no(offset));
            }
            ++offset;
        }
    }

    for (unsigned i = 0; i < initial_map.size(); i++) {
        voper(Mnemonic_MOV, 
              MK_MEM32(REG_BASE, SLOT_SIZE*(m_stack.info_gc_locals()+i)),
              MK_IMM32(initial_map[i]));
    }

    //
    // Ok, now everything is ready, may call VM/whatever, if necessary
    //
    if (m_infoBlock.get_flags() & JMF_PROF_SYNC_CHECK) {
        /*
        XOR EAX, EAX
        CMP     [meth_entry_counter], meth_entry_threshold
        CMOVAE  EAX, [1]
        CMP     [back_edge_counter], back_edge_threshold
        CMOVAE  EAX, [1]
        TEST    EAX, EAX
        JZ      _cont
        MOV     [meth_entry_counter], 0
        MOV     [back_edge_counter], 0
        PUSH    profile_handle
        CALL    recomp
        JMP     _cont
        */
        voper(Mnemonic_XOR, RegName_EAX, RegName_EAX);
        voper(Mnemonic_CMP, MK_MEM32(RegName_Null, (int)m_p_methentry_counter), 
                            MK_IMM32(m_methentry_threshold));
        voper(Mnemonic_CMOVAE, RegName_EAX, MK_MEM32(RegName_Null, (int)&g_1));
        voper(Mnemonic_CMP, MK_MEM32(RegName_Null, (int)m_p_backedge_counter), 
                            MK_IMM32(m_backedge_threshold));
        voper(Mnemonic_CMOVAE, RegName_EAX, MK_MEM32(RegName_Null, (int)&g_1));
        voper(Mnemonic_TEST, RegName_EAX, RegName_EAX);
        unsigned pid = vjcc(ConditionMnemonic_Z, InstPrefix_HintTaken);
        // Zero out both the counters
        //voper(Mnemonic_MOV, MK_MEM32(RegName_Null, (int)m_p_methentry_counter),
        //                    Imm32_0);
        //voper(Mnemonic_MOV, MK_MEM32(RegName_Null, (int)m_p_backedge_counter),
        //                    Imm32_0);
        gen_call_vm(m_recomp_handler_ptr, 1, m_profile_handle);
        patch_set_target(pid, ip());
    }

    // JVMTI method_enter helper - uncomment after bug #XXX get fixed.
    //if (compilation_params.exe_notify_method_entry) {
    //    if (rt_helper_ti_method_enter == NULL) {
    //        rt_helper_ti_method_enter = (char *)
    //               vm_get_rt_support_addr(VM_RT_JVMTI_METHOD_ENTER_CALLBACK);
    //    }
    //    voper(Mnemonic_PUSH, MK_IMM32(m_method));
    //    vcall(rt_helper_ti_method_enter);
    //}

    if (m_java_meth_flags & ACC_SYNCHRONIZED) {
        char * helper;
        if (method_is_static(m_method)) {
            /* push klass  */ voper(Mnemonic_PUSH, MK_IMM32(m_klass));
            helper = rt_helper_monitor_enter_static;
        }
        else {
            /* push this   */ voper(Mnemonic_PUSH, vmlocal(jobj, 0));
            helper = rt_helper_monitor_enter;
        }
        voper(Mnemonic_MOV, RegName_EAX, MK_IMM32(helper));
        voper(Mnemonic_CALL, RegName_EAX);
    }
    if (m_infoBlock.get_flags() & DBG_BRK) {
        gen_dbg_brk();
    }
}

void Compiler::gen_return(jtype retType)
{
    if (m_infoBlock.get_flags() & DBG_TRACE_EE) {
        gen_dbg_rt_out("exiting : %s", m_fname);
    }
    
    if (m_infoBlock.get_bc_size() == 1 && m_bc[0] == OPCODE_RETURN) {
        // empty method, nothing to do; the same is in gen_prolog();
        // TODO: need to check and make sure whether it's absolutely legal
        // to bypass monitors on such an empty method
        // TODO2: this op9n bypasses JVMTI notifications - fixit
        if (m_infoBlock.get_in_slots() == 0) {
            voper(Mnemonic_RET);
        }
        else {
            voper(Mnemonic_RET,
                  MK_IMM16(m_infoBlock.get_in_slots()*STACK_SLOT_SIZE));
        }
        if (retType != jvoid) {
            vpop(retType);
        }
        m_jframe->clear_stack();
        return;
    }
    
    if (m_java_meth_flags & ACC_SYNCHRONIZED) {
        if (m_java_meth_flags & ACC_STATIC) {
            gen_call_vm(rt_helper_monitor_exit_static, 1, m_klass);
        }
        else {
            // As we don not use gen_call_vm(), then 'manually' update the 
            // stack info. Don't care about local vars, as the method is 
            // about to exit anyway
            gen_gc_stack();
            gen_mem(MEM_TO_MEM|MEM_STACK|MEM_UPDATE);
            //
            // show the 'this' incoming argument, as the local vars could be
            // overwritten
            //
            /*
            mov edx, [ebp]
            mov edx, [edx+m_stack.in_slot(0)]
            push edx
            */
            if (m_infoBlock.get_flags() & JMF_ALIGN_STACK) {
                voper(Mnemonic_MOV, RegName_EDX, MK_MEM32(REG_BASE, 0));
            }
            else {
                voper(Mnemonic_MOV, RegName_EDX, REG_BASE);
            }
            voper(Mnemonic_MOV, RegName_EDX, 
                  MK_MEM32(RegName_EDX, m_stack.in_slot(0) * SLOT_SIZE));
            voper(Mnemonic_PUSH, RegName_EDX);
            vcall(rt_helper_monitor_exit);
        }
    }
    //XXX - uncomment after the bug#XXXX get fixed
    //if (compilation_params.exe_notify_method_exit) {
    //    jtype args[1] = {m_jframe->top()};
    //    gen_stack_to_args(false, 1, args);
    //    gen_call_vm(rt_helper_ti_method_exit, 1, m_method);
    //}

    if (is_f(retType)) {
        if (!vstack_swapped(0)) {
            vstack_swap(0);
        }
        voper(Mnemonic_FLD, retType == dbl64 ? RegName_FP0D : RegName_FP0S, 
                            vmstack(0));
    }
    else if (retType != jvoid) {
        if (vstack_swapped(0)) {
            voper(Mnemonic_MOV, RegName_EAX, vstack_mem_slot(0));
        }
        else {
            voper(Mnemonic_MOV, RegName_EAX, vstack(0));
        }
        
        if (retType == i64) {
            if (vstack_swapped(1)) { 
                voper(Mnemonic_MOV, RegName_EDX, vstack_mem_slot(1));
            }
            else { 
                voper(Mnemonic_MOV, RegName_EDX, vstack(1));
            }
        }
    }
    
    if (m_infoBlock.get_flags() & JMF_ALIGN_STACK) {
        voper(Mnemonic_MOV, RegName_ESP, MK_MEM32(REG_BASE, 0));
    }
    else {
        voper(Mnemonic_MOV, RegName_ESP, REG_BASE);
    }
    voper(Mnemonic_POP, RegName_EBP);
    voper(Mnemonic_POP, RegName_EDI);
    voper(Mnemonic_POP, RegName_ESI);
    voper(Mnemonic_POP, RegName_EBX);

    if (m_infoBlock.get_in_slots() == 0) {
        voper(Mnemonic_RET);
    }
    else {
        voper(Mnemonic_RET,
              MK_IMM16(m_infoBlock.get_in_slots() * STACK_SLOT_SIZE));
    }
    if (retType != jvoid) {
        vpop(retType);
    }
    m_jframe->clear_stack();
}

void Compiler::gen_ldc(void)
{
    jtype jtyp = to_jtype(class_get_const_type(
                                m_klass, (unsigned short)m_curr_inst->op0));
    if (jtyp != jobj) { // if not loading String
        const void * p = class_get_const_addr(m_klass, m_curr_inst->op0);
        assert( p );
        if (jtyp == dbl64 || jtyp == flt32) {
            gen_push(jtyp, p);
        }
        else if(jtyp == i64) {
            gen_push(*(jlong*)p );
        }
        else if( jtyp == i32) {
            gen_push(*(int*)p );
        }
        else if(jtyp == u16) {
            gen_push(*(unsigned short*)p);
        }
        else if(jtyp == i16) {
            gen_push(*(short*)p);
        }
        else {
            assert(jtyp == i8);
            gen_push(*(char*)p);
        }
        return;
    }
    assert(m_curr_inst->opcode != OPCODE_LDC2_W);
    gen_call_vm(rt_helper_ldc_string, 2, m_curr_inst->op0, m_klass);
    gen_save_ret(jobj);
    m_jframe->stack_attrs(0, SA_NZ);
    m_curr_bb_state->seen_gcpt = true;
}


void Compiler::gen_push(int lval)
{
    vpush(i32, true);
    RegName r0 = vstack(0);
    if (lval == 0) {
        voper(Mnemonic_XOR, r0, r0);
    }
    else {
        voper(Mnemonic_MOV, r0, EncoderBase::Operand(OpndSize_32, lval));
    }
    if (lval>=0) {
        m_jframe->stack_attrs(0, SA_NOT_NEG);
    }
}

void Compiler::gen_push(jlong lval)
{
    vpush(i64, true);
    
    unsigned h32 = hi32(lval);
    unsigned l32 = lo32(lval);
    
    RegName r0 = vstack(0);
    RegName r1 = vstack(1);
    
    if (h32 == 0) {
        voper(Mnemonic_XOR, r1, r1);
    }
    else {
        voper(Mnemonic_MOV, r1, MK_IMM32(h32));
    }
    if (l32 == 0) {
        voper(Mnemonic_XOR, r0, r0);
    }
    else {
        voper(Mnemonic_MOV, r0, MK_IMM32(l32));
    }
}

void Compiler::gen_push(jtype jt, const void *p)
{
    vpush(jt, true);
    RegName r0 = vstack(0);
    if (p == &g_dconst_0) {
        // dbl64 
        voper(Mnemonic_PXOR, r0, r0);
    }
    else if (p == &g_fconst_0) {
        // flt32
        voper(Mnemonic_PXOR, getAliasReg(r0, OpndSize_64), 
                             getAliasReg(r0, OpndSize_64));
    }
    else if (NULL == p) {
        // aconst_null
        voper(Mnemonic_XOR, r0, r0);
    }
    else {
        const EncoderBase::Operand mptr(typeInfo[jt].size,RegName_Null,(int)p);
        if (jt == u16 || jt==i8 || jt==i16) {
            voper(jt == u16 ? Mnemonic_MOVZX : Mnemonic_MOVSX, r0, mptr);
        }
        else {
            voper(typeInfo[jt].mov, r0, mptr);
        }
        if (is_big(jt)) {
            RegName r1 = vstack(1);
            const EncoderBase::Operand mptr1(typeInfo[jt].size,RegName_Null,
                                             (int)p + STACK_SLOT_SIZE);
            voper(typeInfo[jt].mov, r1, mptr1);
        }
    }
}

void Compiler::gen_pop(jtype jt, void *where)
{
    if (where != NULL) {
        // this must be PUTSTATIC ?
        RegName r = vstack(0);
        if (jt < i32) {
            r = valias(jt,r);
        }
        const EncoderBase::Operand mptr(typeInfo[jt].size,RegName_Null,
                                        (int)where);
        voper(typeInfo[jt].mov, mptr, r);
        if (is_big(jt)) {
            RegName r1 = vstack(1);
            const EncoderBase::Operand mptr1(typeInfo[jt].size,RegName_Null,
                                             (int)where + STACK_SLOT_SIZE);
            voper(typeInfo[jt].mov, mptr1, r1);
        }
    }
    vpop(jt);
}

void Compiler::gen_pop2(void)
{
    jtype jt = m_jframe->top();
    if (is_wide(jt)) {
        vpop(jt);
    }
    else {
        vpop(jt);
        assert(!is_wide(m_jframe->top()));
        vpop(m_jframe->top());
    }
}

void Compiler::gen_dup(JavaByteCodes opc)
{
    //
    // Below is a bit over-complicated procedure: 
    // as the items on operand stack are tracked separately, then for 
    // a DUP_* operation an action to perform depends on which DUP_* to 
    // perform and which items are on the top of the stack. The complete 
    // procedure seems to have too many states, so a quick and easy variant
    // implemented: some number (depends of DUP_*) of operand stack items
    // are spilled onto a native stack, then stack slots are shuffled 
    // as appropriate on the native stack, and then they are loaded back to 
    // registers. TODO: reimplement, make more clean, avoid memory ops
    // Most frequent or simple variants of DUPs are implemented directly on 
    // registers, without spilling out to memory.
    //

#ifdef _DEBUG
    // A test JFrame, used to check that spill out-shuffling-uploading
    // was performed properly.
    JFrame control;
    control.init(m_jframe); // make a copy of the current state
    control.dup(opc);       // perform an operation on operand stack
#endif
    // how many slots on native stack affected (in total, after the DUP_*
    // performed)
    unsigned totalSlots = 0;
    // a state of top of operand stack which was before the DUP_*
    unsigned num_was = 0;   jtype was[6];
    // a state of top of operand stack which to become after the DUP_*
    unsigned num_new = 0;   jtype _new[6];
    // which items were added during the the DUP_*
    unsigned num_added = 0; unsigned added[2];
    
    switch (opc) {
    case OPCODE_DUP:
        // [.. val] => [.. val, val]
        {
        m_jframe->dup(opc);
        vsync();
        const jtype jt = m_jframe->top();
        voper(typeInfo[jt].mov, vstack(0), vstack(1));
        }
        break;
    case OPCODE_DUP_X1:
        // [..., value2, value1] => [.. value1, value2, value1]
        totalSlots = 3;
        num_added = 1; added[0] = 0;
        num_was = 2;   was[0] = m_jframe->top(1);   was[1] = m_jframe->top(0);
        num_new = 3;   _new[0] = was[1]; _new[1] = was[0]; _new[2] = was[1];
        break;
    case OPCODE_DUP_X2:
        // [.. value2.64, value1] => [.. value1, value2.64, value1]
        /*
        pop          op1                     ; op1 <= value1
        movSD        xmm0, [esp]     ; xmm0 <= value2.64
        ; >> add esp, 8 + push op1 + sub esp, 8 : 
        mov          [esp+4], op1
        sub          esp, 4
        ; <<
        movSD        [esp], xmm0
        push op1
        */
        // Form 1:
        // .. value3, value2, value1 => value1, value3, value2, value1
        // Form 2:
        // .. value2, value1 =>  ..value1, value2, value1
        totalSlots = 4;
        num_added = 1; added[0] = 0;
        if (is_wide(m_jframe->top(1))) {
            // Form 2
            num_was = 2;    was[0] = m_jframe->top(2);  was[1] = m_jframe->top(0);
            num_new = 3;   _new[0] = was[1]; _new[1] = was[0]; _new[2] = was[1];
        }
        else {
            num_was = 3; was[0] = m_jframe->top(2);   was[1] = m_jframe->top(1);
                         was[2] = m_jframe->top(0);
            num_new = 4; _new[0] = m_jframe->top(0); _new[1] = m_jframe->top(2);
                        _new[2] = m_jframe->top(1); _new[3] = m_jframe->top(0);
        }
        break;
    case OPCODE_DUP2:
        // [.. value.64] => [.. value.64, value.64]
        // [.. value.32.1, value32.0] => [.. value.32.1, value32.0, 
        //          value.32.1, value32.0]
        if (m_jframe->top() == dbl64) {
            m_jframe->dup(opc);
            vsync();
            const jtype jt = m_jframe->top();
            voper(typeInfo[jt].mov, vstack(0), vstack(2));
        }
        else {
            totalSlots = 4;
            num_added = 2; added[0] = 1; added[1] = 0;
            if (is_wide(m_jframe->top())) { 
                num_was = 1;   was[0] = m_jframe->top(0);
                num_new = 2;   _new[0] = _new[1] = was[0];
            }
            else {
                num_was = 2;   was[0] = m_jframe->top(1);
                               was[1] = m_jframe->top(0);
                num_new = 4;   _new[0] = m_jframe->top(1); 
                               _new[1] = m_jframe->top(0);
                               _new[2]=m_jframe->top(1);
                               _new[3] = m_jframe->top(0);
            }
        }
        break;
    case OPCODE_DUP2_X1:
        // [.. value2, value1.64] =>  [.. value1.64, value2, value1.64]
        /*
        movsd sse0, [esp]
        mov eax, [esp+slotSize*2]
        sub          esp, slotSize*2                 ; before: 3 slots 
                                                    accupied ; after: 5 slots
        movsd        esp+3*slotSize, xmm0
        mov          esp+2*slotSize, eax
        movsd   [esp], xmm0
        */
        // value3, value2, value1  => value2, value1, value3, value2, value1
        // value2, value1  => value1, value2, value1        
        totalSlots = 5;
        num_added = 2; added[0] = 1; added[1] = 0;
        if (is_wide(m_jframe->top())) { 
            num_was = 2;   was[0] = m_jframe->top(2); was[1] = m_jframe->top(0);
            num_new = 3;   _new[0] = m_jframe->top(0); 
                           _new[1] = m_jframe->top(2);
                           _new[2] = m_jframe->top(0);
        }
        else {
            num_was = 3;   was[0] = m_jframe->top(2); 
                           was[1] = m_jframe->top(1); was[2] = m_jframe->top(0);
            num_new = 5;   
                _new[0] = m_jframe->top(1); _new[1] = m_jframe->top(0); 
                _new[2]=m_jframe->top(2); _new[3] = m_jframe->top(1);
                _new[4]=m_jframe->top(0);
        }
        break;
    case OPCODE_DUP2_X2:
        // [.. value2.64, value1.64] =>  [.. value1.64, value2.64, value1.64]
        // before: 4 slots ; after: 6 slots
        /*
        movsd xmm0, [esp]
        movsd xmm1, [esp+2*slotSize]
        sub esp, 2*slotSize
        movsd [esp+4*slotSize], xmm0
        movsd [esp+2*slotSize], xmm1
        movsd [esp], xmm0
        */
        totalSlots = 6;
        num_added = 2; added[0] = 1; added[1] = 0;
        //f1 .., value4, value3, value2, value1  ..., value2, value1, value4, 
        // value3, value2, value1
        //f2 .., value3, value2, value1 ..., value1, value3, value2, value1
        //f3 .., value3, value2, value1 ..., value2, value1, value3, value2,
        // value1
        //f4 .., value2, value1 ..., value1, value2, value1
        if (is_wide(m_jframe->top())) {
            // either f4 or f2
            if (is_wide(m_jframe->top(2))) {
                // f4
                num_was = 2;    was[0] = m_jframe->top(2);
                                was[1] = m_jframe->top(0);
                num_new = 3;    _new[0] = was[1]; _new[1] = was[0];
                                 _new[2] = was[1];
            }
            else {
                // f2
                num_was = 3;    was[0] = m_jframe->top(3);
                                was[1] = m_jframe->top(2);
                                was[2] = m_jframe->top(0);
                num_new = 4;    _new[0] = was[2];  _new[1] = was[0];
                                _new[2] = was[1]; _new[3] = was[2];
            }
        }
        else {
            if (is_wide(m_jframe->top(2))) {
                // f3
                num_was = 3;    was[0] = m_jframe->top(2);   
                                was[1] = m_jframe->top(1); 
                                was[2] = m_jframe->top(0);
                num_new = 5;    _new[0] = was[1];  _new[1] = was[2]; 
                                _new[2] = was[0]; _new[3] = was[1];
                                _new[4] = was[2];
            }
            else {
                // f1
                num_was = 4;    
                    was[0] = m_jframe->top(3);   was[1] = m_jframe->top(2);
                    was[2] = m_jframe->top(1); was[3] = m_jframe->top(0);
                num_new = 6;    
                _new[0] = m_jframe->top(1);  _new[1] = m_jframe->top(0); 
                _new[2] = m_jframe->top(3); _new[3] = m_jframe->top(2);
                _new[4] = m_jframe->top(1); _new[5] = m_jframe->top(0);
            }
        }
        break;
    case OPCODE_SWAP:
        {
        jtype j0 = m_jframe->top();
        jtype j1 = m_jframe->top(1);
        // The further pop/push wipes out the correct state, as 
        // JFrame::push presumes that items go to registers, so we 
        // need make sure that both the items are indeed on registers 
        // before any action.
        RegName r0 = vstack(0);
        RegName r1 = vstack(1);

        vpop(j0);
        vpop(j1);
        vpush(j0, false);
        vpush(j1, true);
        if (is_f(j0) != is_f(j1)) {
            // they are on the different vstacks, 
            // thus they're already on different regs -
            // nothing to do
        }
        else if (is_f(j0)) {
            EncoderBase::Operand scratch = vlocal(j0, -1, false, true);
            voper(typeInfo[j0].mov, scratch, r0);
            voper(typeInfo[j0].mov, r0, r1);
            voper(typeInfo[j0].mov, r1, scratch);
        }
        else {
            voper(Mnemonic_XCHG, r0, r1);
        }
        }
        break;
    default:
        assert(false);
    }
    
    if (totalSlots) {
        voper(Mnemonic_SUB, RegName_ESP, MK_IMM32(num_added*STACK_SLOT_SIZE));
        gen_stack_to_args(true, num_was, was);
        const EncoderBase::Operand rscratch = vlocal(i32, -1, false, true);
        for (unsigned i=0; i<num_added; i++) {
            unsigned slot2load = added[i];
            EncoderBase::Operand slot(OpndSize_32, RegName_ESP, 
                                      slot2load*STACK_SLOT_SIZE);
            EncoderBase::Operand new_slot(OpndSize_32, RegName_ESP,
                                          (totalSlots-i-1)*STACK_SLOT_SIZE);
            voper(Mnemonic_MOV, rscratch, slot);
            voper(Mnemonic_MOV, new_slot, rscratch);
        }
        unsigned slot_off = totalSlots-1;
        for (unsigned i=0; i<num_new; i++) {
            jtype jt = _new[i];
            vpush(jt, true);
            if (jt == dbl64) {
                slot_off -= 1;
            }
            EncoderBase::Operands args;
            if (jt!=i64) {
                args.add(EncoderBase::Operand(vstack(0)));
                args.add(EncoderBase::Operand(typeInfo[jt].size, RegName_ESP,
                                              slot_off*STACK_SLOT_SIZE));
                voper(typeInfo[jt].mov, args);
            }
            else {
                args.add(EncoderBase::Operand(vstack(1)));
                args.add(EncoderBase::Operand(typeInfo[jt].size, RegName_ESP,
                                              slot_off*STACK_SLOT_SIZE));
                voper(typeInfo[jt].mov, args);
                args.clear();

                --slot_off;

                args.add(EncoderBase::Operand(vstack(0)));
                args.add(EncoderBase::Operand(typeInfo[jt].size, RegName_ESP,
                                              slot_off*STACK_SLOT_SIZE));
                voper(typeInfo[jt].mov, args);
            }
            slot_off--;
        }
        voper(Mnemonic_ADD, RegName_ESP, MK_IMM32(totalSlots*STACK_SLOT_SIZE));
    }

#ifdef _DEBUG
    assert(m_jframe->size() == control.size());
    for (unsigned i = 0; i < control.size(); i++) {
        assert(m_jframe->at(i).jt == control.at(i).jt);
    }
#endif
}

void Compiler::gen_st(jtype jt, unsigned idx)
{
    gen_gc_mark_local(idx, jt == jobj);
    // the presumption is that if're defining a var, then it will
    // be used soon. the presumption may be wrong and need to be checked  -
    // todo. for now, always requesting a register
    if (is_big(jt)) {
        RegName r0 = vstack(0);
        RegName r1 = vstack(1);
        m_jframe->st(jt, idx);
        vsync();
        const EncoderBase::Operand vlo = vlocal(jt, idx, false, true);
        vsync();
        voper(typeInfo[jt].mov, vlo, r0);
        
        ++idx;
        const EncoderBase::Operand vhi = vlocal(jt, idx, true, true);
        vsync();
        voper(typeInfo[jt].mov, vhi, r1);
    }
    else {
        RegName r = vstack(0);
        m_jframe->st(jt, idx);
        vsync();
        const EncoderBase::Operand v = vlocal(jt, idx, false, true);
        vsync();
        voper(typeInfo[jt].mov, v, r);
    }
}

void Compiler::gen_ld(jtype jt, unsigned idx)
{
    m_jframe->ld(jt,idx);
    vsync();
    
    RegName r = vstack(0);
    const EncoderBase::Operand v = vlocal(jt, idx, false, false);
    assert(m_jframe->need_update() == 0);
    voper(typeInfo[jt].mov, r, v);
    
    if (is_big(jt)) {
        const EncoderBase::Operand vhi = vlocal(jt, idx+1, true, false);
        voper(typeInfo[jt].mov, vstack(1), vhi);
    }
}

void Compiler::gen_save_ret(jtype jt)
{
    assert(jt != jvoid);
    vpush(jt, true);
    if (is_f(jt)) {
        const bool is_dbl = jt == dbl64;
        voper(Mnemonic_FSTP, vmstack(0), is_dbl ? RegName_FP0D : RegName_FP0S);
        m_jframe->stack_state(0, SS_SWAPPED, 0);
    }
    else {
        if (jt==i8 || jt==i16) {
            voper(Mnemonic_MOVSX, vstack(0), valias(jt, RegName_EAX));
        }
        else if (jt==u16) {
            voper(Mnemonic_MOVZX, vstack(0), valias(jt, RegName_EAX));
        }
        else {
            voper(Mnemonic_MOV, vstack(0), RegName_EAX);
        }
        if (is_big(jt)) {
            voper(Mnemonic_MOV, vstack(1), RegName_EDX);
        }
    }
}

void Compiler::gen_invoke(JavaByteCodes opcod, Method_Handle meth,
                          const ::std::vector<jtype> &args, jtype retType)
{

    const unsigned slots = count_slots(args);
    // where (stack depth) 'this' is stored for the method being invoked 
    // (if applicable)
    const unsigned thiz_depth = slots - 1;

    const JInst& jinst = *m_curr_inst;

	if (args.size() != 0) {
		gen_stack_to_args(true, args.size(), &args[0]);
	}
    gen_gc_stack(-1, true);
    gen_mem(MEM_TO_MEM|MEM_STACK|MEM_VARS|MEM_UPDATE);

    const bool lazy = m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION;
    
    veax(); // allocate/free EAX
    
    if (lazy) {
        gen_lazy_resolve(m_curr_inst->op0, opcod);
    }
    else if (meth == NULL) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0, 
                       jinst.opcode);
    }
    if (opcod == OPCODE_INVOKEINTERFACE) {
        if (lazy) {
            vedx();
            voper(Mnemonic_PUSH, RegName_EAX);
            voper(Mnemonic_PUSH, RegName_EAX);
            // stack: [..., meth, meth]
            vcall((void*)&method_get_offset);
            // stack: [..., meth, meth] ; EAX = meth_offset
            voper(Mnemonic_MOV, Stack32_1, RegName_EAX);
            // stack: [..., method_offset, meth]
            vcall((void*)&method_get_class);
            //
            voper(Mnemonic_PUSH, RegName_EAX);
            //stack: [..., method_offset, meth, klass]
            vcall((void*)&class_property_is_interface2);
            voper(Mnemonic_TEST, RegName_EAX, RegName_EAX);
            voper(Mnemonic_POP, RegName_EAX);
            // stack: [..., method_offset, meth] ; EAX=klass
            voper(Mnemonic_MOV, StackTop32, RegName_EAX);
            // stack: [..., method_offset, klass]
            //
            // [taken] JNZ - go to regular INVOKEINTERFACE
            // else - switch to INVOKEVIRTUAL.
            // See handle_ik_meth() and OPCODE_INVOKEINTERFACE for this 
            // magic transmutation.
            //
            unsigned pid = vjcc(ConditionMnemonic_NZ, InstPrefix_HintTaken);

            voper(Mnemonic_POP, RegName_EAX);   // pop out 'klass'
            // stack: [..., method_offset]
            
            // '+1' here due to 'method_offset' 
            const EncoderBase::Operand thiz0(OpndSize_32, RegName_ESP, 
                (thiz_depth+1)*STACK_SLOT_SIZE);
            
            voper(Mnemonic_MOV, RegName_EAX, thiz0);
            /* mov eax, [eax]     */
            voper(Mnemonic_MOV, RegName_EAX, MK_MEM32(RegName_EAX, 0));
                        
            unsigned pid2 = vjmp8(); // jump to 'ADD eax, [esp]/CALL [eax]'
            
            patch_set_target(pid, ip()); // connect 'pid' to here - the 
                                         // regular INVOKEINTERFACE 
                                         // processing
            // '+2' here due to 'method_offset' & 'klass'
            const EncoderBase::Operand thiz(OpndSize_32, RegName_ESP, 
                (thiz_depth+2)*STACK_SLOT_SIZE);
            voper(Mnemonic_PUSH, thiz);
            // stack: [..., method_offset, klass, thiz]
            vcall(rt_helper_get_vtable);
            // stack: [..., method_offset] ; EAX = vtbl
            patch_set_target(pid2, ip());
            voper(Mnemonic_ADD, RegName_EAX, StackTop32);
            voper(Mnemonic_ADD, RegName_ESP, MK_IMM32(STACK_SLOT_SIZE));
            voper(Mnemonic_CALL, 
                  EncoderBase::Operand(OpndSize_32, RegName_EAX, 0));
        }
        else if (meth != NULL) {
            // if it's invokeinterface, then first resolve it
            Class_Handle klass = method_get_class(meth);
            /*push klass */ voper(Mnemonic_PUSH, MK_IMM32(klass));
            const EncoderBase::Operand thiz(OpndSize_32, RegName_ESP, 
                (thiz_depth+1)*STACK_SLOT_SIZE);
            voper(Mnemonic_PUSH, thiz);
            /*call helper_get_vtable */
            gen_call_vm(rt_helper_get_vtable, 0);
            //
            // Method's vtable is in EAX now, stack is ready
            //
            // call [eax + offset-of-the-method] - eax is from the helper
            unsigned offset = method_get_offset(meth);
            voper(Mnemonic_CALL, MK_MEM32(RegName_EAX, offset));
        }
    }
    else if (opcod == OPCODE_INVOKEVIRTUAL) {
        /* mov eax, [obj ref] */
        const EncoderBase::Operand thiz(OpndSize_32, RegName_ESP, 
            thiz_depth*STACK_SLOT_SIZE);
        if (lazy) {
            vedx(); // allocate/free EDX
            voper(Mnemonic_PUSH, RegName_EAX);
            // stack: [..., meth]
            vcall((void*)&method_get_offset);
            // stack: [..., meth] ; EAX = meth_offset
            voper(Mnemonic_ADD, RegName_ESP, MK_IMM32(STACK_SLOT_SIZE));
            // stack: [<args>, ] ; EAX = meth_offset
            voper(Mnemonic_MOV, RegName_EDX, thiz);
            voper(Mnemonic_MOV, RegName_EDX,
                  EncoderBase::Operand(OpndSize_32, RegName_EDX, 0));
            voper(Mnemonic_ADD, RegName_EAX, RegName_EDX);
            voper(Mnemonic_CALL, MK_MEM32(RegName_EAX, 0));
        }
        else if (meth != NULL) {
            voper(Mnemonic_MOV, RegName_EAX, thiz);
            /* mov eax, [eax]     */
            voper(Mnemonic_MOV, RegName_EAX, MK_MEM32(RegName_EAX,0));
            /* call [eax+offset]*/
            unsigned offset = method_get_offset(meth);
            voper(Mnemonic_CALL, MK_MEM32(RegName_EAX, offset));
        }
    }
    else {
        //
        // invoke special & static
        //
        if (lazy) {
            voper(Mnemonic_CALL, MK_MEM32(RegName_EAX, 0));
            //voper(Mnemonic_CALL, MK_MEM32(RegName_EAX, 0));
        }
        else if (meth != NULL) {
            void *ppmeth = method_get_indirect_address(meth);
            voper(Mnemonic_CALL, MK_MEM32(RegName_Null, (int)ppmeth));
        }
    }
    if (retType != jvoid) {
        gen_save_ret(retType);
    }
}

void Compiler::gen_if(JavaByteCodes opcod, unsigned target)
{
    if (m_next_bb_is_multiref) {
        gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
    }
    if (target < m_pc) {
        // have back branch here
        if (m_infoBlock.get_flags() & JMF_BBPOOLING) {
            gen_gc_safe_point();
        }
        if (m_infoBlock.get_flags() & JMF_PROF_ENTRY_BE) {
            voper(Mnemonic_INC, MK_MEM32(RegName_Null, (int)m_p_backedge_counter));
        }
    }
    if (opcod == OPCODE_IFNULL) {
        opcod = OPCODE_IFEQ;
    }
    else if (opcod == OPCODE_IFNONNULL) {
        opcod = OPCODE_IFNE;
    }
    assert(m_jframe->dip(0).jt == i32 || m_jframe->dip(0).jt == jobj);
    
    //vvv an order matters !
    RegName r0 = vstack(0);
    vpop(m_jframe->top());
    //^^^
    ConditionMnemonic cond;
    if (opcod == OPCODE_IFEQ || opcod == OPCODE_IFNE) {
        voper(Mnemonic_TEST, r0, r0);
        cond = opcod == OPCODE_IFEQ ? 
                            ConditionMnemonic_Z : ConditionMnemonic_NZ;
    }
    else {
        voper(Mnemonic_CMP, r0, EncoderBase::Operand(OpndSize_32, 0));
        cond = map_cc(opcod);
    }
    if (m_next_bb_is_multiref) {
        gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
    }
    vjcc(target, cond, InstPrefix_Null);
};

void Compiler::gen_if_icmp(JavaByteCodes opcod, unsigned target)
{
    if (m_next_bb_is_multiref) {
        gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
    }

    if (target < m_pc) {
        // have back branch here
        if (m_infoBlock.get_flags() & JMF_BBPOOLING) {
            gen_gc_safe_point();
        }
        if (m_infoBlock.get_flags() & JMF_PROF_ENTRY_BE) {
            voper(Mnemonic_INC, MK_MEM32(RegName_Null, (int)m_p_backedge_counter));
        }
    }

    if (opcod == OPCODE_IF_ACMPEQ) {
        opcod = OPCODE_IF_ICMPEQ;
    }
    else if (opcod == OPCODE_IF_ACMPNE) {
        opcod = OPCODE_IF_ICMPNE;
    }

    assert(m_jframe->dip(0).jt == i32 || m_jframe->dip(0).jt == jobj);
    assert(m_jframe->dip(1).jt == i32 || m_jframe->dip(1).jt == jobj);

    RegName r0 = vstack(0);
    RegName r1 = vstack(1);
    voper(Mnemonic_CMP, r1, r0);
    vpop(m_jframe->top());
    vpop(m_jframe->top());
    if (m_next_bb_is_multiref) {
        gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
    }
    vjcc(target, map_cc(opcod), InstPrefix_Null);
};

void Compiler::gen_goto(unsigned target)
{
    if (m_next_bb_is_multiref) {
        gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
        gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
    }
    if (target < m_pc) {
        // Back branch
        if (m_infoBlock.get_flags() & JMF_PROF_ENTRY_BE) {
            voper(Mnemonic_INC,
                  MK_MEM32(RegName_Null, (int)m_p_backedge_counter));
        }
        if (m_infoBlock.get_flags() & JMF_BBPOOLING) {
            gen_gc_safe_point();
        }
    }
    vjmp(target);
};

void Compiler::gen_gc_safe_point()
{
    if (m_curr_bb_state->seen_gcpt) {
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg(";; > skipping gc_safe_pt due to known GC point before\n");
        }
        return;
    }
    m_curr_bb_state->seen_gcpt = true;

    veax();
#ifdef PLATFORM_POSIX
    gen_call_vm(rt_helper_get_thread_suspend_ptr, 0);
    voper(Mnemonic_CMP, MK_MEM32(RegName_EAX, 0), Imm32_0);
#else
    // This is a bit quicker, but tricky way - VM uses TIB to store 
    // thread-specific info, and the offset of suspend request flag is 
    // known, so we may directly get the flag without a need to call VM. 
    // On Linux, they [perhaps] use GS:[], however this depends on kernel
    // version and is not guaranteed, so on Linux we call VM to obtain the 
    // flags' address.
    vprefix(InstPrefix_FS);
    voper(Mnemonic_MOV, RegName_EAX, MK_MEM32(RegName_Null, 0x14));
    voper(Mnemonic_MOV, RegName_EAX, 
                        MK_MEM32(RegName_EAX, rt_suspend_req_flag_offset));
    voper(Mnemonic_TEST, RegName_EAX, RegName_EAX);
#endif
    unsigned patch_id = vjcc(ConditionMnemonic_E, InstPrefix_HintTaken);
    //
    gen_mem(MEM_TO_MEM|MEM_VARS|MEM_STACK|MEM_NO_UPDATE);
    gen_gc_stack(-1, false);
    vcall(rt_helper_gc_safepoint);
    gen_mem(MEM_FROM_MEM|MEM_VARS|MEM_STACK|MEM_NO_UPDATE|MEM_INVERSE);
    patch_set_target(patch_id, ip());
}

void Compiler::gen_jsr(unsigned target)
{
    // must be last instruction in BB
    assert(m_pc == m_curr_bb->last_pc);
    vpush(jretAddr, true);
    // Always unload vars for JSR.
    // todo: actually, we don't know the state *after* the JSR
    // so can omit it here, but clear vars state later - after JSR
    gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
    const BBInfo& jsrBlock = m_bbs[target];
    if (jsrBlock.ref_count > 1) {
        // Several JSR call this block, need to sync stack
        gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
    }
    // Load address of next BB onto operand stack ..
    reg_patch(m_curr_bb->next_bb, false);
    voper(Mnemonic_MOV, vstack(0), Imm32_0);
    // .. and jump to JSR block
    vjmp(target);
}

void Compiler::gen_ret(unsigned idx)
{
    gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
    gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
    voper(Mnemonic_JMP, vlocal(jretAddr, idx, false, false));
};

void Compiler::gen_array_length(void)
{
    //      i0 = istack(0);
    //      mov i0, [i0 + rt_array_length_offset]
    RegName r0 = vstack(0);
    voper(Mnemonic_MOV, r0, 
          EncoderBase::Operand(OpndSize_32, r0, rt_array_length_offset));
    vpop(jobj);
    vpush(i32, true);
    // the array length is known to be non-negative, marking it as such
    m_jframe->stack_attrs(0, SA_NZ);
}

void Compiler::gen_check_null(unsigned depth)
{
    const JFrame::Slot s0 = m_jframe->dip(depth);
    assert(s0.jt == jobj);
    if (s0.attrs() & SA_NZ) {
        STATS_INC(Stats::npesEliminated,1);
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg(";;> skipping a null check @ depth=%d, due"
                " to a known non-null attr\n", depth);
        }
        return;
    }

    STATS_INC(Stats::npesPerformed,1);
    
    if (s0.swapped()) {
        voper(Mnemonic_TEST, vmstack(depth), MK_IMM32(0xFFFFFFFF));
    }
    else {
        RegName r0 = vstack(depth);
        voper(Mnemonic_TEST, r0, r0);
    }
    unsigned pid = vjcc(ConditionMnemonic_NZ, InstPrefix_HintTaken);
    gen_call_throw(rt_helper_throw_npe, 0);
    patch_set_target(pid, ip());
    m_jframe->stack_attrs(depth, SA_NZ);
}

void Compiler::gen_check_bounds(unsigned aref_depth, unsigned index_depth)
{
    // simply requesting registers here (i.e. for LALOAD) implies 
    // STACK_REGS >= 3. The check can be rewritten to support less 
    // stack_regs, but not now. may be later. todo ?
    
    //const R_Opnd&  rref = istack(aref_depth);
    //const R_Opnd&  ridx = istack(index_depth);
    //M_Opnd plen(rref.reg_no(), rt_array_length_offset);
    //ip( alu( ip(), cmp_opc, plen, ridx) );
    
    RegName rref = vstack(aref_depth);
    RegName ridx = vstack(index_depth);
    EncoderBase::Operand plen(OpndSize_32, rref, rt_array_length_offset);
    voper(Mnemonic_CMP, ridx, plen);    
    /*
        cmp [istack(aref)+rt_array_length_offset], istack(index)
        jl  good_1
            out-of-bounds
    good_1:
        cmp istack(infex), 0
        jge  good_2
            negative-array-index
    */
    //ip( mov( ip(), rs, plen ) );
    unsigned good_1 = vjcc(ConditionMnemonic_L, InstPrefix_Null);
    gen_call_throw(rt_helper_throw_out_of_bounds, 0);
    // good_1:
    patch_set_target(good_1, ip());
    const JFrame::Slot& sidx = m_jframe->dip(index_depth);
    // TODO: seems we can eliminate the <0 check completely ?
    if (sidx.attrs() & SA_NOT_NEG) {
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg(";;> skipping <0 check @ depth=%d, due to"
                " known non-negative attr\n", index_depth);
        }
    }
    else {
        voper(Mnemonic_CMP, ridx, Imm32_0);
        unsigned good_2 = vjcc(ConditionMnemonic_GE, InstPrefix_Null);
        gen_call_throw(rt_helper_throw_out_of_bounds, 0);
        // good_2:
        patch_set_target(good_2, ip());
    }
}

void Compiler::gen_check_div_by_zero(jtype jt, 
                                     unsigned stack_depth_of_divizor)
{
    // for both must sit on a register, which implies I_STACK_REGS >= 4
    assert(m_jframe->regable(stack_depth_of_divizor));
    RegName r0 = vstack(0);
    if (jt == i32) {
        /*test dvsr, dvsr */ voper(Mnemonic_TEST, r0, r0);
        /*[hint.taken] jnz not_zero */
        unsigned pid = vjcc(ConditionMnemonic_NE, InstPrefix_HintTaken);
        /*call throw div-by-zero */
        gen_call_throw(rt_helper_throw_div_by_zero_exc, 0);
        patch_set_target(pid, ip());
        return;
    }
    assert(jt == i64);
    /* or dvsr.l32, dvsr.l32
        [no hints] jnz not_zero
        or dvsr.h32, dvsr.l32        ; as dvsr.l32 is zero at this point, no
                                     ; need to check against immediate
        [hint.taken] jnz not_zero
        call throw DIV-BY-ZERO
        not_zero:
        */
    // i0 currently hold dvsr.l32
    /*test dvsr, dvsr */ voper(Mnemonic_TEST, r0, r0);
    unsigned pid = vjcc(ConditionMnemonic_NE, InstPrefix_Null);
    /*
    unsigned h32_depth = stack_depth_of_divizor + 1;
    // here is OR, not TEST !
    if (stack_on_mem(h32_depth)) {
        ip(alu(ip(), or_opc, mstack(h32_depth), i0));
    }
    else {
        ip(alu(ip(), or_opc, istack(h32_depth), i0));
    }
    */
    RegName r1 = vstack(stack_depth_of_divizor + 1);
    voper(Mnemonic_TEST, r1, r1);
    unsigned pid2 = vjcc(ConditionMnemonic_NE, InstPrefix_HintTaken);
    /*call throw div-by-zero */
    gen_call_throw(rt_helper_throw_div_by_zero_exc, 0);
    // connect both branches to here
    patch_set_target(pid, ip());
    patch_set_target(pid2, ip());
}


void Compiler::gen_aload(jtype jt)
{
    /*
       stack: [... array_ref, index]
       pop edx              ; edx <- index
       pop eax             ; eax <- ref
       the target is [eax+edx*size + offset_of_1st_element]
       mov/movsx/movzx eax, target

     */
    // i1 = aref ; i0 = idx
    // item is at [i1 + i0*sizeof(elem) + offset_of_first_elem]
    //const R_Opnd& i0_tmp = istack(0);
    //const R_Opnd& i1_tmp = istack(1);
    //M_Index_Opnd pitem(i1_tmp.reg_no(), i0_tmp.reg_no(), 
    // jtypes[jt].rt_offset, jtypes[jt].size);
    
    RegName ridx = vstack(0);
    RegName rbase = vstack(1);
    vpop(i32);
    vpop(jobj);
    vpush(jt, true);
    
    EncoderBase::Operand pitem(typeInfo[jt].size, rbase, ridx,
                               jtypes[jt].size, jtypes[jt].rt_offset);
    if (jt == u16 || jt==i8 || jt==i16) {
        voper(jt == u16 ? Mnemonic_MOVZX : Mnemonic_MOVSX, vstack(0), pitem);
    }
    else if (jt==i64) {
        // cant do two simple MOVes here - one will overwrite base/index 
        // register
        // PUSH hi32
       // '4' here as a sizeof(i64)/2 - we're fetching out the high part
       // of the i64 item
        EncoderBase::Operand phi(typeInfo[jt].size, rbase, ridx, 
                                 jtypes[jt].size, jtypes[jt].rt_offset+4);
        voper(Mnemonic_PUSH, phi);
        // MOV stack(0), lo32
        voper(typeInfo[jt].mov, vstack(0), pitem);
        // POP hi32->stack(1)
        voper(Mnemonic_POP, vstack(1));
    }
    else {
        voper(typeInfo[jt].mov, vstack(0), pitem);
    }
}

void Compiler::gen_astore(jtype jt)
{
    // the stack is: [... array_ref, index, value]
    if (jt == jobj) {
        static const jtype args[3] = {jobj, i32, jobj};
        gen_stack_to_args(true, 3, args);
        gen_call_vm(rt_helper_aastore, 0);
        return;
    }
    unsigned idx_depth = is_wide(jt) ? 2 : 1;
    RegName ridx = vstack(idx_depth);
    RegName rref = vstack(idx_depth+1);
    EncoderBase::Operand pitem(typeInfo[jt].size, rref, ridx, 
                               jtypes[jt].size, jtypes[jt].rt_offset);
    RegName r0 = vstack(0);
    if (jt<i32) {
        r0 = valias(jt,r0);        
    }
    voper(typeInfo[jt].mov, pitem,r0);
    assert( is_big(i64) );
    if (jt == i64) {
        // '4' here as a sizeof(i64)/2 - we're storing out the high part of
        // the i64 item
        EncoderBase::Operand phi(typeInfo[jt].size, rref, ridx, 
                                 jtypes[jt].size, jtypes[jt].rt_offset+4);
        voper(Mnemonic_MOV, phi, vstack(1));
    }
    vpop(jt);
    vpop(i32);
    vpop(jobj);
    vsync();
}

void Compiler::gen_patch(const char *codeBlock, const CodePatchItem & cpi)
{
    
    //TODO: a bit sophisticated routine. need improvements.
    
    assert(cpi.instr_len == 7 || cpi.instr_len == 6 || cpi.instr_len == 5 || 
           cpi.instr_len == 2 || cpi.instr_len == 10);
    char *instr_ip = cpi.ip;
    const unsigned char inst_1st_byte = *(unsigned char*)instr_ip;
    
    if (cpi.instr_len == 2) {
        // [currently] the only case:
        // 2 bytes instructions used for relative conditional jmps
        assert(cpi.target_ip == NULL && cpi.target_pc == NOTHING);
        // useless jmp ?
        assert(cpi.target_offset != 0);
        int offset = cpi.target_offset - cpi.instr_len;
        // is in range ?
        assert(CHAR_MIN <= offset && offset <= CHAR_MAX);

        char *addr_ip = instr_ip + 1;   // 1 byte for opcode
        if (cpi.instr_len == 3) {
            ++addr_ip;      // +1 byte for hint
        }
        // well, the target_offset already takes care about the Jcc 
        // instruction length
        *(char *) (addr_ip) = (char)offset;

        if (m_infoBlock.get_flags() & DBG_TRACE_LAYOUT) {
            dbg("code.patch @ %p, offset=%d (=>%p)\n", instr_ip,
                cpi.target_offset,
                instr_ip + cpi.target_offset + cpi.instr_len);
        }
        return;
    }
    // for 5-bytes instruction, the data is normally one byte ahead of 
    // the instruction byte
    char *addr_ip = instr_ip + 1;
    if (cpi.instr_len == 6) {
        ++addr_ip;
        assert(inst_1st_byte  == 0x0F ||    // prefix + smth
               inst_1st_byte == 0x89 ||     // MOV
               inst_1st_byte == 0x87 ||     // XCHG
               inst_1st_byte == 0x8B ||
               inst_1st_byte == 0xFF        // mostly PUSH [mem]
               );      // MOV reg, addr
    }
    else if (cpi.instr_len == 7) {
        ++addr_ip;
        ++addr_ip;
        if (inst_1st_byte == 0xFF) {
            // the only case - JMP [table]
            assert(cpi.data_addr != NULL);
        }
        else {
            // the only case is hint + Jcc imm32
            assert(inst_1st_byte == 0x2E || inst_1st_byte==0x3E);
        }
    }
    
    int offset;
    if (cpi.target_pc != NOTHING) {
        const char *target_ip = m_infoBlock.get_ip(cpi.target_pc);
        offset = (int)target_ip;
        if (cpi.relative) {
            offset -=  (int)(instr_ip + cpi.instr_len);
        }
    }
    else {
        if (cpi.data_addr != NULL) {
            offset = (unsigned)cpi.data_addr;
            // check that all variants, not only JMP [table] were handled
            assert(addr_ip != instr_ip);
            //addr_ip = instr_ip + (cpi.instr_len == 5 ? 1 : 2);
        }
        else if (cpi.instr_len == 6 && 
            (0x89 == inst_1st_byte || 0x87 == inst_1st_byte)) {
            offset = (int)instr_ip + cpi.target_offset + 1;
        }
        else if (cpi.instr_len == 7) {
            assert(cpi.target_offset != 0); // useless jmp ?
            offset = cpi.target_offset - cpi.instr_len;
        }
        else {
            if (cpi.target_ip != NULL) {
                offset = cpi.target_ip - instr_ip - cpi.instr_len;
            }
            else {
                assert(cpi.target_offset != 0); // useless jmp ?
                offset = cpi.target_offset - cpi.instr_len;
            }
        }
    }
    *(int *)(addr_ip) = offset;
    if (m_infoBlock.get_flags() & DBG_TRACE_LAYOUT) {
        dbg("code.patch @ %p/bb=%u, byte=%02X, offset=%d (=>%p)\n", 
            instr_ip, cpi.bb,
            (unsigned)inst_1st_byte, offset, offset);
    }
}

void Compiler::gen_iinc(unsigned idx, int value)
{
    m_jframe->var_def(i32, idx, 0);
    //const JFrame::Slot& v = m_jframe->var(idx);
    EncoderBase::Operand imm_val(OpndSize_32, value);
    if (!vlocal_on_mem(idx)) {
        voper(Mnemonic_ADD, vlocal(i32, idx, false, true), imm_val);
    }
    else {
        /* add [local_idx], value */
        voper(Mnemonic_ADD, vmlocal(i32,idx), imm_val);
    }
}

void Compiler::gen_a(JavaByteCodes op, jtype jt)
{
    bool shft = op == OPCODE_ISHL || op == OPCODE_ISHR || op == OPCODE_IUSHR;
    if (jt == i32) {
        gen_a_i32(op);
        return;
    }
    if (jt == i64 && !shft && 
            (op == OPCODE_IADD || op == OPCODE_ISUB || op == OPCODE_IOR ||
             op == OPCODE_IXOR || op == OPCODE_IAND || op == OPCODE_INEG)) {

        if (op == OPCODE_INEG) {
            RegName r0_lo = vstack(0);
            RegName r0_hi = vstack(1);
            /* neg lo32    */ voper(Mnemonic_NEG, r0_lo);
            /* adc hi32, 0 */ voper(Mnemonic_ADC, r0_hi, Imm32_0);
            /* neg hi32    */ voper(Mnemonic_NEG, r0_hi);
            vpop(jt);
            vpush(jt, true);
            return;
        }
        RegName v1_hi = vstack(3);
        RegName v1_lo = vstack(2);
        RegName v2_hi = vstack(1);
        RegName v2_lo = vstack(0);
        if (op == OPCODE_IADD) {
            /* add stack_0, eax */ voper(Mnemonic_ADD, v1_lo, v2_lo);
            /* adc stack_1, edx */ voper(Mnemonic_ADC, v1_hi, v2_hi);
        }
        else if (op == OPCODE_ISUB) {
            /* sub stack_0, eax */ voper(Mnemonic_SUB, v1_lo, v2_lo);
            /* sbb stack_1, edx */ voper(Mnemonic_SBB, v1_hi, v2_hi);
        }
        else if (op == OPCODE_IOR) {
            /* or  stack_0, eax */ voper(Mnemonic_OR, v1_lo, v2_lo);
            /* or  stack_1, edx */ voper(Mnemonic_OR, v1_hi, v2_hi);
        }
        else if (op == OPCODE_IXOR) {
            /* xor stack_0, eax */ voper(Mnemonic_XOR, v1_lo, v2_lo);
            /* xor stack_1, edx */ voper(Mnemonic_XOR, v1_hi, v2_hi);
        }
        else if (op == OPCODE_IAND) {
            /* and stack_0, eax */ voper(Mnemonic_AND, v1_lo, v2_lo);
            /* and stack_1, edx */ voper(Mnemonic_AND, v1_hi, v2_hi);
        }
        else {
            assert(false);
        }
        vpop(jt);
        vpop(jt);
        vpush(jt, true);
        return;
    }

    if (is_f(jt) && 
            (op == OPCODE_IADD || op == OPCODE_ISUB || 
            op == OPCODE_IMUL ||  op == OPCODE_IDIV ||
            op == OPCODE_INEG )) {
        bool is_dbl = jt == dbl64;
        if (op == OPCODE_INEG) {
            vstack_swap(0);
            RegName freg = is_dbl ? RegName_FP0D : RegName_FP0S;
            voper(Mnemonic_FLD, freg, vmstack(0));
            voper(Mnemonic_FCHS, freg);
            voper(Mnemonic_FSTP, vmstack(0), freg);
            return;
        }
        
        RegName v1 = vstack(is_dbl ? 2 : 1);
        RegName v2 = vstack(0);
        if (op == OPCODE_IADD) {
            voper(is_dbl ? Mnemonic_ADDSD : Mnemonic_ADDSS, v1, v2);
        }
        else if(op == OPCODE_ISUB) {
            voper(is_dbl ? Mnemonic_SUBSD : Mnemonic_SUBSS, v1, v2);
        }
        else if(op == OPCODE_IMUL) {
            voper(is_dbl ? Mnemonic_MULSD : Mnemonic_MULSS, v1, v2);
        }
        else if(op == OPCODE_IDIV) {
            voper(is_dbl ? Mnemonic_DIVSD : Mnemonic_DIVSS, v1, v2);
        }
        vpop(jt);
        vpop(jt);
        vpush(jt, true);
        return;
    }
    
    bool negop = op == OPCODE_INEG;
    char * helper = NULL;
    if( jt == i64 && shft )  {
        helper = (char*)rt_h_i64_shift;
        shft = true;
    }
    else if( jt == dbl64 ) {
        helper = negop ? (char*)&rt_h_neg_dbl64 : (char*)&rt_h_dbl_a;
    }
    else if( jt == flt32 ) {
        helper = negop ? (char*)&rt_h_neg_flt32 : (char*)&rt_h_flt_a;
    }
    else if( jt == i64 ) {
        helper = negop ? (char*)&rt_h_neg_i64 : (char*)&rt_h_i64_a;
    }
    else {
        assert( jt == i32 );
        helper = negop ? (char*)&rt_h_neg_i32 : (char*)&rt_h_i32_a;
    }
    if (negop) {
        const jtype args[1] = {jt};
        gen_stack_to_args(true, 1, args);
    }
    else if(shft) {
        const jtype args[2] = {jt, i32};
        gen_stack_to_args(true, 2, args);
    }
    else {
        const jtype args[2] = {jt,jt};
        gen_stack_to_args(true, 2, args);
    }
    if (negop) {
        gen_call_novm(helper, 0);
    }
    else {
        gen_call_novm(helper, 1, op);
    }
    gen_save_ret( jt );
}

void Compiler::gen_a_i32(JavaByteCodes op) {
    const jtype jt = i32;
    if (op == OPCODE_INEG) {
        RegName r0 = vstack(0);
        voper(Mnemonic_NEG, r0);
        vpop(jt);
        vpush(jt, true);
        assert(!m_jframe->need_update());
        return;
    }
    RegName v1 = vstack(1);
    RegName v2 = vstack(0);
    if (op == OPCODE_IADD) {
        voper(Mnemonic_ADD, v1, v2);
    }
    else if (op == OPCODE_ISUB) {
        voper(Mnemonic_SUB, v1, v2);
    }
    else if (op == OPCODE_IOR) {
        voper(Mnemonic_OR, v1, v2);
    }
    else if (op == OPCODE_IAND) {
        voper(Mnemonic_AND, v1, v2);
    }
    else if (op == OPCODE_IXOR) {
        voper(Mnemonic_XOR, v1, v2);
    }
    else if (op == OPCODE_IMUL) {
        voper(Mnemonic_IMUL, v1, v2);
    }
    else if (op == OPCODE_IDIV || op == OPCODE_IREM) {
        vedx();
        veax();
        // pseudo code:
        // IDIV:  return (v2 == -1 && v1 == INT_MIN) ? v1 : v1 / v2;
        // IREM:  return (v2 == -1 && v1 == INT_MIN) ? 0  : v1 % v2;
       /*
        CMP v2, -1
        JNE normal
        CMP v1, INT_MIN
        if (rem) {
            JE  cont
            MOV v1, 0
            JMP exit
        }
        else {
            JE exit         // leave INT_MIN in v1
        }
        cont:
        MOV  eax, v1
        CDQ
        IDIV v2
        MOV  v1, op == OPCODE_IDIV ? RegName_EAX : RegName_EDX
        exit:
        */
        voper(Mnemonic_CMP, v2, Imm32_m1);
        unsigned pid_normal = vjcc(ConditionMnemonic_NE);
        voper(Mnemonic_CMP, v1, MK_IMM32(INT_MIN));
        unsigned pid_cont = NOTHING, pid_exit = NOTHING;
        if (op == OPCODE_IREM) {
            pid_cont = vjcc(ConditionMnemonic_NE);
            voper(Mnemonic_XOR, v1, v1);
            pid_exit = vjmp8();
        }
        else {
            pid_exit = vjcc(ConditionMnemonic_E);
        }
        patch_set_target(pid_normal, ip());
        if (pid_cont != NOTHING) {
            patch_set_target(pid_cont, ip());
        }
        voper(Mnemonic_MOV, RegName_EAX, v1);
        voper(Mnemonic_CDQ, RegName_EDX, RegName_EAX);
        EncoderBase::Operands args(RegName_EDX, RegName_EAX, v2);
        voper(Mnemonic_IDIV, args);
        voper(Mnemonic_MOV, v1, op == OPCODE_IREM ? RegName_EDX : RegName_EAX);
        if (pid_exit != NOTHING) {
            patch_set_target(pid_exit, ip());
        }
    }
    else {
        assert(op == OPCODE_ISHL || op == OPCODE_ISHR || op == OPCODE_IUSHR);
        //
        //todo: might want to change XCHG to mov if there are free registers
        //todo: might want to track constants on the stack (need measurement)
        //
        /*a strategy:
        if (v1 is ECX)          {  xchg v1=ecx, v2 | shift v2, v1=ecx | 
                                  mov v1=ecx, v2 }
        else if( v2 is ECX)     { / * we are so lucky ! * /shift v1, v2=ecx }
        else if (is ecx free ?) { mov ecx, v2 | shift v2, ecx }
        else                    { xchg ecx, v2 | shift v1,ecx | mov ecx, v2 }
        */
        const Mnemonic shi = op == OPCODE_ISHL ? 
                            Mnemonic_SHL : 
                            (op==OPCODE_ISHR ? Mnemonic_SAR : Mnemonic_SHR);
        if (v2 == RegName_ECX) {
            voper(shi, v1, RegName_CL);
        }
        else if (v1 == RegName_ECX) {
            voper(Mnemonic_XCHG, RegName_ECX, v2);
            voper(shi, v2, RegName_CL);
            voper(Mnemonic_MOV, RegName_ECX, v2);
        }
        else {
            voper(Mnemonic_XCHG, RegName_ECX, v2);
            voper(shi, v1, RegName_CL);
            voper(Mnemonic_MOV, RegName_ECX, v2);
        }
    }
    vpop(jt);
    vpop(jt);
    vpush(jt, true);
}

void Compiler::gen_cnv(jtype from, jtype to)
{
    if (from == i32 && to != i64) {
        RegName i0 = vstack(0);
        vpop(from);
        vpush(to, true);
        if (to == u16 || to == i16 || to == i8) {
            voper(to == u16 ? Mnemonic_MOVZX : Mnemonic_MOVSX, i0, valias(to, i0));
        }
        else {
            assert(is_f(to));
            RegName f0 = vstack(0);
            voper(to == dbl64 ? Mnemonic_CVTSI2SD : Mnemonic_CVTSI2SS, f0, i0);
        }
    }
    else {    
        char *helper = (char *) cnv_matrix_impls[from][to];
        const jtype args[1] = { from };
        gen_stack_to_args(true, 1, args);
        gen_call_novm(helper, 0);
        gen_save_ret(to);
    }
}

void Compiler::gen_new_array(Allocation_Handle ah)
{
    const JInst& jinst = *m_curr_inst;
    assert(jinst.opcode == OPCODE_NEWARRAY || 
           jinst.opcode == OPCODE_ANEWARRAY);
    
    static const jtype args[1] = { i32 };
    gen_stack_to_args(true, 1, args);
    // Free EAX
    veax();
    const bool do_lazily = (m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION) && 
                           jinst.opcode == OPCODE_ANEWARRAY;

    if (jinst.opcode == OPCODE_NEWARRAY) {
        // it's unexpected that that something failed for a primitive type
        assert(ah != 0); 
    }
    
    if (do_lazily) {
        gen_lazy_resolve(jinst.op0, jinst.opcode);
        // EAX = allocation handle
        voper(Mnemonic_PUSH, RegName_EAX);
    }
    else if (ah == 0) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0,
                       jinst.opcode);
    }
    else {
        voper(Mnemonic_PUSH, MK_IMM32(ah));
    }
    gen_call_vm(rt_helper_new_array, 0);
    gen_save_ret(jobj);
    // the returned can not be null, marking as such.
    m_jframe->stack_attrs(0, SA_NZ);
}

void Compiler::gen_multianewarray(Class_Handle klass, unsigned num_dims)
{
    // stack: [..., count1, [...count N] ]
    // args: (klassHandle, num_dims, count_n, ... count_1)
    // native stack to be estblished:  ..., count_1, .. count_n, num_dims, 
    // klassHandle
    
    const JInst& jinst = *m_curr_inst;

    // note: need to restore the stack - the cdecl-like function
    jtype *arrArgs = (jtype *) alloca(num_dims * sizeof(unsigned));
    for (unsigned i = 0; i < num_dims; i++) {
        arrArgs[i] = i32;
    }
    gen_stack_to_args(true, num_dims, arrArgs);
    voper(Mnemonic_PUSH, MK_IMM32(num_dims));
    
    if (m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION) {
        gen_lazy_resolve(jinst.op0, jinst.opcode);
        // EAX = klass handle
        voper(Mnemonic_PUSH, RegName_EAX);
    }
    else if (klass == NULL) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0,
                       jinst.opcode);
    }
    else {
        voper(Mnemonic_PUSH, MK_IMM32(klass));
    }
    gen_call_vm(rt_helper_multinewarray, 0);
    // must restore the stack - the cdecl-like function
    voper(Mnemonic_ADD, RegName_ESP, MK_IMM32(STACK_SLOT_SIZE*(num_dims+2)));
    gen_save_ret(jobj);
}

void Compiler::gen_x_cmp(JavaByteCodes op, jtype jt)
{
    if (is_f(jt)) {
        const bool is_dbl = jt == dbl64;
        RegName v1 = vstack(is_dbl ? 2 : 1);
        RegName v2 = vstack(0);
        bool is_g = (op == OPCODE_FCMPG || op == OPCODE_DCMPG);
        /*
            C-kode: 
            fcmp_g: if( _isnan(v1) || _isnan(v2) ) { return 1; }
            fcmp_l: if( _isnan(v1) || _isnan(v2) ) { return -1; }
            if( v1 > v2 )        return 1;
            if( v1 < v2 )        return -1;
            return 0;
            */
        vpop(jt);
        vpop(jt);
        vpush(i32, true);
        RegName i0 = vstack(0);
        /*ASM:
            xor          i0, i0
            comiss       v1, v2
            cmova        i0, 1   ; CF=0 && ZF=0
            cmovb        i0, -1  ; CF=1
            ; the order matters - cmovp must be the least !
            cmovp        i0, is_g ? 1 : -1       ; PF=1
            ;; if no one of cmov happend, then i0 remains zeroed
            */
        voper(Mnemonic_XOR, i0, i0);
        voper(is_dbl ? Mnemonic_COMISD : Mnemonic_COMISS, v1, v2);
        
        EncoderBase::Operand op_m1(OpndSize_32, RegName_Null, (int)&g_iconst_m1);
        EncoderBase::Operand op_1(OpndSize_32, RegName_Null, (int)&g_iconst_1);
        
        //ip(cmov(ip(), ConditionMnemonic_A, i0, opnd_1));
        //ip(cmov(ip(), ConditionMnemonic_B, i0, opnd_m1));
        //ip(cmov(ip(), ConditionMnemonic_P, i0, is_g ? opnd_1 : opnd_m1));
        voper((Mnemonic)(Mnemonic_CMOVcc + ConditionMnemonic_A), i0, op_1);
        voper((Mnemonic)(Mnemonic_CMOVcc + ConditionMnemonic_B), i0, op_m1);
        voper((Mnemonic)(Mnemonic_CMOVcc + ConditionMnemonic_P), i0, is_g ? op_1 : op_m1);
        return;
    }
    assert(op == OPCODE_LCMP);
    char *helper = (char *)rt_h_lcmp;
    static const jtype args[2] = { i64, i64 };
    gen_stack_to_args(true, 2, args);
    gen_call_novm(helper, 0);
    gen_save_ret(i32);
}

void Compiler::gen_field_op(JavaByteCodes op, jtype jt, Field_Handle fld)
{
    assert(OPCODE_PUTFIELD == op || OPCODE_GETFIELD == op);
    assert(I_STACK_REGS > 2);
    
    const bool put = op == OPCODE_PUTFIELD;
    const bool wide = is_wide(jt);
    const bool lazy = m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION;
    const JInst& jinst = *m_curr_inst;
    unsigned fld_offset = fld ? field_get_offset(fld) : 0;

    if (lazy) {
        gen_lazy_resolve(jinst.op0, jinst.opcode);
        // EAX = field handle
        voper(Mnemonic_PUSH, RegName_EAX);
        // stack: [.., fld]
        gen_call_novm((void*)&field_get_offset, 0);
        // stack: [.., fld] ; EAX = offset
        voper(Mnemonic_ADD, RegName_ESP, (int)STACK_SLOT_SIZE);
    }
    else if(fld == NULL) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0,
                       jinst.opcode);
    }
    else {
    }

    const RegName rref = vstack(put ? (wide ? 2 : 1) : 0);
//    jtype jt_org = jt;
    if (jt<i32) {
        jt=i32;
    }
    
    // 
    // Operand(OpndSize size, RegName base, RegName index, 
    //         unsigned scale, int disp)
    const EncoderBase::Operand pfield(typeInfo[jt].size, rref, 
                                      lazy ? RegName_EAX : RegName_Null, 
                                      lazy ? 1 : 0, // scale
                                      fld_offset);  // disp
    if (put) {
        voper(typeInfo[jt].mov, pfield, vstack(0));
        if (is_big(jt)) {
            assert(jt == i64);
            // move also the high part of the item
            // '4' is sizeof(i64)/4
            EncoderBase::Operand phi(pfield.size(), 
                                     pfield.base(), pfield.index(),
                                     pfield.scale(), pfield.disp() + 4);
            voper(typeInfo[jt].mov, phi, vstack(1));
        }
        //
        vpop(jt);
        vpop(jobj);
    }
    else {
        vpop(jobj);
        vpush(jt, true);
        Mnemonic mv = typeInfo[jt].mov;
        voper(mv, vstack(0), pfield);
        if (is_big(jt)) {
            assert(jt == i64);
            // move also the high part of the item
            
            // ******
            // NOTE: order dependency - the high part can not be loaded 
            // first, as it will rewrite vstack(1), which was vstack(0) 
            // and is currently used as a base !
            // ******
            
            // '4' is sizeof(i64)/4
            EncoderBase::Operand phi(pfield.size(), 
                pfield.base(), pfield.index(),
                pfield.scale(), pfield.disp() + 4);
            voper(typeInfo[jt].mov, vstack(1), phi);
        }
    }
}

void Compiler::gen_static_op(JavaByteCodes op, jtype jt, Field_Handle fld) {
    assert(OPCODE_PUTSTATIC == op || OPCODE_GETSTATIC == op);

    const bool put = op == OPCODE_PUTSTATIC;
    const bool lazy = m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION;
    const JInst& jinst = *m_curr_inst;
    void * fld_addr = fld ? field_get_addr(fld) : 0;

    if (lazy) {
        gen_lazy_resolve(jinst.op0, jinst.opcode);
        // EAX = field address
    }
    else if(fld == NULL) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0,
                       jinst.opcode);
    }
    else {
    }
    if (jt<i32) {
        jt = i32;
    }
    // 
    // Operand(OpndSize size, RegName base, RegName index, 
    //         unsigned scale, int disp)
    EncoderBase::Operand pstatic(typeInfo[jt].size, 
        lazy ? RegName_EAX : RegName_Null, RegName_Null, 
        0, (int)fld_addr);
    
    if (put) {
        voper(typeInfo[jt].mov, pstatic, vstack(0));
        if (is_big(jt)) {
            assert(jt == i64);
            // move also the high part of the item
            // '4' is sizeof(i64)/4
            EncoderBase::Operand phi(pstatic.size(), 
                                     pstatic.base(), pstatic.index(),
                                     pstatic.scale(), pstatic.disp() + 4);
            voper(typeInfo[jt].mov, phi, vstack(1));
        }
        vpop(jt);
    }
    else {
        vpush(jt, true);
        voper(typeInfo[jt].mov, vstack(0), pstatic);
        if (is_big(jt)) {
            assert(jt == i64);
            // move also the high part of the item
            // '4' is sizeof(i64)/4
            EncoderBase::Operand phi(pstatic.size(), 
                                     pstatic.base(), pstatic.index(),
                                     pstatic.scale(), pstatic.disp() + 4);
            voper(typeInfo[jt].mov, vstack(1), phi);
        }
    }
}


void Compiler::gen_switch(const JInst & jinst)
{
    assert(jinst.opcode == OPCODE_LOOKUPSWITCH
           || jinst.opcode == OPCODE_TABLESWITCH);

    if (jinst.opcode == OPCODE_LOOKUPSWITCH) {
        /* TODO: each comparation takes about 12 bytes. May want to replace 
        it with SCAS-based version for some number of keys.
        LOOKUPSWITCH's data is represented in byte code as array of 
        pairs - {value, target}.
        We split this representation up for our native representation, 
        with continuous sets of values and targets:
        values:              {value0, value1, ..., valueN, any-value-here}
        targets:     {target1, target2, ..., targetN, default}
        so may implement something like this:
           ;;;
           cld
           ; free ECX, ESI
           mov          ecx, count=N+1
           mov          esi, values
           repnz        scas
           ;; here the esi point either on the value next after the key found
           ;; or ecx is zero and esi points after the last (default) entry
           jmp          [esi + (values-targtes)-sizeof(int)]
           ;;;
        
        Current approach:
        ;;;
        N times: 
        cmp          [value], (n)
        jz           addr
        ...
        jmp          default
        ;;;
        */
        unsigned n = jinst.get_num_targets();
        const RegName r0 = vstack(0);
        vpop(i32);
        if (m_next_bb_is_multiref) {
            gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
            gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
        }
        for (unsigned i = 0; i < n; i++) {
            int key = jinst.key(i);
            unsigned pc = jinst.get_target(i);
            voper(Mnemonic_CMP, r0, EncoderBase::Operand(OpndSize_32, key));
            vjcc(pc, ConditionMnemonic_Z, InstPrefix_Null);
        }
        vjmp(jinst.get_def_target());
        return;
    }
    //
    // TABLESWITCH
    //
    const RegName r0 = vstack(0);
    vpop(i32);
    if (m_next_bb_is_multiref) {
        gen_mem(MEM_TO_MEM|MEM_VARS|MEM_UPDATE);
        gen_mem(MEM_FROM_MEM|MEM_STACK|MEM_UPDATE);
    }
    voper(Mnemonic_CMP, r0, EncoderBase::Operand(OpndSize_32, jinst.high()));
    vjcc(jinst.get_def_target(), ConditionMnemonic_G, InstPrefix_Null);
    voper(Mnemonic_CMP, r0, EncoderBase::Operand(OpndSize_32, jinst.low()));
    vjcc(jinst.get_def_target(), ConditionMnemonic_L, InstPrefix_Null);
    voper(Mnemonic_SUB, r0, EncoderBase::Operand(OpndSize_32, jinst.low()));
    
    reg_table_switch_patch();
    EncoderBase::Operand ptable(OpndSize_32, RegName_Null, r0, 
                                sizeof(void*), 0);
    voper(Mnemonic_JMP, ptable);
}

void Compiler::gen_dbg_brk(void)
{
    voper(Mnemonic_INT3);
}

void Compiler::gen_instanceof_cast(bool checkcast, Class_Handle klass)
{
    const JInst& jinst = *m_curr_inst;
    if (m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION) {
        gen_lazy_resolve(jinst.op0, jinst.opcode);
        voper(Mnemonic_PUSH, RegName_EAX);
    }
    else if (klass==NULL) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0, 
                       jinst.opcode);
    }
    else {
        /*push klass*/voper(Mnemonic_PUSH, MK_IMM32(klass));
    }
    static const jtype args[1] = { jobj };
    gen_stack_to_args(true, 1, args);
    char * helper = checkcast ? rt_helper_checkcast : rt_helper_instanceof;
    gen_call_vm(helper, 0);
    gen_save_ret(checkcast ? jobj : i32);
}

void Compiler::gen_gc_stack(int depth /*=-1*/, bool trackIt /*=false*/)
{
    if (depth == -1) {
        depth = m_jframe->size();
    }
    // prepare GC info for stack
    // Store the current depth
    if (m_curr_bb_state->stack_depth == (unsigned)depth) {
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg(";; > skipping stack depth storage due to known"
                " same depth (%d)\n", m_curr_bb_state->stack_depth);
        }
    }
    else {
        EncoderBase::Operand stack_depth_info = 
                MK_MEM32(REG_BASE, SLOT_SIZE * m_stack.info_gc_stack_depth());
        voper(Mnemonic_MOV, stack_depth_info, MK_IMM32(depth));
        if (trackIt) {
            m_curr_bb_state->stack_depth = depth;
        }
    };
    unsigned n_words = words(depth);
    if (n_words != 0) {
        // check whether we do need to store first word
        unsigned gc_word = m_jframe->gc_mask(0);
        if (m_curr_bb_state->stack_mask_valid && 
            gc_word == m_curr_bb_state->stack_mask) {
            // do not need to update the GC mask, it's the same
            if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
                dbg(";; > skipping GC mask due to known same value (0x%X)\n",
                    gc_word );
            }
        }
        else {
            EncoderBase::Operand info_word = 
                MK_MEM32(REG_BASE, SLOT_SIZE*(0 + m_stack.info_gc_stack()));
            voper(Mnemonic_MOV, info_word, MK_IMM32(gc_word));
            if (trackIt) {
                m_curr_bb_state->stack_mask = gc_word;
                m_curr_bb_state->stack_mask_valid = true;
            }
        }
    }
    // store the bit masks
    for (unsigned i = 1; i < n_words; i++) {
        EncoderBase::Operand info_word = 
            MK_MEM32(REG_BASE, SLOT_SIZE*(i + m_stack.info_gc_stack()));
        voper(Mnemonic_MOV, info_word, MK_IMM32(m_jframe->gc_mask(i)));
    }
}

void Compiler::gen_gc_mark_local(unsigned idx, bool mark)
{
    const JFrame::Slot& v = m_jframe->var(idx);
    // If an item was already marked
    // and its type is still the same, than skip the mark
    if ((v.state() & SS_MARKED) && 
        ((v.jt == jobj && mark) || (v.jt != jobj && !mark))) {
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg(";;> skipping GC mark, due to known object type\n");
        }
        return;
    }
    // prepare GC info for stack
    unsigned word = word_no(idx);
    unsigned offset = (word + m_stack.info_gc_locals()) * SLOT_SIZE;
    EncoderBase::Operand gcinfo = MK_MEM32(REG_BASE, offset);
    long mask = 1 << bit_no(idx);
    if (mark) {
        voper(Mnemonic_OR, gcinfo, MK_IMM32(mask));
    }
    else {
        voper(Mnemonic_AND, gcinfo, MK_IMM32(~mask));
    }
    m_jframe->var_state(idx, SS_MARKED, 0);
}

void Compiler::gen_stack_to_args(bool pop, unsigned num, const jtype * args)
{
    unsigned slots = count_slots(num, args);
    unsigned depth = slots - 1;
    for (unsigned i = 0; i < num; i++) {
        jtype jt = args[i];
        if (is_f(jt)) {
            bool is_dbl = jt == dbl64;
            if (is_dbl) {
                --depth;
            }
            const JFrame::Slot& s = m_jframe->dip(depth);
            if (s.swapped()) {
                if (is_dbl) {
                    voper(Mnemonic_PUSH, vstack_mem_slot(depth + 1));
                }
                voper(Mnemonic_PUSH, vstack_mem_slot(depth));
            }
            else {
                voper(Mnemonic_SUB, RegName_ESP, 
                      is_dbl ? Imm8_StackSlotX2 : Imm8_StackSlot);
                voper(typeInfo[s.jt].mov, 
                      EncoderBase::Operand(typeInfo[s.jt].size, RegName_ESP, 0),
                      vstack(depth));
            }
            assert((m_jframe->dip(depth).jt == jt) || 
                    (m_jframe->dip(depth).jt<=i32 && jt<=i32));
            assert(!m_jframe->dip(depth).hipart());
        }
        else {
            const JFrame::Slot& s = m_jframe->dip(depth);
            if (s.swapped()) {
                voper(Mnemonic_PUSH, vstack_mem_slot(depth));
            }
            else {
                voper(Mnemonic_PUSH, vstack(depth));
            }
            assert((m_jframe->dip(depth).jt == jt) || 
                   (m_jframe->dip(depth).jt<=i32 && jt<=i32));
            //assert(m_jframe->dip(depth).jt == jt);
            if (jt == i64) {
                assert(m_jframe->dip(depth).hipart());
                --depth;
                assert(!m_jframe->dip(depth).hipart());
                const JFrame::Slot& s1 = m_jframe->dip(depth);
                if (s1.swapped()) {
                    voper(Mnemonic_PUSH, vstack_mem_slot(depth));
                }
                else {
                    voper(Mnemonic_PUSH, vstack(depth));
                }
            }
        }
        --depth;
    }
    if (pop) {
        for (unsigned i = 0; i < num; i++) {
            vpop(args[num - i - 1]);
        }
    }
}

void Compiler::gen_new(Class_Handle klass)
{
    const JInst& jinst = *m_curr_inst;
    bool lazy = m_infoBlock.get_flags() & JMF_LAZY_RESOLUTION;
    const unsigned cp_idx = jinst.op0;
    if (lazy) {
        gen_lazy_resolve(cp_idx, jinst.opcode);
        // EAX - allocation handle
        voper(Mnemonic_PUSH, RegName_EAX);
        // push size
        reg_data_patch(RefType_Size, cp_idx);
        voper(Mnemonic_PUSH, MK_MEM32(RegName_Null, 0));
        // stack: [.., alloc_h, size]
        gen_call_vm(rt_helper_new, 0);
    }
    else if (klass == NULL) {
        gen_call_throw(rt_helper_throw_linking_exc, 3, m_klass, jinst.op0, 
                       jinst.opcode);
    }
    else {
        unsigned size = class_get_boxed_data_size(klass);
        Allocation_Handle ah = class_get_allocation_handle(klass);
        gen_call_vm(rt_helper_new, 2, ah, size);
    }
    gen_save_ret(jobj);
    m_jframe->stack_attrs(0, SA_NZ);
}

void Compiler::gen_lazy_resolve(unsigned idx, JavaByteCodes opkod)
{
    const bool is_new = opkod == OPCODE_NEW;
    
    veax();
    reg_data_patch(toRefType(opkod), idx);
    voper(Mnemonic_MOV, RegName_EAX, MK_MEM32(RegName_Null, 0));
    
    if (m_curr_bb_state->resState.test(opkod, idx)) {
        if (m_infoBlock.get_flags() & DBG_TRACE_CG) {
            dbg(";;>skipping resolution gen due to known resolved state\n");
        };
        return;
    }
    m_curr_bb_state->resState.done(opkod, idx);
    
    voper(Mnemonic_TEST, RegName_EAX, RegName_EAX);
    unsigned pid_done = vjcc(ConditionMnemonic_NZ, InstPrefix_HintTaken);

    gen_mem(MEM_TO_MEM|MEM_VARS|MEM_STACK|MEM_NO_UPDATE);
    gen_gc_stack(-1, false);

    voper(Mnemonic_PUSH, MK_IMM32(opkod));
    voper(Mnemonic_PUSH, MK_IMM32(idx));
    voper(Mnemonic_PUSH, MK_IMM32((int)m_klass));
    vcall((void*)vm_get_rt_support_addr(VM_RT_RESOLVE));
    // For some operations we need some additional work
    if (opkod == OPCODE_PUTSTATIC || opkod == OPCODE_GETSTATIC) {
        voper(Mnemonic_PUSH, RegName_EAX);
        vcall((void*)&field_get_addr);
        voper(Mnemonic_ADD, RegName_ESP, Imm8_StackSlot);
    }
    else if (opkod == OPCODE_INVOKESPECIAL || opkod == OPCODE_INVOKESTATIC) {
        voper(Mnemonic_PUSH, RegName_EAX);
        vcall((void*)&method_get_indirect_address);
        voper(Mnemonic_ADD, RegName_ESP, Imm8_StackSlot);
    }
    else if(opkod == OPCODE_ANEWARRAY) {
        voper(Mnemonic_PUSH, RegName_EAX);
        //// stack: [..., Class_Handle]
        //vcall((void*)&class_get_array_of_class);
        //// stack: [..., Class_Handle]
        //voper(Mnemonic_MOV, StackTop32, RegName_EAX);
        //// stack: [..., Class_Handle_of_aray]
        vcall((void*)&class_get_allocation_handle);
        voper(Mnemonic_ADD, RegName_ESP, Imm8_StackSlot);
    }
    else if(is_new) {
        voper(Mnemonic_PUSH, RegName_EAX);
        voper(Mnemonic_PUSH, RegName_EAX);
        // stack: [..., Class_Handle, Class_Handle]
        vcall((void*)&class_get_allocation_handle);
        voper(Mnemonic_MOV, Stack32_1, RegName_EAX);
        // stack: [..., allocation_handle, Class_Handle]
        vcall((void*)&class_get_boxed_data_size);
        // stack: [..., allocation_handle, Class_Handle], EAX - size
        
        reg_data_patch(RefType_Size, idx);
        voper(Mnemonic_MOV, MK_MEM32(RegName_Null, 0), RegName_EAX);
        
        voper(Mnemonic_POP, RegName_EAX);
        voper(Mnemonic_POP, RegName_EAX);
        // stack: [...], EAX = allocation_handle
    }
    //
    /*
    Can't use LOCK together with MOV, though there is a problem: according
    to the arch manual, the 'mov mem32, ..' is atomic on multi-cores only 
    when the memory address is 4-bytes aligned. As we can't guarantee here 
    that the address is 4bytes aligned, then we might have a problem 
    here - todo ?
    
    if (!SysInfo::onecpu()) {
        voper(Mnemonic_PUSH, RegName_EAX);
        vprefix(InstPrefix_LOCK);
        reg_data_patch( );
        voper(Mnemonic_XCHG, MK_MEM32(RegName_Null, 0), RegName_EAX);
        voper(Mnemonic_POP, RegName_EAX);
    }
    */
    
    reg_data_patch(toRefType(opkod), idx);
    voper(Mnemonic_MOV, MK_MEM32(RegName_Null, 0), RegName_EAX);
    
    // will not touch EAX, as EAX was free-ed earlier
    gen_mem(MEM_FROM_MEM|MEM_VARS|MEM_STACK|MEM_NO_UPDATE|MEM_INVERSE);
    
    patch_set_target(pid_done, ip());
}


void Compiler::gen_call_throw(void * target, unsigned num_args, ...)
{
    // save only vars, and do not update the state
    gen_mem(MEM_VARS|MEM_TO_MEM|MEM_NO_UPDATE);
    // say 'stack is empty'
    gen_gc_stack(0, false);
    
    va_list valist;
    va_start(valist, num_args);
    for (unsigned i = 0; i < num_args; i++) {
        int val = va_arg(valist, int);
        voper(Mnemonic_PUSH, MK_IMM32(val));
    }
    vcall(target);
#ifdef _DEBUG
    // just to make sure we do not return from there
    voper(Mnemonic_INT3);
#endif
}

void Compiler::gen_call_vm(void * target, unsigned num_args, ...)
{
    gen_gc_stack(-1, true); // <= the only difference with gen_call_novm()
    gen_mem(MEM_STACK|MEM_VARS|MEM_TO_MEM|MEM_UPDATE);
    va_list valist;
    va_start(valist, num_args);
    for (unsigned i = 0; i < num_args; i++) {
        int val = va_arg(valist, int);
        voper(Mnemonic_PUSH, MK_IMM32(val));
    }
    vcall(target);
}

void Compiler::gen_call_novm(void * target, unsigned num_args, ...)
{
    gen_mem(MEM_STACK|MEM_VARS|MEM_TO_MEM|MEM_UPDATE);
    va_list valist;
    va_start(valist, num_args);
    for (unsigned i = 0; i < num_args; i++) {
        int val = va_arg(valist, int);
        voper(Mnemonic_PUSH, MK_IMM32(val));
    }
    vcall(target);
}

void Compiler::gen_mem(unsigned flags)
{
#ifdef _DEBUG
    //
    // check contract
    //
    // either STACK or MEM or both must be specified to synchronize
    assert((flags & MEM_STACK) || (flags & MEM_VARS));
    // either TO_MEM or FROM_MEM but not both must be specified 
    if (flags & MEM_TO_MEM) {
        assert(!(flags & MEM_FROM_MEM));
    }
    else {
        assert(flags & MEM_FROM_MEM);
    }
    // either UPDATE or NO_UPDATE but not both must be specified
    if (flags & MEM_UPDATE) {
        assert(!(flags & MEM_NO_UPDATE));
    }
    else {
        assert(flags & MEM_NO_UPDATE);
    }
    if (flags & MEM_INVERSE) {
        assert(flags & MEM_NO_UPDATE);
        assert(flags & MEM_FROM_MEM);
    }
#endif // ifdef _DEBUG
    //
    // NOTE: MUST NOT change flags, as the code may be executed
    // between a test and conditional jump.
    //
    bool toMem = flags & MEM_TO_MEM;
    bool updateState = flags & MEM_UPDATE;
    bool inverse = flags & MEM_INVERSE;
    
    if (flags & MEM_STACK) {
        for (unsigned i = 0, n = m_jframe->size(); i < n; i++) {
            const JFrame::Slot& s = m_jframe->dip(i);
            if (s.hipart() && !is_big(s.jt)) {
                continue;
            }
            if (!m_jframe->regable(i)) {
                continue;
            }
            
            assert(s.regid() != -1 && s.vslot() != -1 && s.jt != jvoid);
            RegName r = get_regs(s.jt)[s.regid()];
            assert(s.jt != i8); // to avoid cast of ESI/EDI/EBP to CH/DH/BH
            r = getAliasReg(r, typeInfo[s.jt].size);
            // the resulting name must be valid
            assert(NULL != getRegNameString(r));
            if (inverse) {
                assert(!toMem && !updateState);
                if (!s.swapped()) {
                    voper(typeInfo[s.jt].mov, r, vmstack(i));
                }
            }
            else if (s.swapped() && !toMem) {
                voper(typeInfo[s.jt].mov, r, vmstack(i));
                if (updateState) {
                    m_jframe->stack_state(i, 0, SS_SWAPPED);
                }
            }
            else if(!s.swapped() && toMem) {
                voper(typeInfo[s.jt].mov, vmstack(i), r);
                if (updateState) {
                    m_jframe->stack_state(i, SS_SWAPPED, 0);
                }
            }
        }
    }
    
    if (flags & MEM_VARS) {
        for (unsigned i=0; i<m_jframe->num_vars(); i++) {
            const JFrame::Slot& v = m_jframe->var(i);
            // cant operate on a frame which has non-processed items
            assert(!v.needswap());
            if (v.regid() == -1) continue;
            if (v.hipart() && !is_big(v.jt)) continue;
            if (inverse) {
                assert(!toMem && !updateState);
                if (!v.swapped()) {
                    voper(typeInfo[v.jt].mov, 
                    vlocal(v.jt, i, v.hipart(), false), vmlocal(v.jt, i));
                }
            }
            else if (v.swapped() && !toMem) {
                voper(typeInfo[v.jt].mov, 
                      vlocal(v.jt, i, v.hipart(), false), vmlocal(v.jt, i));
                if (updateState) {
                    m_jframe->var_state(i, 0, SS_SWAPPED);
                }
            }
            else if(!v.swapped() && toMem) {
                voper(typeInfo[v.jt].mov, 
                      vmlocal(v.jt, i), vlocal(v.jt, i, v.hipart(), false));
                if (updateState) {
                    m_jframe->var_state(i, SS_SWAPPED, 0);
                }
            }
        }
    }
}

void Compiler::gen_dbg_check_stack(bool start) {
    if (start) {
        // We store ESP before a code to be checked ...
        voper(Mnemonic_PUSH, RegName_ESP);
        // ... then extracts size of stack slot ...
        voper(Mnemonic_SUB, StackTop32, Imm8_StackSlot);
        // ... so now we have 
        // ESP == [ESP]
        // This condition is checked after the code and it it's not met, 
        // INT3 raised
        return;
    }
    voper(Mnemonic_CMP, RegName_ESP, StackTop32);
    unsigned pid = vjcc(ConditionMnemonic_Z);
//    dbg_rt("Stack corrupted !!!\n");
    voper(Mnemonic_INT3);
    patch_set_target(pid, ip());
    voper(Mnemonic_ADD, RegName_ESP, Imm8_StackSlot);
}

void Compiler::gen_dbg_check_bb_stack(void) {
    if (m_infoBlock.get_bc_size() == 1 && m_bc[0] == OPCODE_RETURN) {
        return; // empty method, nothing to do
    }

    // With the current code generation scheme, the depth of native stack
    // at the beginning of a basic block is exactly the same and is equal
    // to the stack depth right after the method prolog executed.
    // So, this check is enforced
    // This is not valid for basic blocks which are part of a JSR subroutine
    // because we have return address at the top of the stack (or even 
    // several ones). 
    // Such a blocks are not checked this way - the check is not called in 
    // bbs_gen_code. The expectation is that the JSR subroutine is 
    // self-checked - when we perform RET from JSR subroutine, if we have
    // corrupted stack, we'll crash for sure. 
    voper(Mnemonic_PUSH, REG_BASE);
    //voper(Mnemonic_SUB, StackTop32, Imm8_StackSlot);
    //voper(Mnemonic_SUB, StackTop32, MK_IMM32(m_stack.size()*STACK_SLOT_SIZE));
    
    // '+1' here due to just pushed REG_BASE/EBP
    voper(Mnemonic_SUB, StackTop32, 
                        MK_IMM32((m_stack.size()+1)*STACK_SLOT_SIZE));
    voper(Mnemonic_CMP, RegName_ESP, StackTop32);
    unsigned pid = vjcc(ConditionMnemonic_Z);
    gen_dbg_brk();
    patch_set_target(pid, ip());
    voper(Mnemonic_ADD, RegName_ESP, Imm8_StackSlot); // pop out EBP/REG_BASE
}

}};             // ~namespace Jitrino::Jet
