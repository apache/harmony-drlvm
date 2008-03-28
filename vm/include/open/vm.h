/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements. See the NOTICE file distributed with
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
 * These are the functions that a VM built as a DLL must export.
 * Some functions may be optional and are marked as such.
 */

#ifndef _VM_EXPORT_H
#define _VM_EXPORT_H

#ifdef WIN32
#include <stddef.h>
#else
#include <unistd.h>
#endif



#include "open/types.h"

#define O_A_H_VM_VMDIR         "org.apache.harmony.vm.vmdir"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This structure is meant to be opaque. External modules should not
 * attempt to directly access any of its fields.
 */ 
typedef struct ChaClassIterator {
    Class_Handle _root_class;
    Class_Handle _current;
    Boolean _is_valid;
} ChaClassIterator;


/**
 * This structure is meant to be opaque. External modules should not
 * attempt to directly access any of its fields.
 */
typedef struct ChaMethodIterator {
    Method_Handle _method;
    Method_Handle _current;
    ChaClassIterator _class_iter;
} ChaMethodIterator;


/**
 * Dynamic interface adaptor, should return specific API by its name.
 */
VMEXPORT void* get_vm_interface(const char*);

/**
 * Begin class-related functions.
 */

/** 
 * @return A handle for the <code>Object</code> class. For Java applications, it's
 *         <code>java.lang.Object</code>.
 */ 
VMEXPORT Class_Handle get_system_object_class();

/** 
 * @return  A handle for the <code>Class</code> class. For Java applications, it's
 *          <code>java.lang.Class</code>.
 */
VMEXPORT Class_Handle get_system_class_class();

/** 
 * @return  A handle for the string class. For Java applications, it's
 *          java.lang.String.
 */
 
VMEXPORT Class_Handle get_system_string_class();

/** 
 * Find already loaded class of a given name. 
 *
 * @return <code>NULL</code> if a class is not loaded.
 */
VMEXPORT Class_Handle
class_lookup_class_by_name_using_bootstrap_class_loader(const char *name);

/** 
 * The following three functions will be eventually renamed to
 * \arg <code>class_is_final</code>
 * \arg <code>class_is_abstract</code>
 * \arg <code>class_is_interface</code>
 * but right now that would conflict 
 * with the names of some internal macros.
 */

VMEXPORT Boolean class_is_final(Class_Handle ch);
VMEXPORT Boolean class_is_abstract(Class_Handle ch);
VMEXPORT BOOLEAN class_is_interface(Class_Handle ch);

/**
 * @return <code>TRUE</code> if the class is likely to be used as an exception object.
 *         This is a hint only. If the result is <code>FALSE</code>, the class may still
 *         be used for exceptions but it is less likely.
 */
VMEXPORT Boolean class_hint_is_exceptiontype(Class_Handle ch);

/**
 * @return <code>TRUE</code> if the class is a value type.
 */
 VMEXPORT Boolean class_is_valuetype(Class_Handle ch);

/**
 * @return <code>TRUE</code> if the class represents an enum. 
 *         For Java 1.4 always returns <code>FALSE</code>.
 */
VMEXPORT Boolean class_is_enum(Class_Handle ch);

/**
 * This function can only be called if (<code>class_is_enum(ch)</code> == <code>TRUE</code>)
 * The returned value is the type of the underlying int type.
 */
VMEXPORT VM_Data_Type class_get_enum_int_type(Class_Handle ch);

/**
 * @return <code>TRUE</code> if the class represents a primitive type (int, float, etc.)
 */
 VMEXPORT Boolean class_is_primitive(Class_Handle ch);

/**
 * @return The name of the class.
 */
 VMEXPORT const char *class_get_name(Class_Handle ch);

/**
 * @return The name of the package containing the class.
 */
 VMEXPORT const char *class_get_package_name(Class_Handle ch);

/**
 * The super class of the current class. 
 * @return <code>NULL</code> for the system object class, i.e.
 *         <code>class_get_super_class</code>(get_system_object_class()) == NULL
 */
