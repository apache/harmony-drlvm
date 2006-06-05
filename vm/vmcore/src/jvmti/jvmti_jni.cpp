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
 * JVMTI JNI API
 */

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "open/vm_util.h"
#include "cxxlog.h"
#include "suspend_checker.h"

/*
 * Set JNI Function Table
 *
 * Set the JNI function table in all current and future JNI
 * environments. As a result, all future JNI calls are directed
 * to the specified functions.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiSetJNIFunctionTable(jvmtiEnv* env,
                         const jniNativeInterface* function_table)
{
    TRACE2("jvmti.jni", "SetJNIFunctionTable called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == function_table) 
        return JVMTI_ERROR_NULL_POINTER;

    memcpy((void *)&jni_vtable, function_table, sizeof(jniNativeInterface));

    return JVMTI_ERROR_NONE;
}

/*
 * Get JNI Function Table
 *
 * Get the JNI function table. The JNI function table is copied
 * into allocated memory.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetJNIFunctionTable(jvmtiEnv* env,
                         jniNativeInterface** function_table)
{
    TRACE2("jvmti.jni", "GetJNIFunctionTable called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_START, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == function_table) 
        return JVMTI_ERROR_NULL_POINTER;

    jniNativeInterface *table;
    jvmtiError errorCode = _allocate(sizeof(jniNativeInterface), (unsigned char **)&table);

    if (JVMTI_ERROR_NONE != errorCode)
        return errorCode;

    memcpy(table, &jni_vtable, sizeof(jniNativeInterface));

    *function_table = table;

    return JVMTI_ERROR_NONE;
}
