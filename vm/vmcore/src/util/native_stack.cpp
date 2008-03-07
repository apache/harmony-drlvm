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
 * @author Ilya Berezhniuk
 * @version $Revision: 1.1.2.1 $
 */

#include <string.h>
#include "lock_manager.h"
#include "m2n.h"
#include "stack_trace.h"
#include "interpreter.h"
#include "interpreter_exports.h"
#include "compile.h"
#include "jvmti_break_intf.h"
#include "environment.h"
#include "native_modules.h"
#include "native_stack.h"


//////////////////////////////////////////////////////////////////////////////
/// Helper functions

static DynamicCode* native_find_stub(void* ip)
{
    for (DynamicCode *dcList = compile_get_dynamic_code_list();
         NULL != dcList; dcList = dcList->next)
    {
        if (ip >= dcList->address &&
            ip < (void*)((POINTER_SIZE_INT)dcList->address + dcList->length))
            return dcList;
    }

    return NULL;
}

char* native_get_stub_name(void* ip, char* buf, size_t buflen)
{
    // Synchronizing access to dynamic code list
    LMAutoUnlock dcll(VM_Global_State::loader_env->p_dclist_lock);

    if (!buf || buflen == 0)
        return NULL;

    DynamicCode* code = native_find_stub(ip);

    if (!code || !code->name)
        return NULL;

    strncpy(buf, code->name, buflen);
    buf[buflen - 1] = '\0';

    return buf;
}

bool native_is_in_stack(WalkContext* context, void* sp)
{
    return (sp >= context->stack.base &&
            sp < (char*)context->stack.base + context->stack.size);
}

bool native_is_ip_stub(void* ip)
{
    // Synchronizing access to dynamic code list
    LMAutoUnlock dcll(VM_Global_State::loader_env->p_dclist_lock);

    return (native_find_stub(ip) != NULL);
}

/*
Now the technique for calling C handler from a signal/exception context
guarantees that all needed return addresses are present in stack, so
there is no need in special processing
static bool native_is_ip_in_breakpoint_handler(void* ip)
{
    return (ip >= &process_native_breakpoint_event &&
            ip < &jvmti_jit_breakpoint_handler);
}*/
/// Helper functions
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//

bool native_init_walk_context(WalkContext* context, native_module_t* modules, Registers* regs)
{
    if (!context)
        return false;

    if (!modules)
    {
        int mod_count;
        native_module_t* mod_list = NULL;

        if (!port_get_all_modules(&mod_list, &mod_count))
            return false;

        context->clean_modules = true;
        context->modules = mod_list;
    }
    else
    {
        context->clean_modules = false;
        context->modules = modules;
    }

    if (!native_get_stack_range(context, regs, &context->stack))
    {
        if (context->clean_modules)
            port_clear_modules(&context->modules);
        return false;
    }

    return true;
}

void native_clean_walk_context(WalkContext* context)
{
    if (!context)
        return;

    if (context->modules && context->clean_modules)
    {
        port_clear_modules(&context->modules);
    }

    context->modules = NULL;
}


static int walk_native_stack_jit(
    WalkContext* context,
    Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array);
static int walk_native_stack_pure(
    WalkContext* context, Registers* pregs,
    int max_depth, native_frame_t* frame_array);
static int walk_native_stack_interpreter(
    WalkContext* context,
    Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array);

int walk_native_stack_registers(WalkContext* context, Registers* pregs,
    VM_thread* pthread, int max_depth, native_frame_t* frame_array)
{
    if (pthread == NULL) // Pure native thread
        return walk_native_stack_pure(context, pregs, max_depth, frame_array);

    if (interpreter_enabled())
        return walk_native_stack_interpreter(context, pregs,
                                            pthread, max_depth, frame_array);

    return walk_native_stack_jit(context, pregs, pthread, max_depth, frame_array);
    return 0;
}


