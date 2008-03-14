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
#ifndef _VM_CLASS_INFO_H
#define _VM_CLASS_INFO_H

#include "open/types.h"
/**
 * @file
 * Part of Class Support interface related to inquiring class data.
 * These functions do not have any side effect, such as  
 * classloading or resolving of constant pool entries. 
 * So they are safe for using out of execution context, 
 * e.g. for asynchronous compilation.
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @return A UTF8 representation of a string declared in a class.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool 
 * index for JVM.
 * This method is generally only for JIT internal use,
 * e.g. printing a string pool constant in a bytecode disassembler.
 * The resulting const char* should of course not be inserted into
 * the jitted code.
 */
DECLARE_OPEN(const char *, class_cp_get_const_string, (Class_Handle ch, U_16 idx));


/**
 * @return The address where the interned version of the string
 *         is stored. 
 * 
 * Calling <code>class_get_const_string_intern_addr</code> has
 * a side-effect of interning the string, so that the JIT can
 * load a reference to the interned string without checking if
 * it is null.
 * FIXME the above side effect is no longer true.
 * FIXME rename??
 */
DECLARE_OPEN(const void *, class_get_const_string_intern_addr, (Class_Handle ch, U_16 idx));

/**
 * @return A pointer to the location where the constant is stored.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 * This function shouldn't be called for constant strings. Instead, either:<br>
 * <ul><li>The jitted code should get the string object at runtime by calling
 *         <code>VM_RT_LDC_STRING</code>, or
 *     <li>Use class_get_const_string_intern_addr().
 *</ul>
 */
DECLARE_OPEN(const void *, class_cp_get_const_addr, (Class_Handle ch, U_16 idx));

/**
 * @return The type of a compile-time constant.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */
DECLARE_OPEN(VM_Data_Type, class_cp_get_const_type, (Class_Handle ch, U_16 idx));

/**
* @return The data type for field in constant pool entry.
* 
* The <code>idx</code> parameter is interpreted as a constant pool index 
* for JVM.
*/
DECLARE_OPEN(VM_Data_Type, class_cp_get_field_type, (Class_Handle src_class, U_16 idx));

/**
* @return The signature for field or (interface) method in constant pool entry.
* The <code>idx</code> parameter is interpreted as a constant pool index 
* for JVM.
*/
DECLARE_OPEN(const char *, class_cp_get_entry_signature, (Class_Handle src_class, U_16 idx));

DECLARE_OPEN(U_32, class_cp_get_num_array_dimensions, (Class_Handle cl, U_16 cpIndex));
/// Returns 'TRUE' if the entry by the given cp_index is resolved.
DECLARE_OPEN(BOOLEAN, class_cp_is_entry_resolved, (Class_Handle clazz, U_16 cp_index));

DECLARE_OPEN(const char *, class_cp_get_field_name, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_field_class_name, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_method_name, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_method_class_name, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_class_name, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_interface_method_name, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_interface_method_class_name, (Class_Handle cl, U_16 index));

//FIXME redundant, replace with class_cp_get_entry_signature
DECLARE_OPEN(const char *, class_cp_get_interface_method_descriptor, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_method_descriptor, (Class_Handle cl, U_16 index));
DECLARE_OPEN(const char *, class_cp_get_field_descriptor, (Class_Handle cl, U_16 index));

#ifdef __cplusplus
}
#endif

#endif // _VM_CLASS_INFO_H
