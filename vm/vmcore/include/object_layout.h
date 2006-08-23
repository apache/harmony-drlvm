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
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#ifndef _OBJECT_LAYOUT_H_
#define _OBJECT_LAYOUT_H_

// Define USE_COMPRESSED_VTABLE_POINTERS here to explicitly enable compressed vtable
// pointers within objects.
// Define USE_UNCOMPRESSED_VTABLE_POINTERS to explicitly disable them.
// Leave both undefined to make it command-line switchable.
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


//
// Built-in types
//

// Note: if the header layout of the object changes, VM_VECTOR_RT_OVERHEAD must changed
// below as well.
typedef struct ManagedObjectCompressedVtablePtr {
    uint32 vt_offset;
    uint32 obj_info;
    VTable *vt_unsafe() { return (VTable *) (vt_offset + vm_get_vtable_base()); }
    VTable *vt() { assert(vt_offset); return vt_unsafe(); }
    uint32 get_obj_info() { return obj_info; }
    void set_obj_info(uint32 value) { obj_info = value; }
    static VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return (VTable *) ((POINTER_SIZE_INT)ah + vm_get_vtable_base());
    }
    static unsigned header_offset() { return sizeof(uint32); }
    static size_t get_size() { return sizeof(ManagedObjectCompressedVtablePtr); }
    static bool are_vtable_pointers_compressed() { return TRUE; }
} ManagedObjectCompressedVtablePtr;

typedef struct ManagedObjectUncompressedVtablePtr {
    VTable *vt_raw;
    POINTER_SIZE_INT obj_info;
    VTable *vt_unsafe() { return vt_raw; }
    VTable *vt() { assert(vt_raw); return vt_unsafe(); }
    uint32 get_obj_info() { return (uint32) obj_info; }
    void set_obj_info(uint32 value) { obj_info = (uint32) value; }
    static VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return (VTable *) ah;
    }
    static unsigned header_offset() { return sizeof(VTable *); }
    static size_t get_size() { return sizeof(ManagedObjectUncompressedVtablePtr); }
    static bool are_vtable_pointers_compressed() { return FALSE; }
} ManagedObjectUncompressedVtablePtr;

typedef struct ManagedObject {
    VTable *vt_unsafe() {
        if (are_vtable_pointers_compressed())
            return ((ManagedObjectCompressedVtablePtr *)this)->vt_unsafe();
        else
            return ((ManagedObjectUncompressedVtablePtr *)this)->vt_unsafe();
    }
    VTable *vt() {
        assert(!hythread_is_suspend_enabled());
        if (are_vtable_pointers_compressed())
            return ((ManagedObjectCompressedVtablePtr *)this)->vt();
        else
            return ((ManagedObjectUncompressedVtablePtr *)this)->vt();
    }
    uint32 get_obj_info() {
        assert(!hythread_is_suspend_enabled());
        if (are_vtable_pointers_compressed())
            return ((ManagedObjectCompressedVtablePtr *)this)->get_obj_info();
        else
            return ((ManagedObjectUncompressedVtablePtr *)this)->get_obj_info();
    }
    uint32 *get_obj_info_addr() { return (uint32 *)((char *)this + header_offset()); }

    void set_obj_info(uint32 value) {
        assert(!hythread_is_suspend_enabled());
        if (are_vtable_pointers_compressed())
            ((ManagedObjectCompressedVtablePtr *)this)->set_obj_info(value);
        else
            ((ManagedObjectUncompressedVtablePtr *)this)->set_obj_info(value);
    }
    static VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return are_vtable_pointers_compressed() ?
            ManagedObjectCompressedVtablePtr::allocation_handle_to_vtable(ah) : ManagedObjectUncompressedVtablePtr::allocation_handle_to_vtable(ah);
    }
    static unsigned header_offset() {
        return are_vtable_pointers_compressed() ?
            ManagedObjectCompressedVtablePtr::header_offset() : ManagedObjectUncompressedVtablePtr::header_offset();
    }
    static size_t get_size() {
        return are_vtable_pointers_compressed() ?
            ManagedObjectCompressedVtablePtr::get_size() : ManagedObjectUncompressedVtablePtr::get_size();
    }
    static bool are_vtable_pointers_compressed() {
#ifdef USE_COMPRESSED_VTABLE_POINTERS
        return true;
#else // !USE_COMPRESSED_VTABLE_POINTERS
#ifdef USE_UNCOMPRESSED_VTABLE_POINTERS
        return false;
#else // !USE_UNCOMPRESSED_VTABLE_POINTERS
        return _compressed;
#endif // !USE_UNCOMPRESSED_VTABLE_POINTERS
#endif // !USE_COMPRESSED_VTABLE_POINTERS
    }
    static bool _compressed;
} ManagedObject;




// See gc_for_vm, any change here needs to be reflected in Partial_Reveal_JavaArray.
typedef struct VM_Vector
{
    // The length field comes immediately after the ManagedObject fields.
    static size_t length_offset() { return ManagedObject::get_size(); }
    int32 get_length() { return *(int32 *)((char *)this + length_offset()); }
    void set_length(int32 len) { *(int32 *)((char *)this + length_offset()) = len; }
} VM_Vector;




//////////////////////////////////////////////////////////
// Field access functions follow

ManagedObject *get_java_lang_throwable_field_trace(Java_java_lang_Throwable *thr);
void set_java_lang_throwable_field_trace(Java_java_lang_Throwable *thr, ManagedObject *trace);


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

