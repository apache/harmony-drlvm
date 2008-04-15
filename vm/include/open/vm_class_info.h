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
* @return The data type for field in constant pool entry.
* 
* The <code>idx</code> parameter is interpreted as a constant pool index 
* for JVM.
*/
DECLARE_OPEN(VM_Data_Type, class_cp_get_field_type, (Class_Handle src_class, U_16 idx));

/// Returns 'TRUE' if the entry by the given cp_index is resolved.

#ifdef __cplusplus
}
#endif

#endif // _VM_CLASS_INFO_H
