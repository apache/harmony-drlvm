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
 * @version $Revision: 1.6.12.3.4.4 $
 */
#include "compiler.h"
#include "trace.h"
#include "cg_ia32.h"
#include "jet.h"

#include "jit_import.h"
#include "jit_intf.h"

#ifdef _DEBUG
#include "jit_runtime_support.h"
#include "open/vm.h"
#endif

/**
 * @file
 * @brief Runtime support utilities which are platform-dependent and are 
 *        specific for IA32/EM64T.
 */
 
namespace Jitrino {
namespace Jet {

#define STACK_SLOT_SIZE (sizeof(long))

/**
 * @brief Prints out a message to identify a program location.
 *
 * The message includes method name and class, IP and PC of the location.
 * Message is preceded with the specified \c name.
 * The function uses #dbg and does not print out new-line character.
 */
static void rt_trace(const char * name, Method_Handle meth, 
                     const MethodInfoBlock& infoBlock,
                     const JitFrameContext * context) {
    dbg("%s @ %s::%s @ %p/%u: ", 
        name,
        class_get_name(method_get_class(meth)), method_get_name(meth),
        *context->p_eip, infoBlock.get_pc((const char*)*context->p_eip));
}

void rt_unwind(JIT_Handle jit, Method_Handle method, 
               JitFrameContext * context)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    
    assert(MethodInfoBlock::is_valid_data(pinfo));

    MethodInfoBlock infoBlock(pinfo);
    StackFrame stackframe(infoBlock.get_num_locals(),
                          infoBlock.get_stack_max(), 
                          infoBlock.get_in_slots());

#if defined(_DEBUG) || defined(JIT_LOGS) || defined(JET_PROTO)
    JitFrameContext saveContext = *context;
#endif
    
    unsigned ebp = *context->p_ebp;
    unsigned esp;
    if (infoBlock.get_flags() & JMF_ALIGN_STACK) {
    /*
        mov smth, [ebp] ; get unaligned ESP
        addr_of_this = smth + offsef_of_input_arg#0
    */
        esp = *(unsigned *)ebp;
    }
    else {
        esp = ebp;
    }
   
    //mov esp, ebp
    context->esp = esp;
    //pop ebp
    context->p_ebp = ((unsigned*)context->esp) + stackframe.save_ebp();
    // pop edi
    context->p_edi = ((unsigned*)context->esp) + stackframe.save_edi();
    // pop esi
    context->p_esi = ((unsigned*)context->esp) + stackframe.save_esi();
    // pop ebx
    context->p_ebx = ((unsigned*)context->esp) + stackframe.save_ebx();
    //pop out-the-IP
    context->p_eip = ((unsigned*)context->esp) + stackframe.in_ret();
    //
    context->esp += 5 * SLOT_SIZE; // '5' here is 'ebp+edi+esi+ebx+retIP'
#if defined(_DEBUG) || defined(JIT_LOGS) || defined(JET_PROTO)
    if (infoBlock.get_flags() & DBG_TRACE_RT) {
        rt_trace("rt.unwind", method, infoBlock, &saveContext);
        dbg("=> %p\n", *context->p_eip);
    }
#endif
}

void rt_enum(JIT_Handle jit, Method_Handle method, 
             GC_Enumeration_Handle henum, JitFrameContext * context)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    
    if (!MethodInfoBlock::is_valid_data(pinfo)) {
        assert(false);
        return;
    }

    MethodInfoBlock infoBlock(pinfo);

    if (DBG_TRACE_RT & infoBlock.get_flags()) {
        rt_trace("rt.enum.start", method, infoBlock, context);
    }
    if (infoBlock.get_flags() & JMF_EMPTY_RS) {
        if (DBG_TRACE_RT & infoBlock.get_flags()) {
            dbg("empty root set\n");
        }
        return;
    }
    
    if (DBG_TRACE_RT & infoBlock.get_flags()) {
        dbg("\n");
    }
    
    StackFrame frame(infoBlock.get_num_locals(), infoBlock.get_stack_max(), 
                  infoBlock.get_in_slots() );

    long * ebp = (long*)(*context->p_ebp);
    long * map = ebp + frame.info_gc_locals();

    for (unsigned i=0; i<infoBlock.get_num_locals(); i++) {
        if (!tst( (long*)map, i)) {
            continue;
        }
        void **p_obj = (void**)(ebp + frame.local(i));
        if (DBG_TRACE_RT & infoBlock.get_flags()) {
            rt_trace("gc.item", method, infoBlock, context);
            dbg(": local#%d=>%X (%X)\n", i, p_obj, *p_obj);
        }
        vm_enumerate_root_reference(p_obj, FALSE);
    }

    map = ebp + frame.info_gc_stack();
    unsigned rt_stack_depth = *(ebp+frame.info_gc_stack_depth());
    
    for (unsigned i=0; i<rt_stack_depth; i++) {
        if (!tst( map, i)) {
            continue;
        }
        void ** p_obj;
        p_obj = (void**)(ebp + frame.stack_slot(i));
        if (DBG_TRACE_RT & infoBlock.get_flags()) {
            rt_trace("gc.item", method, infoBlock, context);
            dbg(": stack#%d=>%X (%X)\n", i, p_obj, *p_obj);
        }
        vm_enumerate_root_reference(p_obj, FALSE);
    }

