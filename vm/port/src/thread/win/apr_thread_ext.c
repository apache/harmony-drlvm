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
/**
 * @author Artem Aliev
 * @version $Revision:$
 */

#include <windows.h>
#include <stdio.h>
#include "apr_thread_ext.h"
#include <apr_atomic.h>

// MSVC barrier intrinsics setup
#if _MSC_VER < 1400
    // VC++ 2003
    extern void _ReadWriteBarrier();
    extern void _mm_mfence(void);
#else
    // VC++ 2005
    #include <intrin.h>
    #include <emmintrin.h>
#endif
#pragma intrinsic (_ReadWriteBarrier)

APR_DECLARE(apr_status_t) apr_thread_set_priority(apr_thread_t *thread, 
                apr_int32_t priority) 
{
    HANDLE *os_thread;
    apr_status_t status;
    
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)) {
        return status;
    }
    
    if (SetThreadPriority(*os_thread, (int)priority)) {
        return APR_SUCCESS;
    } else {
        return apr_get_os_error();
    }
}

// touch thread to flash memory
APR_DECLARE(apr_status_t) apr_thread_yield_other(apr_thread_t* thread) {
    HANDLE *os_thread = NULL;
    apr_status_t status;  

    static CRITICAL_SECTION *yield_other_mutex = NULL;
    if (yield_other_mutex == NULL) {
        CRITICAL_SECTION *cs = malloc(sizeof(CRITICAL_SECTION));
        InitializeCriticalSectionAndSpinCount(cs, 400);
        // there should be the only one CS
        // do nothing if some one else already init it.
        if(apr_atomic_casptr ((volatile void**)&yield_other_mutex, (void*)cs, NULL)) {
            DeleteCriticalSection(cs);
            free(cs);
        }
    }

	if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)) {
        return status;
    }
	if(!os_thread) {
        return status;
    }

    /* 
     * Synchronization is needed to avoid cyclic (mutual) suspension problem.
     * Accordingly to MSDN, it is possible on multiprocessor box that
     * 2 threads suspend each other and become deadlocked.
     */
    EnterCriticalSection(yield_other_mutex);
    if(-1 != SuspendThread(*os_thread)) {
        /* suspended successfully, so resume it back. */
        ResumeThread(*os_thread);
    } 
    LeaveCriticalSection(yield_other_mutex);
    return APR_SUCCESS; 
}

APR_DECLARE(void) apr_memory_rw_barrier() {
#ifdef _EM64T_
    // if x86_64/x64/EM64T, then use an mfence to flush memory caches
    _mm_mfence();
#else
    /* otherwise, we assume this is an x86, so insert an inline assembly 
     * macro to insert a lock instruction
     *
     * the lock is what's needed, so the 'add' is setup, essentially, as a no-op
     */
    __asm {lock add [esp], 0 }
#endif
    _ReadWriteBarrier();
}

APR_DECLARE(apr_status_t) apr_thread_times(apr_thread_t *thread, 
                                apr_time_t * kernel_time, apr_time_t * user_time){
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    HANDLE *hThread;
    SYSTEMTIME sysTime;
    int res;
    __int64 xx;
    __int32 * pp;
    apr_status_t status;

    if (status = apr_os_thread_get(&((apr_os_thread_t *)hThread), thread)) {
        return status;
    }
                    
    res = GetThreadTimes(
        *hThread,
        &creationTime,
        &exitTime,
        &kernelTime,
        &userTime
    );
    
    printf( "Creation time = %08x %08x\n", creationTime.dwHighDateTime, creationTime.dwLowDateTime);
    printf( "Exit     time = %08x %08x\n", exitTime.dwHighDateTime, exitTime.dwLowDateTime);
    printf( "Kernrl   time = %08x %08x %08d\n", kernelTime.dwHighDateTime
                                      , kernelTime.dwLowDateTime, kernelTime.dwLowDateTime);
    printf( "User     time = %08x %08x %08d\n", userTime.dwHighDateTime
                                      , userTime.dwLowDateTime, userTime.dwLowDateTime);
    printf("%d\n", 
        ((unsigned)exitTime.dwLowDateTime - (unsigned)creationTime.dwLowDateTime)/10000000);
    
    FileTimeToSystemTime(&creationTime, &sysTime);
    printf("%d %d %d %d %d %d \n", sysTime.wYear, sysTime.wMonth,
        sysTime.wHour + 3, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
    
    pp = (int*)&xx;
    *pp = kernelTime.dwLowDateTime;
    *(pp + 1) = kernelTime.dwHighDateTime;
    *kernel_time = xx;
    pp = (int*)&xx;
    *pp = userTime.dwLowDateTime;
    *(pp + 1) = userTime.dwHighDateTime;
    *user_time = xx;

    return APR_SUCCESS; 
}

APR_DECLARE(apr_status_t) apr_get_thread_time(apr_thread_t *thread, apr_int64_t* nanos_ptr)
{
    HANDLE *os_thread;
    apr_status_t status;   
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)!=APR_SUCCESS) {
        return status;
    }
    GetThreadTimes(*os_thread, &creation_time, 
        &exit_time, &kernel_time, 
        &user_time);

    *nanos_ptr=(Int64ShllMod32((&user_time)->dwHighDateTime, 32)|(&user_time)->dwLowDateTime);//*100;
   // *nanos_ptr = user_time * 100; // convert to nanos
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_thread_cancel(apr_thread_t *thread) {
    HANDLE *os_thread;
    apr_status_t status;   
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)) {
        return status;
    }
    
    if (TerminateThread(*os_thread, 0)) {
        return APR_SUCCESS;
    } else {
        return apr_get_os_error();
    }
}
