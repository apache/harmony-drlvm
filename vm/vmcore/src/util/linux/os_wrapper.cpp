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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

#include "platform.h"
#include "port_malloc.h"

#ifndef __SMP__
#error
-- recompile with -D__SMP__
#endif

#ifndef _REENTRANT
#error
-- recompile with -D_REENTRANT
#endif

#ifndef __SIGRTMIN
#else
#if __SIGRTMAX - __SIGRTMIN >= 3
// good, this will work. java dbg, also vm can use SIGUSR1, SIGUSR2
#else
#error
-- must be using an old version of pthreads
-- which uses SIGUSR1, SIGUSR2 (which conflicts with the java app debugger and vm)
#endif
#endif


pthread_mutexattr_t mutex_attr;
pthread_mutexattr_t mutex_attr_for_cond_wait;

pthread_condattr_t cond_attr;

pthread_attr_t pthread_attr;
//extern pthread_key_t TLS_key_pvmthread;
void init_linux_thread_system()
{
    static bool initialized = false;

    if (initialized) return;
    initialized = true;
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP);

    int UNUSED stat = pthread_condattr_init(&cond_attr);
    assert(stat == 0);

    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);

    //pthread_key_create(&TLS_key_pvmthread, 0);
}

VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    init_linux_thread_system();
    assert(lpCriticalSection);
    int UNUSED xx = pthread_mutex_init(lpCriticalSection, &mutex_attr);
    assert(xx == 0);
}


VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    assert(lpCriticalSection);
}


VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    assert(lpCriticalSection);
    int UNUSED xx = pthread_mutex_unlock(lpCriticalSection);
    assert(xx == 0);
}


BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    assert(lpCriticalSection);
    int xx = pthread_mutex_trylock(lpCriticalSection);

    if (xx == 0)
        return 1;

    assert(xx == EBUSY);
    return 0;
}


BOOL EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    assert(lpCriticalSection);
    int xx = pthread_mutex_lock(lpCriticalSection);

    if (xx == 0)
        return 1;

    return 0;
}


VmThreadHandle vm_beginthreadex( void * UNREF security,  unsigned UNREF stacksize, unsigned(__stdcall *start_address)(void *), void *arglist, unsigned UNREF initflag, pthread_t *thrdaddr)
{ 
  pthread_t tid = 0;

  void *(*sa)(void *) = ( void *(*)(void *) )start_address;
  int stat = pthread_create(&tid, &pthread_attr, sa, arglist); 
  *thrdaddr = tid;
  if(stat !=0) {
    return 0;
  }
  return (VmThreadHandle)tid;
}

VmThreadHandle vm_beginthread(void (__cdecl *start_address)(void *), unsigned UNREF stacksize, void *arglist)
{ 
  pthread_t tid = 0;

  void *(*sa)(void *) = ( void *(*)(void *) )start_address;
  int stat = pthread_create(&tid, &pthread_attr, sa, arglist); 
  if(stat !=0) {
    return 0;
  }
  
  return (VmThreadHandle)tid;
}

static void init_timespec(struct timespec & ts, DWORD dwMillisec)
{
    ts.tv_sec  = dwMillisec/1000;
    ts.tv_nsec = (dwMillisec%1000)*1000000;

    struct timeval tv;
    int UNUSED stat = gettimeofday(&tv, 0);
    assert(stat == 0);

    ts.tv_sec  += tv.tv_sec;
    ts.tv_nsec += tv.tv_usec*1000;

    ts.tv_sec += (ts.tv_nsec/1000000000);
    ts.tv_nsec = (ts.tv_nsec%1000000000);
}

VmEventHandle vm_create_event(int * UNREF security, unsigned int man_reset_flag, 
                         unsigned int initial_state_flag, char * UNREF p_name)
{
    event_wrapper *p_event = (event_wrapper *)STD_MALLOC( sizeof(struct event_wrapper) );

    int stat = pthread_mutex_init(&p_event->mutex, &mutex_attr_for_cond_wait);
    assert(stat == 0);
  
    stat = pthread_cond_init(&p_event->cond, &cond_attr);
    assert(stat == 0);

    p_event->man_reset_flag = man_reset_flag;
    p_event->state = initial_state_flag;
    p_event->n_wait = 0;

    return (VmEventHandle)p_event;
}

