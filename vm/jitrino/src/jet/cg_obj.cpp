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

#include <malloc.h>
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

void CodeGen::gen_new_array(Allocation_Handle ah)
{
    const JInst& jinst = *m_curr_inst;
    assert(jinst.opcode == OPCODE_NEWARRAY 
        ||  jinst.opcode == OPCODE_ANEWARRAY 
        || jinst.opcode == OPCODE_INVOKESTATIC); //OPCODE_INVOKESTATIC is used to generate magic

    if (ah == 0) {
        // it's unexpected that that something failed for a primitive type
        assert(jinst.opcode != OPCODE_NEWARRAY);
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, jinst.op0, jinst.opcode);
    }
#ifdef _EM64T_
    static const CallSig cs_new_arr(CCONV_HELPERS, i32, jobj);
    unsigned stackFix = gen_stack_to_args(true, cs_new_arr, 0, 1);
    gen_call_vm(cs_new_arr, rt_helper_new_array, 1, ah);
    runlock(cs_new_arr);
#else
    static const CallSig cs_new_arr(CCONV_HELPERS, jobj, i32);
    rlock(cs_new_arr);
    AR artmp = valloc(jobj);
    rlock(artmp);
    Encoder::gen_args(cs_new_arr, artmp, 0, 1, ah);
    unsigned stackFix = gen_stack_to_args(true, cs_new_arr, 1, 1);
    runlock(artmp);
    gen_call_vm(cs_new_arr, rt_helper_new_array, cs_new_arr.count());
    runlock(cs_new_arr);
#endif
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
    gen_save_ret(jobj);
    // the returned can not be null, marking as such.
    vstack(0).set(VA_NZ);
    // allocation assumes GC invocation
    m_bbstate->seen_gcpt = true;
}

void CodeGen::gen_multianewarray(Class_Handle klass, unsigned num_dims)
{
    // stack: [..., count1, [...count N] ]
    // args: (klassHandle, num_dims, count_n, ... count_1)
    // native stack to be established (ia32):  
    //          .., count_1, .. count_n, num_dims, klassHandle
    
    const JInst& jinst = *m_curr_inst;

    vector<jtype> args;
    for (unsigned i = 0; i<num_dims; i++) {
        args.push_back(i32);
    }
    args.push_back(i32);
    args.push_back(jobj);
        
    // note: need to restore the stack - the cdecl-like function
    CallSig ci(CCONV_CDECL_IA32|CCONV_MEM|CCONV_L2R|CCONV_CALLER_POPS, args);
    unsigned stackFix = gen_stack_to_args(true, ci, 0, num_dims);
    
    if (klass == NULL) {
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, jinst.op0, jinst.opcode);
    }
    
    gen_call_vm(ci, rt_helper_multinewarray, num_dims, num_dims, klass);
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


void CodeGen::gen_new(Class_Handle klass)
{
    const JInst& jinst = *m_curr_inst;
    if (klass == NULL) {
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, jinst.op0, jinst.opcode);
    }
    else {
        unsigned size = class_get_boxed_data_size(klass);
        Allocation_Handle ah = class_get_allocation_handle(klass);
        static CallSig ci_new(CCONV_STDCALL, i32, jobj);
        gen_call_vm(ci_new, rt_helper_new, 0, size, ah);
    }
    gen_save_ret(jobj);
    // the returned can not be null, marking as such.
    vstack(0).set(VA_NZ);
    // allocation assumes GC invocation
    m_bbstate->seen_gcpt = true;
}

void CodeGen::gen_instanceof_cast(bool checkcast, Class_Handle klass)
{
    const JInst& jinst = *m_curr_inst;
    if (klass == NULL) {
        gen_call_throw(ci_helper_linkerr, rt_helper_throw_linking_exc, 0,
                       m_klass, jinst.op0, jinst.opcode);
    }
    static const CallSig cs(CCONV_STDCALL, jobj, jobj);
    unsigned stackFix = gen_stack_to_args(true, cs, 0, 1);
    char * helper = checkcast ? rt_helper_checkcast : rt_helper_instanceof;
    gen_call_vm(cs, helper, 1, klass);
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
    runlock(cs);
    gen_save_ret(checkcast ? jobj : i32);
}

void CodeGen::gen_monitor_ee(void)
{
    const JInst& jinst = *m_curr_inst;
    gen_check_null(0);
    static const CallSig cs_mon(CCONV_MANAGED, jobj);
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
    static const CallSig cs_throw(CCONV_MANAGED, jobj);
    unsigned stackFix = gen_stack_to_args(true, cs_throw, 0);
    gen_call_vm(cs_throw, rt_helper_throw, 1);
    runlock(cs_throw);
    if (stackFix != 0) {
        alu(alu_sub, sp, stackFix);
    }
}





}}; // ~namespace Jitrino::Jet
