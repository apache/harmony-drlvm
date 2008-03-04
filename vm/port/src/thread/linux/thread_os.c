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

#define  _GNU_SOURCE
#include <assert.h>
#include <sched.h>		// sched_param
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include "port_thread.h"



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
    thread_context_t        context;

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
 * Terminates the os thread.
 */
int port_thread_cancel(osthread_t os_thread)
{
    int status;
    os_thread_info_t* pinfo;

    if (!suspend_init_lock())
        return -1;

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
void port_thread_yield_other(osthread_t os_thread) {
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
int port_thread_suspend(osthread_t thread)
{
    int status;
    os_thread_info_t* pinfo;

    if (!thread)
        return -1;

    if (!suspend_init_lock())
        return -1;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
        pinfo = suspend_add_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return -1;
    }

    if (pinfo->suspend_count > 0)
    {
        ++pinfo->suspend_count;
        pthread_mutex_unlock(&g_suspend_mutex);
        return 0;
    }

    g_suspendee = thread;
    g_req_type = THREADREQ_SUS;

    if (pthread_kill(thread, SIGUSR2) != 0)
    {
        suspend_remove_thread(thread);
        pthread_mutex_unlock(&g_suspend_mutex);
        return -1;
    }

    /* Waiting for suspendee response */
    sem_wait(&pinfo->wake_sem);
    /* Check result */
    status = (pinfo->suspend_count > 0) ? 0 : -1;

    pthread_mutex_unlock(&g_suspend_mutex);
    return status;
}

/**
 * Resume given thread
 * @param thread The thread to resume
 */
int port_thread_resume(osthread_t thread)
{
    int status;
    os_thread_info_t* pinfo;

    if (!thread)
        return -1;

    if (!suspend_init_lock())
        return -1;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return -1;
    }

    if (pinfo->suspend_count > 1)
    {
        --pinfo->suspend_count;
        pthread_mutex_unlock(&g_suspend_mutex);
        return 0;
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
    return 0;
}

/**
 * Determine suspend count for the given thread
 * @param thread The thread to check
 * @return -1 if error have occured
 */
int port_thread_get_suspend_count(osthread_t thread)
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
int port_thread_get_context(osthread_t thread, thread_context_t *context)
{
    int status = -1;
    os_thread_info_t* pinfo;

    if (!thread || !context)
        return -1;

    if (!suspend_init_lock())
        return -1;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return status;
    }

    if (pinfo->suspend_count > 0)
    {
        *context = pinfo->context;
        status = -1;
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
int port_thread_set_context(osthread_t thread, thread_context_t *context)
{
    int status = -1;
    os_thread_info_t* pinfo;

    if (!thread || !context)
        return -1;

    if (!suspend_init_lock())
        return -1;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        pthread_mutex_unlock(&g_suspend_mutex);
        return status;
    }

    if (pinfo->suspend_count > 0)
    {
        pinfo->context = *context;
        status = 0;
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
