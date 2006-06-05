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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.2.4.4 $
 */  



#define LOG_DOMAIN "vm.arrays"
#include "cxxlog.h"

#include "platform.h"
#include <assert.h>

#include "environment.h"
#include "exceptions.h"
#include "object_handles.h"
#include "object_layout.h"
#include "vm_arrays.h"
#include "vm_synch.h"
#include "open/vm_util.h"
#include "open/thread.h"
#include "vm_stats.h"


#define BITS_PER_BYTE 8
#define HIGH_BIT_SET_MASK (1<<((sizeof(unsigned) * BITS_PER_BYTE)-1))
#define HIGH_BIT_CLEAR_MASK (~HIGH_BIT_SET_MASK)
#ifndef NEXT_TO_HIGH_BIT_SET_MASK
#define NEXT_TO_HIGH_BIT_SET_MASK (1<<((sizeof(unsigned) * BITS_PER_BYTE)-2))
#define NEXT_TO_HIGH_BIT_CLEAR_MASK (~NEXT_TO_HIGH_BIT_SET_MASK)
#endif /* #ifndef NEXT_TO_HIGH_BIT_SET_MAS */
#define TWO_HIGHEST_BITS_SET_MASK (HIGH_BIT_SET_MASK|NEXT_TO_HIGH_BIT_SET_MASK)



/////////////////////////////////////////////////////////////
// begin vector access functions
//
// Functions to access the length field and and the elements of managed vectors


int vector_length_offset()
{
    return (int)VM_Vector::length_offset();
} //vector_length_offset




// end vector access functions
/////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////
// begin Java array allocation



