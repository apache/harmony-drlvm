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

#ifndef _FINALIZER_THREAD_H_
#define _FINALIZER_THREAD_H_

#include "jni_types.h"
#include "open/hythread_ext.h"
#include <assert.h>
#include "open/types.h"
#include <apr_atomic.h>

#ifndef _FINALIZER_WEAKREF_PLATFORM_
#define _FINALIZER_WEAKREF_PLATFORM_

#define VmEventHandle   hysem_t

inline int vm_wait_event(VmEventHandle event)
{
    IDATA stat = hysem_wait(event);
    assert(stat == TM_ERROR_NONE); return stat;
}
inline int vm_set_event(VmEventHandle event, IDATA count)
{
    IDATA stat = hysem_set(event, count);
    assert(stat == TM_ERROR_NONE); return stat;
}
inline int vm_post_event(VmEventHandle event)
{
    IDATA stat = hysem_set(event, 1);
    assert(stat == TM_ERROR_NONE); return stat;
}
inline int vm_create_event(VmEventHandle* event, unsigned int initial_count, unsigned int max_count)
{
    return hysem_create(event, initial_count, max_count);
}


typedef volatile unsigned int SpinLock;
enum Lock_State{
  FREE_LOCK,
  LOCKED
};

#define gc_try_lock(x) (!apr_atomic_cas32(&(x), LOCKED, FREE_LOCK))
#define gc_lock(x) while( !gc_try_lock(x)){ while( x==LOCKED );}
#define gc_unlock(x) do{ x = FREE_LOCK;}while(0)

#endif

#define FINALIZER_THREAD_PRIORITY (HYTHREAD_PRIORITY_USER_MAX - 3)

struct finalizer_thread_info {
    SpinLock lock;
    int thread_num;
    int working_thread_num;
    Boolean shutdown;
    Boolean on_exit;
    VmEventHandle finalizer_pending_event;
    VmEventHandle finalization_end_event;
};


extern Boolean get_finalizer_shutdown_flag();
extern Boolean get_finalizer_on_exit_flag();
extern void finalizer_threads_init(JavaVM *java_vm, JNIEnv *jni_env);
extern void finalizer_shutdown(Boolean start_finalization_on_exit);
extern void activate_finalizer_threads(Boolean wait);
extern void vmmemory_manager_runfinalization(void);

#endif // _FINALIZER_THREAD_H_
