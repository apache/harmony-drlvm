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
 * @author Ivan Volosyuk
 */

#include <assert.h>
#include <vector>
#include <open/vm_gc.h>
#include <jni_types.h>
#include <apr_time.h>
#include "gc_types.h"
#include "collect.h"

extern fast_list<Partial_Reveal_Object*, 65536> objects; // FIXME: duplication of memory slots and objects
                                                  // FIXME: move to header file

static void forced_process_reference(Partial_Reveal_Object *obj, Boolean is_pinned);

static inline void 
forced_scan_array_object(Partial_Reveal_Object *array, int vector_length)
{
    // No primitive arrays allowed
    assert(!is_array_of_primitives(array));

    int32 array_length = vector_length; //vector_get_length((Vector_Handle) array);

    Partial_Reveal_Object **refs = (Partial_Reveal_Object**)
        vector_get_element_address_ref ((Vector_Handle) array, 0);

    for(int i = 0; i < array_length; i++) {
        Partial_Reveal_Object **ref = &refs[i];
        Partial_Reveal_Object *obj = *ref;
        if (obj != 0) {
            forced_process_reference(obj, false);
        }
    }
}

static void forced_process_reference(Partial_Reveal_Object *obj, Boolean is_pinned) {
    assert(!is_pinned);

    assert(obj->vt() & ~FORWARDING_BIT);

    int info = obj->obj_info();
    if (info & heap_mark_phase) {
        return;
    }

    obj->obj_info() = (info & ~MARK_BITS) | heap_mark_phase;

    Partial_Reveal_VTable *vtable = obj->vtable();
    GC_VTable_Info *gcvt = vtable->get_gcvt();

    if (gcvt->is_array()) { // is array
        int vector_length = obj->array_length();
        if (gcvt->has_slots())
            forced_scan_array_object(obj, vector_length);
        return;
    } else {
        if (!gcvt->has_slots()) return;
    }

    // process slots

    WeakReferenceType type = gcvt->reference_type();
    int *offset_list = gcvt->offset_array();

    // handling special references in objects.
    if (type != NOT_REFERENCE) {
        switch (type) {
            case SOFT_REFERENCE:
                add_soft_reference(obj);
                break;
            case WEAK_REFERENCE:
                add_weak_reference(obj);
                break;
            case PHANTOM_REFERENCE:
                add_phantom_reference(obj);
                break;
            default:
                TRACE2("gc.verbose", "Wrong reference type");
                break;
        }
    }

    int offset;
    while ((offset = *offset_list) != 0) {
        Partial_Reveal_Object **slot = (Partial_Reveal_Object**)(((char*)obj) + offset);
        offset_list++;
        Partial_Reveal_Object *object = *slot;
        if (object != 0) {
            objects.push_back(object);
        }
    }
}

static void gc_forced_add_root_set_entry_internal(Partial_Reveal_Object *obj, Boolean is_pinned) {
    forced_process_reference(obj, is_pinned);

    while (!objects.empty()) {
        Partial_Reveal_Object *obj = objects.pop_back();
        forced_process_reference(obj, false);
    }
}

void gc_forced_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned) {
    Partial_Reveal_Object *obj = *(Partial_Reveal_Object**)ref;
    if (obj == 0) return;
    gc_forced_add_root_set_entry_internal(obj, is_pinned);
}

void gc_forced_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned)
{
    int *ref = (int*)slot;
    int obj = *ref - offset;
    if (obj == 0) return;

    gc_forced_add_root_set_entry_internal((Partial_Reveal_Object*)obj, is_pinned);
}
