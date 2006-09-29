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
#include "stdio.h"

// VM interface header files
#include "platform_lowlevel.h"
#include "open/vm_gc.h"
#include "open/gc.h"
#include "open/vm.h"
#include "jit_intf.h"

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
#include "mark.h"
#include "descendents.h"
#include "gc_debug.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

//
// Run through all the objects in the blocks assigned to this heap colocating the interesting ones.
// An object is interesting if there is some value to colocating it with another object and if
// they are not already colocated.
//
// For this version to avoid race conditions with other threads both objects being colocated need
// to be owned by this thread. In addition the referent must not have already been moved.
// We need to see if this is sufficient to colocate a wide number of objects.
//


// Given an object place move it to dest.

void insert_object_location(GC_Thread *gc_thread, void *dest, Partial_Reveal_Object *p_obj)
{
    assert ((p_obj->get_obj_info() & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK); // Make sure we have the forwarding bit.
    Obj_Info_Type orig_obj_info = p_obj->get_obj_info() & ~FORWARDING_BIT_MASK; // The original obj_info without the forwarding bit set.
    if (orig_obj_info) {
        // obj_info is not zero so remember it.
        object_lock_save_info *obj_info = (object_lock_save_info *) STD_MALLOC(sizeof(object_lock_save_info));
        if (!obj_info) {
            DIE("Internal but out of c malloc space.");
        }
        // Save what needs to be restored.
        obj_info->obj_header = orig_obj_info;
        // I need to keep track of the new after-slided address
        obj_info->p_obj = (Partial_Reveal_Object *) dest;
        gc_thread->insert_object_header_info_during_sliding_compaction(obj_info);
        gc_trace (p_obj, "Object being compacted or colocated needs obj_info perserved.");
    } 

    assert ((p_obj->get_obj_info() & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK); // Make sure we have the forwarding bit.
    // clobber the header with the new address.
    assert (dest);
    assert (((POINTER_SIZE_INT)dest | FORWARDING_BIT_MASK) != (POINTER_SIZE_INT)dest); 
    // This might break on Linux if the heap is in the upper 2 gig of memory. (high bit set)
    // If it does we need to change to the low bit.

    p_obj->set_forwarding_pointer(dest);
    gc_trace (p_obj, "Insert new object location for this object");

    gc_trace (dest, "Eventual object location installed.");
}


//
// This code breaks down into 3 pieces. 
// First we build a queue of all objects that need to be colocated
//    For each object we grab the forwarding pointer using a lock instruction. This means we own it.
//    If we can't gain ownership to the base object we just return.
//    If we can't gain ownership to a single referent we don't colocate that referent
//    If we can't gain ownership to any referent we release ownership of the base object and return.
// Second we install the forwarding pointer in the base object.
// Third we go through the queue and install forwarding pointers in the remaining objects.
// We return true if we can colocate objects.
// No objects are actually moved, that is done later.
//
// Sliding compaction presents some problems when moving the objects. First the invariant that
// if we slide objects to the left we will always have room is no longer obvious since colocated
// objects are also interspersed. If I have dead A B C dead D E and I am colocating D and E after A
// Then I slide                               A  D E 
// and I have overwritten B before I can move it into place.
// One way to deal with this is to move D and E to a seperate area until the entire block has been slid
//                                            A  x  x B                stash    D  E
// But this overwwrites C with A. So we need to stash B also.
//                                            A  x  x x C              stash    D E B 
// Now we can move D E and B
//                                            A  D  E  B C
// This means that each block has a stash of up to an additional single block. This would seem to
// double the space requirements of this algorithm.
//
//
// Each GC thread has a stash list associated with each GC thread including itself. All objects
// that can not be immediately placed where they belong go onto this stash list. This introduces an
// additional synchronization point where all sliding is done and no futher objects will be placed in
// the stash blocks. After this synchronization point all stash blocks can be emptied.
// If we start with x A B C x x D E
// and we want      A D E B C x x x
//
// These are the steps.
//                   Stash
// x A B C x x D E                         Slide A
// A x x C x x D E        B                Stash B
// A x x x x x D E        B C              Stash C
// A D x x x x x E        B C              Move D
// A D E x x x x x        B C              Move E 
// later we will move the stashed objects into place so we end up with
// A D E B C x x x 
//

//
// The rule about whether something goes into the stash block or into the "to" block is simple.
// Each gc thread makes available its scanning pointer. If the target is to the left of the scan pointer
// then the object gets copied directly into its destination. If it is to the right it goes
// into the stash block. If there is not room in the stash block and we are out of stash blocks then
// 

//
// If all objects that remain in a GCs area are always moved into another block then we don't have to worry
// about stashing these objects. If we also limit the number of cross thread objects to the amount of stash
// blocks available then we will never end up with the situation where we are out of stash blocks.
//  

//
// What if we stash the collocaing objects at pointer creating time and then ignore them during sliding time.
// Finally we move them into place after the sliding. 
//

//
// What if we smash the colocating if it means that we can't fit it into lower addresses than the next object
// that is to be slid. This means that the sliding will never have to deal with not having a place to slide
// an object. This is good. The invariant is that if an object must be slid to the right as a result of colocating
// then we do not colocate.
//

// *******************************************************
//
// Objects can be colocated or slid. An object that is to be colocated within the same area and colocation
// is to a lower address is considered a "sliding colocation." All other colocation are considered stash 
// colocation. 
//
// So we colocate up to the number of object we have stash blocks for.
// We colocate only if it does not cause an object to be slid to a "high" address.
//
// *******************************************************


// If two objects are in the same area then we may not want to fuse them. T
// SAME_AREA_SHIFT is what we shift 2 by to get the distance between objects that
// qualify them for being in the same area.

#define SAME_AREA_SHIFT 16


//
// Input/Output next_obj_start_arg - a pointer to a pointer to where the
//  fused objects will eventually reside. Updated to the end of the fused
//  objects.
//
bool fuse_objects (GC_Thread *gc_thread,
                   Partial_Reveal_Object *p_obj,
                   void **next_obj_start_arg,
                   unsigned int *problem_locks)
{


    bool UNUSED debug_printf_trigger = false;

    unsigned int            moved_count = 0;
    unsigned int            unmoved_count = 0;
    // If we can fuse an object we do and return it.
    assert (p_obj->vt()->get_gcvt()->gc_fuse_info);
    gc_trace (p_obj, "This object is a candidate for fusing with next object.");
    
    Partial_Reveal_Object   *scan_stack[MAX_FUSABLE_OBJECT_SCAN_STACK];
    unsigned                top = 0;
    
    Partial_Reveal_Object   *fuse_queue[MAX_FUSED_OBJECT_COUNT];
    unsigned                last = 0;
    
    scan_stack[top++] = p_obj;
    unsigned int fused_size = get_object_size_bytes(p_obj);
    unsigned int base_obj_size = fused_size;

    void *to_obj = *next_obj_start_arg;
    void * UNUSED debug_orig_to_obj = to_obj;
    
    // Claim the Forwading bit if you can. If you loose the race you can't fuse since someone else is.
    Obj_Info_Type old_base_value = p_obj->get_obj_info();
    Obj_Info_Type new_base_value = old_base_value;
    if ((old_base_value & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK)  {
        return false; // Some other thread is going to move this object.
    }

    new_base_value = old_base_value | FORWARDING_BIT_MASK;
    
    if (p_obj->compare_exchange(new_base_value, old_base_value) != old_base_value) {
        // We did not get the forwarding pointer successfully, some other thread got it.
        // Since this is the base object we can just return false.
        return false;
    }
    
    // Build a queue of objects to colocate but do not grab the FORWARDING_BIT until the queue is built.
    while (top > 0) {
        Partial_Reveal_Object *p_cur_obj = scan_stack[--top];
        int *offset_scanner = init_fused_object_scanner(p_cur_obj);
        Slot pp_next_object(NULL);
        Partial_Reveal_Object *p_last_object = p_obj;
        while (pp_next_object.set(p_get_ref(offset_scanner, p_cur_obj)) != NULL) {
            // Move the scanner to the next reference.
            offset_scanner = p_next_ref (offset_scanner);
            // This object is to be fused with the object located at the gc_fuse_info so calculate the required size.
            Partial_Reveal_Object *p_next_from_obj = pp_next_object.dereference();
            gc_trace (p_next_from_obj, "This object is a candidate to be fused with previous object.");
            
            if (p_next_from_obj) {
                // Check NULL.
                block_info *fuse_block_info = GC_BLOCK_INFO(p_next_from_obj);
                void * next_natural_obj = (void *) (POINTER_SIZE_INT(p_last_object) + get_object_size_bytes(p_last_object));
                Obj_Info_Type new_value = p_next_from_obj->get_obj_info();
                bool is_colocation_natural = (next_natural_obj == (void *)p_next_from_obj);
                bool overflow = (((POINTER_SIZE_INT)to_obj + fused_size + get_object_size_bytes(p_next_from_obj)) > (POINTER_SIZE_INT)(GC_BLOCK_CEILING(to_obj)));
                    
                bool already_forwarded = ((new_value & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK);
                bool in_compaction_block = gc_thread->_p_gc->is_compaction_block(fuse_block_info);
                
                bool can_fuse = ((!already_forwarded)
                    && (!is_colocation_natural)
                    && (!overflow)
                    && in_compaction_block
                    );

                if (can_fuse){                
                    if (p_next_from_obj->vt()->get_gcvt()->gc_fuse_info) {
                        scan_stack[top++] = p_next_from_obj;
                    }
                
                    fuse_queue[last] = p_next_from_obj;
                
                    fused_size += get_object_size_bytes(p_next_from_obj);
                    last++;
                } else {
                    p_obj->set_obj_info(old_base_value); // Release the forwarding bit and don't colocate this object.
                     return false;
                }  
            }
        }
    }

    unsigned i;
    // Grab the forwarding bits for the other object in the queue.. If you can't get a bit 
    // remove the object from the queue.
    for (i = 0; i < last; i++) {
        Partial_Reveal_Object *p_fuse_obj = fuse_queue[i];
        Obj_Info_Type new_value = p_fuse_obj->get_obj_info();
        Obj_Info_Type old_value = new_value; 
        bool already_forwarded = ((new_value & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK);
        new_value = old_value | FORWARDING_BIT_MASK; // Create the value with a the forwarding bit set.
        if (!already_forwarded) {
            // install the forwarding bit if it has not already been forwarded.
            already_forwarded = (p_fuse_obj->compare_exchange(new_value, old_value) != old_value);
        }

        if (already_forwarded) {

            debug_printf_trigger = true;
            TRACE("REMOVING FROM FUSE QUEUE.");
            // Remove this object from the queue since we can colocate it.
            unsigned int j;
            for (j = i; j < last - 1; j++) {
                fuse_queue[j] = fuse_queue[j+1];
            }
            // We have one less object on the queue.
            fuse_queue[last] = NULL;
            last--;
            i--; // Redo since fuse_queue[i] now holds a new object.
            unmoved_count++;
        }
        gc_trace (p_fuse_obj, "No space so this object is not fused with parent.");
    }

    // We don't fuse more than a single block worth of objects.
    assert (fused_size <= GC_BLOCK_ALLOC_SIZE);
    // We own all the forwarding bits in all the objects in the fuse_queue.
    
    // If we only have the base object and no other object to colocate with it just return.
    if (last == 0) {
        p_obj->set_obj_info(old_base_value); // Release the forwarding bit and don't colocate this object.
        // No objects to relocate.
        TRACE("3");
        return false;
    }
    
    // At this point all objects in the queue will be fused, we have the forwarding bits 
    // so we now figure out where they will be colocated.
    
    gc_trace (p_obj, "Fusing this object with offspring.");
    assert ((POINTER_SIZE_INT)(GC_BLOCK_INFO (to_obj + get_object_size_bytes(p_obj) - 1)) <= (POINTER_SIZE_INT)(GC_BLOCK_CEILING(to_obj)));
    assert ((p_obj->get_obj_info() & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK);
 
    if (object_info_is_not_zero(p_obj)) {
            if ((p_obj->get_obj_info() & ~FORWARDING_BIT_MASK) != 0) {
                object_lock_save_info *obj_info = (object_lock_save_info *) STD_MALLOC(sizeof(object_lock_save_info));
                assert(obj_info);
                // Save what needs to be restored.
                obj_info->obj_header = p_obj->get_obj_info();
                obj_info->obj_header = obj_info->obj_header & ~FORWARDING_BIT_MASK; // Clear forwarding bit.
                // I need to keep track of the new after-slided address
                obj_info->p_obj = (Partial_Reveal_Object *) to_obj;
                gc_thread->insert_object_header_info_during_sliding_compaction(obj_info);
                *problem_locks = *problem_locks + 1;; // placement code does not deal with this so this is likely to be wrong.
                gc_trace (p_obj, "Object being fused needs obj_info preserved.");
                debug_printf_trigger = true;
                INFO("preserving base fused object header");
            }
        }

    // Finally deal with this placement, moving the base object first.
    insert_object_location (gc_thread, to_obj, p_obj);
    gc_trace (to_obj, " In allocate_forwarding_pointers_for_compaction_live_objects forwarding *to* this location. (vtable not yet legal)");
    gc_trace(p_obj, " was forwarded...");
    if (verify_live_heap) {
        add_repointed_info_for_thread(p_obj, (Partial_Reveal_Object *) to_obj, gc_thread->get_id());
    }

    assert (base_obj_size == get_object_size_bytes(p_obj));
    to_obj = (void *) ((POINTER_SIZE_INT) to_obj + base_obj_size);
    
    // Now figure out where the referent objects belong and set up their forwarding pointers.
    
    for (i = 0; i < last; i++) {
        Partial_Reveal_Object *p_fuse_obj = fuse_queue[i];
        unsigned int fused_obj_size = get_object_size_bytes(p_fuse_obj); 
        gc_trace (p_fuse_obj, "Fusing this object with parent.");
        // Finally deal with this colocations.
        assert (p_fuse_obj != p_obj); // Nulls should have been filtered out up above.
        
        if (object_info_is_not_zero(p_fuse_obj)) {
            if ((p_fuse_obj->get_obj_info() & ~FORWARDING_BIT_MASK) != 0) {
                object_lock_save_info *obj_info = (object_lock_save_info *) STD_MALLOC(sizeof(object_lock_save_info));
                assert(obj_info);
                // Save what needs to be restored.
                obj_info->obj_header = p_fuse_obj->get_obj_info();
                obj_info->obj_header = obj_info->obj_header & ~FORWARDING_BIT_MASK; // Clear forwarding bit.
                // I need to keep track of the new after-slided address
                obj_info->p_obj = (Partial_Reveal_Object *) to_obj;
                gc_thread->insert_object_header_info_during_sliding_compaction(obj_info);
                *problem_locks = *problem_locks + 1;; // placement code does not deal with this so this is likely to be wrong.
                gc_trace (p_fuse_obj, "Object being fused needs obj_info preserved.");
                debug_printf_trigger = true;
                INFO("preserving fused object header");
            }
        }
        
        // Counts are not thread safe but it is just an approximation....
        moved_count++;

        // The object in the queue its forwarding bit set.
        {
            POINTER_SIZE_INT UNUSED next_available = (POINTER_SIZE_INT)to_obj + get_object_size_bytes(p_fuse_obj) -1;
            assert ((fuse_queue[i]->get_obj_info() & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK);
            assert (next_available <=  ((POINTER_SIZE_INT)(GC_BLOCK_CEILING(to_obj))));
        }
        insert_object_location(gc_thread, to_obj, p_fuse_obj);
        gc_trace (to_obj, " In allocate_forwarding_pointers_for_compaction_live_objects forwarding *to* this location. (vtable not yet legal)");
        gc_trace(p_obj, " was forwarded...");
        if (verify_live_heap) {
            add_repointed_info_for_thread(p_fuse_obj, (Partial_Reveal_Object *) to_obj, gc_thread->get_id());
        }
 
        to_obj = (void *) ((POINTER_SIZE_INT) to_obj + fused_obj_size);
    }

    *next_obj_start_arg = to_obj; // Update and return.
    TRACE("next_obj_start_arg addr: " << next_obj_start_arg 
        << ", old_val " << debug_orig_to_obj << ", new_val " << to_obj);
    return true;
}       

#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
