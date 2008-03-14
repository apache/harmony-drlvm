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

#ifndef _JIT_INTF_H_
#define _JIT_INTF_H_


#include "open/types.h"
#include "open/vm.h"
#include "jit_export.h"
#include "jit_import.h"

#ifdef __cplusplus
extern "C" {
#endif


//////////////// begin C interface
//
// calls from Compiler to VM
//
//-----------------------------------------------------------------------------
// Constant pool resolution
//-----------------------------------------------------------------------------
//
// The following byte codes reference constant pool entries:
//
//  field
//      getstatic           static field
//      putstatic           static field
//      getfield            non-static field
//      putfield            non-static field
//
//  method
//      invokevirtual       virtual method
//      invokespecial       special method
//      invokestatic        static method
//      invokeinterface     interface method
//
//  class
//      new                 class
//      anewarray           class
//      checkcast           class
//      instanceof          class
//      multianewarray      class
//

//
// For the method invocation byte codes, certain linkage exceptions are thrown 
// at run-time:
//
//  (1) invocation of a native methods throws the UnsatisfiedLinkError if the 
//      code that implements the method cannot be loaded or linked.
//  (2) invocation of an interface method throws 
//          - IncompatibleClassChangeError if the object does not implement 
//            the called method, or the method is implemented as static
//          - IllegalAccessError if the implemented method is not public
//          - AbstractMethodError if the implemented method is abstract
//

VMEXPORT Field_Handle
resolve_field(Compile_Handle h, Class_Handle c, unsigned index);

//
// resolve constant pool reference to a virtual method
// used for invokevirtual and invoke special.
//
// DEPRECATED
//
VMEXPORT Method_Handle 
resolve_nonstatic_method(Compile_Handle h, Class_Handle c, unsigned index);

//
// resolve constant pool reference to a virtual method
// used for invokespecial
//
VMEXPORT Method_Handle 
resolve_special_method(Compile_Handle h, Class_Handle c, unsigned index);

//
// resolve constant pool reference to a class
// used for
//      (1) new 
//              - InstantiationError exception if resolved class is abstract
//      (2) anewarray
//      (3) multianewarray
//
// resolve_class_new is used for resolving references to class entries by the
// the new byte code.
//
VMEXPORT Class_Handle 
resolve_class_new(Compile_Handle h, Class_Handle c, unsigned index);

//
// resolve_class is used by all the other byte codes that reference classes,
// as well as exception handlers.
//
VMEXPORT Class_Handle 
resolve_class(Compile_Handle h, Class_Handle c, unsigned index);

//
// Field
//
VMEXPORT Boolean      field_is_public(Field_Handle f);
VMEXPORT Boolean      field_is_injected(Field_Handle f);
VMEXPORT Boolean      method_is_public(Method_Handle m);
VMEXPORT unsigned     method_get_max_locals(Method_Handle m);
VMEXPORT Boolean      method_is_fake(Method_Handle m);


VMEXPORT Method_Side_Effects method_get_side_effects(Method_Handle m);
VMEXPORT void method_set_side_effects(Method_Handle m, Method_Side_Effects mse);


VMEXPORT Class_Handle method_get_return_type_class(Method_Handle m);


VMEXPORT Boolean method_has_annotation(Method_Handle target, Class_Handle antn_type);


VMEXPORT unsigned     class_number_fields(Class_Handle ch);
VMEXPORT Field_Handle class_get_field(Class_Handle ch, unsigned idx);
VMEXPORT int          class_get_super_offset();

VMEXPORT Field_Handle   class_get_field_by_name(Class_Handle ch, const char* name);
VMEXPORT Method_Handle  class_get_method_by_name(Class_Handle ch, const char* name);

VMEXPORT int          class_get_depth(Class_Handle cl);
VMEXPORT const char  *class_get_source_file_name(Class_Handle cl);

VMEXPORT ClassLoaderHandle class_get_class_loader(Class_Handle c);

VMEXPORT Class_Handle
class_load_class_by_name(const char *name,
                         Class_Handle c);

VMEXPORT Class_Handle
class_load_class_by_descriptor(const char *descr,
                               Class_Handle c);

VMEXPORT Method_Handle
class_lookup_method_recursively(Class_Handle clss,
                                const char *name,
                                const char *descr);

// This function is for native library support
// It takes a class name with .s not /s.
VMEXPORT Class_Handle class_find_loaded(ClassLoaderHandle, const char*);

// This function is for native library support
// It takes a class name with .s not /s.
VMEXPORT Class_Handle class_find_class_from_loader(ClassLoaderHandle, const char*, Boolean init);



/**
 * Adds information about inlined method.
 * @param[in] method - method which is inlined
 * @param[in] codeSize - size of inlined code block
 * @param[in] codeAddr - size of inlined code block
 * @param[in] mapLength - number of AddrLocation elements in addrLocationMap
 * @param[in] addrLocationMap - native addresses to bytecode locations
 *       correspondence table
 * @param[in] compileInfo - VM specific information.
 * @param[in] outer_method - target method to which inlining was made
 */
DECLARE_OPEN(void, compiled_method_load, (Method_Handle method, U_32 codeSize, 
                                  void* codeAddr, U_32 mapLength, 
                                  AddrLocation* addrLocationMap, 
                                  void* compileInfo, Method_Handle outer_method));

//////////////// end C interface

#ifdef __cplusplus
}
#endif

#endif
