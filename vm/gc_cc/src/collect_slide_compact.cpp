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
#include "slide_compact.h"

unsigned char *mark_bits;
size_t mark_bits_size;
fast_list<Partial_Reveal_Object*, 65536> objects;
reference_vector references_to_enqueue;

static inline bool
is_compaction_object(Partial_Reveal_Object *refobj) {
    if ((unsigned char*) refobj < heap.compaction_region_start()) return false;
    if ((unsigned char*) refobj >= heap.compaction_region_end()) return false; // FIXME: is it ok to remove upper limit?
    return true;
}

static inline bool
is_forwarded_object(Partial_Reveal_Object *obj) {
    return obj->vt() & FORWARDING_BIT;
}

static inline void
update_forwarded_reference(Partial_Reveal_Object *obj, Slot slot) {
    assert(!(obj->vt() & RESCAN_BIT));
    assert(obj->vt() & FORWARDING_BIT);
    slot.write(fw_to_pointer(obj->vt() & ~FORWARDING_BIT));
}

#if GC_OBJECT_ALIGNMENT == 8
#define GC_OBJECT_ALIGNMENT_SHIFT 3
#elif GC_OBJECT_ALIGNMENT == 4
#define GC_OBJECT_ALIGNMENT_SHIFT 2
#else
#error not detected GC_OBJECT_ALIGNMENT
#endif

static inline bool mark_bit_is_set(Partial_Reveal_Object *obj) {
    size_t addr = (POINTER_SIZE_INT)obj - (POINTER_SIZE_INT) heap_base;
    addr >>= GC_OBJECT_ALIGNMENT_SHIFT;
    size_t bit = addr & 7; // FIXME: use defines
    size_t byte = addr >> 3;
    return mark_bits[byte] & ((unsigned char)1 << bit);
}

static inline void enqueue_reference(Partial_Reveal_Object *refobj, Slot slot) {
    assert(is_compaction_object(refobj));
    assert(!is_forwarded_object(refobj));
    //assert(*ref == refobj);
    assert(refobj->obj_info());

    unsigned &info = refobj->obj_info();
    slot.write_raw(info);
    info = slot.addr() | heap_mark_phase; //(int)ref
}

static inline bool is_object_marked(Partial_Reveal_Object *obj) {
    return obj->obj_info() & heap_mark_phase;
}

static inline void set_mark_bit(Partial_Reveal_Object *obj) {
    size_t addr = (POINTER_SIZE_INT)obj - (POINTER_SIZE_INT) heap_base;
    addr >>= GC_OBJECT_ALIGNMENT_SHIFT;
    size_t bit = addr & 7; // FIXME: use defines
    size_t byte = addr >> 3;
    mark_bits[byte] |=  ((unsigned char) 1 << bit);
}

static inline bool mark_object(Partial_Reveal_Object *obj) {
    int phase = heap_mark_phase;

    assert((unsigned char*) obj >= heap.base && (unsigned char*) obj < heap.ceiling);
    assert(obj->vt() != 0);

    // is object already marked
    if (obj->obj_info() & phase) {
        return false;
    }

    obj->valid();
    assert(!is_forwarded_object(obj));

    unsigned info = obj->obj_info();

    if (is_compaction_object(obj)) {
        set_mark_bit(obj);

        if (info & OBJECT_IS_PINNED_BITS) {
            pinned_areas_unsorted.push_back((unsigned char*)obj);
            int size = get_object_size(obj, obj->vtable()->get_gcvt());
            pinned_areas_unsorted.push_back((unsigned char*)obj + size
                    + ((info & HASHCODE_IS_ALLOCATED_BIT) ? GC_OBJECT_ALIGNMENT : 0));
            TRACE2("gc.pin", "add pinned area = " << (unsigned char*)obj << " " << pinned_areas_unsorted.back());
        }

        info |= MARK_BITS;
    } else {
        info = (info & ~MARK_BITS) | phase;
    }
    obj->obj_info() = info;

    assert(obj->obj_info() != 0);
    return true;
}