VMEXPORT Class_Handle class_get_super_class(Class_Handle ch);

/**
 * @return The vtable handle of the given class.
 */
 VMEXPORT VTable_Handle class_get_vtable(Class_Handle ch);

/**
 * @return The allocation handle to be used for the object allocation
 *         routines, given a class handle.
 */
VMEXPORT Allocation_Handle class_get_allocation_handle(Class_Handle ch);

/**
 * @return The class handle corresponding to a given allocation handle.
 */
VMEXPORT Class_Handle allocation_handle_get_class(Allocation_Handle ah);

/**
 * @return An <code>VM_Data_Type</code> value for a given class.
 */
 VMEXPORT VM_Data_Type class_get_primitive_type_of_class(Class_Handle ch);

/**
 * @return A class corresponding to a primitive type. For all primitive types t
 *         t == <code>class_get_primitive_type_of_class(class_get_class_of_primitive_type(t))</code>
 */
 VMEXPORT Class_Handle class_get_class_of_primitive_type(VM_Data_Type typ);

/** 
 * @return <code>TRUE</code> is the class is an array.
 */
VMEXPORT Boolean class_is_array(Class_Handle ch);

/** 
 * @return <code>TRUE</code> if class <code>s</code> is assignment 
 * compatible with class <code>t</code>.
 */ 
VMEXPORT Boolean class_is_instanceof(Class_Handle s, Class_Handle t);

/**
 * Given a class handle <code>cl</code> construct a class handle of the type
 * representing array of <code>cl</code>. If class cl is value type, assume
 * that the element is a reference to a boxed instance of that type.
 */
 VMEXPORT Class_Handle class_get_array_of_class(Class_Handle ch);

/**
 * Given a class handle <code>cl</code> construct a class handle of the type
 * representing array of <code>cl</code>. Class <code>cl</code> is assumed to be a
 * value type. 
 * 
 * @return <code>NULL</code> if cl is not a value type.
 */
 VMEXPORT Class_Handle class_get_array_of_unboxed(Class_Handle ch);

/**
 * @return For a class that is an array return the type info for the elements
 *         of the array.
 */
 VMEXPORT Type_Info_Handle class_get_element_type_info(Class_Handle ch);

 /**
  * @return <code>TRUE</code> if the class is already fully initialized.
  */
  VMEXPORT Boolean class_is_initialized(Class_Handle ch);

/**
 * @return <code>TRUE</code> if the class is neither initialized nor in the process
 *         of being initialized. The intention is that the JIT will emit a call
 *         to <code>VM_RT_INITIALIZE_CLASS</code> before every access to a static 
 *         field in Java.
 */
 VMEXPORT Boolean class_needs_initialization(Class_Handle ch);

/**
 * For Java returns <code>FALSE</code>.
 */
 VMEXPORT Boolean class_is_before_field_init(Class_Handle ch);

/**
 * Number of instance fields defined in a class. That doesn't include
 * inherited fields.
 */
 VMEXPORT unsigned class_num_instance_fields(Class_Handle ch);

/**
 * Get the handle for a field. If <code>idx</code> is greater than or equal to
 * <code>class_num_instance_fields</code>. 
 *
 * @return <code>NULL</code>
 *
 * The value of idx indexes into the fields defined in this class and
 * doesn't include inherited fields.
 */
 VMEXPORT Field_Handle class_get_instance_field(Class_Handle ch, unsigned idx);

/**
 * Number of instance fields defined in a class. This number includes
 * inherited fields.
 */

VMEXPORT unsigned class_num_instance_fields_recursive(Class_Handle ch);
/**
 * Get the handle for a field.  
 *
 * @return  <code>NULL</code> if idx is greater than or equal to
 *          <code>class_num_instance_fields_recursive</code>.
 *
 * The value of idx indexes into the set of fields that includes both fields
 * defined in this class and inherited fields.
 */
