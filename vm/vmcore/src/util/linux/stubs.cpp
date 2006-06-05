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
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#define LOG_DOMAIN "port.old"
#include "cxxlog.h"

#include "platform.h"
#include "vm_threads.h"
#include "exceptions.h"
#include "method_lookup.h"
#include "open/gc.h"

// void vm_thread_enumerate_from_native(VM_thread *thread); // unused anywhere

BOOL port_CloseHandle(VmEventHandle UNREF hh)
{
    ABORT("Not implemented");
    return 0;
}

//wgs: I wonder if we could give it a errno
#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:584) // omission of exception specification is incompatible with previous function "__errno_location" (declared at line 38 of "/usr/include/bits/errno.h")
#endif

extern int errno;

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

DWORD IJGetLastError(VOID)
{
    return errno;
}

//wgs: I wonder if we could give it a errno
DWORD WSAGetLastError(VOID)
{
    ABORT("Not implemented");
    return errno;
}

void _fpreset(void)
{
    ABORT("Not implemented");
}

BOOL GetThreadContext(VmThreadHandle UNREF hthread, const CONTEXT * UNREF lpcontext)
{
  ABORT("Not implemented");
  return 0;
}

BOOL SetThreadContext(VmThreadHandle UNREF hh, const CONTEXT * UNREF cc)
{
  ABORT("Not implemented");
  return 0;
}

int VM_thread::setPriority(int UNREF priority)
{
    ABORT("Not implemented");
    return 0;
}

#include <sys/time.h>
__uint64 GetTickCount(void)
{
    static bool not_initialized = true;
    static timeval basetime;
    __uint64 now;
    struct timezone UNUSED tz;
    assert(!gettimeofday(&basetime, &tz));
    now = (*(__uint64 *)&basetime.tv_sec)*1000000+*(__uint64 *)&basetime;
    if (not_initialized) {
        basetime = *(timeval *)&now;
    not_initialized = false;
    return (__uint64)0;
    } else {
    return (now - *(__uint64*)&basetime);
    }
}

CriticalSection::CriticalSection()
{
    m_cs = (void *) new CRITICAL_SECTION;
    InitializeCriticalSection((CRITICAL_SECTION *)m_cs);
}

CriticalSection::~CriticalSection()
{
    DeleteCriticalSection((CRITICAL_SECTION *)m_cs);
    delete (CRITICAL_SECTION *)m_cs;
}

void CriticalSection::lock()
{
    EnterCriticalSection((CRITICAL_SECTION *)m_cs);
}

void CriticalSection::unlock()
{
    LeaveCriticalSection((CRITICAL_SECTION *)m_cs);
}

bool CriticalSection::tryLock()
{
    return TryEnterCriticalSection((CRITICAL_SECTION *)m_cs) ? true : false;
}

VmRegisterContext::VmRegisterContext()
{
    CONTEXT *ctx = new CONTEXT;
    ctx->ContextFlags = 0;
    _pcontext = (void *) ctx;
}

VmRegisterContext::~VmRegisterContext()
{
    delete (CONTEXT *) _pcontext;
}

void VmRegisterContext::setFlag(VmRegisterContext::ContextFlag flag)
{
    unsigned old = ((CONTEXT *)_pcontext)->ContextFlags;
    switch (flag)
    {
    case VmRegisterContext::CF_FloatingPoint:
        old |= CONTEXT_FLOATING_POINT;
        break;
    case VmRegisterContext::CF_Integer:
        old |= CONTEXT_INTEGER;
        break;
    case VmRegisterContext::CF_Control:
        old |= CONTEXT_CONTROL;
        break;
    default:
        ASSERT(0, "Unexpected context flag");
        break;
    }
    ((CONTEXT *)_pcontext)->ContextFlags = old;
}

