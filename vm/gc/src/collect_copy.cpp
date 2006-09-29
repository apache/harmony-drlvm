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
#include <algorithm>
#include <open/vm_gc.h>
#include <apr_time.h>
#include <jni_types.h>
#include "gc_types.h"
#include "collect.h"

void gc_copy_update_regions() {
    int n = 0;
    for(fast_list<unsigned char*, 1024>::iterator it = pinned_areas_unsorted.begin();
            it != pinned_areas_unsorted.end(); ++it) {
        TRACE2("gc.pin", "pinned area unsorted[" << n << "] = 0x" << *it);
        n++;
    }
    
    pinned_areas.resize(pinned_areas_unsorted.count());
    partial_sort_copy(pinned_areas_unsorted.begin(), pinned_areas_unsorted.end(), pinned_areas.begin(), pinned_areas.end());

    for(unsigned i = 0; i < pinned_areas.size(); i++) {
        TRACE2("gc.pin", "pinned area [" << i << "] = 0x" << pinned_areas[i]);
    }

    heap.pos = heap.compaction_region_start();
    heap.pos_limit = pinned_areas.empty() ? heap.compaction_region_end() : (unsigned char*) pinned_areas[0];

    assert(heap.pos >= heap.old_objects.end);
    assert(heap.old_objects.pos_limit <= heap.old_objects.end);
    assert(heap.old_objects.pos <= heap.old_objects.end);
    assert(heap.pos <= heap.pos_limit);
    pinned_areas_pos = 1;
    cleaning_needed = true;
}

static bool gc_copy_process_reference(Partial_Reveal_Object **ref, Boolean is_pinned, int phase);

static inline bool 
gc_copy_scan_array_object(Partial_Reveal_Object *array, int vector_length, int phase)
{
    assert(!is_array_of_primitives(array));

    int32 array_length = vector_length; //vector_get_length((Vector_Handle) array);

    Partial_Reveal_Object **refs = (Partial_Reveal_Object**)
        vector_get_element_address_ref ((Vector_Handle) array, 0);

    for(int i = 0; i < array_length; i++) {
        Partial_Reveal_Object **ref = &refs[i];

        bool success = gc_copy_process_reference(ref, false, phase);

        if (!success) {
            // overflow in old objects
            // continue transition to sliding compaction
            gc_slide_process_transitional_slots(refs, i + 1, array_length);
            return false;
        }
    }
    return true;
}

bool place_into_old_objects(unsigned char *&newpos,
                                          unsigned char *&endpos,
                                          int size) {
    newpos = heap.old_objects.pos;
    endpos = newpos + size;

    assert(heap.old_objects.pos_limit <= heap.old_objects.end);

    if (endpos <= heap.old_objects.pos_limit) {
        heap.old_objects.pos = endpos;
        assert(endpos <= heap.old_objects.end);
        return true;
    }
    TRACE2("gc.pin.gc", "old area: reached heap.old_objects.pos_limit =" << heap.old_objects.pos_limit);



    // object doesn't feet in old objects region, skip possibly
    // pinned objects in old objects region
    // FIXME: can add 'heap.old_objects.end' to the vector
    //        one less check
    while(old_pinned_areas_pos < old_pinned_areas.size()) {
        assert(heap.old_objects.pos <= old_pinned_areas[old_pinned_areas_pos]);

        heap.old_objects.pos = old_pinned_areas[old_pinned_areas_pos];

        if (old_pinned_areas_pos + 1 < old_pinned_areas.size()) {
            // more pinned objects exists
            unsigned char *new_limit = old_pinned_areas[old_pinned_areas_pos + 1];
            if (new_limit > heap.old_objects.end) {
                TRACE2("gc.pin.gc", "old area: new limit is greater then heap.old_objects.end = "
                        << new_limit);
                heap.old_objects.pos = heap.old_objects.pos_limit;
                old_pinned_areas_pos += 2;
                return false;
            }
            heap.old_objects.pos_limit = new_limit;
        } else {
            heap.old_objects.pos_limit = heap.old_objects.end;
        }
        old_pinned_areas_pos += 2;
        TRACE2("gc.pin.gc", "old area: heap.old_objects.pos =" << heap.old_objects.pos);
        TRACE2("gc.pin.gc", "old area: heap.old_objects.pos_limit =" << heap.old_objects.pos_limit);

        newpos = heap.old_objects.pos;
        endpos = newpos + size;
        if (endpos <= heap.old_objects.pos_limit) {
            heap.old_objects.pos = endpos;
            return true;
        }
    }
    return false;
}

