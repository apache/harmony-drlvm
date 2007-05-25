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
#include "finalize.h"
#include "vm_threads.h"
#include "init.h"
#include "open/jthread.h"
#include "jni_direct.h"
#include "jni_utils.h"
#include "slot.h"

static Boolean native_fin_thread_flag = FALSE;
static Fin_Thread_Info *fin_thread_info = NULL;
unsigned int cpu_num_bits;

static uint32 atomic_inc32(volatile apr_uint32_t *mem)
{  return (uint32)apr_atomic_inc32(mem); }

static uint32 atomic_dec32(volatile apr_uint32_t *mem)
{  return (uint32)apr_atomic_dec32(mem); }

Boolean get_native_finalizer_thread_flag()
{  return native_fin_thread_flag; }

void set_native_finalizer_thread_flag(Boolean flag)
{  native_fin_thread_flag = flag; }

Boolean get_finalizer_shutdown_flag()
{  return fin_thread_info->shutdown; }

Boolean get_finalizer_on_exit_flag()
{  return fin_thread_info->on_exit; }

static unsigned int coarse_log(unsigned int num)
{
    unsigned int i = sizeof(unsigned int) << 3;
    unsigned int comp = 1 << (i-1);
    while(i--){
        if(num & comp)
            return i;
        num = num << 1;
    }
    return 0;
}

static IDATA finalizer_thread_func(void **args);

static void set_fin_thread_attached(void)
{ fin_thread_info->thread_attached = 1; }

static void dec_fin_thread_num(void)
{ atomic_dec32(&fin_thread_info->thread_num); }

static volatile unsigned int get_fin_thread_attached(void)
{ return fin_thread_info->thread_attached; }

static void clear_fin_thread_attached(void)
{ fin_thread_info->thread_attached = 0; }

static void wait_fin_thread_attached(void)
{ while(!fin_thread_info->thread_attached){hythread_yield();}}


jobject get_system_thread_group(JNIEnv* jni_env)
{
    jclass thread_class = GetObjectClass(jni_env, jthread_self());
    jfieldID sysTG_field = GetStaticFieldID(jni_env, thread_class,
        "systemThreadGroup", "Ljava/lang/ThreadGroup;");
    assert(sysTG_field);
    return GetStaticObjectField(jni_env, thread_class, sysTG_field);
}

void finalizer_threads_init(JavaVM *java_vm, JNIEnv* jni_env)
{   if(!native_fin_thread_flag)
        return;
    
    fin_thread_info = (Fin_Thread_Info *)STD_MALLOC(sizeof(Fin_Thread_Info));
    fin_thread_info->thread_num = port_CPUs_number();
    cpu_num_bits = coarse_log(fin_thread_info->thread_num);
    fin_thread_info->working_thread_num = 0;
    fin_thread_info->end_waiting_num = 0;
    fin_thread_info->shutdown = FALSE;
    fin_thread_info->on_exit = FALSE;
    
    IDATA status = hysem_create(&fin_thread_info->pending_sem, 0, fin_thread_info->thread_num);
    assert(status == TM_ERROR_NONE);
    
    status = hycond_create(&fin_thread_info->end_cond);
    assert(status == TM_ERROR_NONE);
    status = hymutex_create(&fin_thread_info->end_mutex, TM_MUTEX_DEFAULT);
    assert(status == TM_ERROR_NONE);
    
    status = hycond_create(&fin_thread_info->mutator_block_cond);
    assert(status == TM_ERROR_NONE);
    status = hymutex_create(&fin_thread_info->mutator_block_mutex, TM_MUTEX_DEFAULT);
    assert(status == TM_ERROR_NONE);
    
    fin_thread_info->thread_ids = (hythread_t *)STD_MALLOC(sizeof(hythread_t) * fin_thread_info->thread_num);
    
    for(unsigned int i = 0; i < fin_thread_info->thread_num; i++){
        void **args = (void **)STD_MALLOC(sizeof(void *) * 3);
        args[0] = (void *)java_vm;
        args[1] = (void *)(UDATA)(i + 1);
        args[2] = (void*)get_system_thread_group(jni_env);
        fin_thread_info->thread_ids[i] = NULL;
        clear_fin_thread_attached();
        status = hythread_create(&fin_thread_info->thread_ids[i], 0, FINALIZER_THREAD_PRIORITY, 0, (hythread_entrypoint_t)finalizer_thread_func, args);
        assert(status == TM_ERROR_NONE);
        wait_fin_thread_attached();
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
        fin_thread_info->on_exit = TRUE;
        activate_finalizer_threads(TRUE);
    }
    fin_thread_info->shutdown = TRUE;
    ref_enqueue_shutdown();
    activate_finalizer_threads(TRUE);
}

void wait_native_fin_threads_detached(void)
{
    hymutex_lock(&fin_thread_info->end_mutex);
    while(fin_thread_info->thread_num){
        atomic_inc32(&fin_thread_info->end_waiting_num);
        IDATA status = hycond_wait_timed(&fin_thread_info->end_cond, &fin_thread_info->end_mutex, (I_64)1000, 0);
        atomic_dec32(&fin_thread_info->end_waiting_num);
        if(status != TM_ERROR_NONE) break;
    }
    hymutex_unlock(&fin_thread_info->end_mutex);
}

