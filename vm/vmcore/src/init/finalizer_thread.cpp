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

#include "finalizer_thread.h"
#include "ref_enqueue_thread.h"
#include "open/gc.h"
#include "port_sysinfo.h"
#include "finalize.h"
#include "vm_threads.h"
#include "../../../thread/src/thread_private.h"

static Boolean native_finalizer_thread_flag = FALSE;
static struct finalizer_thread_info *finalizer_thread_info = NULL;


Boolean get_native_finalizer_thread_flag()
{
  return native_finalizer_thread_flag;
}

void set_native_finalizer_thread_flag(Boolean flag)
{
  native_finalizer_thread_flag = flag;
}

Boolean get_finalizer_shutdown_flag()
{
  return finalizer_thread_info->shutdown;
}

Boolean get_finalizer_on_exit_flag()
{
  return finalizer_thread_info->on_exit;
}

static int finalizer_thread_func(void **args);

void finalizer_threads_init(JavaVM *java_vm, JNIEnv *jni_env)
{
    if(!native_finalizer_thread_flag)
        return;
    
    finalizer_thread_info = (struct finalizer_thread_info *)STD_MALLOC(sizeof(struct finalizer_thread_info));
    finalizer_thread_info->lock = FREE_LOCK;
    finalizer_thread_info->thread_num = port_CPUs_number();
    finalizer_thread_info->working_thread_num = 0;
    finalizer_thread_info->shutdown = FALSE;
    finalizer_thread_info->on_exit = FALSE;
    
    int status = vm_create_event(&finalizer_thread_info->finalizer_pending_event, 0, finalizer_thread_info->thread_num);
    assert(status == TM_ERROR_NONE);
    status = vm_create_event(&finalizer_thread_info->finalization_end_event, 0, 1);
    assert(status == TM_ERROR_NONE);
    
    void **args = (void **)STD_MALLOC(sizeof(void *)*2);
    args[0] = (void *)java_vm;
    args[1] = (void *)jni_env;
    for(int i = 0; i < finalizer_thread_info->thread_num; i++){
        status = (unsigned int)hythread_create(NULL, 0, 0, 0, (hythread_entrypoint_t)finalizer_thread_func, args);
        assert(status == TM_ERROR_NONE);
    }
}

void finalizer_shutdown(Boolean start_finalization_on_exit)
{
    if(start_finalization_on_exit){
        tmn_suspend_disable();
        gc_force_gc();      
        tmn_suspend_enable();
        activate_finalizer_threads(TRUE);
        tmn_suspend_disable();
        gc_finalize_on_exit();
        tmn_suspend_enable();
        gc_lock(finalizer_thread_info->lock);
        finalizer_thread_info->on_exit = TRUE;
        gc_unlock(finalizer_thread_info->lock);
        activate_finalizer_threads(TRUE);
    }
    gc_lock(finalizer_thread_info->lock);
    finalizer_thread_info->shutdown = TRUE;
    gc_unlock(finalizer_thread_info->lock);
    ref_enqueue_shutdown();
    activate_finalizer_threads(FALSE);
}

static void wait_finalization_end(void)
{
    vm_wait_event(finalizer_thread_info->finalization_end_event);
}

void activate_finalizer_threads(Boolean wait)
{
    gc_lock(finalizer_thread_info->lock);
    vm_set_event(finalizer_thread_info->finalizer_pending_event,
                    finalizer_thread_info->thread_num - finalizer_thread_info->working_thread_num);
    gc_unlock(finalizer_thread_info->lock);
    
    if(wait)
        wait_finalization_end();
}

void vmmemory_manager_runfinalization(void)
{
    activate_finalizer_threads(TRUE);
}


static int do_finalization_func(void)
{
    return vm_do_finalization(0);
}

static void wait_pending_finalizer(void)
{
    vm_wait_event(finalizer_thread_info->finalizer_pending_event);
}

static void notify_finalization_end(void)
{
    vm_post_event(finalizer_thread_info->finalization_end_event);
}

static void finalizer_notify_work_done(void)
{
    gc_lock(finalizer_thread_info->lock);
    --finalizer_thread_info->working_thread_num;
    if(finalizer_thread_info->working_thread_num == 0)
        notify_finalization_end();
    gc_unlock(finalizer_thread_info->lock);
}

static int finalizer_thread_func(void **args)
{
    JavaVM *java_vm = (JavaVM *)args[0];
    JNIEnv *jni_env = (JNIEnv *)args[1];
    
    IDATA status = vm_attach(java_vm, &jni_env, NULL);
    if(status != TM_ERROR_NONE)
        return status;
    
    while(true){
        /* Waiting for pending finalizers */
        wait_pending_finalizer();
        
        gc_lock(finalizer_thread_info->lock);
        ++finalizer_thread_info->working_thread_num;
        gc_unlock(finalizer_thread_info->lock);
        
        /* do the real finalization work */
        do_finalization_func();
        
        finalizer_notify_work_done();
        
        gc_lock(finalizer_thread_info->lock);
        if(finalizer_thread_info->shutdown){
            gc_unlock(finalizer_thread_info->lock);
            break;
        }
        gc_unlock(finalizer_thread_info->lock);
    }

    return TM_ERROR_NONE;
}
