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

#include "lock_manager.h"
#include "thread_manager.h"
#include "thread_generic.h"
#include "open/thread_helpers.h"
#include "open/jthread.h"

#include "vm_threads.h"
#include "tl/memory_pool.h"
#include "open/vm_util.h"
#include "suspend_checker.h"
#include "jni_utils.h"
#include "heap.h"
#include "vm_strings.h"
#include "interpreter.h"
#include "exceptions_int.h"

#ifdef _IPF_
#include "java_lang_thread_ipf.h"
#elif defined _EM64T_
//#include "java_lang_thread_em64t.h"
#else
#include "java_lang_thread_ia32.h"
#endif

#define LOG_DOMAIN "vmcore.thread"
#include "cxxlog.h"

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

VM_thread * allocate_thread_block(JavaVM_Internal * java_vm) {
    VM_thread * p_vmthread;
    apr_pool_t * thread_pool;
  
    if (apr_pool_create(&thread_pool, java_vm->vm_env->mem_pool) != APR_SUCCESS) {
	return NULL;
    }
    p_vmthread = (VM_thread *) apr_pcalloc(thread_pool, sizeof(VM_thread));

    if (!p_vmthread) return NULL;
    
    p_vmthread->pool = thread_pool;

    return p_vmthread;
}
  


VM_thread * get_a_thread_block(JavaVM_Internal * java_vm) {
    VM_thread * p_vmthread;

    p_vmthread = p_TLS_vmthread;
    if (!p_vmthread) {
      p_vmthread = allocate_thread_block(java_vm);
      
      if (!p_vmthread) return NULL;
      set_TLS_data(p_vmthread);

    } else {
        memset(p_vmthread, 0, sizeof(VM_thread));
    }
    return p_vmthread;
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
    //printf ("init TLS data, TLS key = %x \n", TLS_key_pvmthread);
    TLS_key_pvmthread = TM_THREAD_VM_TLS_KEY;
}
  
void set_TLS_data(VM_thread *thread) {
    hythread_tls_set(hythread_self(), TLS_key_pvmthread, thread);
    //printf ("sett ls call %p %p\n", get_thread_ptr(), get_vm_thread(hythread_self()));
}

IDATA jthread_throw_exception(char* name, char* message) {
    assert(hythread_is_suspend_enabled());
    jobject jthe = exn_create(name);
    return jthread_throw_exception_object(jthe);
}

IDATA jthread_throw_exception_object(jobject object) {
    if (interpreter_enabled()) {
        // FIXME - Function set_current_thread_exception does the same
        // actions as exn_raise_object, and it should be replaced.
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
        return (char*)(((ManagedObject*)ptr)->vt()->clss->get_name()->bytes);
}

void* vm_jthread_get_tm_data(jthread thread)
{
    static int offset = -1;
    Class * clazz;
    Field * field;
    ManagedObject * thread_obj;
    Byte * java_ref;
    POINTER_SIZE_INT val;

    hythread_suspend_disable();

    thread_obj = ((ObjectHandle)thread)->object;
    if (offset == -1) {
        clazz = thread_obj->vt()->clss;
        field = class_lookup_field_recursive(clazz, "vm_thread", "J");
        offset = field->get_offset();
    }
    java_ref = (Byte *)thread_obj;
    val = *(POINTER_SIZE_INT *)(java_ref + offset);

    hythread_suspend_enable();

    return (void *)val;
}

void vm_jthread_set_tm_data(jthread thread, void* val) {
    static int offset = -1;
    Class * clazz;
    Field * field;
    ManagedObject * thread_obj;
    Byte * java_ref;

    hythread_suspend_disable();

    thread_obj = ((ObjectHandle)thread)->object;
    if (offset == -1) {
        clazz = thread_obj->vt()->clss;
        field = class_lookup_field_recursive(clazz, "vm_thread", "J");
        offset = field->get_offset();
    }

    java_ref = (Byte *)thread_obj;
    *(jlong *)(java_ref + offset) = (jlong)(POINTER_SIZE_INT)val;

    hythread_suspend_enable();
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