static inline void set_rescan_bit(Partial_Reveal_Object *obj) {
    obj->vt() |= RESCAN_BIT;
}

static inline void process_reference_queue(Partial_Reveal_Object *newobj, Partial_Reveal_Object *obj) {
    unsigned info = obj->obj_info();
    assert(info);
    assert(info & heap_mark_phase); assert(is_compaction_object(obj));

    while (!(info & prev_mark_phase)) {
        assert(info);
        assert(info & heap_mark_phase);
        Slot slot((Reference*) fw_to_pointer(info & ~MARK_BITS));
        info = slot.read_raw(); //(int)*ref;
        slot.write(newobj);
    }
    obj->obj_info() = info & ~MARK_BITS;
}

static void postprocess_array(Partial_Reveal_Object *array, int vector_length, Partial_Reveal_Object *oldobj) {
    // No primitive arrays allowed
    assert(!is_array_of_primitives(array));
    assert(is_compaction_object(array));
    assert(!is_forwarded_object(array));

    int array_length = vector_length; //vector_get_length((Vector_Handle) array);

    Reference *refs = (Reference*) vector_get_element_address_ref ((Vector_Handle) array, 0);

    for(int i = 0; i < array_length; i++) {
        Slot slot(refs + i);
        POINTER_SIZE_INT refobj_int = (POINTER_SIZE_INT)slot.read();
        POINTER_SIZE_INT refobj_unmarked = refobj_int & ~1;
        if (refobj_int == refobj_unmarked) continue; // not specially marked reference
        Partial_Reveal_Object *refobj = (Partial_Reveal_Object*) refobj_unmarked;
        enqueue_reference(refobj, slot);
    }
}

// after moving some objects should be rescanned
// storing references to this object to the linked list
// of reversers to the right of this one.
// oldobj = original position of object:
// if this object is pinned and referenced object is moved only original
// position of this object contains valid (unchanged) information of left/right direction
static void postprocess_object(Partial_Reveal_Object *obj, Partial_Reveal_Object *oldobj) {
    assert(obj);
    assert(is_compaction_object(obj));
    assert(!is_forwarded_object(obj));
 
    assert((unsigned char*) obj >= heap.base && (unsigned char*) obj < heap.ceiling);
    assert(obj->vt() & RESCAN_BIT);

    VT32 vt = obj->vt() & ~RESCAN_BIT;
    obj->vt() = vt;
    Partial_Reveal_VTable *vtable = ah_to_vtable(vt);
    GC_VTable_Info *gcvt = vtable->get_gcvt();

    // process slots
    assert(gcvt->has_slots());

    if (gcvt->is_array()) {
        int vector_length = obj->array_length();
        postprocess_array(obj, vector_length, oldobj);
        return;
    }

    if (gcvt->reference_type() != NOT_REFERENCE) {
        Slot slot((Reference*)((char*)obj + global_referent_offset));

        POINTER_SIZE_INT refobj_int = (POINTER_SIZE_INT)slot.read();
        POINTER_SIZE_INT refobj_unmarked = refobj_int & ~1;
        if (refobj_int != refobj_unmarked) {
            Partial_Reveal_Object *refobj = (Partial_Reveal_Object*) refobj_unmarked;
            enqueue_reference(refobj, slot);
        }
    }

    int *offset_list = gcvt->offset_array();
    int offset;
    while ((offset = *offset_list) != 0) {
        Slot slot( (Reference*)((char*)obj + offset));
        offset_list++;

        POINTER_SIZE_INT refobj_int = (POINTER_SIZE_INT)slot.read();
        POINTER_SIZE_INT refobj_unmarked = refobj_int & ~1;
        if (refobj_int == refobj_unmarked) continue; // not specially marked reference
        Partial_Reveal_Object *refobj = (Partial_Reveal_Object*) refobj_unmarked;
        enqueue_reference(refobj, slot);
    }
}