VMEXPORT Field_Handle class_get_instance_field_recursive(Class_Handle ch, unsigned idx);

/**
 * Number of methods declared in the class.
 */
 VMEXPORT unsigned class_get_number_methods(Class_Handle ch);

/// Check if fast_instanceof is applicable for the class
VMEXPORT Boolean class_get_fast_instanceof_flag(Class_Handle cl);

/**
 * Get handle for a method declared in class.
 */
 VMEXPORT Method_Handle class_get_method(Class_Handle ch, unsigned index);

/**
 * @return <code>TRUE</code> if all instances of this class are pinned.
 */
 VMEXPORT Boolean class_is_pinned(Class_Handle ch);

/**
 * @return <code>TRUE</code> if all instances of this class are pinned.
 */
 VMEXPORT void* class_alloc_via_classloader(Class_Handle ch, int32 size);

/**
 * @return <code>TRUE</code> if this is an array of primitives.
 */
 VMEXPORT Boolean class_is_non_ref_array(Class_Handle ch);

/**
 * @return <code>TRUE</code> if the class has a non-trivial finalizer.
 */
 VMEXPORT BOOLEAN class_is_finalizable(Class_Handle ch);

/**
 * This exactly what I want.
 * Get the alignment of the class.
 */
 VMEXPORT unsigned class_get_alignment(Class_Handle ch);

/**
 * Get the alignment of the class when it's unboxed.
 */
 VMEXPORT unsigned class_get_alignment_unboxed(Class_Handle ch);

/**
 * @return For a class handle that represents and array, return the size of the
 *         element of the array.
 */
 VMEXPORT unsigned class_element_size(Class_Handle ch);

/**
 * @return The size in bytes of an instance in the heap.
 */
 VMEXPORT unsigned class_get_boxed_data_size(Class_Handle ch);

/**
 * @return The offset to the start of user data form the start of a boxed
 *         instance.
 */
 VMEXPORT unsigned class_get_unboxed_data_offset(Class_Handle ch);


 /**
 * @return The size of array element for the given array class
 */
 VMEXPORT unsigned class_get_array_element_size(Class_Handle ch);


/**
 * @return The class of the array element of the given class.
 *
 * The behavior is undefined if the parameter does not represent
 * an array class.
 */
 VMEXPORT Class_Handle class_get_array_element_class(Class_Handle ch);

/**
 * @return The offset from the start of the vtable at which the
 *         superclass hierarchy is stored. This is for use with fast type
 *         checking.
 */ 
 VMEXPORT int vtable_get_super_array_offset();

/**
 * @return Class handle given object's <code>VTable_Handle</code>.
 */ 
 DECLARE_OPEN(Class_Handle, vtable_get_class, (VTable_Handle vh));

/**
 * @return The number of superclass hierarchy elements that are
 *         stored within the vtable. This is for use with fast type checking.
 */
 VMEXPORT int vm_max_fast_instanceof_depth();


////
// begin class iterator related functions.
////

/**
 * Initializes the <code>CHA_Class_Iterator</code>, to iterate over all 
 * classes that descend from <code>root_class</code>, including 
 * <code>root_class</code> itself.
 *
 * @return <code>TRUE</code> if iteration is supported over 
 *         <code>root_class</code>, <code>FALSE</code> if not.
 */ 
 VMEXPORT Boolean class_iterator_initialize(ChaClassIterator*, Class_Handle root_class);

/**
 * @return The current class of the iterator. <code>NULL</code> if
 *         there are no more classes.
 */
 VMEXPORT Class_Handle class_iterator_get_current(ChaClassIterator*);

/**
 * Advances the iterator.
 */
 VMEXPORT void class_iterator_advance(ChaClassIterator*);

////
// begin inner-class related functions.
///

/**
 * @return <code>TRUE</code> the number of inner classes.
 */ 
 VMEXPORT unsigned class_number_inner_classes(Class_Handle ch);

