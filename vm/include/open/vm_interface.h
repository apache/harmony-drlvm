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
PROTOTYPE_WITH_NAME(uint32      , class_get_num_array_dimensions, (Class_Handle cl, unsigned short cpIndex));
PROTOTYPE_WITH_NAME(Class_Handle, class_get_class_of_primitive_type, (VM_Data_Type typ));
PROTOTYPE_WITH_NAME(void*       , class_get_const_string_intern_addr, (Class_Handle cl, unsigned index));
PROTOTYPE_WITH_NAME(VM_Data_Type, class_get_const_type, (Class_Handle cl, unsigned index));
PROTOTYPE_WITH_NAME(const void* , class_get_const_addr, (Class_Handle cl, unsigned index));
PROTOTYPE_WITH_NAME(Method_Handle, class_get_method_by_name, (Class_Handle ch, const char* name));
PROTOTYPE_WITH_NAME(Field_Handle, class_get_field_by_name, (Class_Handle ch, const char* name));
PROTOTYPE_WITH_NAME(ClassLoaderHandle, class_get_class_loader, (Class_Handle ch));

PROTOTYPE_WITH_NAME(Boolean     , class_is_array, (Class_Handle cl));
PROTOTYPE_WITH_NAME(Boolean     , class_is_enum, (Class_Handle ch));
PROTOTYPE_WITH_NAME(Boolean     , class_is_final, (Class_Handle cl)); //class_property_is_final
PROTOTYPE_WITH_NAME(Boolean     , class_is_throwable, (Class_Handle ch)); //class_hint_is_exceptiontype
PROTOTYPE_WITH_NAME(Boolean     , class_is_interface, (Class_Handle cl)); //class_property_is_interface2
PROTOTYPE_WITH_NAME(Boolean     , class_is_abstract, (Class_Handle cl)); //class_property_is_abstract
PROTOTYPE_WITH_NAME(Boolean     , class_is_initialized, (Class_Handle ch)); //class_needs_initialization && class_is_initialized()
PROTOTYPE_WITH_NAME(Boolean     , class_is_finalizable, (Class_Handle ch));
PROTOTYPE_WITH_NAME(Boolean     , class_is_instanceof, (Class_Handle s, Class_Handle t));
PROTOTYPE_WITH_NAME(Boolean     , class_is_support_fast_instanceof, (Class_Handle cl));// class_get_fast_instanceof_flag
PROTOTYPE_WITH_NAME(bool        , class_is_primitive, (Class_Handle vmTypeHandle));

PROTOTYPE_WITH_NAME(Class_Handle, class_lookup_class_by_name_using_bootstrap_class_loader, (const char *name));
PROTOTYPE_WITH_NAME(Method_Handle, class_lookup_method_recursively,
                   (Class_Handle clss,
                    const char *name,
                    const char *descr));

// Const Pool
PROTOTYPE_WITH_NAME(VM_Data_Type, class_cp_get_field_type, (Class_Handle src_class, unsigned short cp_index));// VM_Data_Type class_get_cp_field_type(Class_Handle src_class, unsigned short cp_index);
PROTOTYPE_WITH_NAME(const char* , class_cp_get_entry_signature, (Class_Handle src_class, unsigned short cp_index));//const char*  class_get_cp_entry_signature(Class_Handle src_class, unsigned short index); ? const char*  const_pool_get_field_descriptor(Class_Handle cl, unsigned index);
PROTOTYPE_WITH_NAME(bool        , class_cp_is_entry_resolved, (Compile_Handle ch, Class_Handle clazz, unsigned short cp_index));//bool class_is_cp_entry_resolved(Compile_Handle ch, Class_Handle clazz, unsigned cp_index);
PROTOTYPE_WITH_NAME(const char* , class_cp_get_class_name, (Class_Handle cl, unsigned short cp_index));//const char* const_pool_get_class_name(Class_Handle cl, unsigned index);
PROTOTYPE_WITH_NAME(const char* , class_cp_get_method_class_name,(Class_Handle cl, unsigned index));//const char *const_pool_get_method_class_name(Class_Handle cl, unsigned index);
PROTOTYPE_WITH_NAME(const char* , class_cp_get_method_name, (Class_Handle cl, unsigned index));//const char* const_pool_get_method_name(Class_Handle cl, unsigned index);