static int walk_native_stack_jit(
    WalkContext* context,
    Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array)
{
    // Register context for current frame
    Registers regs = *pregs;

    int frame_count = 0;
    
    // Search for method containing corresponding address
    VM_Code_Type code_type = vm_identify_eip(regs.get_ip());
    bool flag_dummy_frame = false;
    jint java_depth = 0;
    StackIterator* si = NULL;
    bool is_java = false;

    if (code_type == VM_TYPE_JAVA)
    { // We must add dummy M2N frame to start SI iteration
        if (!pthread)
            return 0;

        is_java = true;

        M2nFrame* lm2n = m2n_get_last_frame(pthread);

        if (!m2n_is_suspended_frame(lm2n) || m2n_get_ip(lm2n) != regs.get_ip())
        { // We should not push frame if it was pushed by breakpoint handler
            m2n_push_suspended_frame(pthread, &regs);
            flag_dummy_frame = true;
        }
    }

    si = si_create_from_native(pthread);

    if (is_java ||
        // Frame was pushed already by breakpoint handler
        (si_is_native(si) && si_get_m2n(si) && m2n_is_suspended_frame(si_get_m2n(si))))
    {
        si_goto_previous(si);
    }

    jint inline_index = -1;
    jint inline_count;
    CodeChunkInfo* cci = NULL;
    bool is_stub = false;
    int special_count = 0;
    bool flag_breakpoint = false;

    while (1)
    {
        if (frame_array != NULL && frame_count >= max_depth)
            break;

        if (frame_array)
        { // If frames requested, store current frame
            native_fill_frame_info(&regs, &frame_array[frame_count],
                is_java ? java_depth : -1);
        }

        ++frame_count;

/////////////////////////
// vv Java vv
        if (is_java) //code_type == VM_TYPE_JAVA
        { // Go to previous frame using StackIterator
            cci = si_get_code_chunk_info(si);
            if (!cci) // Java method should contain cci
                break;

            // if (inline_index < 0) we have new si
            // (inline_index < inline_count) we must process inline
            // (inline_index == inline_count) we must process si itself
            if (inline_index < 0)
            {
                inline_count = si_get_inline_depth(si);
                inline_index = 0;
            }

            ++java_depth;

            if (inline_index < inline_count)
            { // Process inlined method
                // We don't need to update context,
                // because context is equal for
                // all inlined methods
                ++inline_index;
            }
            else
            {
                inline_index = -1;
                // Go to previous stack frame from StackIterator
                si_goto_previous(si);
                native_get_regs_from_jit_context(si_get_jit_context(si), &regs);
            }

            code_type = vm_identify_eip(regs.get_ip());
            is_java = (code_type == VM_TYPE_JAVA);
            continue;
        }
// ^^ Java ^^
/////////////////////////
// vv Native vv
        is_stub = native_is_ip_stub(regs.get_ip());

        if (is_stub)
        { // Native stub, previous frame is Java frame
            if (!si_is_native(si))
                break;

            if (si_get_method(si)) // Frame represents JNI frame
            {
                // Mark first frame (JNI stub) as Java frame
                if (frame_array && frame_count == 1)
                    frame_array[frame_count - 1].java_depth = java_depth;

                if (frame_array && frame_count > 1)
                    frame_array[frame_count - 2].java_depth = java_depth;

                ++java_depth;
            }

            // Ge to previous stack frame from StackIterator
            si_goto_previous(si);
            // Let's get context from si
            native_get_regs_from_jit_context(si_get_jit_context(si), &regs);
        }
        else
        {
            Registers tmp_regs = regs;

            if (native_is_frame_exists(context, &tmp_regs))
            { // Stack frame (x86)
                if (!native_unwind_stack_frame(context, &tmp_regs))
                    break;
            }
            else
            { // Stack frame does not exist, try using heuristics
                if (!native_unwind_special(context, &tmp_regs))
                    break;
            }

            VMBreakPoints* vm_breaks = VM_Global_State::loader_env->TI->vm_brpt;
            vm_breaks->lock();
/*
Now the technique for calling C handler from a signal/exception context
guarantees that all needed return addresses are present in stack, so
there is no need in special processing
            if (native_is_ip_in_breakpoint_handler(tmp_regs.get_ip()))
            {
                regs = *pthread->jvmti_thread.jvmti_saved_exception_registers;
                flag_breakpoint = true;
            }
            else*/
                regs = tmp_regs;

            vm_breaks->unlock();
        }
            
        code_type = vm_identify_eip(regs.get_ip());
        is_java = (code_type == VM_TYPE_JAVA);

        // If we've reached Java without native stub (or breakpoint handler frame)
        if (is_java && !is_stub && !flag_breakpoint)
            break; // then stop processing
        flag_breakpoint = false;
// ^^ Native ^^
/////////////////////////
    }

    if (flag_dummy_frame)
    { // Delete previously added dummy frame
        M2nFrame* plm2n = m2n_get_last_frame(pthread);
        m2n_set_last_frame(pthread, m2n_get_previous_frame(plm2n));
        STD_FREE(plm2n);
    }

    si_free(si);

    return frame_count;
}


