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
 * @version $Revision: 1.1.2.2.4.4 $
 */  



#define LOG_DOMAIN "vm.arrays"
#include "cxxlog.h"

#include <assert.h>

#include "environment.h"
#include "exceptions.h"
#include "object_handles.h"
#include "object_layout.h"
#include "vm_arrays.h"
#include "open/vm_util.h"

#include "vm_stats.h"
#include "port_threadunsafe.h"

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
    VM_Statistics::get_vm_stats().num_anewarray++;  
#endif
    ASSERT_RAISE_AREA;
    assert(!hythread_is_suspend_enabled());
    assert(!arr_clss->is_array_of_primitives());

    unsigned sz = arr_clss->calculate_array_size(length);
    if (sz == 0) {
        tmn_suspend_enable();

        if (length < 0) {
            exn_raise_by_name("java/lang/NegativeArraySizeException");
        } else {
            exn_raise_by_name("java/lang/OutOfMemoryError",
                    "VM doesn't support arrays of the requested size");
        }
        tmn_suspend_disable();
        return NULL;
    }

    assert((sz & NEXT_TO_HIGH_BIT_SET_MASK) == 0);

    Vector_Handle object_array = (Vector_Handle )gc_alloc(sz,
        arr_clss->get_allocation_handle(), vm_get_gc_thread_local());
#ifdef VM_STATS
    arr_clss->instance_allocated(sz);