void gc_slide_move_all() {
    unsigned char *compact_pos = heap.compaction_region_start();
    unsigned char *compact_pos_limit = heap.compaction_region_end();
    unsigned char *next_pinned_object = heap.compaction_region_end();
    unsigned next_pinned_object_pos = 0;

    prev_mark_phase = heap_mark_phase ^ 3;
    pinned_areas_pos = 1;
    if (pinned_areas.size() != 0) compact_pos_limit = pinned_areas[0];

#if _DEBUG
    POINTER_SIZE_INT pin_size = 0;
    for(pinned_areas_unsorted_t::iterator iii = pinned_areas_unsorted.begin();
            iii != pinned_areas_unsorted.end(); ++iii) {
        unsigned char *start = *iii; ++iii;
        unsigned char *end = *iii;
        pin_size += end - start;
    }
#endif

    pinned_areas.resize(pinned_areas_unsorted.count());
    partial_sort_copy(pinned_areas_unsorted.begin(), pinned_areas_unsorted.end(), pinned_areas.begin(), pinned_areas.end());

#if _DEBUG
    POINTER_SIZE_INT sorted_pin_size = 0;
    for(unsigned ii = 0; ii < pinned_areas.size(); ii+=2) {
        TRACE2("gc.pin", "pinned_areas[" << ii << "] = " << pinned_areas[ii]);
        TRACE2("gc.pin", "pinned_areas[" << ii+1 << "] = " << pinned_areas[ii+1]);
        sorted_pin_size += pinned_areas[ii+1] - pinned_areas[ii];
    }
    assert(pin_size == sorted_pin_size);
#endif

    for(unsigned i = 0; i < pinned_areas.size(); i+=2) {
        unsigned char *obj_start = pinned_areas[i];
        if ((unsigned char*)obj_start < compact_pos) {
            assert(pinned_areas[i+1] <= compact_pos);
            continue;
        }
        compact_pos_limit = obj_start;
        pinned_areas_pos = i + 1;
        next_pinned_object = obj_start;
        next_pinned_object_pos = i;
        TRACE2("gc.pin", "next pinned object " << next_pinned_object_pos << " = " << next_pinned_object);
        break;
    }

    pinned_areas.push_back(heap.compaction_region_end());

    int *mark_words = (int*) mark_bits;
    // Searching marked bits
    size_t start = (size_t)(heap.compaction_region_start() - heap_base) / GC_OBJECT_ALIGNMENT / sizeof(int) / 8;
    size_t end = (size_t)(heap.compaction_region_end() - heap_base + GC_OBJECT_ALIGNMENT * sizeof(int) * 8 - 1) / GC_OBJECT_ALIGNMENT / sizeof(int) / 8;
    if (end > mark_bits_size/sizeof(int)) end = mark_bits_size/sizeof(int);
    for(size_t i = start; i < end; i++) {
        // no marked bits in word - skip

        int word = mark_words[i];
        if (word == 0) continue;

        for(int bit = 0; bit < 32; bit++) {
            if (word & 1) {
                unsigned char *pos = heap_base + i * 8 * GC_OBJECT_ALIGNMENT * sizeof(int) + bit * GC_OBJECT_ALIGNMENT;
                Partial_Reveal_Object *obj = (Partial_Reveal_Object*) pos;

                VT32 vt = obj->vt();
                bool post_processing = vt & RESCAN_BIT;
                Partial_Reveal_VTable *vtable = ah_to_vtable(vt & ~RESCAN_BIT);
                int size = get_object_size(obj, vtable->get_gcvt());

                assert(is_object_marked(obj));
                assert(!is_forwarded_object(obj));

                if ((unsigned char*)obj != next_pinned_object) {

                    // 4/8 bytes reserved for hash
                    while (compact_pos + size > compact_pos_limit) {

                        if (compact_pos == pos) {
                            break;
                        }

                        assert(compact_pos_limit <= pos);
                        clear_mem_for_heap_iteration(compact_pos, compact_pos_limit - compact_pos);

                        assert(pinned_areas_pos < pinned_areas.size());
                        compact_pos = pinned_areas[pinned_areas_pos];
                        compact_pos_limit = pinned_areas[pinned_areas_pos+1];
                        pinned_areas_pos += 2;
                    }
                    
                    Partial_Reveal_Object *newobj;

                    assert(compact_pos <= pos);

                    if (compact_pos == pos) {
                        newobj = obj;
                        process_reference_queue(obj, obj);

                        unsigned info = obj->obj_info();
                        compact_pos += size +
                            (((info & HASHCODE_IS_ALLOCATED_BIT) != 0) ? GC_OBJECT_ALIGNMENT : 0);
                    } else {
                        unsigned char *newpos = compact_pos;
                        compact_pos += size;

                        newobj = (Partial_Reveal_Object*) newpos;
                        process_reference_queue(newobj, obj);
                        unsigned info = obj->obj_info();

                        if (info & HASHCODE_IS_SET_BIT) {
                            size += GC_OBJECT_ALIGNMENT;
                            compact_pos += GC_OBJECT_ALIGNMENT;
                        }

                        if (newpos + size <= pos) {
                            memcpy(newpos, pos, size);
                        } else {
                            memmove(newpos, pos, size);
                        }
                        if (info & HASHCODE_IS_SET_BIT && !(info & HASHCODE_IS_ALLOCATED_BIT)) {
                            *(int*)(newpos + size - GC_OBJECT_ALIGNMENT) = gen_hashcode(pos);
                            newobj->obj_info() |= HASHCODE_IS_ALLOCATED_BIT;
                        }
                    }

                    if (post_processing) postprocess_object(newobj, obj);
                    else assert(!(newobj->vt() & RESCAN_BIT));
                    assert(!(newobj->vt() & RESCAN_BIT));
                    assert(!(newobj->obj_info() & OBJECT_IS_PINNED_BITS));
                } else {
                    process_reference_queue(obj, obj);
                    if (obj->vt() & RESCAN_BIT) postprocess_object(obj, obj);
                    obj->vt() &= ~RESCAN_BIT;
                    next_pinned_object_pos += 2;
                    next_pinned_object = pinned_areas[next_pinned_object_pos];
                }

                // FIXME: is it really speedup?
                if (!(word >> 1)) break;
            }
            word >>= 1;
        }
    }
    assert(next_pinned_object >= heap.compaction_region_end());
    pinned_areas.pop_back(); //heap.compaction_region_end()

    TRACE2("gc.mem", "compaction: region size = "
            << (heap.compaction_region_end() - heap.compaction_region_start()) / 1024 / 1024 << " mb");
    TRACE2("gc.mem", "compaction: free_space = "
            << (heap.compaction_region_end() - compact_pos) / 1024 / 1024 << " mb");

    cleaning_needed = true;
    heap.pos = compact_pos;
    heap.pos_limit = compact_pos_limit;

    heap.old_objects.end = compact_pos;
    heap.old_objects.pos = compact_pos;
    heap.old_objects.pos_limit = compact_pos;

    old_pinned_areas.clear();
    old_pinned_areas_pos = 1;
}

