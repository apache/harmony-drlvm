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
#include "m2n.h"
#include "stack_iterator.h"
#include "cxxlog.h"

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

    return JVMTI_ERROR_NONE;
}

void jvmti_jit_do_pop_frame()
{
    TRACE("jvmti_jit_do_pop_frame() is called");
}

void jvmti_safe_point()
{
//    __asm int 3;
    TRACE("entering safe_point");
    hythread_safe_point();

    TRACE("left safe_point");
    frame_type type = m2n_get_frame_type(m2n_get_last_frame());

    if (FRAME_POP_NOW == (FRAME_POP_NOW & type))
        jvmti_jit_do_pop_frame();
}

void jvmti_pop_frame_callback()
{
    TRACE("suspend_disable post callback...");
    frame_type type = m2n_get_frame_type(p_TLS_vmthread->last_m2n_frame);

    if (FRAME_POP_NOW != (FRAME_POP_NOW & type))
        return;

    if (is_unwindable()) {
        jvmti_jit_do_pop_frame();
    } else {
        exn_raise_object(VM_Global_State::loader_env->popFrameException);
    }
}
