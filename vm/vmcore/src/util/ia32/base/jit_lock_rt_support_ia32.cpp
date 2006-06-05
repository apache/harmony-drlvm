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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.2.4.3 $
 */  


#include "platform_lowlevel.h"

//MVM
#include <iostream>

using namespace std;

#include <assert.h>

#include "vm_synch.h"
#include "open/vm_util.h"
#include "encoder.h"
#include "vm_stats.h"
#include "nogc.h"
#include "compile.h"

#include "exceptions.h"
#include "lil.h"
#include "lil_code_generator.h"
#include "../m2n_ia32_internal.h"
#include "object_handles.h"
#include "open/thread.h"
#include "Class.h"

#ifdef VM_STATS
#include "jit_runtime_support.h"
#endif

#ifndef NDEBUG
#include "dump.h"
extern bool dump_stubs;
#endif

char *gen_setup_j2n_frame(char *s);
char *gen_pop_j2n_frame(char *s);



// patch_addr_null_arg_ptr is the address of a variable holding the
// address of a branch instruction byte to patch with the destination
// to be taken if the struct Class* argument is NULL.
static char * gen_convert_struct_class_to_object(char *ss, char **patch_addr_null_arg_ptr)
{    
    // First make sure the struct Class* argument is non-NULL.
    ss = mov(ss,  ecx_opnd,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    ss = test(ss,  ecx_opnd,   ecx_opnd);
    ss = branch8(ss, Condition_Z,  Imm_Opnd(size_8, 0));
    *patch_addr_null_arg_ptr = ((char *)ss) - 1;

    // Convert the struct Class* argument to the corresponding java_lang_Class reference. 
    ss = push(ss,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    ss = call(ss, (char *)struct_Class_to_java_lang_Class);
    ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(4));
    ss = mov(ss,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET),  eax_opnd);  // overwrite the struct Class* with the raw java_lang_Class reference
    return ss;
} //gen_convert_struct_class_to_object


static char * gen_restore_monitor_enter(char *ss, char *patch_addr_null_arg)
{
    const unsigned header_offset = ManagedObject::header_offset();
#ifdef VM_STATS
    ss = inc(ss,  M_Opnd((unsigned)&(vm_stats_total.num_monitor_enter)));
#endif

#ifdef USE_TLS_API
    ss = call(ss, (char *)get_self_stack_key);
    ss = mov(ss,  edx_opnd,  eax_opnd);
#else //USE_TLS_API
    *ss++ = (char)0x64;
    *ss++ = (char)0xa1;
    *ss++ = (char)0x14;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    ss = mov(ss,  edx_opnd,  M_Base_Opnd(eax_reg, (uint32)&((VM_thread *)0)->stack_key) );
#endif //!USE_TLS_API
    
    ss = alu(ss, xor_opc,  eax_opnd,  eax_opnd);
    ss = mov(ss,  ecx_opnd,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    
    ss = test(ss,  ecx_opnd,   ecx_opnd);
    ss = branch8(ss, Condition_Z,  Imm_Opnd(size_8, 0));
    char *backpatch_address__null_pointer = ((char *)ss) - 1;

    ss = prefix(ss, lock_prefix);   
    ss = cmpxchg(ss,  M_Base_Opnd(ecx_reg, header_offset + STACK_KEY_OFFSET),  edx_opnd, size_16);
    ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));
    char *backpatch_address__fast_monitor_failed = ((char *)ss) - 1;

    ss = ret(ss,  Imm_Opnd(4));

    signed offset = (signed)ss - (signed)backpatch_address__fast_monitor_failed - 1;
    *backpatch_address__fast_monitor_failed = (char)offset;

    ss = gen_setup_j2n_frame(ss);
    ss = push(ss,  M_Base_Opnd(esp_reg, m2n_sizeof_m2n_frame));
 
