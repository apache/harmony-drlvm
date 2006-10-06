 /* Licensed to the Apache Software Foundation (ASF) under one or more
 /* contributor license agreements.  See the NOTICE file distributed with
 /* this work for additional information regarding copyright ownership.
 /* The ASF licenses this file to You under the Apache License, Version 2.0
 /* (the "License"); you may not use this file except in compliance with
 /* the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * @author Artem Aliev
 * @version $Revision$
 */

#ifndef OPEN_THREAD_EXTERNALS_H
#define OPEN_THREAD_EXTERNALS_H

#include "open/types.h"
#include "open/hycomp.h"
#include <jni.h>
#include "jvmti_types.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Returns the address of the memory chunk in the object which can be used by the 
 * Thread Manager for synchronization purposes.
 *
 * @param[in] obj jobject those address needs to be given
 * @return 
 */
VMEXPORT void *vm_object_get_lockword_addr(jobject obj);

/**
 * Returns the size of the memory chunk in the object that can be used by
 * Thread Manager for synchronization purposes. 
 *
 * The returned size must be equal for all Java objets and is constant over time. 
 * It should be possible to call this method during initialization time.
 *
 * @return the size 
 */
 VMEXPORT size_t vm_object_get_lockword_size();


/**
 * Stores a pointer to TM-specific data in the java.lang.Thread object.
 *
 * A typical implementation may store a pointer within a private
 * non-static field of Thread.
 *
 * @param[in] thread a java.lang.Thread object those private field is going to be used for data storage
 * @param[in] data_ptr a pointer to data to be stored
 */
VMEXPORT void vm_jthread_set_tm_data(jthread thread, void *data_ptr);

/**
 * Retrieves TM-specific data from the java.lang.Thread object
 *
 * @param[in] thread a thread 
 * @return TM-specific data previously stored, or NULL if there are none.
 */
VMEXPORT void *vm_jthread_get_tm_data(jthread thread);

/** 
  * registtrate thread in VM, so it could execute Java
  */
VMEXPORT IDATA vm_attach();
/**
  * free java related resources before thread exit
  */
VMEXPORT IDATA vm_detach();

/**
 * creates exception object using given class name and message and throws it
 * using jthread_throw_exception method;
 * @param[in] name char* -name
 * @param[in] message char* -message
 * @return int.
 */
VMEXPORT IDATA jthread_throw_exception(char* name, char* message);

/**
 * Throws given exception object; Desides whether current thread is unwindable
 * and throws it, raises exception otherwise;
 */
VMEXPORT IDATA jthread_throw_exception_object(jobject object);

// TI support interface
VMEXPORT void jvmti_send_thread_start_end_event(int);
VMEXPORT void jvmti_send_contended_enter_or_entered_monitor_event(jobject obj, int isEnter);
VMEXPORT void jvmti_send_waited_monitor_event(jobject obj, jboolean is_timed_out);
VMEXPORT void jvmti_send_wait_monitor_event(jobject obj, jlong timeout);

/**
 * vm_objects_are_equal
 * obj1 jobject
 * obj2 jobject
 * @return int
 */
VMEXPORT int vm_objects_are_equal(jobject obj1, jobject obj2);

/**
 * ti is enabled
 * @return int
 */
VMEXPORT int ti_is_enabled();

/**
 * get JNIEnv *
 * @return JNIEnv *
 */
VMEXPORT JNIEnv * get_jnienv(void);


#ifdef __cplusplus
}

/**
  * all folowing entries is requiried for VM helpers only 
  */

#include "encoder.h"


#endif

#endif  /* OPEN_THREAD_EXTERNALS_H */

