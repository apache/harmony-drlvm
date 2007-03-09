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
 * @author Ivan Volosyuk
 */

#include <assert.h>
#include <open/vm_gc.h>
#include <open/vm.h>
#include <open/gc.h>
#include <port_malloc.h>
#include <tl/memory_pool.h>
#include "gc_types.h"

tl::MemoryPoolMT *gcvt_pool;
int global_referent_offset = 0;

void init_gcvt() {
    gcvt_pool = new tl::MemoryPoolMT();
}

void deinit_gcvt() {
    delete gcvt_pool;
}

// A comparison function for qsort().
static int 
intcompare(const void *vi, const void *vj)
{
    const int *i = (const int *) vi;
    const int *j = (const int *) vj;
    if (*i > *j)
        return 1;
    if (*i < *j)
        return -1;
    return 0;
}

static GC_VTable_Info* build_slot_offset_array(Class_Handle ch, Partial_Reveal_VTable *vt, WeakReferenceType type) 
{
    GC_VTable_Info *result = NULL;

    unsigned num_ref_fields = 0;
    //
    // Careful this doesn't give you the number of instance fields.
    // Calculate the size needed for the offset table.
    //
    unsigned num_fields = class_num_instance_fields_recursive(ch);

    unsigned idx;
    for(idx = 0; idx < num_fields; idx++) {
        Field_Handle fh = class_get_instance_field_recursive(ch, idx);
		if(field_is_enumerable_reference(fh)) {
            num_ref_fields++;
        }
    }

    int skip = -1; // not skip any reference
    if (type != NOT_REFERENCE) {
        int offset = class_get_referent_offset(ch);
        if (global_referent_offset == 0) {
            global_referent_offset = offset;
        } else {
            assert(global_referent_offset == offset);
        }

        skip = global_referent_offset; // skip global referent offset
        num_ref_fields--;
    }

    // We need room for the terminating 0 so add 1.
    unsigned int size = (num_ref_fields+1) * sizeof (unsigned int) + sizeof(GC_VTable_Info);

    // malloc up the array if we need one.
    result = (GC_VTable_Info*) gcvt_pool->alloc(size);

    int *new_ref_array = (int *) (result + 1);
    int *refs = new_ref_array;

    for(idx = 0; idx < num_fields; idx++) {
        Field_Handle fh = class_get_instance_field_recursive(ch, idx);
		if(field_is_enumerable_reference(fh)){
            int offset = field_get_offset(fh);
            if (offset == skip) continue;
            *refs = offset;
            refs++;
        }
    }

    // It is 0 delimited.
    *refs = 0;

    // The VM doesn't necessarily report the reference fields in
    // memory order, so we sort the slot offset array.  The sorting
    // is required by the verify_live_heap code.
    qsort(new_ref_array, num_ref_fields, sizeof(*result), intcompare);
    return result;
}


void gc_class_prepared(Class_Handle ch, VTable_Handle vth) {
    TRACE2("gc.init", "gc_class_prepared " << class_get_name(ch));
    assert(ch);
    assert(vth);
    Partial_Reveal_VTable *vt = (Partial_Reveal_VTable *)vth;


    if (class_is_array(ch)) {
        int el_size = class_element_size(ch);
        int el_offset;
        for(el_offset = -1; el_size; el_size >>= 1, el_offset++);

        int first_element = vector_first_element_offset_unboxed(
                class_get_array_element_class(ch));

        TRACE2("gc.init.size", "first array element offset " << first_element);

        POINTER_SIZE_INT flags = GC_VT_ARRAY
            | (el_offset << GC_VT_ARRAY_ELEMENT_SHIFT)
            | (first_element << GC_VT_ARRAY_FIRST_SHIFT);

        if (!class_is_non_ref_array(ch)) {
            flags |= GC_VT_HAS_SLOTS;
        }
        
        GC_VTable_Info *info = (GC_VTable_Info*) flags;
        vt->set_gcvt(info);
        return;
    }

    WeakReferenceType type = class_is_reference(ch);
    GC_VTable_Info *info = build_slot_offset_array(ch, vt, type);
    info->size_and_ref_type = class_get_boxed_data_size(ch) | (int)type;

    POINTER_SIZE_INT flags = 0;
    if (!ignore_finalizers && class_is_finalizable(ch)) {
        flags |= GC_VT_FINALIZIBLE;
    }

    int *offset_array = (int*)(info + 1);
    if (type != NOT_REFERENCE || (*offset_array != 0)) {
        flags |= GC_VT_HAS_SLOTS;
    }

    POINTER_SIZE_INT addr = (POINTER_SIZE_INT) info;
    assert((addr & 7) == 0); // required alignment

    flags |= addr;
    vt->set_gcvt((GC_VTable_Info*) flags);
}

