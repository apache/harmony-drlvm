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
 * @author Intel, Evgueni Brevnov, Ivan Volosyuk
 * @version $Revision: 1.1.2.1.4.4 $
 */  





#ifndef _JIT_RUNTIME_SUPPORT_COMMON_H_
#define _JIT_RUNTIME_SUPPORT_COMMON_H_

#include "lil.h"

#include "heap.h"

VMEXPORT // temporary solution for interpreter unplug
int __stdcall vm_instanceof(ManagedObject *obj, Class *c) stdcall__;

// Implements VM_RT_AASTORE
void * __stdcall
vm_rt_aastore(ManagedObject *elem, int idx, Vector_Handle array) stdcall__;

// Implements VM_RT_AASTORE_TEST
int __stdcall
vm_aastore_test(ManagedObject *elem, Vector_Handle array) stdcall__;


void *vm_get_interface_vtable(ManagedObject *obj, Class *iid);

void vm_instanceof_update_stats(ManagedObject *obj, Class *super);
void vm_checkcast_update_stats(ManagedObject *obj, Class *super);
void vm_aastore_test_update_stats(ManagedObject *elem, Vector_Handle array);

void vm_rt_class_initialize(Class *clss);
void vm_rt_class_throw_linking_error(Class_Handle ch, unsigned index, unsigned opcode);

ManagedObject* vm_rt_class_alloc_new_object(Class *c);
Vector_Handle vm_rt_new_vector(Class *vector_class, int length);
Vector_Handle vm_rt_new_vector_using_vtable_and_thread_pointer(
        int length, Allocation_Handle vector_handle, void *tp);

// creates a LIL code stub for checkcast or instanceof
// can be used by both IA32 and IPF code
LilCodeStub *gen_lil_typecheck_stub(bool is_checkcast);

// creates a SPECIALIZED LIL code stub for checkcast or instanceof
// it assumes that the class is suitable for fast instanceof checks.
// The result is a different fast stub for every class.
// will_inline should be set to TRUE if this stub will be inlined
// in a JIT, and false if it will be passed to a code generator
// (this is due to the slightly different treatment of exceptions)
LilCodeStub *gen_lil_typecheck_stub_specialized(bool is_checkcast,
                                                bool will_inline,
                                                Class *superclass);

#endif