static Vector_Handle vm_anewarray_resolved_array_type(Class *arr_clss, int length)
{
#ifdef VM_STATS
    vm_stats_total.num_anewarray++;  
#endif
    assert(!tmn_is_suspend_enabled());
    assert(!arr_clss->is_array_of_primitives);

    if (length < 0) {
        tmn_suspend_enable();
        throw_java_exception("java/lang/NegativeArraySizeException");
        tmn_suspend_disable();
    }

    unsigned sz = vm_array_size(arr_clss->vtable, length);

    if ((length & TWO_HIGHEST_BITS_SET_MASK) || (sz & TWO_HIGHEST_BITS_SET_MASK)) {
        // VM does not support arrays of length >= MAXINT>>2
        // GC does not support objects of length >= 1Gb
        tmn_suspend_enable();
        throw_java_exception("java/lang/OutOfMemoryError", 
            "VM doesn't support arrays of the requested size");
        tmn_suspend_disable();
        return NULL; // may be reached on interpreter or when called from jni
    }

    Vector_Handle object_array = (Vector_Handle )gc_alloc(sz, arr_clss->allocation_handle, vm_get_gc_thread_local());
#ifdef VM_STATS
    arr_clss->num_allocations++;    
    arr_clss->num_bytes_allocated += sz;
#endif //VM_STATS

    if (NULL == object_array) {
        exn_throw(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return NULL; // may be reached on interpreter or when called from jni
    }

    set_vector_length(object_array, length);
    assert(get_vector_vtable(object_array) == arr_clss->vtable);
    return object_array;
} //vm_anewarray_resolved_array_type

// Allocate a vector (i.e., a zero-based, one-dimensional array).
// This function works for array of primitive types and reference
// types.  We will have to extend it to arrays of unboxed value types.
//
// Called from:
//  - jit_runtime_support_ia32.cpp
//  - jit_runtime_support_ipf.cpp
//  - IntelVM.cpp
//

VMEXPORT // temporary solution for interpreter unplug
Vector_Handle vm_new_vector_primitive(Class *vector_class, int length) {
    assert(!tmn_is_suspend_enabled());
    // VM does not support arrays of length >= MAXINT>>2
    if (length & TWO_HIGHEST_BITS_SET_MASK) {
        tmn_suspend_enable();
        if (length < 0) {
            throw_java_exception("java/lang/NegativeArraySizeException");
        } else {
            throw_java_exception("java/lang/OutOfMemoryError", 
                "VM doesn't support arrays of the requested size");
        }
        tmn_suspend_disable();
        return NULL;
    }
    assert(vector_class->is_array_of_primitives);
    assert(vector_class->vtable);
    unsigned sz = vm_array_size(vector_class->vtable, length);
    Vector_Handle vector = (Vector_Handle)gc_alloc(sz, 
        vector_class->allocation_handle, vm_get_gc_thread_local());
#ifdef VM_STATS
    vector_class->num_allocations++;
    vector_class->num_bytes_allocated += sz;
#endif //VM_STATS

    if (NULL == vector) {
        exn_throw(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return 0; // may be reached when interpreter is used
    }

    set_vector_length(vector, length);
    assert(get_vector_vtable(vector) == vector_class->vtable);
    return vector;
}

VMEXPORT // temporary solution for interpreter unplug
Vector_Handle vm_new_vector(Class *vector_class, int length)
{
    if(vector_class->is_array_of_primitives) {
        return vm_new_vector_primitive(vector_class,length);
    } else {
        return vm_anewarray_resolved_array_type(vector_class, length);
    }
}



// This function is deprecated.
// "Fast" version of vm_new_vector: returns NULL if a GC would be required for the allocation or no storage is available.
Vector_Handle vm_new_vector_or_null(Class * UNREF vector_class, int UNREF length)
{
    return NULL;
} //vm_new_vector_or_null



void vm_new_vector_update_stats(Allocation_Handle vector_handle, POINTER_SIZE_INT length, void * UNREF tp)
{
#ifdef VM_STATS
    if (0 != (length&TWO_HIGHEST_BITS_SET_MASK)) return;
    VTable *vector_vtable = ManagedObject::allocation_handle_to_vtable(vector_handle);
    unsigned sz = vm_array_size(vector_vtable, (unsigned)length);
    vector_vtable->clss->num_allocations++;
    vector_vtable->clss->num_bytes_allocated += sz;
    if (!get_prop_non_ref_array(vector_vtable->class_properties))
    {
        vm_stats_total.num_anewarray++;
    }
#endif //VM_STATS
}



Vector_Handle vm_new_vector_using_vtable_and_thread_pointer(Allocation_Handle vector_handle, int length, void *tp)
{
    assert( ! tmn_is_suspend_enabled());
    // VM does not support arrays of length >= MAXINT>>2
    if (length & TWO_HIGHEST_BITS_SET_MASK) {
        tmn_suspend_enable();
        if (length < 0) {
            throw_java_exception("java/lang/NegativeArraySizeException");
        } else {
            throw_java_exception("java/lang/OutOfMemoryError", 
                "VM doesn't support arrays of the requested size");
        }
        tmn_suspend_disable();
        return NULL;
    }

    VTable *vector_vtable = ManagedObject::allocation_handle_to_vtable(vector_handle);
    unsigned sz = vm_array_size(vector_vtable, length);
#ifdef VM_STATSxxx // Functionality moved into vm_new_vector_update_stats().
    vector_vtable->clss->num_allocations++;
    vector_vtable->clss->num_bytes_allocated += sz;
#endif //VM_STATS
    assert( ! tmn_is_suspend_enabled());
    Vector_Handle vector = (Vector_Handle)gc_alloc(sz, vector_handle, tp);
    assert( ! tmn_is_suspend_enabled());
    
    if (NULL == vector) {
        exn_throw(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return 0; // should never be reached
    }

    set_vector_length(vector, length);
    assert(get_vector_vtable(vector) == vector_vtable);
    return vector;
} //vm_new_vector_using_vtable_and_thread_pointer



Vector_Handle vm_new_vector_or_null_using_vtable_and_thread_pointer(Allocation_Handle vector_handle, int length, void *tp)
{
    // VM does not support arrays of length >= MAXINT>>2
    if (0 != (length&TWO_HIGHEST_BITS_SET_MASK)) {
        // exception wiil be thown from other method
        return NULL;
    }

    VTable *vector_vtable = ManagedObject::allocation_handle_to_vtable(vector_handle);
    unsigned sz = vm_array_size(vector_vtable, length);
    Vector_Handle vector = (Vector_Handle)gc_alloc_fast(sz, vector_handle, tp);
    if (vector == NULL)
    {
        return NULL;
    }
#ifdef VM_STATSxxx // Functionality moved into vm_new_vector_update_stats().
    vector_vtable->clss->num_allocations++;
    vector_vtable->clss->num_bytes_allocated += sz;
    if (!get_prop_non_ref_array(vector_vtable->class_properties))
    {
        vm_stats_total.num_anewarray++;
    }
#endif //VM_STATS
    
    set_vector_length(vector, length);
    assert(get_vector_vtable(vector) == vector_vtable);
    return vector;
} //vm_new_vector_or_null_using_vtable_and_thread_pointer



Vector_Handle
vm_multianewarray_recursive(Class    *c,
                             int      *dims_array,
                             unsigned  dims)
{
    assert(!tmn_is_suspend_enabled());
    Global_Env *global_env = VM_Global_State::loader_env;
    int length = *dims_array;
    assert(length >= 0);
    assert((length&TWO_HIGHEST_BITS_SET_MASK) == 0);
    assert(c->name->bytes[0] == '[');
    assert(c->name->len > 1);

    if(dims == 1) {
        return (Vector_Handle)vm_new_vector(c, length);
    } else {
        // Allocate an array of arrays.
        volatile Vector_Handle object_array =
            (Vector_Handle) vm_new_vector(c, length);
        assert(!tmn_is_suspend_enabled());
        // Alexei
        // Since this function is called from a managed code
        // generated by JIT and this is a native code,
        // I assume that there exist an m2n frame
        // to allocate a local handle from.
        ObjectHandle handle = oh_convert_to_local_handle((ManagedObject*)object_array);

        Class *elem_type_clss = c->array_element_class;
        assert(elem_type_clss);
        // ppervov: array_element_class is now prepared upon creation of an array class
        //assert(class_prepare(elem_type_clss));

        // Allocate all subarrays end store them in the parent array.
        if (global_env->compress_references) {
            for (int i = 0; i < length; i++) {
                Vector_Handle elem = vm_multianewarray_recursive(elem_type_clss,
                                                                  dims_array + 1,
                                                                  dims - 1);
                object_array = handle->object;
                gc_heap_slot_write_ref_compressed((Managed_Object_Handle)object_array, 
                                                  (uint32 *)get_vector_element_address_ref(object_array, i), 
                                                  (Managed_Object_Handle)elem);
            }
        } else {
            for (int i = 0; i < length; i++) {
                Vector_Handle elem = vm_multianewarray_recursive(elem_type_clss,
                                                                  dims_array + 1,
                                                                  dims - 1);
                object_array = handle->object;
                gc_heap_slot_write_ref((Managed_Object_Handle)object_array, 
                                       (Managed_Object_Handle *)get_vector_element_address_ref(object_array, i), 
                                       (Managed_Object_Handle)elem);
            }    
        }
        oh_discard_local_handle(handle);

        return (Vector_Handle)object_array;
    }
} //vm_multianewarray_recursive



// Create a multidimensional Java array
// There is a variable # of arguments:
//  - class id
//  - number of dimensions
//  - count1
//  ...
// Return value:
//  - Reference to the new object
//
// This is __cdecl function and the caller must pop the arguments.
//
//
// Called from:
//  - jit_runtime_support_ia32.cpp
//  - jit_runtime_support_ipf.cpp
//
Vector_Handle vm_multianewarray_resolved(Class *cc, unsigned dims, ...)
{
#ifdef VM_STATS
    vm_stats_total.num_multianewarray++;  
#endif
    assert(!tmn_is_suspend_enabled());

    const unsigned max_dims = 100;
    int dims_array[max_dims];
    assert(dims <= max_dims);

    va_list args;
    va_start(args, dims);
    for(unsigned i = 0; i < dims; i++) {
        int d = va_arg(args, int);
        if (d < 0) {
            throw_java_exception("java/lang/NegativeArraySizeException");
        }
        dims_array[(dims - 1) - i] = d;
    }
    va_end(args);

    Vector_Handle arr = vm_multianewarray_recursive(cc, dims_array, dims);
    return arr;
} //vm_multianewarray_resolved



// end Java array allocation
/////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////
// Copy an Array
#ifdef VM_STATS
static void increment_array_copy_counter(uint64 &counter)
{
    counter ++;
}
#endif // VM_STATS

// This implementation may be Java specific

ArrayCopyResult array_copy(ManagedObject *src, int32 srcOffset, ManagedObject *dst, int32 dstOffset, int32 length)
{
    if (src == ((ManagedObject *)Class::managed_null) ||
        dst == ((ManagedObject *)Class::managed_null)) {
        return ACR_NullPointer;
    }

    Class *src_class = src->vt()->clss;
    assert(src_class);
    Class *dst_class = dst->vt()->clss;
    assert(dst_class);

    if (!(src_class->is_array && dst_class->is_array)) return ACR_TypeMismatch;
    assert(src_class->name->bytes[0] == '[');
    assert(dst_class->name->bytes[0] == '[');

    if (src_class->name != dst_class->name
        && (src_class->is_array_of_primitives || dst_class->is_array_of_primitives)) {
        return ACR_TypeMismatch;
    }

    if((srcOffset < 0) || (dstOffset < 0) || (length < 0)) return ACR_BadIndices;

    if(!length) return ACR_Okay;

    // We haven't determined at this point the types of arrays, but the
    // length is always at the same offset from the start of the array, so we
    // safely cast the pointers to any array type to get the lengths.
    int32 src_length = get_vector_length((Vector_Handle)src);
    int32 dst_length = get_vector_length((Vector_Handle)dst);
    if((srcOffset + length) > src_length || (dstOffset + length) > dst_length)
       return ACR_BadIndices;

    char src_elem_type = src_class->name->bytes[1];

    switch(src_elem_type) {
    case 'C': 
        {
            // 20030303 Use a C loop to (hopefully) speed up short array copies
            register uint16 *dst_addr = get_vector_element_address_uint16(dst, dstOffset);
            register uint16 *src_addr = get_vector_element_address_uint16(src, srcOffset);

#ifdef VM_STATS
            increment_array_copy_counter(vm_stats_total.num_arraycopy_char);
#endif // VM_STATS

            // 20030219 The length threshold 32 here works well for SPECjbb and should be reasonable for other applications.
            if (length < 32) {
                register int i;
                if (src_addr > dst_addr) {
                    for (i = length;  i > 0;  i--) {
                        *dst_addr++ = *src_addr++;
                    }
                } else {
                    // copy down, from higher address to lower
                    src_addr += length-1;
                    dst_addr += length-1;
                    for (i = length;  i > 0;  i--) {
                        *dst_addr-- = *src_addr--;
                    }
                }
            } else {
                memmove(dst_addr, src_addr, (length * sizeof(uint16)));
            }
        } 
        break;
    case 'B':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_byte);
#endif
        memmove(get_vector_element_address_int8(dst, dstOffset),
                get_vector_element_address_int8(src, srcOffset),
                length * sizeof(int8));
        break;
    case 'Z':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_bool);
#endif
        memmove(get_vector_element_address_bool(dst, dstOffset),
                get_vector_element_address_bool(src, srcOffset),
                length * sizeof(int8));
        break;
    case 'S':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_short);
#endif
        memmove(get_vector_element_address_int16(dst, dstOffset),
                get_vector_element_address_int16(src, srcOffset),
                length * sizeof(int16));
        break;
    case 'I':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_int);
#endif // VM_STATS
        memmove(get_vector_element_address_int32(dst, dstOffset),
                get_vector_element_address_int32(src, srcOffset),
                length * sizeof(int32));
        break;
    case 'J':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_long);
