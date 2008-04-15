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

#define OPEN_VM "vm"
#define OPEN_VM_VERSION "1.0"
#define OPEN_INTF_VM "open.interface.vm." OPEN_VM_VERSION

#define O_A_H_VM_VMDIR  "org.apache.harmony.vm.vmdir"

#ifdef WIN32
#include <stddef.h>
#else
#include <unistd.h>
#endif
#include "open/types.h"


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
 * @return <code>TRUE</code> if the class is a value type.
 */
 VMEXPORT Boolean class_is_valuetype(Class_Handle ch);

/**
 * This function can only be called if (<code>class_is_enum(ch)</code> == <code>TRUE</code>)
 * The returned value is the type of the underlying int type.
 */
VMEXPORT VM_Data_Type class_get_enum_int_type(Class_Handle ch);

/**
 * The super class of the current class. 
 * @return <code>NULL</code> for the system object class, i.e.
 *         <code>class_get_super_class</code>(vm_get_system_object_class()) == NULL
 */
VMEXPORT Class_Handle class_get_super_class(Class_Handle ch);

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
 * @return <code>TRUE</code> if all instances of this class are pinned.
 */
 VMEXPORT void* class_alloc_via_classloader(Class_Handle ch, int32 size);

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
 * @return The size in bytes of an instance in the heap.
 */
 VMEXPORT unsigned class_get_boxed_data_size(Class_Handle ch);

/**
 * @return The offset to the start of user data form the start of a boxed
 *         instance.
 */
 VMEXPORT unsigned class_get_unboxed_data_offset(Class_Handle ch);

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
