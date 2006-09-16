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
 * @author Alexander Astapchuk
 * @version $Revision$
 */
/**
 * @file
 * @brief CodeGen's routines for instrumentation and profiling.
 */
 
#include "cg.h"
#include "trace.h"

namespace Jitrino {
namespace Jet {

void CodeGen::gen_prof_be(void)
{
    if (!is_set(JMF_PROF_ENTRY_BE)) {
        return;
    }

#ifdef _IA32_
    AR addr = ar_x;
    int off = (int)m_p_backedge_counter;
#else
    AR addr = valloc(jobj);
    movp(addr, m_p_backedge_counter);
    int off = 0;
#endif
    alu(alu_add, Opnd(i32, addr, off), 1);
}

void CodeGen::gen_gc_safe_point()
{
    if (!is_set(JMF_BBPOLLING)) {
        return;
    }
    if (m_bbstate->seen_gcpt) {
        if (is_set(DBG_TRACE_CG)) {
            dbg(";;>gc.safepoint - skipped\n");
        }
        return;
    }
    m_bbstate->seen_gcpt = true;
    // On Windows we could use a bit, but tricky way - we know about VM 
    // internals and we know how Windows manages TIB, and thus we can get a 
    // direct access to the flag, without need to call VM:
    //      mov eax, fs:14
    //      test [eax+rt_suspend_req_flag_offset], 0
    // I don't believe this will gain any improvements for .jet, so using 
    // portable and 'official' way:
    gen_call_vm(cs_v, rt_helper_get_thread_suspend_ptr, 0);
    // The address of flag is now in gr_ret
    Opnd mem(i32, gr_ret, 0);
    alu(alu_cmp, mem, Opnd(0));
    unsigned br_off = br(z, 0, 0, taken);
    gen_call_vm_restore(false, cs_v, rt_helper_gc_safepoint, 0);
    patch(br_off, ip());
}

void CodeGen::gen_modification_watchpoint(JavaByteCodes opcode, jtype jt, Field_Handle fld) {

    unsigned ref_depth = is_wide(jt) ? 2 : 1;
    bool field_op = (opcode == OPCODE_PUTFIELD) ? true : false;

    // Check whether VM need modification notifications

    char* fld_tr_add;
    char fld_tr_mask;
    field_get_track_modification_flag(fld, &fld_tr_add, &fld_tr_mask);

    Val fld_track_mask((int)fld_tr_mask);

    AR fld_trackAr = valloc(jobj);
    movp(fld_trackAr, (void*)fld_tr_add);
    Opnd fld_track_opnd(i32, fld_trackAr, 0);
    //mov(fld_trackAr, Opnd(0xFFFFFFFF)); // Emulation to check access flag enabled
    //Opnd fld_track_opnd(fld_trackAr);
    
    rlock(fld_track_opnd);

    alu(alu_test, fld_track_opnd, fld_track_mask.as_opnd());
    runlock(fld_track_opnd);

    unsigned br_off = br(z, 0, 0, taken);
    
    // Store all scratch registers and operand stack state
    BBState saveBB;
    push_all_state(&saveBB);

    //JVMTI helper takes field handle, method handle, byte code location, pointer
    //to reference for fields or NULL for statics, pointer to field value

    AR fieldValBaseAr = valloc(jobj);
    Val fieldValPtr = Val(jobj, fieldValBaseAr);
    rlock(fieldValPtr);
    

    if (jt != jvoid) {
        // Make sure the value item is on the memory
        vswap(0);
        if (is_big(jt)) {
            vswap(1);
        }
        const Val& s = vstack(0);
        assert(s.is_mem());
        lea(fieldValPtr.as_opnd(), s.as_opnd());
    } else {
        Opnd stackTop(jobj, m_base, voff(m_stack.unused()));
        lea(fieldValPtr.as_opnd(), stackTop);
    }
    runlock(fieldValPtr);

#ifndef _EM64T_
    // Workaround since do_mov do not put jlong on stack in gen_args on ia32
    const CallSig cs_ti_fmodif(CCONV_HELPERS, jobj, jobj, i32, i32, jobj, jobj);
    Val vlocation((jlong)m_pc);
    Val vlocationHi((jlong)0);
#else
    const CallSig cs_ti_fmodif(CCONV_HELPERS, jobj, jobj, i64, jobj, jobj);
    Val vlocation((jlong)m_pc);
#endif

    Val vfield(jobj, fld);
    Val vmeth(jobj, m_method);
    Val vobject =  Val(jobj, NULL);

    if (field_op) {
        vobject = vstack(ref_depth);
    }
#ifndef _EM64T_
    // Workaround since do_mov do not put jlong on stack in gen_args on ia32
    gen_args(cs_ti_fmodif, 0, &vfield, &vmeth, &vlocationHi, &vlocation, &vobject, &fieldValPtr);
#else
    gen_args(cs_ti_fmodif, 0, &vfield, &vmeth, &vlocation, &vobject, &fieldValPtr);
#endif

    // 2. Park all locals and operand stack
    vpark();
    // Store gc info
    gen_gc_stack(-1, true);

    // 3. Call VM
    rlock(cs_ti_fmodif);
    AR gr = valloc(jobj);
    call( is_set(DBG_CHECK_STACK), gr, rt_helper_ti_field_modification, cs_ti_fmodif, cs_ti_fmodif.count());
    runlock(cs_ti_fmodif);
    
    //Restore operand stack state and scratch registers
    pop_all_state(&saveBB);

    patch(br_off, ip());
}


void CodeGen::gen_access_watchpoint(JavaByteCodes opcode, jtype jt, Field_Handle fld) {

    bool field_op = (opcode == OPCODE_GETFIELD) ? true : false;

    // Check whether VM need access notifications
    
    char* fld_tr_add;
    char fld_tr_mask;
    field_get_track_access_flag(fld, &fld_tr_add, &fld_tr_mask);

    Val fld_track_mask((int)fld_tr_mask);

    AR fld_trackAr = valloc(jobj);
    rlock(fld_trackAr);
    
    //mov(fld_trackAr, Opnd(0xFFFFFFFF)); // Emulation to check access flag enabled
    //Opnd fld_track_opnd(fld_trackAr);
    movp(fld_trackAr, (void*)fld_tr_add);
    Opnd fld_track_opnd(i32, fld_trackAr, 0);
    alu(alu_test, fld_track_opnd, fld_track_mask.as_opnd());

    runlock(fld_trackAr);

    unsigned br_off = br(z, 0, 0, taken);

    // Store all scratch registers and operand stack state
    BBState saveBB;
    push_all_state(&saveBB);


    //JVMTI helper takes field handle, method handle, byte code location, pointer
    //to reference for fields or NULL for statics

#ifndef _EM64T_
    // Workaround since do_mov do not put jlong on stack in gen_args on ia32
    const CallSig cs_ti_faccess(CCONV_HELPERS, jobj, jobj, i32, i32, jobj);
    Val vlocation((jlong)m_pc);
    Val vlocationHi((jlong)0);
#else
    const CallSig cs_ti_faccess(CCONV_HELPERS, jobj, jobj, i64, jobj);
    Val vlocation((jlong)m_pc);
#endif
    rlock(cs_ti_faccess);


    Val vfield(jobj, fld);
    Val vmeth(jobj, m_method);
    Val vobject =  Val(jobj, NULL);

    if (field_op) {
        vobject = vstack(0);
    }
    
#ifndef _EM64T_
    // Workaround since do_mov do not put jlong on stack in gen_args on ia32
    gen_args(cs_ti_faccess, 0, &vfield, &vmeth, &vlocationHi, &vlocation, &vobject);
#else
    gen_args(cs_ti_faccess, 0, &vfield, &vmeth, &vlocation, &vobject);
#endif

    // 2. Park all locals and operand stack
    vpark();
    // Store gc info
    gen_gc_stack(-1, true);

    // 3. Call VM
    rlock(cs_ti_faccess);
    AR gr = valloc(jobj);
    call( is_set(DBG_CHECK_STACK), gr, rt_helper_ti_field_access, cs_ti_faccess, cs_ti_faccess.count());
    runlock(cs_ti_faccess);

    //Restore operand stack state and scratch registers
    pop_all_state(&saveBB);


    patch(br_off, ip());
}


void CodeGen::push_all_state(BBState *saveBB){
    *saveBB = *m_bbstate;
    // 1. store scratch registers in a secret place
    // 2. park everything
    // 3. call whatever
    // 4. restore scratch regs from the secret place
    // 5. restore the state for callee-save registers
    //-----------------------------------------------
    // 1. 
    bool saveScratch = true;
    for (unsigned i=0; i<ar_num; i++) {
        AR ar = _ar(i);
        if (is_callee_save(ar)) continue;
        if (saveScratch && rrefs(ar) != 0) {
            jtype jt = is_f(ar) ? dbl64 : jobj;
            Opnd mem(jt, m_base, voff(m_stack.spill(ar)));
            Opnd reg(jt, ar);
            mov(mem, reg);
        }
        if (rlocks(ar) != 0) {
            runlock(ar, true);
        }
    }
}

void CodeGen::pop_all_state(BBState* saveBB) {
    bool saveScratch = true;
    // 4.
    *m_bbstate = *saveBB;
    // restore the registers state
    for (unsigned i=0; saveScratch && i<ar_num; i++) {
        AR ar = _ar(i);
        if (is_callee_save(ar)) continue;
        if (rrefs(ar) != 0) {
            jtype jt = is_f(ar) ? dbl64 : jobj;
            Opnd mem(jt, m_base, voff(m_stack.spill(ar)));
            Opnd reg(jt, ar);
            mov(reg, mem);
            }
        }
// 5. 
// Actually nothing to do here.
// If we had a local var on register before, then it's still on the reg
// If we had the var with static assignment which was in memory, before,
// then the memory was not corrupted.
// So, just nothing to do with callee-save regs
//
}

}}; // ~namespace Jitrino::Jet
