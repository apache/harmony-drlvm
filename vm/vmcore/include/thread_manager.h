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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#ifndef THREAD_MANAGER_HEADER
#define THREAD_MANAGER_HEADER

#include "vm_threads.h"


#ifdef __cplusplus
extern "C" {
#endif

void free_this_thread_block(VM_thread *);
VM_thread * get_a_thread_block(JavaVM_Internal * java_vm);


extern volatile VM_thread *p_the_safepoint_control_thread;  // only set when a gc is happening
extern volatile safepoint_state global_safepoint_status;


#ifdef __cplusplus
}
#endif

#endif // THREAD_MANAGER_HEADER