/*
     // packing heap pointer to handle -salikh
    ss = call(ss, (char *)oh_convert_to_local_handle);
    ss = alu(ss, add_opc, esp_opnd, Imm_Opnd(4)); // pop parameters
    ss = push(ss, eax_opnd); // push the address of the handle
    ss = call(ss, (char *)tmn_suspend_enable); // enable gc
    // -salikh

    ss = call(ss, (char *)vm_monitor_enter_slow_handle);
    ss = alu(ss, add_opc, esp_opnd, Imm_Opnd(4)); // pop parameters

    // disable gc afterwards -salikh
    ss = call(ss, (char *)tmn_suspend_disable); // disable gc
    // -salikh
*/
    ss = call(ss, (char *)vm_monitor_enter_slow);
    ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(4));
  
    ss = gen_pop_j2n_frame(ss);
    ss = ret(ss,  Imm_Opnd(4));

    offset = (signed)ss - (signed)backpatch_address__null_pointer - 1;
    *backpatch_address__null_pointer = (char)offset;
    if (patch_addr_null_arg != NULL) {
        offset = (signed)ss - (signed)patch_addr_null_arg - 1;
        *patch_addr_null_arg = (char)offset;
    }
    // Object is null so throw a null pointer exception
    ss = jump(ss, (char*)exn_get_rth_throw_null_pointer());

    return ss;
} //gen_restore_monitor_enter


void * restore__vm_monitor_enter_naked(void * code_addr)
{
    char *stub = (char *)code_addr;

#ifdef _DEBUG
    const int stub_size = 86;
    memset(stub, 0xcc, stub_size);
#endif
    char *ss = stub;

    ss = gen_restore_monitor_enter(ss, /*patch_addr_null_arg*/ NULL);

    assert((ss - stub) < stub_size);
#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_enter_naked_mt", ss - stub);
#endif
    return code_addr;
} //restore__vm_monitor_enter_naked


void * restore__vm_monitor_enter_static_naked(void * code_addr)
{
    char *stub = (char *)code_addr;

#ifdef _DEBUG
    const int stub_size = 107;
    memset(stub, 0xcc, stub_size);
#endif
    char *ss = stub;

    char *patch_addr_null_arg;
    ss = gen_convert_struct_class_to_object(ss, &patch_addr_null_arg);
    ss = gen_restore_monitor_enter(ss, patch_addr_null_arg);

    assert((ss - stub) < stub_size);
#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_enter_static_naked_mt", ss - stub);
#endif
    return code_addr;
} //restore__vm_monitor_enter_static_naked


static char * gen_restore_monitor_exit(char *ss, char *patch_addr_null_arg)
{
    const unsigned header_offset = ManagedObject::header_offset();
#ifdef VM_STATS
    ss = inc(ss,  M_Opnd((unsigned)&(vm_stats_total.num_monitor_exit)));
#endif

#define CHECK_FOR_ILLEGAL_MONITOR_STATE_EXCEPTION
#ifdef CHECK_FOR_ILLEGAL_MONITOR_STATE_EXCEPTION
#ifdef USE_TLS_API
    ss = call(ss, (char *)get_self_stack_key);
    ss = shift(ss, shl_opc,  eax_opnd,  Imm_Opnd(16));
#else
//#error This code has not been tested.
    ss = mov(ss,  eax_opnd,  esp_opnd);
    ss = alu(ss, and_opc,  eax_opnd,  Imm_Opnd(0xffff0000));
#endif //#ifdef USE_TLS_API else
    ss = mov(ss,  ecx_opnd,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    ss = alu(ss, xor_opc,  eax_opnd,  M_Base_Opnd(ecx_reg, header_offset));
    ss = test(ss,  eax_opnd,  Imm_Opnd(0xffffff80));
    ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));
    char *backpatch_address__slow_path = ((char *)ss) - 1;
    ss = alu(ss, and_opc,  M_Base_Opnd(ecx_reg, header_offset),  Imm_Opnd(0x0000ffff));
    ss = ret(ss,  Imm_Opnd(4));
    signed offset1 = (signed)ss-(signed)backpatch_address__slow_path - 1;
    *backpatch_address__slow_path = (char)offset1;
