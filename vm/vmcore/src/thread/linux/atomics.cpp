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
#include "atomics.h"


void MemoryReadWriteBarrier() {
#if defined(_EM64T_)
    asm volatile ("mfence" : : : "memory");
#elif defined(_IPF_)
    asm volatile ("mf" : : : "memory");
#else // General x86 case
    /*
     * This code must use a lock-prefixed assembly instruction, so that 
     * we can support P3 processors (SSE2 only). With P4 and SSE3, we 
     * could use 'mfence'. 
     * References:
     * Java Memory Model cookbook 
     *      - http://gee.cs.oswego.edu/dl/jmm/cookbook.html
     * Linux Kernel, mb() function 
     *      - http://lxr.linux.no/source/include/asm-i386/system.h
     * This is a GCC inline assembly command. The final bit, "memory", will
     * clobber all of the memory registers.
     */
    asm volatile ("lock; addl $0,0(%%esp)" : : : "memory");
#endif
}

void MemoryWriteBarrier() {
#if defined(_IPF_)
    asm volatile ("mf" : : : "memory");
#else // General x86 and x86_64 case
    /*
     * We could use the same lock-prefixed assembly instruction above,
     * but since we have support for P3 processors (SSE2) we'll just 
     * use 'sfence'.
     */
     asm volatile ("sfence" : : : "memory");
#endif
}

