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
#if defined(LINUX)
#include <linux/unistd.h>	// gettid()
#endif
#include <sched.h>		// sched_param
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "thread_private.h"

#ifdef LINUX
#ifdef _syscall0
_syscall0(pid_t,gettid)
pid_t gettid(void);
#else
static pid_t gettid(void)
{
    return (pid_t)syscall(__NR_gettid);
}
#endif
#endif

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
 * Joins the os thread.
 *
 * @param os_thread     thread handle
 *
 * @return              0 on success, systerm error otherwise
 */
int os_thread_join(osthread_t os_thread)
{
    void *status;
    return pthread_join(os_thread, &status);
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
 * Calculates absolute time in future for sem_timedwait timeout.
 * @param ptime The pointer to time structure to fill
 * @param delay Desired timeout in ns; not greater than 10^9 (1s)
 */
static inline __attribute__((always_inline))
void get_exceed_time(struct timespec* ptime, long delay)
{
    clock_gettime(CLOCK_REALTIME, ptime);

    ptime->tv_nsec += delay;
    if (ptime->tv_nsec >= 1000000000L) // overflow
    {
        ptime->tv_nsec -= 1000000000L;
        ++ptime->tv_sec;
    }
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


typedef enum
{
    THREADREQ_NONE = 0,
    THREADREQ_SUS = 1,
    THREADREQ_RES = 2,
    THREADREQ_YIELD = 3
} os_suspend_req_t;

typedef struct os_thread_info_t os_thread_info_t;

struct os_thread_info_t
{
    osthread_t              thread;
    int                     suspend_count;
    sem_t                   wake_sem;       /* to sem_post from signal handler */
    os_thread_context_t     context;

    os_thread_info_t*       next;
};


/* Global mutex to syncronize access to os_thread_info_t list */
static pthread_mutex_t g_suspend_mutex;
/* Global list with suspended threads info */
static os_thread_info_t* g_suspended_list;
/* request type for signal handler */
os_suspend_req_t g_req_type;
/* The thread which is processed */
static osthread_t g_suspendee;
/* Semaphore used to inform signal sender about signal delivery */
static sem_t g_yield_sem;


/* Forward declarations */
static int suspend_init();
static int suspend_init_lock();
static os_thread_info_t* init_susres_list_item();
static os_thread_info_t* suspend_add_thread(osthread_t thread);
static void suspend_remove_thread(osthread_t thread);
static os_thread_info_t* suspend_find_thread(osthread_t thread);
static void sigusr2_handler(int signum, siginfo_t* info, void* context);


/**
 * Terminates the os thread.
 */
int os_thread_cancel(osthread_t os_thread)
{
    int status;
    os_thread_info_t* pinfo;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(os_thread);
    status = pthread_cancel(os_thread);

    if (pinfo && status == 0)
        suspend_remove_thread(os_thread);

    pthread_mutex_unlock(&g_suspend_mutex);
    return status;
}

/**
* Sends a signal to a thread to make sure thread's write
 * buffers are flushed.
 */
void os_thread_yield_other(osthread_t os_thread) {
    struct timespec timeout;
    os_thread_info_t* pinfo;

    if (!suspend_init_lock())
        return;

    pinfo = suspend_find_thread(os_thread);

    if (pinfo && pinfo->suspend_count > 0) {
        pthread_mutex_unlock(&g_suspend_mutex);
        return;
    }

    g_suspendee = os_thread;
    g_req_type = THREADREQ_YIELD;

    assert(os_thread);
    if (pthread_kill(os_thread, SIGUSR2) == 0) {
        // signal sent, let's do timed wait to make sure the signal
        // was actually delivered
		get_exceed_time(&timeout, 1000000L);
        sem_timedwait(&g_yield_sem, &timeout);
    } else {
        if (pinfo)
            suspend_remove_thread(os_thread);
    }

    g_req_type = THREADREQ_NONE;
    pthread_mutex_unlock(&g_suspend_mutex);
}


/**
 * Suspend given thread
 * @param thread The thread to suspend
 */
int os_thread_suspend(osthread_t thread)
{
    int status;
    os_thread_info_t* pinfo;

    if (!thread)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
        pinfo = suspend_add_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return TM_ERROR_OUT_OF_MEMORY;
    }

    if (pinfo->suspend_count > 0)
    {
        ++pinfo->suspend_count;
        pthread_mutex_unlock(&g_suspend_mutex);
        return TM_ERROR_NONE;
    }

    g_suspendee = thread;
    g_req_type = THREADREQ_SUS;

    if (pthread_kill(thread, SIGUSR2) != 0)
    {
        suspend_remove_thread(thread);
        pthread_mutex_unlock(&g_suspend_mutex);
        return TM_ERROR_INTERNAL;
    }

    /* Waiting for suspendee response */
    sem_wait(&pinfo->wake_sem);
    /* Check result */
    status = (pinfo->suspend_count > 0) ? TM_ERROR_NONE : TM_ERROR_INTERNAL;

    pthread_mutex_unlock(&g_suspend_mutex);
    return status;
}

/**
 * Resume given thread
 * @param thread The thread to resume
 */
int os_thread_resume(osthread_t thread)
{
    int status;
    os_thread_info_t* pinfo;

    if (!thread)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return TM_ERROR_UNATTACHED_THREAD;
    }

    if (pinfo->suspend_count > 1)
    {
        --pinfo->suspend_count;
        pthread_mutex_unlock(&g_suspend_mutex);
        return TM_ERROR_NONE;
    }

    g_suspendee = thread;
    g_req_type = THREADREQ_RES;

    if ((status = pthread_kill(thread, SIGUSR2)) != 0)
    {
        suspend_remove_thread(thread);
        pthread_mutex_unlock(&g_suspend_mutex);
        return status;
    }

    /* Waiting for resume notification */
    sem_wait(&pinfo->wake_sem);

    suspend_remove_thread(thread);

    pthread_mutex_unlock(&g_suspend_mutex);
    return TM_ERROR_NONE;
}

