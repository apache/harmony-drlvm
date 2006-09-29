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
 * @version $Revision: 1.1.2.1.4.5 $
 */  


#include "open/thread_externals.h"

#include "platform_lowlevel.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;

#ifndef PLATFORM_POSIX
#include "vm_process.h"
#endif

#include "object_layout.h"
#include "open/vm_util.h"
#include "object_handles.h"
//FIXME remove this code
#include "exceptions.h"
#include "Class.h"
#include "environment.h"

#include "open/vm_util.h"
#include "nogc.h"
#include "sync_bits.h"
#include "vm_synch.h"

#include "lock_manager.h"
#include "thread_manager.h"
#include "thread_generic.h"
#include "open/thread_helpers.h"
#include "open/jthread.h"

#include "vm_threads.h"
#define LOG_DOMAIN "thread"
#include "cxxlog.h"
#include "tl/memory_pool.h"
#include "open/vm_util.h"
#include "suspend_checker.h"

#ifdef PLATFORM_NT
// wjw -- following lines needs to be generic for all OSs
#include "java_lang_thread_nt.h"
#elif defined _IPF_
#include "java_lang_thread_ipf.h"
#elif defined _EM64T_
//#include "java_lang_thread_em64t.h"
#else
#include "java_lang_thread_ia32.h"
#endif

#include "interpreter.h"
#include "exceptions_int.h"

static StaticInitializer thread_runtime_initializer;

static tl::MemoryPool thr_pool;

hythread_tls_key_t TLS_key_pvmthread;

#ifdef __cplusplus
extern "C" {
#endif

volatile VM_thread *p_the_safepoint_control_thread = 0;  // only set when a gc is happening
volatile safepoint_state global_safepoint_status = nill;

#ifdef __cplusplus
}
#endif

void init_TLS_data();

void vm_thread_init(Global_Env * UNREF p_env___not_used)
{
   init_TLS_data();
}


void vm_thread_shutdown()
{
} //vm_thread_shutdown



VM_thread * get_a_thread_block()
{
    VM_thread *p_vmthread;

    p_vmthread = p_TLS_vmthread;   
    if (!p_vmthread) {
        p_vmthread = (VM_thread *)thr_pool.alloc(sizeof(VM_thread));
        memset(p_vmthread, 0, sizeof(VM_thread) );
    }
    return p_vmthread;
    } 
    
void free_this_thread_block(VM_thread *p_vmthread)
{
     }
        
void vm_thread_attach()
{
    VM_thread *p_vmthread;
   
    p_vmthread = p_TLS_vmthread;   
    if (!p_vmthread) {
        p_vmthread = (VM_thread *)thr_pool.alloc(sizeof(VM_thread));
        memset(p_vmthread, 0, sizeof(VM_thread) );
        set_TLS_data(p_vmthread);
    }
} //init_thread_block

VM_thread *get_vm_thread(hythread_t thr) {
    if (thr == NULL) {
        return NULL;
}
    return (VM_thread *)hythread_tls_get(thr, TLS_key_pvmthread);
}

VM_thread *get_vm_thread_ptr_safe(JNIEnv *jenv, jobject jThreadObj)
{
   hythread_t t=jthread_get_native_thread(jThreadObj);
   if(t == NULL) {
      return NULL;
   }
   return (VM_thread *)hythread_tls_get(t, TLS_key_pvmthread);
}

VM_thread *get_thread_ptr_stub()
{   
 return get_vm_thread(hythread_self());
}
  
vm_thread_accessor* get_thread_ptr = get_thread_ptr_stub;
void init_TLS_data() {
    hythread_tls_alloc(&TLS_key_pvmthread);
#ifndef _EM64T_
    get_thread_ptr = (vm_thread_accessor*) get_tls_helper(TLS_key_pvmthread);
    //printf ("init fast call %p\n", get_thread_ptr);
#endif

}
  
void set_TLS_data(VM_thread *thread) {
    hythread_tls_set(hythread_self(), TLS_key_pvmthread, thread);
        //printf ("sett ls call %p %p\n", get_thread_ptr(), get_vm_thread(hythread_self()));
}

IDATA jthread_throw_exception(char* name, char* message) {
    jobject jthe = exn_create(name);
    return jthread_throw_exception_object(jthe);
}

IDATA jthread_throw_exception_object(jobject object) {
    if (interpreter_enabled()) {
        set_current_thread_exception(object->object);
    } else {
        if (is_unwindable()) {
            exn_throw_object(object);
        } else {
            ASSERT_RAISE_AREA;
            exn_raise_object(object);
        }
    }

    return 0;
}

/**
 * This file contains the functions which eventually should become part of vmcore.
 * This localizes the dependencies of Thread Manager on vmcore component.
 */

void *vm_object_get_lockword_addr(jobject monitor){
    return (*(ManagedObject**)monitor)->get_obj_info_addr();
}

extern "C" char *vm_get_object_class_name(void* ptr) {
        return (char*) (((ManagedObject*)ptr)->vt()->clss->name->bytes);
}

void* vm_jthread_get_tm_data(jthread thread)
{
    JNIEnv *jenv = (JNIEnv*)jni_native_intf;    
    jclass jThreadClass = jenv->GetObjectClass(thread);
    jfieldID field_id = jenv->GetFieldID(jThreadClass, "vm_thread", "J");
    POINTER_SIZE_INT data = (POINTER_SIZE_INT)jenv->GetLongField(thread, field_id);

    return (void *)data;
}

void vm_jthread_set_tm_data(jthread jt, void* nt) {
    JNIEnv *jenv = (JNIEnv*)jni_native_intf;    
    jclass jthreadclass = jenv->GetObjectClass(jt);
    jfieldID field_id = jenv->GetFieldID(jthreadclass, "vm_thread", "J");
    jenv->SetLongField(jt, field_id, (jlong)(POINTER_SIZE_INT)nt);
}

JNIEnv * get_jnienv(void)
{
    return (JNIEnv*)jni_native_intf;
}

int vm_objects_are_equal(jobject obj1, jobject obj2){
    //ObjectHandle h1 = (ObjectHandle)obj1;
    //ObjectHandle h2 = (ObjectHandle)obj2;
    if (obj1 == NULL && obj2 == NULL){
        return 1;
}
    if (obj1 == NULL || obj2 == NULL){
    return 0;
}
    return obj1->object == obj2->object;
    }

int ti_is_enabled(){
    return VM_Global_State::loader_env->TI->isEnabled();
}

