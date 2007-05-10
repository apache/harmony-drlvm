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
 * @author Alexander Astapchuk
 * @version $Revision$
 */
/**
 * @file
 * @brief Implementation of CodeGen routines for fields, statics and array
 *        accesses.
 */
 
#include "cg.h"
#include <open/vm.h>
#include "trace.h"

namespace Jitrino {
namespace Jet {

void CodeGen::gen_array_length(void)
{
    Opnd arr = vstack(0, true).as_opnd();
    rlock(arr);
    gen_check_null(0);
    vpop();
    vpush(Val(i32, arr.reg(), rt_array_length_offset));
    runlock(arr);
    // the array length is known to be non-negative, marking it as such
    vstack(0, VA_NZ);
}

void CodeGen::gen_arr_load(jtype jt)
{
    // stack: [.., aref, idx]
    // If index is not immediate, then force it to register
    Val& idx = vstack(0, !vis_imm(0));
    assert(idx.is_imm() || idx.is_reg());
    rlock(idx);
    // Force reference on register
    Val& arr = vstack(1, true);
    rlock(arr);
    // Runtime checks
    gen_check_null(1);
    gen_check_bounds(1, 0);
    
    vpop();
    vpop();
    // For compressed refs, the size is always 4
    unsigned size = jt==jobj && g_refs_squeeze ? 4 : jtypes[jt].size;
    AR base = arr.reg();
    int disp = jtypes[jt].rt_offset +
               (idx.is_imm() ? size*idx.ival() : 0);
    
    AR index = idx.is_imm() ? ar_x : idx.reg();
    unsigned scale = idx.is_imm() ? 0 : size;
    Opnd elem(jt, base, disp, index, scale);

    if (!is_ia32() && jt==jobj && g_refs_squeeze) {
        AR gr_base = valloc(jobj);
        rlock(gr_base);
        AR gr_ref = valloc(jobj);
        rlock(gr_ref);
        
        Opnd where32(i32, elem.base(), elem.disp(), 
                          elem.index(), elem.scale());
        mov(Opnd(i32, gr_ref), where32);
        //sx(Opnd(jobj, gr_ref), where32);
        movp(gr_base, OBJ_BASE);
        Opnd obj(jobj, gr_ref);
        alu(alu_add, obj, Opnd(jobj, gr_base));
        //
        runlock(gr_ref);
        runlock(gr_base);
        //
        runlock(arr);
        runlock(idx);
        vpush(obj);
    }
    else if (is_big(jt)) {
        AR ar_lo = valloc(jt);
        Opnd lo(jt, ar_lo);
        rlock(lo);
        
        do_mov(lo, elem);
        runlock(idx);
        
        AR ar_hi = valloc(jt);
        Opnd hi(jt, ar_hi);
        rlock(hi);
        Opnd elem_hi(jt, base, disp+4, index, scale);
        do_mov(hi, elem_hi);
        runlock(arr);
        vpush2(lo, hi);
        runlock(lo);
        runlock(hi);
    }
    else {
        jtype jtm = jt < i32 ? i32 : jt;
        runlock(idx);
        AR ar = valloc(jtm);
        Opnd val(jtm, ar);
        rlock(val);
        if (jt == i8)       { sx1(val, elem); }
        else if (jt == i16) { sx2(val, elem); }
        else if (jt == u16) { zx2(val, elem); }
        else                { do_mov(val, elem); }
        runlock(val);
        runlock(arr);
        vpush(val);
    }
}

void CodeGen::gen_arr_store(jtype jt, bool helperOk)
{
    vunref(jt);
    // stack: [.., aref, idx, val]
    if (jt == jobj && helperOk) {
        gen_write_barrier(m_curr_inst->opcode, NULL);
        static const CallSig cs_aastore(CCONV_HELPERS, jobj, i32, jobj);
        unsigned stackFix = gen_stack_to_args(true, cs_aastore, 0);
#ifdef _EM64T_
        // Huh ? Do we really have another order of args here ?
        AR gr = valloc(jobj);
        mov(gr, cs_aastore.reg(0));
        mov(cs_aastore.reg(0), cs_aastore.reg(2));
        mov(cs_aastore.reg(2), gr);
#endif
        gen_call_vm(cs_aastore, rt_helper_aastore, 3);
        if (stackFix != 0) {
            alu(alu_sub, sp, stackFix);
        }
        runlock(cs_aastore);
        return;
    }
    unsigned idx_depth = is_wide(jt) ? 2 : 1;
    unsigned ref_depth = idx_depth + 1;
    
    // Force reference on register
    const Val& arr = vstack(ref_depth, true);
    rlock(arr);
    // If index is not immediate, then force it to register
    const Val& idx = vstack(idx_depth, !vis_imm(idx_depth));
    assert(idx.is_imm() || idx.is_reg());
    rlock(idx);
    //
    //
    gen_check_null(ref_depth);
    gen_check_bounds(ref_depth, ref_depth-1);
    
    // Where to store
    AR base = arr.reg();
    int disp = jtypes[jt].rt_offset +
               (idx.is_imm() ? jtypes[jt].size*idx.ival() : 0);
    AR index = idx.is_imm() ? ar_x : idx.reg();
    unsigned scale = idx.is_imm() ? 0 : jtypes[jt].size;

    Opnd where(jt, base, disp, index, scale);
    // If we need to perform a narrowing convertion, then have the 
    // item on a register.
    const Val& val = vstack(0, vis_mem(0));
    rlock(val);
    if (is_big(jt)) {
        do_mov(where, val);
        runlock(idx);
        runlock(val);
        Opnd where_hi(jt, base, disp+4, index, scale);
        Val& val_hi = vstack(1);
        rlock(val_hi);
        do_mov(where_hi, val_hi);
        runlock(val_hi);
    }
    else if (jt<i32) {
        do_mov(where, val.as_opnd(jt));
        runlock(idx);
        runlock(val);
    }
    else {
        runlock(idx);
        runlock(val);
        do_mov(where, val);
    }
    
    runlock(arr);
    //
    vpop();
    vpop();
    vpop();
}

void CodeGen::gen_static_op(JavaByteCodes op, jtype jt, Field_Handle fld) {
    do_field_op(op, jt, fld);
}

void CodeGen::gen_field_op(JavaByteCodes op, jtype jt, Field_Handle fld)
{
    do_field_op(op, jt, fld);
}

void CodeGen::do_field_op(JavaByteCodes opcode, jtype jt, Field_Handle fld)
{
    bool field_op = false, get = false; // PUTSTATIC is default
    unsigned ref_depth = 0;
    if (opcode == OPCODE_PUTFIELD) {
        field_op = true;
        ref_depth = is_wide(jt) ? 2 : 1;
    }
    else if (opcode == OPCODE_GETFIELD) {
        field_op = true;
        get = true;
    }
    else if (opcode == OPCODE_GETSTATIC) {
        get = true;
    }
    else {
        assert(opcode == OPCODE_PUTSTATIC);
    }
    
    if(fld == NULL) {
        const JInst& jinst = *m_curr_inst;
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, jinst.op0, jinst.opcode);
    }
    
