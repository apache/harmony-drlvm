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
typedef pthread_t VmThreadHandle;
#ifdef POINTER64
#define VmEventHandle void*
#define HANDLE void*
#else
#define VmEventHandle unsigned int
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

void init_linux_thread_system();
typedef struct VM_thread VM_thread;
bool suspend_thread(VM_thread *);
bool resume_thread(VM_thread *);

#define CRITICAL_SECTION pthread_mutex_t
#define LPCRITICAL_SECTION pthread_mutex_t *

VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
BOOL EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

VmThreadHandle vm_beginthreadex( void * security, unsigned stack_size, unsigned(__stdcall *start_address)(void *), void *arglist, unsigned initflag, pthread_t *thrdaddr);
VmThreadHandle vm_beginthread(void (__cdecl *start_address)(void *), unsigned stack_size, void *arglist);
void vm_endthreadex(int);
void vm_endthread(void);

#define CreateEvent vm_create_event
#define SetEvent vm_set_event
#define ResetEvent vm_reset_event
#define WaitForSingleObject vm_wait_for_single_object
#define WaitForMultipleObjects vm_wait_for_multiple_objects

void Sleep(DWORD);
void SleepEx(DWORD, bool);

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
#define OS_THREAD_INIT_1()
#define OS_SYNC_SUPPORT()
#define OS_THREAD_INIT_2() p_TLS_vmthread = p_vm_thread; 
#define THREAD_CLEANUP() 
#define BEGINTHREADEX_SUSPENDSTATE 1
#define SET_THREAD_DATA_MACRO()
#define CHECK_HIJACK_SUPPORT_MACRO()
#include "stdlib.h"

#include <assert.h>

#include "stdarg.h"


#define INFINITE 0XFFffFFff
  
DWORD IJGetLastError(VOID);

void _fpreset(void);


VmEventHandle vm_create_event(int *, unsigned int, unsigned int, char *);
BOOL vm_destroy_event( VmEventHandle );

typedef struct event_wrapper {
  pthread_mutex_t          mutex;
  pthread_cond_t           cond;
  __uint32                 man_reset_flag;
  __uint32                 state;
  unsigned int             n_wait;
} event_wrapper;

BOOL vm_reset_event(VmEventHandle hEvent);
BOOL vm_set_event(VmEventHandle hEvent);

void vm_yield();
DWORD vm_wait_for_single_object(VmEventHandle hHandle, DWORD dwMilliseconds);
DWORD vm_wait_for_multiple_objects(DWORD num, const VmEventHandle * handle, BOOL flag, DWORD dwMilliseconds);


#define WAIT_TIMEOUT 0x102
#define WAIT_OBJECT_0 0
#define WAIT_FAILED (DWORD)0xFFFFFFFF


typedef struct _FLOATING_SAVE_AREA {
    DWORD   ControlWord;
    DWORD   StatusWord;
    DWORD   TagWord;
    DWORD   ErrorOffset;
    DWORD   ErrorSelector;
    DWORD   DataOffset;
    DWORD   DataSelector;
    BYTE    RegisterArea[80];
    DWORD   Cr0NpxState;
} FLOATING_SAVE_AREA;

typedef FLOATING_SAVE_AREA *PFLOATING_SAVE_AREA;

