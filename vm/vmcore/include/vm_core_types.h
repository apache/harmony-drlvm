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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.3.4.3 $
 */  


#ifndef _VM_CORE_TYPES_H_
#define _VM_CORE_TYPES_H_

#include "open/types.h"

// This file names all types commonly used in the VM.
// It defines those types that have no other logical place, other types are defined in header files appropriate to them.

struct String;
struct Class;
class Class_Member;
struct Field;
struct Method;
struct ClassLoader;
class  BootstrapClassLoader;
class  UserDefinedClassLoader;
class  ClassTable;
class  CodeChunkInfo;
class  GcFrame;
struct Intfc_Table;
struct LilCodeStub;
struct M2nFrame;
struct ManagedObject;
class  NativeObjectHandles;
struct VM_thread;
class  Package;
class  Package_Table;
class  Properties;
struct StackIterator;
struct TypeDesc;

typedef void         (*GenericFunctionPointer)();
typedef LilCodeStub* (*NativeStubOverride)(LilCodeStub*, Method_Handle);

// Architectural Registers

#ifdef _IPF_

struct Registers {

    uint64 gr[32];
    uint64 fp[128];
    uint64 br[8];
    uint64 preds;
    uint64 nats;
    uint64 pfs;
    uint64 *bsp;
    uint64 ip;

    void  reset_ip() { ip = 0; }
    void* get_ip() { return (void*)ip; }
    void  set_ip(void* src_ip) { ip = (uint64)src_ip; }
    void* get_sp() { return (void*)bsp; }
    void  set_sp(void* src_sp) { }
}; //Registers

#else  // !_IPF_

#ifdef _EM64T_

struct Registers {
    uint64 rsp;
    uint64 rbp;
    uint64 rip;
    // callee-saved
    uint64 rbx;
    uint64 r12;
    uint64 r13;
    uint64 r14;
    uint64 r15;
    // scratched
    uint64 rax;
    uint64 rcx;
    uint64 rdx;
    uint64 rsi;
    uint64 rdi;
    uint64 r8;
    uint64 r9;
    uint64 r10;
    uint64 r11;

    uint32 eflags;

    void  reset_ip() { rip = 0; }
    void* get_ip() { return (void*)rip; }
    void  set_ip(void* src_ip) { rip = (uint64)src_ip; }
    void* get_sp() { return (void*)rsp; }
    void  set_sp(void* src_sp) { rsp = (uint64)src_sp; }
}; //Registers

#else // ! _EM64T_

struct Registers {
    uint32 eax;
    uint32 ebx;
    uint32 ecx;
    uint32 edx;
    uint32 edi;
    uint32 esi;
    uint32 ebp;
    uint32 esp;
    uint32 eip;
    uint32 eflags;

    void  reset_ip() { eip = 0; }
    void* get_ip() { return (void*)eip; }
    void  set_ip(void* src_ip) { eip = (uint32)src_ip; }
    void* get_sp() { return (void*)esp; }
    void  set_sp(void* src_sp) { esp = (uint32)src_sp; }
}; //Registers

#endif // _EM64T_

#endif //!_IPF_

#endif //!_VM_CORE_TYPES_H_