void VmRegisterContext::getContext(VM_thread *thread)
{
    CONTEXT *ctx = (CONTEXT *) _pcontext;
    BOOL UNUSED stat = GetThreadContext(thread->thread_handle, ctx);
    assert(stat);
#ifdef _IPF_
    thread->regs.gr[4] = ctx->IntS0;
    thread->regs.gr[5] = ctx->IntS1;
    thread->regs.gr[6] = ctx->IntS2;
    thread->regs.gr[7] = ctx->IntS3;
    thread->regs.gr[12] = ctx->IntSp;
    thread->regs.preds = ctx->Preds;
    thread->regs.pfs = ctx->RsPFS;
    thread->regs.bsp = (uint64 *)ctx->RsBSP;
    thread->regs.ip = ctx->StIIP;
#else // !_IPF_
#ifdef _EM64T_
    thread->regs.rax = ctx->Eax;
    thread->regs.rbx = ctx->Ebx;
    thread->regs.rcx = ctx->Ecx;
    thread->regs.rdx = ctx->Edx;
    thread->regs.rdi = ctx->Edi;
    thread->regs.rsi = ctx->Esi;
    thread->regs.rbp = ctx->Ebp;
    thread->regs.rsp = ctx->Esp;
    thread->regs.rip = ctx->Eip; 
#else
    thread->regs.eax = ctx->Eax;
    thread->regs.ebx = ctx->Ebx;
    thread->regs.ecx = ctx->Ecx;
    thread->regs.edx = ctx->Edx;
    thread->regs.edi = ctx->Edi;
    thread->regs.esi = ctx->Esi;
    thread->regs.ebp = ctx->Ebp;
    thread->regs.esp = ctx->Esp;
    thread->regs.eip = ctx->Eip; 
#endif //_EM64T_
#endif // !_IPF_
}

void VmRegisterContext::setContext(VM_thread *thread)
{
    CONTEXT *ctx = (CONTEXT *) _pcontext;
    BOOL stat = GetThreadContext(thread->thread_handle, ctx);
    assert(stat);
#ifdef _IPF_
    ctx->IntS0 = thread->regs.gr[4];
    ctx->IntS1 = thread->regs.gr[5];
    ctx->IntS2 = thread->regs.gr[6];
    ctx->IntS3 = thread->regs.gr[7];
#else // !_IPF_
#ifdef _EM64T_
    ctx->Eax = thread->regs.rax;
    ctx->Ebx = thread->regs.rbx;
    ctx->Ecx = thread->regs.rcx;
    ctx->Edx = thread->regs.rdx;
    ctx->Edi = thread->regs.rdi;
    ctx->Esi = thread->regs.rsi;
    ctx->Ebp = thread->regs.rbp;
#else
    ctx->Eax = thread->regs.eax;
    ctx->Ebx = thread->regs.ebx;
    ctx->Ecx = thread->regs.ecx;
    ctx->Edx = thread->regs.edx;
    ctx->Edi = thread->regs.edi;
    ctx->Esi = thread->regs.esi;
    ctx->Ebp = thread->regs.ebp;
#endif //_EM64T_
#endif // !_IPF_
    stat = SetThreadContext(thread->thread_handle, ctx);
    assert(stat);
}

void VmRegisterContext::getBspAndRnat(VM_thread * UNREF thread, uint64 ** UNREF bspstore, uint64 * UNREF rnat)
{
#ifdef _IPF_
    CONTEXT *ctx = (CONTEXT *) _pcontext;
    BOOL stat;
    while ((stat = GetThreadContext(thread->thread_handle, ctx)) == 0) {
        DWORD error_number = IJGetLastError();
        DIE("Unexpected error from GetThreadContext : " << error_number);
        // GetThreadContext should never return zero on NT
    }
    *bspstore = (uint64*)ctx->RsBSPSTORE;
    *rnat = ctx->RsRNAT;
#else // !_IPF_
    ABORT("Not supported"); // shouldn't be called under IA-32
#endif // !_IPF_
}
