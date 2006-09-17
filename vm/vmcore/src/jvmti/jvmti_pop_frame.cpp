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

/*
 * JVMTI JIT pop frame support functions.
 */

#define LOG_DOMAIN "jvmti.stack.popframe"

#include "jvmti_direct.h"
#include "jvmti_interface.h"
#include "exceptions.h"
#include "environment.h"
#include "vm_threads.h"
#include "jit_intf_cpp.h"
#include "m2n.h"
#include "mon_enter_exit.h"
#include "stack_iterator.h"
//#include "cxxlog.h"
#include "clog.h"

jvmtiError jvmti_jit_pop_frame(VM_thread *thread)
{
    assert(hythread_is_suspend_enabled());

    M2nFrame* top_frame = thread->last_m2n_frame;
    frame_type type = m2n_get_frame_type(top_frame);

    if (FRAME_POPABLE != (FRAME_POPABLE & type))
        return JVMTI_ERROR_OPAQUE_FRAME;

    StackIterator *si = si_create_from_native(thread);

    // check that topmost frame is M2n
    assert(si_is_native(si));

    // go to 2-d frame & check it's managed
    si_goto_previous(si);
    assert(! si_is_native(si));

    // go to 3-d frame & check its type
    si_goto_previous(si);

    if (si_is_native(si)) {
        M2nFrame* third_frame = m2n_get_previous_frame(top_frame);
        
        if (FRAME_POPABLE != (FRAME_POPABLE & m2n_get_frame_type(top_frame)))
            return JVMTI_ERROR_OPAQUE_FRAME;
    }

    type = (frame_type) (type | FRAME_POP_NOW);
    m2n_set_frame_type(top_frame, type);

    si_free(si);

    return JVMTI_ERROR_NONE;
}

void jvmti_jit_do_pop_frame()
{
    // Destructive Unwinding!!! NO CXZ Logging put here.

    // create stack iterator from native
    StackIterator* si = si_create_from_native();
    si_transfer_all_preserved_registers(si);

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

    // free lock of synchronized method
    /*
    Currently JIT does not unlock monitors of synchronized blocks relying
    on compiler which generates pseudo finally statement to unlock them.
    For correct implementation of PopFrame these monitors will have to be
    unlocked by VM, so JIT has to store information about these monitors
    somewhere.
    */
    if (method->is_synchronized()) {
        if (is_method_static) {
            assert(!hythread_is_suspend_enabled());
            TRACE2("tm.locks", ("unlock staic sync methods... "));
            vm_monitor_exit(struct_Class_to_java_lang_Class(method->
                    get_class()));
            exn_clear();
        } else {
            JIT *jit = cci->get_jit();
            void **p_this =
                (void **) jit->get_address_of_this(method, jitContext);
            TRACE2("tm.locks", ("unlock sync methods...%x" , *p_this));
            vm_monitor_exit((ManagedObject *) * p_this);
            exn_clear();
        }
    }

    // pop java frame
    si_goto_previous(si);

    // find correct ip and restore required regiksters context
    NativeCodePtr current_method_addr = NULL;
    NativeCodePtr ip = si_get_ip(si);
    size_t ip_reduce;

    // invoke static
    if (is_method_static) {
        ip_reduce = 6;

    // invoke interface
    } else if (0xd0ff == (*((unsigned short*)(((char*)ip)-2)))) {
        ip_reduce = 2;
        current_method_addr = cci->get_code_block_addr();
        jitContext->p_eax = (uint32*)&current_method_addr;

    // invoke virtual and special
    } else {
        VTable_Handle vtable = class_get_vtable( method_class);
        unsigned short code = (*((unsigned short*)(((char*)ip)-3)));

        // invoke virtual
        if (0x50ff == code) {
            jitContext->p_eax = (uint32*) &vtable;
            ip_reduce = 3;
        } else if (0x51ff == code) {
            jitContext->p_ecx = (uint32*) &vtable;
            ip_reduce = 3;
        } else if (0x52ff == code) {
            jitContext->p_edx = (uint32*) &vtable;
            ip_reduce = 3;
        } else if (0x53ff == code) {
            jitContext->p_ebx = (uint32*) &vtable;
            ip_reduce = 3;

        // invoke special
        } else{
            ip_reduce = 6;
        }
    }

    // set corrrrect ip
    ip = (NativeCodePtr)(((char*)ip) - ip_reduce);
    si_set_ip(si, ip, false);

    // transfer cdontrol
    si_transfer_control(si);
}

void jvmti_safe_point()
{
//    __asm int 3;
    TRACE(("entering safe_point"));
    hythread_safe_point();

    TRACE(("left safe_point"));
    frame_type type = m2n_get_frame_type(m2n_get_last_frame());

    if (FRAME_POP_NOW == (FRAME_POP_NOW & type))
        jvmti_jit_do_pop_frame();
}

void jvmti_pop_frame_callback()
{
    TRACE(("suspend_disable post callback..."));
    frame_type type = m2n_get_frame_type(p_TLS_vmthread->last_m2n_frame);

    if (FRAME_POP_NOW != (FRAME_POP_NOW & type))
        return;

    if (is_unwindable()) {
        jvmti_jit_do_pop_frame();
    } else {
        exn_raise_object(VM_Global_State::loader_env->popFrameException);
    }
}