    if (infoBlock.get_flags() & JMF_REPORT_THIS) {
        void ** p_obj = (void**)rt_get_address_of_this(jit, method, context);
        if (DBG_TRACE_RT & infoBlock.get_flags()) {
            rt_trace("gc.item", method, infoBlock, context);
            dbg(": in.this#=>%X (%X)\n", p_obj, *p_obj);
        }
        vm_enumerate_root_reference(p_obj, FALSE);
    }
    if (DBG_TRACE_RT & infoBlock.get_flags()) {
        rt_trace("rt.enum.done", method, infoBlock, context);
        dbg("\n");
    }
}

void rt_fix_handler_context(JIT_Handle jit, Method_Handle method, 
                            JitFrameContext * context)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);

    if (!MethodInfoBlock::is_valid_data(pinfo)) {
        assert(false);
        return;
    }
    //
    MethodInfoBlock infoBlock(pinfo);
    StackFrame stackframe(infoBlock.get_num_locals(),
                          infoBlock.get_stack_max(),
                          infoBlock.get_in_slots());

    unsigned new_esp = *context->p_ebp + 
                       (stackframe.native_stack_bot())*SLOT_SIZE;
    if (infoBlock.get_flags() & DBG_TRACE_RT) {
        rt_trace("rt.fix_handler", method, infoBlock, context);
        dbg("oldESP=%p ; newESP=%p\n", context->esp, new_esp);
    }
    context->esp = new_esp;
}

void * rt_get_address_of_this(JIT_Handle jit, Method_Handle method,
                              const JitFrameContext * context)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    assert(MethodInfoBlock::is_valid_data(pinfo));
    
    MethodInfoBlock infoBlock(pinfo);
    if (!(infoBlock.get_flags() & JMF_REPORT_THIS)) {
        return NULL;
    }
    
    StackFrame stackframe(infoBlock.get_num_locals(),
                          infoBlock.get_stack_max(),
                          infoBlock.get_in_slots());
    
    
    unsigned ebp = *context->p_ebp;
    unsigned esp;
    if (infoBlock.get_flags() & JMF_ALIGN_STACK) {
    /*
        mov smth, [ebp] ; get unaligned ESP
        addr_of_this = smth + offsef_of_input_arg#0
    */
        esp = *(unsigned *)ebp;
    }
    else {
        esp = ebp;
    }
    void ** p_obj = (void**)((char**)esp + stackframe.in_slot(0));
    
    if (infoBlock.get_flags() & DBG_TRACE_RT) {
        rt_trace("rt.get_thiz", method, infoBlock, context);
        dbg("p_thiz=%p, thiz=%p\n", p_obj, p_obj ? *p_obj : NULL);
    }
    return p_obj;
}

::OpenExeJpdaError rt_get_local_var(JIT_Handle jit, Method_Handle method,
                                    const ::JitFrameContext *context,
                                    unsigned var_num, VM_Data_Type var_type,
                                    void *value_ptr)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    
    if (!MethodInfoBlock::is_valid_data(pinfo)) {
        assert(false);
        return EXE_ERROR_UNSUPPORTED;
    }


    MethodInfoBlock infoBlock(pinfo);
    if (var_num >= infoBlock.get_num_locals()) {
        return EXE_ERROR_INVALID_SLOT;
    }
    StackFrame stackframe(infoBlock.get_num_locals(),
                          infoBlock.get_stack_max(),
                          infoBlock.get_in_slots());

    uint32* ebp = (uint32*)(*context->p_ebp);
    uint64* var_ptr_to_64;
    uint32* var_ptr_to32;
    long* map = (long*) (ebp + stackframe.info_gc_locals());

    switch(var_type) {
    case VM_DATA_TYPE_INT64:
    case VM_DATA_TYPE_UINT64:
    case VM_DATA_TYPE_F8:
        var_ptr_to_64 = (uint64*)value_ptr;
        *var_ptr_to_64 = *(uint64*)(ebp + stackframe.local((unsigned)var_num));
        break;
    case VM_DATA_TYPE_ARRAY:
    case VM_DATA_TYPE_CLASS:
        if (!tst((long*)map, var_num)) {
            return EXE_ERROR_TYPE_MISMATCH;
        }
    default:
        var_ptr_to32 = (uint32*)value_ptr;
        *var_ptr_to32 = *(uint32*)(ebp + stackframe.local((unsigned)var_num));
    }

    return EXE_ERROR_NONE;
}

::OpenExeJpdaError rt_set_local_var(JIT_Handle jit, Method_Handle method,
                                    const ::JitFrameContext *context,
                                    unsigned var_num, VM_Data_Type var_type,
                                    void *value_ptr)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    
    if (!MethodInfoBlock::is_valid_data(pinfo)) {
        assert(false);
        return EXE_ERROR_UNSUPPORTED;
    }

    MethodInfoBlock infoBlock(pinfo);
    if (var_num >= infoBlock.get_num_locals()) {
        return EXE_ERROR_INVALID_SLOT;
    }
    StackFrame stackframe(infoBlock.get_num_locals(),
                          infoBlock.get_stack_max(),
                          infoBlock.get_in_slots());

    uint32* ebp = (uint32*)(*context->p_ebp);
    uint64* var_ptr_to_64;
    uint32* var_ptr_to32;
    
    switch(var_type) {
    case VM_DATA_TYPE_INT64:
    case VM_DATA_TYPE_UINT64:
    case VM_DATA_TYPE_F8:
        var_ptr_to_64 = (uint64*)(ebp + stackframe.local((unsigned)var_num));
        *var_ptr_to_64 = *(uint64*)value_ptr;
        break;
    default:
        var_ptr_to32 = (uint32*)(ebp + stackframe.local((unsigned)var_num));
        *var_ptr_to32 = *(uint32*)value_ptr;
    }
    return EXE_ERROR_NONE;
}

};};    // ~namespace Jitrino::Jet