#ifdef _IPF_
typedef struct _CONTEXT {

    DWORD ContextFlags;
    DWORD Fill1[3];

    ULONGLONG DbI0;
    ULONGLONG DbI1;
    ULONGLONG DbI2;
    ULONGLONG DbI3;
    ULONGLONG DbI4;
    ULONGLONG DbI5;
    ULONGLONG DbI6;
    ULONGLONG DbI7;

    ULONGLONG DbD0;
    ULONGLONG DbD1;
    ULONGLONG DbD2;
    ULONGLONG DbD3;
    ULONGLONG DbD4;
    ULONGLONG DbD5;
    ULONGLONG DbD6;
    ULONGLONG DbD7;

    FLOAT128 FltS0;
    FLOAT128 FltS1;
    FLOAT128 FltS2;
    FLOAT128 FltS3;
    FLOAT128 FltT0;
    FLOAT128 FltT1;
    FLOAT128 FltT2;
    FLOAT128 FltT3;
    FLOAT128 FltT4;
    FLOAT128 FltT5;
    FLOAT128 FltT6;
    FLOAT128 FltT7;
    FLOAT128 FltT8;
    FLOAT128 FltT9;

    FLOAT128 FltS4;
    FLOAT128 FltS5;
    FLOAT128 FltS6;
    FLOAT128 FltS7;
    FLOAT128 FltS8;
    FLOAT128 FltS9;
    FLOAT128 FltS10;
    FLOAT128 FltS11;
    FLOAT128 FltS12;
    FLOAT128 FltS13;
    FLOAT128 FltS14;
    FLOAT128 FltS15;
    FLOAT128 FltS16;
    FLOAT128 FltS17;
    FLOAT128 FltS18;
    FLOAT128 FltS19;

    FLOAT128 FltF32;
    FLOAT128 FltF33;
    FLOAT128 FltF34;
    FLOAT128 FltF35;
    FLOAT128 FltF36;
    FLOAT128 FltF37;
    FLOAT128 FltF38;
    FLOAT128 FltF39;

    FLOAT128 FltF40;
    FLOAT128 FltF41;
    FLOAT128 FltF42;
    FLOAT128 FltF43;
    FLOAT128 FltF44;
    FLOAT128 FltF45;
    FLOAT128 FltF46;
    FLOAT128 FltF47;
    FLOAT128 FltF48;
    FLOAT128 FltF49;

    FLOAT128 FltF50;
    FLOAT128 FltF51;
    FLOAT128 FltF52;
    FLOAT128 FltF53;
    FLOAT128 FltF54;
    FLOAT128 FltF55;
    FLOAT128 FltF56;
    FLOAT128 FltF57;
    FLOAT128 FltF58;
    FLOAT128 FltF59;

    FLOAT128 FltF60;
    FLOAT128 FltF61;
    FLOAT128 FltF62;
    FLOAT128 FltF63;
    FLOAT128 FltF64;
    FLOAT128 FltF65;
    FLOAT128 FltF66;
    FLOAT128 FltF67;
    FLOAT128 FltF68;
    FLOAT128 FltF69;

    FLOAT128 FltF70;
    FLOAT128 FltF71;
    FLOAT128 FltF72;
    FLOAT128 FltF73;
    FLOAT128 FltF74;
    FLOAT128 FltF75;
    FLOAT128 FltF76;
    FLOAT128 FltF77;
    FLOAT128 FltF78;
    FLOAT128 FltF79;

    FLOAT128 FltF80;
    FLOAT128 FltF81;
    FLOAT128 FltF82;
    FLOAT128 FltF83;
    FLOAT128 FltF84;
    FLOAT128 FltF85;
    FLOAT128 FltF86;
    FLOAT128 FltF87;
    FLOAT128 FltF88;
    FLOAT128 FltF89;

    FLOAT128 FltF90;
    FLOAT128 FltF91;
    FLOAT128 FltF92;
    FLOAT128 FltF93;
    FLOAT128 FltF94;
    FLOAT128 FltF95;
    FLOAT128 FltF96;
    FLOAT128 FltF97;
    FLOAT128 FltF98;
    FLOAT128 FltF99;

    FLOAT128 FltF100;
    FLOAT128 FltF101;
    FLOAT128 FltF102;
    FLOAT128 FltF103;
    FLOAT128 FltF104;
    FLOAT128 FltF105;
    FLOAT128 FltF106;
    FLOAT128 FltF107;
    FLOAT128 FltF108;
    FLOAT128 FltF109;

    FLOAT128 FltF110;
    FLOAT128 FltF111;
    FLOAT128 FltF112;
    FLOAT128 FltF113;
    FLOAT128 FltF114;
    FLOAT128 FltF115;
    FLOAT128 FltF116;
    FLOAT128 FltF117;
    FLOAT128 FltF118;
    FLOAT128 FltF119;

    FLOAT128 FltF120;
    FLOAT128 FltF121;
    FLOAT128 FltF122;
    FLOAT128 FltF123;
    FLOAT128 FltF124;
    FLOAT128 FltF125;
    FLOAT128 FltF126;
    FLOAT128 FltF127;

    ULONGLONG StFPSR;

    ULONGLONG IntGp;
    ULONGLONG IntT0;
    ULONGLONG IntT1;
    ULONGLONG IntS0;
    ULONGLONG IntS1;
    ULONGLONG IntS2;
    ULONGLONG IntS3;
    ULONGLONG IntV0;
    ULONGLONG IntT2;
    ULONGLONG IntT3;
    ULONGLONG IntT4;
    ULONGLONG IntSp;
    ULONGLONG IntTeb;
    ULONGLONG IntT5;
    ULONGLONG IntT6;
    ULONGLONG IntT7;
    ULONGLONG IntT8;
    ULONGLONG IntT9;
    ULONGLONG IntT10;
    ULONGLONG IntT11;
    ULONGLONG IntT12;
    ULONGLONG IntT13;
    ULONGLONG IntT14;
    ULONGLONG IntT15;
    ULONGLONG IntT16;
    ULONGLONG IntT17;
    ULONGLONG IntT18;
    ULONGLONG IntT19;
    ULONGLONG IntT20;
    ULONGLONG IntT21;
    ULONGLONG IntT22;

    ULONGLONG IntNats;
 
    ULONGLONG Preds;

    ULONGLONG BrRp;
    ULONGLONG BrS0;
    ULONGLONG BrS1;
    ULONGLONG BrS2;
    ULONGLONG BrS3;
    ULONGLONG BrS4;
    ULONGLONG BrT0;
    ULONGLONG BrT1;

    ULONGLONG ApUNAT;
    ULONGLONG ApLC;
    ULONGLONG ApEC;
    ULONGLONG ApCCV;
    ULONGLONG ApDCR;

 
    ULONGLONG RsPFS;
    ULONGLONG RsBSP;
    ULONGLONG RsBSPSTORE;
    ULONGLONG RsRSC;
    ULONGLONG RsRNAT;

    ULONGLONG StIPSR;
    ULONGLONG StIIP;
    ULONGLONG StIFS;

    ULONGLONG StFCR;
    ULONGLONG Eflag;
    ULONGLONG SegCSD;
    ULONGLONG SegSSD;
    ULONGLONG Cflag;
    ULONGLONG StFSR;
    ULONGLONG StFIR;
    ULONGLONG StFDR;

    ULONGLONG UNUSEDPACK;

} CONTEXT;
#else
typedef struct _CONTEXT {

    DWORD ContextFlags;

    DWORD   Dr0;
    DWORD   Dr1;
    DWORD   Dr2;
    DWORD   Dr3;
    DWORD   Dr6;
    DWORD   Dr7;

    FLOATING_SAVE_AREA FloatSave;

    DWORD   SegGs;
    DWORD   SegFs;
    DWORD   SegEs;
    DWORD   SegDs;

    DWORD   Edi;
    DWORD   Esi;
    DWORD   Ebx;
    DWORD   Edx;
    DWORD   Ecx;
    DWORD   Eax;

    DWORD   Ebp;
    DWORD   Eip;
    DWORD   SegCs;
    DWORD   EFlags;
    DWORD   Esp;
    DWORD   SegSs;

} CONTEXT;
#endif // _IPF_


typedef CONTEXT *PCONTEXT;

#define CONTEXT_CONTROL 1
#define CONTEXT_FLOATING_POINT 2
#define CONTEXT_INTEGER 3
#define THREAD_PRIORITY_NORMAL 4


BOOL GetThreadContext(VmThreadHandle hthread, const CONTEXT *lpcontext);
BOOL SetThreadContext(VmThreadHandle hh, const CONTEXT *cc);
BOOL SetThreadPriority(VmThreadHandle hh, int pp);

BOOL port_CloseHandle(VmEventHandle hh);
pthread_t GetCurrentThreadId(void);
VmEventHandle GetCurrentProcess(void);
VmEventHandle GetCurrentThread(void);

#define DUPLICATE_SAME_ACCESS 3

BOOL DuplicateHandle(VmEventHandle aa, VmEventHandle bb, 
                     VmEventHandle cc, VmEventHandle *dd, 
                     DWORD ee, BOOL ff, DWORD gg);

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

__uint64 GetTickCount();

void vm_endthreadex(int);
typedef void * FARPROC;

inline void vm_terminate_thread(VmThreadHandle thrdaddr) {
    pthread_cancel(thrdaddr);    
}
#ifdef __cplusplus
}
#endif

#endif // include_platform_h ******************

