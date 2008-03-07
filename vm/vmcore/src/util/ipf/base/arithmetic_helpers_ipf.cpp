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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  
//

//MVM
#include <iostream>

using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include "object_layout.h"
#include "open/types.h"
#include "Class.h"
#include "environment.h"
#include "exceptions.h"
#include "open/gc.h" 
#include "ini.h"
#include "open/vm_util.h"
#include "vm_threads.h"
#include "vm_stats.h"
#include "merced.h"
#include "Code_Emitter.h"
#include "stub_code_utils.h"
#include "vm_ipf.h"
#include "nogc.h"
#include "vm_arrays.h"

#include "jit_runtime_support_common.h"
#include "jit_runtime_support.h"
#include "mon_enter_exit.h"



void gen_vm_rt_athrow_internal_compactor(Merced_Code_Emitter &emitter);



void *get_vm_rt_athrow_naked_div_compactor()
{
    static void *addr = 0;
    if (addr) {
        return addr;
    }
    tl::MemoryPool mem_pool;
    Merced_Code_Emitter emitter(mem_pool, 2, 0);
    emitter.disallow_instruction_exchange();
    emitter.memory_type_is_unknown();
    
    // Control drops through to common code that does a procedure call to vm_rt_athrow().
    gen_vm_rt_athrow_internal_compactor(emitter);

    void *stub = finalize_stub(emitter, "rt_athrow_naked_div");
    return stub;
} //get_vm_rt_athrow_naked_div_compactor



int32 vm_rt_imul(int32 a, int32 b)
{
    int result = a * b;
    return result;
} //vm_rt_imul



int64 vm_rt_lmul(int64 a, int64 b)
{
    int64 result = a * b;
    return result;
} //vm_rt_lmul



int32 vm_rt_irem(int32 a, int32 b)
{
    assert(b);
    int32 result = a % b;
    return result;
} //vm_rt_irem



int64 vm_rt_lrem(int64 a, int64 b)
{
    assert(b);
    int64 result = a % b;
    return result;
} //vm_rt_lrem



int32 vm_rt_idiv(int32 a, int32 b)
{
    assert(b);
    int32 result = a / b;
    return result;
} //vm_rt_idiv



int64 vm_rt_ldiv(int64 a, int64 b)
{
    assert(b);
    int64 result = a / b;
    return result;
} //vm_rt_ldiv



float vm_rt_frem(float a, float b)
{
    float result = (float) fmod(a, b);
    return result;
} //vm_rt_fdiv



float vm_rt_fdiv(float a, float b)
{
    float result = a / b;
    return result;
} //vm_rt_fdiv



double vm_rt_drem(double a, double b)
{
    double result = fmod(a, b);
    return result;
} //vm_rt_ddiv



double vm_rt_ddiv(double a, double b)
{
    double result = a / b;
    return result;
} //vm_rt_ddiv

// gashiman - fix _isnan on linux
#ifdef PLATFORM_POSIX
#define _isnan isnan
#endif

int32 vm_rt_f2i(float f)
{
    int32 result;
    if(_isnan(f)) {
        result = 0;
    } else if(f > (double)2147483647) {
        result = 2147483647;      // maxint
    } else if(f < -(double)2147483648) {
        result = 0x80000000;     // minint
    } else {
        // The above should exhaust all possibilities
        result = (int32)f;
    }
    return result;
} //vm_rt_f2i



int64 vm_rt_f2l(float f)
{
    printf("vm_rt_f2l hasn't been tested yet\n");
    int64 result;
    if(_isnan(f)) {
        result = 0;
    } else if(f > (double)0x7fffffffffffffff) {
        result = 0x7fffffffffffffff;      // maxint
    } else if(f < -(double)0x8000000000000000) {
        result = 0x8000000000000000;     // minint
    } else {
        // The above should exhaust all possibilities
        result = (int64)f;
    }
    return result;
} //vm_rt_f2l



void *get_vm_rt_int_div_address_compactor(void *func, char *stub_name)
{
    void *addr = 0;
    tl::MemoryPool mem_pool;
    Merced_Code_Emitter emitter(mem_pool, 2, 0);
    emitter.disallow_instruction_exchange();
    emitter.memory_type_is_unknown();
    
    emitter.ipf_cmp(icmp_eq, cmp_none, SCRATCH_PRED_REG2, SCRATCH_PRED_REG, 33, 0, false);
    
    uint64 func_addr = (uint64)*(void **)func;
    emit_movl_compactor(emitter, SCRATCH_GENERAL_REG, func_addr);
    emitter.ipf_mtbr(SCRATCH_BRANCH_REG, SCRATCH_GENERAL_REG);
    emitter.ipf_bri(br_cond, br_many, br_sptk, br_none, SCRATCH_BRANCH_REG, SCRATCH_PRED_REG);

    //The following may be broken...
    uint64 vm_rt_athrow_naked_div_addr = (uint64)get_vm_rt_athrow_naked_div_compactor();

    emit_movl_compactor(emitter, SCRATCH_GENERAL_REG, vm_rt_athrow_naked_div_addr);
    
    emit_movl_compactor(emitter, 33, (uint64)VM_Global_State::loader_env->java_lang_ArithmeticException_Class);
    
    emitter.ipf_add(32, 0, 0);

    emitter.ipf_mtbr(SCRATCH_BRANCH_REG, SCRATCH_GENERAL_REG);
    emitter.ipf_bri(br_cond, br_many, br_sptk, br_none, SCRATCH_BRANCH_REG);

    addr = finalize_stub(emitter, stub_name);
    return addr;
} //get_vm_rt_int_div_address_compactor



void *get_vm_rt_lrem_address()
{
    static void *addr = 0;
    if(addr) {
        return addr;
    }
    
    // gashiman - added (void*) to make it compile on gcc
    addr = get_vm_rt_int_div_address_compactor((void*)vm_rt_lrem, "rt_lrem");

    return addr;
} //get_vm_rt_lrem_address



void *get_vm_rt_ldiv_address()
{
    static void *addr = 0;
    if(addr) {
        return addr;
    }

    // gashiman - added (void*) to make it compile on gcc
    addr = get_vm_rt_int_div_address_compactor((void*)vm_rt_ldiv, "rt_ldiv");

    return addr;
} //get_vm_rt_ldiv_address



void *get_vm_rt_irem_address()
{
    static void *addr = 0;
    if(addr) {
        return addr;
    }

    // gashiman - added (void*) to make it compile on gcc
    addr = get_vm_rt_int_div_address_compactor((void*)vm_rt_irem, "rt_irem");

    return addr;
} //get_vm_rt_irem_address



void *get_vm_rt_idiv_address()
{
    static void *addr = 0;
    if(addr) {
        return addr;
    }

    // gashiman - added (void*) to make it compile on gcc
    addr = get_vm_rt_int_div_address_compactor((void*)vm_rt_idiv, "rt_idiv");

    return addr;
} //get_vm_rt_idiv_address