#endif //VM_STATS

    if (NULL == object_array) {
        exn_raise_object(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return NULL;
    }

    set_vector_length(object_array, length);
    assert(get_vector_vtable(object_array) == arr_clss->get_vtable());
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
    ASSERT_RAISE_AREA;
    assert(!hythread_is_suspend_enabled());
    assert(vector_class->is_array_of_primitives());
    unsigned sz = vector_class->calculate_array_size(length);

    if (sz == 0) {
        tmn_suspend_enable();
        if (length < 0) {
            exn_raise_by_name("java/lang/NegativeArraySizeException");
        } else {
            exn_raise_by_name("java/lang/OutOfMemoryError",
                    "VM doesn't support arrays of the requested size");
        }
        tmn_suspend_disable();
        return NULL;
    }

    Vector_Handle vector = (Vector_Handle)gc_alloc(sz, 
        vector_class->get_allocation_handle(), vm_get_gc_thread_local());
#ifdef VM_STATS
    vector_class->instance_allocated(sz);
#endif //VM_STATS

    if (NULL == vector) {
        exn_raise_object(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return 0;
    }

    set_vector_length(vector, length);
    assert(get_vector_vtable(vector) == vector_class->get_vtable());
    return vector;
}

VMEXPORT // temporary solution for interpreter unplug
Vector_Handle vm_new_vector(Class *vector_class, int length)
{
    ASSERT_RAISE_AREA;
    Vector_Handle returned_vector;

    if(vector_class->is_array_of_primitives()) {
        returned_vector = vm_new_vector_primitive(vector_class,length);
    } else {
        returned_vector = vm_anewarray_resolved_array_type(vector_class, length);
    }

    return returned_vector;
}



// This function is deprecated.
// "Fast" version of vm_new_vector: returns NULL if a GC would be required for the allocation or no storage is available.
Vector_Handle vm_new_vector_or_null(Class * UNREF vector_class, int UNREF length)
{
    return NULL;
} //vm_new_vector_or_null



void vm_new_vector_update_stats(int length, Allocation_Handle vector_handle, void * UNREF tp)
{
#ifdef VM_STATS
    if (0 != (length&TWO_HIGHEST_BITS_SET_MASK)) return;
    VTable *vector_vtable = ManagedObject::allocation_handle_to_vtable(vector_handle);
    unsigned sz = vector_vtable->clss->calculate_array_size(length);
    assert(sz > 0);
    vector_vtable->clss->instance_allocated(sz);
    if((vector_vtable->class_properties & CL_PROP_NON_REF_ARRAY_MASK) == 0)
    {
        VM_Statistics::get_vm_stats().num_anewarray++;
    }
#endif //VM_STATS
}


Vector_Handle vm_new_vector_using_vtable_and_thread_pointer(int length, Allocation_Handle vector_handle, void *tp)
{
    assert( ! hythread_is_suspend_enabled());

    VTable *vector_vtable = ManagedObject::allocation_handle_to_vtable(vector_handle);
    unsigned sz = vector_vtable->clss->calculate_array_size(length);
    if (sz == 0) {
        tmn_suspend_enable();
        if (length < 0) {
            exn_raise_by_name("java/lang/NegativeArraySizeException");
        } else {
            exn_raise_by_name("java/lang/OutOfMemoryError",
                    "VM doesn't support arrays of the requested size");
        }
        tmn_suspend_disable();
        return NULL;
    }


#ifdef VM_STATSxxx // Functionality moved into vm_new_vector_update_stats().
    vector_vtable->clss->instance_allocated(sz);
#endif //VM_STATS
    assert( ! hythread_is_suspend_enabled());
    Vector_Handle vector = (Vector_Handle)gc_alloc(sz, vector_handle, tp);
    assert( ! hythread_is_suspend_enabled());
    
    if (NULL == vector) {
        exn_raise_object(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return NULL;
    }

    set_vector_length(vector, length);
    assert(get_vector_vtable(vector) == vector_vtable);
    return vector;
} //vm_new_vector_using_vtable_and_thread_pointer



Vector_Handle vm_new_vector_or_null_using_vtable_and_thread_pointer(int length, Allocation_Handle vector_handle, void *tp)
{
    VTable *vector_vtable = ManagedObject::allocation_handle_to_vtable(vector_handle);
    unsigned sz = vector_vtable->clss->calculate_array_size(length);

    if (sz == 0) {
        // Unsupported size
        return NULL;
    }
    Vector_Handle vector = (Vector_Handle)gc_alloc_fast(sz, vector_handle, tp);
    if (vector == NULL)
    {
        return NULL;
    }

    set_vector_length(vector, length);
    assert(get_vector_vtable(vector) == vector_vtable);
    return vector;
} //vm_new_vector_or_null_using_vtable_and_thread_pointer



Vector_Handle
vm_multianewarray_recursive(Class    *c,
                            int      *dims_array,
                            unsigned  dims)
{
    ASSERT_RAISE_AREA;
    assert(!hythread_is_suspend_enabled());
    Global_Env *global_env = VM_Global_State::loader_env;

    int *pos = (int*) STD_ALLOCA(sizeof(int) * dims);
    Class **clss = (Class**) STD_ALLOCA(sizeof(Class*) * dims);
    ObjectHandle *obj = (ObjectHandle*) STD_ALLOCA(sizeof(ObjectHandle) * dims);

    unsigned max_depth = dims - 1;
    unsigned d;

    // check dimensions phase
    for(d = 0; d < dims; d++) {
        pos[d] = 0;
        int len = dims_array[d];
        if (len < 0) {
            // The function uses gc allocation, so it is not gc safe anyway
            tmn_suspend_enable();
            exn_raise_by_name("java/lang/NegativeArraySizeException");
            tmn_suspend_disable();
            return 0;
        }

        if (len & TWO_HIGHEST_BITS_SET_MASK) {
            // The function uses gc allocation, so it is not gc safe anyway
            tmn_suspend_enable();
            exn_raise_by_name("java/lang/OutOfMemoryError",
                    "VM doesn't support arrays of the requested size");
            tmn_suspend_disable();
            return 0;
        }

        if (len == 0) {
            if (d < max_depth) max_depth = d;
        }
        obj[d] = oh_allocate_local_handle();
    }

    // init Class* array
    clss[0] = c;
    assert(c->get_name()->bytes[0] == '[');
    assert(c->get_name()->len > 1);

    for(d = 1; d < dims; d++) {
        c = c->get_array_element_class();
        assert(c->get_name()->bytes[0] == '[');
        assert(c->get_name()->len > 1);
        clss[d] = c;
    }

    // init root element
    ManagedObject* array = obj[0]->object = (ManagedObject*) vm_new_vector(clss[0], dims_array[0]);
    if (!array) {
        assert(exn_raised());
        return 0;
    }
    if (max_depth == 0) return array;

    d = 1;
    // allocation dimensions
    while(true) {
        ManagedObject *element = (ManagedObject*) vm_new_vector(clss[d], dims_array[d]);
        if (!element) {
            assert(exn_raised());
            // OutOfMemoryError occured
            return 0;
        }

        if (d != max_depth) {
            obj[d]->object = element;
            d++;
            continue;
        }

        while(true) {
            ManagedObject *subarray = obj[d - 1]->object;
            ManagedObject **slot = get_vector_element_address_ref(subarray, pos[d-1]);
            STORE_REFERENCE(subarray, slot, element);
            pos[d-1]++;

            if (pos[d-1] < dims_array[d-1]) {
                break;
            }

            pos[d-1] = 0;
            element = subarray;
            d--;

            if (d == 0) return obj[0]->object;
        }
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
ASSERT_THROW_AREA;
#ifdef VM_STATS
    VM_Statistics::get_vm_stats().num_multianewarray++;  
#endif
    assert(!hythread_is_suspend_enabled());

    const unsigned max_dims = 100;
    int dims_array[max_dims];
    assert(dims <= max_dims);

    va_list args;
    va_start(args, dims);
    for(unsigned i = 0; i < dims; i++) {
        int d = va_arg(args, int);
        if (d < 0) {
            exn_throw_by_name("java/lang/NegativeArraySizeException");
        }
        dims_array[(dims - 1) - i] = d;
    }
    va_end(args);

    Vector_Handle arr;
    BEGIN_RAISE_AREA;
    arr = vm_multianewarray_recursive(cc, dims_array, dims);
    END_RAISE_AREA;
    exn_rethrow_if_pending();
    return arr;
} //vm_multianewarray_resolved



// end Java array allocation
/////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////
// Copy an Array
#ifdef VM_STATS
static void increment_array_copy_counter(uint64 &counter)
{
    UNSAFE_REGION_START
    counter ++;
    UNSAFE_REGION_END
}

#endif // VM_STATS

// This implementation may be Java specific

ArrayCopyResult array_copy(ManagedObject *src, int32 srcOffset, ManagedObject *dst, int32 dstOffset, int32 length)
{
    if (src == ((ManagedObject *)VM_Global_State::loader_env->managed_null) ||
        dst == ((ManagedObject *)VM_Global_State::loader_env->managed_null)) {
        return ACR_NullPointer;
    }

    Class *src_class = src->vt()->clss;
    assert(src_class);
    Class *dst_class = dst->vt()->clss;
    assert(dst_class);

    if (!(src_class->is_array() && dst_class->is_array())) return ACR_TypeMismatch;
    assert(src_class->get_name()->bytes[0] == '[');
    assert(dst_class->get_name()->bytes[0] == '[');

    if (src_class->get_name() != dst_class->get_name()
        && (src_class->is_array_of_primitives() || dst_class->is_array_of_primitives())) {
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

    char src_elem_type = src_class->get_name()->bytes[1];

    switch(src_elem_type) {
    case 'C': 
        {
            // 20030303 Use a C loop to (hopefully) speed up short array copies
            register uint16 *dst_addr = get_vector_element_address_uint16(dst, dstOffset);
            register uint16 *src_addr = get_vector_element_address_uint16(src, srcOffset);

#ifdef VM_STATS
            increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_char);
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
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_byte);
#endif
        memmove(get_vector_element_address_int8(dst, dstOffset),
                get_vector_element_address_int8(src, srcOffset),
                length * sizeof(int8));
        break;
    case 'Z':
#ifdef VM_STATS
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_bool);
#endif
        memmove(get_vector_element_address_bool(dst, dstOffset),
                get_vector_element_address_bool(src, srcOffset),
                length * sizeof(int8));
        break;
    case 'S':
#ifdef VM_STATS
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_short);
#endif
        memmove(get_vector_element_address_int16(dst, dstOffset),
                get_vector_element_address_int16(src, srcOffset),
                length * sizeof(int16));
        break;
    case 'I':
#ifdef VM_STATS
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_int);
#endif // VM_STATS
        memmove(get_vector_element_address_int32(dst, dstOffset),
                get_vector_element_address_int32(src, srcOffset),
                length * sizeof(int32));
        break;
    case 'J':
#ifdef VM_STATS
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_long);
#endif // VM_STATS
        memmove(get_vector_element_address_int64(dst, dstOffset),
                get_vector_element_address_int64(src, srcOffset),
                length * sizeof(int64));
        break;
    case 'F':
#ifdef VM_STATS
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_float);
#endif // VM_STATS
        memmove(get_vector_element_address_f32(dst, dstOffset),
                get_vector_element_address_f32(src, srcOffset),
                length * sizeof(float));
        break;
    case 'D':
