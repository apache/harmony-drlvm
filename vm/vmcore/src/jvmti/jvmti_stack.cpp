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
 * @author Gregory Shimansky, Pavel Afremov
 * @version $Revision: 1.1.2.2.4.5 $
 */  
/*
 * JVMTI stack frame API
 */

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "interpreter_exports.h"
#include "vm_threads.h"
#include "environment.h"
#include "Class.h"
#include "cxxlog.h"
#include "thread_generic.h"
#include "open/jthread.h"
#include "suspend_checker.h"
#include "stack_trace.h"
#include "stack_iterator.h"
#include "jit_intf_cpp.h"
#include "thread_manager.h"
#include "cci.h"

#define jvmti_test_jenv (p_TLS_vmthread->jni_env)

jthread getCurrentThread() {
    tmn_suspend_disable();
    ObjectHandle hThread = oh_allocate_local_handle();
    jthread thread = jthread_self();
    if(thread) {
        hThread->object = (Java_java_lang_Thread *)thread->object;
    } else {
        hThread->object = NULL;
    }
    tmn_suspend_enable();
    return (jthread) hThread;
}

jint get_thread_stack_depth(VM_thread *thread, jint* pskip)
{
    StackIterator* si = si_create_from_native(thread);
    jint depth = 0;
    Method_Handle method = NULL;

    jint skip = 0;

    while (!si_is_past_end(si))
    {
        method = si_get_method(si);

        if (method)
        {
            depth += 1 + si_get_inline_depth(si);

            Class *clss = method_get_class(method);
            assert(clss);

            if (!strcmp(method_get_name(method), "runImpl")) {
                if(!strcmp(class_get_name(clss), "java/lang/VMStart$MainThread")) {
                    skip = 3;
                } else if(!strcmp(class_get_name(clss), "java/lang/Thread")) {
                    skip = 1;
                }
            }
        }

        si_goto_previous(si);
    }

    depth -= skip;

    if (pskip)
        *pskip = skip;

    si_free(si);
    return depth;
}

/*
 * Get Stack Trace
 *
 * Get information about the stack of a thread. If max_frame_count
 * is less than the depth of the stack, the max_frame_count
 * deepest frames are returned, otherwise the entire stack is
 * returned. Deepest frames are at the beginning of the returned
 * buffer.
 *
 * REQUIRED Functionality
 */