#else // !CHECK_FOR_ILLEGAL_MONITOR_STATE_EXCEPTION
    ss = mov(ss,  ecx_opnd,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    // The current ia32.cpp doesn't support a 16-bit test instruction, so
    // we hack it here.
    *ss = 0x66; ss++; // prefix 0x66 means use the 16-bit form instead of the 32-bit form
    ss = test(ss,  M_Base_Opnd(ecx_reg, header_offset),  Imm_Opnd(0xff80));
    ss -= 2; // Adjust for a 16-bit immediate instead of a 32-bit immediate.
    ss = branch8(ss, cc_ne,  Imm_Opnd(size_8, 0), 0);
    char *backpatch_address__slow_path = ((char *)ss) - 1;
    ss = mov(ss,  M_Base_Opnd(ecx_reg, header_offset + STACK_KEY_OFFSET),  Imm_Opnd(0), opnd_16);
    ss = ret(ss,  Imm_Opnd(4));
    signed offset1 = (signed)ss-(signed)backpatch_address__slow_path - 1;
    *backpatch_address__slow_path = offset1;
#endif // !CHECK_FOR_ILLEGAL_MONITOR_STATE_EXCEPTION

#ifdef USE_TLS_API
    ss = call(ss, (char *)get_self_stack_key);
    ss = mov(ss,  edx_opnd,  eax_opnd);
#else //USE_TLS_API
    *ss++ = (char)0x64;
    *ss++ = (char)0xa1;
    *ss++ = (char)0x14;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    ss = mov(ss,  edx_opnd,  M_Base_Opnd(eax_reg, (uint32)&((VM_thread *)0)->stack_key) );
#endif //!USE_TLS_API

    ss = mov(ss,  ecx_opnd,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    ss = mov(ss,  eax_opnd,  M_Base_Opnd(ecx_reg, header_offset + STACK_KEY_OFFSET ), size_16);
    ss = alu(ss, cmp_opc,  eax_opnd,  edx_opnd, size_16);
    ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));
    char *backpatch_address__illegal_monitor_failed = ((char *)ss) - 1;

    ss = mov(ss,  eax_opnd,  M_Base_Opnd(ecx_reg,header_offset + HASH_CONTENTION_AND_RECURSION_OFFSET), size_16);

    // need to code AH: ESP & size_8 -> AH
    ss = alu(ss, cmp_opc,  esp_opnd,  Imm_Opnd(size_8, 0), size_8);

    ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));
    char *backpatch_address__recursed_monitor_failed = ((char *)ss) - 1;

    //release the lock

    ss = mov(ss,  edx_opnd,  Imm_Opnd(0));
    ss = mov(ss,  M_Base_Opnd(ecx_reg, header_offset + STACK_KEY_OFFSET),  edx_opnd, size_16);

    ss = mov(ss,  edx_opnd,  eax_opnd);
    ss = alu(ss, and_opc,  edx_opnd,  Imm_Opnd(0x80));

    ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));
    char *backpatch_address__contended_monitor_failed = ((char *)ss) - 1;
    ss = ret(ss,  Imm_Opnd(4));

    signed 
    offset = (signed)ss-(signed)backpatch_address__contended_monitor_failed - 1;
    *backpatch_address__contended_monitor_failed = (char)offset;
    ss = push(ss,  M_Base_Opnd(esp_reg, INPUT_ARG_OFFSET));
    ss = call(ss, (char *)find_an_interested_thread);
    ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(4));
    ss = ret(ss,  Imm_Opnd(4));

    offset = (signed)ss-(signed)backpatch_address__recursed_monitor_failed - 1;
    *backpatch_address__recursed_monitor_failed = (char)offset;
    // esp_opnd & size_8 translates to AH
    ss = dec(ss, esp_opnd,  size_8);
    ss = mov(ss,   M_Base_Opnd(ecx_reg, header_offset + RECURSION_OFFSET),  esp_opnd, size_8);
    ss = ret(ss,  Imm_Opnd(4));

    offset = (signed)ss-(signed)backpatch_address__illegal_monitor_failed - 1;
    *backpatch_address__illegal_monitor_failed = (char)offset;
    if (patch_addr_null_arg != NULL) {
        offset = (signed)ss - (signed)patch_addr_null_arg - 1;
        *patch_addr_null_arg = (char)offset;
    }
    ss = gen_setup_j2n_frame(ss);
    ss = push(ss,  M_Base_Opnd(esp_reg, m2n_sizeof_m2n_frame));

 /*
     // packing heap pointer to handle -salikh
    ss = call(ss, (char *)oh_convert_to_local_handle);
    ss = alu(ss, add_opc, esp_opnd, Imm_Opnd(4)); // pop parameters
    ss = push(ss, eax_opnd); // push the address of the handle
    ss = call(ss, (char *)tmn_suspend_enable); // enable gc
    // -salikh

    ss = call(ss, (char *)vm_monitor_exit_handle);
    ss = alu(ss, add_opc, esp_opnd, Imm_Opnd(4));

    // disable gc afterwards -salikh
    ss = call(ss, (char *)tmn_suspend_disable); // disable gc
    // -salikh
*/
    ss = call(ss, (char *)vm_monitor_exit);
    ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(4));
    
    return ss;
} //gen_restore_monitor_exit


