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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.4 $
 */
/*
 * JVMTI thread group APIO
 */

#include "jvmti_utils.h"
#include "vm_threads.h"
#include <open/jthread.h>
#include "open/vm_util.h"
#include "cxxlog.h"
#include "suspend_checker.h"
#include "environment.h"
#include "exceptions.h"
#include "Class.h"

#define jvmti_test_jenv (p_TLS_vmthread->jni_env)

/*
 * Get Top Thread Groups
 *
 * Return all top-level (parentless) thread groups in the VM.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetTopThreadGroups(jvmtiEnv* env,
                        jint* group_count_ptr,
                        jthreadGroup** groups_ptr)
{
    TRACE2("jvmti.thread.group", "GetTopThreadGroups called");
    SuspendEnabledChecker sec;
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (group_count_ptr == NULL || groups_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    ti->setLocallyDisabled();//-----------------------------------V

    jclass cl = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->java_lang_Thread_Class);
    jmethodID id = jvmti_test_jenv -> GetMethodID(cl,
            "getThreadGroup","()Ljava/lang/ThreadGroup;");
    jobject current_thread = (jobject)jthread_self();
    jobject group = jvmti_test_jenv -> CallObjectMethod (current_thread, id);

    cl = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->java_lang_ThreadGroup_Class);
    id = jvmti_test_jenv -> GetMethodID(cl,
            "getParent","()Ljava/lang/ThreadGroup;");

    jobject parent = jvmti_test_jenv -> CallObjectMethod (group, id);
    while (parent)
    {
        group = parent;
        parent = jvmti_test_jenv -> CallObjectMethod (group, id);
    }

    ti->setLocallyEnabled();//-----------------------------------^

    jvmtiError jvmti_error = _allocate(sizeof(jthreadGroup*),
            (unsigned char **)groups_ptr);

    if (JVMTI_ERROR_NONE != jvmti_error){
        return jvmti_error;
    }

    (*groups_ptr)[0] = group;
    *group_count_ptr = 1;

    return JVMTI_ERROR_NONE;
}

/*
* Get Thread Group Info
*
* Get information about the thread group. The fields of the
* jvmtiThreadGroupInfo structure are filled in with details of
* the specified thread group.
*
* REQUIRED Functionality
*/
jvmtiError JNICALL
jvmtiGetThreadGroupInfo(jvmtiEnv* env,
                    jthreadGroup group,
                    jvmtiThreadGroupInfo* info_ptr)
{
    TRACE2("jvmti.thread.group", "GetThreadGroupInfo called");
    SuspendEnabledChecker sec;
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (! is_valid_thread_group_object(group)) {
        return JVMTI_ERROR_INVALID_THREAD_GROUP;
    }

    if (info_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    ti->setLocallyDisabled();//-----------------------------------V

    jclass cl = GetObjectClass(jvmti_test_jenv, group);
    jmethodID id = jvmti_test_jenv -> GetMethodID(cl, "getParent","()Ljava/lang/ThreadGroup;");
    info_ptr -> parent = jvmti_test_jenv -> CallObjectMethod (group, id);

    id = jvmti_test_jenv -> GetMethodID(cl, "getName","()Ljava/lang/String;");
    jstring  name = jvmti_test_jenv -> CallObjectMethod (group, id);
    info_ptr -> name = (char *)jvmti_test_jenv -> GetStringUTFChars (name, false);

    id = jvmti_test_jenv -> GetMethodID(cl, "getMaxPriority","()I");
    info_ptr -> max_priority = jvmti_test_jenv -> CallIntMethod (group, id);

    id = jvmti_test_jenv -> GetMethodID(cl, "isDaemon","()Z");
    info_ptr -> is_daemon = jvmti_test_jenv -> CallBooleanMethod (group, id);

    ti->setLocallyEnabled();//-----------------------------------^

    return JVMTI_ERROR_NONE;
}

/**
 * This function extracts element specified by 'index' from 'pair' array.
 * Extracted element assumed to be an jobjectArray and is converted to native
 * array of jobject that is returned via 'array_ptr' ant 'count_ptr' args.
 */
static jvmtiError read_array(jvmtiEnv* env,
                                  jobjectArray pair,
                                  jint index,
                                  jint* count_ptr,
                                  jobject** array_ptr)
{
    jint count = 0;
    jobjectArray object_array = NULL;

    if (NULL != pair) {
        object_array = jvmti_test_jenv->GetObjectArrayElement(pair, index);

        if (NULL != object_array)
            count = jvmti_test_jenv->GetArrayLength(object_array);
    }

    jvmtiError err;

    jobject* native_array = NULL;
    err = _allocate(sizeof(jobject) * count, (unsigned char**) &native_array);

    if (JVMTI_ERROR_NONE != err)
        return err;

    for (int i = 0; i < count; i++)
        native_array[i] = jvmti_test_jenv->
                GetObjectArrayElement(object_array, i);

    *count_ptr = count;
    *array_ptr = native_array;

    return JVMTI_ERROR_NONE;
}

/*
 * Get Thread Group Children
 *
 * Get the active threads and active subgroups in this thread group.
 *
 * REQUIRED Functionality
 */
jvmtiError JNICALL
jvmtiGetThreadGroupChildren(jvmtiEnv* env,
                            jthreadGroup group,
                            jint* thread_count_ptr,
                            jthread** threads_ptr,
                            jint* group_count_ptr,
                            jthreadGroup** groups_ptr)
{
    TRACE2("jvmti.thread.group", "GetThreadGroupChildren called");
    SuspendEnabledChecker sec;
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (! is_valid_thread_group_object(group)) {
        return JVMTI_ERROR_INVALID_THREAD_GROUP;
    }

    if (thread_count_ptr == NULL || threads_ptr == NULL ||
        group_count_ptr  == NULL || groups_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    jclass thread_group_class = GetObjectClass(jvmti_test_jenv, group);
    jmethodID method = jvmti_test_jenv -> GetMethodID(thread_group_class,
            "getActiveChildren","()[Ljava/lang/Object;");
    assert(method);

    ti->setLocallyDisabled();//-----------------------------------V

    // by contract this method returns Object[2] array.
    // First element is Object[] array of child Thread objects.
    // Second element is Object[] array of child ThreadGroup objects.
    jobjectArray result = jvmti_test_jenv->CallObjectMethod(group, method);

    ti->setLocallyEnabled();//-----------------------------------^

    if (exn_raised())
        return JVMTI_ERROR_INTERNAL;

    jvmtiError err;

    // extract child threads array
    err = read_array(env, result, 0, thread_count_ptr, threads_ptr);

    if (JVMTI_ERROR_NONE != err)
        return err;

    // extract child groups array
    err = read_array(env, result, 1, group_count_ptr, groups_ptr);

    if (JVMTI_ERROR_NONE != err) {
        _deallocate((unsigned char*) *threads_ptr);
        return err;
    }

    return JVMTI_ERROR_NONE;
} // jvmtiGetThreadGroupChildren
