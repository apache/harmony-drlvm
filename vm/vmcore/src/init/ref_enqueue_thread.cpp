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
#include "../../../thread/src/thread_private.h"

static Boolean native_ref_enqueue_thread_flag = FALSE;
static struct ref_enqueue_thread_info *ref_enqueue_thread_info = NULL;


Boolean get_native_ref_enqueue_thread_flag()
{
  return native_ref_enqueue_thread_flag;
}

void set_native_ref_enqueue_thread_flag(Boolean flag)
{
  native_ref_enqueue_thread_flag = flag;
}

static int ref_enqueue_thread_func(void **args);

void ref_enqueue_thread_init(JavaVM *java_vm, JNIEnv *jni_env)
{
    if(!get_native_ref_enqueue_thread_flag())
        return;
    
    ref_enqueue_thread_info = (struct ref_enqueue_thread_info *)STD_MALLOC(sizeof(struct ref_enqueue_thread_info));
    ref_enqueue_thread_info->lock = FREE_LOCK;
    ref_enqueue_thread_info->shutdown = FALSE;
    
    int status = vm_create_event(&ref_enqueue_thread_info->reference_pending_event, 0, 1);
    assert(status == TM_ERROR_NONE);
    
    void **args = (void **)STD_MALLOC(sizeof(void *)*2);
    args[0] = (void *)java_vm;
    args[1] = (void *)jni_env;
    status = (unsigned int)hythread_create(NULL, 0, REF_ENQUEUE_THREAD_PRIORITY, 0, (hythread_entrypoint_t)ref_enqueue_thread_func, args);
    assert(status == TM_ERROR_NONE);
}

void ref_enqueue_shutdown(void)
{
    gc_lock(ref_enqueue_thread_info->lock);
    ref_enqueue_thread_info->shutdown = TRUE;
    gc_unlock(ref_enqueue_thread_info->lock);
    activate_ref_enqueue_thread();
}

void activate_ref_enqueue_thread(void)
{
    vm_post_event(ref_enqueue_thread_info->reference_pending_event);
}


static int ref_enqueue_func(void)
{
    vm_ref_enqueue_func();
    return 0;
}

static void wait_pending_reference(void)
{
    vm_wait_event(ref_enqueue_thread_info->reference_pending_event);
}


static int ref_enqueue_thread_func(void **args)
{
    JavaVM *java_vm = (JavaVM *)args[0];
    JNIEnv *jni_env = (JNIEnv *)args[1];
    
    IDATA status = vm_attach(java_vm, &jni_env, NULL);
    if(status != TM_ERROR_NONE)
        return status;
    
    while(true){
        /* Waiting for pending weak references */
        wait_pending_reference();
        
        /* do the real reference enqueue work */
        ref_enqueue_func();
                
        gc_lock(ref_enqueue_thread_info->lock);
        if(ref_enqueue_thread_info->shutdown){
            gc_unlock(ref_enqueue_thread_info->lock);
            break;
        }
        gc_unlock(ref_enqueue_thread_info->lock);
    }

    return TM_ERROR_NONE;
}
