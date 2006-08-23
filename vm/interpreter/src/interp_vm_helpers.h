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
 * @author Ivan Volosyuk
 * @version $Revision: 1.2.12.2.4.3 $
 */  
#include "Class.h"



/**
 * Set thread-local exception with name 'exc'.
 */
void interp_throw_exception(const char* exc);

/**
 * Set thread-local exception with name 'exc' and 'message'.
 */
void interp_throw_exception(const char* exc, const char *message);

/**
 * Looks up implementation for native method
 */
GenericFunctionPointer interp_find_native(Method_Handle method);

/**
 * Resolve class in constant pool of 'clazz' with index = 'classId'. Throw
 * exception if resolution error.
 */
Class* interp_resolve_class(Class *clazz, int classId);

/**
 * Resolve class suitable for new operation in constant pool of 'clazz' with
 * index = 'classId'. Throw exception if resolution error.
 */
Class* interp_resolve_class_new(Class *clazz, int classId);

/**
 * Resolve static field in constant pool of 'clazz' with index = 'fieldId'.
 * Throw exception if resolution error.
 */
Field* interp_resolve_static_field(Class *clazz, int fieldId, bool putfield);

/**
 * Resolve nonstatic field in constant pool of 'clazz' with index = 'fieldId'.
 * Throw exception if resolution error.
 */
Field* interp_resolve_nonstatic_field(Class *clazz, int fieldId, bool putfield);

/**
 * Resolve virtual method in constant pool of 'clazz' with index = 'methodId'.
 * Throw exception if resolution error.
 */
Method* interp_resolve_virtual_method(Class *clazz, int methodId);

/**
 * Resolve interface method in constant pool of 'clazz' with index = 'methodId'.
 * Throw exception if resolution error.
 */
Method* interp_resolve_interface_method(Class *clazz, int methodId);

/**
 * Resolve virtual method in constant pool of 'clazz' with index = 'methodId'.
 * Throw exception if resolution error.
 */
Method* interp_resolve_static_method(Class *clazz, int methodId);

/**
 * Resolve virtual method in constant pool of 'clazz' with index = 'methodId'.
 * Throw exception if resolution error.
 */
Method* interp_resolve_special_method(Class *clazz, int methodId);

/**
 * Resolve array of class for specified objClass.
 */
Class* interp_class_get_array_of_class(Class *objClass);