jvmtiError JNICALL
jvmtiGetStackTrace(jvmtiEnv* env,
                   jthread thread,
                   jint start_depth,
                   jint max_frame_count,
                   jvmtiFrameInfo* frame_buffer,
                   jint* count_ptr)
{
    TRACE2("jvmti.stack", "GetStackTrace called");
    SuspendEnabledChecker sec;
    jint state;
    jvmtiError err;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    // check error condition: JVMTI_ERROR_INVALID_THREAD
    err = jvmtiGetThreadState(env, thread, &state);

    if (err != JVMTI_ERROR_NONE) {
        return err;
    }

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0) {
        return JVMTI_ERROR_THREAD_NOT_ALIVE;
    }

    // check error condition: JVMTI_ERROR_NULL_POINTER
    if (frame_buffer == 0) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    // check error condition: JVMTI_ERROR_NULL_POINTER
    if (count_ptr == 0) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    // check error condition: JVMTI_ERROR_ILLEGAL_ARGUMENT
    if (max_frame_count < 0) {
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }

    bool thread_suspended = false;
    // Suspend thread before getting stacks
    VM_thread *vm_thread;
    if (NULL != thread)
    {
        vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);
        // Check that this thread is not current
        if (vm_thread != p_TLS_vmthread)
        {
            jthread_suspend(thread);
            thread_suspended = true;
        }
    }
    else
        vm_thread = p_TLS_vmthread;

    jvmtiError errStack;
    if (interpreter_enabled())
        // check error condition: JVMTI_ERROR_ILLEGAL_ARGUMENT
        errStack = interpreter.interpreter_ti_getStackTrace(env,
            vm_thread, start_depth, max_frame_count, frame_buffer, count_ptr);
    else
    {
        jint start, skip = 0;
        jint depth = get_thread_stack_depth(vm_thread, &skip);

        if (start_depth < 0)
        {
            start = depth + start_depth;
            if (start < 0)
            {
                if (thread_suspended)
                    jthread_resume(thread);
                return JVMTI_ERROR_ILLEGAL_ARGUMENT;
            }
        }
        else
            start = start_depth;

        StackIterator *si = si_create_from_native(vm_thread);
        jint count = 0;
        jint stack_depth = 0;
        jint max_depth = depth;
        while (!si_is_past_end(si) && count < max_frame_count &&
               count < max_depth)
        {
            jint inlined_depth = si_get_inline_depth(si);
            if (stack_depth + inlined_depth >= start)
            {
                Method *method = si_get_method(si);
                if (NULL != method)
                {
                    CodeChunkInfo *cci = si_get_code_chunk_info(si);
                    if (NULL != cci)
                    {
                        NativeCodePtr ip = si_get_ip(si);
                        // FIXME64: missing support for methods
                        // with compiled code greater than 2GB
                        uint32 offset = (uint32)((POINTER_SIZE_INT)ip -
                            (POINTER_SIZE_INT)cci->get_code_block_addr());
                        JIT *jit = cci->get_jit();

                        uint16 bc;
                        for (jint iii = 0;
                             iii < inlined_depth && count < max_frame_count &&
                             count < max_depth; iii++)
                        {
                            if (stack_depth >= start)
                            {
                                Method *inlined_method = jit->get_inlined_method(
                                    cci->get_inline_info(), offset, iii);

                                OpenExeJpdaError UNREF result =
                                    jit->get_bc_location_for_native(
                                        inlined_method, ip, &bc);
                                assert(result == EXE_ERROR_NONE);

                                if (0 < stack_depth - iii) {
                                    bc--;
                                }
                                frame_buffer[count].location = bc;
                                frame_buffer[count].method =
                                    reinterpret_cast<jmethodID>(method);
                                count++;
                            }
                            stack_depth++;
                        }

                        // With all inlines we could've moved past the user limit
                        if (count < max_frame_count && count < max_depth)
                        {
                            OpenExeJpdaError UNREF result =
                                jit->get_bc_location_for_native(
                                    method, ip, &bc);
                            assert(result == EXE_ERROR_NONE);

                            if (0 < stack_depth - inlined_depth) {
                                bc--;
                            }
                            frame_buffer[count].location = bc;
                        }
                    }
                    else
                    {
                        assert(si_is_native(si));
                        frame_buffer[count].location = -1;
                    }

                    // With all inlines we could've moved past the user limit
                    if (count < max_frame_count)
                    {
                        frame_buffer[count].method =
                            reinterpret_cast<jmethodID>(method);
                        count++;
                    }

                    stack_depth++;
                }
            }
            si_goto_previous(si);
        }

        *count_ptr = count;
        if (si_is_past_end(si) && count == 0)
            errStack = JVMTI_ERROR_ILLEGAL_ARGUMENT;
        else
            errStack = JVMTI_ERROR_NONE;
        si_free(si);
    }

    if (thread_suspended)
        jthread_resume(thread);

    return errStack;
}

