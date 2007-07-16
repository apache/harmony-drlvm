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

#include "lock_manager.h"
#include "method_lookup.h"
#include "m2n.h"
#include "stack_trace.h"
#include "interpreter.h"
#include "interpreter_exports.h"
#include "compile.h"
#include "jvmti_break_intf.h"
#include "environment.h"
#include "native_modules.h"
#include "native_stack.h"



// Global lock used for locking module list access
static Lock_Manager g_list_lock;


//////////////////////////////////////////////////////////////////////////////
/// Helper functions

static bool get_modules(native_module_t** pmodules)
{
    assert(pmodules);

    if (*pmodules != NULL)
        return true;

    int mod_count;
    native_module_t* mod_list = NULL;

    LMAutoUnlock aulock(&g_list_lock);

    bool result = get_all_native_modules(&mod_list, &mod_count);

    if (!result)
        return false;

    *pmodules = mod_list;
    return true;
}

bool native_is_ip_in_modules(native_module_t* modules, void* ip)
{
    for (native_module_t* module = modules; module; module = module->next)
    {
        for (size_t i = 0; i < module->seg_count; i++)
        {
            char* base = (char*)module->segments[i].base;

            if (ip >= base &&
                ip < (base + module->segments[i].size))
                return true;
        }
    }

    return false;
}

bool native_is_ip_stub(void* ip)
{
    // Synchronizing access to dynamic code list
    LMAutoUnlock dcll(VM_Global_State::loader_env->p_dclist_lock);

    for (DynamicCode *dcList = compile_get_dynamic_code_list();
         NULL != dcList; dcList = dcList->next)
    {
        if (ip >= dcList->address &&
            ip < (void*)((POINTER_SIZE_INT)dcList->address + dcList->length))
            return true;
    }

    return false;
}

static bool native_is_ip_in_breakpoint_handler(void* ip)
{
    return (ip >= &process_native_breakpoint_event &&
            ip < &jvmti_jit_breakpoint_handler);
}
/// Helper functions
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//

static int walk_native_stack_jit(Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array);
static int walk_native_stack_pure(Registers* pregs,
    int max_depth, native_frame_t* frame_array);
static int walk_native_stack_interpreter(Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array);

int walk_native_stack_registers(Registers* pregs,
    VM_thread* pthread, int max_depth, native_frame_t* frame_array)
{
    if (pthread == NULL) // Pure native thread
        return walk_native_stack_pure(pregs, max_depth, frame_array);

    if (interpreter_enabled())
        return walk_native_stack_interpreter(pregs, pthread, max_depth, frame_array);

    return walk_native_stack_jit(pregs, pthread, max_depth, frame_array);
}


static int walk_native_stack_jit(Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array)
{
    // These vars store current frame context for each iteration
    void *ip, *bp, *sp;
    // To translate platform-dependant info; we will use ip from here
    native_get_frame_info(pregs, &ip, &bp, &sp);

    int frame_count = 0;
    
    // Search for method containing corresponding address
    VM_Code_Type code_type = vm_identify_eip(ip);
    bool flag_dummy_frame = false;
    jint java_depth = 0;
    StackIterator* si = NULL;
    bool is_java = false;
    native_module_t* modules = NULL;

    if (code_type == VM_TYPE_JAVA)
    { // We must add dummy M2N frame to start SI iteration
        assert(pthread);
        m2n_push_suspended_frame(pthread, pregs);
        flag_dummy_frame = true;
        is_java = true;
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
            frame_array[frame_count].java_depth =
                is_java ? java_depth : -1;

            frame_array[frame_count].ip = ip;
            frame_array[frame_count].frame = bp;
            frame_array[frame_count].stack = sp;
        }

        ++frame_count;

/////////////////////////
// vv Java vv
        if (is_java) //code_type == VM_TYPE_JAVA
        { // Go to previous frame using StackIterator
            cci = si_get_code_chunk_info(si);
            assert(cci); // Java method should contain cci

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
                native_get_ip_bp_from_si_jit_context(si, &ip, &bp);//???
                native_get_sp_from_si_jit_context(si, &sp);
            }

            code_type = vm_identify_eip(ip);
            is_java = (code_type == VM_TYPE_JAVA);
            continue;
        }
