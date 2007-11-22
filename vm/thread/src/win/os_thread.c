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

#include <apr_atomic.h>
#
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
    HANDLE handle = (HANDLE)_beginthreadex(NULL, stacksize, (unsigned(__stdcall *)(void*))func, data, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

    if (handle) {
        *phandle = handle;
        if (priority)
            SetThreadPriority(handle, priority);
        return 0;
    } else {
        int error = GetLastError();
        if (error == ERROR_OUTOFMEMORY)
            return TM_ERROR_OUT_OF_MEMORY;
        return error;
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
    if (SetThreadPriority(os_thread, (int)priority)) {
        return 0;
    } else {
        return GetLastError();
    }
}

/**
 * Returns os handle of the current thread.
 *
 * @return current thread handle on success, NULL on error
 */
osthread_t os_thread_current()
{
    HANDLE hproc = GetCurrentProcess();
    HANDLE hthread = GetCurrentThread();
    if (!DuplicateHandle(hproc, hthread,
                         hproc, &hthread, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        return NULL;
    }
    return hthread;
}

/**
 * Free native thread handle
 *
 * @param os_thread     thread handle
 *
 * @return              0 on success, systerm error otherwise
 */
int os_thread_free(osthread_t os_thread)
{
    BOOL r = CloseHandle(os_thread);
    return !r;
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
    int error = 0;
    DWORD r;
    r = WaitForSingleObject(os_thread, INFINITE);
    if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED)
        r = 0;
    else
        r = GetLastError();
    CloseHandle(os_thread);
    return r;
}

/**
 * Causes the current thread to stop execution.
 *
 * @param status        returns status of a thread
 */
void os_thread_exit(IDATA status)
{
    ExitThread(status);
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
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    int r;

    r = GetThreadTimes(os_thread,
            &creation_time, &exit_time, &kernel_time, &user_time);

    if (r) {
        // according to MSDN, time is counted in 100 ns units, so we need to multiply by 100
        *pkernel = 100 *
            (((int64)kernel_time.dwHighDateTime << 32)
             | kernel_time.dwLowDateTime);
        *puser = 100 *
            (((int64)user_time.dwHighDateTime << 32)
             | user_time.dwLowDateTime);
        return 0;
    } else
        return GetLastError();
}

UDATA os_get_foreign_thread_stack_size() {
    void* stack_addr;
    size_t stack_size;
    size_t reg_size;
    MEMORY_BASIC_INFORMATION memory_information;

    VirtualQuery(&memory_information, &memory_information, sizeof(memory_information));
    reg_size = memory_information.RegionSize;
    stack_addr = ((char*) memory_information.BaseAddress) + reg_size;
    stack_size = ((char*) stack_addr) - ((char*) memory_information.AllocationBase);

    return (UDATA)stack_size;
}

typedef struct os_thread_info_t os_thread_info_t;

struct os_thread_info_t
{
    osthread_t              thread;
    int                     suspend_count;
    os_thread_context_t     context;

    os_thread_info_t*       next;
};


static CRITICAL_SECTION g_crit_section;
static os_thread_info_t* g_suspended_list = NULL;

/* Forward declarations */
static int suspend_init_lock();
static os_thread_info_t* init_susres_list_item();
static os_thread_info_t* suspend_add_thread(osthread_t thread);
static void suspend_remove_thread(osthread_t thread);
static os_thread_info_t* suspend_find_thread(osthread_t thread);


/**
 * Terminates the os thread.
 */
int os_thread_cancel(osthread_t os_thread)
{
    os_thread_info_t* pinfo;
    int status = TM_ERROR_NONE;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(os_thread);

    if (pinfo)
        suspend_remove_thread(os_thread);

    if (!TerminateThread(os_thread, 0))
        status = (int)GetLastError();

    LeaveCriticalSection(&g_crit_section);
    return status;
}

/**
 * Causes the other thread to have a memory barrier by suspending
 * and resuming it.
 */
void os_thread_yield_other(osthread_t os_thread)
{
    os_thread_info_t* pinfo;

    /*
     * Synchronization is needed to avoid cyclic (mutual) suspension problem.
     * Accordingly to MSDN, it is possible on multiprocessor box that
     * 2 threads suspend each other and become deadlocked.
     */
    if (!suspend_init_lock()) // Initializes and enters a critical section
        return;

    pinfo = suspend_find_thread(os_thread);

    if (pinfo && pinfo->suspend_count > 0) {
        LeaveCriticalSection(&g_crit_section);
        return;
    }

    if (SuspendThread(os_thread) != -1) {
        /* suspended successfully, so resume it back. */
        ResumeThread(os_thread);
    }

    LeaveCriticalSection(&g_crit_section);
}


/**
 * Suspend given thread
 * @param thread The thread to suspend
 */
int os_thread_suspend(osthread_t thread)
{
    os_thread_info_t* pinfo;
    DWORD old_count;

    if (!thread)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
        pinfo = suspend_add_thread(thread);

    if (!pinfo)
    {
        LeaveCriticalSection(&g_crit_section);
        return TM_ERROR_OUT_OF_MEMORY;
    }

    if (pinfo->suspend_count > 0)
    {
        ++pinfo->suspend_count;
        LeaveCriticalSection(&g_crit_section);
        return TM_ERROR_NONE;
    }

    old_count = SuspendThread(thread);

    if (old_count == (DWORD)-1)
    {
        int status = (int)GetLastError();
        LeaveCriticalSection(&g_crit_section);
        return status;
    }

    ++pinfo->suspend_count;
    LeaveCriticalSection(&g_crit_section);
    return TM_ERROR_NONE;
}

/**
 * Resume given thread
 * @param thread The thread to resume
 */
int os_thread_resume(osthread_t thread)
{
    os_thread_info_t* pinfo;
    DWORD old_count;

    if (!thread)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        LeaveCriticalSection(&g_crit_section);
        return TM_ERROR_UNATTACHED_THREAD;
    }

    if (pinfo->suspend_count > 1)
    {
        --pinfo->suspend_count;
        LeaveCriticalSection(&g_crit_section);
        return TM_ERROR_NONE;
    }

    old_count = ResumeThread(thread);

    if (old_count == (DWORD)-1)
    {
        int status = (int)GetLastError();
        LeaveCriticalSection(&g_crit_section);
        return status;
    }

    if (--pinfo->suspend_count == 0)
        suspend_remove_thread(thread);

    LeaveCriticalSection(&g_crit_section);
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

    LeaveCriticalSection(&g_crit_section);
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
    os_thread_info_t* pinfo;
    CONTEXT local_context;

    if (!thread || !context)
        return TM_ERROR_NULL_POINTER;

    if (!suspend_init_lock())
        return TM_ERROR_INTERNAL;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        LeaveCriticalSection(&g_crit_section);
        return TM_ERROR_UNATTACHED_THREAD;
    }

