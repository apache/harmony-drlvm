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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#ifndef THREAD_MANAGER_HEADER
#define THREAD_MANAGER_HEADER


#ifdef __cplusplus
extern "C" {
#endif

 
void vm_thread_shutdown();
void vm_thread_init(Global_Env *p_env);

void vm_thread_attach();
void vm_thread_detach();

void free_this_thread_block(VM_thread *);
VM_thread * get_a_thread_block();


extern volatile VM_thread *p_the_safepoint_control_thread;  // only set when a gc is happening
extern volatile safepoint_state global_safepoint_status;


#ifdef __cplusplus
}
#endif

#endif // THREAD_MANAGER_HEADER
