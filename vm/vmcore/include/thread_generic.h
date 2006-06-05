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


#ifndef THREAD_GENERIC_HEADER
#define THREAD_GENERIC_HEADER

#include "vm_threads.h"
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VM_THREADS 2048

#define java_lang_Thread_NORM_PRIORITY 5L

VM_thread *get_vm_thread_ptr(void *p_ref);

void Java_java_lang_Thread_setPriority_generic(VM_thread *p_thr, long pty);
jint Java_java_lang_Thread_countStackFrames_generic(VM_thread *p_thr);

VMEXPORT VM_thread *get_vm_thread_ptr_safe(JNIEnv *, jobject);


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////


void Java_java_lang_Thread_start_generic(JNIEnv *, jobject,
                                         jvmtiEnv * jvmtiEnv, jvmtiStartFunction proc, 
                                         const void* arg, jint priority);

void Java_java_lang_Thread_interrupt_generic(VM_thread *) ;

void Java_java_lang_Thread_sleep_generic(JNIEnv *, VM_thread *, int64);

void set_interrupt_flag_in_thread_object(JNIEnv *, jobject );

void wait_until_non_daemon_threads_are_dead();

#ifdef __cplusplus
}
#endif


#endif //THREAD_GENERIC_HEADER