#ifdef VM_STATS
        increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_double);
#endif // VM_STATS
        memmove(get_vector_element_address_f64(dst, dstOffset),
                get_vector_element_address_f64(src, srcOffset),
                length * sizeof(double));
        break;
    case 'L':
    case '[':
        {
#ifdef VM_STATS
            increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_object);
#endif // VM_STATS
            ManagedObject **src_body =
                (ManagedObject **)get_vector_element_address_ref(src, srcOffset);
            ManagedObject **dst_body =
                (ManagedObject **)get_vector_element_address_ref(dst, dstOffset);

            if(src_class == dst_class) {
                // If the types of arrays are the same, no type conflicts of array elements are possible.
#ifdef VM_STATS
                increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_object_same_type);
#endif // VM_STATS
                if (VM_Global_State::loader_env->compress_references) {
                    memmove(dst_body, src_body, length * sizeof(COMPRESSED_REFERENCE));
                } else {
                    memmove(dst_body, src_body, length * sizeof(RAW_REFERENCE));  // length pointers, each a ManagedObject*
                }
            } else {
                // If the types are different, the arrays are different and no overlap of the source and destination is possible.
#ifdef VM_STATS
                increment_array_copy_counter(VM_Statistics::get_vm_stats().num_arraycopy_object_different_type);
#endif // VM_STATS
                Class* dst_elem_clss = dst_class->get_array_element_class();
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
                            } else if (!src_elem_clss->is_instanceof(dst_elem_clss)) {
                                // note: VM_STATS values are updated when Class::is_instanceof() is called.
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
                            } else if (!src_elem_clss->is_instanceof(dst_elem_clss)) {
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

