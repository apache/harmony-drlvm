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

/*
 * JVMTI JIT pop frame support functions.
 */

#define LOG_DOMAIN "jvmti.stack.popframe"

#include "jvmti_direct.h"
#include "jvmti_interface.h"
#include "exceptions.h"
#include "environment.h"
#include "open/jthread.h"
#include "vm_threads.h"
#include "jit_intf_cpp.h"
#include "m2n.h"
#include "mon_enter_exit.h"
#include "stack_iterator.h"
#include "jvmti_break_intf.h"
#include "cci.h"
#include "clog.h"

static void jvmti_pop_frame_callback()
{
    TRACE(("JVMTI PopFrame callback is called"));
    frame_type type = m2n_get_frame_type(p_TLS_vmthread->last_m2n_frame);

    // frame wasn't requested to be popped
    if (FRAME_POP_NOW != (FRAME_POP_NOW & type)) {
        TRACE(("PopFrame callback is not FRAME_POP_NOW"));
        return;
    }

    // if we are in hythread_safe_point() frame is unwindable
    if (FRAME_SAFE_POINT == (FRAME_SAFE_POINT & type)) {
        TRACE(("PopFrame callback is FRAME_SAFE_POINT"));
        jvmti_jit_prepare_pop_frame();

    } else if (is_unwindable()) {
        // unwindable frame, wait for resume
        TRACE(("PopFrame callback is FRAME_SAFE_POINT"));

        // switch execution to the previous frame
        jvmti_jit_do_pop_frame();
        assert(0 /* mustn't get here */);

    } else {
        // nonunwindable frame, raise special exception object
        TRACE(("PopFrame callback is raising exception"));
        exn_raise_object(VM_Global_State::loader_env->popFrameException);
    }
} //jvmti_pop_frame_callback

jvmtiError jvmti_jit_pop_frame(jthread java_thread)
{
    assert(hythread_is_suspend_enabled());
    TRACE(("Called PopFrame for JIT"));

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_POP_FRAME)) {
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
    }

    hythread_t hy_thread = jthread_get_native_thread(java_thread);
    VM_thread* vm_thread = get_vm_thread(hy_thread);

    M2nFrame* top_frame = m2n_get_last_frame(vm_thread);
    frame_type type = m2n_get_frame_type(top_frame);

    if (FRAME_POPABLE != (FRAME_POPABLE & type))
        return JVMTI_ERROR_OPAQUE_FRAME;

    StackIterator *si = si_create_from_native(vm_thread);

    // check that topmost frame is M2n
    assert(si_is_native(si));

    // go to 2-d frame & check it's managed
    si_goto_previous(si);
    assert(! si_is_native(si));
    TRACE(("PopFrame is called for method %s.%s%s :%p",
        class_get_name(method_get_class(si_get_code_chunk_info(si)->get_method())),
        method_get_name(si_get_code_chunk_info(si)->get_method()),
        method_get_descriptor(si_get_code_chunk_info(si)->get_method()),
        si_get_ip(si) ));

    // go to 3-d frame & check its type
    si_goto_previous(si);

    if (si_is_native(si)) {
        si_free(si);
        return JVMTI_ERROR_OPAQUE_FRAME;
    }

    si_free(si);

    // change type from popable to pop_now, pop_done should'n be changed
    if (FRAME_POPABLE == (type & FRAME_POP_MASK)) {
        type = (frame_type)((type & ~FRAME_POP_MASK) | FRAME_POP_NOW);
    }
    m2n_set_frame_type(top_frame, type);

    // Install safepoint callback that would perform popping job
    hythread_set_safepoint_callback(hy_thread, &jvmti_pop_frame_callback);

    return JVMTI_ERROR_NONE;
} //jvmti_jit_pop_frame


#ifdef _IPF_

void jvmti_jit_prepare_pop_frame(){
    assert(0);
}

void jvmti_jit_complete_pop_frame(){
    assert(0);
}

void jvmti_jit_do_pop_frame(){
    assert(0);
}

#elif defined _EM64T_

void jvmti_jit_prepare_pop_frame(){
    assert(0);
}

void jvmti_jit_complete_pop_frame(){
    assert(0);
}

void jvmti_jit_do_pop_frame(){
    assert(0);
}

#else // _IA32_

