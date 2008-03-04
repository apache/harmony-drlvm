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

#ifndef _SIGNALS_COMMON_H_
#define _SIGNALS_COMMON_H_

#include <sys/ucontext.h>
#include "vm_core_types.h"


#ifdef _IPF_
#error IPF architecture is not adopted for unified signal handling
#endif

#ifdef _EM64T_
// vvvvvvvv EM64T vvvvvvvv

inline size_t get_mem_protect_stack_size() { return 0x0400; }

inline size_t get_restore_stack_size() {
    return get_mem_protect_stack_size() + 0x0400;
}

#define DECL_CHANDLER __attribute__ ((used))

// ^^^^^^^^ EM64T ^^^^^^^^
#else // #ifdef _EM64T_
// vvvvvvvv IA-32 vvvvvvvv

inline size_t get_mem_protect_stack_size() { return 0x0100; }

inline size_t get_restore_stack_size() {
    return get_mem_protect_stack_size() + 0x0100;
}

#define DECL_CHANDLER __attribute__ ((used, cdecl))

// ^^^^^^^^ IA-32 ^^^^^^^^
#endif // #ifdef _EM64T_


// Linux/FreeBSD defines
#if defined(FREEBSD)
#define STACK_MMAP_ATTRS \
    (MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_STACK)
#else
#define STACK_MMAP_ATTRS \
    (MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN)
#endif


#endif // _SIGNALS_COMMON_H_