#endif // VM_STATS
        memmove(get_vector_element_address_int64(dst, dstOffset),
                get_vector_element_address_int64(src, srcOffset),
                length * sizeof(int64));
        break;
    case 'F':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_float);
#endif // VM_STATS
        memmove(get_vector_element_address_f32(dst, dstOffset),
                get_vector_element_address_f32(src, srcOffset),
                length * sizeof(float));
        break;
    case 'D':
#ifdef VM_STATS
        increment_array_copy_counter(vm_stats_total.num_arraycopy_double);
#endif // VM_STATS
        memmove(get_vector_element_address_f64(dst, dstOffset),
                get_vector_element_address_f64(src, srcOffset),
                length * sizeof(double));
        break;
    case 'L':
    case '[':
        {
#ifdef VM_STATS
            increment_array_copy_counter(vm_stats_total.num_arraycopy_object);
#endif // VM_STATS
            ManagedObject **src_body =
                (ManagedObject **)get_vector_element_address_ref(src, srcOffset);
            ManagedObject **dst_body =
                (ManagedObject **)get_vector_element_address_ref(dst, dstOffset);

            if(src_class == dst_class) {
                // If the types of arrays are the same, no type conflicts of array elements are possible.
#ifdef VM_STATS
                increment_array_copy_counter(vm_stats_total.num_arraycopy_object_same_type);
#endif // VM_STATS
                if (VM_Global_State::loader_env->compress_references) {
                    memmove(dst_body, src_body, length * sizeof(COMPRESSED_REFERENCE));
                } else {
                    memmove(dst_body, src_body, length * sizeof(RAW_REFERENCE));  // length pointers, each a ManagedObject*
                }
            } else {
                // If the types are different, the arrays are different and no overlap of the source and destination is possible.
#ifdef VM_STATS
                increment_array_copy_counter(vm_stats_total.num_arraycopy_object_different_type);
#endif // VM_STATS
                Class *dst_elem_clss = dst_class->array_element_class;
                assert(dst_elem_clss);
                
                if (VM_Global_State::loader_env->compress_references) {
                    COMPRESSED_REFERENCE *src_body_compressed = (COMPRESSED_REFERENCE *)src_body;
                    COMPRESSED_REFERENCE *dst_body_compressed = (COMPRESSED_REFERENCE *)dst_body;
                    for (int count = 0; count < length; count++) {
                        // For non-null elements check if types are compatible.
                        COMPRESSED_REFERENCE src_elem_offset = src_body_compressed[count];
                        if (src_elem_offset != 0) {
                            ManagedObject *src_elem = (ManagedObject *)uncompress_compressed_reference(src_elem_offset);
                            Class *src_elem_clss = src_elem->vt()->clss;
                            if (src_elem_clss == dst_elem_clss) {
                            } else if (!class_is_subtype_fast(src_elem_clss->vtable, dst_elem_clss)) {
                                // note: VM_STATS values are updated when class_is_subtype_fast() is called.
                                // Since we only flag the base do it before we throw exception
                                gc_heap_wrote_object(dst);
                                return ACR_TypeMismatch;
                            }
                        }
                        // If ArrayStoreException hasn't been thrown, copy the element.
                        dst_body_compressed[count] = src_body_compressed[count];
                        // There is not a gc_heap_write_ref call here since gc is disabled and we use gc_heap_wrote_object interface below.
                    }
                } else {
                     for (int count = 0; count < length; count++) {
                        // For non-null elements check if types are compatible.
                        if (src_body[count] != NULL) {
                            Class *src_elem_clss = src_body[count]->vt()->clss;
                            if (src_elem_clss == dst_elem_clss) {
                            } else if (!class_is_subtype_fast(src_elem_clss->vtable, dst_elem_clss)) {
                                // note: VM_STATS values are updated when class_is_subtype_fast() is called.
                                // Since we only flag the base do it before we throw exception
                                gc_heap_wrote_object(dst);
                                return ACR_TypeMismatch;
                            }
                        }
                        // If ArrayStoreException hasn't been thrown, copy the element.
                        dst_body[count] = src_body[count];
                        // There is not a gc_heap_write_ref call here since gc is disabled and we use gc_heap_wrote_object interface below.
                    }
                }
            }

            gc_heap_wrote_object(dst);
        }
        break;
    default:
        ABORT("Unexpected type specifier");
    }

    return ACR_Okay;
} //array_copy

