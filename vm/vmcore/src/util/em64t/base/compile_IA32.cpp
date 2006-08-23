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
 * @version $Revision: 1.1.2.1.4.5 $
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
#include "Class.h"
#include "environment.h"
#include "method_lookup.h"
#include "stack_iterator.h"
#include "m2n.h"
#include "../m2n_em64t_internal.h"
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

// Used by prepare_native_method() to compute the offsets of fields in structures
#define offset_of(type,member) ((size_t)(&((type *)0)->member))

extern bool dump_stubs;

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


// FIXME: em64t not tested
static uint64* get_arg_word(unsigned num_arg_words, unsigned word)
{
    //return m2n_get_args(m2n_get_last_frame())+num_arg_words-word-1;
    return NULL;
}

void compile_protect_arguments(Method_Handle method, GcFrame* gc)
{
    assert(!hythread_is_suspend_enabled());
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
            ABORT("Unexpected data type");
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
  return NULL;
} //create_call_native_proc_stub


void patch_code_with_threads_suspended(Byte *code_block, Byte *new_code, size_t size)
{
    ABORT("Not supported");      // 20030203 Not supported on IA32 currently
} //patch_code_with_threads_suspended


Emitter_Handle gen_throw_if_managed_null_ia32(Emitter_Handle emitter, unsigned stack_pointer_offset)
{
    char* ss = (char*)emitter;
    return (Emitter_Handle)ss;
}


// Convert a reference on the stack, if null, from a managed null (represented by heap_base) to an unmanaged one (NULL/0). Uses %eax.
Emitter_Handle gen_convert_managed_to_unmanaged_null_ia32(Emitter_Handle emitter, unsigned stack_pointer_offset)
{
    char *ss = (char *)emitter;
    return (Emitter_Handle)ss;
} //gen_convert_managed_to_unmanaged_null_ia32


// Convert a reference on the stack, if null, from an unmanaged null (NULL/0) to an managed one (heap_base). Uses %eax.
Emitter_Handle gen_convert_unmanaged_to_managed_null_ia32(Emitter_Handle emitter, unsigned stack_pointer_offset)
{
    char *ss = (char *)emitter;
    return (Emitter_Handle)ss;
} //gen_convert_unmanaged_to_managed_null_ia32

char *create_unboxer(Method *method)
{
    ABORT("Not implemented");
    return 0;
} //create_unboxer

//////////////////////////////////////////////////////////////////////////
// Compile-Me Stubs

NativeCodePtr compile_gen_compile_me(Method_Handle method)
{
    return NULL;
} //compile_gen_compile_me


void gen_native_hashcode(Emitter_Handle h, Method *m);
unsigned native_hashcode_fastpath_size(Method *m);
void gen_native_system_currenttimemillis(Emitter_Handle h, Method *m);
unsigned native_getccurrenttime_fastpath_size(Method *m);
void gen_native_readinternal(Emitter_Handle h, Method *m);
unsigned native_readinternal_fastpath_size(Method *m);
void gen_native_newinstance(Emitter_Handle h, Method *m);
unsigned native_newinstance_fastpath_size(Method *m);
// ****** 10/09/2003 above are additions to bring original on par with LIL
void gen_native_getclass_fastpath(Emitter_Handle h, Method *m);
unsigned native_getclass_fastpath_size(Method *m);

void gen_native_arraycopy_fastpath(Emitter_Handle h, Method *m);
unsigned native_arraycopy_fastpath_size(Method *m);

static Stub_Override_Entry _stub_override_entries_base[] = {
    {"java/lang/VMSystem", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", gen_native_arraycopy_fastpath, native_arraycopy_fastpath_size},
    {"java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", gen_native_arraycopy_fastpath, native_arraycopy_fastpath_size},
    {"java/lang/Object", "getClass", "()Ljava/lang/Class;", gen_native_getclass_fastpath, native_getclass_fastpath_size},
    // ****** 10/09/2003: below are additions to bring baseline on par with LIL
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


