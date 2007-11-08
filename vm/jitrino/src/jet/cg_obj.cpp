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
#include "compiler.h"
#include "trace.h"

#ifdef WIN32
#include <malloc.h>
#endif
#include <memory.h>
#include <assert.h>
#include <stdlib.h>

#include <jit_import.h>

/**
 * @file
 * @brief Object creation, monitors and cast checks.
 */

namespace Jitrino {
namespace Jet {

void CodeGen::gen_new_array(Class_Handle enclClass, unsigned cpIndex) 
{
    bool lazy = m_lazy_resolution;
    bool resolve = !lazy || class_is_cp_entry_resolved(m_compileHandle, enclClass, cpIndex);
    if (resolve) {
        Allocation_Handle ah = 0;
        Class_Handle klass = resolve_class(m_compileHandle, enclClass,  cpIndex);
        if (klass != NULL) {
            klass = class_get_array_of_class(klass);
        }
        if (klass != NULL) {
            ah = class_get_allocation_handle(klass);
        }
        gen_new_array(ah);
        return;
    } 
    assert(lazy);
    static const CallSig cs_newarray_withresolve(CCONV_HELPERS, iplatf, i32, i32);
    Val sizeVal = vstack(0);
    // setup constant parameters first,
    Val vclass(iplatf, enclClass);
    Val vcpIdx((int)cpIndex);
    gen_args(cs_newarray_withresolve, 0, &vclass, &vcpIdx, &sizeVal);
    gen_call_vm(cs_newarray_withresolve, rt_helper_new_array_withresolve, 3);
    vpop();// pop array size
    gen_save_ret(jobj);

    // the returned can not be null, marking as such.
    vstack(0).set(VA_NZ);
    // allocation assumes GC invocation
    m_bbstate->seen_gcpt = true;
}

void CodeGen::gen_new_array(Allocation_Handle ah) {
    const JInst& jinst = *m_curr_inst;
    assert(jinst.opcode == OPCODE_NEWARRAY ||  jinst.opcode == OPCODE_ANEWARRAY);
        
    if (ah == 0) {
        assert(!m_lazy_resolution);
        // it's unexpected that that something failed for a primitive type
        assert(jinst.opcode != OPCODE_NEWARRAY);
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, jinst.op0, jinst.opcode);
    }
    static const CallSig cs_new_arr(CCONV_HELPERS, i32, jobj);
    unsigned stackFix = gen_stack_to_args(true, cs_new_arr, 0, 1);
    gen_call_vm(cs_new_arr, rt_helper_new_array, 1, ah);
    runlock(cs_new_arr);
    
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
    gen_save_ret(jobj);
    // the returned can not be null, marking as such.
    vstack(0).set(VA_NZ);
    // allocation assumes GC invocation
    m_bbstate->seen_gcpt = true;
}

void CodeGen::gen_multianewarray(Class_Handle enclClass, unsigned short cpIndex, unsigned num_dims)
{
    // stack: [..., count1, [...count N] ]
    // args: (klassHandle, num_dims, count_n, ... count_1)
    // native stack to be established (ia32):  
    //          .., count_1, .. count_n, num_dims, klassHandle
    
    vector<jtype> args;
    for (unsigned i = 0; i<num_dims; i++) {
        args.push_back(i32);
    }
    args.push_back(i32);
    args.push_back(jobj);
        
    Class_Handle klass = NULL;
    Val klassVal;

    bool lazy = m_lazy_resolution;
    bool resolve = !lazy || class_is_cp_entry_resolved(m_compileHandle, enclClass, cpIndex);
    if(!resolve) {
        assert(lazy);
        static CallSig ci_get_class_withresolve(CCONV_HELPERS, iplatf, i32);
        gen_call_vm(ci_get_class_withresolve, rt_helper_get_class_withresolve, 0, enclClass, cpIndex);
        runlock(ci_get_class_withresolve);
        klassVal = Val(jobj, gr_ret);
    }  else {
        klass = resolve_class(m_compileHandle, enclClass, cpIndex);
        klassVal = Val(jobj, klass);
    }
    rlock(klassVal); // to protect gr_ret while setting up helper args

    // note: need to restore the stack - the cdecl-like function
    CallSig ci(CCONV_CDECL_IA32|CCONV_MEM|CCONV_L2R|CCONV_CALLER_POPS, args);
    unsigned stackFix = gen_stack_to_args(true, ci, 0, num_dims);

    runlock(klassVal);

    if (klass == NULL && !lazy) {
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, cpIndex, OPCODE_MULTIANEWARRAY);
    }
    Val vnum_dims = Opnd(num_dims);
    gen_args(ci, num_dims, &vnum_dims, &klassVal);