/* Restrict waiting time; Unit: msec */
static unsigned int restrict_wait_time(unsigned int wait_time, unsigned int max_time)
{
    if(!wait_time) return 1;
    if(wait_time > max_time) return max_time;
    return wait_time;
}

static void wait_finalization_end(Boolean must_wait)
{
    hymutex_lock(&fin_thread_info->end_mutex);
    unsigned int fin_obj_num = vm_get_finalizable_objects_quantity();
    while(fin_thread_info->working_thread_num || fin_obj_num){
        unsigned int wait_time = restrict_wait_time(fin_obj_num + 100, FIN_MAX_WAIT_TIME << 7);
        atomic_inc32(&fin_thread_info->end_waiting_num);
        IDATA status = hycond_wait_timed(&fin_thread_info->end_cond, &fin_thread_info->end_mutex, (I_64)wait_time, 0);
        atomic_dec32(&fin_thread_info->end_waiting_num);
        unsigned int temp = vm_get_finalizable_objects_quantity();
        if(must_wait){
            if((status != TM_ERROR_NONE) && (fin_obj_num == temp)) break;
        }else{
            if(status != TM_ERROR_NONE) break; 
        }
        fin_obj_num = temp;     
    }
    hymutex_unlock(&fin_thread_info->end_mutex);
}

void activate_finalizer_threads(Boolean wait)
{
    IDATA stat = hysem_set(fin_thread_info->pending_sem, fin_thread_info->thread_num);
    assert(stat == TM_ERROR_NONE);
    
    if(wait)
        wait_finalization_end(true);
}

static void notify_finalization_end(void)
{
    if(vm_get_finalizable_objects_quantity()==0 && fin_thread_info->working_thread_num==0)
        hycond_notify_all(&fin_thread_info->end_cond);
}

static void wait_pending_finalizer(void)
{
    IDATA stat = hysem_wait(fin_thread_info->pending_sem);
    assert(stat == TM_ERROR_NONE);
}

extern jint set_current_thread_context_loader(JNIEnv* jni_env);

static IDATA finalizer_thread_func(void **args)
{
    JavaVM *java_vm = (JavaVM *)args[0];
    JNIEnv *jni_env;
    char *name = "finalizer";
    // FIXME: use args[1] (thread number) to distinguish finalization threads by name

    JavaVMAttachArgs *jni_args = (JavaVMAttachArgs*)STD_MALLOC(sizeof(JavaVMAttachArgs));
    jni_args->version = JNI_VERSION_1_2;
    jni_args->name = name;
    jni_args->group = (jobject)args[2];
    IDATA status = AttachCurrentThreadAsDaemon(java_vm, (void**)&jni_env, jni_args);
    assert(status == JNI_OK);
    set_current_thread_context_loader(jni_env);
    assert(!get_fin_thread_attached());
    set_fin_thread_attached();
    
    /* Choice: use VM_thread or hythread to indicate the finalizer thread ?
     * Now we use hythread
     * p_TLS_vmthread->finalize_thread_flags = thread_id;
     */
    
    while(true){
        /* Waiting for pending finalizers */
        wait_pending_finalizer();
        
        /* do the real finalization work */
        atomic_inc32(&fin_thread_info->working_thread_num);
        vm_do_finalization(0);
        atomic_dec32(&fin_thread_info->working_thread_num);
        
        vm_heavy_finalizer_resume_mutator();
        
        if(fin_thread_info->end_waiting_num)
            notify_finalization_end();
        
        if(fin_thread_info->shutdown)
            break;
    }
    
    vm_heavy_finalizer_resume_mutator();
    status = DetachCurrentThread(java_vm);
    assert(status == JNI_OK);
    dec_fin_thread_num();
    //status = jthread_detach(java_thread);
    return status;
}


/* Heavy finalizable object load handling mechanism */

/* Choice: use VM_thread or hythread to indicate the finalizer thread ?
 * Now we use hythread
 * p_TLS_vmthread->finalize_thread_flags = thread_id;
 */
static Boolean self_is_finalizer_thread(void)
{
    hythread_t self = hythread_self();
    
    for(unsigned int i=0; i<fin_thread_info->thread_num; ++i)
        if(self == fin_thread_info->thread_ids[i])
            return TRUE;
    
    return FALSE;
}

void vm_heavy_finalizer_block_mutator(void)
{
    /* Maybe self test is not needed. This needs further test. */
    if(self_is_finalizer_thread())
        return;
    
    hymutex_lock(&fin_thread_info->mutator_block_mutex);
    unsigned int fin_obj_num = vm_get_finalizable_objects_quantity();
    fin_obj_num = fin_obj_num >> (MUTATOR_BLOCK_THRESHOLD_BITS + cpu_num_bits);
    unsigned int wait_time = restrict_wait_time(fin_obj_num, FIN_MAX_WAIT_TIME);
    if(fin_obj_num)
        IDATA status = hycond_wait_timed_raw(&fin_thread_info->mutator_block_cond, &fin_thread_info->mutator_block_mutex, wait_time, 0);
    hymutex_unlock(&fin_thread_info->mutator_block_mutex);
}

void vm_heavy_finalizer_resume_mutator(void)
{
    if(gc_clear_mutator_block_flag())
        hycond_notify_all(&fin_thread_info->mutator_block_cond);
}








