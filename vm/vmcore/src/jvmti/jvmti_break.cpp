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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.4 $
 */  
/*
 * JVMTI API for working with breakpoints
 */

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "jvmti_internal.h"
#include "environment.h"
#include "Class.h"
#include "cxxlog.h"

#include "interpreter_exports.h"
#include "interpreter_imports.h"
#include "suspend_checker.h"
#include "jit_intf_cpp.h"
#include "encoder.h"
#include "m2n.h"


#define INSTRUMENTATION_BYTE_HLT 0xf4 // HLT instruction
#define INSTRUMENTATION_BYTE_CLI 0xfa // CLI instruction
#define INSTRUMENTATION_BYTE_INT3 0xcc // INT 3 instruction
#define INSTRUMENTATION_BYTE INSTRUMENTATION_BYTE_INT3


#include "open/jthread.h"
#include "open/hythread_ext.h"

// Called when current thread reached breakpoint.
// Returns breakpoint ID received from executing engine on creating of the
// breakpoint
VMEXPORT void*
jvmti_process_interpreter_breakpoint_event(jmethodID method, jlocation location)
{
    TRACE2("jvmti.break", "BREAKPOINT occured, location = " << location);
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_get_java_thread(hythread_self())->object;
    tmn_suspend_enable();

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;

    ti->brkpntlst_lock._lock();

    BreakPoint *bp = ti->find_first_bpt(method, location);
    assert(bp); // Can't be bytecode without breakpoint
    assert(bp->env); // Can't be breakpoint with no environments
    void *id = bp->id;

    do
    {
        TIEnv *env = bp->env;
        BreakPoint *next_bp = ti->find_next_bpt(bp, method, location);

        if (env->global_events[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventBreakpoint func = (jvmtiEventBreakpoint)env->get_event_callback(JVMTI_EVENT_BREAKPOINT);
            if (NULL != func)
            {
                JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
                TRACE2("jvmti.break", "Calling interpreter global breakpoint callback method = " <<
                    ((Method*)method)->get_name() << " location = " << location);

                ti->brkpntlst_lock._unlock();
                func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                ti->brkpntlst_lock._lock();

                TRACE2("jvmti.break", "Finished interpreter global breakpoint callback method = " <<
                    ((Method*)method)->get_name() << " location = " << location);
            }
            bp = next_bp;
            continue; // Don't send local events
        }

        TIEventThread *next_et;
        for (TIEventThread *et = env->event_threads[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL];
            NULL != et; et = next_et)
        {
            next_et = et->next;
            if (et->thread == hythread_self())
            {
                jvmtiEventBreakpoint func = (jvmtiEventBreakpoint)env->get_event_callback(JVMTI_EVENT_BREAKPOINT);
                if (NULL != func)
                {
                    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
                    TRACE2("jvmti.break", "Calling interpreter local breakpoint callback method = " <<
                        ((Method*)method)->get_name() << " location = " << location);

                    ti->brkpntlst_lock._unlock();
                    func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                    ti->brkpntlst_lock._lock();

                    TRACE2("jvmti.break", "Finished interpreter local breakpoint callback method = " <<
                        ((Method*)method)->get_name() << " location = " << location);
                }
            }

            et = next_et;
        }

        bp = next_bp;
    } while(bp);

    ti->brkpntlst_lock._unlock();

    tmn_suspend_disable();
    oh_discard_local_handle(hThread);
    return id;
}

ConditionCode get_condition_code(InstructionDisassembler::CondJumpType jump_type)
{
    // Right now InstructionDisassembler::CondJumpType enum values are
    // equal to enums in ia32/em64t encoder, so this statement is ok
    return (ConditionCode)jump_type;
}

bool jvmti_send_jit_breakpoint_event(Registers *regs)
{
#if PLATFORM_POSIX && INSTRUMENTATION_BYTE == INSTRUMENTATION_BYTE_INT3
    // Int3 exception address points to the instruction after it
    NativeCodePtr native_location = (NativeCodePtr)(((POINTER_SIZE_INT)regs->get_ip()) - 1);
#else
    NativeCodePtr native_location = (NativeCodePtr)regs->get_ip();
#endif

    TRACE2("jvmti.break", "BREAKPOINT occured, location = " << native_location);

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() || ti->getPhase() != JVMTI_PHASE_LIVE)
        return false;

    ti->brkpntlst_lock._lock();
    BreakPoint *bp = ti->find_first_bpt(native_location);
    if (NULL == bp)
    {
        ti->brkpntlst_lock._unlock();
        return false;
    }

    assert(ti->isEnabled());
    assert(!interpreter_enabled());

    M2nFrame *m2nf = m2n_push_suspended_frame(regs);

    hythread_t h_thread = hythread_self();
    jthread j_thread = jthread_get_java_thread(h_thread);
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)j_thread->object;
    tmn_suspend_enable();

    void *id = bp->id;
    jbyte original_byte = (POINTER_SIZE_INT)id;
    // Copy disassembler instance in case a breakpoint is deleted
    // inside of callbacks
    InstructionDisassembler idisasm(*bp->disasm);
    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;

    BreakPoint *ss_breakpoint = NULL;
    // Check if there are Single Step type breakpoints in TLS
    if (ti->is_single_step_enabled())
    {
        VM_thread *vm_thread = p_TLS_vmthread;
        if (NULL != vm_thread->ss_state)
        {
            for(unsigned iii = 0; NULL != vm_thread->ss_state &&
                    iii < vm_thread->ss_state->predicted_bp_count; iii++)
            {
                if (vm_thread->ss_state->predicted_breakpoints[iii]->native_location ==
                    native_location)
                {
                    ss_breakpoint =
                        vm_thread->ss_state->predicted_breakpoints[iii];

                    TIEnv *ti_env = ti->getEnvironments();
                    TIEnv *next_env;
                    jlocation location = ss_breakpoint->location;
                    jmethodID method = ss_breakpoint->method;

                    while (NULL != ti_env)
                    {
                        next_env = ti_env->next;
                        jvmtiEventSingleStep func =
                            (jvmtiEventSingleStep)ti_env->get_event_callback(JVMTI_EVENT_SINGLE_STEP);
                        if (NULL != func)
                        {
                            if (ti_env->global_events[JVMTI_EVENT_SINGLE_STEP - JVMTI_MIN_EVENT_TYPE_VAL])
                            {
                                TRACE2("jvmti.break.ss",
                                    "Calling JIT global SingleStep breakpoint callback method = " <<
                                    ((Method*)method)->get_name() << " location = " << location);
                                // fire global event
                                ti->brkpntlst_lock._unlock();
                                func((jvmtiEnv*)ti_env, jni_env, (jthread)hThread, method, location);
                                ti->brkpntlst_lock._lock();
                                TRACE2("jvmti.break.ss",
                                    "Finished JIT global SingleStep breakpoint callback method = " <<
                                    ((Method*)method)->get_name() << " location = " << location);
                                ti_env = next_env;
                                continue;
                            }

                            // fire local events
                            for(TIEventThread* ti_et = ti_env->event_threads[JVMTI_EVENT_SINGLE_STEP - JVMTI_MIN_EVENT_TYPE_VAL];
                                ti_et != NULL; ti_et = ti_et->next)
                                if (ti_et->thread == hythread_self())
                                {
                                    TRACE2("jvmti.break.ss",
                                        "Calling JIT local SingleStep breakpoint callback method = " <<
                                    ((Method*)method)->get_name() << " location = " << location);
                                    ti->brkpntlst_lock._unlock();
                                    func((jvmtiEnv*)ti_env, jni_env,
                                        (jthread)hThread, method, location);
                                    ti->brkpntlst_lock._lock();
                                    TRACE2("jvmti.break.ss",
                                        "Finished JIT local SingleStep breakpoint callback method = " <<
                                    ((Method*)method)->get_name() << " location = " << location);
                                }
                        }
                        ti_env = next_env;
                    }
                }
            }
            // Reinitialize breakpoint after SingleStep because this
            // breakpoint could have been deleted inside of callback
            // if agent terminated single step
            bp = ti->find_first_bpt(native_location);
        }
    }


    // Send events for all normally set breakpoints for this location
    // if there are any
    while (bp)
    {
        TIEnv *env = bp->env;
        jlocation location = bp->location;
        jmethodID method = bp->method;
        BreakPoint *next_bp = ti->find_next_bpt(bp, native_location);

        if (bp == ss_breakpoint)
        {
            // Don't send breakpoint event for breakpoint which was
            // actually SingleStep breakpoint
            bp = next_bp;
            continue;
        }

        if (env->global_events[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventBreakpoint func = (jvmtiEventBreakpoint)env->get_event_callback(JVMTI_EVENT_BREAKPOINT);
            if (NULL != func)
            {
                TRACE2("jvmti.break", "Calling JIT global breakpoint callback method = " <<
                    ((Method*)method)->get_name() << " location = " << location);

                ti->brkpntlst_lock._unlock();
                func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                ti->brkpntlst_lock._lock();

                TRACE2("jvmti.break", "Finished JIT global breakpoint callback method = " <<
                    ((Method*)method)->get_name() << " location = " << location);
            }
            bp = next_bp;
            continue; // Don't send local events
        }

        TIEventThread *next_et;
        for (TIEventThread *et = env->event_threads[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL];
            NULL != et; et = next_et)
        {
            next_et = et->next;
            if (et->thread == hythread_self())
            {
                jvmtiEventBreakpoint func = (jvmtiEventBreakpoint)env->get_event_callback(JVMTI_EVENT_BREAKPOINT);
                if (NULL != func)
                {
                    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
                    TRACE2("jvmti.break", "Calling JIT local breakpoint callback method = " <<
                        ((Method*)method)->get_name() << " location = " << location);

                    ti->brkpntlst_lock._unlock();
                    func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                    ti->brkpntlst_lock._lock();

                    TRACE2("jvmti.break", "Finished JIT local breakpoint callback method = " <<
                        ((Method*)method)->get_name() << " location = " << location);
                }
            }

            et = next_et;
        }

        bp = next_bp;
    }

    // Now we need to return back to normal code execution, it is
    // necessary to execute the original instruction The idea is to
    // recreate the original instriction in a special thread local
    // buffer followed by a jump to an instruction next after it. In
    // case the instruction was a relative jump or call it requires
    // special handling.
    InstructionDisassembler::Type type = idisasm.get_type();

    VM_thread *vm_thread = p_TLS_vmthread;
    jbyte *instruction_buffer = vm_thread->jvmti_jit_breakpoints_handling_buffer;
    jbyte *interrupted_instruction = (jbyte *)native_location;
    jint instruction_length = idisasm.get_length_with_prefix();

    switch(type)
    {
    case InstructionDisassembler::UNKNOWN:
    {
        char *next_instruction = (char *)interrupted_instruction +
            instruction_length;

        // Copy original instruction to the exeuction buffer
        *instruction_buffer = original_byte;
        memcpy(instruction_buffer + 1, interrupted_instruction + 1,
            instruction_length - 1);

        // Create JMP $next_instruction instruction in the execution buffer
        jump((char *)instruction_buffer + instruction_length,
            next_instruction);
        break;
    }
    case InstructionDisassembler::RELATIVE_JUMP:
    {
        jint instruction_length = idisasm.get_length_with_prefix();
        char *jump_target = (char *)idisasm.get_jump_target_address();

        // Create JMP to the absolute address which conditional jump
        // had in the execution buffer
        jump((char *)instruction_buffer, jump_target);
        break;
    }
    case InstructionDisassembler::RELATIVE_COND_JUMP:
    {
        char *code = (char *)instruction_buffer;
        InstructionDisassembler::CondJumpType jump_type =
            idisasm.get_cond_jump_type();
        char *next_instruction = (char *)interrupted_instruction +
            instruction_length;
        char *jump_target = (char *)idisasm.get_jump_target_address();

        // Create a conditional JMP of the same type over 1
        // instruction forward, the next instruction is JMP to the
        // $next_instruction
        code = branch8(code, get_condition_code(jump_type), Imm_Opnd(size_8, 0));
        char *branch_address = code - 1;

        code = jump(code, next_instruction);
        jint offset = code - branch_address - 1;
        *branch_address = offset;

        jump(code, jump_target);
        break;
    }
    case InstructionDisassembler::ABSOLUTE_CALL:
    case InstructionDisassembler::RELATIVE_CALL:
    {
        jbyte *next_instruction = interrupted_instruction + instruction_length;
        char *jump_target = (char *)idisasm.get_jump_target_address();
        char *code = (char *)instruction_buffer;

        // Push "return address" to the $next_instruction
        code = push(code, Imm_Opnd((POINTER_SIZE_INT)next_instruction));

        // Jump to the target address of the call instruction
        jump(code, jump_target);
        break;
    }
    }

    // Set exception or signal return address to the instruction buffer
#ifndef _EM64T_
    regs->eip = (POINTER_SIZE_INT)instruction_buffer;
#else
    regs->rip = (POINTER_SIZE_INT)instruction_buffer;
#endif

    // Set breakpoints on bytecodes after the current one
    if (ti->is_single_step_enabled())
    {
        VM_thread *vm_thread = p_TLS_vmthread;
        if (NULL != vm_thread->ss_state)
        {
            jvmti_StepLocation *locations;
            unsigned locations_count;

            jvmti_SingleStepLocation(vm_thread, (Method *)ss_breakpoint->method,
                (unsigned)ss_breakpoint->location, &locations, &locations_count);

            jvmti_remove_single_step_breakpoints(ti, vm_thread);

            jvmtiError UNREF errorCode = jvmti_set_single_step_breakpoints(
                ti, vm_thread, locations, locations_count);
            assert(JVMTI_ERROR_NONE == errorCode);
        }
    }

    ti->brkpntlst_lock._unlock();

    tmn_suspend_disable();
    oh_discard_local_handle(hThread);

    m2n_set_last_frame(m2n_get_previous_frame(m2nf));
    STD_FREE(m2nf);

    return true;
}

