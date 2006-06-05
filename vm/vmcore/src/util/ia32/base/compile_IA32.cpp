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
 * @version $Revision: 1.1.2.2.4.4 $
 */  


#define LOG_DOMAIN "port.old"
#include "cxxlog.h"


#include "platform.h"

//MVM
#include <iostream>

using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "lock_manager.h"
#include "open/types.h"
#include "open/thread.h"
#include "Class.h"
#include "environment.h"
#include "method_lookup.h"
#include "stack_iterator.h"
#include "m2n.h"
#include "../m2n_ia32_internal.h"
#include "exceptions.h"
#include "jit_intf.h"
#include "jit_intf_cpp.h"
#include "jit_runtime_support.h"

#include "encoder.h"

#include "object_layout.h"
#include "nogc.h"

#include "open/gc.h"
 
#include "open/vm_util.h"
#include "vm_synch.h"
#include "vm_threads.h"
#include "ini.h"
#include "vm_stats.h"

#include "compile.h"
#include "lil.h"
#include "lil_code_generator.h"

#ifndef NDEBUG
#include "dump.h"
extern bool dump_stubs;
#endif

// Used by prepare_native_method() to compute the offsets of fields in structures
#define offset_of(type,member) ((size_t)(&((type *)0)->member))

#define REWRITE_INVALID_TYPE                        0  
#define REWRITE_PATCH_COMPILE_ME_STUB               1
#define REWRITE_PATCH_CALLER                        2
#define REWRITE_PATCH_OLD_CODE_TO_REWRITER          3

void compile_flush_generated_code_block(Byte*, size_t)
{
    // Nothing to do on IA32
}


void compile_flush_generated_code()
{
    // Nothing to do on IA32
}


static uint32* get_arg_word(unsigned num_arg_words, unsigned word)
{
    return m2n_get_args(m2n_get_last_frame())+num_arg_words-word-1;
}

void compile_protect_arguments(Method_Handle method, GcFrame* gc)
{
    assert(!tmn_is_suspend_enabled());
    Method_Signature_Handle msh = method_get_signature(method);
    unsigned num_args = method_args_get_number(msh);
    unsigned num_arg_words = ((Method*)method)->get_num_arg_bytes()>>2;

    unsigned cur_word = 0;
    for(unsigned i=0; i<num_args; i++) {
        Type_Info_Handle tih = method_args_get_type_info(msh, i);
        switch (type_info_get_type(tih)) {
        case VM_DATA_TYPE_INT64:
        case VM_DATA_TYPE_UINT64:
        case VM_DATA_TYPE_F8:
            cur_word += 2;
            break;
        case VM_DATA_TYPE_INT8:
        case VM_DATA_TYPE_UINT8:
        case VM_DATA_TYPE_INT16:
        case VM_DATA_TYPE_UINT16:
        case VM_DATA_TYPE_INT32:
        case VM_DATA_TYPE_UINT32:
        case VM_DATA_TYPE_INTPTR:
        case VM_DATA_TYPE_UINTPTR:
        case VM_DATA_TYPE_F4:
        case VM_DATA_TYPE_BOOLEAN:
        case VM_DATA_TYPE_CHAR:
        case VM_DATA_TYPE_UP:
            cur_word++;
            break;
        case VM_DATA_TYPE_CLASS:
        case VM_DATA_TYPE_ARRAY:
            gc->add_object((ManagedObject**)get_arg_word(num_arg_words, cur_word));
            cur_word++;
            break;
        case VM_DATA_TYPE_MP:
            gc->add_managed_pointer((ManagedPointer*)get_arg_word(num_arg_words, cur_word));
            break;
        case VM_DATA_TYPE_VALUE:
            {
                // This should never cause loading
                Class_Handle UNUSED c = type_info_get_class(tih);
                assert(c);
                DIE("This functionality is not currently supported");
                break;
            }
        default:
            ASSERT(0, "Unexpected data type: " << type_info_get_type(tih));
        }
    }
}


