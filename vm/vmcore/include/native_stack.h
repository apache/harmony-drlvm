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

#ifndef _NATIVE_STACK_H_
#define _NATIVE_STACK_H_

#include "jni.h"
#include "stack_iterator.h"
#include "native_modules.h"
#include "vm_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    jint    java_depth;
    void*   ip;
    void*   frame;
    void*   stack;
} native_frame_t;


// If frame_array is NULL, only returns real frame count
int walk_native_stack_registers(Registers* pregs,
    VM_thread* pthread, int max_depth, native_frame_t* frame_array);

//////////////////////////////////////////////////////////////////////////////
// Interchange between platform-dependent and general functions
void native_get_frame_info(Registers* regs, void** ip, void** bp, void** sp);
bool native_unwind_bp_based_frame(void* frame, void** ip, void** bp, void** sp);
void native_get_ip_bp_from_si_jit_context(StackIterator* si, void** ip, void** bp);
void native_get_sp_from_si_jit_context(StackIterator* si, void** sp);
bool native_is_out_of_stack(void* value);
bool native_is_frame_valid(native_module_t* modules, void* bp, void* sp);
int native_test_unwind_special(native_module_t* modules, void* sp);
bool native_unwind_special(native_module_t* modules,
                void* stack, void** ip, void** sp, void** bp, bool is_last);
void native_unwind_interrupted_frame(jvmti_thread_t thread, void** p_ip, void** p_bp, void** p_sp);
bool native_is_ip_in_modules(native_module_t* modules, void* ip);
bool native_is_ip_stub(void* ip);


#ifdef __cplusplus
}
#endif

#endif // _NATIVE_STACK_H_