jvmtiError jvmti_set_jit_mode_breakpoint(BreakPoint *bp)
{
    // Function is always executed under global TI breakpoints lock

    // Find native location in the method code
    NativeCodePtr np = NULL;
    Method *m = (Method *)bp->method;

    for (CodeChunkInfo* cci = m->get_first_JIT_specific_info(); cci; cci = cci->_next)
    {
        JIT *jit = cci->get_jit();
        OpenExeJpdaError res = jit->get_native_location_for_bc(m,
            (uint16)bp->location, &np);
        if (res == EXE_ERROR_NONE)
            break;
    }

    if (NULL == np)
        return JVMTI_ERROR_INTERNAL;

    TRACE2("jvmti.break", "SetBreakpoint instrumenting native location " << np);

    bp->native_location = np;
    bp->disasm = new InstructionDisassembler(np);

    jbyte *target_instruction = (jbyte *)np;
    bp->id = (void *)(POINTER_SIZE_INT)*target_instruction;
    *target_instruction = (jbyte)INSTRUMENTATION_BYTE;

    return JVMTI_ERROR_NONE;
}

void jvmti_set_pending_breakpoints(Method *method)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    assert(method->get_pending_breakpoints() > 0);

    LMAutoUnlock lock(&ti->brkpntlst_lock);

    jmethodID mid = (jmethodID)method;
    BreakPoint *bp = ti->find_first_bpt(mid);
    assert(bp);

    jlocation *locations = (jlocation *)STD_MALLOC(sizeof(jlocation) *
        method->get_pending_breakpoints());
    assert(locations);
    uint32 location_count = 0;

    do
    {
        // It is necessary to set breakpoints only once for each
        // location, so we need to filter out duplicate breakpoints
        for (uint32 iii = 0; iii < location_count; iii++)
            if (bp->location == locations[iii])
                continue;

        jvmti_set_jit_mode_breakpoint(bp);
        locations[location_count++] = bp->location;

        method->remove_pending_breakpoint();
        bp = ti->find_next_bpt(bp, mid);
    }
    while(NULL != bp);

    assert(method->get_pending_breakpoints() == 0);
    STD_FREE(locations);
}

