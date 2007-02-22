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

#ifndef _PORT_ATOMIC_H_
#define _PORT_ATOMIC_H_

#include "open/types.h"
#include "port_general.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
#define INLINE inline
#else 
#ifdef WIN32
#define INLINE __forceinline
#else 
#define INLINE static
#endif
#endif
/**
 * All atomic operrations are perfomance critical, 
 * thus they are defined as inlined for most platforms in this file
 */
 
 #if defined(_IPF_)

    
/**
* Atomic compare and exchange operation
* @data[in, out]
* @value[in] new value
* @comp[in] value to compare with
* return old value
*/
APR_DECLARE(uint8) port_atomic_cas8(volatile uint8 * data, 
                                               uint8 value, uint8 comp);

/**
* Atomic compare and exchange operation
* @data[in, out]
* @value[in] new value
* @comp[in] value to compare with
* return old value
*/
APR_DECLARE(uint16) port_atomic_cas16(volatile uint16 * data, 
                                                 uint16 value, uint16 comp);

/**
* Atomic compare and exchange operation
* @data[in, out]
* @value[in] new value
* @comp[in] value to compare with
* return old value
*/
APR_DECLARE(uint64) port_atomic_cas64(volatile uint64 * data, 
                                                 uint64 value, uint64 comp);

#elif defined(WIN32) && !defined(_WIN64)

INLINE uint8 port_atomic_cas8(volatile uint8 * data , uint8 value, uint8 comp) {
    __asm {
        mov al,  comp
        mov dl,  value
        mov ecx, data
        lock cmpxchg [ecx], dl
        mov comp, al
    }
    return comp;
}

INLINE uint16 port_atomic_cas16(volatile uint16 * data , uint16 value, uint16 comp) {
    __asm {
        mov ax,  comp
        mov dx,  value
        mov ecx, data
        lock cmpxchg [ecx], dx
        mov comp, ax
    }
    return comp;
}

INLINE uint64 port_atomic_cas64(volatile uint64 * data , uint64 value, uint64 comp) {
    __asm {
        lea esi, comp
        mov eax, [esi]
        mov edx, [esi] + 4
            
        lea esi, value
        mov ebx, [esi]
        mov ecx, [esi] + 4
            
        mov esi, data
        lock cmpxchg8b [esi]
        
        lea esi, comp
        mov [esi], eax
        mov [esi] + 4, edx
    }
    return comp;
}

#elif defined(_EM64T_) && defined (_WIN64)

#pragma intrinsic(_InterlockedCompareExchange16);
#pragma intrinsic(_InterlockedCompareExchange64);

APR_DECLARE(uint8) port_atomic_cas8(volatile uint8 * data, 
                                               uint8 value, uint8 comp);

INLINE uint16 port_atomic_cas16(volatile uint16 * data, 
                                                 uint16 value, uint16 comp)
{
    return _InterlockedCompareExchange16((volatile SHORT *)data, value, comp);
}    

INLINE uint64 port_atomic_cas64(volatile uint64 * data, 
                                                 uint64 value, uint64 comp)
{
    return _InterlockedCompareExchange64((volatile LONG64 *)data, value, comp);
}
    

#elif defined (PLATFORM_POSIX)  

INLINE uint8 port_atomic_cas8(volatile uint8 * data , uint8 value, uint8 comp) {
#if defined(_IA32_) || defined(_EM64T_)
    __asm__ __volatile__(
        "lock cmpxchgb %1, (%2)"
        :"=a"(comp)
        :"d"(value), "r"(data), "0"(comp)
    );
    return comp;
#else
    ABORT("Not supported");
#endif
}

INLINE uint16 port_atomic_cas16(volatile uint16 * data , uint16 value, uint16 comp) {
    uint16 ret;
#if defined(_IA32_) || defined(_EM64T_)
    __asm__ __volatile__(
        "lock cmpxchgw %w1, %2"
        :"=a"(ret)
        :"q"(value), "m"(*data), "0"(comp)
        : "memory"
    );
    return ret;
#else
    ABORT("Not supported");
#endif
}

INLINE uint64 port_atomic_cas64(volatile uint64 * data , uint64 value, uint64 comp) {
#if defined(_IA32_)
    __asm__ __volatile__(
        "push %%ebx;\n\t"
        "lea %0, %%esi;\n\t"
        "mov (%%esi), %%eax;\n\t"
        "mov 4(%%esi), %%edx;\n\t"
        "lea %1, %%esi;\n\t"
        "mov (%%esi), %%ebx;\n\t"
        "mov 4(%%esi), %%ecx;\n\t"
        "mov %2, %%esi;\n\t"
        "lock cmpxchg8b (%%esi);\n\t"
        "lea %0, %%esi;\n\t"
        "mov %%eax, (%%esi);\n\t"
        "mov %%edx, 4(%%esi);\n\t"
        "pop %%ebx"
        : /* no outputs (why not comp?)*/
        :"m"(comp), "m"(value), "m"(data) /* inputs */
        :"%eax", "%ecx", "%edx", "%esi" /* clobbers */
    );
    return comp;
#elif defined(_EM64T_) // defined(_IA32_)
    __asm__ __volatile__(
        "lock cmpxchgq %1, (%2)"
        :"=a"(comp) /* outputs */
        :"d"(value), "r"(data), "a"(comp)
    );
    return comp;
#else
    ABORT("Not supported");
#endif
}

INLINE void * port_atomic_compare_exchange_pointer(volatile void ** data, void * value, const void * comp) {
#if defined(_IA32_)
    //return (void *) port_atomic_compare_exchange32((uint32 *)data, (uint32)value, (uint32)comp);
    uint32 Exchange = (uint32)value;
    uint32 Comperand = (uint32)comp;
    __asm__ __volatile__(
        "lock cmpxchgl %1, (%2)"
        :"=a"(Comperand)
        :"d"(Exchange), "r"(data), "a"(Comperand) 
        );
    return (void*)Comperand;

#elif defined(_EM64T_) // defined(_IA32_)
    //return (void *) port_atomic_cas64((uint64 *)data, (uint64)value, (uint64)comp);
    uint64 Exchange = (uint64)value;
    uint64 Comperand = (uint64)comp;
    __asm__(
        "lock cmpxchgq %1, (%2)"
        :"=a"(Comperand)
        :"d"(Exchange), "r"(data), "a"(Comperand) 
        );
    return (void *)Comperand;

#else // defined(_EM64T_)
    ABORT("Not supported");
#endif
}

#endif //defined (POSIX)



#ifdef __cplusplus
}
#endif

#endif /* _PORT_ATOMIC_H_ */
