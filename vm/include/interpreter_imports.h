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
 * @author Ivan Volosyuk
 * @version $Revision: 1.1.2.1.4.3 $
 */  
#ifndef _INTERPRETER_IMPORTS_H_
#define _INTERPRETER_IMPORTS_H_

#include "open/types.h"
#include "vm_core_types.h"
#include "jvmti.h"

struct ManagedObject;
typedef struct ManagedObject ManagedObject;
VMEXPORT void vm_monitor_enter_wrapper(ManagedObject *obj);
VMEXPORT void vm_monitor_exit_wrapper(ManagedObject *obj);
VMEXPORT void class_throw_linking_error_for_interpreter(Class_Handle ch,
        unsigned cp_index, unsigned opcode);

VMEXPORT JNIEnv * get_jni_native_intf();

VMEXPORT jbyte jvmti_process_interpreter_breakpoint_event(jmethodID method, jlocation loc);
VMEXPORT void jvmti_process_single_step_event(jmethodID method, jlocation location);

VMEXPORT void jvmti_process_frame_pop_event(jvmtiEnv *env,
        jmethodID method, jboolean was_popped_by_exception);
VMEXPORT GenericFunctionPointer classloader_find_native(const Method_Handle method);

#endif /* _INTERPRETER_IMPORTS_H_ */