jvmtiError jvmti_set_breakpoint_for_jit(DebugUtilsTI *ti, BreakPoint *bp)
{
    // Function is always executed under global TI breakpoints lock

    BreakPoint *other_bp = ti->get_other_breakpoint_same_location(bp->method,
        bp->location);

    if (NULL == other_bp) // No other breakpoints were set in this place
    {
        Method *m = (Method *)bp->method;

        if (m->get_state() == Method::ST_Compiled)
        {
            jvmtiError errorCode = jvmti_set_jit_mode_breakpoint(bp);

            if (JVMTI_ERROR_NONE != errorCode)
                return JVMTI_ERROR_INTERNAL;
        }
        else
        {
            TRACE2("jvmti.break", "Skipping setting breakpoing in method " <<
                m->get_class()->name->bytes << "." <<
                m->get_name()->bytes << " " << m->get_descriptor()->bytes <<
                " because it is not compiled yet");
            m->insert_pending_breakpoint();
        }
    }
    else
    {
        bp->id = other_bp->id;
        if (NULL != bp->disasm)
            bp->disasm = new InstructionDisassembler(*bp->disasm);
    }

    ti->add_breakpoint(bp);
    return JVMTI_ERROR_NONE;
}

/*
* Set Breakpoint
*
* Set a breakpoint at the instruction indicated by method and
* location. An instruction can only have one breakpoint.
*
* OPTIONAL Functionality
*/
jvmtiError JNICALL
jvmtiSetBreakpoint(jvmtiEnv* env,
                   jmethodID method,
                   jlocation location)
{
    TRACE2("jvmti.break", "SetBreakpoint called, method = " << method << " , location = " << location);
    SuspendEnabledChecker sec;

    jvmtiError errorCode;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (method == NULL)
        return JVMTI_ERROR_INVALID_METHODID;

    Method *m = (Method*) method;
    TRACE2("jvmti.break", "SetBreakpoint method = " << m->get_class()->name->bytes << "." <<
        m->get_name()->bytes << " " << m->get_descriptor()->bytes);

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

    if (location < 0 || unsigned(location) >= m->get_byte_code_size())
        return JVMTI_ERROR_INVALID_LOCATION;

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif
    /*
    * JVMTI_ERROR_MUST_POSSESS_CAPABILITY
    */
    jvmtiCapabilities capptr;
    errorCode = jvmtiGetCapabilities(env, &capptr);

    if (errorCode != JVMTI_ERROR_NONE)
        return errorCode;

    if (capptr.can_generate_breakpoint_events == 0)
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;

    TIEnv *p_env = (TIEnv *)env;
    DebugUtilsTI *ti = p_env->vm->vm_env->TI;
    LMAutoUnlock lock(&ti->brkpntlst_lock);
    BreakPoint *bp = ti->find_breakpoint(method, location, p_env);

    if (NULL != bp)
        return JVMTI_ERROR_DUPLICATE;

    errorCode = _allocate(sizeof(BreakPoint), (unsigned char **)&bp);
    if (JVMTI_ERROR_NONE != errorCode)
        return errorCode;

    bp->method = method;
    bp->location = location;
    bp->env = p_env;
    bp->disasm = NULL;

    if (interpreter_enabled())
    {
        BreakPoint *other_bp = ti->get_other_breakpoint_same_location(method, location);

        if (NULL == other_bp) // No other breakpoints were set in this place
            bp->id = interpreter.interpreter_ti_set_breakpoint(method, location);

        ti->add_breakpoint(bp);
    }
    else
    {
        errorCode = jvmti_set_breakpoint_for_jit(ti, bp);

        if (JVMTI_ERROR_NONE != errorCode)
            return JVMTI_ERROR_INTERNAL;
    }

    TRACE2("jvmti.break", "SetBreakpoint successfull");
    return JVMTI_ERROR_NONE;
}

