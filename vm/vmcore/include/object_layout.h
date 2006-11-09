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

#ifndef _OBJECT_LAYOUT_H_
#define _OBJECT_LAYOUT_H_

// Define USE_COMPRESSED_VTABLE_POINTERS here to explicitly enable compressed vtable
// pointers within objects.
// Define USE_UNCOMPRESSED_VTABLE_POINTERS to explicitly disable them.
#if defined _IPF_ || defined _EM64T_
#define USE_COMPRESSED_VTABLE_POINTERS
#else // !_IPF_
#define USE_UNCOMPRESSED_VTABLE_POINTERS
#endif // _IPF_

#include <assert.h>
#include "open/types.h"
#include "open/hythread_ext.h"
#include "jni.h"
#include "open/vm.h"

typedef struct VTable VTable;

#ifdef __cplusplus
extern "C" {
#endif

/// Raw and compressed reference pointers

typedef ManagedObject*  RAW_REFERENCE;
typedef uint32          COMPRESSED_REFERENCE;

VMEXPORT bool is_compressed_reference(COMPRESSED_REFERENCE value);
VMEXPORT bool is_null_compressed_reference(COMPRESSED_REFERENCE value);

VMEXPORT COMPRESSED_REFERENCE compress_reference(ManagedObject *obj);
VMEXPORT ManagedObject* uncompress_compressed_reference(COMPRESSED_REFERENCE compressed_ref);
VMEXPORT ManagedObject* get_raw_reference_pointer(ManagedObject **slot_addr);

// Store the reference "VALUE" in the slot at address "SLOT_ADDR"
// in the object "CONTAINING_OBJECT".
// Signature: void store_reference(ManagedObject* CONTAINING_OBJECT,
//                                 ManagedObject** SLOT_ADDR,
//                                 ManagedObject* VALUE);
#define STORE_REFERENCE(CONTAINING_OBJECT, SLOT_ADDR, VALUE)                                  \
    {                                                                                         \
        if (VM_Global_State::loader_env->compress_references) {                               \
            gc_heap_slot_write_ref_compressed((Managed_Object_Handle)(CONTAINING_OBJECT),     \
                                              (uint32*)(SLOT_ADDR),                           \
                                              (Managed_Object_Handle)(VALUE));                \
        } else {                                                                              \
            gc_heap_slot_write_ref((Managed_Object_Handle)(CONTAINING_OBJECT),                \
                                   (Managed_Object_Handle*)(SLOT_ADDR),                      \
                                   (Managed_Object_Handle)(VALUE));                           \
        }                                                                                     \
    }

// Store the reference "VALUE" in the static field or
// other global slot at address "SLOT_ADDR".
// Signature: void store_global_reference(COMPRESSED_REFERENCE* SLOT_ADDR,
//                                        ManagedObject* VALUE);
#define STORE_GLOBAL_REFERENCE(SLOT_ADDR, VALUE)                              \
    {                                                                         \
        if (VM_Global_State::loader_env->compress_references) {               \
            gc_heap_write_global_slot_compressed((uint32*)(SLOT_ADDR),        \
                (Managed_Object_Handle)(VALUE));                              \
        } else {                                                              \
            gc_heap_write_global_slot((Managed_Object_Handle*)(SLOT_ADDR),    \
                                      (Managed_Object_Handle)(VALUE));        \
        }                                                                     \
    }


// The object layout is currently as follows
//
//    * VTable* / uint32 vt_offset  +
//  .-* uint32 lockword             +- get_constant_header_size()
//  |   [int32 array lenth]
//  | * [void* tag pointer]
//  |   [padding]
//  |   fields / array elements
//  |
//  `- get_size()
//
// tag pointer field is present iff ManagedObject::_tag_pointer is true
// length field is present in array objects

typedef struct ManagedObject {
#if defined USE_COMPRESSED_VTABLE_POINTERS
    uint32 vt_offset;
    uint32 obj_info;
    VTable *vt_unsafe() { return (VTable*)(vt_offset + vm_get_vtable_base()); }
    VTable *vt() { assert(vt_offset); return vt_unsafe(); }
    static VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return (VTable *) ((POINTER_SIZE_INT)ah + vm_get_vtable_base());
    }
    static unsigned header_offset() { return sizeof(uint32); }
    static bool are_vtable_pointers_compressed() { return true; }
#elif defined USE_UNCOMPRESSED_VTABLE_POINTERS
    VTable *vt_raw;
    POINTER_SIZE_INT obj_info;
    VTable *vt_unsafe() { return vt_raw; }
    VTable *vt() { assert(vt_raw); return vt_unsafe(); }
    static VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return (VTable *) ah;
    }
    static unsigned header_offset() { return sizeof(VTable *); }
    static bool are_vtable_pointers_compressed() { return false; }
#endif
    /// returns the size of constant object header part (vt pointer and obj_info)
    static size_t get_constant_header_size() { return sizeof(ManagedObject); }
    /// returns the size of object header including dynamically enabled fields.
    static size_t get_size() { 
        return get_constant_header_size() + (_tag_pointer ? sizeof(void*) : 0); 
    }

    uint32 get_obj_info() { return (uint32)obj_info; }
    void set_obj_info(uint32 value) { obj_info = value; }
    uint32 *get_obj_info_addr() { return (uint32 *)((char *)this + header_offset()); }

    /**
     * returns address of a tag pointer field in a _non-array_ object.
     * For array objects use VM_Vector::get_tag_pointer_address().
     */
    void** get_tag_pointer_address() {
        assert(_tag_pointer);
        return (void**)((char*)this + get_constant_header_size());
    }

    VMEXPORT static bool _tag_pointer;
} ManagedObject;



// See gc_for_vm, any change here needs to be reflected in Partial_Reveal_JavaArray.
typedef struct VM_Vector
{
    ManagedObject object;
    int32 length;

    static size_t length_offset() { return (size_t)(&((VM_Vector*)NULL)->length); }
    int32 get_length() { return length; }
    void set_length(int32 len) { length = len; }

    void** get_tag_pointer_address() {
        assert(ManagedObject::_tag_pointer);
        int offset = sizeof(VM_Vector);
        return (void**)((char*)this + offset);
    }

} VM_Vector;




// Every vector has two pointers that can be found in any VM object
// and an int32 field to hold the length (i.e., the number of elements).
// The combined size of those fields is:
#define VM_VECTOR_RT_OVERHEAD ((unsigned)(ManagedObject::get_size() + sizeof(int32)))


// The offset to the first element of a vector with 8-byte elements.
#define VM_VECTOR_FIRST_ELEM_OFFSET_8 ((VM_VECTOR_RT_OVERHEAD + 7) & ~7)

// The offset to the first element of a vector with 1, 2, or 4-byte elements.
// After the code in CVS gets stable, change this to be
// the same on IPF and IA-32.
#ifdef POINTER64
#define VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4 VM_VECTOR_FIRST_ELEM_OFFSET_8
#else
#define VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4 VM_VECTOR_RT_OVERHEAD
#endif

// References are either 4 or 8-byte wide.
#ifdef POINTER64
#define VM_VECTOR_FIRST_ELEM_OFFSET_REF VM_VECTOR_FIRST_ELEM_OFFSET_8
#else
#define VM_VECTOR_FIRST_ELEM_OFFSET_REF VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4
#endif




#ifdef __cplusplus
}
#endif

#endif // _OBJECT_LAYOUT_H_