    bool fieldIsMagic = fld && field_is_magic(fld);
    if (fieldIsMagic) {
        jt = iplatf;
    }

    if (!get && compilation_params.exe_notify_field_modification && !fieldIsMagic)  {
        gen_modification_watchpoint(opcode, jt, fld);
    }

    if (!get && ! fieldIsMagic) {
        gen_write_barrier(opcode, fld);
    }

    
    Opnd where;
    if (field_op) {
        Val& ref = vstack(ref_depth, true);
        gen_check_null(ref_depth);
        unsigned fld_offset = fld ? field_get_offset(fld) : 0;
        where = Opnd(jt, ref.reg(), fld_offset);
    }
    else {
        // static s
        char * fld_addr = fld ? (char*)field_get_address(fld) : NULL;
        where = vaddr(jt, fld_addr);
    }
    rlock(where);
    
    // Presumption: we dont have compressed refs on IA32 and all other 
    // (64bits) platforms have compressed refs. 
    // is_ia32() check added below so on IA32 it becomes 'false' during the 
    // compilation, without access to g_refs_squeeze in runtime.
    assert(is_ia32() || g_refs_squeeze);
    

    if (get && compilation_params.exe_notify_field_access && !fieldIsMagic) {
        gen_access_watchpoint(opcode, jt, fld);
    }
    