// requires stack iterator and buffer to save intermediate information
static void jvmti_jit_prepare_pop_frame(StackIterator* si, uint32* buf) {
    TRACE(("Prepare PopFrame for JIT"));
    // pop native frame
    assert(si_is_native(si));
    si_goto_previous(si);

    // save information about java frame
    assert(!si_is_native(si));
    CodeChunkInfo *cci = si_get_code_chunk_info(si);
    JitFrameContext* jitContext = si_get_jit_context(si);

    // save information about java method
    assert(cci);
    Method *method = cci->get_method();
    Class* method_class = method->get_class();
    bool is_method_static = method->is_static();
    TRACE(("PopFrame method %s.%s%s, stop IP: %p",
        class_get_name(method_get_class(cci->get_method())),
        method_get_name(cci->get_method()),
        method_get_descriptor(cci->get_method()), si_get_ip(si) ));

    // free lock of synchronized method
    /*
    Currently JIT does not unlock monitors of synchronized blocks relying
    on compiler which generates pseudo finally statement to unlock them.
    For correct implementation of PopFrame these monitors will have to be
    unlocked by VM, so JIT has to store information about these monitors
    somewhere.
    */
    vm_monitor_exit_synchronized_method(si);

    // pop java frame
    si_goto_previous(si);

    // find correct ip and restore required registers context
    NativeCodePtr current_method_addr = NULL;
    NativeCodePtr ip = si_get_ip(si);
    TRACE(("PopFrame method %s.%s%s, set IP begin: %p",
        class_get_name(method_get_class(si_get_code_chunk_info(si)->get_method())),
        method_get_name(si_get_code_chunk_info(si)->get_method()),
        method_get_descriptor(si_get_code_chunk_info(si)->get_method()), ip ));
    size_t ip_reduce;

    // invoke static
    if (is_method_static) {
        ip_reduce = 6;

    // invoke interface
    } else if (0xd0ff == (*((unsigned short*)(((char*)ip)-2)))) {
        ip_reduce = 2;
        current_method_addr = cci->get_code_block_addr();
        *buf = (uint32)current_method_addr;
        jitContext->p_eax = buf;

    // invoke virtual and special
    } else {
        VTable_Handle vtable = class_get_vtable( method_class);
        *buf = (uint32) vtable;
        unsigned short code = (*((unsigned short*)(((char*)ip)-3)));

        // invoke virtual
        if (0x50ff == code) {
            jitContext->p_eax = buf;
            ip_reduce = 3;
        } else if (0x51ff == code) {
            jitContext->p_ecx = buf;
            ip_reduce = 3;
        } else if (0x52ff == code) {
            jitContext->p_edx = buf;
            ip_reduce = 3;
        } else if (0x53ff == code) {
            jitContext->p_ebx = buf;
            ip_reduce = 3;

        // invoke special
        } else{
            ip_reduce = 6;
        }
    }

    // set correct ip
    ip = (NativeCodePtr)(((char*)ip) - ip_reduce);
    TRACE(("PopFrame method %s.%s%s, set IP end: %p",
        class_get_name(method_get_class(si_get_code_chunk_info(si)->get_method())),
        method_get_name(si_get_code_chunk_info(si)->get_method()),
        method_get_descriptor(si_get_code_chunk_info(si)->get_method()), ip ));
    si_set_ip(si, ip, false);
}

void jvmti_jit_prepare_pop_frame() {
    // Find top m2n frame
    M2nFrame* top_frame = m2n_get_last_frame();
    frame_type type = m2n_get_frame_type(top_frame);

    // Check that frame has correct type
    assert((FRAME_POP_NOW == (FRAME_POP_MASK & type))
            ||(FRAME_POP_DONE == (FRAME_POP_MASK & type)));

    // create stack iterator from native
    StackIterator* si = si_create_from_native();
    si_transfer_all_preserved_registers(si);
    TRACE(("PopFrame prepare for method IP: %p", si_get_ip(si) ));

    // prepare pop frame - find regs values
    uint32 buf = 0;
    jvmti_jit_prepare_pop_frame(si, &buf);

    // save regs value from jit context to m2n
    JitFrameContext* jitContext = si_get_jit_context(si);
    Registers* regs = get_pop_frame_registers(top_frame);

    regs->esp = jitContext->esp;
    regs->eip = *(jitContext->p_eip);
    regs->esi = *(jitContext->p_esi);
    regs->edi = *(jitContext->p_edi);
    regs->ebp = *(jitContext->p_ebp);

    if (0 == jitContext->p_eax) {
        regs->eax = 0;
    } else {
        regs->eax = *(jitContext->p_eax);
    }

    if (0 == jitContext->p_ebx) {
        regs->ebx = 0;
    } else {
        regs->ebx = *(jitContext->p_ebx);
    }

    if (0 == jitContext->p_ecx) {
        regs->ecx = 0;
    } else {
        regs->ecx = *(jitContext->p_ecx);
    }

    if (0 == jitContext->p_edx) {
        regs->edx = 0;
    } else {
        regs->edx = *(jitContext->p_edx);
    }

    // set pop done frame state
    m2n_set_frame_type(top_frame, frame_type(FRAME_POP_DONE | FRAME_MODIFIED_STACK));
    return;
}

