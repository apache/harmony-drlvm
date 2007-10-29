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
 * @version $Revision: 1.10 $
 */
#ifndef _VM_CLASS_MANIPULATION_H
#define _VM_CLASS_MANIPULATION_H

/**
 * @file
 * Part of Class Support interface related to retrieving and changing
 * different class properties.
 *
 * The list of properties includes, but is not limited to, class name
 * class super class, fields, methods and so on.
 */

/** 
 * Returns the class name.
 *
 * @param klass - the class handle
 *
 * @return Class name bytes.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 */
const char*
class_get_name(Open_Class_Handle klass);

/** 
 * Returns the class name.
 *
 * @param klass - the class handle
 *
 * @return Class name bytes.
 *
 * @ingroup Extended
 */
const char*
class_get_java_name(Open_Class_Handle klass);


/** 
 * Returns the super class of the current class.
 *
 * @param klass - the class handle
 *
 * @return The class handle of the super class.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 */
Open_Class_Handle
class_get_super_class(Open_Class_Handle klass);

/**
 * Returns the class loader of the current class.
 *
 * @param klass - the class handle
 *
 * @return The class loader.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 */
Open_Class_Loader_Handle
class_get_class_loader(Open_Class_Handle klass);

/**
 * Checks whether the current class is a primitive type.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> for a primitive class; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 */
Boolean
class_is_primitive(Open_Class_Handle klass);

/**
 * Checks whether the current class is an array.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> for an array; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 */
Boolean
class_is_array(Open_Class_Handle klass);

/**
 * Checks whether the current class is an instance of another class.
 *
 * @param klass - the class handle
 * @param super_klass - super class handle
 *
 * @return <code>TRUE</code> if <i>klass</i> is an instance of a <i>super_class</i>; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass</i> or <i>super_klass</i> equals to <code>NULL</code>.
 */
Boolean
class_is_instanceof(Open_Class_Handle klass, Open_Class_Handle super_klass);

/**
 * Checks whether the current class is abstract.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> for an abstract class; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 * @note Replaces the class_property_is_abstract function.
 */
Boolean
class_is_abstract(Open_Class_Handle klass);

/**
 * Checks whether the current class is an interface class.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> for an interface class; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 * @note Replaces functions class_is_interface_ and class_property_is_interface2.
 */
Boolean
class_is_interface(Open_Class_Handle klass);

/**
 * Checks whether the current class is final.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> for a final class; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 * @note Replaces functions class_is_final_ and class_property_is_final.
 */
Boolean
class_is_final(Open_Class_Handle klass);

/**
 * Checks whether the given classes are the same.
 *
 * @param klass1  - the first class handle
 * @param klass2 - the second class handle
 *
 * @return <code>TRUE</code> for the same classes; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>klass1</i> or <i>klass2</i> equal to <code>NULL</code>.
 */
Boolean
class_is_same_class(Open_Class_Handle klass1, Open_Class_Handle klass2);

/**
 * Returns the offset of the referent field 
 * in the <code>java.lang.ref.Reference</code> object.
 *
 * It is assumed that the class represents the reference object,
 * that is, running the class_is_reference function returns a non-zero value.
 *
 * @note The returned value is most probably a constant
 *             and does not depend on the class.
 *
 * @note This interface allows only one weak, soft or phantom reference per object.
 *             It seems to be sufficient for the JVM spec.
 */
int
class_get_referent_offset(Open_Class_Handle clss);

/**
 * Returns the VM_Data_Type value for the given class.
 *
 * @param klass - the class handle
 *
 * @return The VM_Data_Type value.
 */
VM_Data_Type
class_get_primitive_type_of_class(Open_Class_Handle klass);

/**
 * Returns the class corresponding to the primitive type.
 *
 * @param type - the primitive type
 *
 * @return The class corresponding to a primitive type.
 *
 * @note For all primitive types <i>type</i> is:
 *            <code>type == class_get_primitive_type_of_class(class_get_class_of_primitive_type(type))</code>
 */
Open_Class_Handle
class_get_class_of_primitive_type(VM_Data_Type type);

/**
 * Returns the size of an instance in the heap, in bytes.
 * 
 * @param klass - the class handle
 *
 * @return The size of an instance in the heap.
 *
 * @note Replaces class_get_boxed_data_size function.
 */
unsigned
class_get_instance_size(Open_Class_Handle klass);

/**
 * For given a class handle <i>klass</i> constructs a class of
 * the type representing on-dimentional array of <i>klass</i>.
 * For example, given the class of Ljava/lang/String; this function
 * will return array class [Ljava/lang/String;.
 *
 * @param klass - the class handle
 *
 * @return The class handle of the type representing the array of <i>klass</i>.
 */
