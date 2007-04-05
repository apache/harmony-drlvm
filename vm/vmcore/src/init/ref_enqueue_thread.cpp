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

void ref_enqueue_thread_init(JavaVM *java_vm)
{
    if(!native_ref_thread_flag)
        return;
    
    ref_thread_info = (Ref_Enqueue_Thread_Info *)STD_MALLOC(sizeof(Ref_Enqueue_Thread_Info));
    ref_thread_info->shutdown = false;
    ref_thread_info->thread_attached = 0;
    
    IDATA status = hysem_create(&ref_thread_info->pending_sem, 0, REF_ENQUEUE_THREAD_NUM);
    assert(status == TM_ERROR_NONE);
    
    void **args = (void **)STD_MALLOC(sizeof(void *));
    args[0] = (void *)java_vm;
    status = hythread_create(NULL, 0, REF_ENQUEUE_THREAD_PRIORITY, 0, (hythread_entrypoint_t)ref_enqueue_thread_func, args);
    assert(status == TM_ERROR_NONE);
    
    while(ref_thread_info->thread_attached == 0);
}

void ref_enqueue_shutdown(void)
{
    ref_thread_info->shutdown = TRUE;
    activate_ref_enqueue_thread();
}

void activate_ref_enqueue_thread(void)
{
    IDATA stat = hysem_set(ref_thread_info->pending_sem, REF_ENQUEUE_THREAD_NUM);
    assert(stat == TM_ERROR_NONE);
}

static void wait_pending_reference(void)
{
    IDATA stat = hysem_wait(ref_thread_info->pending_sem);
    assert(stat == TM_ERROR_NONE);
}

static IDATA ref_enqueue_thread_func(void **args)
{
    JavaVM *java_vm = (JavaVM *)args[0];
    JNIEnv *jni_env;
    //jthread java_thread;
    char *name = "ref handler";
    //jboolean daemon = JNI_TRUE;
    
    //IDATA status = vm_attach_internal(&jni_env, &java_thread, java_vm, NULL, name, daemon);
    //assert(status == JNI_OK);
    //status = jthread_attach(jni_env, java_thread, daemon);
    //assert(status == TM_ERROR_NONE);
    JavaVMAttachArgs *jni_args = (JavaVMAttachArgs*)STD_MALLOC(sizeof(JavaVMAttachArgs));
    jni_args->version = JNI_VERSION_1_2;
    jni_args->name = name;
    jni_args->group = NULL;
    IDATA status = AttachCurrentThreadAsDaemon(java_vm, (void**)&jni_env, jni_args);
    assert(status == JNI_OK);
    ref_thread_info->thread_attached = 1;
    
    while(true){
        /* Waiting for pending weak references */
        wait_pending_reference();
        
        /* do the real reference enqueue work */
        vm_ref_enqueue_func();
        
        if(ref_thread_info->shutdown)
            break;
    }
    
    status = DetachCurrentThread(java_vm);
    //status = jthread_detach(java_thread);
    return status;
}