void * restore__vm_monitor_exit_naked(void * code_addr)
{
    char *stub = (char *)code_addr;

#ifdef _DEBUG
    const int stub_size = /*106*/210;
    memset(stub, 0xcc, stub_size);
#endif
    char *ss = stub;

    ss = gen_restore_monitor_exit(ss, /*patch_addr_null_arg*/ NULL);

    assert((ss - stub) < stub_size);
#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_exit_naked_mt", ss - stub);
#endif
    return code_addr; 
} //restore__vm_monitor_exit_naked


void * restore__vm_monitor_exit_static_naked(void * code_addr)
{
    char *stub = (char *)code_addr;

#ifdef _DEBUG
    const int stub_size = /*106*/210;
    memset(stub, 0xcc, stub_size);
#endif
    char *ss = stub;

    char *patch_addr_null_arg;
    ss = gen_convert_struct_class_to_object(ss, &patch_addr_null_arg);
    ss = gen_restore_monitor_exit(ss, patch_addr_null_arg);

    assert((ss - stub) < stub_size);
#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_exit_static_naked_mt", ss - stub);
#endif
    return code_addr; 
} //restore__vm_monitor_exit_static_naked


void * getaddress__vm_monitor_enter_naked()
{
    static void *addr = NULL;
    if (addr != NULL) {
        return addr;
    }

    const int stub_size = 126;
    char *stub = (char *)malloc_fixed_code_for_jit(stub_size, DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_MAX/2, CAA_Allocate);
#ifdef _DEBUG
    memset(stub, 0xcc /*int 3*/, stub_size);
#endif
    char *ss = stub;

#ifdef VM_STATS
    int * value = vm_stats_total.rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_ENTER, 0, NULL);
    ss = inc(ss,  M_Opnd((unsigned)value));
#endif

    ss = gen_restore_monitor_enter(ss, /*patch_addr_null_arg*/ NULL);

    addr = stub;
    assert((ss - stub) < stub_size);

    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        jvmti_add_dynamic_generated_code_chunk("vm_monitor_enter_naked", stub, stub_size);
        jvmti_send_dynamic_code_generated_event("vm_monitor_enter_naked", stub, stub_size);
    }


#ifndef NDEBUG
    if (dump_stubs) 
        dump(stub, "getaddress__vm_monitor_enter_naked", ss - stub);
#endif
    return addr;
}