static int walk_native_stack_pure(
    WalkContext* context, Registers* pregs,
    int max_depth, native_frame_t* frame_array)
{
    // Register context for current frame
    Registers regs = *pregs;
    if (vm_identify_eip(regs.get_ip()) == VM_TYPE_JAVA)
        return 0;

    int frame_count = 0;

    while (1)
    {
        if (frame_array != NULL && frame_count >= max_depth)
            break;

        if (frame_array)
        { // If frames requested, store current frame
            native_fill_frame_info(&regs, &frame_array[frame_count], -1);
        }

        ++frame_count;

        if (native_is_frame_exists(context, &regs))
        { // Stack frame (x86)
            // Here must be special processing for breakpoint handler frames
            // But it requires VM_thread structure attached to thread
            // TODO: Investigate possibility
            if (!native_unwind_stack_frame(context, &regs))
                break;
        }
        else
        { // Stack frame does not exist, try using heuristics
            if (!native_unwind_special(context, &regs))
                break;
        }
    }

    return frame_count;
}


static int walk_native_stack_interpreter(
    WalkContext* context,
    Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array)
{
    // Register context for current frame
    Registers regs = *pregs;

    assert(pthread);
    FrameHandle* last_frame = interpreter.interpreter_get_last_frame(pthread);
    FrameHandle* frame = last_frame;

    int frame_count = 0;
    jint java_depth = 0;

    while (1)
    {
        if (frame_array != NULL && frame_count >= max_depth)
            break;

        if (frame_array)
        { // If frames requested, store current frame
            native_fill_frame_info(&regs, &frame_array[frame_count], -1);
        }

        ++frame_count;

        // Store previous value to identify frame range later
        void* prev_sp = regs.get_sp();
        Registers tmp_regs = regs;

        if (native_is_frame_exists(context, &tmp_regs))
        { // Stack frame (x86)
            if (!native_unwind_stack_frame(context, &tmp_regs))
                break;
        }
        else
        { // Stack frame does not exist, try using heuristics
            if (!native_unwind_special(context, &tmp_regs))
                break;
        }

        VMBreakPoints* vm_breaks = VM_Global_State::loader_env->TI->vm_brpt;
        vm_breaks->lock();
/*
Now the technique for calling C handler from a signal/exception context
guarantees that all needed return addresses are present in stack, so
there is no need in special processing
        if (native_is_ip_in_breakpoint_handler(tmp_regs.get_ip()))
            regs = *pthread->jvmti_thread.jvmti_saved_exception_registers;
        else*/
            regs = tmp_regs;

        vm_breaks->unlock();

        bool is_java = interpreter.is_frame_in_native_frame(frame, prev_sp, regs.get_sp());

        if (is_java)
        {
            // Set Java frame number
            if (frame_array && frame_count > 1)
                frame_array[frame_count - 2].java_depth = java_depth++;

            // Go to previous frame
            frame = interpreter.interpreter_get_prev_frame(frame);
        }
    }

    return frame_count;
}
