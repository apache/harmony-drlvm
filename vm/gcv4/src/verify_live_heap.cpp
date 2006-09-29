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
 */  
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>

// VM interface header files
#include "platform_lowlevel.h"
#include "open/vm_gc.h"

// GC header files
#include "gc_cout.h"
#include "gc_header.h"
#include "gc_v4.h"
#include "remembered_set.h"
#include "block_store.h"
#include "object_list.h"
#include "work_packet_manager.h"
#include "garbage_collector.h"
#include "gc_globals.h"
#include "gc_thread.h"
#include "descendents.h"

#include <algorithm>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern bool cross_block_compaction;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_OBJECTS 1024 * 1024
#define MAX_THREADS 4
#define JAVA_OBJECT_OVERHEAD (Partial_Reveal_Object::object_overhead_bytes())


typedef struct {
    Partial_Reveal_Object *p_obj;
    Partial_Reveal_Object *p_obj_copied;
    Partial_Reveal_Object *p_obj_moved;
} one_live_object;


one_live_object all_lives_before_gc[MAX_OBJECTS];

unsigned int num_lives_before_gc = 0;

one_live_object all_moved_lives_during_gc[MAX_THREADS][MAX_OBJECTS];

unsigned int cursors[MAX_THREADS];
bool skip_verification;


void
init_verify_live_heap_data_structures()
{
    memset(all_lives_before_gc, 0, sizeof(one_live_object) * MAX_OBJECTS);
    memset(all_moved_lives_during_gc, 0, sizeof(one_live_object) * MAX_OBJECTS);
    memset(all_moved_lives_during_gc, 0, sizeof(one_live_object) * MAX_THREADS * MAX_OBJECTS);
    memset(cursors, 0, sizeof(unsigned int) * MAX_THREADS);
    skip_verification = false;
}


void
add_repointed_info_for_thread(Partial_Reveal_Object *p_old, Partial_Reveal_Object *p_new, unsigned int thread_id)
{
    assert(p_old);
    assert(p_new);
    if (thread_id > (MAX_THREADS - 1)) {
        if (!skip_verification) {
            WARN("add_repointed_info_for_thread() -- Only 4 threads allowed");
            skip_verification = true;
        }
        return;
    }
    unsigned int curr_index_for_thread = cursors[thread_id];
    if (curr_index_for_thread >= MAX_OBJECTS) {
        if (!skip_verification) {
            WARN("add_repointed_info_for_thread() -- OVERFLOW of all_moved_lives_during_gc");
            skip_verification = true;
        }
        return;
    } 
    all_moved_lives_during_gc[thread_id][curr_index_for_thread].p_obj = p_old;
    all_moved_lives_during_gc[thread_id][curr_index_for_thread].p_obj_moved = p_new;
    cursors[thread_id]++;
}


static int 
verify_live_heap_compare( const void *arg1, const void *arg2 )
{
    POINTER_SIZE_INT a = (POINTER_SIZE_INT) ((one_live_object *)arg1)->p_obj;
    POINTER_SIZE_INT b = (POINTER_SIZE_INT) ((one_live_object *)arg2)->p_obj;
    if (a < b) {
        return -1;
    }
    if (a == b) {
        return 0;
    }
    return 1;
}

void
take_snapshot_of_lives_before_gc(unsigned int num_lives, Object_List *all_lives)  
{
    unsigned int total_bytes_allocated_by_verifier_to_replicate_live_heap = 0;

    all_lives->rewind();
    num_lives_before_gc = num_lives;

    if (num_lives > MAX_OBJECTS) {
        if (!skip_verification) {
            WARN("Verify.live.heap can only verify " << MAX_OBJECTS << " objects. Skipping verification");
            skip_verification = true;
        }
        return;
    }
    for (unsigned int i = 0; i < num_lives; i++) {
        Partial_Reveal_Object *p_obj = all_lives->next();
        assert(p_obj);
        all_lives_before_gc[i].p_obj = p_obj;
        int obj_sz = get_object_size_bytes(p_obj);
        all_lives_before_gc[i].p_obj_copied = (Partial_Reveal_Object *) STD_MALLOC(obj_sz);
        assert(all_lives_before_gc[i].p_obj_copied);
        total_bytes_allocated_by_verifier_to_replicate_live_heap += obj_sz;
        memcpy(all_lives_before_gc[i].p_obj_copied, all_lives_before_gc[i].p_obj, obj_sz);
    }
    all_lives->rewind();

    // Sort and return the array of objects
    qsort(all_lives_before_gc, num_lives, sizeof(one_live_object), verify_live_heap_compare);

    // Verify integrity of qsort
    for (unsigned int j = 0; j < num_lives; j++) {
        if (j > 0) {
            // stricly sorted and no duplicates...
            assert((POINTER_SIZE_INT) all_lives_before_gc[j-1].p_obj < (POINTER_SIZE_INT) all_lives_before_gc[j].p_obj);
            assert(all_lives_before_gc[j].p_obj_copied->vt() == all_lives_before_gc[j].p_obj->vt());
            assert(all_lives_before_gc[j].p_obj_copied->get_obj_info() == all_lives_before_gc[j].p_obj->get_obj_info());
        } 
    }

    INFO("total_bytes_allocated_by_verifier_to_replicate_live_heap = " 
        << total_bytes_allocated_by_verifier_to_replicate_live_heap);
}


