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
 * Object compaction code.
 */

// Portlib
#include "port_malloc.h"

// VM interface header files
#include "platform_lowlevel.h"
#include "open/vm_gc.h"
#include "open/gc.h"

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


#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.compact"
 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern bool verify_live_heap;
extern bool cross_block_compaction;
void add_repointed_info_for_thread(Partial_Reveal_Object *p_old, Partial_Reveal_Object *p_new, unsigned int thread_id);
void dump_object_layouts_in_compacted_block(block_info *, unsigned int);
void close_dump_file();
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
fix_slots_to_compaction_live_objects(GC_Thread *gc_thread)
{
    block_info *p_compaction_block = NULL;
    unsigned int num_slots_fixed = 0;

    while ((p_compaction_block = gc_thread->_p_gc->get_block_for_fix_slots_to_compaction_live_objects(gc_thread->get_id(), p_compaction_block))) {

        while (true) {

            Remembered_Set *some_slots = gc_thread->_p_gc->get_slots_into_compaction_block(p_compaction_block);
            if (some_slots == NULL) {
                // Done with fixing all slots
                break;
            }

            some_slots->rewind();
            Slot one_slot(NULL);

            while (one_slot.set(some_slots->next().get_address())) {

                Partial_Reveal_Object *p_obj = one_slot.dereference();

                // This slot points into this compaction block
                assert(GC_BLOCK_INFO(p_obj) == p_compaction_block);
                assert(p_obj->get_obj_info() & FORWARDING_BIT_MASK);
                Partial_Reveal_Object *p_new_obj = p_obj->get_forwarding_pointer();
                
                gc_trace(p_obj, " fix_slots_to_compaction_live_objects(): a slot pointed to this object, but the slot is being repointed to " << p_new_obj);
                gc_trace(p_new_obj, " fix_slots_to_compaction_live_objects(): a slot repointed to this object, but the slot was pointing to " << p_obj);                
                // update slot
                one_slot.update(p_new_obj);
                num_slots_fixed++;

                assert(GC_BLOCK_INFO(p_obj)->in_nursery_p == true);
                assert(GC_BLOCK_INFO(p_new_obj)->in_nursery_p == true);
                if (!cross_block_compaction) {
                    // objects move within the same block
                    assert(GC_BLOCK_INFO(p_new_obj) == GC_BLOCK_INFO(p_obj));
                } else {
                    // the block this moves into better have been flagged for compaction..
                    assert(GC_BLOCK_INFO(p_new_obj)->is_compaction_block);
                }

            } // while (one_slot)

            // Delete the remebered set since it is not needed anymore...all slot updates for this list are done
            delete some_slots;

        } // while (true)
    } // while 

    INFOW(gc_thread, "fixed " << num_slots_fixed << " slots");
}

// We make available all bytes after we are finished sliding objects into this block.

POINTER_SIZE_INT
sweep_slide_compacted_block(block_info * p_compaction_block, void * UNREF first_free_byte_in_this_block_after_sliding_compaction)
{
    gc_trace_block (p_compaction_block, " calling clear_block_free_areas in sweep_slide_compacted_block.");
    // Clear all the free areas computed during the previous GC
    clear_block_free_areas(p_compaction_block);

    POINTER_SIZE_INT bytes_freed_by_compaction_in_this_block = 0;
    p_compaction_block->num_free_areas_in_block = 0;

    p_compaction_block->current_alloc_area = -1;    

    // FIXME: free space in to-blocks is ignored
    bytes_freed_by_compaction_in_this_block = 0;
    p_compaction_block->curr_free = NULL;
    p_compaction_block->curr_ceiling = NULL;
    // No need to sweep this block during allocation
    p_compaction_block->block_has_been_swept = true;

    assert(p_compaction_block->block_used_bytes + p_compaction_block->block_free_bytes == GC_BLOCK_ALLOC_SIZE);

    if (p_compaction_block->block_used_bytes <= GC_BLOCK_ALLOC_SIZE - 4) {
        // mark the end of allocated objects (needed for heap iteration)
        *(int*)((char*)GC_BLOCK_ALLOC_START(p_compaction_block) + p_compaction_block->block_used_bytes) = 0;
    }

    // These blocks dont get swept during allocation after mutators are restarted.
    // Means we need to explicitly clear their mark bit vectors, since they are fully useless until they get
    // repopulated with bits in the next GC cycle   
    clear_block_mark_bit_vector(p_compaction_block);

    return bytes_freed_by_compaction_in_this_block;
}


