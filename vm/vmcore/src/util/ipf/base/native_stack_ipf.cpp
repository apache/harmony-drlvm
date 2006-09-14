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
 * @author Ilya Berezhniuk
 * @version $Revision: 1.1.2.1 $
 */

#include "native_stack.h"

void native_get_frame_info(Registers* regs, void** ip, void** ret, void** bp, void** sp)
{
    // TODO: implement copying
}

bool native_unwind_bp_based_frame(void* frame, void** ip, void** bp, void** sp)
{
    return false; // Not implemented
}

void native_get_ip_bp_from_si_jit_context(StackIterator* si, void** ip, void** bp)
{ // Not implemented
}

void native_get_sp_from_si_jit_context(StackIterator* si, void** sp)
{ // Not implemented
}

bool native_is_out_of_stack(void* value)
{
    return true; // Not implemented
}

bool native_is_frame_valid(native_module_t* modules, void* bp, void* sp)
{
    return false; // Not implemented
}

int native_test_unwind_special(native_module_t* modules, void* sp)
{
    return false; // Not implemented
}

bool native_unwind_special(native_module_t* modules,
                void* stack, void** ip, void** sp, void** bp, bool is_last)
{
    return false; // Not implemented
}