// ^^ Java ^^
/////////////////////////
// vv Native vv
        is_stub = native_is_ip_stub(ip);

        if (is_stub)
        { // Native stub, previous frame is Java frame
            assert(si_is_native(si));

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
            native_get_ip_bp_from_si_jit_context(si, &ip, &bp);
            native_get_sp_from_si_jit_context(si, &sp);
        }
        else
        {
            if (!modules && !get_modules(&modules))
                break;

            if (native_is_frame_valid(modules, bp, sp))
            { // Simply bp-based frame, let's unwind it
                void *tmp_ip, *tmp_bp, *tmp_sp;
                native_unwind_bp_based_frame(bp, &tmp_ip, &tmp_bp, &tmp_sp);

                VMBreakPoints* vm_breaks = VM_Global_State::loader_env->TI->vm_brpt;
                vm_breaks->lock();

                if (native_is_ip_in_breakpoint_handler(tmp_ip))
                {
                    native_unwind_interrupted_frame(&pthread->jvmti_thread, &ip, &bp, &sp);
                    flag_breakpoint = true;
                }
                else
                {
                    ip = tmp_ip;
                    bp = tmp_bp;
                    sp = tmp_sp;
                }

                vm_breaks->unlock();
            }
            else
            { // Is not bp-based frame
                if (frame_count == 1)
                { // For first frame, test special unwinding possibility
                    special_count = native_test_unwind_special(modules, sp);
                    if (special_count <= 0)
                        ip = NULL;
                }

                if (frame_count <= special_count)
                { // Specially unwind first native frames
                    bool res = native_unwind_special(modules,
                                        sp, &ip, &sp, &bp,
                                        frame_count == special_count);

                    if (!res)
                        break; // Unwinding failed
                }
                else
                    break; // There is another invalid frame in the stack
            }
        }
            
        code_type = vm_identify_eip(ip);
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


static int walk_native_stack_pure(Registers* pregs,
    int max_depth, native_frame_t* frame_array)
{
    // These vars store current frame context for each iteration
    void *ip, *bp, *sp;
    // To translate platform-dependant info; we will use ip from here
    native_get_frame_info(pregs, &ip, &bp, &sp);
    assert(vm_identify_eip(ip) != VM_TYPE_JAVA);

    int frame_count = 0;
    int special_count = 0;
    native_module_t* modules = NULL;

    while (1)
    {
        if (frame_array != NULL && frame_count >= max_depth)
            break;

        if (frame_array)
        { // If frames requested, store current frame
            frame_array[frame_count].java_depth = -1;
            frame_array[frame_count].ip = ip;
            frame_array[frame_count].frame = bp;
            frame_array[frame_count].stack = sp;
        }

        ++frame_count;

        if (!modules && !get_modules(&modules))
            break;

        // Simply bp-based frame, let's unwind it
        if (native_is_frame_valid(modules, bp, sp))
        {
            native_unwind_bp_based_frame(bp, &ip, &bp, &sp);
        }
        else
        { // Wrong frame
            if (frame_count == 1)
            { // For first frame, test special unwinding possibility
                special_count = native_test_unwind_special(modules, sp);
                if (special_count <= 0)
                    break;
            }

            if (frame_count <= special_count)
            { // Specially unwind first native frames
                native_unwind_special(modules, sp, &ip, &sp, &bp,
                                    frame_count == special_count);
            }
            else // There is another invalid frame in the stack
                break;
        }
    }

    return frame_count;
}


static int walk_native_stack_interpreter(Registers* pregs, VM_thread* pthread,
    int max_depth, native_frame_t* frame_array)
{
    // These vars store current frame context for each iteration
    void *ip, *bp, *sp;
    // To translate platform-dependant info; we will use ip from here
    native_get_frame_info(pregs, &ip, &bp, &sp);

    assert(pthread);
    FrameHandle* last_frame = interpreter.interpreter_get_last_frame(pthread);
    FrameHandle* frame = last_frame;

    int frame_count = 0;
    jint java_depth = 0;
    int special_count = 0;
    native_module_t* modules = NULL;

    while (1)
    {
        if (frame_array != NULL && frame_count >= max_depth)
            break;

        if (frame_array)
        { // If frames requested, store current frame
            frame_array[frame_count].java_depth = -1;
            frame_array[frame_count].ip = ip;
            frame_array[frame_count].frame = bp;
            frame_array[frame_count].stack = sp;
        }

        ++frame_count;

        // Store previous value to identify frame range later
        void* prev_sp = sp;

        if (!modules && !get_modules(&modules))
            break;

        if (native_is_frame_valid(modules, bp, sp))
        { // Simply bp-based frame, let's unwind it
            native_unwind_bp_based_frame(bp, &ip, &bp, &sp);
        }
        else
        { // Is not bp-based frame
            if (frame_count == 1)
            { // For first frame, test special unwinding possibility
                special_count = native_test_unwind_special(modules, sp);
                if (special_count <= 0)
                    break;
            }

            if (frame_count <= special_count)
            { // Specially unwind first native frames
                native_unwind_special(modules, sp, &ip, &sp, &bp,
                                    frame_count == special_count);
            }
            else
                break; // There is another invalid frame in the stack
        }

        bool is_java = interpreter.is_frame_in_native_frame(frame, prev_sp, sp);

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
