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

typedef struct WalkContext {
    native_module_t*    modules;
    bool                clean_modules;
    native_segment_t    stack;
} WalkContext;


// If frame_array is NULL, only returns real frame count
int walk_native_stack_registers(WalkContext* context, Registers* pregs,
    VM_thread* pthread, int max_depth, native_frame_t* frame_array);

bool native_init_walk_context(WalkContext* context, native_module_t* modules, Registers* regs);
void native_clean_walk_context(WalkContext* context);

//////////////////////////////////////////////////////////////////////////////
// Interchange between platform-dependent and general functions

bool native_unwind_stack_frame(WalkContext* context, Registers* regs);
void native_get_regs_from_jit_context(JitFrameContext* jfc, Registers* regs);
bool native_get_stack_range(WalkContext* context, Registers* regs, native_segment_t* seg);
bool native_is_frame_exists(WalkContext* context, Registers* regs);
bool native_unwind_special(WalkContext* context, Registers* regs);
bool native_is_in_code(WalkContext* context, void* ip);
bool native_is_in_stack(WalkContext* context, void* sp);
bool native_is_ip_stub(void* ip);
char* native_get_stub_name(void* ip, char* buf, size_t buflen);
void native_fill_frame_info(Registers* regs, native_frame_t* frame, jint jdepth);


#ifdef __cplusplus
}
#endif

#endif // _NATIVE_STACK_H_
