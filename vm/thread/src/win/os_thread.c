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
        int (VMAPICALL *func)(void*), void *data)
{
    HANDLE handle = (HANDLE)_beginthreadex(NULL, stacksize, (unsigned(__stdcall *)(void*))func, data, 0, NULL);
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
 * Terminates the os thread.
 */
int os_thread_cancel(osthread_t os_thread)
{
    if (TerminateThread(os_thread, 0))
        return 0;
    else
        return GetLastError();
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
void os_thread_exit(int status)
{
    ExitThread(status);
}

/**
 * Causes the other thread to have a memory barrier by suspending
 * and resuming it.
 */
void os_thread_yield_other(osthread_t os_thread)
{
    static CRITICAL_SECTION yield_other_mutex;
    static int initialized = 0;
    if (!initialized) {
        // Critical section should be initialized only once,
        // do nothing in case someone else already initialized it.
        if (apr_atomic_cas32((volatile uint32*)&initialized, 1, 0) == 0) {
            InitializeCriticalSectionAndSpinCount(&yield_other_mutex, 400);
        }
    }

    // FIXME: should not use yield_other_mutex until it is guaranteed to be initialized

    /*
     * Synchronization is needed to avoid cyclic (mutual) suspension problem.
     * Accordingly to MSDN, it is possible on multiprocessor box that
     * 2 threads suspend each other and become deadlocked.
     */
    EnterCriticalSection(&yield_other_mutex);
    if (SuspendThread(os_thread) != -1) {
        /* suspended successfully, so resume it back. */
        ResumeThread(os_thread);
    }
    LeaveCriticalSection(&yield_other_mutex);
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