// create_call_native_proc_stub() creates a customized stub that can be used to call a native procedure from managed code. 
// Arguments used to customize the generated stub:
//  - "proc_addr"    - address of the native procedure (e.g., address of jit_a_method or delegate_invocation_work)
//  - "stub_name"    - name of the new stub.
//  - "final_action" - action to take after calling "proc_addr". For example:
//       - CNP_ReturnFinalAction - return after calling the native procedure (e.g. delegate Invoke)
//       - CNP_JmpToRetAddrFinalAction - call the procedure whose address is returned by the native procedure (e.g. JIT compile method)
//  - "return_type"  - If CNP_ReturnFinalAction, the type returned by the called "proc_addr". NB: "proc_addr" MUST return any result 
//    in eax/edx as a long (int64) value; "return_type" determines what registers to set.from this value.
//
// On entry to the generated stub:
//  - the stack contains arguments already pushed by a caller
//  - eax contains a first argument to be passed to "proc_addr": e.g., a method handle.
//  - ecx contains a stack adjustment value used to pop the arguments when "final_action" is CNP_ReturnFinalAction. 
//    This is also passed to "proc_addr" as a second argument.
//
void *create_call_native_proc_stub(char *proc_addr, char *stub_name, CNP_FinalAction final_action, CNP_ReturnType return_type)
{
    const int stub_size = 100;
    char *stub = (char *)malloc_fixed_code_for_jit(stub_size, DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_DEFAULT, CAA_Allocate);
#ifdef _DEBUG
    memset(stub, 0xcc /*int 3*/, stub_size);
#endif
    char *ss = stub;

    ss = push(ss,  ebp_opnd);
    ss = push(ss,  ebx_opnd);
    ss = push(ss,  esi_opnd);
    ss = push(ss,  edi_opnd);
    ss = mov(ss,  esi_opnd,  eax_opnd);
    ss = mov(ss,  edi_opnd,  ecx_opnd);
    ss = m2n_gen_push_m2n(ss, NULL, FRAME_UNKNOWN, false, 4);

    // Call the procedure passing the 2 arguments: the values originally in eax and ecx
    ss = push(ss,  edi_opnd);   // 2nd arg: value from ecx originally
    ss = push(ss,  esi_opnd);   // 1st arg: value from eax, typically a method handle
    ss = call(ss, proc_addr);
    ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(8));
    // Any return value is left in eax/edx


    // Exception rethrow code.

    // Save eax and edx's values in callee-saved registers
    ss = mov(ss,  esi_opnd,  eax_opnd);
    ss = mov(ss,  ebp_opnd,  edx_opnd);

    /////////////////////////////////////////////////////////////
    // begin exceptions

    ss = call(ss, (char *)get_current_thread_exception);
    ss = alu(ss, or_opc,  eax_opnd,  eax_opnd);
    ss = branch8(ss, Condition_E,  Imm_Opnd(size_8, 5));
    ss = call(ss, (char *)rethrow_current_thread_exception);

    // end exceptions
    /////////////////////////////////////////////////////////////

    // Restore eax and edx's values from callee-saved registers
    ss = mov(ss,  eax_opnd,  esi_opnd);
    ss = mov(ss,  edx_opnd,  ebp_opnd);

    ss = mov(ss,  ecx_opnd,  edi_opnd);
    ss = m2n_gen_pop_m2n(ss, false, 0, 0, 2);

    // Perform end-of-stub action after calling the procedure. For example, just return
    // or jump to the address the called procedure returned.
    switch (final_action) {
    case CNP_ReturnFinalAction: {
        // Return. First set the appropriate result registers.
        switch (return_type) {
        case CNP_VoidReturnType:
        case CNP_IntReturnType:
            break;
        case CNP_FloatReturnType:
            // move the float32 value in eax to ST(0), the top of the FP stack
            // It appears the JNI interpreter wrapper fixes up the FP stack for you,
            // meaning these instructions merely stomp on previous values.
            ss = push(ss,  eax_opnd);
            ss = fld(ss,  M_Base_Opnd(esp_reg, 0), /*is_double*/ 0);
            ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(4));
            break;
        case CNP_DoubleReturnType:
            // move the float64 value in eax/edx to ST(0), the top of the FP stack
            // It appears the JNI interpreter wrapper fixes up the FP stack for you,
            // meaning these instructions merely stomp on previous values.
            ss = push(ss,  edx_opnd);
            ss = push(ss,  eax_opnd);
            ss = fld(ss,  M_Base_Opnd(esp_reg, 0), /*is_double*/ 1);
            ss = alu(ss, add_opc,  esp_opnd,  Imm_Opnd(8));
            break;
        default:
            ABORT("Wrong return type");
        }
        // Adjust the stack pointer to pop the incoming arguments.
        // We'd like to do a "ret [%ecx]", but that's not a legal instruction.
        // The stack holds %ecx bytes of arguments followed by the return address.
        // Note that all registers are in use except ecx.
        ss = push(ss,  edi_opnd);
        // temporarily save edi on the stack
        // Copy the return address to where we want it for the ret instruction:
        // at the stack location of the first pushed arg
        
        // displacement "4" because of the push we just did
        M_Index_Opnd first_arg(esp_reg, ecx_reg, /*disp*/ 4, /*shift*/ 0);
        // edi := return address
        ss = mov(ss,  edi_opnd,  M_Base_Opnd(esp_reg, 4));
        // store return address where we want it for the ret instruction
        ss = mov(ss,  first_arg,  edi_opnd);
        // restore edi
        ss = pop(ss,  edi_opnd);
        // pop args, leaving stack pointer pointing at return address
        ss = alu(ss, add_opc,  esp_opnd,  ecx_opnd);
        ss = ret(ss);
        break;
    }
    case CNP_JmpToRetAddrFinalAction: {
        // Continue execution in, e.g., the newly compiled method.
        ss = jump(ss,  eax_opnd);
        break;
    }
    default:
        ABORT("Wrong final action");
    }

    assert((ss - stub) <= stub_size);

    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        jvmti_add_dynamic_generated_code_chunk(stub_name, stub, stub_size);
        jvmti_send_dynamic_code_generated_event(stub_name, stub, stub_size);
    }