Open_Class_Handle
class_get_array_of_class(Open_Class_Handle klass);

/**
 * Returns the class of the array element of the given class.
 *
 * @param klass - the class handle
 *
 * @return The class of the array element of the given class.
 *
 * @note The behavior is undefined if the parameter does not represent
 * an array class.
 */
Open_Class_Handle
class_get_array_element_class(Open_Class_Handle klass);

/**
 * Returns the type info for the elements of the array for array classes.
 *
 * @param klass - the class handle
 *
 * @return Type information for the elements of the array.
 */
Open_Type_Info_Handle
class_get_element_type_info(Open_Class_Handle klass);

/**
 * Gets the handle for the field. 
 *
 * @param klass - the class handle
 * @param index - this value is the sequence number of field in the set of fields
 *                both inherited and defined in this class.
 *
 * @return The handle for the field. If <i>index</i> is greater than or equal to
 * <code>class_num_instance_fields_recursive</code>, function returns NULL.
 */
Open_Field_Handle
class_get_instance_field_recursive(Open_Class_Handle klass, unsigned index);

/**
 * Returns the size of the element of the array for class handles that represent an array.
 * 
 * This function is a combination of functions class_get_instance_size and
 * class_get_array_element_class. 
 *
 * @param klass - the class handle
 *
 * @return The size of the element of the array.
 *
 * @ingroup Extended
 */
unsigned
class_element_size(Open_Class_Handle klass);

/**
 * Returns the vtable handle for the given class.
 *
 * @param klass - the class handle
 *
 * @return The vtable handle for the given class.
 */
Open_VTable_Handle
class_get_vtable(Open_Class_Handle klass);

/**
 * Verifies that the class is fully initialized.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> if the class is already fully
 *                initialized; otherwise, <code>FALSE</code>. 
 *
 * @note The function class_needs_initialization is merged with this function. 
 */
Boolean
class_is_initialized(Open_Class_Handle klass);

/**
 * Gets the alignment of the class.
 *
 * @param klass - the class handle
 *
 * @return The alignment of the class.
 */
unsigned
class_get_alignment(Open_Class_Handle klass);

/**
 * Checks whether the class has a non-trivial finalizer.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> if the class has a non-trivial finalizer. 
 *                : otherwise, <code>FALSE</code>.
 */
Boolean
class_is_finalizable(Open_Class_Handle klass);

/**
 * Checks whether the class is an array of primitives.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> if this is an array of primitives.
 *               : otherwise, <code>FALSE</code>.
 */
Boolean
class_is_non_ref_array(Open_Class_Handle klass);

/**
 * Checks whether all instances of the given class are pinned.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> if all instances of this class are pinned; otherwise, <code>FALSE</code>.
 */
Boolean
class_is_pinned(Open_Class_Handle klass);

/**
 * Checks whether the class represented by Class_Handle
 * is a descendant of <code>java.lang.ref.Reference</code>. 
 * 
 * The type of reference (weak, soft or phantom) is encoded in the return 
 * value of WeakReferenceType.
 *
 * @param klass - the class handle
 *
 * @return One of WEAK_REFERENCE, SOFT_REFERENCE, or PHANTOM_REFERENCE if
 * the given class is corresponding descendant of <code>java.lang.ref.Reference</code>.
 * NOT_REFERENCE (0) is returned otherwise.
 */
WeakReferenceType
class_get_reference_type(Open_Class_Handle klass);

/**
 * Checks whether the class is likely to be used as an exception object. 
 *
 * This is a hint only. Even if this function returns <code>FALSE</code>,
 * the class might still be used for exceptions.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> if the class is likely to be used
 *                as an exception object; otherwise, <code>FALSE</code>. 
 */
Boolean
class_hint_is_exceptiontype(Open_Class_Handle klass);

/**
 * Checks whether the class represents an enumerator.
 *
 * @param klass - the class handle
 *
 * @return <code>TRUE</code> if the class represents an enum.
 *               : otherwise, <code>FALSE</code>.
 */
Boolean
class_is_enum(Open_Class_Handle klass);

/**
 * Returns the number of instance fields defined in the given class.
 * This number includes inherited fields.
 *
 * @param klass - the class handle
 *
 * @return The number of instance fields defined in a class.
 *
 * @note Replaces the class_num_instance_fields_recursive function.
 */
unsigned
class_get_all_instance_fields_number(Open_Class_Handle klass);

/**
 * Returns the name of the package containing the class.
 *
 * @param klass - the class handle
 *
 * @return The name of the package containing the class.
 *
 * @note Not used
 */
const char*
class_get_package_name(Open_Class_Handle klass);