    gen_call_vm(ci, rt_helper_multinewarray, 2+num_dims);
    runlock(ci);
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
    gen_save_ret(jobj);
    // the returned can not be null, marking as such.
    vstack(0).set(VA_NZ);
    // allocation assumes GC invocation
    m_bbstate->seen_gcpt = true;
}


void CodeGen::gen_new(Class_Handle enclClass, unsigned short cpIndex)
{
    bool lazy = m_lazy_resolution;
    bool resolve = !lazy || class_is_cp_entry_resolved(m_compileHandle, enclClass, cpIndex);
    if (resolve) {
        Class_Handle klass = resolve_class_new(m_compileHandle, enclClass, cpIndex);
        if (klass == NULL) {
            gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0, enclClass, cpIndex, OPCODE_NEW);
        } else {
            if ( klass!=enclClass && class_needs_initialization(klass)) {
                gen_call_vm(ci_helper_o, rt_helper_init_class, 0, klass);
            }
            unsigned size = class_get_boxed_data_size(klass);
            Allocation_Handle ah = class_get_allocation_handle(klass);
            static CallSig ci_new(CCONV_HELPERS, i32, jobj);
            gen_call_vm(ci_new, rt_helper_new, 0, size, ah);
        }
    } else {
        assert(lazy);
        static CallSig ci_new_with_resolve(CCONV_HELPERS, iplatf, i32);
        gen_call_vm(ci_new_with_resolve, rt_helper_new_withresolve, 0, enclClass, cpIndex);
    }
    gen_save_ret(jobj);
    vstack(0).set(VA_NZ);// the returned can not be null, marking as such.
    m_bbstate->seen_gcpt = true;// allocation assumes GC invocation

}

void CodeGen::gen_instanceof_cast(JavaByteCodes opcode, Class_Handle enclClass, unsigned short cpIdx)
{
    assert (opcode == OPCODE_INSTANCEOF || opcode == OPCODE_CHECKCAST);
    bool lazy = m_lazy_resolution;
    bool resolve  = !lazy || class_is_cp_entry_resolved(m_compileHandle, enclClass, cpIdx);
    if (resolve) {
        Class_Handle klass = resolve_class(m_compileHandle, enclClass, cpIdx);
        if (klass == NULL) {
            assert(!lazy);
            gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0, enclClass, cpIdx, opcode);
        }
        static const CallSig cs(CCONV_HELPERS, jobj, jobj);
        unsigned stackFix = gen_stack_to_args(true, cs, 0, 1);
        char * helper = opcode == OPCODE_CHECKCAST ? rt_helper_checkcast : rt_helper_instanceof;
        gen_call_vm(cs, helper, 1, klass);
        if (stackFix != 0) {
            alu(alu_sub, sp, stackFix);
        }
        runlock(cs);
        gen_save_ret(opcode == OPCODE_CHECKCAST ? jobj : i32);
    } else {
        assert(lazy);
        static const CallSig cs_with_resolve(CCONV_HELPERS, iplatf, i32, jobj);
        char * helper = opcode == OPCODE_CHECKCAST ? rt_helper_checkcast_withresolve : rt_helper_instanceof_withresolve;
        Val tos = vstack(0);
        // setup constant parameters first,
        Val vclass(iplatf, enclClass);
        Val vcpIdx(cpIdx);
        gen_args(cs_with_resolve, 0, &vclass, &vcpIdx, &tos);
        gen_call_vm(cs_with_resolve, helper, 3);
        runlock(cs_with_resolve);
        vpop();//pop obj
        gen_save_ret(opcode == OPCODE_CHECKCAST ? jobj : i32);
    }
}

void CodeGen::gen_monitor_ee(void)
{
    const JInst& jinst = *m_curr_inst;
    gen_check_null(0);
    static const CallSig cs_mon(CCONV_HELPERS, jobj);
    unsigned stackFix = gen_stack_to_args(true, cs_mon, 0);
    gen_call_vm(cs_mon,
            jinst.opcode == OPCODE_MONITORENTER ? 
                rt_helper_monitor_enter : rt_helper_monitor_exit, 1);
    runlock(cs_mon);
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
}

void CodeGen::gen_athrow(void)
{
    static const CallSig cs_throw(CCONV_HELPERS, jobj);
    unsigned stackFix = gen_stack_to_args(true, cs_throw, 0);
    gen_call_vm(cs_throw, rt_helper_throw, 1);
    runlock(cs_throw);
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
}





}}; // ~namespace Jitrino::Jet