#ifndef NDEBUG
    if (dump_stubs)
        dump(stub, stub_name, ss - stub);
#endif
    return stub;
} //create_call_native_proc_stub


void patch_code_with_threads_suspended(Byte * UNREF code_block, Byte * UNREF new_code, size_t UNREF size)
{
    ABORT("Not supported on IA32 currently");
} //patch_code_with_threads_suspended


Emitter_Handle gen_throw_if_managed_null_ia32(Emitter_Handle emitter, unsigned stack_pointer_offset)
{
    char* ss = (char*)emitter;
    ss = mov(ss,  eax_opnd,  M_Base_Opnd(esp_reg, stack_pointer_offset));
    if (VM_Global_State::loader_env->compress_references) {
        ss = alu(ss, cmp_opc,  eax_opnd,  Imm_Opnd((int32)Class::heap_base) );
    } else {
        ss = test(ss,  eax_opnd,  eax_opnd);
    }
    ss = branch8(ss, Condition_NE,  Imm_Opnd(size_8, 0));  // not null, branch around the throw
    char *backpatch_address__not_managed_null = ((char *)ss) - 1;
    ss = jump(ss, (char*)exn_get_rth_throw_null_pointer());
    signed offset = (signed)ss - (signed)backpatch_address__not_managed_null - 1;
    *backpatch_address__not_managed_null = (char)offset;
    return (Emitter_Handle)ss;
}


// Convert a reference on the stack, if null, from a managed null (represented by heap_base) to an unmanaged one (NULL/0). Uses %eax.
Emitter_Handle gen_convert_managed_to_unmanaged_null_ia32(Emitter_Handle emitter, unsigned stack_pointer_offset)
{
    char *ss = (char *)emitter;
    if (VM_Global_State::loader_env->compress_references) {
        ss = mov(ss,  eax_opnd,  M_Base_Opnd(esp_reg, stack_pointer_offset));
        ss = alu(ss, cmp_opc,  eax_opnd,  Imm_Opnd((int32)Class::heap_base) );
        ss = branch8(ss, Condition_NE,  Imm_Opnd(size_8, 0));  // not null, branch around the mov 0
        char *backpatch_address__not_managed_null = ((char *)ss) - 1;
        ss = mov(ss,  M_Base_Opnd(esp_reg, stack_pointer_offset),  Imm_Opnd(0));
        signed offset = (signed)ss - (signed)backpatch_address__not_managed_null - 1;
        *backpatch_address__not_managed_null = (char)offset;
    } 
    return (Emitter_Handle)ss;
} //gen_convert_managed_to_unmanaged_null_ia32