/**
 * Determine suspend count for the given thread
 * @param thread The thread to check
 * @return -1 if error have occured
 */
int os_thread_get_suspend_count(osthread_t thread)
{
    os_thread_info_t* pinfo;
    int suspend_count;

    if (!thread)
        return -1;

    if (!suspend_init_lock())
        return -1;

    pinfo = suspend_find_thread(thread);

    suspend_count = pinfo ? pinfo->suspend_count : 0;

    pthread_mutex_unlock(&g_suspend_mutex);
    return suspend_count;
}

/**
 * Get context for given thread
 * @param thread The thread to process
 * @param context Pointer to platform-dependant context structure
 * @note The thread must be suspended
 */
int os_thread_get_context(osthread_t thread, os_thread_context_t *context)
{
    int status = TM_ERROR_UNATTACHED_THREAD;
    os_thread_info_t* pinfo;

    if (!thread || !context)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return status;
    }

    if (pinfo->suspend_count > 0)
    {
        *context = pinfo->context;
        status = TM_ERROR_NONE;
    }

    pthread_mutex_unlock(&g_suspend_mutex);
    return status;
}

/**
 * Set context for given thread
 * @param thread The thread to process
 * @param context Pointer to platform-dependant context structure
 * @note The thread must be suspended
 */
int os_thread_set_context(osthread_t thread, os_thread_context_t *context)
{
    int status = TM_ERROR_UNATTACHED_THREAD;
    os_thread_info_t* pinfo;

    if (!thread || !context)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return status;
    }

    if (pinfo->suspend_count > 0)
    {
        pinfo->context = *context;
        status = TM_ERROR_NONE;
    }

    pthread_mutex_unlock(&g_suspend_mutex);
    return status;
}


