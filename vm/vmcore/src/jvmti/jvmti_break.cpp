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

#include "suspend_checker.h"
#include "interpreter_exports.h"
#include "jvmti_break_intf.h"

#include "open/jthread.h"
#include "open/hythread_ext.h"


// Callback function for JVMTI breakpoint processing
bool jvmti_process_breakpoint_event(VmBrkptIntf* intf, VmBrkptRef* bp_ref)
{
    VmBrkpt* bp = bp_ref->brpt;
    assert(bp);

    TRACE2("jvmti.break", "BREAKPOINT occured: location=" << bp->location);

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() || ti->getPhase() != JVMTI_PHASE_LIVE)
        return false;

    jlocation location = bp->location;
    jmethodID method = bp->method;
    TIBrptData* data = (TIBrptData*)bp_ref->data;
    TIEnv *env = data->env;
    
    hythread_t h_thread = hythread_self();
    jthread j_thread = jthread_get_java_thread(h_thread);
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)j_thread->object;
    tmn_suspend_enable();

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;

    jvmtiEventBreakpoint func =
        (jvmtiEventBreakpoint)env->get_event_callback(JVMTI_EVENT_BREAKPOINT);

    if (NULL != func)
    {
        if (env->global_events[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            TRACE2("jvmti.break", "Calling global breakpoint callback: "
                << class_get_name(method_get_class((Method*)method)) << "."
                << method_get_name((Method*)method)
                << method_get_descriptor((Method*)method)
                << " :" << location);

            intf->unlock();
            func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
            intf->lock();

            TRACE2("jvmti.break", "Finished global breakpoint callback: "
                << class_get_name(method_get_class((Method*)method)) << "."
                << method_get_name((Method*)method)
                << method_get_descriptor((Method*)method)
                << " :" << location);
        }
        else
        {
            TIEventThread *next_et;
            TIEventThread *first_et =
                env->event_threads[JVMTI_EVENT_BREAKPOINT - JVMTI_MIN_EVENT_TYPE_VAL];

            for (TIEventThread *et = first_et; NULL != et; et = next_et)
            {
                next_et = et->next;

                if (et->thread == hythread_self())
                {
                    TRACE2("jvmti.break", "Calling local breakpoint callback: "
                        << class_get_name(method_get_class((Method*)method)) << "."
                        << method_get_name((Method*)method)
                        << method_get_descriptor((Method*)method)
                        << " :" << location);

                    intf->unlock();
                    func((jvmtiEnv*)env, jni_env, (jthread)hThread, method, location);
                    intf->lock();

                    TRACE2("jvmti.break", "Finished local breakpoint callback: "
                        << class_get_name(method_get_class((Method*)method)) << "."
                        << method_get_name((Method*)method)
                        << method_get_descriptor((Method*)method)
                        << " :" << location);
                }
            }
        }
    }

    tmn_suspend_disable();
    oh_discard_local_handle(hThread);

    return true;
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
    TRACE2("jvmti.break", "SetBreakpoint: "
        << class_get_name(method_get_class((Method *)method)) << "."
        << method_get_name((Method *)method)
        << method_get_descriptor((Method *)method)
        << " :" << location);

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
    VmBrkptIntf* brpt_intf = p_env->brpt_intf;
    LMAutoUnlock lock(brpt_intf->get_lock());

    VmBrkptRef* bp = brpt_intf->find(method, location);

    if (NULL != bp)
        return JVMTI_ERROR_DUPLICATE;

    TIBrptData* data;
    errorCode = _allocate(sizeof(TIBrptData), (unsigned char**)&data);
    if (JVMTI_ERROR_NONE != errorCode)
        return errorCode;

    data->env = p_env;

    if (!brpt_intf->add(method, location, data))
    {
        _deallocate((unsigned char*)data);
        return JVMTI_ERROR_INTERNAL;
    }

    TRACE2("jvmti.break", "SetBreakpoint successfull");
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
    TRACE2("jvmti.break", "ClearBreakpoint: "
        << class_get_name(method_get_class((Method*)method)) << "."
        << method_get_name((Method*)method)
        << method_get_descriptor((Method*)method)
        << " :" << location);

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
    VmBrkptIntf* brpt_intf = p_env->brpt_intf;
    LMAutoUnlock lock(brpt_intf->get_lock());

    VmBrkptRef* bp = brpt_intf->find(method, location);

    if (NULL == bp)
        return JVMTI_ERROR_NOT_FOUND;

    if (!brpt_intf->remove(bp))
        return JVMTI_ERROR_INTERNAL;

    TRACE2("jvmti.break", "ClearBreakpoint successfull");
    return JVMTI_ERROR_NONE;
}
