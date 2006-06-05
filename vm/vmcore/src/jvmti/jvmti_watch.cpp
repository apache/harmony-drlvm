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
 * JVMTI watchpoints API
 */

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "cxxlog.h"
#include "suspend_checker.h"

/*
 * Set Field Access Watch
 *
 * Generate a FieldAccess event when the field specified by klass
 * and field is about to be accessed.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiSetFieldAccessWatch(jvmtiEnv* env,
                         jclass UNREF klass,
                         jfieldID UNREF field)
{
    TRACE2("jvmti.watch", "SetFieldAccessWatch called");
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
 * Clear Field Access Watch
 *
 * Cancel a field access watch previously set by SetFieldAccessWatch,
 * on the field specified by klass and field.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiClearFieldAccessWatch(jvmtiEnv* env,
                           jclass UNREF klass,
                           jfieldID UNREF field)
{
    TRACE2("jvmti.watch", "ClearFieldAccessWatch called");
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
 * Set Field Modification Watch
 *
 * Generate a FieldModification event when the field specified
 * by klass and field is about to be modified.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiSetFieldModificationWatch(jvmtiEnv* env,
                               jclass UNREF klass,
                               jfieldID UNREF field)
{
    TRACE2("jvmti.watch", "SetFieldModificationWatch called");
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
 * Clear Field Modification Watch
 *
 * Cancel a field modification watch previously set by
 * SetFieldModificationWatch, on the field specified by klass and
 * field.
 *
 * OPTIONAL Functionality
 */
jvmtiError JNICALL
jvmtiClearFieldModificationWatch(jvmtiEnv* env,
                                 jclass UNREF klass,
                                 jfieldID UNREF field)
{
    TRACE2("jvmti.watch", "ClearFieldModificationWatch called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    //TBD

    return JVMTI_NYI;
}