static void slide_process_object(Partial_Reveal_Object *obj);

static inline void 
slide_scan_array_object(Partial_Reveal_Object *array, Partial_Reveal_VTable *vtable, int vector_length)
{
    // No primitive arrays allowed
    assert(!is_array_of_primitives(array));
    assert(!is_forwarded_object(array));

    int32 array_length = vector_length; //vector_get_length((Vector_Handle) array);

    Reference *refs = (Reference*) vector_get_element_address_ref ((Vector_Handle) array, 0);

    if (is_compaction_object(array)) {
        bool rescan = false;
        for(int i = 0; i < array_length; i++) {
            Slot slot(refs + i);
            Partial_Reveal_Object *refobj = slot.read();
            if (refobj == heap_null) continue;

            if (mark_object(refobj)) {
                slide_process_object(refobj);
            } else if (is_forwarded_object(refobj)) {
                update_forwarded_reference(refobj, slot);
                continue;
            }

            if (is_compaction_object(refobj)) {
                if (is_left_object(refobj, slot)) {
                    enqueue_reference(refobj, slot);
                } else {
                    // mark_rescan_reference
                    slot.write( (Partial_Reveal_Object*) ((size_t)refobj | 1) );
                    rescan = true;
                }
            }
        }
        if (rescan) set_rescan_bit(array);
    } else {
        for(int i = 0; i < array_length; i++) {
            Slot slot(refs + i);
            Partial_Reveal_Object *refobj = slot.read();
            if (refobj == heap_null) continue;

            if (mark_object(refobj)) {
                slide_process_object(refobj);
            } else if (is_forwarded_object(refobj)) {
                update_forwarded_reference(refobj, slot);
                continue;
            }

            if (is_compaction_object(refobj)) {
                enqueue_reference(refobj, slot);
            }
        }
    }
}

