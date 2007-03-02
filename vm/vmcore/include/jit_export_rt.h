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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


//
// These are the functions that a JIT built as a DLL must export for
// the purpose of runtime interaction.
//

#ifndef _JIT_EXPORT_RT_H
#define _JIT_EXPORT_RT_H


#include "open/types.h"
#include "jit_export.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus



///////////////////////////////////////////////////////
// begin Frame Contexts for JITs

#ifdef _IPF_

// Note that the code in transfer context is very depend upon the ordering of fields in this structure.
// Be very careful in changing this structure.
typedef
struct JitFrameContext {
    uint64 *p_ar_pfs;
    uint64 *p_eip;
    uint64 sp;
    uint64 *p_gr[128];
    uint64 *p_fp[128];
    uint64 preds;
    uint64 *p_br[8];
    uint64 nats_lo;
    uint64 nats_hi;
    Boolean is_ip_past;
    uint64 ar_fpsr;
    uint64 ar_unat;
    uint64 ar_lc;
} JitFrameContext; //JitFrameContext

#elif defined _EM64T_

typedef
struct JitFrameContext {
    uint64   rsp;
    uint64 * p_rbp;
    uint64 * p_rip;

    // Callee-saved registers
    uint64 * p_rbx;
    uint64 * p_r12;
    uint64 * p_r13;
    uint64 * p_r14;
    uint64 * p_r15;
    
    // The scratch registers are currently only valid during GC enumeration.
    uint64 * p_rax;
    uint64 * p_rcx;
    uint64 * p_rdx;
    uint64 * p_rsi;
    uint64 * p_rdi;
    uint64 * p_r8;
    uint64 * p_r9;
    uint64 * p_r10;
    uint64 * p_r11;

    // To restore processor flags during transfer
    uint32 eflags;

    Boolean is_ip_past;
} JitFrameContext;

#else // "_IA32_"

typedef
struct JitFrameContext {
    uint32 esp;
    uint32 *p_ebp;
    uint32 *p_eip;

    // Callee-saved registers
    uint32 *p_edi;
    uint32 *p_esi;
    uint32 *p_ebx;

    // The scratch registers are currently only valid during GC enumeration.
    uint32 *p_eax;
    uint32 *p_ecx;
    uint32 *p_edx;

    // To restore processor flags during transfer
    uint32 eflags;

    Boolean is_ip_past;
} JitFrameContext;

#endif // "_IA32_"

typedef void * InlineInfoPtr;

// end Frame Contexts for JITs
///////////////////////////////////////////////////////



///////////////////////////////////////////////////////
// begin direct call support


// The following are optional functions used by the direct call-related JIT interface. 
// These functions are implemented by a JIT and invoked by the VM. They allow a JIT 
// to be notified whenever, e.g., a VM data structure changes that would require 
// code patching or recompilation.
 
// The callback that corresponds to vm_register_jit_extended_class_callback.  
// The newly loaded class is new_class.  The JIT should return TRUE if any code was modified
// (consequently the VM will ensure correctness such as synchronizing I- and D-caches), 
// and FALSE otherwise.
JITEXPORT Boolean 
JIT_extended_class_callback(JIT_Handle jit, 
                            Class_Handle  extended_class,
                            Class_Handle  new_class,
                            void         *callback_data);

// The callback that corresponds to vm_register_jit_overridden_method_callback. 
// The overriding method is new_method. The JIT should return TRUE if any code was modified
// (consequently the VM will ensure correctness such as synchronizing I- and D-caches),
// and FALSE otherwise.
JITEXPORT Boolean 
JIT_overridden_method_callback(JIT_Handle jit,
                               Method_Handle  overridden_method,
                               Method_Handle  new_method, 
                               void          *callback_data);

// The callback that corresponds to vm_register_jit_recompiled_method_callback.  
// The JIT should return TRUE if any code was modified (consequently the VM will ensure 
// correctness such as synchronizing I- and D-caches), and FALSE otherwise.
JITEXPORT Boolean 
JIT_recompiled_method_callback(JIT_Handle jit,
                               Method_Handle  recompiled_method,
                               void          *callback_data); 


// end direct call support
///////////////////////////////////////////////////////



///////////////////////////////////////////////////////
// begin stack unwinding

JITEXPORT void
JIT_unwind_stack_frame(JIT_Handle         jit, 
                       Method_Handle      method,
                       JitFrameContext* context
                       );

JITEXPORT void 
JIT_get_root_set_from_stack_frame(JIT_Handle             jit,
                                  Method_Handle          method,
                                  GC_Enumeration_Handle  enum_handle,
                                  JitFrameContext* context
                                  );

JITEXPORT void 
JIT_get_root_set_for_thread_dump(JIT_Handle             jit,
                                  Method_Handle          method,
                                  GC_Enumeration_Handle  enum_handle,
                                  JitFrameContext* context
                                  );

JITEXPORT uint32 
JIT_get_inline_depth(JIT_Handle jit, 
                     InlineInfoPtr   ptr, 
                     uint32          offset);

JITEXPORT Boolean
JIT_can_enumerate(JIT_Handle        jit, 
                  Method_Handle     method,
                  NativeCodePtr     eip
                  );

JITEXPORT void
JIT_fix_handler_context(JIT_Handle         jit,
                        Method_Handle      method,
                        JitFrameContext* context
                        );

JITEXPORT void *
JIT_get_address_of_this(JIT_Handle               jit,
                        Method_Handle            method,
                        const JitFrameContext* context
                        );

JITEXPORT Boolean
JIT_call_returns_a_reference(JIT_Handle               jit,
                             Method_Handle            method,
                             const JitFrameContext* context
                             );

// end stack unwinding
///////////////////////////////////////////////////////



///////////////////////////////////////////////////////
// begin compressed references


// Returns TRUE if the JIT will compress references within objects and vector elements by representing 
// them as offsets rather than raw pointers. The JIT should call the VM function vm_references_are_compressed()
// during initialization in order to decide whether it should compress references.
JITEXPORT Boolean 
JIT_supports_compressed_references(JIT_Handle jit);

// end compressed references
///////////////////////////////////////////////////////

JITEXPORT Boolean
JIT_code_block_relocated(JIT_Handle jit, Method_Handle method, int id, NativeCodePtr old_address, NativeCodePtr new_address);


#ifdef __cplusplus
}
#endif // __cplusplus


#endif // _JIT_EXPORT_RT_H