BOOL vm_destroy_event(VmEventHandle hEvent)
{
    struct timespec ts;
    event_wrapper *p_event = (event_wrapper *)hEvent;

    int xx = pthread_mutex_lock(&p_event->mutex);
    assert(xx == 0);
    // no threads in the wait set
    if (p_event->n_wait == 0) goto success;

    // no chance to release waited threads
    if (p_event->state == 0) goto fail;

    if (p_event->man_reset_flag == 0) {
        // event is in automatic state so not more than one thread can be released
        if (p_event->n_wait > 1) goto fail;
        p_event->man_reset_flag = 1;
    }

    init_timespec(ts, 1000);

    do {
        int stat = pthread_cond_timedwait(&p_event->cond, &p_event->mutex, &ts);
        assert(stat != EINVAL);
        // ensure that the stat is still signaling
        if (stat == ETIMEDOUT && p_event->state == 0 && p_event->n_wait != 0)
            goto fail;
    } while (p_event->n_wait != 0);

success:
    xx = pthread_mutex_unlock(&p_event->mutex);
    assert(xx == 0);
    STD_FREE(p_event);
    return 1;

fail:
    xx = pthread_mutex_unlock(&p_event->mutex);
    assert(xx == 0);
    return 0;
}

BOOL vm_reset_event(VmEventHandle hEvent)
{
  event_wrapper *p_event = (event_wrapper *)hEvent;

  assert(p_event);
  int xx = pthread_mutex_lock(&p_event->mutex);
  assert(xx == 0);
  p_event->state = 0;
  xx = pthread_mutex_unlock(&p_event->mutex);
  assert(xx == 0);
  return 1;
}


BOOL vm_set_event(VmEventHandle hEvent)
{
  event_wrapper *p_event = (event_wrapper *)hEvent;

  assert(p_event);
  int stat = pthread_mutex_lock(&p_event->mutex);
  assert(stat == 0);

  p_event->state = 1;

  stat = pthread_cond_broadcast(&p_event->cond);
  assert(stat == 0);

  stat = pthread_mutex_unlock(&p_event->mutex);
  assert(stat == 0);
  return 1;
}

void vm_yield() {
    sched_yield();
}

DWORD vm_wait_for_single_object(VmEventHandle hHandle, DWORD dwMillisec)
{
  if (dwMillisec < 10)
    return WAIT_TIMEOUT;

  struct timespec ts;
  init_timespec(ts, dwMillisec);

  event_wrapper *p_event = (event_wrapper *)hHandle;
  assert(p_event);

  int stat = pthread_mutex_lock(&p_event->mutex);
  assert(stat == 0);

  int wait_status = WAIT_OBJECT_0;

  while (1)
    {
      if (p_event->state != 0)
          break;
      
      ++p_event->n_wait;
      wait_status = pthread_cond_timedwait(&p_event->cond, 
                                           &p_event->mutex,
                                           &ts );
      --p_event->n_wait;

      assert(wait_status != EINVAL);
      if (wait_status == ETIMEDOUT)
        break;
      if (wait_status == 0)
        break;
    }

  if (p_event->man_reset_flag == 0)
      p_event->state = 0;

  stat = pthread_mutex_unlock(&p_event->mutex);
  assert(stat == 0);

  if (wait_status == ETIMEDOUT)
    return WAIT_TIMEOUT;

  if (wait_status == 0)
      return WAIT_OBJECT_0;

  return WAIT_TIMEOUT;  
}

DWORD vm_wait_for_multiple_objects(DWORD numobj, const VmEventHandle *hHandle, BOOL waitAll, DWORD dwMillisec)
{
    DWORD ret = 0;
    assert(waitAll);
    assert(dwMillisec == INFINITE);
    
    for(unsigned int i = 0; i < numobj; i++) {
        ret = vm_wait_for_single_object((VmEventHandle)hHandle[i], dwMillisec);
        if (ret != WAIT_OBJECT_0)
            return ret;
    }
    return ret;
}

void Sleep(DWORD msec)
{
  if (msec < 1000){
    sched_yield();
    return;
  }

  unsigned int zz = msec/1000;

  sleep(zz);

}


void SleepEx(DWORD time, bool UNREF bb)
{
  Sleep(time);
}

void vm_endthreadex(int UNREF zz)
{
    // on linux, this is a nop,
    // call_the_run_method() simply returns into the linux kernel
}

void vm_endthread(void )
{
    // on linux, this is a nop,
    // call_the_run_method() simply returns into the linux kernel
}

/****************************************
 *  Moved from stubs.h
 *  Some function need to be implemented
 ****************************************/
 
pthread_t GetCurrentThreadId (void)
{
    return pthread_self();
}

/*
 * Need to implement (?)
 */
BOOL DuplicateHandle(VmEventHandle UNREF aa, VmEventHandle UNREF bb, 
                     VmEventHandle UNREF cc, VmEventHandle * UNREF dd,
                     DWORD UNREF ee, BOOL UNREF ff, DWORD UNREF gg)
{
  return TRUE;
}

/*
 * Need to implement (?)
 */
VmEventHandle GetCurrentProcess(void)
{
  return 0;
}

/*
 * Need to implement (?)
 */
VmEventHandle GetCurrentThread(void)
{
  return 0;
}