/*
 * Get All Stack Traces
 *
 * Get information about the stacks of all live threads.
 * If max_frame_count is less than the depth of a stack,
 * the max_frame_count deepest frames are returned for that
 * thread, otherwise the entire stack is returned. Deepest
 * frames are at the beginning of the returned buffer.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetAllStackTraces(jvmtiEnv* env,
                       jint max_frame_count,
                       jvmtiStackInfo** stack_info_ptr,
                       jint* thread_count_ptr)
{
    TRACE2("jvmti.stack", "GetAllStackTraces called");
    SuspendEnabledChecker sec;
    jint count;
    jthread *threads;
    jvmtiError res;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == stack_info_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL == thread_count_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    if (max_frame_count < 0)
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    // FIXME: new threads can be created before all threads are suspended
    // event handler for thread_creating should block creation new threads
    // at this moment
    assert(hythread_is_suspend_enabled());
    hythread_suspend_all(NULL, NULL);
    res = jvmtiGetAllThreads(env, &count, &threads);

    if (res != JVMTI_ERROR_NONE) {
       hythread_resume_all(NULL);
       return res;
    }

    jvmtiStackInfo *info;
    // FIXME: memory leak in case of error
    res = _allocate(sizeof(jvmtiStackInfo) * count +
        sizeof(jvmtiFrameInfo) * max_frame_count * count,
        (unsigned char **)&info);

    if (JVMTI_ERROR_NONE != res) {
       hythread_resume_all(NULL);
       return res;
    }

    // getting thread states
    for(int i = 0; i < count; i++) {
        info[i].thread = threads[i];
        res = jvmtiGetThreadState(env, threads[i], &info[i].state);

        if (JVMTI_ERROR_NONE != res) {
            hythread_resume_all(NULL);
            return res;
        }

        // Frame_buffer pointer should pointer to the memory right
        // after the jvmtiStackInfo structures
        info[i].frame_buffer = ((jvmtiFrameInfo *)&info[count]) +
            max_frame_count * i;
        res = jvmtiGetStackTrace(env, info[i].thread, 0, max_frame_count,
            info[i].frame_buffer, &info[i].frame_count);

        if (JVMTI_ERROR_NONE != res) {
            hythread_resume_all(NULL);
            return res;
        }
    }

    hythread_resume_all(NULL);

    *thread_count_ptr = count;
    *stack_info_ptr = info;
    return JVMTI_ERROR_NONE;
}

/*
 * Get Thread List Stack Traces
 *
 * Get information about the stacks of the supplied threads.
 * If max_frame_count is less than the depth of a stack, the
 * max_frame_count deepest frames are returned for that thread,
 * otherwise the entire stack is returned. Deepest frames are
 * at the beginning of the returned buffer.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetThreadListStackTraces(jvmtiEnv* env,
                              jint thread_count,
                              const jthread* thread_list,
                              jint max_frame_count,
                              jvmtiStackInfo** stack_info_ptr)
{
    TRACE2("jvmti.stack", "GetThreadListStackTraces called");
    SuspendEnabledChecker sec;
    jint count = thread_count;
    const jthread *threads = thread_list;
    jvmtiError res;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == stack_info_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL == thread_list)
        return JVMTI_ERROR_NULL_POINTER;

    if (thread_count < 0)
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    if (max_frame_count < 0)
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    jvmtiStackInfo *info;
    res = _allocate(sizeof(jvmtiStackInfo) * count +
        sizeof(jvmtiFrameInfo) * max_frame_count * count,
        (unsigned char **)&info);

    if (JVMTI_ERROR_NONE != res)
       return res;

    int i;
    jthread currentThread = getCurrentThread();
    // stopping all threads
    for(i = 0; i < count; i++) {
        // FIXME: thread can be dead at this time
        // event handler for thread death should block thread death
        // until the function end.
        info[i].thread = threads[i];
        res = jvmtiGetThreadState(env, threads[i], &info[i].state);

        if (JVMTI_ERROR_NONE != res)
            return res;

        // FIXME: suspended thread might be resumed in other jvmti thread?
        if (info[i].state != JVMTI_THREAD_STATE_SUSPENDED) {
            if (IsSameObject(jvmti_test_jenv, currentThread, threads[i]))
                continue;
            jthread_suspend(threads[i]);
        }
    }

    // getting thread states
    for(i = 0; i < count; i++) {
        // Frame_buffer pointer should pointer to the memory right
        // after the jvmtiStackInfo structures
        info[i].frame_buffer = ((jvmtiFrameInfo *)&info[count]) +
            max_frame_count * i;
        res = jvmtiGetStackTrace(env, info[i].thread, 0, max_frame_count,
                info[i].frame_buffer, &info[i].frame_count);

        // if thread was terminated before we got it's stack - return null frame_count
        if (JVMTI_ERROR_THREAD_NOT_ALIVE == res)
            info[i].frame_count = 0;
        else if (JVMTI_ERROR_NONE != res)
            break;
    }

    // unsuspend suspended threads.
    for(i = 0; i < count; i++) {
        if (info[i].state != JVMTI_THREAD_STATE_SUSPENDED)
            jthread_resume(threads[i]);
    }

    if (JVMTI_ERROR_NONE != res)
        return res;

    *stack_info_ptr = info;
    return JVMTI_ERROR_NONE;
}

/*
 * Get Frame Count
 *
 * Get the number of frames currently in the specified thread's
 * call stack.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetFrameCount(jvmtiEnv* env,
                   jthread thread,
                   jint* count_ptr)
{
    TRACE2("jvmti.stack", "GetFrameCount called");
    SuspendEnabledChecker sec;
    jint state;
    jvmtiError err;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (thread == 0) {
        thread = getCurrentThread();
    }

    // check error condition: JVMTI_ERROR_INVALID_THREAD
    err = env->GetThreadState(thread, &state);

    if (err != JVMTI_ERROR_NONE) {
        return err;
    }

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0) {
        return JVMTI_ERROR_THREAD_NOT_ALIVE;
    }

    // check error condition: JVMTI_ERROR_NULL_POINTER
    if (count_ptr == 0) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    bool thread_suspended = false;
    // Suspend thread before getting stacks
    VM_thread *vm_thread;
    if (NULL != thread)
    {
        vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);
        // Check that this thread is not current
        if (vm_thread != p_TLS_vmthread)
        {
            jthread_suspend(thread);
            thread_suspended = true;
        }
    }
    else
        vm_thread = p_TLS_vmthread;

    jvmtiError errStack;
    if (interpreter_enabled())
        errStack = interpreter.interpreter_ti_get_frame_count(env,
            vm_thread, count_ptr);
    else
    {
        jint depth = get_thread_stack_depth(vm_thread);
        *count_ptr = depth;
        errStack = JVMTI_ERROR_NONE;
    }

    if (thread_suspended)
        jthread_resume(thread);

    return errStack;
}

/*
 * Pop Frame
 *
 * Pop the topmost stack frame of thread's stack. Popping a frame
 * takes you to the preceding frame.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiPopFrame(jvmtiEnv* env,
              jthread thread)
{
    TRACE2("jvmti.stack", "PopFrame called");
    SuspendEnabledChecker sec;
    jint state;
    jvmtiError err;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();
    CHECK_CAPABILITY(can_pop_frame);

    if (NULL == thread) {
        return JVMTI_ERROR_INVALID_THREAD;
    }

    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    jthread curr_thread = getCurrentThread();
    if (jni_env->IsSameObject(thread,curr_thread) ) {
        // cannot pop frame yourself
        return JVMTI_ERROR_THREAD_NOT_SUSPENDED;
    }

    // get thread state
    err = env->GetThreadState(thread, &state);
    if (err != JVMTI_ERROR_NONE) {
        return err;
    }

    // check thread state
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0) {
        return JVMTI_ERROR_THREAD_NOT_ALIVE;
    }
    if( (state & JVMTI_THREAD_STATE_SUSPENDED) == 0) {
        return JVMTI_ERROR_THREAD_NOT_SUSPENDED;
    }

    // check stack depth
    hythread_t hy_thread = jthread_get_native_thread(thread);
    VM_thread* vm_thread = get_vm_thread(hy_thread);

    jint depth;
    if (interpreter_enabled()) {
        err = interpreter.interpreter_ti_get_frame_count(env,
            vm_thread, &depth);
    } else {
        depth = get_thread_stack_depth(vm_thread);
        err = JVMTI_ERROR_NONE;
    }
    if (err != JVMTI_ERROR_NONE) {
        return err;
    }

    if(depth <= 1) {
        return JVMTI_ERROR_NO_MORE_FRAMES;
    }

    if (interpreter_enabled()) {
        return interpreter.interpreter_ti_pop_frame(env, vm_thread);
    } else {
        return jvmti_jit_pop_frame(thread);
    }
}

/*
 * Get Frame Location
 *
 * For a Java programming language frame, return the location of
 * the instruction currently executing.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetFrameLocation(jvmtiEnv* env,
                      jthread thread,
                      jint depth,
                      jmethodID* method_ptr,
                      jlocation* location_ptr)
{
    TRACE2("jvmti.stack", "GetFrameLocation called");
    SuspendEnabledChecker sec;
    jint state;
    jvmtiError err;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (thread == 0) {
        thread = getCurrentThread();
    }

    // check error condition: JVMTI_ERROR_INVALID_THREAD
    err = env->GetThreadState(thread, &state);

    if (err != JVMTI_ERROR_NONE) {
        return err;
    }

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0) {
        return JVMTI_ERROR_THREAD_NOT_ALIVE;
    }

    // check error condition: JVMTI_ERROR_NULL_POINTER
    if (location_ptr == NULL) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    // check error condition: JVMTI_ERROR_NULL_POINTER
    if (method_ptr == NULL) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    // check error condition: JVMTI_ERROR_ILLEGAL_ARGUMENT
    if (depth < 0) {
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }

    bool thread_suspended = false;
    // Suspend thread before getting stacks
    VM_thread *vm_thread;
    if (NULL != thread)
    {
        vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);
        // Check that this thread is not current
        if (vm_thread != p_TLS_vmthread)
        {
            jthread_suspend(thread);
            thread_suspended = true;
        }
    }
    else
        vm_thread = p_TLS_vmthread;

    jvmtiError errStack;
    if (interpreter_enabled())
        // check error condition: JVMTI_ERROR_ILLEGAL_ARGUMENT
        // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
        errStack = interpreter.interpreter_ti_getFrameLocation(env,
            get_vm_thread_ptr_safe(jvmti_test_jenv, thread),
            depth, method_ptr, location_ptr);
    else
    {
        StackIterator* si = si_create_from_native(vm_thread);

        jint stack_depth = 0;
        bool frame_found = false;
        while (!si_is_past_end(si))
        {
            jint inlined_depth = si_get_inline_depth(si);
            if (stack_depth + inlined_depth >= depth)
            {
                Method *method = si_get_method(si);
                if (NULL != method)
                {
                    CodeChunkInfo *cci = si_get_code_chunk_info(si);
                    if (NULL != cci)
                    {
                        NativeCodePtr ip = si_get_ip(si);
                        // FIXME64: no support for methods
                        // with compiled code greated than 4GB
                        uint32 offset = (uint32)((POINTER_SIZE_INT)ip -
                            (POINTER_SIZE_INT)cci->get_code_block_addr());
                        JIT *jit = cci->get_jit();

                        uint16 bc;
                        if (stack_depth + inlined_depth > depth)
                        {
                            // Target frame is inlined
                            jint inline_depth_method = inlined_depth - depth;
                            Method *inlined_method = jit->get_inlined_method(
                                cci->get_inline_info(), offset,
                                inline_depth_method);

                                OpenExeJpdaError UNREF result =
                                    jit->get_bc_location_for_native(
                                        inlined_method, ip, &bc);
                                assert(result == EXE_ERROR_NONE);
                                *location_ptr = bc;
                                *method_ptr = reinterpret_cast<jmethodID>(method);
                                frame_found = true;
                                break;
                        }
                        else
                        {
                            // Target frame is not inlined
                            OpenExeJpdaError UNREF result =
                                jit->get_bc_location_for_native(
                                    method, ip, &bc);
                            assert(result == EXE_ERROR_NONE);
                            *location_ptr = bc;
                            *method_ptr = reinterpret_cast<jmethodID>(method);
                            frame_found = true;
                            break;
                        }
                        stack_depth += inlined_depth;
                    }
                    else
                    {
                        assert(si_is_native(si));
                        *location_ptr = -1;
                        *method_ptr = reinterpret_cast<jmethodID>(method);
                        frame_found = true;
                        break;
                    }

                    stack_depth++;
                }
            }

            si_goto_previous(si);
        }

        if (!frame_found)
            errStack = JVMTI_ERROR_NO_MORE_FRAMES;
        else
            errStack = JVMTI_ERROR_NONE;

        si_free(si);
    }

    if (thread_suspended)
        jthread_resume(thread);

    return errStack;
}

/*
 * Notify Frame Pop
 *
 * When the frame that is currently at depth is popped from the
 * stack, generate a FramePop event. See the FramePop event for
 * details. Only frames corresponding to non-native Java
 * programming language methods can receive notification.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiNotifyFramePop(jvmtiEnv* env,
                    jthread thread,
                    jint depth)
{
    TRACE2("jvmti.stack", "NotifyFramePop called: thread: "
        << thread << ", depth: " << depth);
    SuspendEnabledChecker sec;
    jint state;
    jthread curr_thread = getCurrentThread();
    jvmtiError err;

    // Check given env & current phase.
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};
    CHECK_EVERYTHING();
    CHECK_CAPABILITY(can_generate_frame_pop_events);

    if (NULL == thread)
        thread = curr_thread;

    // check error condition: JVMTI_ERROR_INVALID_THREAD
    err = env->GetThreadState(thread, &state);

    if (err != JVMTI_ERROR_NONE) {
        return err;
    }

    // check error condition: JVMTI_ERROR_THREAD_NOT_ALIVE
    if ((state & JVMTI_THREAD_STATE_ALIVE) == 0) {
        return JVMTI_ERROR_THREAD_NOT_ALIVE;
    }

    // check error condition: JVMTI_ERROR_THREAD_NOT_SUSPENDED
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;
    if (!jni_env->IsSameObject(thread,curr_thread)
            && ((state & JVMTI_THREAD_STATE_SUSPENDED) == 0)) {
        return JVMTI_ERROR_THREAD_NOT_SUSPENDED;
    }

    // check error condition: JVMTI_ERROR_ILLEGAL_ARGUMENT
    if (depth < 0) {
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }

    VM_thread *vm_thread;
    if (NULL != thread)
        vm_thread = get_vm_thread_ptr_safe(jvmti_test_jenv, thread);
    else
        vm_thread = p_TLS_vmthread;

    if (interpreter_enabled())
        // check error condition: JVMTI_ERROR_OPAQUE_FRAME
        // check error condition: JVMTI_ERROR_NO_MORE_FRAMES
        return interpreter.interpreter_ti_notify_frame_pop(env,
            vm_thread, depth);
    else
    {
        StackIterator* si = si_create_from_native(vm_thread);
        if (!si_get_method(si)) {
            // skip VM native
            si_goto_previous(si);
        }
        jint current_depth = 0;
        while (!si_is_past_end(si) && current_depth < depth)
        {
            if (si_get_method(si))
                current_depth += 1 + si_get_inline_depth(si);

            si_goto_previous(si);
        }
        if ((current_depth == depth && si_is_native(si)) || // native method
            current_depth > depth) // Inlined frame
        {
            si_free(si);
            return JVMTI_ERROR_OPAQUE_FRAME;
        }
        Method* UNREF method = si_get_method(si);
        si_free(si);

        jint UNREF skip;
        current_depth = get_thread_stack_depth(vm_thread, &skip);
        if (current_depth < depth)
            return JVMTI_ERROR_NO_MORE_FRAMES;

        jvmti_frame_pop_listener *new_listener =
            (jvmti_frame_pop_listener *)STD_MALLOC(
                sizeof(jvmti_frame_pop_listener));
        assert(new_listener);

        new_listener->depth = current_depth - depth;
        new_listener->env = reinterpret_cast<TIEnv *>(env);
        new_listener->next = vm_thread->frame_pop_listener;
        vm_thread->frame_pop_listener = new_listener;
        TRACE2("jvmti.stack", "Pop listener is created: thread: "
            << vm_thread << ", listener: " << new_listener
            << ", env: " << new_listener->env << ", depth: " << new_listener->depth
            << " -> " << class_get_name(method_get_class(method)) << "."
            << method_get_name(method) << method_get_descriptor(method));

        return JVMTI_ERROR_NONE;
    }
}