#ifdef CONTEXT_ALL
    local_context.ContextFlags = CONTEXT_ALL;
#else
    local_context.ContextFlags = CONTEXT_FULL;
#endif

    if (!GetThreadContext(thread, &local_context))
    {
        int status = (int)GetLastError();
        LeaveCriticalSection(&g_crit_section);
        return status;
    }

    pinfo->context = local_context;
    *context = local_context;
    LeaveCriticalSection(&g_crit_section);
    return TM_ERROR_NONE;
}

/**
 * Set context for given thread
 * @param thread The thread to process
 * @param context Pointer to platform-dependant context structure
 * @note The thread must be suspended
 */
int os_thread_set_context(osthread_t thread, os_thread_context_t *context)
{
    os_thread_info_t* pinfo;

    if (!thread || !context)
        return -1;

    if (!suspend_init_lock())
        return -2;

    pinfo = suspend_find_thread(thread);

    if (!pinfo)
    {
        LeaveCriticalSection(&g_crit_section);
        return TM_ERROR_UNATTACHED_THREAD;
    }

    if (!SetThreadContext(thread, context))
    {
        int status = (int)GetLastError();
        LeaveCriticalSection(&g_crit_section);
        return status;
    }

    pinfo->context = *context;
    LeaveCriticalSection(&g_crit_section);
    return TM_ERROR_NONE;
}


static int suspend_init_lock()
{
    static uint32 initialized = 0;

    if (!initialized)
    {
        // Critical section should be initialized only once,
        // do nothing in case someone else already initialized it.
        if (apr_atomic_cas32((volatile uint32*)&initialized, 1, 0) == 0)
            InitializeCriticalSectionAndSpinCount(&g_crit_section, 400);
    }

    EnterCriticalSection(&g_crit_section);
    return 1;
}

static os_thread_info_t* init_susres_list_item()
{
    os_thread_info_t* pinfo =
        (os_thread_info_t*)malloc(sizeof(os_thread_info_t));

    if (pinfo)
        pinfo->suspend_count = 0;

    return pinfo;
}

static os_thread_info_t* suspend_add_thread(osthread_t thread)
{
    os_thread_info_t* pinfo = init_susres_list_item();

    if (!pinfo)
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

    if (pinfo)
    {
        *pprev = pinfo->next;
        free(pinfo);
    }
}

static os_thread_info_t* suspend_find_thread(osthread_t thread)
{
    os_thread_info_t* pinfo;

    for (pinfo = g_suspended_list; pinfo; pinfo = pinfo->next)
    {
        if (pinfo->thread == thread)
            break;
    }

    return pinfo;
}

