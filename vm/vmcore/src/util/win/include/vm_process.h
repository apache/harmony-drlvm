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
#ifndef _VM_PROCESS_H_
#define _VM_PROCESS_H_

//
// This file holds the routines needed to implement process related functionality across VM/GC/JIT
// projects. The calls are inlined so that the JIT and GC can use this .h file without having to 
// deal with the overhead of doing .dll calls.
//
// While these can be implemented by direct calls to the related windows API, the Linux and other 
// versions are not as robust as the windows API. This will be dealt with on a case by case basis
// either by documenting this file or by improving the robustness of the systems.
//

#include "platform_lowlevel.h"
#include <process.h>
#include <windows.h>
#include <assert.h>

inline VmEventHandle vm_beginthreadex( void * security, unsigned stack_size, unsigned(__stdcall *start_address)(void *), void *arglist, unsigned initflag, pthread_t *thrdaddr)
{
    return (VmEventHandle) _beginthreadex(security, stack_size, start_address, arglist, initflag, thrdaddr);
}

inline void vm_endthreadex(int value)
{
    _endthreadex(value);
}

inline VmEventHandle vm_beginthread(void(__cdecl *start_address)(void *), unsigned stack_size, void *arglist)
{
    uintptr_t result = _beginthread(start_address, stack_size, arglist);
    // on windows error code is -1 !!!
    if (result == -1L)
        return NULL;

    return (VmEventHandle) result;
}

inline void vm_endthread(void)
{
    _endthread();
}

inline VmEventHandle vm_create_event(int *lpEventAttributes, unsigned int bManualReset, unsigned int bInitialState, char *lpName)
{
    assert(lpEventAttributes == NULL);
    return CreateEvent(NULL, bManualReset, bInitialState, lpName);
}

inline BOOL vm_destroy_event( VmEventHandle handle )
{
    assert( handle != INVALID_HANDLE_VALUE );
    return CloseHandle( handle ) == FALSE ? 0 : 1;
}

inline BOOL vm_reset_event(VmEventHandle hEvent)
{
    return ResetEvent(hEvent);
}

inline BOOL vm_set_event(VmEventHandle hEvent)
{
    return SetEvent(hEvent);
}

inline void vm_yield() {
    Sleep(0);
}

inline DWORD vm_wait_for_single_object(VmEventHandle hHandle, DWORD dwMilliseconds)
{
    return WaitForSingleObject(hHandle, dwMilliseconds);
}

inline DWORD vm_wait_for_multiple_objects(DWORD num, const VmEventHandle * handle, BOOL flag, DWORD dwMilliseconds)
{
    return WaitForMultipleObjects(num, handle, flag, dwMilliseconds);
}

inline void vm_terminate_thread(VmThreadHandle thrdaddr) {
    TerminateThread(thrdaddr,0);    
}
#endif
