/*
 *  Copyright 2006 The Apache Software Foundation or its licensors, as applicabl
e.
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
 * @author Intel, Pavel Pervov
 * @version $Revision: 1.4 $
 */
#ifndef _VM_TYPE_ACCESS_H
#define _VM_TYPE_ACCESS_H

/**
 * @file
 * Part of Class Support interface related to accessing generalized
 * types' properties.
 */

/**
 * Checks whether type information contains the reference type. 
 *
 * @param typeinfo - the type information handle
 *
 * @return <code>TRUE</code> if type information contains reference description; otherwise, <code>FALSE</code>.
 */
Boolean
type_info_is_reference(Type_Info_Handle typeinfo);

/**
 * Checks whether type information contains the void type. 
 *
 * @param typeinfo - the type information handle
 *
 * @return <code>TRUE</code> if type information contains void type; otherwise, <code>FALSE</code>.
 */
Boolean
type_info_is_void(Type_Info_Handle tih);

/**
 * Checks whether  type information describes the one-dimensional array. 
 *
 * @param typeinfo - the type information handle
 *
 * @return <code>TRUE</code> if type information describes the one-dimensional array; otherwise, <code>FALSE</code>.
 */
Boolean
type_info_is_vector(Type_Info_Handle tih);

/**
 * Returns the C++ class structure for the given type information. 
 *
 * @param typeinfo - the type information handle
 * @param exc         - the variable to return the exception "code"
 *
 * @return The handle of the C++ class structure for the specified type information.
 *
 * @note Reference to the internal type <i>Loader_Exception</i>.
 */
Class_Handle
type_info_get_class(Type_Info_Handle typeinfo, Loader_Exception* exc);

/**
 * Returns recursive type information for the given type information. 
 *
 * @param typeinfo - type information to query
 *
 * @return Recursive type information if available; otherwise, <code>NULL</code>.
 */
Type_Info_Handle
type_info_get_type_info(Type_Info_Handle typeinfo);

#endif // _VM_TYPE_ACCESS_H