/**
 * Returns the pointer to the location of the constant.
 *
 * @param klass  - the class handle
 * @param index - interpreted as a constant pool index
 *
 * @return The pointer to the location of the constant in the
 * constant pool of the class.
 *
 * @note This function must not be called for constant strings.  
 *             Instead, one of the following can be done:
 *            <ol>
 *               <li>JIT-compiled code gets the string object at run time by calling
 *                   VM_RT_LDC_STRING
 *               <li>The class_get_const_string_intern_addr() function is used.
 *            </ol>
 */
const void*
class_get_const_addr(Open_Class_Handle klass, unsigned short index);

/**
 * Returns the type of the compile-time constant.
 *
 * @param klass  - the class handle
 * @param index - interpreted as a constant pool index
 *
 * @return The type of a compile-time constant.
 */
VM_Data_Type
class_get_const_type(Open_Class_Handle klass, unsigned short index);

/**
 * Returns the address of the interned version of the string. 
 * 
 * Calling this function has
 * a side-effect of interning the string, so that the JIT compiler can
 * load a reference to the interned string without checking whether 
 * it is <code>NULL</code>.
 *
 * @param klass  - the class handle
 * @param index - interpreted as a constant pool index
 *
 * @return The address of the interned version of the string.
 *
 * @note Not used
 */
void*
class_get_const_string_intern_addr(Open_Class_Handle klass, unsigned short index);

/**
 * Returns the descriptor for the field or method/interface in the constant pool entry.
 *
 * @param klass         - the class handle
 * @param cp_index - interpreted as a constant pool index
 *
 * @return The descriptor for field or method/interface in constant pool entry.
 */
const char*
class_get_cp_entry_descriptor(Open_Class_Handle klass, unsigned short cp_index);

/**
 * Returns the data type for the field in the constant pool entry.
 *
 * @param klass         - the class handle
 * @param cp_index - interpreted as a constant pool index
 *
 * @return The data type for the field in the constant pool entry.
 */
VM_Data_Type
class_get_cp_field_type(Open_Class_Handle klass, unsigned short cp_index);

/**
 * Initializes the <i>iterator</i> to iterate over all classes that
 * descend from <i>klass</i>, including <i>klass</i> itself.
 *
 * @param klass      - the class handle
 * @param iterator - the class iterator
 *
 * @return <code>TRUE</code> if iteration is supported over <i>klass</i> and
 *               <code>FALSE</code> if it is not.
 *
 * @note Reference to the internal type ChaClassIterator.
 */
Boolean
class_iterator_initialize(ChaClassIterator* iterator, Open_Class_Handle klass);

/**
 * Returns the current class of the iterator.
 *
 * @param iterator - the class iterator
 *
 * @return The current class of the iterator. If there are no more classes, 
 *                returns <code>NULL</code>. 
 *
 * @note Reference to the internal type ChaClassIterator.
 */
Open_Class_Handle
class_iterator_get_current(ChaClassIterator* iterator);

/**
 * Advances the iterator.
 *
 * @param iterator  - the class iterator
 *
 * @note Reference to the internal type ChaClassIterator.
 */
void class_iterator_advance(ChaClassIterator* iterator);

/**
 * Returns the descriptor for the given method.
 *
 * @param klass  - the class handle
 * @param index - interpreted as a constant pool index
 *
 * @return The descriptor for the method.
 *
 * @note Replaces the const_pool_get_method_descriptor function.
 */
const char*
class_get_cp_method_descriptor(Open_Class_Handle klass, unsigned short index);

/**
 * Returns the descriptor for the field.
 *
 * @param klass - the class handle
 * @param index - interpreted as a constant pool index
 *
 * @return  The descriptor for the field.
 *
 * @note Replaces the const_pool_get_field_descriptor function.
 */
