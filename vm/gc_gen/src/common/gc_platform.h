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
 * @author Xiao-Feng Li, 2006/10/05
 */

#ifndef _GC_PLATFORM_H_
#define _GC_PLATFORM_H_


#include <apr_time.h>
#include <apr_atomic.h>

#include <open/hythread_ext.h>

#define USEC_PER_SEC INT64_C(1000000)

#define VmThreadHandle  void*
#define VmEventHandle   hysem_t
#define THREAD_OK       TM_ERROR_NONE

inline int vm_wait_event(VmEventHandle event)
{   IDATA stat = hysem_wait(event);
    assert(stat == TM_ERROR_NONE); return stat;
}

inline int vm_set_event(VmEventHandle event)
{   IDATA stat = hysem_post(event);
    assert(stat == TM_ERROR_NONE); return stat;
}

inline int vm_reset_event(VmEventHandle event)
{   IDATA stat = hysem_set(event,0);
    assert(stat == TM_ERROR_NONE); return stat;
}

inline int vm_create_event(VmEventHandle* event, unsigned int initial_count, unsigned int max_count)
{
  return hysem_create(event, initial_count, max_count);
}

inline void vm_thread_yield()
{
  hythread_yield();
}

inline int vm_create_thread(void* ret_thread, unsigned int stacksize, unsigned int priority, unsigned int suspend, int (*func)(void*), void *data)
{ 
  return hythread_create((hythread_t*)ret_thread, (UDATA)stacksize, (UDATA)priority, (UDATA)suspend, 
                             (hythread_entrypoint_t)func, data);
}

inline void *atomic_casptr(volatile void **mem, void *with, const void *cmp) {
  return apr_atomic_casptr(mem, with, cmp);
}

inline uint32 atomic_cas32(volatile apr_uint32_t *mem,
                                           apr_uint32_t swap,
                                           apr_uint32_t cmp) {
  return (uint32)apr_atomic_cas32(mem, swap, cmp);
}

inline uint32 atomic_inc32(volatile apr_uint32_t *mem){
  return (uint32)apr_atomic_inc32(mem);
}

inline uint32 atomic_dec32(volatile apr_uint32_t *mem){
  return (uint32)apr_atomic_dec32(mem);
}

inline uint32 atomic_add32(volatile apr_uint32_t *mem, apr_uint32_t val) {
  return (uint32)apr_atomic_add32(mem, val);
}

inline Boolean pool_create(apr_pool_t **newpool, apr_pool_t *parent) {
  return (Boolean)apr_pool_create(newpool, parent);
}

inline void pool_destroy(apr_pool_t *p) {
  apr_pool_destroy(p);
}


inline int64 time_now() {
  return apr_time_now();
}

typedef volatile unsigned int SpinLock;

enum Lock_State{
  FREE_LOCK,
  LOCKED
};

#define try_lock(x) (!atomic_cas32(&(x), LOCKED, FREE_LOCK))
#define lock(x) while( !try_lock(x)){ while( x==LOCKED );}
#define unlock(x) do{ x = FREE_LOCK;}while(0)

#endif //_GC_PLATFORM_H_