static void slide_process_object(Partial_Reveal_Object *obj) {

    assert(obj);
    assert((unsigned char*) obj >= heap.base && (unsigned char*) obj < heap.ceiling);
    assert(is_object_marked(obj));
    //assert(mark_bit_is_set(obj) || !is_compaction_object(obj));

    unsigned vt = obj->vt();
    assert(obj->vt() & ~RESCAN_BIT); // has vt

    Partial_Reveal_VTable *vtable = ah_to_vtable(vt & ~RESCAN_BIT);
    GC_VTable_Info *gcvt = vtable->get_gcvt();

    // process slots
    if (!gcvt->has_slots()) return;

    if (gcvt->is_array()) {
        int vector_length = obj->array_length();
        slide_scan_array_object(obj, vtable, vector_length);
        return;
    }


    WeakReferenceType type = gcvt->reference_type();
    int *offset_list = gcvt->offset_array();

    // handling special references in objects.
    if (type != NOT_REFERENCE) {
        switch (type) {
            case SOFT_REFERENCE:
                add_soft_reference(obj);
                soft_refs++;
                break;
            case WEAK_REFERENCE:
                add_weak_reference(obj);
                weak_refs++;
                break;
            case PHANTOM_REFERENCE:
                add_phantom_reference(obj);
                phantom_refs++;
                break;
            default:
                TRACE2("gc.verbose", "Wrong reference type");
                break;
        }
    }

    if (is_compaction_object(obj)) {
        bool rescan = false;
        int offset;
        while ((offset = *offset_list) != 0) {
            Slot slot((Reference*)((char*)obj + offset));
            Partial_Reveal_Object *refobj = slot.read();
            offset_list++;

            if (refobj == heap_null) continue;

            if (mark_object(refobj)) {
                objects.push_back(refobj);
            } else if (is_forwarded_object(refobj)) {
                update_forwarded_reference(refobj, slot);
                continue;
            }

            if (is_compaction_object(refobj)) {
                if (is_left_object(refobj, slot)) {
                    enqueue_reference(refobj, slot);
                } else {
                    // mark_rescan_reference
                    slot.write( (Partial_Reveal_Object*) ((size_t)refobj | 1) );
                    rescan = true;
                }
            }
        }
        if (rescan) set_rescan_bit(obj);
    } else {
        int offset;
        while ((offset = *offset_list) != 0) {
            Slot slot((Reference*)((char*)obj + offset));
            Partial_Reveal_Object *refobj = slot.read();
            offset_list++;

            if (refobj == heap_null) continue;

            if (mark_object(refobj)) {
                objects.push_back(refobj);
            } else if (is_forwarded_object(refobj)) {
                update_forwarded_reference(refobj, slot);
                continue;
            }

            if (is_compaction_object(refobj)) {
                enqueue_reference(refobj, slot);
            }
        }
    }

}

void gc_slide_add_root_set_entry(Slot slot) {
    // get object
    Partial_Reveal_Object *refobj = slot.read();

    // check no garbage
    assert(((POINTER_SIZE_INT)refobj & 3) == 0);

    // empty references is not interesting
    if (refobj == heap_null) return;

    if (mark_object(refobj)) {
        // object wasn't marked yet
        slide_process_object(refobj);
    } else if (is_forwarded_object(refobj)) {
        update_forwarded_reference(refobj, slot);
        goto skip;
    }

    if (is_compaction_object(refobj)) {
        enqueue_reference(refobj, slot);
    }
skip:

    while (true) {
        if (objects.empty()) break;
        Partial_Reveal_Object *obj = objects.pop_back();
        slide_process_object(obj);
    }
}

