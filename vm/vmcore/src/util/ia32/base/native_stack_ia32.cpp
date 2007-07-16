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

#include "method_lookup.h"
#include "native_stack.h"

void native_get_frame_info(Registers* regs, void** ip, void** bp, void** sp)
{
    *ip = (void*)regs->eip;
    *bp = (void*)regs->ebp;
    *sp = (void*)regs->esp;
}

bool native_unwind_bp_based_frame(void* frame, void** ip, void** bp, void** sp)
{
    void** frame_ptr = (void**)frame;

    *ip = frame_ptr[1];
    *bp = frame_ptr[0];
    // FIXME - wrong value, we cannot get sp anyhow
    *sp = (void*)((POINTER_SIZE_INT)frame + 2*sizeof(void*));

    return (*bp != NULL && *ip != NULL);
}

void native_get_ip_bp_from_si_jit_context(StackIterator* si, void** ip, void** bp)
{
    JitFrameContext* jfc = si_get_jit_context(si);
    *ip = (void*)*jfc->p_eip;
    *bp = (void*)*jfc->p_ebp;
}

void native_get_sp_from_si_jit_context(StackIterator* si, void** sp)
{
    *sp = (void*)si_get_jit_context(si)->esp;
}

bool native_is_out_of_stack(void* value)
{
    // FIXME: Invalid criterion
    return (value < (void*)0x10000) || (value > (void*)0xC0000000);
}

bool native_is_frame_valid(native_module_t* modules, void* bp, void* sp)
{
    // Check for frame layout and stack values
    if ((bp < sp) || native_is_out_of_stack(bp))
        return false; // Invalid frame

    void** dw_ptr = (void**)bp;
    void* ret_ip = *(dw_ptr + 1); // Return address for frame

    // Check return address for meaning
    if (!native_is_ip_in_modules(modules, ret_ip) && !native_is_ip_stub(ret_ip))
        return false;

    return true;
}

// Searches for correct return address in the stack
// Returns stack address pointing to return address
static void** native_search_special_frame(native_module_t* modules, void* sp)
{
// Max search depth for return address
#define MAX_SPECIAL_DEPTH 0x40

    POINTER_SIZE_INT sp_begin = (POINTER_SIZE_INT)sp;
    for (POINTER_SIZE_INT sp_int = sp_begin;
         sp_int < sp_begin + MAX_SPECIAL_DEPTH;
         sp_int +=2) // 2 is minimum stack item for ia32
    {
        void** sp_pointer = (void**)sp_int;

        if (native_is_ip_in_modules(modules, *sp_pointer))
            return sp_pointer;
    }

    return NULL;
}

// Tests if stack contains non-BP-based frames on top
int native_test_unwind_special(native_module_t* modules, void* sp)
{
#define MAX_SPECIAL_COUNT 16

#if (!defined PLATFORM_POSIX)
    return -1; // Because we cannot identify executable code on Windows
#endif

    if (modules == NULL)
        return false;

    int count = 0;
    void** sp_pointer = (void**)sp;

    do
    {
        if (native_is_ip_stub(sp_pointer[-1]))
            break; // We've found JNI stub

        // We've reached Java without native stub
        if (vm_identify_eip(sp_pointer[-1]) == VM_TYPE_JAVA)
            break;

        void** next_sp = native_search_special_frame(modules, sp_pointer);

        if (next_sp == NULL)
            break; // We cannot unwind anymore

        if (count > 0 &&                     // Check BP-frame for upper frames
            sp_pointer[-2] >= sp_pointer &&  // Correct frame layout
            sp_pointer[-2] == next_sp - 1 && // is BP saved correctly
            next_sp[-1] >= next_sp + 1)      // Correct next frame layout
        {
            break;
        }

        sp_pointer = next_sp + 1;

    } while (++count <= MAX_SPECIAL_COUNT);

    return count;
}

// Tries to unwind non-BP-based frame
bool native_unwind_special(native_module_t* modules,
    void* stack, void** ip, void** sp, void** bp, bool is_last)
{
    if (modules == NULL)
    {
        *ip = NULL;
        return false;
    }

    void** found = NULL;

    POINTER_SIZE_INT sp_begin = (POINTER_SIZE_INT)stack;
    for (POINTER_SIZE_INT sp_int = sp_begin;
         sp_int < sp_begin + MAX_SPECIAL_DEPTH;
         sp_int +=2) // 2 is minimum stack item for ia32
    {
        void** sp_pointer = (void**)sp_int;

        if (native_is_ip_in_modules(modules, *sp_pointer))
        {
            found = sp_pointer;
            break;
        }
    }

    if (!found)
    {
        *ip = NULL;
        return false;
    }

    *ip = *found;
    *sp = found + 1;
    
    if (is_last && !native_is_ip_stub(*ip))
        *bp = found[-1];
    else
        *bp = *sp;

    return true;
}

void native_unwind_interrupted_frame(jvmti_thread_t thread, void** p_ip, void** p_bp, void** p_sp)
{
    if (!thread) {
        return;
    }
    Registers* pregs = (Registers*)(thread->jvmti_saved_exception_registers);
    *p_ip = (void*)pregs->eip;
    *p_bp = (void*)pregs->ebp;
    *p_sp = (void*)pregs->esp;
}
