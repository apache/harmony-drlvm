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
#include <vector>
#include <open/vm_gc.h>
#include <jni_types.h>
#include <apr_time.h>
#include "gc_types.h"
#include "collect.h"

extern fast_list<Partial_Reveal_Object*, 65536> objects; // FIXME: duplication of memory slots and objects
                                                  // FIXME: move to header file

static void forced_process_reference(Partial_Reveal_Object *obj);

static inline void 
forced_scan_array_object(Partial_Reveal_Object *array, int vector_length)
{
    // No primitive arrays allowed
    assert(!is_array_of_primitives(array));

    int32 array_length = vector_length; //vector_get_length((Vector_Handle) array);

    Reference *refs = (Reference*)
        vector_get_element_address_ref ((Vector_Handle) array, 0);

    for(int i = 0; i < array_length; i++) {
        Slot slot(refs + i);
        Partial_Reveal_Object *obj = slot.read();
        if (obj != heap_null) {
            forced_process_reference(obj);
        }
    }
}

static void forced_process_reference(Partial_Reveal_Object *obj) {

    assert(obj->vt() & ~FORWARDING_BIT);

    unsigned info = obj->obj_info();
    if (info & heap_mark_phase) {
        return;
    }
    obj->valid();

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
        Slot slot( (Reference*)(((char*)obj) + offset) );
        offset_list++;
        Partial_Reveal_Object *object = slot.read();
        if (object != heap_null) {
            objects.push_back(object);
        }
    }
}

void gc_forced_add_root_set_entry(Slot slot) {
    Partial_Reveal_Object *obj = slot.read();
    if (obj == heap_null) return;
    forced_process_reference(obj);

    while (!objects.empty()) {
        Partial_Reveal_Object *obj = objects.pop_back();
        forced_process_reference(obj);
    }
}

