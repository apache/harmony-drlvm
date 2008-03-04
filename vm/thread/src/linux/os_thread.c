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

#include <assert.h>
#include <apr_atomic.h>
#include <sched.h>		// sched_param
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "port_thread.h"
#include "thread_private.h"


/**
 * Creates new thread.
 *
 * @param[out] handle on success, thread handle is stored in memory pointed by handle
 * @param stacksize size of stack to be allocated for a new thread
 * @param priority priority of a new thread
 * @param func function to be started on a new thread
 * @param data value to be passed to a function started on a new thread
 *
 * @return 0 on success, TM_ERROR_OUT_OF_MEMORY if system is thread cannot be created because
 *         of insufficient memory, system error otherwise.
 */
int os_thread_create(/* out */osthread_t* phandle, UDATA stacksize, UDATA priority,
        hythread_wrapper_t func, void *data)
{
    pthread_t thread;
    pthread_attr_t attr;
    int r;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (stacksize != 0) {
	r = pthread_attr_setstacksize(&attr, stacksize);
	if (r) {
	    pthread_attr_destroy(&attr);
	    return r;
	}
    }

    r = pthread_create(&thread, &attr, (void*(*)(void*))func, data);

    pthread_attr_destroy(&attr);

    if (r == 0) {
        *phandle = thread;
	// priority is set from within the thread context
        return 0;
    } else {
        if (r == EAGAIN || r == ENOMEM) {
	    // EAGAIN may be returned if PTHREAD_THREADS_MAX limit is exceeded
            return TM_ERROR_OUT_OF_MEMORY;
	}
        return r;
    }
}

/**
 * Adjusts priority of the running thread.
 *
 * @param thread        handle of thread
 * @param priority      new priority value
 *
 * @return              0 on success, system error otherwise
 */
int os_thread_set_priority(osthread_t os_thread, int priority)
{
#if defined(FREEBSD)
    /* Not sure why we don't just use this on linux? - MRH */
    struct sched_param param;
    int policy;
    int r = pthread_getschedparam(os_thread, &policy, &param);
    if (r == 0) {
        param.sched_priority = priority;
        r = pthread_setschedparam(os_thread, policy, &param);
    }
    return r;
#else
    // setting thread priority on linux is only supported for current thread
    if (os_thread == pthread_self()) {
	int r;
	struct sched_param param;
	pid_t self = gettid();
	param.sched_priority = priority;
	r = sched_setparam(self, &param);
	return r ? errno : 0;
    } else {
        // setting other thread priority not supported on linux
        return 0;
    }
#endif
}

/**
 * Returns os handle of the current thread.
 *
 * @return current thread handle on success, NULL on error
 */
osthread_t os_thread_current()
{
    return pthread_self();
}

/**
 * Not used on Linux
 *
 * @param os_thread     thread handle
 *
 * @return              0 on success, systerm error otherwise
 */
int os_thread_free(osthread_t os_thread)
{
    return 0;
}

/**
 * Joins the os thread.
 *
 * @param os_thread     thread handle
 *
 * @return              0 on success, systerm error otherwise
 */
int os_thread_join(osthread_t os_thread)
{
    int error;

    do {
        // FIXME - somehow pthread_join returns before thread is terminated
        error = pthread_join(os_thread, NULL);
    } while (error != ESRCH && error != EINVAL && error != EDEADLK);
    return 0;
}

/**
 * Causes the current thread to stop execution.
 *
 * @param status        returns status of a thread
 */
void os_thread_exit(IDATA status)
{
    pthread_exit((void*)status);
}

/**
 * Queries amount of user and kernel times consumed by the thread,
 * in nanoseconds.
 *
 * @param os_thread     thread handle
 * @param[out] pkernel  a pointer to a variable to store kernel time to
 * @param[out] puser    a pointer to a variable to store user time to
 *
 * @return      0 on success, system error otherwise
 */
int os_get_thread_times(osthread_t os_thread, int64* pkernel, int64* puser)
{
    clockid_t clock_id;
    struct timespec tp;
    int r;
#ifdef FREEBSD
    return EINVAL; /* TOFIX: Implement */
#else

    r = pthread_getcpuclockid(os_thread, &clock_id);
    if (r) return r;

    r = clock_gettime(clock_id, &tp);
    if (r) return r;

    *puser = tp.tv_sec * 1000000000ULL + tp.tv_nsec;
    return 0;
#endif
}

UDATA os_get_foreign_thread_stack_size() {
    int err;
    void* stack_addr;
    pthread_attr_t pthread_attr;
    size_t stack_size;

    static UDATA common_stack_size = -1;

    if (common_stack_size == -1) {
	    pthread_attr_init(&pthread_attr);
	    err = pthread_attr_getstacksize(&pthread_attr, &common_stack_size);
	    pthread_attr_destroy(&pthread_attr);
    }

    return common_stack_size;

}
