/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef OPEN_THREAD_EXTERNALS_H
#define OPEN_THREAD_EXTERNALS_H

#include "open/types.h"
#include "open/hycomp.h"
#include "jni.h"
#include "jvmti_types.h"
#include "open/hythread_ext.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * @param[in] obj - jobject those address needs to be given
 *
 * @return The address of the memory chunk in the object which can be used by the 
 *         Thread Manager for synchronization purposes.
 */
VMEXPORT hythread_thin_monitor_t * vm_object_get_lockword_addr(jobject obj);

/**
 * @return The size of the memory chunk in the object that can be used by
 *         Thread Manager for synchronization purposes. 
 *
 * The returned size must be equal for all Java objets and is constant over time. 
 * It should be possible to call this method during initialization time. 
 */
 VMEXPORT size_t vm_object_get_lockword_size();


/**
 * Stores a pointer to TM-specific data in the <code>java.lang.Thread</code> object.
 *
 * A typical implementation may store a pointer within a private
 * non-static field of Thread.
 *
 * @param[in] thread    - a <code>java.lang.Thread</code> object those private field 
 *                        is going to be used for data storage
 * @param[in] data_ptr  - a pointer to data to be stored
 */
VMEXPORT void vm_jthread_set_tm_data(jthread thread, void *data_ptr);

/**
 * Retrieves TM-specific data from the <code>java.lang.Thread</code> object.
 *
 * @param[in] thread - a thread
 *
 * @return TM-specific data previously stored, or <code>NULL</code>,
 *         if there are none.
 */
VMEXPORT hythread_t vm_jthread_get_tm_data(jthread thread);

/** 
 * Registrates current thread in VM, so it could execute Java.
 *
 * @param[in] java_vm    - current thread will be attached to the specified VM
 * @param[out] p_jni_env - will point to JNI environment assocciated with the thread
 */
VMEXPORT jint vm_attach(JavaVM * java_vm, JNIEnv ** p_jni_env);

/**
 * Frees java related resources before thread exit.
 */
VMEXPORT jint vm_detach(jthread java_thread);

/**
 * Creates exception object using given class name and message and throws it
 * using <code>jthread_throw_exception</code> method.
 *
 * @param[in] name     - char* name
 * @param[in] message  - char* message
 *
 * @return <code>int</code>
 */
VMEXPORT IDATA jthread_throw_exception(char* name, char* message);

/**
 * Throws given exception object. Desides whether current thread is unwindable
 * and throws it, raises exception otherwise.
 */
VMEXPORT IDATA jthread_throw_exception_object(jobject object);

// TI support interface
VMEXPORT void jvmti_send_thread_start_end_event(int);
VMEXPORT void jvmti_send_contended_enter_or_entered_monitor_event(jobject obj, int isEnter);
VMEXPORT void jvmti_send_waited_monitor_event(jobject obj, jboolean is_timed_out);
VMEXPORT void jvmti_send_wait_monitor_event(jobject obj, jlong timeout);

/**
 * <code>vm_objects_are_equal<br>
 * obj1 jobject<br>
 * obj2 jobject</code>
 * 
 * @return <code>int</code>
 */
VMEXPORT int vm_objects_are_equal(jobject obj1, jobject obj2);

/**
 * <code>ti</code> is enabled
 *
 * @return <code>int</code>
 */
VMEXPORT int ti_is_enabled();

/** 
 * Allocates memory needed for creating new vm thread
 *
 * @param[in] java_vm    - current thread will be attached to the specified VM
 */

VMEXPORT void *vm_allocate_thread_dummies(JavaVM *java_vm);

/**
 * A structure for thread manager properties
 * There is the only yet. Others to be added here.
 */
struct tm_props {
    int use_soft_unreservation;
};

#if !defined(_TM_PROP_EXPORT)
extern VMIMPORT struct tm_props *tm_properties;
#else 
struct tm_props *tm_properties = NULL;
#endif


#ifdef __cplusplus
}
#endif

#endif  /* OPEN_THREAD_EXTERNALS_H */