POINTER_SIZE_INT
sweep_free_block(block_info * p_compaction_block) 
{
    gc_trace_block (p_compaction_block, " Sweeping this compaction block");
    assert (!p_compaction_block->is_to_block);
    TRACE2("gc.sweep", "sweeping block " << p_compaction_block);
    // Clear all the free areas computed during the previous GC
    
    gc_trace_block (p_compaction_block, " calling clear_block_free_areas in sweep_free_block.");
    clear_block_free_areas(p_compaction_block);

    // determine that there is exactly ONE large allocation area in this block
    p_compaction_block->num_free_areas_in_block = 1;

    p_compaction_block->current_alloc_area = -1;    

    p_compaction_block->curr_free = p_compaction_block->block_free_areas[0].area_base = GC_BLOCK_ALLOC_START(p_compaction_block);
    p_compaction_block->curr_ceiling = p_compaction_block->block_free_areas[0].area_ceiling = GC_BLOCK_CEILING(p_compaction_block);
    p_compaction_block->block_free_areas[0].area_size = GC_BLOCK_ALLOC_SIZE; // This is how we declare that the entire block is avaliable. (contains fo object)

    p_compaction_block->block_free_areas[0].has_been_zeroed = false;
    p_compaction_block->block_has_been_swept = true;
    p_compaction_block->block_free_bytes = GC_BLOCK_ALLOC_SIZE;
    p_compaction_block->block_used_bytes = 0;


    // These blocks dont get swept during allocation after mutators are restarted.
    // Means we need to explicitly clear their mark bit vectors, since they are fully useless until they get
    // repopulated with bits in the next GC cycle   
    clear_block_mark_bit_vector(p_compaction_block);


    return GC_BLOCK_ALLOC_SIZE;
}

#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

// Moved to object_placement.cpp
bool fuse_objects (GC_Thread *gc_thread,
                   Partial_Reveal_Object *p_obj,
                   void **next_obj_start_arg,
                   unsigned int *problem_locks);

