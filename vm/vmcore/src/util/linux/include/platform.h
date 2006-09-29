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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#ifndef include_platform_h
#define include_platform_h

#include <pthread.h>
#include <limits.h>
#include <semaphore.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "open/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define __fastcall
#define __stdcall
#if defined(_IPF_) || defined(_EM64T_)
#define stdcall__
#else
#define stdcall__  __attribute__ ((__stdcall__))
#endif
#define __cdecl
#define _stat stat
#define WINAPI

#define WORD unsigned short
#define DWORD unsigned int
#define BOOL unsigned int
#ifdef POINTER64
#define HANDLE void*
#else
#define HANDLE unsigned int
#endif

typedef uint32 ULONG;
typedef int32 LONG;
typedef unsigned long long LONGLONG;
typedef uint64 ULONGLONG;

typedef struct
{
    int64 LowPart;
    int64 HighPart;
} FLOAT128;

#define IN
#define OUT
#define VOID void
typedef void *PVOID;
typedef void *LPVOID;

typedef unsigned char byte;
typedef unsigned char BYTE;

#ifdef __INTEL_COMPILER
// __int8 is built-in type in icc
// __int16 is built-in type in icc
// __int32 is built-in type in icc
// __int64 is built-in type in icc
typedef unsigned __int8 __uint8;
typedef unsigned __int16 __uint16;
typedef unsigned __int32 __uint32;
typedef unsigned __int64 __uint64;
#else
typedef unsigned char __uint8;
typedef unsigned short __uint16;
typedef unsigned int __uint32;
typedef unsigned long long __uint64;
typedef   signed char  __int8;
typedef   signed short __int16;
typedef   signed int  __int32; 
typedef   signed long long __int64;
#endif


typedef   signed long long int64;
typedef    unsigned char boolean;

#define dllexport
#define __declspec(junk)

#ifndef FMT64
#define FMT64 "ll"
#endif // FMT64

typedef struct VM_thread VM_thread;

/*
 * There are two ways to implement Thread-Specific_Data(TSD or Thread Local 
 * Storage:TLS) in LinuxThreads.
 * 1) One is using gs register to declare a segment for pthread_descr_struct, 
 * where (p_specific[0..31])[0..31] locates in.
 * 2) The other way that gets rid of gs register and segment addressing is 
 * simply to put pthread_descr_struct at the bottom of thread stack.
 *
 * So for the 1) approach, we use gs:displacement to get the needed TSD;
 * for 2), we need get the stack bottom and TSD offset to the bottom. 
 */

#define THREAD_STACK_SIZE 2*1024*1024 //this is the default thread stack size 

//MVM
#ifdef __GLIBC__
#if (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2)

#define SIZE_OF_PTHREAD_DESCR 1024
#define OFFSET_OF_SPECIFIC_IN_THREAD_DESCR 300

#else  
#if (__GLIBC__ == 2 && __GLIBC_MINOR__ == 1) 

#define SIZE_OF_PTHREAD_DESCR 448
#define OFFSET_OF_SPECIFIC_IN_THREAD_DESCR 236

#endif //glibc 2.1
#endif //glibc 2.2
#else

#define SIZE_OF_PTHREAD_DESCR 1024
#define OFFSET_OF_SPECIFIC_IN_THREAD_DESCR 300

#endif // __GLIBC__


#define REVERSE_OFFSET_OF_SPECIFIC_DATA \
    (SIZE_OF_PTHREAD_DESCR - OFFSET_OF_SPECIFIC_IN_THREAD_DESCR)

#ifndef _IPF_

#ifdef USE_SEGMENT // approach 1); turn off by default

inline void *get_specific00(void){
    register void *specific;
    asm volatile(
        "movl %%gs:%c1, %0\n\t"
        "movl (%0), %0"
        :"=r"(specific)
        :"i"(OFFSET_OF_SPECIFIC_IN_THREAD_DESCR) 
    );
    return specific;
}

#else //not USE_SEGMENT;  approach 2), turn on by default
inline void *get_specific00(void){
        register void *specific;
        asm volatile(
                "mov %%esp, %0\n\t"
                "or   %1, %0\n\t"
                "mov  %c2(%0), %0\n\t"
                "mov (%0), %0\n\t"
                :"=r"(specific)
                :"i"( THREAD_STACK_SIZE - 1 ), 
                 "i"( 1 - REVERSE_OFFSET_OF_SPECIFIC_DATA )
        );
        return specific;
}

/*
 *  Notes:
 *  Redhat 7.1 uses approach 1) for libpthread.a, and 2) for libpthread.so. 
 *  Because VM links libpthread.a statically for the executable, we
 *  use approach 2) by default. 
 */

#endif //USE_SEGMENT

#endif /* #ifndef _IPF_ */

#define N_T_S_H NULL
#define E_H_M NULL
#define E_H_S NULL
#define E_H_I NULL
#define E_H_S0 NULL
#define G_S_R_E_H NULL
#define J_V_M_T_I_E_H NULL
#define E_H_RECOMP NULL

#include "stdlib.h"

#include <assert.h>

#include "stdarg.h"


#define INFINITE 0XFFffFFff
  
DWORD IJGetLastError(VOID);

void _fpreset(void);

typedef struct event_wrapper {
  pthread_mutex_t          mutex;
  pthread_cond_t           cond;
  __uint32                 man_reset_flag;
  __uint32                 state;
  unsigned int             n_wait;
} event_wrapper;


#define THREAD_PRIORITY_NORMAL 4

#define _MAX_PATH PATH_MAX

struct _finddata_t {
    unsigned aa;
    long ttc;
    long ta;
    long tw;
    unsigned long size;
    char name[260];
    };
    
long _findfirst(const char *, struct _finddata_t *);
int _findnext(long, struct _finddata_t *);
int _findclose(long);

#define SEEK_CUR 1
__int64 _lseeki64(int, int64, int);


void * _alloca(int);

typedef void * FARPROC;

/*inline void vm_terminate_thread(VmThreadHandle thrdaddr) {
    pthread_cancel(thrdaddr);    
}*/
#ifdef __cplusplus
}
#endif

#endif // include_platform_h ******************