void * getaddress__vm_monitor_enter_static_naked()
{    
    static void *addr = NULL;
    if (addr != NULL) {
        return addr;
    }

    const int stub_size = 150;
    char *stub = (char *)malloc_fixed_code_for_jit(stub_size, DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_MAX/2, CAA_Allocate);
#ifdef _DEBUG
    memset(stub, 0xcc /*int 3*/, stub_size);
#endif
    char *ss = stub;

#ifdef VM_STATS
    int * value = vm_stats_total.rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_ENTER_STATIC, 0, NULL);
    ss = inc(ss,  M_Opnd((unsigned)value));
#endif

    char *patch_addr_null_arg;
    ss = gen_convert_struct_class_to_object(ss, &patch_addr_null_arg);
    ss = gen_restore_monitor_enter(ss, patch_addr_null_arg);    

    addr = stub;
    assert((ss - stub) < stub_size);

    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        jvmti_add_dynamic_generated_code_chunk("vm_monitor_enter_static_naked", stub, stub_size);
        jvmti_send_dynamic_code_generated_event("vm_monitor_enter_static_naked", stub, stub_size);
    }

    
#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_enter_static_naked", ss - stub);
#endif
    return addr;
} //getaddress__vm_monitor_enter_static_naked


void * getaddress__vm_monitor_exit_naked()
{
    static void *addr = NULL;
    if (addr != NULL) {
        return addr;
    }

    const int stub_size = /*126*/210;
    char *stub = (char *)malloc_fixed_code_for_jit(stub_size, DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_MAX/2, CAA_Allocate);
    char *ss = stub;

#ifdef VM_STATS
    int * value = vm_stats_total.rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_EXIT, 0, NULL);
    ss = inc(ss,  M_Opnd((unsigned)value));
#endif

    ss = (char *)gen_convert_managed_to_unmanaged_null_ia32((Emitter_Handle)ss, /*stack_pointer_offset*/ INPUT_ARG_OFFSET);
    ss = gen_restore_monitor_exit(ss, /*patch_addr_null_arg*/ NULL);

    addr = stub;
    assert((ss - stub) < stub_size);

    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        jvmti_add_dynamic_generated_code_chunk("vm_monitor_enter_naked", stub, stub_size);
        jvmti_send_dynamic_code_generated_event("vm_monitor_enter_naked", stub, stub_size);
    }

#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_exit_naked", ss - stub);
#endif
    return addr;
} //getaddress__vm_monitor_exit_naked


void * getaddress__vm_monitor_exit_static_naked()
{
    static void *addr = NULL;
    if (addr != NULL) {
        return addr;
    }

    const int stub_size = /*126*/210;
    char *stub = (char *)malloc_fixed_code_for_jit(stub_size, DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_MAX/2, CAA_Allocate);
    char *ss = stub;

#ifdef VM_STATS
    int * value = vm_stats_total.rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_EXIT_STATIC, 0, NULL);
    ss = inc(ss,  M_Opnd((unsigned)value));
#endif

    char *patch_addr_null_arg;
    ss = gen_convert_struct_class_to_object(ss, &patch_addr_null_arg);
    ss = gen_restore_monitor_exit(ss, patch_addr_null_arg);    

    addr = stub;
    assert((ss - stub) < stub_size);

    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        jvmti_add_dynamic_generated_code_chunk("vm_monitor_exit_static_naked", stub, stub_size);
        jvmti_send_dynamic_code_generated_event("vm_monitor_exit_static_naked", stub, stub_size);
    }

#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, "getaddress__vm_monitor_exit_static_naked", ss - stub);
#endif
    return addr;
} //getaddress__vm_monitor_exit_static_naked

Boolean jit_may_inline_object_synchronization(unsigned * UNREF thread_id_register,
                                              unsigned * UNREF sync_header_offset,
                                              unsigned * UNREF sync_header_width,
                                              unsigned * UNREF lock_owner_offset,
                                              unsigned * UNREF lock_owner_width,
                                              Boolean  * UNREF jit_clears_ccv)
{
    return FALSE;
}
