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


#ifndef _platform_lowlevel_H_
#define _platform_lowlevel_H_

//MVM

#if _MSC_VER >= 1300 || __INTEL_COMPILER
// workaround for the
// http://www.microsoft.com/msdownload/platformsdk/sdkupdate/2600.2180.7/contents.htm
#include <winsock2.h>
#endif

#include <windows.h>

#if _MSC_VER < 1300
#include <winsock2.h>
#endif

#include <crtdbg.h>

#include "platform.h"

// moved from util_log.h, there should be better place for this definition.

typedef unsigned pthread_t; // see documentation on _beginthread
typedef CRITICAL_SECTION pthread_mutex_t;
typedef struct pthread_mutexattr_t pthread_mutexattr_t;
typedef void *sem_t;

inline pthread_t pthread_self() {
    return GetCurrentThreadId();
}

inline int pthread_mutex_init (pthread_mutex_t *mutex, pthread_mutexattr_t *attr) { 
    InitializeCriticalSection(mutex);
    return 0; 
}

inline int pthread_mutex_destroy (pthread_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

inline int pthread_mutex_lock (pthread_mutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

inline int pthread_mutex_unlock (pthread_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

inline void disable_assert_dialogs() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _set_error_mode(_OUT_TO_STDERR);
}

inline void debug_break() {
    _CrtDbgBreak();
}

inline DWORD IJGetLastError(VOID)
{
    return GetLastError();
}

struct timespec {
    long tv_sec;
    long tv_nsec;
};

inline int sem_init(sem_t *semaphore, int pshared, unsigned int value) {
    *semaphore = CreateSemaphore(NULL, value, INFINITE, NULL);
    return 0;
}

inline int sem_destroy(sem_t *sem) {
    return CloseHandle(*sem);
}

inline int sem_post(sem_t *semaphore) {
    return ReleaseSemaphore(*semaphore, 1, NULL);
}

inline int sem_wait(sem_t *semaphore) {
    return WaitForSingleObject(*semaphore, INFINITE);
}

inline int sem_timedwait(sem_t *semaphore,
               const struct timespec *abs_timeout) {
    return WaitForSingleObject(*semaphore, abs_timeout->tv_sec*1000);
}


#endif // _platform_lowlevel_H_