void 
allocate_forwarding_pointers_for_compaction_live_objects(GC_Thread *gc_thread)
{
    unsigned int problem_locks = 0; // placement code does not deal with this.
    unsigned int num_to_blocks = 0;
    unsigned int num_from_blocks = 0;
    block_info *p_compaction_block = NULL;
    block_info *p_destination_block = NULL;
    void *next_obj_start = NULL;
    unsigned int num_forwarded = 0;
    
    if (cross_block_compaction) {
        // Initialize the first block into which live objects will start sliding into
        p_destination_block = gc_thread->_p_gc->iter_get_next_compaction_block_for_gc_thread(gc_thread->get_id(), NULL);
        if (p_destination_block == NULL) {
            // There is no block for compaction for this thread...no work to do in this phase....
            assert(gc_thread->_p_gc->get_block_for_sliding_compaction_allocation_pointer_computation(gc_thread->get_id(), NULL) == NULL);
            return;
        }
        p_destination_block->is_to_block = true;
        num_to_blocks++;
        gc_trace_block (p_destination_block, "is_to_block set to true.");
        next_obj_start = GC_BLOCK_ALLOC_START(p_destination_block);
    } 
    
    while ((p_compaction_block = gc_thread->_p_gc->get_block_for_sliding_compaction_allocation_pointer_computation(gc_thread->get_id(), p_compaction_block ))) {
        
        num_from_blocks++;

        unsigned int num_forwarded_in_this_block = 0;
        assert(p_compaction_block->in_nursery_p == true);
        
        if (!cross_block_compaction) {
            DIE("This no longer works.");
            // All live objects get slided to the allocated base of the current compaction block in this case
            next_obj_start = GC_BLOCK_ALLOC_START(p_compaction_block);
        } 
        
        Partial_Reveal_Object *p_prev_obj = NULL;
        gc_thread->_p_gc->init_live_object_iterator_for_block(p_compaction_block);
        Partial_Reveal_Object *p_obj = gc_thread->_p_gc->get_next_live_object_in_block(p_compaction_block);
        
        while (p_obj) {
           
            assert(!p_compaction_block->from_block_has_been_evacuated); // This block has not been evacuated yet.

            gc_trace (p_obj, "Examining object to see if it should be compacted");
            assert(GC_BLOCK_INFO(p_obj) == p_compaction_block);
            // Only objects in compaction blocks get forwarded
            assert(gc_thread->_p_gc->is_compaction_block(GC_BLOCK_INFO(p_obj)));
            // Assert that the list is pre-sorted
            assert(p_obj > p_prev_obj);
            unsigned int p_obj_size = get_object_size_bytes(p_obj);
            
            if (cross_block_compaction) {
                // Check for possible overflow in destination block where p_obj could go...
                if ((void *) ((POINTER_SIZE_INT) next_obj_start + p_obj_size) > GC_BLOCK_CEILING(p_destination_block)) {
                    TRACE2("gc.compact", "block " << p_destination_block << " is filled up to " << next_obj_start);
                    p_destination_block->block_used_bytes = 
                        (char*)next_obj_start - (char*)GC_BLOCK_ALLOC_START(p_destination_block);
                    p_destination_block->block_free_bytes = 
                        GC_BLOCK_ALLOC_SIZE - p_destination_block->block_used_bytes;
                    // this object will not fit in current destination block...get a new one..
                    p_destination_block = gc_thread->_p_gc->iter_get_next_compaction_block_for_gc_thread(gc_thread->get_id(), p_destination_block);
                    p_destination_block->is_to_block = true;        
                    num_to_blocks++;
                    gc_trace_block (p_destination_block, "is_to_block set to true.");
                    assert(p_destination_block);
                    next_obj_start = GC_BLOCK_ALLOC_START(p_destination_block);
                } 
                // next_obj_start is where p_obj will go and it surely belongs in p_destination_block
                assert(GC_BLOCK_INFO(next_obj_start) == p_destination_block);
            } 
            
                
                bool object_fused = false;
                
                if (!object_fused) {
                    if (object_info_is_not_zero(p_obj)) {
                        object_lock_save_info *obj_info = (object_lock_save_info *) STD_MALLOC(sizeof(object_lock_save_info));
                        assert(obj_info);
                        // Save obj_info
                        obj_info->obj_header = p_obj->get_obj_info();
                        // I need to keep track of the new after-slided address
                        obj_info->p_obj = (Partial_Reveal_Object *) next_obj_start;
                        gc_thread->insert_object_header_info_during_sliding_compaction(obj_info);
                        problem_locks++; // placement code does not deal with this so this is likely to be wrong.
                        gc_trace (p_obj, "Object being compacted needs obj_info preserved.");
                    } 

                        // Objects slide to lower addresses if at all regardless of cross-block or intra-block compaction
                        assert((POINTER_SIZE_INT)next_obj_start <= (POINTER_SIZE_INT) p_obj);

                    // clobber the header with the new address.
                    // This needs to be done atomically since some other thread may try to steal it to do colocation.!!!
                    bool success = false;
                    assert ((POINTER_SIZE_INT)next_obj_start + get_object_size_bytes(p_obj) < (POINTER_SIZE_INT)(GC_BLOCK_CEILING(next_obj_start))); // Check overflow.

                        p_obj->set_forwarding_pointer(next_obj_start);
                        success = true; // We always succeed if we are not colocating objects.


                    gc_trace (next_obj_start, " In allocate_forwarding_pointers_for_compaction_live_objects forwarding *to* this location. (vtable not yet legal) from " << p_obj);
                    gc_trace(p_obj, " was forwarded to " << next_obj_start);
                    if (verify_live_heap && success) {
                        add_repointed_info_for_thread(p_obj, (Partial_Reveal_Object *) next_obj_start, gc_thread->get_id());
                    }
                    num_forwarded++;
                    num_forwarded_in_this_block++;
                    if (success) {
                        // Compute the address of the next slid object
                        next_obj_start = (void *) ((POINTER_SIZE_INT) next_obj_start + p_obj_size);
                    }
                }

            p_prev_obj = p_obj;
            p_obj = gc_thread->_p_gc->get_next_live_object_in_block(p_compaction_block);
        } // end of while that gets the next object in a block, some could have been colocated.
        
    } // while 

    TRACE2("gc.compact", "block " << p_destination_block << " is filled up to " << next_obj_start);
    p_destination_block->block_used_bytes = (char*)next_obj_start - (char*)GC_BLOCK_ALLOC_START(p_destination_block);
    p_destination_block->block_free_bytes = GC_BLOCK_ALLOC_SIZE - p_destination_block->block_used_bytes;
    
    INFOW(gc_thread, "compacted " << num_from_blocks << " blocks to " << num_to_blocks << " blocks");
    INFOW(gc_thread, "allocated forwarding pointers for " << num_forwarded << " objects");

    spin_lock(&p_global_gc->lock_number_of_blocks);
    p_global_gc->number_of_to_compaction_blocks += num_to_blocks;
    p_global_gc->number_of_from_compaction_blocks += num_from_blocks;

    unsigned dst_index = p_destination_block->block_store_info_index;
    assert(dst_index == p_global_gc->_p_block_store->get_block_index(p_destination_block));

    unsigned &last = p_global_gc->_p_block_store->previous_compaction_last_destination_block;
    last = last < dst_index ? dst_index : last;

    spin_unlock(&p_global_gc->lock_number_of_blocks);
    
} // allocate_forwarding_pointers_for_compaction_live_objects

