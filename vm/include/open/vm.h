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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.2.4.3 $
 */


//
// These are the functions that a VM built as a DLL must export.
// Some functions may be optional and are marked as such.
//

#ifndef _VM_EXPORT_H
#define _VM_EXPORT_H





#include "open/types.h"

#define O_A_H_VM_VMDIR         "org.apache.harmony.vm.vmdir"

#ifdef __cplusplus
extern "C" {
#endif

// This structure is meant to be opaque.  External modules should not
// attempt to directly access any of its fields.
typedef struct ChaClassIterator {
    Class_Handle _root_class;
    Class_Handle _current;
    Boolean _is_valid;
} ChaClassIterator;


// This structure is meant to be opaque.  External modules should not
// attempt to directly access any of its fields.
typedef struct ChaMethodIterator {
    Method_Handle _method;
    Method_Handle _current;
    ChaClassIterator _class_iter;
} ChaMethodIterator;


/////////////////////////////////////////////////////////////////
// begin class-related functions.


// Returns a handle for the object class.  For Java applications, it's
// java.lang.Object.
VMEXPORT Class_Handle get_system_object_class();

// Returns a handle for the class class.  For Java applications, it's
// java.lang.Class.
VMEXPORT Class_Handle get_system_class_class();

// Returns a handle for the string class.  For Java applications, it's
// java.lang.String.
VMEXPORT Class_Handle get_system_string_class();

// Loads a class of a given name.  If a class cannot be loaded, returns NULL
VMEXPORT Class_Handle
class_load_class_by_name_using_bootstrap_class_loader(const char *name);

// The following three functions will be eventually renamed to
// class_is_final, class_is_abstract and class_is_interface,
// but right now that would conflict with the names of some internal macros.
VMEXPORT Boolean class_property_is_final(Class_Handle ch);
VMEXPORT Boolean class_property_is_abstract(Class_Handle ch);
VMEXPORT Boolean class_property_is_interface2(Class_Handle ch);

// Returns TRUE if th class is likely to be used as an exception object.
// This is a hint only.  If the result is FALSE, the class may still
// be used for exceptions but it is less likely.
VMEXPORT Boolean class_hint_is_exceptiontype(Class_Handle ch);

// Returns TRUE if the class is a value type.
VMEXPORT Boolean class_is_valuetype(Class_Handle ch);

// Return TRUE if the class represents an enum. For Java 1.4 always returns FALSE.
VMEXPORT Boolean class_is_enum(Class_Handle ch);

// This function can only be called if (class_is_enum(ch) == TRUE)
// The returned value is the type of the underlying int type.
VMEXPORT VM_Data_Type class_get_enum_int_type(Class_Handle ch);

// Returns TRUE if the class represents a primitive type (int, float, etc.)
VMEXPORT Boolean class_is_primitive(Class_Handle ch);

// Returns the name of the class.
VMEXPORT const char *class_get_name(Class_Handle ch);

// Returns the name of the package containing the class.
VMEXPORT const char *class_get_package_name(Class_Handle ch);

// Returns the super class of the current class.
// Returns NULL for the system object class, i.e.
// class_get_super_class(get_system_object_class()) == NULL
VMEXPORT Class_Handle class_get_super_class(Class_Handle ch);

// Return the vtable handle of the given class.
VMEXPORT VTable_Handle class_get_vtable(Class_Handle ch);

// Returns the allocation handle to be used for the object allocation
// routines, given a class handle.
VMEXPORT Allocation_Handle class_get_allocation_handle(Class_Handle ch);

// Returns the class handle corresponding to a given allocation handle.
VMEXPORT Class_Handle allocation_handle_get_class(Allocation_Handle ah);

// Return an VM_Data_Type value for a given class.
VMEXPORT VM_Data_Type class_get_primitive_type_of_class(Class_Handle ch);

// Return a class corresponding to a primitive type.  For all primitive types t
// t == class_get_primitive_type_of_class(class_get_class_of_primitive_type(t))
VMEXPORT Class_Handle class_get_class_of_primitive_type(VM_Data_Type typ);

// Returns TRUE is the class is an array.
VMEXPORT Boolean class_is_array(Class_Handle ch);

// Returns TRUE if class s is assignment compatible with class t.
VMEXPORT Boolean class_is_instanceof(Class_Handle s, Class_Handle t);

// Given a class handle cl construct a class handle of the type
// representing array of cl.  If class cl is value type, assume
// that the element is a reference to a boxed instance of that type.
VMEXPORT Class_Handle class_get_array_of_class(Class_Handle ch);

// Given a class handle cl construct a class handle of the type
// representing array of cl.  Class cl is assumed to be a
// value type.  Return NULL if cl is not a value type.
VMEXPORT Class_Handle class_get_array_of_unboxed(Class_Handle ch);

// For a class that is an array return the type info for the elements
// of the array
VMEXPORT Type_Info_Handle class_get_element_type_info(Class_Handle ch);

// Returns TRUE if the class is already fully initialized.
VMEXPORT Boolean class_is_initialized(Class_Handle ch);

// Returns TRUE if the class is neither initialized nor in the process
// of being initialized.  The intention is that the JIT will emit a call
// to VM_RT_INITIALIZE_CLASS before every access to a static field in
// Java.
VMEXPORT Boolean class_needs_initialization(Class_Handle ch);

// For Java returns FALSE.
VMEXPORT Boolean class_is_before_field_init(Class_Handle ch);

// Number of instance fields defined in a class.  That doesn't include
// inherited fields.
VMEXPORT unsigned class_num_instance_fields(Class_Handle ch);

// Get the handle for a field.  If idx is greater than or equal to
// class_num_instance_fields, return NULL.
// The value of idx indexes into the fields defined in this class and
// doesn't include inherited fields.
VMEXPORT Field_Handle class_get_instance_field(Class_Handle ch, unsigned idx);

// Number of instance fields defined in a class.  This number includes
// inherited fields.
VMEXPORT unsigned class_num_instance_fields_recursive(Class_Handle ch);

// Get the handle for a field.  If idx is greater than or equal to
// class_num_instance_fields_recursive, return NULL.
// The value of idx indexes into the set of fields that includes both fields
// defined in this class and inherited fields.
VMEXPORT Field_Handle class_get_instance_field_recursive(Class_Handle ch, unsigned idx);

// Number of methods declared in the class
VMEXPORT unsigned class_get_number_methods(Class_Handle ch);

// Get handle for a method declared in class
VMEXPORT Method_Handle class_get_method(Class_Handle ch, unsigned index);


// Returns TRUE if all instances of this class are pinned.
VMEXPORT Boolean class_is_pinned(Class_Handle ch);

// Returns TRUE if all instances of this class are pinned.
VMEXPORT void* class_alloc_via_classloader(Class_Handle ch, int32 size);

// Returns TRUE if this is an array of primitives.
VMEXPORT Boolean class_is_non_ref_array(Class_Handle ch);

// Returns TRUE if the class has a non-trivial finalizer.
VMEXPORT Boolean class_is_finalizable(Class_Handle ch);
// This exactly what I want.
// Get the alignment of the class.
VMEXPORT unsigned class_get_alignment(Class_Handle ch);

// Get the alignment of the class when it's unboxed.
VMEXPORT unsigned class_get_alignment_unboxed(Class_Handle ch);

// For a class handle that represents and array, return the size of the
// element of the array.
VMEXPORT unsigned class_element_size(Class_Handle ch);

// Return the size in bytes of an instance in the heap.
VMEXPORT unsigned class_get_boxed_data_size(Class_Handle ch);

// Return the offset to the start of user data form the start of a boxed
// instance.
VMEXPORT unsigned class_get_unboxed_data_offset(Class_Handle ch);

// Return the class of the array element of the given class.
// The behavior is undefined if the parameter does not represent
// an array class.
VMEXPORT Class_Handle class_get_array_element_class(Class_Handle ch);

// Returns the offset from the start of the vtable at which the
// superclass hierarchy is stored.  This is for use with fast type
// checking.
VMEXPORT int vtable_get_super_array_offset();

// Returns the number of superclass hierarchy elements that are
// stored within the vtable.  This is for use with fast type checking.
VMEXPORT int vm_max_fast_instanceof_depth();


/////////////////////////////////////////////////////////////////
// begin class iterator related functions.

// Initializes the CHA_Class_Iterator, to iterate over all classes that
// descend from root_class, including root_class itself.
// Returns TRUE if iteration is supported over root_class, FALSE if not.
VMEXPORT Boolean class_iterator_initialize(ChaClassIterator*, Class_Handle root_class);

// Returns the current class of the iterator.  NULL is returned if
// therea are no more classes.
VMEXPORT Class_Handle class_iterator_get_current(ChaClassIterator*);

// Advances the iterator.
VMEXPORT void class_iterator_advance(ChaClassIterator*);


/////////////////////////////////////////////////////////////////
// begin inner-class related functions.

// Returns TRUE the number of inner classes
VMEXPORT unsigned class_number_inner_classes(Class_Handle ch);

// Returns TRUE if an inner class is public
VMEXPORT Boolean class_is_inner_class_public(Class_Handle ch, unsigned idx);

// Return an inner class
VMEXPORT Class_Handle class_get_inner_class(Class_Handle ch, unsigned idx);

// Return the class that declared this one, or NULL if top-level class
VMEXPORT Class_Handle class_get_declaring_class(Class_Handle ch);


// end inner-class related functions.
/////////////////////////////////////////////////////////////////


// end class-related functions.
/////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////
// begin field-related functions.


// Returns TRUE is the field is static.
VMEXPORT Boolean field_is_static(Field_Handle fh);

// Returns the type info that represents the type of the field.
VMEXPORT Type_Info_Handle field_get_type_info_of_field_value(Field_Handle fh);

// Returns the class that represents the type of the field.
VMEXPORT Class_Handle field_get_class_of_field_value(Field_Handle fh);

// Returns the class that defined that field.
VMEXPORT Class_Handle field_get_class(Field_Handle fh);

// Returns TRUE if the field is of a reference type.
// This function doesn't cause resolution of the class of the field.
VMEXPORT Boolean field_is_reference(Field_Handle fh);

// Returns TRUE if the field is literal.  In Java, it means that the
// field had the ConstantValue attribute set (JVMS2, Section  4.7.2).
VMEXPORT Boolean field_is_literal(Field_Handle fh);

// For Java always FALSE.
VMEXPORT Boolean field_is_unmanaged_static(Field_Handle fh);

// Returns the offset to an instance field.
VMEXPORT unsigned field_get_offset(Field_Handle fh);

// Returns an address of a static field.
VMEXPORT void *field_get_address(Field_Handle fh);

// Returns a name of the field.
VMEXPORT const char *field_get_name(Field_Handle fh);

// Returns the field descriptor.  The descriptor is a string representation
// of the field types as defined by the
// JVM spec.
VMEXPORT const char *field_get_descriptor(Field_Handle fh);

// Returns TRUE if the field is final.
VMEXPORT Boolean field_is_final(Field_Handle fh);

// Returns TRUE if the field is volatile.
VMEXPORT Boolean field_is_volatile(Field_Handle fh);

// Returns TRUE if the field is private.
VMEXPORT Boolean field_is_private(Field_Handle fh);

/**
 * Returns the address and bit mask, for the flag which determine whether field
 * access event should be sent. JIT may use the following expression to
 * determine if specified field access should be tracked:
 * ( **address & *mask != 0 )
 * @param field - handle of the field
 * @param[out] address - pointer to the address of the byte which contains the flag
 * @param[out] mask - pointer to the bit mask of the flag
 */
VMEXPORT void
field_get_track_access_flag(Field_Handle field, char** address, char* mask);

/**
 * Returns the address and bit mask, for the flag which determine whether field
 * modification event should be sent. JIT may use the following expression to
 * determine if specified field modification should be tracked:
 * ( **address & *mask != 0 )
 * @param field - handle of the field
 * @param[out] address - pointer to the address of the byte which contains the flag
 * @param[out] mask - pointer to the bit mask of the flag
 */
VMEXPORT void
field_get_track_modification_flag(Field_Handle field, char** address,
                                  char* mask);


// end field-related functions.
/////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////
// begin method-related functions.


// Returns TRUE if this a Java method.  Every Java JIT must call this
// function before compiling a method and return JIT_FAILURE if
// method_is_java returned FALSE.
VMEXPORT Boolean method_is_java(Method_Handle mh);

// Returns the method name.
VMEXPORT const char  *method_get_name(Method_Handle mh);

// Returns the method descriptor.  The descriptor is a string representation
// of the parameter and return value types as defined by the
// JVM spec.
VMEXPORT const char  *method_get_descriptor(Method_Handle mh);

// Returns a class in which the method is declared.
VMEXPORT Class_Handle method_get_class(Method_Handle mh);

// Returns TRUE if the method is private.
VMEXPORT Boolean method_is_private(Method_Handle mh);

// Returns TRUE if the method is static.
VMEXPORT Boolean method_is_static(Method_Handle mh);

// Returns TRUE if the method is final.
VMEXPORT Boolean method_is_final(Method_Handle mh);

// Returns TRUE if the method is native.
VMEXPORT Boolean method_is_native(Method_Handle mh);

// Returns TRUE if the method is synchronized.
VMEXPORT Boolean method_is_synchronized(Method_Handle mh);

// Returns TRUE if the method is abstract.
VMEXPORT Boolean method_is_abstract(Method_Handle mh);

// Java methods may have a flag set to indicate that floating point operations
// must be performed in the strict mode.  method_is_strict() returns TRUE is
// the ACC_STRICT flag is set for a Java method and FALSE otherwise.
VMEXPORT Boolean method_is_strict(Method_Handle m);

// Returns TRUE if the method has been overriden in a subclass and FALSE
// otherwise.  Note that if method_is_overridden returns FALSE, loading
// of a subclass later in the execution of the program may change invalidate
// this condition.  If a JIT uses method_is_overridden to implement
// unconditional inlining, it must be prepared to patch the code later
// (see vm_register_jit_overridden_method_callback).
VMEXPORT Boolean method_is_overridden(Method_Handle m);

// Returns TRUE if the method should not be inlined.  There may also
// be other reasons why a method shouldn't be inlined, e.g., native methods
// can't be inlined and in Java you can't inlined methods that are
// loaded by a different class loader than the caller.
// Always FALSE for Java.
VMEXPORT Boolean method_is_no_inlining(Method_Handle mh);

// Always FALSE for Java.
VMEXPORT Boolean method_is_require_security_object(Method_Handle mh);

// Return a signature that can be used to iterate over method's arguments
// and query the type of the method result.
VMEXPORT Method_Signature_Handle method_get_signature(Method_Handle mh);

// Class ch is a subclass of method_get_class(mh).  The function returns a method handle
// for an accessible method overriding mh in ch or in its closest superclass that overrides mh.
// Class ch must be a class not an interface.
VMEXPORT Method_Handle method_find_overridden_method(Class_Handle ch, Method_Handle mh);


/////////////////////////////////////////////////////////////////
// begin method iterator related functions.

// Initializes the CHA_Method_Iterator, to iterate over all methods that
// match the method signature and descend from root_class (including root_class itself).
// Returns TRUE if iteration is supported over root_class, FALSE if not.
VMEXPORT Boolean method_iterator_initialize(ChaMethodIterator*, Method_Handle method, Class_Handle root_class);

// Returns the current method of the iterator.  NULL is returned if
// therea are no more methods.
VMEXPORT Method_Handle method_iterator_get_current(ChaMethodIterator*);

// Advances the iterator.
VMEXPORT void method_iterator_advance(ChaMethodIterator*);


///////////////////////////////////////////////////////
// begin method signature-related functions.

// Returns the method descriptor for a given method signature.
// See method_get_descriptor().
VMEXPORT const char *method_sig_get_descriptor(Method_Signature_Handle s);

// Return a signature that can be used to iterate over method's arguments
// and query the type of the method result.
// Java doesn't have standalone signatures, so for Java, always return NULL.
VMEXPORT Method_Signature_Handle method_standalone_signature(Class_Handle ch,
                                                              unsigned idx);

// end method signature-related functions.
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// begin local variables-related functions.

// Returns the number of local variables defined for the method.
VMEXPORT unsigned method_vars_get_number(Method_Handle mh);

// Return the type info of the local variable number idx.
// Since local variables are not typed in Java. this function
// always returns NULL for Java methods.
VMEXPORT Type_Info_Handle method_vars_get_type_info(Method_Handle mh,
                                                     unsigned idx);

// Returns TRUE if the local variable is a managed pointer.
VMEXPORT Boolean method_vars_is_managed_pointer(Method_Handle mh, unsigned idx);

// Returns TRUE if the local variable is pinned.
VMEXPORT Boolean method_vars_is_pinned(Method_Handle mh, unsigned idx);

// end local variables-related functions.
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// begin argument-related functions.

// Returns the number of arguments defined for the method.
// This number automatically includes the this pointer (if present).
VMEXPORT unsigned method_args_get_number(Method_Signature_Handle msh);

VMEXPORT Boolean method_args_has_this(Method_Signature_Handle msh);

// Return the class handle of the argument number idx.
// That's true even for primitive types like int or float.
VMEXPORT Type_Info_Handle method_args_get_type_info(Method_Signature_Handle msh,
                                                     unsigned idx);

// Returns TRUE if the argument is a managed pointer.
VMEXPORT Boolean method_args_is_managed_pointer(Method_Signature_Handle msh, unsigned idx);

// end argument-related functions.
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// begin return value-related functions.

VMEXPORT Type_Info_Handle method_ret_type_get_type_info(Method_Signature_Handle msh);

// Returns TRUE if the return value is a managed pointer.
VMEXPORT Boolean method_ret_type_is_managed_pointer(Method_Signature_Handle msh);

// end return value-related functions.
///////////////////////////////////////////////////////


// end method-related functions.
/////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////
// begin type info-related functions.

// Array shapes and custom modifiers are not implemented yet.

// If type info is a reference, type_info_get_class will return the class
// of the reference.
VMEXPORT Boolean type_info_is_reference(Type_Info_Handle tih);

// If type info is unboxed, type_info_get_class will return the class
// of the unboxed type and class_is_primitive will return its VM_Data_Type.
VMEXPORT Boolean type_info_is_unboxed(Type_Info_Handle tih);

// Returns TRUE if the type is a primitive type.  type_info_is_primitive does
// not cause resolution in contrast to the otherwise equivalentcall sequence
// suggested in the description of type_info_is_unboxed (i.e.
// type_info_is_unboxed-->type_info_get_class-->class_is_primitive).
VMEXPORT Boolean type_info_is_primitive(Type_Info_Handle tih);

// If TRUE, then
// type_info_get_type_info returns the type info that the pointer
// points to.
VMEXPORT Boolean type_info_is_unmanaged_pointer(Type_Info_Handle tih);

// For a return value a type can be void when it is not an unmanaged pointer.
// In all other contexts, if type_info_is_void is TRUE then
// type_info_is_unmanaged_pointer is TRUE too.
VMEXPORT Boolean type_info_is_void(Type_Info_Handle tih);

// If TRUE, use type_info_get_method_sig to retrieve the method signature.
VMEXPORT Boolean type_info_is_method_pointer(Type_Info_Handle tih);

// Is it a vector, i.e., a one-dimensional, zero-based array.
VMEXPORT Boolean type_info_is_vector(Type_Info_Handle tih);

// Is it a general array, i.e., either multidimensional or non zero-based.
// Returns FALSE for vectors.  Always FALSE for Java.
VMEXPORT Boolean type_info_is_general_array(Type_Info_Handle tih);

// Get the class if type_info_is_reference or type_info_is_unboxed
// returned TRUE. If the type info is a vector or a general array, return the
// class handle for the array type (not the element type).
VMEXPORT Class_Handle type_info_get_class(Type_Info_Handle tih);

// Returns loading error (jthrowable) from classloader. This function is only
// valid after type_info_get_class. If type_info_get_class returned no runtime
// exception then loading error happened which should be obtained by this
// function.
VMEXPORT Managed_Object_Handle* type_info_get_loading_error(Type_Info_Handle tih);

// Get the method signature if type_info_is_method_pointer returned TRUE.
VMEXPORT Method_Signature_Handle type_info_get_method_sig(Type_Info_Handle tih);

// Get recursively type info if type_info_is_unmanaged_pointer,
// type_info_is_vector or type_info_is_general_array returned TRUE.
VMEXPORT Type_Info_Handle type_info_get_type_info(Type_Info_Handle tih);

// Return an VM_Data_Type corresponding to a type info.
// This function is provided for convenience as it can be implemented in terms
// of other functions provided in this interface.
VMEXPORT VM_Data_Type type_info_get_type(Type_Info_Handle tih);

// end type info-related functions.
///////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////
// begin vector layout functions.

// Vectors are one-dimensional, zero-based arrays.  All Java "arrays" are
// vectors.
// Functions provided in this section do not work on multidimensional or
// non-zero based arrays (i.e. arrays with a lower bound that is non-zero
// for at least one of the dimensions.


// Return the offset to the length field of the array.  That field has
// the same offset for vectors of all types.
VMEXPORT int vector_length_offset();

// Deprecated.  Please use vector_length_offset instead.
//VMEXPORT int array_length_offset();

// Return the offset to the first element of the vector of the given type.
// This function is provided for the cases when the class handle of the
// element is not available.
VMEXPORT int vector_first_element_offset(VM_Data_Type element_type);

// Deprecated.  Please use vector_first_element_offset instead.
//VMEXPORT int array_first_element_offset(VM_Data_Type element_type);

// Return the offset to the first element of the vector of the given type.
// Assume that the elements are boxed. Byte offset.
VMEXPORT int vector_first_element_offset_class_handle(Class_Handle element_type);
VMEXPORT int vector_first_element_offset_vtable_handle(VTable_Handle element_type);

// Deprecated.  Please use vector_first_element_offset_class_handle instead.
//VMEXPORT int array_first_element_offset_class_handle(Class_Handle element_type);

// Return the offset to the first element of the vector of the given type.
// If the class is a value type, assume that elements are unboxed.
// If the class is not a value type, assume that elements are references.
VMEXPORT int vector_first_element_offset_unboxed(Class_Handle element_type);

// Deprecated.  Please use vector_first_element_offset_unboxed instead.
//VMEXPORT int array_first_element_offset_unboxed(Class_Handle element_type);

// Return the length of a vector.  The caller must ensure that GC will not
// move or deallocate the vector while vector_get_length() is active.
VMEXPORT int32 vector_get_length(Vector_Handle vector);

// Return the address to an element of a vector of references.
// The caller must ensure that GC will not move or deallocate the vector
// while vector_get_element_address_ref() is active.
VMEXPORT Managed_Object_Handle *
vector_get_element_address_ref(Vector_Handle vector, int32 idx);

// Return the size of a vector of a given number of elements.
// The size is rounded up to take alignment into account.
VMEXPORT unsigned vm_vector_size(Class_Handle vector_class, int length);


// end vector layout functions.
/////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////
// begin miscellaneous functions.


// Returns TRUE if references within objects and vector elements are
// to be treated as offsets rather than raw pointers.
VMEXPORT Boolean vm_references_are_compressed();

// Returns the starting address of the GC heap.
VMEXPORT void *vm_heap_base_address();

// Returns the ending address of the GC heap.
VMEXPORT void *vm_heap_ceiling_address();

// Returns TRUE if vtable pointers within objects are to be treated
// as offsets rather than raw pointers.
VMEXPORT Boolean vm_vtable_pointers_are_compressed();

// Returns the offset to the vtable pointer in an object.
VMEXPORT int object_get_vtable_offset();

// Returns the base address of the vtable memory area.  This value will
// never change and can be cached at startup.
VMEXPORT POINTER_SIZE_INT vm_get_vtable_base();

// Returns the width in bytes (e.g. 4 or 8) of the vtable type
// information in each object's header.  This is typically used
// by the JIT for generating type-checking code, e.g. for inlined
// type checks or for inlining of virtual methods.
VMEXPORT unsigned vm_get_vtable_ptr_size();

// Returns a printable signature.  The character buffer is owned by the
// caller.  Call free_string_buffer to reclaim the memory.
VMEXPORT char *method_sig_get_string(Method_Signature_Handle msh);

// Free a string buffer returned by method_sig_get_string.
VMEXPORT void free_string_buffer(char *buffer);



typedef enum {
    VM_PROPERTIES  = 0,
    JAVA_PROPERTIES = 1
} PropertyTable;

//Sets the property for table_number property table. NULL value is supported.
VMEXPORT void set_property(const char* key, const char* value, PropertyTable table_number);

//Returns the value of the property from table_number property table if it
//has been set by set_property function. Otherwise returns NULL.
VMEXPORT char* get_property(const char* key, PropertyTable table_number);

//Safety frees memory of value returned by get_property function.
VMEXPORT void destroy_property_value(char* value);

//Checks if the property is set. Return:
//   -1 if table_number is incorrect.
//    1 if property is set in table_number property table.
//    0 otherwise.
VMEXPORT int is_property_set(const char* key, PropertyTable table_number);

//Unsets the property in table_number property table.
VMEXPORT void unset_property(const char* key, PropertyTable table_number);

//Returns an array of keys from table_number properties table.
VMEXPORT char** get_properties_keys(PropertyTable table_number);

//Returns an array of keys which start with specified prefix from table_number properties table.
VMEXPORT char** get_properties_keys_staring_with(const char* prefix, PropertyTable table_number);

//Safety frees array of keys memory which returned by get_properties_keys
//or get_properties_keys_staring_with functions.
VMEXPORT void destroy_properties_keys(char** keys);

//Tries to interpret property value as Boolean and returns it. In case of failure returns default_value.
VMEXPORT Boolean get_boolean_property(const char* property, Boolean default_value, PropertyTable table_number);

//Tries to interpret property value as int and returns it. In case of failure returns default_value.
VMEXPORT int get_int_property(const char *property_name, int default_value, PropertyTable table_number);
// end miscellaneous functions.
/////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif // _VM_EXPORT_H
