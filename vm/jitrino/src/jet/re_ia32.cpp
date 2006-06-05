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
 * @version $Revision: 1.1.12.3.4.4 $
 */
#include "compiler.h"

/**
 * @file
 * @brief Contains an implementation of register machinery for IA32/EM64T 
 *        platforms.
 */
 
namespace Jitrino {
namespace Jet {

const RegName Compiler::g_frnames[F_STACK_REGS + F_LOCAL_REGS] = {
    RegName_XMM0, RegName_XMM1, RegName_XMM2,
    RegName_XMM3, RegName_XMM4, RegName_XMM5,
};

const RegName Compiler::g_irnames[I_STACK_REGS + I_LOCAL_REGS] = {
    RegName_EBX, RegName_EDI, RegName_ESI, RegName_ECX,
    RegName_EDX, RegName_EAX,
};

void Compiler::vpop(jtype jt)
{
    m_jframe->pop(jt);
}

void Compiler::veax(void) 
{
    static const unsigned idx = 1;
    assert(g_irnames[idx+I_STACK_REGS] == RegName_EAX);
    m_jframe->release(i32, idx);
    vsync();
}

void Compiler::vedx(void)
{
    static const unsigned idx = 0;
    assert(g_irnames[idx+I_STACK_REGS] == RegName_EDX);
    m_jframe->release(i32, idx);
    vsync();
}

void Compiler::vpush(jtype jt, bool sync)
{
    m_jframe->push(jt);
    if (sync && m_jframe->need_update()) {
        vsync();
    }
}

EncoderBase::Operand Compiler::vmstack(unsigned depth)
{
    jtype jt = m_jframe->top(depth);
    return EncoderBase::Operand(
        typeInfo[jt].size, REG_BASE, 
        m_stack.stack_slot(m_jframe->depth2slot(depth))*STACK_SLOT_SIZE);
}

EncoderBase::Operand Compiler::vstack_mem_slot(unsigned depth)
{
    jtype jt = i32;
    return EncoderBase::Operand(
        typeInfo[jt].size, REG_BASE, 
        m_stack.stack_slot(m_jframe->depth2slot(depth))*STACK_SLOT_SIZE);
}


EncoderBase::Operand Compiler::vmlocal(jtype jt, unsigned idx)
{
    return EncoderBase::Operand(
        typeInfo[jt].size, REG_BASE, m_stack.local(idx)*STACK_SLOT_SIZE);
}


EncoderBase::Operand Compiler::vlocal(jtype jt, int idx, bool upper,
                                      bool willDef)
{
    if (idx != -1 && !willDef) {
        // use of existing var, can return memory location here
        const JFrame::Slot& v = m_jframe->var(idx);
        if (v.regid() == -1) {
            // not on the reg, return memory
            return vmlocal(jt, idx);
        }
    }
    unsigned regid = m_jframe->alloc(jt, idx, upper, willDef);
    vsync();
    assert(regid < m_jframe->_rlocalregs(jt));
    RegName r = get_regs(jt)[regid + m_jframe->_rstackregs(jt)];
    assert(NULL != getRegNameString(r));
    assert(jt != i8); // to avoid cast of ESI/EDI/EBP to CH/DH/BH
    r = getAliasReg(r, typeInfo[jt].size);
    assert(NULL != getRegNameString(r));
    return EncoderBase::Operand(r);
}

void Compiler::vsync(void)
{
    if (m_jframe->need_update()==0) {
        return;
    }
    // scan stack
    for (unsigned i=0; i<m_jframe->size(); i++) {
        const JFrame::Slot& s = m_jframe->dip(i);
        if (s.state()&SS_NEED_SWAP) {
            assert(s.regid() != -1);
            RegName r = get_regs(s.jt)[s.regid()];
            assert(s.jt != i8); // to avoid cast of ESI/EDI/EBP to CH/DH/BH
            r = getAliasReg(r, typeInfo[s.jt].size);
            voper(typeInfo[s.jt].mov, vmstack(i), r);
            m_jframe->stack_state(i, SS_SWAPPED, SS_NEED_SWAP);
        }
        else if (s.state()&SS_NEED_UPLOAD) {
            assert(false); // should not happen on stack slots
            //voper(typeInfo[s.jt].mov, vmstack(i), r);
            //m_jframe->stack_state(i, 0, SS_NEED_UPLOAD);
        }
    }
    // scan locals
    for (unsigned i=0; i<m_jframe->num_vars(); i++) {
        const JFrame::Slot& s = m_jframe->var(i);
        if (s.regid() < 0) {
            continue;
        }
        RegName r = get_regs(s.jt)[s.regid() + m_jframe->_rstackregs(s.jt)];
        assert(s.jt != i8); // to avoid cast of ESI/EDI/EBP to CH/DH/BH
        r = getAliasReg(r, typeInfo[s.jt].size);
        
        if (s.state()&SS_NEED_SWAP) {
            voper(typeInfo[s.jt].mov, vmlocal(s.jt, i), r);
            m_jframe->var_state(i, SS_SWAPPED, SS_NEED_SWAP);
        }
        else if (s.state()&SS_NEED_UPLOAD) {
            voper(typeInfo[s.jt].mov, r, vmlocal(s.jt,i));
            m_jframe->var_state(i, 0, SS_NEED_UPLOAD);
        }
    }
    assert(m_jframe->need_update()==0);
}

void Compiler::vstack_swap(unsigned depth)
{
    const JFrame::Slot& s = m_jframe->dip(depth);
    if (s.swapped()) {
        return;
    }
    RegName r = get_regs(s.jt)[s.regid()];
    assert(s.jt != i8); // to avoid cast of ESI/EDI/EBP to CH/DH/BH
    r = getAliasReg(r, typeInfo[s.jt].size);
    voper(typeInfo[s.jt].mov, vmstack(depth), r);
    m_jframe->stack_state(depth, SS_SWAPPED, SS_NEED_SWAP);
}

RegName Compiler::vstack(unsigned depth)
{
    assert(m_jframe->need_update() == 0);
    const JFrame::Slot& s = m_jframe->dip(depth);
    assert(s.regid() != -1 && s.vslot() != -1 && s.jt != jvoid);
    
    assert(m_jframe->regable(depth));
    RegName r = get_regs(s.jt)[s.regid()];
    assert(s.jt != i8); // to avoid cast of ESI/EDI/EBP to CH/DH/BH
    r = getAliasReg(r, typeInfo[s.jt].size);
    // the resulting name must be valid
    assert(NULL != getRegNameString(r));
    if (s.swapped()) {
        // bring it back from the memory
        voper(typeInfo[s.jt].mov, r, vmstack(depth));
        m_jframe->stack_state(depth, 0, SS_SWAPPED);
    }
    return r;
}

RegName Compiler::valias(jtype jt, RegName rbig)
{
    RegName rsmall;
    if (jt==i8 && (rbig == RegName_ESI || rbig == RegName_EDI || 
                   rbig == RegName_EBP)) {
        // cant use low 8bit part of these register, need to transfer 
        // somewhere. requesting a scratch register for now, as they are 
        // known to be good from this point - EDX/EAX.
        EncoderBase::Operand rscratch = vlocal(i32,-1,false,true);
        voper(Mnemonic_MOV, rscratch, rbig);
        rsmall = getAliasReg(rscratch.reg(), OpndSize_8);
    }
    else {
        assert(jt<i32);
        rsmall = getAliasReg(rbig, typeInfo[jt].size);
    }
    return rsmall;
}

void Compiler::vdostack(Mnemonic mn, unsigned depth0, int depth1)
{
    assert(m_jframe->need_update() == 0);
    RegName r0 = vstack(depth0);
    
    const JFrame::Slot& s = m_jframe->dip(depth1);
    assert(s.regid() != -1 && s.vslot() != -1 && s.jt != jvoid);
    assert(m_jframe->vslots(s.jt) - s.vslot() < m_jframe->_rstackregs(s.jt));
    if (s.swapped()) {
        voper(mn, r0, vmstack(depth1));
    }
    else {
        voper(mn, r0, vstack(depth1));
    }
}

bool Compiler::vlocal_on_mem(unsigned idx)
{
    const JFrame::Slot& v = m_jframe->var(idx);
    if (v.jt == jvoid ) return true;
    return v.regid() == -1 || v.swapped();
}


}}; // ~namespace Jitrino::Jet