void gc_slide_process_special_references(reference_vector& array) {
    if (array.empty()) return;
    pending_finalizers = true;

    for(reference_vector::iterator i = array.begin();
            i != array.end(); ++i) {
        Partial_Reveal_Object *obj = *i;

        Slot slot(
            (Reference*) ((unsigned char *)obj + global_referent_offset));
        Partial_Reveal_Object* refobj = slot.read();

        if (refobj == 0) {
            // reference already cleared, no post processing needed
            *i = heap_null;
            continue;
        }

        if (is_object_marked(refobj)) {
            //assert(mark_bit_is_set(refobj) || !is_compaction_object(refobj) || is_forwarded_object(refobj));

            if (is_forwarded_object(refobj)) {
                update_forwarded_reference(refobj, slot);
            } else if (is_compaction_object(refobj)) {
                if (is_left_object(refobj, slot) || !is_compaction_object(obj)) {
                    enqueue_reference(refobj, slot);
                } else {
                    // mark_rescan_reference
                    slot.write( (Partial_Reveal_Object*) ((size_t)refobj | 1) );
                    set_rescan_bit(obj);
                }
            }

            // no post processing needed
            *i = heap_null;
            continue;
        } else {
            //assert(!mark_bit_is_set(refobj));
        }

        // object not marked, clear reference
        slot.write((Partial_Reveal_Object*) heap_null);
        references_to_enqueue.push_back(obj);
        Slot root = make_direct_root(&references_to_enqueue.back());

        if (is_forwarded_object(obj)) {
            update_forwarded_reference(obj, root);
        } else if (is_compaction_object(obj)) {
            enqueue_reference(obj, root);
        }
    }
    array.clear();
}

void gc_slide_process_special_roots(slots_vector& array) {
    for(slots_vector::iterator i = array.begin();
            i != array.end(); ++i) {
        Partial_Reveal_Object **ref = *i;
        Partial_Reveal_Object* refobj = *ref;

        if (refobj == 0) {
            // reference already cleared, no post processing needed
            continue;
        }

        if (is_object_marked(refobj)) {
            //assert(mark_bit_is_set(refobj) || !is_compaction_object(refobj) || is_forwarded_object(refobj));

            Slot root = make_direct_root(ref);

            if (is_forwarded_object(refobj)) {
                update_forwarded_reference(refobj, root);
            } else if (is_compaction_object(refobj)) {
                enqueue_reference(refobj, root);
            }
        } else {
            *ref = NULL;
        }
    }
    array.clear();
}

void gc_slide_postprocess_special_references() {
    for(reference_vector::iterator i = references_to_enqueue.begin();
            i != references_to_enqueue.end(); ++i) {
        Partial_Reveal_Object *obj = *i;

        assert (obj != heap_null);
        vm_enqueue_reference((Managed_Object_Handle)obj);
    }
    references_to_enqueue.clear();
}

// transition from coping collector code
// all previous references are processed in copying collector
// so will not move, they can be considered as root references here

void gc_slide_process_transitional_slots(fast_list<Slot,65536>& slots) {
    // also process pinned objects all but last
    pinned_areas_unsorted_t::iterator end = --(--pinned_areas_unsorted.end());
    for(pinned_areas_unsorted_t::iterator i = pinned_areas_unsorted.begin();
            i != end; ++i,++i) {
        Partial_Reveal_Object *obj = (Partial_Reveal_Object*) *i;
        if (is_compaction_object(obj)) {
            set_mark_bit(obj);
            obj->obj_info() |= MARK_BITS;
        }
    }

    while (true) {
        if (slots.empty()) break;
        Slot slot = slots.pop_back();
        gc_slide_add_root_set_entry(slot);
    }
}

void gc_slide_process_transitional_slots(Reference *refs, int pos, int length) {
    for(int i = pos; i < length; i++) {
        Slot slot(refs + i);
        gc_slide_add_root_set_entry(slot);
    }
}