static bool gc_copy_process_reference(Partial_Reveal_Object **ref, Boolean is_pinned, int phase) {
    assert(ref);
 
    Partial_Reveal_Object* obj = *ref;

    if (!obj) return true;
    assert(obj->vt() & ~(FORWARDING_BIT|RESCAN_BIT));
    TRACE2("gc.debug", "0x" << obj << " info = " << obj->obj_info());

    int info = obj->obj_info();
    int vt = obj->vt();

    if (info & phase) {
        // object already marked, need to check if it is forwared still
        
        if (vt & FORWARDING_BIT) {
            Partial_Reveal_Object *newpos = (Partial_Reveal_Object*) (vt & ~FORWARDING_BIT);
            assert_vt(newpos);
            *ref = newpos;
        }
        return true;
    }

    VMEXPORT Class_Handle vtable_get_class(VTable_Handle vh);
    assert(class_get_vtable(vtable_get_class((VTable_Handle)obj->vt())) == (VTable_Handle)obj->vt());
    TRACE2("gc.debug", "0x" << obj << " is " << class_get_name(vtable_get_class((VTable_Handle)obj->vt())));

    obj->obj_info() = (info & ~MARK_BITS) | phase;

    // move the object?
#define pos ((unsigned char*) obj)
    Partial_Reveal_VTable *vtable = (Partial_Reveal_VTable*) vt;
    GC_VTable_Info *gcvt = vtable->get_gcvt();

    if (pos >= heap.compaction_region_start() && pos < heap.compaction_region_end()) {
        int size = get_object_size(obj, gcvt);

        // is it not pinned?
        if (size < 5000 &&  (!is_pinned) && ((info & OBJECT_IS_PINNED_BITS) == 0)) {
            if (info & HASHCODE_IS_SET_BIT) {
                size += 4;
            }

            // move the object
            unsigned char *newpos, *endpos;
            if (place_into_old_objects(newpos, endpos, size)) {

                TRACE2("gc.debug", "move 0x" << obj << " to 0x" << newpos << " info = " << obj->obj_info());

                Partial_Reveal_Object *newobj = (Partial_Reveal_Object*) newpos;
                if ((info & HASHCODE_IS_SET_BIT) && !(info & HASHCODE_IS_ALLOCATED_BIT)) {
                    memcpy(newobj, obj, size-4);
                    *(int*)(newpos + size-4) = gen_hashcode(obj);
                    newobj->obj_info() |= HASHCODE_IS_ALLOCATED_BIT;
                } else {
                    memcpy(newobj, obj, size);
                }
                //TRACE2("gc.copy", "obj " << obj << " -> " << newobj << " + " << size);
                assert(newobj->vt() == obj->vt());
                assert(newobj->obj_info() & phase);
                obj->vt() = (POINTER_SIZE_INT)newobj | FORWARDING_BIT;
                assert_vt(newobj);
                *ref = newobj;
                obj = newobj;
            } else {
                // overflow! no more space in old objects area
                // pinning the overflow object
                pinned_areas_unsorted.push_back(pos);
                pinned_areas_unsorted.push_back(pos + size
                        + ((obj->obj_info() & HASHCODE_IS_ALLOCATED_BIT) ? 4 : 0));
                TRACE2("gc.pin", "add failed pinned area = " << pos << " " << pinned_areas_unsorted.back());
                TRACE2("gc.pin", "failed object = " << pos);
                // arange transition to slide compaction
                obj->obj_info() &= ~MARK_BITS;
                slots.push_back(ref);
                transition_copy_to_sliding_compaction(slots);
                return false;
            }
        } else {
            TRACE2("gc.debug", "pinned 0x" << obj);
            assert(gc_num != 1 || !(obj->obj_info() & HASHCODE_IS_ALLOCATED_BIT));
            pinned_areas_unsorted.push_back(pos);
            pinned_areas_unsorted.push_back(pos + size
                    + ((obj->obj_info() & HASHCODE_IS_ALLOCATED_BIT) ? 4 : 0));
            TRACE2("gc.pin", "add pinned area = " << pos << " " << pinned_areas_unsorted.back() << " hash = " 
                    << ((obj->obj_info() & HASHCODE_IS_ALLOCATED_BIT) ? 4 : 0));
        }
    }

    if (!gcvt->has_slots()) return true;

    if (gcvt->is_array()) {
        int vector_length = obj->array_length();
        return gc_copy_scan_array_object(obj, vector_length, phase);
    }

    WeakReferenceType type = gcvt->reference_type();
    int *offset_list = gcvt->offset_array();

    // handling special references in objects.
    if (type != NOT_REFERENCE) {
        switch (type) {
            case SOFT_REFERENCE:
                TRACE2("gc.debug", "soft reference 0x" << obj);
                add_soft_reference(obj);
                break;
            case WEAK_REFERENCE:
                TRACE2("gc.debug", "weak reference 0x" << obj);
                add_weak_reference(obj);
                break;
            case PHANTOM_REFERENCE:
                TRACE2("gc.debug", "phantom reference 0x" << obj);
                add_phantom_reference(obj);
                break;
            default:
                TRACE2("gc.verbose", "Wrong reference type");
                break;
        }
    }

    int offset;
    while ((offset = *offset_list) != 0) {
        Partial_Reveal_Object **slot = (Partial_Reveal_Object**)(pos + offset);
        //if (*slot) { looks like without check is better
            TRACE2("gc.debug", "0x" << *slot << " referenced from object = 0x" << obj);
            slots.push_back(slot);
        //}

        offset_list++;
    }

    return true;
#undef pos
}

static void gc_copy_add_root_set_entry_internal(Partial_Reveal_Object **ref, Boolean is_pinned) {
    // FIXME: check for zero here, how it reflect perfomance, should be better!
    // and possibly remove check in gc_copy_process_reference
    // while added check in array handling

#ifdef _DEBUG
    if (*ref) {
        TRACE2("gc.debug", "0x" << *ref << " referenced from root = 0x" << ref << " info = " << (*ref)->obj_info());
    }
#endif

    int phase = heap_mark_phase;
    gc_copy_process_reference(ref, is_pinned, phase);

    while (true) {
        if (slots.empty()) break;
        Partial_Reveal_Object **ref = slots.pop_back();
        *ref;
        gc_copy_process_reference(ref, false, phase);
    }
}

void gc_copy_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned) {
    assert(!is_pinned);
    //TRACE2("gc.enum", "gc_add_root_set_entry");
    gc_copy_add_root_set_entry_internal((Partial_Reveal_Object**)ref, is_pinned);
}

void gc_copy_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned)
{
    assert(!is_pinned);
    int *ref = (int*)slot;
    int oldobj = *ref - offset;
    int newobj = oldobj;

    gc_copy_add_root_set_entry_internal((Partial_Reveal_Object**)&newobj, is_pinned);
    if (newobj != oldobj) {
        *ref = newobj + offset;
    }
}