const char*
class_get_cp_field_descriptor(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the class constant pool size.
 *
 * @param klass - the class handle
 *
 * @return Constant pool size.
 *
 * @note An assertion is raised if <i>klass</i> equals to <code>NULL</code>.
 */
unsigned short
class_get_cp_size(Open_Class_Handle klass);

/** 
 * Returns the constant pool entry tag.
 *
 * @param klass  - the class handle
 * @param index - the constant pool entry index
 *
 * @return The constant pool entry tag.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned char
class_get_cp_tag(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the class name entry index in the constant pool.
 * This function is only legal for constant pool entries with CONSTANT_Class tags.
 *
 * @param klass  - the class handle
 * @param index - the constant pool entry index
 *
 * @return The class name entry index.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned short
class_get_cp_class_name_index(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the class name entry index in the constant pool.
 * 
 * The function is legal for constant pool entries with  CONSTANT_Fieldref, 
 * CONSTANT_Methodref and CONSTANT_InterfaceMethodref tags.
 *
 * @param klass  - the class handle
 * @param index - the constant pool entry index
 *
 * @return The class name entry index.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned short
class_get_cp_ref_class_index(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the name and type entry index in the constant pool.
 *
 * This function is legal for constant pool entries with CONSTANT_Fieldref, 
 * CONSTANT_Methodref and CONSTANT_InterfaceMethodref tags. 
 *
 * @param klass  - the class handle
 * @param index - the constant pool entry index
 *
 * @return The name_and_type entry index.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned short
class_get_cp_ref_name_and_type_index(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the string entry index in the constant pool.
 *
 * This function is legal for constant pool entries with CONSTANT_String tags. 
 *
 * @param klass  - the class handle
 * @param index - the constant pool entry index
 *
 * @return The string entry index.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned short
class_get_cp_string_index(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the name entry index in the constant pool.
 *
 * @note Function is legal for constant pool entry with CONSTANT_NameAndType tags.
 *
 * @param klass - the class handle
 * @param index - the constant pool entry index
 *
 * @return  The name entry index.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *       <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned short
class_get_cp_name_index(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns the descriptor entry index in the constant pool.
 * This function is legal for constant pool entries with CONSTANT_NameAndType tags.
 *
 * @param klass   - the class handle
 * @param index - the constant pool entry index
 *
 * @return The descriptor entry index.
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
unsigned short
class_get_cp_descriptor_index(Open_Class_Handle klass, unsigned short index);

/** 
 * Returns bytes for the UTF8 constant pool entry.
 * This function is legal for constant pool entries with CONSTANT_UTF8 tags.
 *
 * @param klass  - the class handle
 * @param index - the constant pool entry index
 *
 * @return Bytes for the UTF8 constant pool entry. 
 *
 * @note An assertion is raised if <i>klass</i> equals to 
 *            <code>NULL</code> or if <i>index</i> is out of range.
 */
const char*
class_get_cp_utf8_bytes(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class for the constant pool entry.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - the pointer to exception
 *
 * @return The class resolved for the constant pool entry.
 *
 * @note Replaces the vm_resolve_class and resolve_class functions.
 */
Open_Class_Handle
class_resolve_class(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class for the constant pool entry 
 * and checks whether an instance can be created.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - the pointer to the exception
 *
 * @return The class resolved for the constant pool entry. If no instances
 *                 of the class can be created, returns <code>NULL</code> and raises and exception.
 *
 * @note Replaces the vm_resolve_class_new and resolve_class_new functions.
 *
 */
Open_Class_Handle
class_resolve_class_new(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class interface method for the constant pool entry.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - the pointer to exception
 *
 * @return  interface method resolved for the constant pool entry.
 *
 * @note Replace the resolve_interface_method function.
 */
Open_Method_Handle
class_resolve_interface_method(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves class static method for the constant pool entry.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - the pointer to exception
 *
 * @return The static method resolved for the constant pool entry.
 *
 * @note Replaces the resolve_static_method function.
 */
Open_Method_Handle
class_resolve_static_method(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class virtual method for the constant pool entry.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - the pointer to exception
 *
 * @return The virtual method resolved for the constant pool entry.
 *
 * @note Replaces the resolve_virtual_method function.
 */
Open_Method_Handle
class_resolve_virtual_method(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class special method for the constant pool entry.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - pointer to exception
 *
 * @return The special method resolved for the constant pool entry.
 *
 * @note Replaces the resolve_special_method function.
 */
Open_Method_Handle
class_resolve_special_method(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class static field for the constant pool entry.
 *
 * @param klass - the class handle
 * @param index - the constant pool entry index
 * @param exc   - pointer to exception
 *
 * @return The static field resolved for the constant pool entry.
 *
 * @note Replaces the resolve_static_field function.
 */
Open_Field_Handle
class_resolve_static_field(Open_Class_Handle klass, unsigned short index);

/**
 * Resolves the class non-static field for the constant pool entry.
 *
 * @param klass   - the class handle
 * @param index  - the constant pool entry index
 * @param exc     - pointer to exception
 *
 * @return The non-static field resolved for the constant pool entry.
 *
 * @note Replaces the resolve_nonstatic_field function.
 */
Open_Field_Handle
class_resolve_nonstatic_field(Open_Class_Handle klass, unsigned short index);

/**
 * Provides the initialization phase for the given class.
 *
 * @param klass  - class handle
 *
 * @note For interpreter use only.
 */
void
class_initialize(Class_Handle klass);

#endif // _VM_CLASS_MANIPULATION_H