/**
 * @return <code>TRUE</code> if an inner class is public.
 */ 
 VMEXPORT Boolean class_is_inner_class_public(Class_Handle ch, unsigned idx);

/**
 * @return an inner class
 */
 VMEXPORT Class_Handle class_get_inner_class(Class_Handle ch, unsigned idx);

/**
 * @return the class that declared this one, or <code>NULL</code> if top-level class
 */
 VMEXPORT Class_Handle class_get_declaring_class(Class_Handle ch);


///
// end class-related functions.
///

////
// end class-related functions.
////

////
// begin field-related functions.
////

/**
 * @return <code>TRUE</code> if the field must be enumerated by GC
 *
 * This function doesn't cause resolution of the class of the field.
 *
 * FIXME: move to internal headers
 */
 VMEXPORT Boolean field_is_enumerable_reference(Field_Handle fh);

////
// end field-related functions.
////

////
// begin vector layout functions.
////

/**
 * Vectors are one-dimensional, zero-based arrays. All Java 
 * <code>arrays</code> are vectors.
 * Functions provided in this section do not work on multidimensional or
 * non-zero based arrays (i.e. arrays with a lower bound that is non-zero
 * for at least one of the dimensions.
 */

/**
 * Return the offset to the length field of the array. That field has
 * the same offset for vectors of all types.
 */
VMEXPORT int vector_length_offset();

/**
 * Deprecated. Please use <code>vector_length_offset</code> instead.
 */
VMEXPORT int array_length_offset();

/**
 * Return the offset to the first element of the vector of the given type.
 * This function is provided for the cases when the class handle of the
 * element is not available.
 */
VMEXPORT int vector_first_element_offset(VM_Data_Type element_type);

/**
 * Deprecated. Please use <code>vector_first_element_offset</code> instead.
 */ 
VMEXPORT int array_first_element_offset(VM_Data_Type element_type);


/**
 * Return the offset to the first element of the vector of the given type.
 * Assume that the elements are boxed. Byte offset.
 */ 
VMEXPORT int vector_first_element_offset_class_handle(Class_Handle element_type);

/**
 * Deprecated. Please use <code>vector_first_element_offset_class_handle</code> instead.
 */
VMEXPORT int array_first_element_offset_class_handle(Class_Handle element_type);

/**
 * Return the offset to the first element of the vector of the given type.
 * If the class is a value type, assume that elements are unboxed.
 * If the class is not a value type, assume that elements are references.
 */ 
VMEXPORT int vector_first_element_offset_unboxed(Class_Handle element_type);

/**
 * Deprecated. Please use <code>vector_first_element_offset_unboxed</code> instead.
 */
VMEXPORT int array_first_element_offset_unboxed(Class_Handle element_type);

/**
 * Return the length of a vector. The caller must ensure that GC will not
 * move or deallocate the vector while vector_get_length() is active.
 */
VMEXPORT int32 vector_get_length(Vector_Handle vector);

/**
 * Return the address to an element of a vector of references.
 * The caller must ensure that GC will not move or deallocate the vector
 * while vector_get_element_address_ref() is active.
 */
VMEXPORT Managed_Object_Handle *
vector_get_element_address_ref(Vector_Handle vector, int32 idx);

/**
 * Return the size of a vector of a given number of elements.
 * The size is rounded up to take alignment into account.
 */
VMEXPORT unsigned vm_vector_size(Class_Handle vector_class, int length);

////
// end vector layout functions.
////

////
// begin miscellaneous functions.
////

/**
 * @return <code>TRUE</code> if references within objects and vector elements are
 *          to be treated as offsets rather than raw pointers.
 */
VMEXPORT Boolean vm_references_are_compressed();

/**
 * @return The starting address of the GC heap.
 */
VMEXPORT void *vm_heap_base_address();

/**
 * @return The ending address of the GC heap.
 */
VMEXPORT void *vm_heap_ceiling_address();

/**
 * @return <code>TRUE</code> if vtable pointers within objects are to be treated
 *         as offsets rather than raw pointers.
 */