    if (get) {

        if (field_op) {
            // pop out ref
            vpop();
        }
        if (!is_ia32() && g_refs_squeeze && (jt == jobj || fieldIsMagic)) {
            if (fieldIsMagic) {
                AR gr_ref = valloc(jobj);
                rlock(gr_ref);
                Opnd obj(jobj, gr_ref);
                mov(Opnd(jobj, gr_ref), where);
                runlock(gr_ref);
                vpush(obj);
            } else {
                AR gr_base = valloc(jobj);
                rlock(gr_base);
                AR gr_ref = valloc(jobj);
                rlock(gr_ref);

                Opnd where32(i32, where.base(), where.disp(), 
                              where.index(), where.scale());
                mov(Opnd(i32, gr_ref), where32);
                movp(gr_base, OBJ_BASE);
                Opnd obj(jobj, gr_ref);
                alu(alu_add, obj, Opnd(jobj, gr_base));
                //
                runlock(gr_ref);
                runlock(gr_base);
                //
                vpush(obj);
            }
        }
        else if (jt<i32) {
            AR gr = valloc(i32);
            Opnd reg(i32, gr);
            //
            if (jt == i8)       { sx1(reg, where); }
            else if (jt == i16) { sx2(reg, where); }
            else if (jt == u16) { zx2(reg, where); }
            //
            vpush(Val(i32, gr));
        }
        else {
            if (is_big(jt)){
                Opnd where_hi(jt, where.base(), where.disp()+4, 
                                  where.index(), where.scale());
                vpush2(where, where_hi);
            }
            else {
                vpush(where);
            }
        }
        runlock(where);
        return;
    } // if (get)
    
    vunref(jt);

    if (!is_ia32() && g_refs_squeeze && jt == jobj && vis_imm(0)) {
        const Val& s = m_jframe->dip(0);
        unsigned ref = (unsigned)(int_ptr)((const char*)s.pval() - OBJ_BASE);
        Opnd where32(i32, where.base(), where.disp(), 
                          where.index(), where.scale());
        mov(where32, Opnd(ref));
    }
    else if (!is_ia32() && g_refs_squeeze && (jt == jobj || fieldIsMagic)) {
        // have the reference on a register
        Val& s0 = vstack(0, true);
        rlock(s0);
        if (fieldIsMagic) {
            mov(where, s0.as_opnd());
        } else {
            // compress the reference
            AR tmp = valloc(jobj);
            void * inv_base = (void*)-(int_ptr)OBJ_BASE;
            movp(tmp, inv_base);
            alu(alu_add, Opnd(jobj, tmp), s0.as_opnd());
            // store the resulting int32
            Opnd where32(i32, where.base(), where.disp(), 
                          where.index(), where.scale());
            mov(where32, Opnd(jobj, tmp)); //s0.as_opnd(i32));
        }
        runlock(s0);
    }
    else if (jt<i32) {
        // No need to unref() - we just can't have jt()<i32 on the stack 
        Val& val = vstack(0, vis_mem(0));
        assert(val.jt() == i32);
        do_mov(where, val.as_opnd(jt));
    }
    else {
        vunref(jt, where);
        Val& val = vstack(0, vis_mem(0));
        do_mov(where, val);
        if (is_big(jt)) {
            Opnd where_hi(jt, where.base(), where.disp()+4, 
                              where.index(), where.scale());
            vunref(jt, where_hi);
            Opnd val_hi = vstack(1, vis_mem(1)).as_opnd();
            do_mov(where_hi, val_hi);
        }
    }
    
    runlock(where);

    
    vpop(); // pop out value
    if (field_op) {
        vpop(); // pop out ref
    }
}


}}; // ~namespace Jitrino::Jet