// Convert a reference on the stack, if null, from an unmanaged null (NULL/0) to an managed one (heap_base). Uses %eax.
Emitter_Handle gen_convert_unmanaged_to_managed_null_ia32(Emitter_Handle emitter, unsigned stack_pointer_offset)
{
    char *ss = (char *)emitter;
    if (VM_Global_State::loader_env->compress_references) {
        ss = mov(ss,  eax_opnd,  M_Base_Opnd(esp_reg, stack_pointer_offset));
        ss = test(ss,  eax_opnd,  eax_opnd);
        ss = branch8(ss, Condition_NE,  Imm_Opnd(size_8, 0));  // not null, branch around the mov 0
        char *backpatch_address__not_unmanaged_null = ((char *)ss) - 1;
        ss = mov(ss,  M_Base_Opnd(esp_reg, stack_pointer_offset),  Imm_Opnd((int32)Class::heap_base));
        signed offset = (signed)ss - (signed)backpatch_address__not_unmanaged_null - 1;
        *backpatch_address__not_unmanaged_null = (char)offset;
    } 
    return (Emitter_Handle)ss;
} //gen_convert_unmanaged_to_managed_null_ia32



char *create_unboxer(Method * UNREF method)
{
    ABORT("Not implemented");
    return 0;
} //create_unboxer

//////////////////////////////////////////////////////////////////////////
// Compile-Me Stubs

NativeCodePtr compile_jit_a_method(Method* method);

static NativeCodePtr compile_get_compile_me_generic() {
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    const int STUB_SIZE = 8 + m2n_push_m2n_size(false, 0) + m2n_pop_m2n_size(false, 0, 0, 1);
    char * stub = (char *) malloc_fixed_code_for_jit(STUB_SIZE,
        DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_DEFAULT, CAA_Allocate);
    addr = stub;
#ifndef NDEBUG
    memset(stub, 0xcc /*int 3*/, STUB_SIZE);
#endif
    // push m2n to the stack
    stub = m2n_gen_push_m2n(stub, NULL, FRAME_COMPILATION, false, 0);
    // ecx register should contain correct Mehod_Handle
    stub = push(stub, ecx_opnd);
    // compile the method
    stub = call(stub, (char *)&compile_jit_a_method);
    // remove ecx from the stack
    stub = pop(stub, ecx_opnd);
    // pop m2n from the stack
    stub = m2n_gen_pop_m2n(stub, false, 0, 0, 1);
    // transfer control to the compiled code
    stub = jump(stub, eax_opnd);
    
    assert(stub - (char *)addr <= STUB_SIZE);

    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        jvmti_add_dynamic_generated_code_chunk("compile_me_generic", addr, STUB_SIZE);
        jvmti_send_dynamic_code_generated_event("compile_me_generic", addr, STUB_SIZE);
    }

#ifndef NDEBUG
    if (dump_stubs)
        dump((char *)addr, "compileme_generic", stub - (char *)addr);
#endif
    return addr;
} //compile_get_compile_me_generic


NativeCodePtr compile_gen_compile_me(Method_Handle method) {
    int STUB_SIZE = 8;
#ifdef VM_STATS
    STUB_SIZE = 16;
    ++vm_stats_total.num_compileme_generated;
#endif
    char * stub = (char *) malloc_fixed_code_for_jit(STUB_SIZE,
        DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_DEFAULT, CAA_Allocate);
    NativeCodePtr addr = stub; 
#ifndef NDEBUG
    memset(stub, 0xcc /*int 3*/, STUB_SIZE);
#endif

#ifdef VM_STATS
    stub = inc(stub, M_Base_Opnd(n_reg, (int32)&vm_stats_total.num_compileme_used));
#endif
    stub = mov(stub, ecx_opnd, Imm_Opnd((int32)method));
    stub = jump(stub, (char *)compile_get_compile_me_generic());
    assert(stub - (char *)addr <= STUB_SIZE);


    if (VM_Global_State::loader_env->TI->isEnabled())
    {
        char * name;
        const char * c = class_get_name(method_get_class(method));
        const char * m = method_get_name(method);
        const char * d = method_get_descriptor(method);
        size_t sz = strlen(c)+strlen(m)+strlen(d)+12;
        name = (char *)STD_MALLOC(sz);
        sprintf(name, "compileme.%s.%s%s", c, m, d);
        jvmti_add_dynamic_generated_code_chunk(name, addr, STUB_SIZE);
        jvmti_send_dynamic_code_generated_event(name, addr, STUB_SIZE);
    }


#ifndef NDEBUG
    static unsigned done = 0;
    // dump first 10 compileme stubs
    if (dump_stubs && ++done <= 10) {
        char * buf;
        const char * c = class_get_name(method_get_class(method));
        const char * m = method_get_name(method);
        const char * d = method_get_descriptor(method);
        size_t sz = strlen(c)+strlen(m)+strlen(d)+12;
        buf = (char *)STD_MALLOC(sz);
        sprintf(buf, "compileme.%s.%s%s", c, m, d);
        assert(strlen(buf) < sz);
        dump((char *)addr, buf, stub - (char *)addr);
        STD_FREE(buf);
    }
#endif
    return addr;
} //compile_gen_compile_me