static int suspend_init()
{
    static int initialized = 0;
    struct sigaction sa;
    static pthread_mutex_t suspend_init_mutex = PTHREAD_MUTEX_INITIALIZER;

    if (initialized)
        return 1;

    pthread_mutex_lock(&suspend_init_mutex);

    if (!initialized)
    {
        /* Initialize all nesessary objects */
        int status;
        pthread_mutex_t mut_init = PTHREAD_MUTEX_INITIALIZER;

        status = sem_init(&g_yield_sem, 0, 0);

        if (status != 0)
        {
            pthread_mutex_unlock(&suspend_init_mutex);
            return 0;
        }

        g_suspend_mutex = mut_init;
        pthread_mutex_init(&g_suspend_mutex, NULL);

        g_suspended_list = NULL;
        g_req_type = THREADREQ_NONE;

        /* set signal handler */
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sa.sa_sigaction = sigusr2_handler;
        sigaction(SIGUSR2, &sa, NULL);

        initialized = 1;
    }

    pthread_mutex_unlock(&suspend_init_mutex);
    return 1;
}

static int suspend_init_lock()
{
    if (!suspend_init())
        return 0;

    if (pthread_mutex_lock(&g_suspend_mutex) != 0)
        return 0;

    return 1;
}

static os_thread_info_t* init_susres_list_item()
{
    os_thread_info_t* pinfo =
        (os_thread_info_t*)malloc(sizeof(os_thread_info_t));

    if (pinfo == NULL)
        return NULL;

    pinfo->suspend_count = 0;

    int status = sem_init(&pinfo->wake_sem, 0, 0);

    if (status != 0)
    {
        free(pinfo);
        return NULL;
    }

    return pinfo;
}

static os_thread_info_t* suspend_add_thread(osthread_t thread)
{
    os_thread_info_t* pinfo = init_susres_list_item();

    if (pinfo == NULL)
        return NULL;

    pinfo->thread = thread;
    pinfo->next = g_suspended_list;
    g_suspended_list = pinfo;
    return pinfo;
}

static void suspend_remove_thread(osthread_t thread)
{
    os_thread_info_t** pprev = &g_suspended_list;
    os_thread_info_t* pinfo;

    for (pinfo = g_suspended_list; pinfo; pinfo = pinfo->next)
    {
        if (pinfo->thread == thread)
            break;

        pprev = &pinfo->next;
    }

    if (pinfo != NULL)
    {
        sem_destroy(&pinfo->wake_sem);
        *pprev = pinfo->next;
        free(pinfo);
    }
}

static os_thread_info_t* suspend_find_thread(osthread_t thread)
{
    os_thread_info_t* pinfo;
    int status;

    for (pinfo = g_suspended_list; pinfo; pinfo = pinfo->next)
    {
        if (pinfo->thread == thread)
            break;
    }

    return pinfo;
}


static void sigusr2_handler(int signum, siginfo_t* info, void* context)
{
    int status;
    os_thread_info_t* pinfo;

    if (!suspend_init())
        return;

    if (signum != SIGUSR2)
        return;

    /* We have g_suspend_mutex locked already */

    if (g_req_type == THREADREQ_YIELD)
    {
        g_req_type = THREADREQ_NONE;
        /* Inform requester */
        sem_post(&g_yield_sem);
        return;
    }

    if ((pinfo = suspend_find_thread(g_suspendee)) == NULL)
        return;

    if (g_req_type == THREADREQ_SUS)
    {
        pinfo->suspend_count++;
        g_req_type = THREADREQ_NONE;
        memcpy(&pinfo->context, context, sizeof(ucontext_t));
        /* Inform suspender */
        sem_post(&pinfo->wake_sem);

        do
        {
            sigset_t sig_set;
            sigemptyset(&sig_set);
            sigsuspend(&sig_set);

        } while (pinfo->suspend_count > 0);

        /* We have returned from THREADREQ_RES handler */
        memcpy(context, &pinfo->context, sizeof(ucontext_t));
        /* Inform suspender */
        sem_post(&pinfo->wake_sem);
        return;
    }
    else if (g_req_type == THREADREQ_RES)
    {
        pinfo->suspend_count--;
        g_req_type = THREADREQ_NONE;
        return; /* Return to interrupted THREADREQ_SUS handler */
    }
}
