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
 * @author Intel, Salikh Zakirov
 * @version $Revision: 1.1.2.2.4.3 $
 *
 * @file
 * The heap tracing code.
 */

#include "remembered_set.h"
#include "garbage_collector.h"
#include "gc_thread.h"
#include "mark.h"
#include "descendents.h"

static void scan_object(Partial_Reveal_Object *, GC_Mark_Activity *);

static void
scan_slot (Slot p_slot, GC_Mark_Activity *gc_thread)
{
    assert(p_slot.get_address());
    if (p_slot.is_null()) {
        return;
    }
    Partial_Reveal_Object *p_obj = p_slot.dereference();

    gc_trace(p_obj, " scan_slot(): a slot points to this object...");

    // I will try to be the first to mark it. If success..I will add it to my mark 
    // stack, since I will then be owning it.

    if (mark_object(p_obj)) {

        // Add to mark stack
        Mark_Stack *ms = gc_thread->get_mark_stack();
        if (ms->push_back(p_obj) == false) {
            DIE("Mark stack overflowed");
        }
    }

    if (gc_thread->is_compaction_turned_on_during_this_gc()) {  // Collect slots as we go
        block_info *p_obj_block_info = GC_BLOCK_INFO(p_obj);
        if (gc_thread->_p_gc->is_compaction_block(p_obj_block_info)) {
            // Adding the slot to the thread-local list of slots for the block.
            gc_thread->_p_gc->_p_block_store->
                add_slot_to_compaction_block(p_slot,
                                             p_obj_block_info,
                                             gc_thread->get_id());
            if (stats_gc) {
                gc_thread->increment_num_slots_collected();
            }
        }
    }
}



static inline void 
scan_array_object(Partial_Reveal_Object *array, GC_Mark_Activity *gc_thread)
{
    // No primitive arrays allowed
    assert(!is_array_of_primitives(array));

    Type_Info_Handle tih;
    tih = class_get_element_type_info(array->vt()->get_gcvt()->gc_clss);

    assert (type_info_is_reference(tih) || type_info_is_vector(tih) || type_info_is_general_array(tih));

    int32 array_length = vector_get_length((Vector_Handle) array);
    for (int i = 0; i < array_length; i++) {
        Slot p_element(vector_get_element_address_ref
                ((Vector_Handle) array, i));

        scan_slot(p_element, gc_thread);
    }
}

static void 
scan_object(Partial_Reveal_Object *p_obj, GC_Mark_Activity *gc_thread)
{
    // This functions is called exactly once for each live object in the heap

    // Object had better be marked.
    assert(is_object_marked(p_obj) == true);

    if (!p_obj->has_slots()) {
        gc_trace(p_obj, " scan_object(): object doesn't contain slots");
        return;
    }

    if (is_array(p_obj)) {
        scan_array_object(p_obj, gc_thread);
        gc_trace(p_obj, " scan_object(): array object has been scanned...");
        return;
    }

    int *offset_scanner = init_strong_object_scanner(p_obj);
    Slot slot(NULL);

    while (true) {
        void *ref = p_get_ref(offset_scanner, p_obj);

        if (ref == NULL) break;
        slot.set(ref);

        gc_trace(p_obj, "scan_object(): scanning slot "
                << *offset_scanner << " of " << p_obj << " -> " << slot.dereference());

        scan_slot(slot, gc_thread);
        // Move the scanner to the next reference.
        offset_scanner = p_next_ref(offset_scanner);
    }

    WeakReferenceType type = p_obj->vt()->get_gcvt()->reference_type;

    // handling special references in objects.
    if (type != NOT_REFERENCE) {
        switch (type) {
            case SOFT_REFERENCE:
                gc_thread->_p_gc->add_soft_reference(p_obj);
                break;
            case WEAK_REFERENCE:
                gc_thread->_p_gc->add_weak_reference(p_obj);
                break;
            case PHANTOM_REFERENCE:
                gc_thread->_p_gc->add_phantom_reference(p_obj);
                break;
            default:
                DIE("Wrong reference type");
                break;
        }
    }

    gc_trace(p_obj, " scan_object(): this object has been scanned...");
}

void
scan_root(Partial_Reveal_Object *p_obj, GC_Mark_Activity *gc_thread)
{
    assert (p_obj != NULL);

    Mark_Stack *ms = gc_thread->get_mark_stack();

    gc_trace(p_obj, "this object was found to be a root");

    // Mark the object. Means, it will be added to the mark stack, and later scanned
    if (mark_object(p_obj)) {

        gc_trace(p_obj, "putting object onto mark stack");
        // Add to mark stack
        if (ms->push_back(p_obj) == false) {
            DIE("Mark stack overflowed");
        }
    }

    while (true) {

        Partial_Reveal_Object *p_obj = ms->pop_back();
        gc_trace(p_obj, "taking object off mark stack, about to scan it");
        if (p_obj == NULL) {
            break;
        }
        scan_object(p_obj, gc_thread);      
    }
    // MARK STACK IS EMPTY....GO AND GET SOME MORE ROOTS FROM GC
}

void Garbage_Collector::mark_scan_heap(GC_Thread *gc_thread)
{
    mark_scan_roots(gc_thread);

    INFOW(gc_thread, "collected "
          << gc_thread->get_num_slots_collected()
          << " slots for later fixing");
}
