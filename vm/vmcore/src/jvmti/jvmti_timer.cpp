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
 * JVMTI timer API
 */

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "time.h"
#include "cxxlog.h"
#include "port_sysinfo.h"
#include "suspend_checker.h"

/*
 * Get Current Thread CPU Timer Information
 *
 * Get information about the GetCurrentThreadCpuTime timer.
 * The fields of the jvmtiTimerInfo structure are filled in
 * with details about the timer. This information is specific
 * to the platform and the implementation of GetCurrentThreadCpuTime
 * and thus does not vary by thread nor does it vary during a
 * particular invocation of the VM.
 *
 * OPTIONAL Functionality.
 */
jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTimerInfo(jvmtiEnv* env,
                                  jvmtiTimerInfo* UNREF info_ptr)
{
    TRACE2("jvmti.timer", "GetCurrentThreadCpuTimerInfo called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Get Current Thread CPU Time
 *
 * Return the CPU time utilized by the current thread.
 *
 * OPTIONAL Functionality.
 */
jvmtiError JNICALL
jvmtiGetCurrentThreadCpuTime(jvmtiEnv* env,
                             jlong* UNREF nanos_ptr)
{
    TRACE2("jvmti.timer", "GetCurrentThreadCpuTime called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Get Thread CPU Timer Information
 *
 * Get information about the GetThreadCpuTime timer. The fields
 * of the jvmtiTimerInfo structure are filled in with details
 * about the timer.
 *
 * OPTIONAL Functionality.
 */
jvmtiError JNICALL
jvmtiGetThreadCpuTimerInfo(jvmtiEnv* env,
                           jvmtiTimerInfo* UNREF info_ptr)
{
    TRACE2("jvmti.timer", "GetThreadCpuTimerInfo called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Get Thread CPU Time
 *
 * Return the CPU time utilized by the specified thread.
 *
 * OPTIONAL Functionality.
 */
jvmtiError JNICALL
jvmtiGetThreadCpuTime(jvmtiEnv* env,
                      jthread UNREF thread,
                      jlong* UNREF nanos_ptr)
{
    TRACE2("jvmti.timer", "GetThreadCpuTime called");
     SuspendEnabledChecker sec;
   /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Get Timer Information
 *
 * Get information about the GetTime timer. The fields of the
 * jvmtiTimerInfo structure are filled in with details about the
 * timer. This information will not change during a particular
 * invocation of the VM.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetTimerInfo(jvmtiEnv* env,
                  jvmtiTimerInfo* UNREF info_ptr)
{
    TRACE2("jvmti.timer", "GetTimerInfo called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase* phases = NULL;

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

/*
 * Get Time
 *
 * Return the current value of the system timer, in nanoseconds.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetTime(jvmtiEnv* env,
             jlong* nanos_ptr)
{
    TRACE2("jvmti.timer", "GetTime called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase* phases = NULL;

    CHECK_EVERYTHING();

    if (nanos_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    time_t value = 0;

    time( &value );

    *nanos_ptr = value * 1000000000UL;

    return JVMTI_ERROR_NONE;
}

/*
 * Get Available Processors
 *
 * Returns the number of processors available to the Java virtual
 * machine.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetAvailableProcessors(jvmtiEnv* env,
                            jint* processor_count_ptr)
{
    TRACE2("jvmti.timer", "GetAvailableProcessors called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase* phases = NULL;

    CHECK_EVERYTHING();

    if (processor_count_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    *processor_count_ptr = port_CPUs_number();

    return JVMTI_ERROR_NONE;
}