//Field

PROTOTYPE_WITH_NAME(void*       , field_get_address, (Field_Handle fh));
PROTOTYPE_WITH_NAME(Class_Handle, field_get_class, (Field_Handle fh));
PROTOTYPE_WITH_NAME(const char* , field_get_descriptor, (Field_Handle fh));
PROTOTYPE_WITH_NAME(const char* , field_get_name, (Field_Handle fh));
PROTOTYPE_WITH_NAME(unsigned    , field_get_offset, (Field_Handle fh));
PROTOTYPE_WITH_NAME(Type_Info_Handle, field_get_type_info, (Field_Handle fh)); //field_get_type_info_of_field_value
PROTOTYPE_WITH_NAME(bool        , field_is_final, (Field_Handle fh));
PROTOTYPE_WITH_NAME(bool        , field_is_magic, (Field_Handle fh)); //Boolean field_is_magic(Field_Handle fh);
PROTOTYPE_WITH_NAME(bool        , field_is_private, (Field_Handle fh));
PROTOTYPE_WITH_NAME(bool        , field_is_static, (Field_Handle fh));
PROTOTYPE_WITH_NAME(Boolean     , field_is_volatile, (Field_Handle fh));

//Method

PROTOTYPE_WITH_NAME(Method_Handle, method_get_overridden_method, (Class_Handle ch, Method_Handle mh));//method_find_overridden_method
PROTOTYPE_WITH_NAME(Byte*       , method_get_info_block_jit, (Method_Handle m, JIT_Handle j));
PROTOTYPE_WITH_NAME(unsigned    , method_get_info_block_size_jit, (Method_Handle m, JIT_Handle j));
PROTOTYPE_WITH_NAME(const char* , method_get_name, (Method_Handle mh));
PROTOTYPE_WITH_NAME(const char* , method_get_descriptor, (Method_Handle mh));
PROTOTYPE_WITH_NAME(const Byte* , method_get_byte_code_addr, (Method_Handle mh));
PROTOTYPE_WITH_NAME(uint32      , method_get_byte_code_size, (Method_Handle mh));
PROTOTYPE_WITH_NAME(uint16      , method_get_max_stack, (Method_Handle mh));
PROTOTYPE_WITH_NAME(uint32      , method_get_num_handlers, (Method_Handle mh));
PROTOTYPE_WITH_NAME(uint32      , method_get_offset, (Method_Handle mh));
PROTOTYPE_WITH_NAME(void*       , method_get_indirect_address, (Method_Handle mh));
PROTOTYPE_WITH_NAME(void*       , method_get_native_func_addr, (Method_Handle mh));
PROTOTYPE_WITH_NAME(uint32      , method_vars_get_number, (Method_Handle mh));
PROTOTYPE_WITH_NAME(unsigned    , method_args_get_number, (Method_Signature_Handle mh));
PROTOTYPE_WITH_NAME(Type_Info_Handle, method_args_get_type_info, (Method_Signature_Handle msh, unsigned idx));
PROTOTYPE_WITH_NAME(Type_Info_Handle, method_ret_type_get_type_info, (Method_Signature_Handle msh));
PROTOTYPE_WITH_NAME(Method_Signature_Handle, method_get_signature, (Method_Handle mh));
PROTOTYPE_WITH_NAME(Class_Handle, method_get_class, (Method_Handle mh));
PROTOTYPE_WITH_NAME(void        , method_get_handler_info,
                   (Method_Handle mh, 
                    unsigned handler_id, unsigned *begin_offset,
                    unsigned *end_offset, unsigned *handler_offset,
                    unsigned *handler_cpindex));