void jvmti_remove_breakpoint_for_jit(DebugUtilsTI *ti, BreakPoint *bp)
{
    // Function is always executed under global TI breakpoints lock

    if (NULL == ti->get_other_breakpoint_same_location(bp->method, bp->location))
    {
        Method *m = (Method *)bp->method;

        if (m->get_state() == Method::ST_Compiled)
        {
            jbyte *target_instruction = (jbyte *)bp->native_location;
            *target_instruction = (POINTER_SIZE_INT)bp->id;
        }
        else
            m->remove_pending_breakpoint();
    }

    ti->remove_breakpoint(bp);
}

/*
* Clear Breakpoint
*
* Clear the breakpoint at the bytecode indicated by method and
* location.
*
* OPTIONAL Functionality
*/
jvmtiError JNICALL
jvmtiClearBreakpoint(jvmtiEnv* env,
                     jmethodID method,
                     jlocation location)
{
    TRACE2("jvmti.break", "ClearBreakpoint called, method = " << method << " , location = " << location);
    SuspendEnabledChecker sec;
    jvmtiError errorCode;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (method == NULL)
        return JVMTI_ERROR_INVALID_METHODID;

    Method *m = (Method*) method;
    TRACE2("jvmti.break", "ClearBreakpoint method = " << m->get_class()->name->bytes << "." <<
        m->get_name()->bytes << " " << m->get_descriptor()->bytes);

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

    if (location < 0 || unsigned(location) >= m->get_byte_code_size())
        return JVMTI_ERROR_INVALID_LOCATION;

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

    /*
    * JVMTI_ERROR_MUST_POSSESS_CAPABILITY
    */
    jvmtiCapabilities capptr;
    errorCode = jvmtiGetCapabilities(env, &capptr);

    if (errorCode != JVMTI_ERROR_NONE)
        return errorCode;

    if (capptr.can_generate_breakpoint_events == 0)
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;

    TIEnv *p_env = (TIEnv *)env;
    DebugUtilsTI *ti = p_env->vm->vm_env->TI;
    LMAutoUnlock lock(&ti->brkpntlst_lock);
    BreakPoint *bp = ti->find_breakpoint(method, location, p_env);

    if (NULL == bp)
        return JVMTI_ERROR_NOT_FOUND;

    if (interpreter_enabled())
    {
        if (NULL == ti->get_other_breakpoint_same_location(method, location))
            // No other breakpoints were set in this place
            interpreter.interpreter_ti_clear_breakpoint(method, location, bp->id);

        ti->remove_breakpoint(bp);
    }
    else
        jvmti_remove_breakpoint_for_jit(ti, bp);

    TRACE2("jvmti.break", "ClearBreakpoint successfull");
    return JVMTI_ERROR_NONE;
}
