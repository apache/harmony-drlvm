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

#ifndef _PORT_THREAD_H_
#define _PORT_THREAD_H_

/**
 * @file port_thread.h
 * @brief PORT thread support
 */

/* osthread_t and thread_context_t types, and proper windows.h inclusion */
#include "open/hythread_ext.h"

#include "port_general.h"

/* Thread context definition for UNIX-like systems */
#if defined(LINUX) || defined(FREEBSD) 
#if defined(LINUX)

#include <sys/types.h>
#include <linux/unistd.h>
#include <errno.h>

#ifdef _syscall0
static _syscall0(pid_t, gettid)/* static definition */
#else /* _syscall0 */
#include <sys/syscall.h>
#include <unistd.h>
#define gettid() ((pid_t)syscall(__NR_gettid))
#endif /* _syscall0 */

#else /* !LINUX */
#define gettid() getpid()
#endif

#endif /* LINUX || FREEBSD */

/* To skip platform_types.h inclusion */
typedef struct Registers Registers;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/** @name OS thread operations
 */
//@{

PORT_INLINE int port_gettid()
{
#ifdef PLATFORM_POSIX
    return gettid();
#else
    return (int)GetCurrentThreadId();
#endif
}

void port_thread_yield_other(osthread_t thread);
int port_thread_cancel(osthread_t os_thread);


int port_thread_suspend(osthread_t thread);
int port_thread_resume(osthread_t thread);
int port_thread_get_suspend_count(osthread_t thread);

int port_thread_get_context(osthread_t thread, thread_context_t* pcontext);
int port_thread_set_context(osthread_t thread, thread_context_t* pcontext);

void port_thread_context_to_regs(Registers* regs, thread_context_t* context);
void port_thread_regs_to_context(thread_context_t* context, Registers* regs);

/* Transfer control to specified register context */
void port_transfer_to_regs(Registers* regs);

/**
* Prepares 'Registers' structure and stack area pointed in for calling
* 'fn' function with a set of arguments provided in variable args list.
* THe 'fn' function is called through a special stub function with
* preserving 'red zone' on Linux and clearing direction flag on Windows.
* After returning from 'fn' and stub, processor registers are restored
* with a values provided in 'regs' argument.
* The function can be used to prepare register context for transfering
* a control to a signal/exception handling function out of the OS handler.
*
* When the first argument passed to 'fn' is the same 'regs' pointer, its
* value is substituted with the pointer stored 'Registers' structure used
* to restore register context. If 'fn' function modifies the context
* pointed by the first argument, these changes will take effect after
* returning from 'fn'.
*
* The stub for calling 'fn' is written in assembler language; 'Registers'
* fields and size are hardcoded. It would be better to rewrite it using
* encoder in future, to keep control on 'Registers' structure and size.
*
* @param [in] fn    - the address of the function to be called
* @param [in] regs  - the register context
* @param [in] num   - the number of parameters passed to the 'fn' function
*                     in the variable args list (6 args at maximum)
* @param [in] ...   - the parameters for 'fn'; should all be void* or of
*                     the same size (pointer-sized)
*/
void port_set_longjump_regs(void* fn, Registers* regs, int num, ...);

/**
* The same as 'port_set_longjump_regs', but transfers a control to the
* prepared registers context by itself.
* Actually it's a combination of 'port_set_longjump_regs' and
* 'port_transfer_to_regs' functions, but 'regs' fields are kept unchanged.
*
* @param [in] fn    - the address of the function to be called
* @param [in] regs  - the register context
* @param [in] num   - the number of parameters passed to the 'fn' function
*                     in the variable args list (6 args at maximum)
* @param [in] ...   - the parameters for 'fn'; should all be void* or of
*                     the same size (pointer-sized)
*/
void port_transfer_to_function(void* fn, Registers* regs, int num, ...);



//@}

#ifdef __cplusplus
}
#endif

#endif  /* _PORT_THREAD_H_ */