PROTOTYPE_WITH_NAME(Byte*       , method_get_code_block_addr_jit_new,
                   (Method_Handle mh, 
                    JIT_Handle j,
                    int id));
PROTOTYPE_WITH_NAME(unsigned    , method_get_code_block_size_jit_new,
                   (Method_Handle nh,
                    JIT_Handle j,
                    int id));
PROTOTYPE_WITH_NAME(Method_Side_Effects, method_get_side_effects, (Method_Handle mh));

PROTOTYPE_WITH_NAME(bool        , method_has_annotation, (Method_Handle mh, Class_Handle antn_type));
PROTOTYPE_WITH_NAME(bool        , method_is_private, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_static, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_native, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_synchronized, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_final, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_abstract, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_strict, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_overridden, (Method_Handle mh));
PROTOTYPE_WITH_NAME(bool        , method_is_no_inlining, (Method_Handle mh));


PROTOTYPE_WITH_NAME(void        , method_set_side_effects, (Method_Handle mh, Method_Side_Effects mse));
PROTOTYPE_WITH_NAME(void        , method_set_num_target_handlers,
                   (Method_Handle mh,
                    JIT_Handle j,
                    unsigned num_handlers));
PROTOTYPE_WITH_NAME(void        , method_set_target_handler_info,
                   (Method_Handle mh,
                    JIT_Handle j,
                    unsigned eh_number,
                    void *start_ip,
                    void *end_ip,
                    void *handler_ip,
                    Class_Handle catch_cl,
                    bool exc_obj_is_dead));


PROTOTYPE_WITH_NAME(void        , method_lock, (Method_Handle m));
PROTOTYPE_WITH_NAME(void        , method_unlock, (Method_Handle m));

PROTOTYPE_WITH_NAME(Byte*       , method_allocate_code_block,
                   (Method_Handle m,
                    JIT_Handle j,
                    size_t size,
                    size_t alignment,
                    CodeBlockHeat heat,
                    int id,
                    Code_Allocation_Action action));
PROTOTYPE_WITH_NAME(Byte*       , method_allocate_data_block, (Method_Handle m, JIT_Handle j, size_t size, size_t alignment));
PROTOTYPE_WITH_NAME(Byte*       , method_allocate_info_block, (Method_Handle m, JIT_Handle j, size_t size));
PROTOTYPE_WITH_NAME(Byte*       , method_allocate_jit_data_block, (Method_Handle m, JIT_Handle j, size_t size, size_t alignment));


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


PROTOTYPE_WITH_NAME(void        , vm_properties_destroy_keys, (char** keys));//void destroy_properties_keys(char** keys)
PROTOTYPE_WITH_NAME(void        , vm_properties_destroy_value, (char* value));//void destroy_property_value(char* value)
PROTOTYPE_WITH_NAME(char**      , vm_properties_get_keys, (PropertyTable table_number));//char** get_properties_keys(PropertyTable table_number);
PROTOTYPE_WITH_NAME(char**      , vm_properties_get_keys_starting_with, (const char* prefix, PropertyTable table_number));//get_properties_keys_staring_with
PROTOTYPE_WITH_NAME(char*       , vm_properties_get_value, (const char* key, PropertyTable table_number));//char* get_property(const char* key, PropertyTable table_number)


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

PROTOTYPE_WITH_NAME(void *      , vm_helper_get_addr, (VM_RT_SUPPORT id));//void * vm_get_rt_support_addr(VM_RT_SUPPORT id)
PROTOTYPE_WITH_NAME(void *      , vm_helper_get_addr_optimized, (VM_RT_SUPPORT id, Class_Handle ch));//void * vm_get_rt_support_addr_optimized(VM_RT_SUPPORT id, Class_Handle ch)



/////////////////////////// temporary ///////////////////////////
//VTable
PROTOTYPE_WITH_NAME(Class_Handle , vtable_get_class, (VTable_Handle vh));



#endif // _VM_INTERFACE_H