void
slide_cross_compact_live_objects_in_compaction_blocks(GC_Thread *gc_thread)
{
    block_info *p_compaction_block = NULL;
    block_info *p_destination_block = NULL;
    unsigned int num_slided = 0;
    POINTER_SIZE_INT bytes_freed_by_compaction_by_this_gc_thread = 0;
    unsigned int num_actually_slided = 0;
    Partial_Reveal_Object *p_prev_obj = NULL;

    // This function will be called only for cross-block compaction...
    assert(cross_block_compaction);
    p_destination_block = gc_thread->_p_gc->iter_get_next_compaction_block_for_gc_thread(gc_thread->get_id(), NULL);

    if (p_destination_block) {
        assert(p_destination_block->is_compaction_block);
    } else {
        // No work to do for this thread....???
        assert(gc_thread->_p_gc->get_block_for_slide_compact_live_objects(gc_thread->get_id(), NULL) == NULL);
        return;
    } 
    
    while ((p_compaction_block = gc_thread->_p_gc->get_block_for_slide_compact_live_objects(gc_thread->get_id(), p_compaction_block))) {
        gc_trace_block (p_compaction_block, "Memmoving objects out of this block.");
        assert(p_compaction_block->in_nursery_p == true);

        gc_thread->_p_gc->init_live_object_iterator_for_block(p_compaction_block);
        Partial_Reveal_Object *p_obj =  gc_thread->_p_gc->get_next_live_object_in_block(p_compaction_block);

        while (p_obj) {
            assert(GC_BLOCK_INFO(p_obj) == p_compaction_block);
           
            gc_trace(p_obj, " slide_compact_live_objects_in_compaction_blocks(): this object will be slided up...");
            // Grab the eventual location of p_obj
            Partial_Reveal_Object *p_new_obj = p_obj->get_forwarding_pointer();
            gc_trace(p_new_obj, " some object is being slid into this spot: slide_compact_live_objects_in_compaction_blocks():...");
            

            // We move the object to the forwarding pointer location *not* to the next available location....
            
            if (p_obj != p_new_obj) {

                    if ( !(GC_BLOCK_INFO(p_new_obj))->from_block_has_been_evacuated ) {
                        // We might be sliding inside the same block
                        assert ((GC_BLOCK_INFO(p_new_obj)) == (GC_BLOCK_INFO(p_obj)));
                        // Make sure we are sliding downward.
                        assert ((POINTER_SIZE_INT)p_new_obj < (POINTER_SIZE_INT)p_obj);

                }
                assert( (GC_BLOCK_INFO(p_new_obj))->is_to_block);              
                memmove(p_new_obj, p_obj, get_object_size_bytes(p_obj));
                gc_trace(p_new_obj, "This new object just memmoved into place.");
                num_actually_slided++;
            }
            num_slided++;
            // clear the object header since the use for the forwarding pointer is finished
            p_new_obj->set_obj_info(0);
            gc_trace(p_new_obj, " slide_compact_live_objects_in_compaction_blocks(): is clearing the just moved object obj_info field...");
            p_prev_obj = p_new_obj;
            p_obj = gc_thread->_p_gc->get_next_live_object_in_block(p_compaction_block);
            assert (p_obj != p_prev_obj);

        } // while

        // Signify that it is save to move objects into this block without concern.
        p_compaction_block->from_block_has_been_evacuated = true;
        gc_trace_block (p_compaction_block, "from_block_has_been_evacuated now set to true, block empty.");
    } // while

    block_info *p_block_to_set = NULL;
    // Sweep all blocks owned by this GC thread if it is a to block do not make it available otherwise declare it free.
    // HOW ABOUT SETTING THESE UP FOR ALLOCATION SWEEPS??!!!!!!!!!!!!!! this will reduce stop-the-world time!!
    while ((p_block_to_set = gc_thread->_p_gc->iter_get_next_compaction_block_for_gc_thread(gc_thread->get_id(), p_block_to_set))) {
        if (p_block_to_set->is_to_block) {
            sweep_slide_compacted_block(p_block_to_set, NULL);
        } else {
            bytes_freed_by_compaction_by_this_gc_thread += sweep_free_block(p_block_to_set); // is this a hotspot??
        }
    }

    INFOW(gc_thread, "slide compacted "
        << num_slided << " objects, num_actually_slided = " << num_actually_slided);
    INFOW(gc_thread, "freed "
        << (unsigned int) (bytes_freed_by_compaction_by_this_gc_thread / 1024)
        << " bytes by compaction");

} // slide_cross_compact_live_objects_in_compaction_block


// Each thread restores headers of objects that it hijacked in an earlier phase....
void
restore_hijacked_object_headers(GC_Thread *gc_thread)
{
    object_lock_save_info *one_header = gc_thread->object_headers_to_restore_at_end_of_sliding_compaction();
    while(one_header) {
        Partial_Reveal_Object *p_obj = one_header->p_obj;
        // This is the current address of the slided object
        assert(p_obj);
        Obj_Info_Type orig_header_val = one_header->obj_header;
        // better be non-zero, otherwise there is nothing to restore in the first place
        assert(orig_header_val);
        gc_trace(p_obj, " slide_compact_live_objects_in_compaction_blocks(): is restoring the just moved object obj_info field...");
        p_obj->set_obj_info(orig_header_val);
        object_lock_save_info *save = one_header->p_next;
        STD_FREE(one_header);
        one_header = save;
    }
} // restore_hijacked_object_headers

#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
