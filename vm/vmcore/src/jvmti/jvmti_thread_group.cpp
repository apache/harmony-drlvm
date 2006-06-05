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
 * JVMTI thread group APIO
 */

#include "jvmti_utils.h"
#include "vm_threads.h"
#include "open/vm_util.h"
#include "cxxlog.h"
#include "suspend_checker.h"
#include "environment.h"

static JNIEnv_Internal * jvmti_test_jenv = jni_native_intf;
jobject get_jobject(VM_thread * thread);

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
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (group_count_ptr == NULL || groups_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    jclass cl = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->java_lang_Thread_Class);
    jmethodID id = jvmti_test_jenv -> GetMethodID(cl,
            "getThreadGroup","()Ljava/lang/ThreadGroup;");
    jobject current_thread = get_jobject(p_TLS_vmthread);
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
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (group == NULL)
    { // TBD group is not a thread object
        return JVMTI_ERROR_INVALID_THREAD;
    }

    if (info_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

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
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (group == NULL){ // TBD group is not a thread object
        return JVMTI_ERROR_INVALID_THREAD;
    }

    if (thread_count_ptr == NULL || threads_ptr == NULL ||
        group_count_ptr  == NULL || groups_ptr == NULL)
    {
        return JVMTI_ERROR_NULL_POINTER;
    }

    jclass cl = GetObjectClass(jvmti_test_jenv, group);
    jobjectArray jg = jvmti_test_jenv -> NewObjectArray(10, cl, 0);
    jmethodID id = jvmti_test_jenv -> GetMethodID(cl, "enumerate","([Ljava/lang/ThreadGroup;)I");
    int cc = jvmti_test_jenv -> CallIntMethod (group, id, jg);
    jthreadGroup* groups = NULL;
    jvmtiError jvmti_error = _allocate(sizeof(jthreadGroup) * cc,
            (unsigned char **)&groups);

    if (JVMTI_ERROR_NONE != jvmti_error)
    {
        return jvmti_error;
    }

    int i; // Visual C++ 6.0 does not treat "for" as C++ scope

    for (i = 0; i < cc; i++)
    {
        groups[i] = jvmti_test_jenv -> GetObjectArrayElement(jg, i);
    }

    *group_count_ptr = cc;
    *groups_ptr = groups;

    jclass cll = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->java_lang_Thread_Class);
    jobjectArray jt = jvmti_test_jenv -> NewObjectArray(10, cll, 0);
    id = jvmti_test_jenv -> GetMethodID(cl, "enumerate","([Ljava/lang/Thread;)I");
    cc = jvmti_test_jenv -> CallIntMethod (group, id, jt);
    jthread * threads = NULL;
    jvmti_error = _allocate(sizeof(jthread) * cc, (unsigned char **)&threads);

    if (JVMTI_ERROR_NONE != jvmti_error)
    {
        return jvmti_error;
    }

    for (i = 0; i < cc; i++)
    {
        threads[i] = jvmti_test_jenv -> GetObjectArrayElement(jt, i);
    }

    *thread_count_ptr = cc;
    *threads_ptr = threads;

    return JVMTI_ERROR_NONE;
}


