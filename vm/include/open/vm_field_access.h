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
 * @author Intel, Pavel Pervov
 * @version $Revision: 1.4 $
 */
#ifndef _VM_FIELD_ACCESS_H
#define _VM_FIELD_ACCESS_H

/**
 * @file
 * Part of Class Support interface related to retrieving different
 * properties of fields contained in class
 */

/**
 * Returns the name of the field.
 *
 * @param field - the field handle
 *
 * @return The name of the field.
 */
const char*
field_get_name(Open_Field_Handle field);

/**
 * Returns the field <i>descriptor</i>.
 * 
 * The descriptor is a string representation of the field types as
 * defined by the JVM specification.
 *
 * @param field - the field handle
 *
 * @return The field descriptor.
 */
const char*
field_get_descriptor(Open_Field_Handle field);

/**
 * Returns the class that defined the given field.
 *
 * @param field - the field handle
 *
 * @return The class that defined the field.
 */
Open_Class_Handle
field_get_class(Open_Field_Handle field);

/**
 * Returns the address of the given static field.
 *
 * @param field - the field handle
 *
 * @return The address of the static field.
 */
void*
field_get_address(Open_Field_Handle field);

/**
 * Returns the offset to the given instance field.
 *
 * @param field - the field handle
 *
 * @return The offset to the instance field.
 */
unsigned
field_get_offset(Open_Field_Handle field);

/**
 * Returns the type info that represents the type of the field.
 *
 * @param field - the field handle
 *
 * @return Type information.
 *
 * @note Replaces field_get_type_info_of_field_value. 
 */
Open_Type_Info_Handle
field_get_type_info_of_field_type(Open_Field_Handle field);

/**
 * Returns the class that represents the type of the field.
 *
 * @param field - the field handle
 *
 * @return the class that represents the type of the field.
 *
 * @note Replaces field_get_class_of_field_value.
 */
Open_Class_Handle
field_get_class_of_field_type(Open_Field_Handle field);

/**
 *  Checks whether the field is final.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is final.
 *
 * #note Extended
 */
Boolean
field_is_final(Open_Field_Handle field);

/**
 *  Checks whether the field is static.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is static; otherwise, <code>FALSE</code>. 
 *
 * @ingroup Extended 
 */
Boolean
field_is_static(Open_Field_Handle field);

/**
 *  Checks whether the field is private.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is private; otherwise, <code>FALSE</code>. 
 *
 * @ingroup Extended 
 */
Boolean
field_is_private(Open_Field_Handle field);

/**
 *  Checks whether the field is public.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is public; otherwise, <code>FALSE</code>. 
 *
 * @ingroup Extended 
 */
Boolean
field_is_public(Open_Field_Handle field);

/**
 *  Checks whether the field is volatile.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is volatile; otherwise, <code>FALSE</code>. 
 *
 * @ingroup Extended 
 */
Boolean
field_is_volatile(Open_Field_Handle field);

/**
 *  Checks whether the field is literal.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is literal.
 */
Boolean
field_is_literal(Open_Field_Handle field);

/**
 *  Checks whether the field is injected.
 *
 * @param field - the field handle
 *
 * @return <code>TRUE</code> if the field is injected; otherwise, <code>FALSE</code>. 
 */
Boolean
field_is_injected(Open_Field_Handle field);

#endif // _VM_FIELD_ACCESS_H