void 
insert_moved_reference(Partial_Reveal_Object *p_old, Partial_Reveal_Object *p_new)
{
    assert(p_new);
    // Insertion using binary search.
    unsigned int low = 0;
    unsigned int high = num_lives_before_gc;
    bool done = false;
    while (low <= high) {
        unsigned int mid = (high + low) / 2;
        if (all_lives_before_gc[mid].p_obj == p_old) {
            assert(all_lives_before_gc[mid].p_obj_moved == NULL);
            all_lives_before_gc[mid].p_obj_moved = p_new;
            // only objects resident in compaction block actually move
            assert(p_global_gc->is_compaction_block(GC_BLOCK_INFO(p_old)));
            // DONE
            done = true;
            break;
        } else if ((POINTER_SIZE_INT) all_lives_before_gc[mid].p_obj <  (POINTER_SIZE_INT) p_old) { 
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    } 
    if (!done) {
        assert((low > high) || ((low == high) && (all_lives_before_gc[low].p_obj != p_old)));
        // Can't find live object in the list of live objects before gc
        DIE("insert_moved_reference() -- CANT FIND LIVE OBJECT " << p_old);
    }
}


// The input arg is an object that was moved during GC (p_old, is the heap address before GC started)
// return address is in C m_malloc space...it is the saved away copy of p_old before GC mutated the heap..

Partial_Reveal_Object *
find_saved_away_copy_of_live_before_GC_started(Partial_Reveal_Object *p_old) 
{
    assert(p_old);
    // Insertion using binary search.
    unsigned int low = 0;
    unsigned int high = num_lives_before_gc;
    while (low <= high) {
        unsigned int mid = (high + low) / 2;
        if (all_lives_before_gc[mid].p_obj == p_old) {
            // This object was moved during GC
            assert(all_lives_before_gc[mid].p_obj_moved != NULL);
            // only objects resident in compaction block actually move
            assert(p_global_gc->is_compaction_block(GC_BLOCK_INFO(p_old)));
            // A copy was made before GC started
            assert(all_lives_before_gc[mid].p_obj_copied != NULL);
            // DONE
            return all_lives_before_gc[mid].p_obj_copied;
        } else if ((POINTER_SIZE_INT) all_lives_before_gc[mid].p_obj <  (POINTER_SIZE_INT) p_old) { 
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    } 

    assert((low > high) || ((low == high) && (all_lives_before_gc[low].p_obj != p_old)));
    DIE("find_saved_away_copy_of_live_before_GC_started() -- CANT FIND LIVE OBJECT " << p_old);
    return NULL;
}


// first arg  -- Copy of object that was made before GC....in C m_malloc space and outside the heap
// second arg -- the object after GC (a pointer to it into the heap)


void 
verify_live_object_was_not_corrupted_by_gc(Partial_Reveal_Object *p_obj_before_gc, Partial_Reveal_Object *p_obj_after_gc)
{
    // ZZZZZ
    assert(p_obj_before_gc != p_obj_after_gc);
    assert(p_obj_before_gc);
    assert(p_obj_after_gc);

    // VTable needs to be the same before and after GC, otherwise this is a corruption problem
    assert(p_obj_before_gc->vt() == p_obj_after_gc->vt());
    // Though GC changes obj_info for some objects, it needs to be restored when GC ends...
    assert(p_obj_before_gc->get_obj_info() == p_obj_after_gc->get_obj_info());
    
    // Now verify the rest of the bytes....
    struct Partial_Reveal_VTable *obj_vt = p_obj_before_gc->vt();
    unsigned int obj_sz = get_object_size_bytes(p_obj_before_gc);
    assert(obj_sz = get_object_size_bytes(p_obj_after_gc));

    if (obj_vt->get_gcvt()->gc_object_has_slots) {
        
        // Either an array of references....or an object with reference slots...
        assert(!is_array_of_primitives(p_obj_before_gc));

        if (is_array(p_obj_before_gc)) {
            // This is an array of references....we will step through the array one slot at a time...

            // Make sure that array lengths match up...
            int32 array_length = vector_get_length((Vector_Handle)p_obj_before_gc);
            assert(array_length == vector_get_length((Vector_Handle)p_obj_after_gc));
            
            for (int32 i= array_length - 1; i >= 0; i--) {
                Slot p_old_slot(vector_get_element_address_ref((Vector_Handle)p_obj_before_gc, i), false);
                Slot p_new_slot(vector_get_element_address_ref((Vector_Handle)p_obj_after_gc , i));
                if (p_old_slot.is_null()) { 
                    assert(p_new_slot.is_null()) ;
                    continue;
                } 
                // Non-NULL slot before GC 
                if (p_old_slot.dereference() == p_new_slot.dereference()) {
                    // Slot was not updated or updated to the same value...it may have pointed to an object 
                    // in a non-moving part of the heap or was in the moving part but did not move..
                    continue;
                } 
                // unequal slot values before and after GC...
                if (!cross_block_compaction) {
                    // they better be in the same block which is a compaction block
                    assert(GC_BLOCK_INFO(p_old_slot.dereference()) == GC_BLOCK_INFO(p_new_slot.dereference()));
                } 
                assert(p_global_gc->is_compaction_block(GC_BLOCK_INFO(p_new_slot.dereference())));
                // The types of the objects pointed to before and after GC from this slot need to be the same...
                Partial_Reveal_Object * UNUSED p_saved_copy = find_saved_away_copy_of_live_before_GC_started(p_old_slot.dereference());
                assert(p_saved_copy->vt() == p_new_slot.dereference()->vt());
            } // for

        } else {
            // This is a regular object with reference slots...we need to step through here...
            assert(obj_vt->get_gcvt()->gc_number_of_slots > 0);

            uint8* p_byte_old = (uint8 *) ((POINTER_SIZE_INT)p_obj_before_gc + JAVA_OBJECT_OVERHEAD);
            uint8* p_byte_new = (uint8 *) ((POINTER_SIZE_INT)p_obj_after_gc + JAVA_OBJECT_OVERHEAD);
            uint8* p_search_end = (uint8 *) ((POINTER_SIZE_INT)p_obj_before_gc + obj_sz);

            int *old_obj_offset_scanner = init_object_scanner (p_obj_before_gc);
            int *new_obj_offset_scanner = init_object_scanner (p_obj_after_gc);

            while (true) {

                Slot p_old_slot(p_get_ref(old_obj_offset_scanner, p_obj_before_gc), false);
                Slot p_new_slot(p_get_ref(new_obj_offset_scanner, p_obj_after_gc));

                if (p_old_slot.get_address() == NULL) { 
                    // Went past the last reference slot in this object...
                    assert(p_new_slot.get_address() == NULL);
                    break;
                }

                // Skip and compare bytes till we hit the next reference slot..
                while (p_byte_old != (uint8*) p_old_slot.get_address()) {
                    assert(*p_byte_old == *p_byte_new);
                    p_byte_old++;
                    p_byte_new++;
                } 
                assert(p_byte_new == (uint8*) p_new_slot.get_address());
                
                // Now check the reference slots for integrity
                // Non-NULL slot before GC 
                if (p_old_slot.dereference() != p_new_slot.dereference()) {
                    if (Slot::managed_null() == p_new_slot.dereference()) {
                        // Probably the weak referent field was cleared
                        assert(obj_vt->get_gcvt()->reference_type != NOT_REFERENCE);
                        assert(obj_vt->get_gcvt()->referent_offset == *new_obj_offset_scanner);
                    } else {
                        if (!cross_block_compaction) {
                            // unequal slot values before and after GC...they better be in the same block which is a compaction block
                            assert(GC_BLOCK_INFO(p_old_slot.dereference()) == GC_BLOCK_INFO(p_new_slot.dereference()));
                        }
                        assert(p_global_gc->is_compaction_block(GC_BLOCK_INFO(p_new_slot.dereference())));
                        // The types of the objects pointed to before and after GC from this slot need to be the same...
                        Partial_Reveal_Object * UNUSED p_saved_copy = find_saved_away_copy_of_live_before_GC_started(p_old_slot.dereference());
                        assert(p_saved_copy->vt() == p_new_slot.dereference()->vt());
                    }
                } else {
                    // Slot was not updated or updated to the same value...it may have pointed to an object 
                    // in a non-moving part of the heap or was in the moving part but did not move..
                }
                // Move the scanners to the next reference slot
                old_obj_offset_scanner = p_next_ref (old_obj_offset_scanner);
                new_obj_offset_scanner = p_next_ref (new_obj_offset_scanner);

                // Move the byte pointers beyond the reference slot...
                p_byte_old = (uint8 *) ((POINTER_SIZE_INT) p_byte_old + Partial_Reveal_Object::vtable_bytes());
                p_byte_new = (uint8 *) ((POINTER_SIZE_INT) p_byte_new + Partial_Reveal_Object::vtable_bytes());

            } // while

            // We have gone past the last reference slot in this object

            // Check for equality the remaining bytes in this object..
            while (p_byte_old != (uint8 *) p_search_end) {
                assert(*p_byte_old == *p_byte_new);
                p_byte_old++;
                p_byte_new++;
            } 
            assert(p_byte_new == (uint8 *) ((POINTER_SIZE_INT)p_obj_after_gc + obj_sz));
        } 

    } else {
        // This is either an array of primitives or an object with no reference slots...
        assert(is_array_of_primitives(p_obj_before_gc) || (obj_vt->get_gcvt()->gc_number_of_slots == 0));

        if (obj_sz > JAVA_OBJECT_OVERHEAD) {
            // Need simple byte compare of rest of the bytes...regardless of what this is....
            if (memcmp( (void *)((POINTER_SIZE_INT)p_obj_before_gc + JAVA_OBJECT_OVERHEAD),
                        (void *)((POINTER_SIZE_INT)p_obj_after_gc  + JAVA_OBJECT_OVERHEAD),
                        obj_sz - JAVA_OBJECT_OVERHEAD   
                        ) != 0) {
                // BAD
                DIE("Object " << p_obj_before_gc << " of type "
                    << p_obj_before_gc->vt()->get_gcvt()->gc_class_name 
                    << " was corrupted by GC");
            } // if

        } // if

    } // if

    // ALL DONE
    return;
}



void
verify_live_heap_before_and_after_gc(unsigned int num_lives_after_gc, Object_List *all_lives_after_gc)
{

    if (skip_verification) {
        INFO("skipped LIVE HEAP VERIFICATION");
        return;
    }
    assert(num_lives_after_gc == num_lives_before_gc);
    // First, process all the repointing information collected during GC
    for (unsigned int j = 0; j < p_global_gc->get_num_worker_threads(); j++) {
        for (unsigned int k = 0; k < cursors[j]; k++) {
            insert_moved_reference(all_moved_lives_during_gc[j][k].p_obj, all_moved_lives_during_gc[j][k].p_obj_moved);
            assert(all_moved_lives_during_gc[j][k].p_obj_copied == NULL);
        } 
    }

    // Lets move the "all_lives_after_gc" from a Object_List to a HashTable so that has a faster "exists" implementation
    // check for duplicates in the process....
    all_lives_after_gc->rewind();
    Partial_Reveal_Object *p_obj = NULL;
    Hash_Table *all_lives_after_gc_ht = new Hash_Table();
    
    while ((p_obj = all_lives_after_gc->next()) != NULL) {
        // It is not already present...
        assert(all_lives_after_gc_ht->is_not_present(p_obj));
        all_lives_after_gc_ht->add_entry(p_obj);
    }
    assert(all_lives_after_gc_ht->size() == all_lives_after_gc->size());

    // Now we can sit down and verify all the moved and non-moved objects and bit-level integrity
    for (unsigned int i = 0; i < num_lives_before_gc; i++) {
        // some object exists here....and it was copied properly before GC
        assert(all_lives_before_gc[i].p_obj);
        assert(all_lives_before_gc[i].p_obj_copied);

        if (all_lives_before_gc[i].p_obj_moved) {
            if (!cross_block_compaction) {
                // Object moved in the same block.
                assert(GC_BLOCK_INFO(all_lives_before_gc[i].p_obj_moved) == GC_BLOCK_INFO(all_lives_before_gc[i].p_obj));
            }
            // Was in a compaction block
            assert(p_global_gc->is_compaction_block(GC_BLOCK_INFO(all_lives_before_gc[i].p_obj)));
            // Make sure that this object is still live after GC
            all_lives_after_gc_ht->is_present(all_lives_before_gc[i].p_obj_moved);
            // Compare object bytes with snapshot before GC began
            verify_live_object_was_not_corrupted_by_gc(all_lives_before_gc[i].p_obj_copied, all_lives_before_gc[i].p_obj_moved);
        } else {
            // Object was not moved...but it still needs to be live
            all_lives_after_gc_ht->is_present(all_lives_before_gc[i].p_obj);
            // Compare object bytes with snapshot before GC began...it is still in the same place...
            verify_live_object_was_not_corrupted_by_gc(all_lives_before_gc[i].p_obj_copied, all_lives_before_gc[i].p_obj);
        } // if
    } // for

    // Free the hash table that we just constructed...
    delete all_lives_after_gc_ht;
    
    // We need to free up the live heap that was malloced befored we leave...
    for (unsigned int x = 0; x < num_lives_before_gc; x++) {
        // some object exists here....and it was copied properly before GC
        assert(all_lives_before_gc[x].p_obj);
        assert(all_lives_before_gc[x].p_obj_copied);
        STD_FREE(all_lives_before_gc[x].p_obj_copied);
    }
    
    INFO("LIVE HEAP VERIFICATION COMPLETED WITH NO ERRORS FOUND!!");
}