//NativeCodePtr compile_gen_compile_me_exc_throw(int exp)
//{
//    // ppervov: FIXME: should rewrite generation stub
//    //Class* (*p_convert_exn)(unsigned, Class*, Loader_Exception) = linking_error_to_exception_class;
//    Class* (*p_convert_exn)(unsigned, Class*, unsigned) = NULL;
//    void (*p_athrow)(ManagedObject*, Class_Handle, Method_Handle, uint8*) = exn_athrow;
//    LilCodeStub* cs = lil_parse_code_stub("entry 0:rth::void;");
//    assert(cs);
//    cs = lil_parse_onto_end(cs,
//        "push_m2n 0, 0;"
//        "m2n_save_all;"
//        "out platform:g4,pint,g4:pint;"
//        "o0=0:g4;"
//        "o1=0;"
//        "o2=%0i:g4;"
//        "call %1i;"
//        "out platform:ref,pint,pint,pint:void;"
//        "o0=0:ref;"
//        "o1=r;"
//        "o2=0;"
//        "o3=0;"
//        "call.noret %2i;",
//        exp, p_convert_exn, p_athrow);
//    assert(cs && lil_is_valid(cs));
//    NativeCodePtr addr = LilCodeGenerator::get_platform()->compile(cs, "rth_throw_linking_exception", dump_stubs);
//    lil_free_code_stub(cs);
//
//    return addr;
//} // compile_gen_compile_me_exc_throw
 

void gen_native_hashcode(Emitter_Handle h, Method *m);
unsigned native_hashcode_fastpath_size(Method *m);
void gen_native_system_currenttimemillis(Emitter_Handle h, Method *m);
unsigned native_getccurrenttime_fastpath_size(Method *m);
void gen_native_readinternal(Emitter_Handle h, Method *m);
unsigned native_readinternal_fastpath_size(Method *m);
void gen_native_newinstance(Emitter_Handle h, Method *m);
unsigned native_newinstance_fastpath_size(Method *m);
// ****** 20031009 above are additions to bring original on par with LIL
void gen_native_getclass_fastpath(Emitter_Handle h, Method *m);
unsigned native_getclass_fastpath_size(Method *m);

void gen_native_arraycopy_fastpath(Emitter_Handle h, Method *m);
unsigned native_arraycopy_fastpath_size(Method *m);

static Stub_Override_Entry _stub_override_entries_base[] = {
    {"java/lang/VMSystem", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", gen_native_arraycopy_fastpath, native_arraycopy_fastpath_size},
    {"java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", gen_native_arraycopy_fastpath, native_arraycopy_fastpath_size},
    {"java/lang/Object", "getClass", "()Ljava/lang/Class;", gen_native_getclass_fastpath, native_getclass_fastpath_size},
    // ****** 20031009 below are additions to bring baseline on par with LIL
    {"java/lang/System", "currentTimeMillis", "()J", gen_native_system_currenttimemillis, native_getccurrenttime_fastpath_size},
    {"java/io/FileInputStream", "readInternal", "(I[BII)I", gen_native_readinternal, native_readinternal_fastpath_size},
#ifndef PLATFORM_POSIX
    // because of threading, this override will not work on Linux!
    {"java/lang/Class", "newInstance", "()Ljava/lang/Object;", gen_native_newinstance, native_newinstance_fastpath_size},
#endif
    {"java/lang/VMSystem", "identityHashCode", "(Ljava/lang/Object;)I", gen_native_hashcode, native_hashcode_fastpath_size}
};

Stub_Override_Entry *stub_override_entries = &(_stub_override_entries_base[0]);

int sizeof_stub_override_entries = sizeof(_stub_override_entries_base) / sizeof(_stub_override_entries_base[0]);


