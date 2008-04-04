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

#ifndef _VM_INTERFACE_H
#define _VM_INTERFACE_H

#include "open/hycomp.h"
#include "open/types.h"
#include "open/rt_types.h"
#include "open/rt_helpers.h"
#include "open/em.h"

#define PROTOTYPE_WITH_NAME(return_type, func_name, prototype) \
    typedef return_type (*func_name##_t)prototype

#define GET_INTERFACE(get_adapter, func_name) \
    (func_name##_t)get_adapter(#func_name)


PROTOTYPE_WITH_NAME(void*, get_vm_interface, (const char* func_name));


// Allocation Handle
PROTOTYPE_WITH_NAME(Class_Handle, allocation_handle_get_class, (Allocation_Handle ah));

//Class
PROTOTYPE_WITH_NAME(Class_Handle, class_get_array_element_class, (Class_Handle cl));
PROTOTYPE_WITH_NAME(unsigned    , class_get_array_element_size, (Class_Handle ch)); 
PROTOTYPE_WITH_NAME(Class_Handle, class_get_array_of_class, (Class_Handle cl));
PROTOTYPE_WITH_NAME(Type_Info_Handle, class_get_element_type_info, (Class_Handle ch));
PROTOTYPE_WITH_NAME(const char* , class_get_name, (Class_Handle cl));
PROTOTYPE_WITH_NAME(Class_Handle, class_get_super_class, (Class_Handle cl));
PROTOTYPE_WITH_NAME(int         , class_get_depth, (Class_Handle cl));
PROTOTYPE_WITH_NAME(VTable_Handle, class_get_vtable, (Class_Handle cl));
PROTOTYPE_WITH_NAME(Allocation_Handle, class_get_allocation_handle, (Class_Handle ch));
PROTOTYPE_WITH_NAME(unsigned    , class_get_boxed_data_size, (Class_Handle ch));

PROTOTYPE_WITH_NAME(Class_Handle, class_get_class_of_primitive_type, (VM_Data_Type typ));
PROTOTYPE_WITH_NAME(Method_Handle, class_get_method_by_name, (Class_Handle ch, const char* name));
PROTOTYPE_WITH_NAME(Field_Handle, class_get_field_by_name, (Class_Handle ch, const char* name));
PROTOTYPE_WITH_NAME(ClassLoaderHandle, class_get_class_loader, (Class_Handle ch));

PROTOTYPE_WITH_NAME(Boolean     , class_is_array, (Class_Handle cl));
PROTOTYPE_WITH_NAME(Boolean     , class_is_enum, (Class_Handle ch));
PROTOTYPE_WITH_NAME(Boolean     , class_is_final, (Class_Handle cl)); //class_is_final
PROTOTYPE_WITH_NAME(Boolean     , class_is_throwable, (Class_Handle ch)); //class_hint_is_exceptiontype
PROTOTYPE_WITH_NAME(BOOLEAN     , class_is_interface, (Class_Handle cl)); //class_is_interface2
PROTOTYPE_WITH_NAME(Boolean     , class_is_abstract, (Class_Handle cl)); //class_is_abstract
PROTOTYPE_WITH_NAME(Boolean     , class_is_initialized, (Class_Handle ch)); //class_needs_initialization && class_is_initialized()
PROTOTYPE_WITH_NAME(BOOLEAN     , class_is_finalizable, (Class_Handle ch));
PROTOTYPE_WITH_NAME(Boolean     , class_is_instanceof, (Class_Handle s, Class_Handle t));
PROTOTYPE_WITH_NAME(Boolean     , class_is_support_fast_instanceof, (Class_Handle cl));// class_get_fast_instanceof_flag
PROTOTYPE_WITH_NAME(Boolean     , class_is_primitive, (Class_Handle vmTypeHandle));

PROTOTYPE_WITH_NAME(Class_Handle, class_lookup_class_by_name_using_bootstrap_class_loader, (const char *name));
PROTOTYPE_WITH_NAME(Method_Handle, class_lookup_method_recursively,
                   (Class_Handle clss,
                    const char *name,
                    const char *descr));

//Method

PROTOTYPE_WITH_NAME(Byte*       , method_get_code_block_addr_jit_new,
                   (Method_Handle mh, 
                    JIT_Handle j,
                    int id));
PROTOTYPE_WITH_NAME(unsigned    , method_get_code_block_size_jit_new,
                   (Method_Handle nh,
                    JIT_Handle j,
                    int id));

//Object
PROTOTYPE_WITH_NAME(int         , object_get_vtable_offset, ());

//Resolve
PROTOTYPE_WITH_NAME(Class_Handle, resolve_class, (Compile_Handle h, Class_Handle c, unsigned index));
PROTOTYPE_WITH_NAME(Class_Handle, resolve_class_new, (Compile_Handle h, Class_Handle c, unsigned index));
PROTOTYPE_WITH_NAME(Method_Handle, resolve_special_method, (Compile_Handle h, Class_Handle c, unsigned index));
PROTOTYPE_WITH_NAME(Method_Handle, resolve_interface_method, (Compile_Handle h, Class_Handle c, unsigned index));
PROTOTYPE_WITH_NAME(Method_Handle, resolve_static_method, (Compile_Handle h, Class_Handle c, unsigned index));
PROTOTYPE_WITH_NAME(Method_Handle, resolve_virtual_method, (Compile_Handle h, Class_Handle c, unsigned index));
PROTOTYPE_WITH_NAME(Field_Handle, resolve_nonstatic_field,
                   (Compile_Handle h,
                    Class_Handle c, 
                    unsigned index,
                    unsigned putfield));
PROTOTYPE_WITH_NAME(Field_Handle, resolve_static_field,
                   (Compile_Handle h,
                    Class_Handle c,
                    unsigned index,
                    unsigned putfield));

//Type Info

//Vector
PROTOTYPE_WITH_NAME(int         , vector_get_first_element_offset, (Class_Handle element_type)); //vector_first_element_offset_class_handle
PROTOTYPE_WITH_NAME(int         , vector_get_length_offset, ()); //vector_length_offset

//Vm
PROTOTYPE_WITH_NAME(IDATA       , vm_tls_alloc, (UDATA* key)); //IDATA VMCALL hythread_tls_alloc(hythread_tls_key_t *handle) 
PROTOTYPE_WITH_NAME(UDATA       , vm_tls_get_offset, (UDATA key)); //UDATA VMCALL hythread_tls_get_offset(hythread_tls_key_t key)
PROTOTYPE_WITH_NAME(UDATA       , vm_tls_get_request_offset, ()); //DATA VMCALL hythread_tls_get_request_offset
PROTOTYPE_WITH_NAME(UDATA       , vm_tls_is_fast, (void));//UDATA VMCALL hythread_uses_fast_tls
PROTOTYPE_WITH_NAME(IDATA       , vm_get_tls_offset_in_segment, (void));//IDATA VMCALL hythread_get_hythread_offset_in_tls(void)

PROTOTYPE_WITH_NAME(Class_Handle, vm_get_system_object_class, ()); // get_system_object_class
PROTOTYPE_WITH_NAME(Class_Handle, vm_get_system_class_class, ()); // get_system_class_class
PROTOTYPE_WITH_NAME(Class_Handle, vm_get_system_string_class, ()); // get_system_string_class
PROTOTYPE_WITH_NAME(void*       , vm_get_vtable_base, ()); //POINTER_SIZE_INT vm_get_vtable_base()
PROTOTYPE_WITH_NAME(void*       , vm_get_heap_base_address, ()); //vm_heap_base_address
PROTOTYPE_WITH_NAME(void*       , vm_get_heap_ceiling_address, ()); //vm_heap_ceiling_address
PROTOTYPE_WITH_NAME(Boolean     , vm_is_heap_compressed, ());//vm_references_are_compressed();
PROTOTYPE_WITH_NAME(Boolean     , vm_is_vtable_compressed, ());//vm_vtable_pointers_are_compressed();
PROTOTYPE_WITH_NAME(void        , vm_patch_code_block, (Byte *code_block, Byte *new_code, size_t size));
PROTOTYPE_WITH_NAME(JIT_Result  , vm_compile_method, (JIT_Handle jit, Method_Handle method));
PROTOTYPE_WITH_NAME(void        , vm_register_jit_extended_class_callback,
                   (JIT_Handle jit,
                    Class_Handle clss,
                    void* callback_data));
PROTOTYPE_WITH_NAME(void        , vm_register_jit_overridden_method_callback,
                   (JIT_Handle jit,
                   Method_Handle method,
                   void* callback_data));
PROTOTYPE_WITH_NAME(void        , vm_register_jit_recompiled_method_callback,
                   (JIT_Handle jit,
                    Method_Handle method,
                    void *callback_data));
PROTOTYPE_WITH_NAME(void        , vm_compiled_method_load,
                   (Method_Handle method, uint32 codeSize, 
                    void* codeAddr,
                    uint32 mapLength, 
                    AddrLocation* addrLocationMap, 
                    void* compileInfo,
                    Method_Handle outer_method));//compiled_method_load


/////////////////////////// temporary ///////////////////////////
//VTable
PROTOTYPE_WITH_NAME(Class_Handle , vtable_get_class, (VTable_Handle vh));



#endif // _VM_INTERFACE_H
