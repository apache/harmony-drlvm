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


#define LOG_DOMAIN "port.old"
#include "cxxlog.h"

#include "platform_lowlevel.h"

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <process.h>

#include "platform_utils.h"
#include "open/vm_util.h"
#include "exception_filter.h"

BOOL ctrl_handler(DWORD ctrlType) 
{ 
    switch (ctrlType) 
    { 
    case CTRL_BREAK_EVENT:
      extern void quit_handler(int);
      quit_handler(0);
      return TRUE; 
    case CTRL_C_EVENT: 
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        extern void interrupt_handler(int);
        interrupt_handler(0);
        return TRUE; 
    default:
        ABORT("Unexpected event");
    } 
        return FALSE; 
} 

void initialize_signals(){
    TRACE2("signals", "Setting console control handle...");
    BOOL ok = SetConsoleCtrlHandler( (PHANDLER_ROUTINE) ctrl_handler, TRUE); 
    assert(ok);

    // add VEH to catch NPE's from bytecode
    TRACE2("signals", "Adding vectored exception handler...");
    PVOID res;
    res = AddVectoredExceptionHandler(0, vectored_exception_handler);
    assert(res);
}

//The following is for socket error handling
const char *sock_errstr[] = { 
NULL, /* 10000 WSABASEERR */
NULL,
NULL,
NULL,
/*10004 WSAEINTR*/          "(10004) Interrupted by socket close",     
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
/*10013 WSAEACCES*/         "(10013) Permission denied",       
/*10014 WSAEFAULT*/         "(10014) Bad address",       
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
/*10022 WSAEINVAL*/         "(10022) Invalid argument",       
NULL,
/*10024 WSAEMFILE*/         "(10024) Too many open files",     
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
/*10035 WSAEWOULDBLOCK*/    "(10035) Resource temporarily unavailable",      
/*10036 WSAEINPROGRESS*/    "(10036) Operation now in progress",     
/*10037 WSAEALREADY*/       "(10037) Operation already in progress",     
/*10038 WSAENOTSOCK*/       "(10038) Socket operation on non-socket",     
/*10039 WSAEDESTADDRREQ*/   "(10039) Destination address required",      
/*10040 WSAEMSGSIZE*/       "(10040) Message too long",      
/*10041 WSAEPROTOTYPE*/     "(10041) Protocol wrong type for socket",    
/*10042 WSAENOPROTOOPT*/    "(10042) Bad protocol option",      
/*10043 WSAEPROTONOSUPPORT*/"(10043) Protocol not supported",      
/*10044 WSAESOCKTNOSUPPORT*/"(10044) Socket type not supported",     
/*10045 WSAEOPNOTSUPP*/     "(10045) Operation not supported",      
/*10046 WSAEPFNOSUPPORT*/   "(10046) Protocol family not supported",     
/*10047 WSAEAFNOSUPPORT*/   "(10047) Address family not supported by protocol family",  
/*10048 WSAEADDRINUSE*/     "(10048) Address already in use",     
/*10049 WSAEADDRNOTAVAIL*/  "(10049) Cannot assign requested address",     
/*10050 WSAENETDOWN*/       "(10050) Network is down",      
/*10051 WSAENETUNREACH*/    "(10051) Network is unreachable",      
/*10052 WSAENETRESET*/      "(10052) Network dropped connection on reset",    
/*10053 WSAECONNABORTED*/   "(10053) Software caused connection abort",     
/*10054 WSAECONNRESET*/     "(10054) Connection reset by peer",     
/*10055 WSAENOBUFS*/        "(10055) No buffer space available",     
/*10056 WSAEISCONN*/        "(10056) Socket is already connected",     
/*10057 WSAENOTCONN*/       "(10057) Socket is not connected",     
/*10058 WSAESHUTDOWN*/      "(10058) Cannot send after socket shutdown",    
/*10060 WSAETIMEDOUT*/      "(10060) Connection timed out",      
/*10061 WSAECONNREFUSED*/   "(10061) Connection refused",       
NULL,
NULL,
NULL,
/*10064 WSAEHOSTDOWN*/      "(10064) Host is down",      
/*10065 WSAEHOSTUNREACH*/   "(10065) No route to host",     
NULL,
/*10067 WSAEPROCLIM*/       "(10067) Too many processes",      
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
/*10091 WSASYSNOTREADY*/    "(10091) Network subsystem is unavailable",     
/*10092 WSAVERNOTSUPPORTED*/"(10092) WINSOCK.DLL version out of range",    
/*10093 WSANOTINITIALISED*/ "(10093) Successful WSAStartup not yet performed",    
/*10094 WSAEDISCON*/        "(10094) Graceful shutdown in progress",     
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
/*10109 WSATYPE_NOT_FOUND*/ "(10109) Class type not found",
};

const char *sock_errstr1[] = { 
/*11001 WSAHOST_NOT_FOUND*/ "(11001) Host not found",      
/*11002 WSATRY_AGAIN*/      "(11002) Non-authoritative host not found",     
/*11003 WSANO_RECOVERY*/    "(11003) This is a non-recoverable error",    
/*11004 WSANO_DATA*/        "(11004) Valid name, no data record of requested type", 
};

const char *socket_strerror(int errcode){
    if(errcode < 10000)
        return NULL;
    if(errcode < 10110)
        return sock_errstr[errcode - 10000];
    if(errcode > 11000 && errcode < 11005)
        return sock_errstr1[errcode - 11001];
    return NULL;
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
        ABORT("Unexpected flag");
        break;
    }
    ((CONTEXT *)_pcontext)->ContextFlags = old;
}

/*  FIXME integration uncomment and fix
void VmRegisterContext::getContext(VM_thread *thread)
{
    CONTEXT *ctx = (CONTEXT *) _pcontext;
    BOOL stat = GetThreadContext(thread->thread_handle, ctx);
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
#endif // _EM64T_
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
#endif // _EM64T_
#endif // !_IPF_
    stat = SetThreadContext(thread->thread_handle, ctx);
    assert(stat);
}

void VmRegisterContext::getBspAndRnat(VM_thread *thread, uint64 **bspstore, uint64 *rnat)
{
    CONTEXT *ctx = (CONTEXT *) _pcontext;
#ifdef _IPF_
    BOOL stat;
    while ((stat = GetThreadContext(thread->thread_handle, ctx)) == 0) {
        DWORD error_number = GetLastError();
        printf("Unexpected error from GetThreadContext %d\n", error_number);
        // GetThreadContext should never return zero on NT
        DIE("Unexpected error from GetThreadContext " << error_number);
    }
    *bspstore = (uint64*)ctx->RsBSPSTORE;
    *rnat = ctx->RsRNAT;
#else // !_IPF_
    DIE("VmRegisterContext::getBspAndRnat shouldn't be called under IA-32");
#endif // !_IPF_
}

int VM_thread::setPriority(int priority)
{
    return SetThreadPriority(thread_handle, priority);
}
*/
