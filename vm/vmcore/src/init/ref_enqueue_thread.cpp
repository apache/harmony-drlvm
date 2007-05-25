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
 * @author Li-Gang Wang, 2006/11/15
 */

#include <apr_atomic.h>
#include "ref_enqueue_thread.h"
#include "finalize.h"
#include "vm_threads.h"
#include "init.h"
#include "open/jthread.h"


static Boolean native_ref_thread_flag = FALSE;
static Ref_Enqueue_Thread_Info *ref_thread_info = NULL;


Boolean get_native_ref_enqueue_thread_flag()
{  return native_ref_thread_flag; }

void set_native_ref_enqueue_thread_flag(Boolean flag)
{  native_ref_thread_flag = flag; }


static IDATA ref_enqueue_thread_func(void **args);
static void wait_ref_thread_attached(void);
extern jobject get_system_thread_group(JNIEnv* jni_env);

void ref_enqueue_thread_init(JavaVM *java_vm, JNIEnv* jni_env)
{
    if(!native_ref_thread_flag)
        return;
    
    ref_thread_info = (Ref_Enqueue_Thread_Info *)STD_MALLOC(sizeof(Ref_Enqueue_Thread_Info));
    ref_thread_info->shutdown = false;
    ref_thread_info->thread_attached = 0;
    
    IDATA status = hysem_create(&ref_thread_info->pending_sem, 0, REF_ENQUEUE_THREAD_NUM);
    assert(status == TM_ERROR_NONE);
    
    status = hycond_create(&ref_thread_info->end_cond);
    assert(status == TM_ERROR_NONE);
    status = hymutex_create(&ref_thread_info->end_mutex, TM_MUTEX_DEFAULT);
    assert(status == TM_ERROR_NONE);
    
    void **args = (void **)STD_MALLOC(sizeof(void *)*2);
    args[0] = (void *)java_vm;
    args[1] = (void*)get_system_thread_group(jni_env);
    status = hythread_create(NULL, 0, REF_ENQUEUE_THREAD_PRIORITY, 0, (hythread_entrypoint_t)ref_enqueue_thread_func, args);
    assert(status == TM_ERROR_NONE);
    
    wait_ref_thread_attached();
}

void ref_enqueue_shutdown(void)
{
    ref_thread_info->shutdown = TRUE;
    activate_ref_enqueue_thread(FALSE);
}

static uint32 atomic_inc32(volatile apr_uint32_t *mem)
{  return (uint32)apr_atomic_inc32(mem); }

static uint32 atomic_dec32(volatile apr_uint32_t *mem)
{  return (uint32)apr_atomic_dec32(mem); }

static void inc_ref_thread_num(void)
{ atomic_inc32(&ref_thread_info->thread_attached); }

static void dec_ref_thread_num(void)
{ atomic_dec32(&ref_thread_info->thread_attached); }

static void wait_ref_thread_attached(void)
{ while(ref_thread_info->thread_attached < REF_ENQUEUE_THREAD_NUM); }

void wait_native_ref_thread_detached(void)
{ while(ref_thread_info->thread_attached){hythread_yield();} }

static void wait_ref_enqueue_end(void)
{
    hymutex_lock(&ref_thread_info->end_mutex);
    unsigned int ref_num = vm_get_references_quantity();
    do {
        unsigned int wait_time = ref_num + 100;
        atomic_inc32(&ref_thread_info->end_waiting_num);
        IDATA status = hycond_wait_timed(&ref_thread_info->end_cond, &ref_thread_info->end_mutex, (I_64)wait_time, 0);
        atomic_dec32(&ref_thread_info->end_waiting_num);
        if(status != TM_ERROR_NONE) break;
        ref_num = vm_get_references_quantity();
    } while(ref_num);
    hymutex_unlock(&ref_thread_info->end_mutex);
}

void activate_ref_enqueue_thread(Boolean wait)
{
    IDATA stat = hysem_set(ref_thread_info->pending_sem, REF_ENQUEUE_THREAD_NUM);
    assert(stat == TM_ERROR_NONE);
    
    if(wait)
        wait_ref_enqueue_end();
}

static void notify_ref_enqueue_end(void)
{
    if(vm_get_references_quantity()==0)
        hycond_notify_all(&ref_thread_info->end_cond);
}

static void wait_pending_reference(void)
{
    IDATA stat = hysem_wait(ref_thread_info->pending_sem);
    assert(stat == TM_ERROR_NONE);
}

extern jint set_current_thread_context_loader(JNIEnv* jni_env);

static IDATA ref_enqueue_thread_func(void **args)
{
    JavaVM *java_vm = (JavaVM *)args[0];
    JNIEnv *jni_env;
    char *name = "ref handler";

    JavaVMAttachArgs *jni_args = (JavaVMAttachArgs*)STD_MALLOC(sizeof(JavaVMAttachArgs));
    jni_args->version = JNI_VERSION_1_2;
    jni_args->name = name;
    jni_args->group = (jobject)args[1];
    IDATA status = AttachCurrentThreadAsDaemon(java_vm, (void**)&jni_env, jni_args);
    assert(status == JNI_OK);
    set_current_thread_context_loader(jni_env);
    inc_ref_thread_num();
    
    while(true){
        /* Waiting for pending weak references */
        wait_pending_reference();
        
        /* do the real reference enqueue work */
        vm_ref_enqueue_func();
        
        if(ref_thread_info->end_waiting_num)
            notify_ref_enqueue_end();
        
        if(ref_thread_info->shutdown)
            break;
    }
    
    status = DetachCurrentThread(java_vm);
    dec_ref_thread_num();
    //status = jthread_detach(java_thread);
    return status;
}
