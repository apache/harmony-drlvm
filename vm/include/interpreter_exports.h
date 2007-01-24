/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements. See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _INTERPRETER_EXPORTS_H_
#define _INTERPRETER_EXPORTS_H_

#include "open/types.h"
#include "jvmti.h"

typedef struct FrameHandle FrameHandle;

typedef struct {
    bool (*interpreter_st_get_frame) (unsigned target_depth, struct StackTraceFrame* stf);
    void (*interpreter_st_get_trace) (class VM_thread *thread, unsigned* res_depth, struct StackTraceFrame** stfs);
    void (*interpreter_enumerate_thread) (class VM_thread *thread);

    FrameHandle* (*interpreter_get_last_frame) (class VM_thread *thread);
    FrameHandle* (*interpreter_get_prev_frame) (FrameHandle* frame);
    // 'end' is not inclusive
    bool (*is_frame_in_native_frame) (struct FrameHandle* frame, void* begin, void* end);

    void (*interpreter_ti_enumerate_thread) (jvmtiEnv*, class VM_thread *thread);

#ifdef _IPF_
    uint64* (*interpreter_get_stacked_register_address) (uint64* bsp, unsigned reg);
#endif

    jvmtiError (*interpreter_ti_getFrameLocation) ( jvmtiEnv*, class VM_thread*,
            int, struct _jmethodID * *, int64 *);
    jvmtiError (*interpreter_ti_getLocal32) ( jvmtiEnv*, class VM_thread*, int, int, int *);
    jvmtiError (*interpreter_ti_getLocal64) ( jvmtiEnv*, class VM_thread*, int, int, int64 *);
    jvmtiError (*interpreter_ti_getObject) ( jvmtiEnv*, class VM_thread*, int, int, struct _jobject * *);
    jvmtiError (*interpreter_ti_getStackTrace) (jvmtiEnv*, class VM_thread*, int, int, jvmtiFrameInfo*, int *);
    jvmtiError (*interpreter_ti_get_frame_count) ( jvmtiEnv*, class VM_thread*, int *);
    jvmtiError (*interpreter_ti_setLocal32) ( jvmtiEnv*, class VM_thread*, int, int, int);
    jvmtiError (*interpreter_ti_setLocal64) ( jvmtiEnv*, class VM_thread*, int, int, int64);
    jvmtiError (*interpreter_ti_setObject) ( jvmtiEnv*, class VM_thread*, int, int, struct _jobject *);
    unsigned int (*interpreter_st_get_interrupted_method_native_bit) (class VM_thread *);


    // Open interfaces part begins

    /**
     * The function is called when global TI event state is changed. This means
     * that atleast one of jvmtiEnv's enabled the event or the event was
     * disabled in all enviroments.
     *
     * @param event_type  -  jvmti to enable / disable
     * @param enable      - enable or disable the events in exe.
     */
    void (*interpreter_ti_set_notification_mode)(jvmtiEvent event_type, bool enable);

    /**
     * Set breakpoint in place identified by method and location.
     * No more then one breakpoint will be set at any specific place. Handling
     * for multiple jvmti environments is done by jvmti framework.
     *
     * @return Bytecode has been replaced by instrumentation.
     */
    jbyte (*interpreter_ti_set_breakpoint)(jmethodID method, jlocation location);

    /**
     * Clear breakpoint in place identified by method and location.
     * Replaced bytecode (returned by <code>interpreter_ti_set_breakpoint(..)</code>)
     * is also passed as a parameter.
     */
    void (*interpreter_ti_clear_breakpoint)(jmethodID method, jlocation location, jbyte saved);

    /**
     * Set callback to notify JVMTI about frame pop event.
     *
     * @return JVMTI_ERROR_NONE           - successfully added notification<br>
     *         JVMTI_ERROR_OPAQUE_FRAME   - frame is native<br>
     *         JVMTI_ERROR_NO_MORE_FRAMES - depth too large<br>
     */
    jvmtiError (*interpreter_ti_notify_frame_pop) (jvmtiEnv*,
                                                   VM_thread *thread,
                                                   int depth);

    jvmtiError (*interpreter_ti_pop_frame) (jvmtiEnv*, VM_thread *thread);

    void (*stack_dump) (VM_thread*);

} Interpreter;

VMEXPORT Interpreter *interpreter_table();

#ifdef BUILDING_VM
extern Interpreter interpreter;
extern bool interpreter_enabled();
#endif

#endif /* _INTERPRETER_EXPORTS_H_ */
