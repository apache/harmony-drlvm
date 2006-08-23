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


#include "open/jthread.h"
#include "open/hythread_ext.h"

// Called when current thread reached breakpoint.
// Returns breakpoint ID received from executing engine on creating of the
// breakpoint
VMEXPORT void*
jvmti_process_breakpoint_event(jmethodID method, jlocation location)
{
    TRACE2("jvmti-break", "BREAKPOINT occured, location = " << location);
    NativeObjectHandles handles;
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_get_java_thread(hythread_self())->object;
    tmn_suspend_enable();

    // FIXME: lock breakpoint table

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    BreakPoint *bp = ti->find_breakpoint(method, location);
    assert(bp); // Can't be bytecode without breakpoint
    assert(bp->envs); // Can't be breakpoint with no environments
    void *id = bp->id;

    for (TIEnvList *el = bp->envs; NULL != el;)
    {
        TIEnv *env = el->env;
        el = el->next;
        if (env->global_events[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventBreakpoint func = (jvmtiEventBreakpoint)env->get_event_callback(JVMTI_EVENT_BREAKPOINT);
            if (NULL != func)
            {
                JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
                TRACE2("jvmti-break", "Calling global breakpoint callback method = " <<
                    ((Method*)method)->get_name() << " location = " << location);
                func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                TRACE2("jvmti-break", "Finished global breakpoint callback method = " <<
                    ((Method*)method)->get_name() << " location = " << location);
            }
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
                    TRACE2("jvmti-break", "Calling local breakpoint callback method = " <<
                        ((Method*)method)->get_name() << " location = " << location);
                    func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                    TRACE2("jvmti-break", "Finished local breakpoint callback method = " <<
                        ((Method*)method)->get_name() << " location = " << location);
                }
            }
        }
    }

    tmn_suspend_disable();
    oh_discard_local_handle(hThread);
    return id;
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
    TRACE2("jvmti-break", "SetBreakpoint method = " << m->get_class()->name->bytes << "." <<
        m->get_name()->bytes << " " << m->get_descriptor()->bytes);

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

    if (location < 0 || unsigned(location) >= m->get_byte_code_size()) {
        return JVMTI_ERROR_INVALID_LOCATION;
    }

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
    BreakPoint *bp = ti->find_breakpoint(method, location);

    // Find breakpoint for this location if it exists already
    if (NULL == bp)
    {
        errorCode = _allocate(sizeof(BreakPoint), (unsigned char **)&bp);
        if (JVMTI_ERROR_NONE != errorCode)
            return errorCode;

        TIEnvList *el;
        errorCode = _allocate(sizeof(TIEnvList), (unsigned char **)&el);
        if (JVMTI_ERROR_NONE != errorCode)
        {
            _deallocate((unsigned char *)bp);
            return errorCode;
        }
        bp->method = method;
        bp->location = location;
        bp->next = 0;
        bp->envs = 0;

        el->env = p_env;
        bp->add_env(el);
        ti->add_breakpoint(bp);

        bp->id = interpreter.interpreter_ti_set_breakpoint(method, location);
    }
    else
    {
        if (NULL != bp->find_env(p_env))
            return JVMTI_ERROR_DUPLICATE;

        TIEnvList *el;
        errorCode = _allocate(sizeof(TIEnvList), (unsigned char **)&el);
        if (JVMTI_ERROR_NONE != errorCode)
            return errorCode;

        el->env = p_env;
        bp->add_env(el);
    }

    TRACE2("jvmti-break", "SetBreakpoint successfull");
    return JVMTI_ERROR_NONE;
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
    TRACE2("jvmti-break", "ClearBreakpoint method = " << m->get_class()->name->bytes << "." <<
        m->get_name()->bytes << " " << m->get_descriptor()->bytes);

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

    if (location < 0 || unsigned(location) >= m->get_byte_code_size()) {
        return JVMTI_ERROR_INVALID_LOCATION;
    }

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
    BreakPoint *bp = ti->find_breakpoint(method, location);

    if (NULL == bp)
        return JVMTI_ERROR_NOT_FOUND;

    TIEnvList *el = bp->find_env(p_env);
    if (NULL == el)
        return JVMTI_ERROR_NOT_FOUND;

    if (NULL == bp->envs->next) // No more environments set breakpoints here
    {
        interpreter.interpreter_ti_clear_breakpoint(method, location, bp->id);
    }

    bp->remove_env(el);
    if (NULL == bp->envs) 
        ti->remove_breakpoint(bp);

    return JVMTI_ERROR_NONE;
}