static void
jvmti_relocate_single_step_breakpoints( StackIterator *si)
{
    // relocate single step
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (ti->isEnabled() && ti->is_single_step_enabled())
    {
        VM_thread *vm_thread = p_TLS_vmthread;
        LMAutoUnlock lock(ti->vm_brpt->get_lock());
        if (NULL != vm_thread->ss_state) {
            // remove old single step breakpoints
            jvmti_remove_single_step_breakpoints(ti, vm_thread);

            // set new single step breakpoints
            CodeChunkInfo *cci = si_get_code_chunk_info(si);
            Method *method = cci->get_method();
            NativeCodePtr ip = si_get_ip(si);
            uint16 bc;
            JIT *jit = cci->get_jit();
            OpenExeJpdaError UNREF result =
                jit->get_bc_location_for_native(method, ip, &bc);
            assert(EXE_ERROR_NONE == result);

            jvmti_StepLocation locations = {method, ip, bc, false};
            jvmti_set_single_step_breakpoints(ti, vm_thread, &locations, 1);
        }
    }
    return;
} // jvmti_relocate_single_step_breakpoints

void jvmti_jit_complete_pop_frame() {
    // Destructive Unwinding!!! NO CXX Logging put here.
    TRACE(("Complete PopFrame for JIT"));

    // Find top m2n frame
    M2nFrame* top_frame = m2n_get_last_frame();
    frame_type type = m2n_get_frame_type(top_frame);

    // Check that frame has correct type
    assert(FRAME_POP_DONE == (FRAME_POP_MASK & type));

    // create stack iterator from native
    StackIterator* si = si_create_from_native();
    si_transfer_all_preserved_registers(si);

    // pop native frame
    assert(si_is_native(si));
    si_goto_previous(si);

    // relocate single step breakpoints
    jvmti_relocate_single_step_breakpoints(si);

    // transfer control
    TRACE(("PopFrame transfer control to: %p",  (void*)si_get_ip(si) ));
    si_transfer_control(si);
}

void jvmti_jit_do_pop_frame() {
    // Destructive Unwinding!!! NO CXX Logging put here.
    TRACE(("Do PopFrame for JIT"));

    // Find top m2n frame
    M2nFrame* top_frame = m2n_get_last_frame();
    frame_type type = m2n_get_frame_type(top_frame);

    // Check that frame has correct type
    assert(FRAME_POP_NOW == (FRAME_POP_MASK & type));

    // create stack iterator from native
    StackIterator* si = si_create_from_native();
    si_transfer_all_preserved_registers(si);

    // prepare pop frame - find regs values
    uint32 buf = 0;
    jvmti_jit_prepare_pop_frame(si, &buf);

    // relocate single step breakpoints
    jvmti_relocate_single_step_breakpoints(si);

    // transfer control
    TRACE(("PopFrame transfer control to: %p",  (void*)si_get_ip(si) ));
    si_transfer_control(si);
}
#endif // _IA32_

void jvmti_safe_point()
{
    Registers regs;
    M2nFrame* top_frame = m2n_get_last_frame();
    set_pop_frame_registers(top_frame, &regs);

    TRACE(("entering exception_safe_point"));
    hythread_exception_safe_point();
    TRACE(("left exception_safe_point"));

    TRACE(("entering safe_point"));
    hythread_safe_point();
    TRACE(("left safe_point"));

    // find frame type
    frame_type type = m2n_get_frame_type(top_frame);

    // complete pop frame if frame has correct type
    if (FRAME_POP_DONE == (FRAME_POP_MASK & type)){
        jvmti_jit_complete_pop_frame();
    }
}