VMEXPORT Boolean vm_vtable_pointers_are_compressed();

/**
 * @return The offset to the vtable pointer in an object.
 */ 
VMEXPORT int object_get_vtable_offset();

/**
 * @return The base address of the vtable memory area. This value will
 *         never change and can be cached at startup.
 */
VMEXPORT POINTER_SIZE_INT vm_get_vtable_base();

/**
 * @return The width in bytes (e.g. 4 or 8) of the vtable type
 *         information in each object's header. This is typically used
 *         by the JIT for generating type-checking code, e.g. for inlined
 *         type checks or for inlining of virtual methods.
 */
VMEXPORT unsigned vm_get_vtable_ptr_size();

/**
 * Sets the property for <code>table_number</code> property table. <code>NULL</code> value is supported.
 */
VMEXPORT void set_property(const char* key, const char* value, PropertyTable table_number);

/**
 * @return The value of the property from <code>table_number</code> property table if it
 *         has been set by <code>set_property</code> function. Otherwise <code>NULL</code>.
 */
VMEXPORT char* get_property(const char* key, PropertyTable table_number);

/**
 * Safety frees memory of value returned by <code>get_property</code> function.
 */ 
VMEXPORT void destroy_property_value(char* value);

/**
 * Checks if the property is set. 
 *
 * @return -1 if <code>table_number</code> is incorrect.<BR>
 *          1 if property is set in <code>table_number</code> property table.<BR>
 *          0 otherwise.
 */
VMEXPORT int is_property_set(const char* key, PropertyTable table_number);

/**
 * Unsets the property in <code>table_number</code> property table.
 */
VMEXPORT void unset_property(const char* key, PropertyTable table_number);

/**
 * @return An array of keys from <code>table_number</code> properties table.
 */ 
VMEXPORT char** get_properties_keys(PropertyTable table_number);

/**
 * @return An array of keys which start with specified prefix from 
 *         <code>table_number</code> properties table.
 */ 
VMEXPORT char** get_properties_keys_staring_with(const char* prefix, PropertyTable table_number);

/**
 * Safety frees array of keys memory which returned by <code>get_properties_keys</code>
 * or <code>get_properties_keys_staring_with</code> functions.
 */
VMEXPORT void destroy_properties_keys(char** keys);

/**
 * Tries to interpret property value as <code>Boolean</code> and returns it. 
 * In case of failure returns <code>default_value</code>.
 */
VMEXPORT Boolean get_boolean_property(const char* property, Boolean default_value, PropertyTable table_number);

/**
 * Tries to interpret property value as <code>int</code> and returns it. In case of failure 
 * returns <code>default_value</code>.
 */
VMEXPORT int get_int_property(const char *property_name, int default_value, PropertyTable table_number);

/**
 * Tries to interpret property value as <code>int</code> and returns it. In case of failure 
 * returns <code>default_value</code>.
 */
VMEXPORT size_t get_size_property(const char *property_name, size_t default_value, PropertyTable table_number);


//Tries to interpret property value as int and returns it. In case of failure returns default_value.
// Numbers can include 'm' or 'M' for megabytes, 'k' or 'K' for kilobytes, and 'g' or 'G' for gigabytes (for example, 32k is the same as 32768).
VMEXPORT int64 get_numerical_property(const char *property_name, int64 default_value, PropertyTable table_number);

/**
 * Returns the address of the global flag that specifies whether
 * MethodEntry event is enabled. JIT should call this function in case
 * a method is compiled with exe_notify_method_entry flag set.
 */
VMEXPORT char *get_method_entry_flag_address();

/**
 * Returns the address of the global flag that specifies whether
 * MethodExit event is enabled. JIT should call this function in case
 * a method is compiled with exe_notify_method_exit flag set.
 */
VMEXPORT char *get_method_exit_flag_address();

////
// end miscellaneous functions.
////

#ifdef __cplusplus
}
#endif


#endif // _VM_EXPORT_H
