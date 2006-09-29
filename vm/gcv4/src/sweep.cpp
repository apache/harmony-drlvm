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
#include "mark.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void get_next_set_bit(set_bit_search_info *info);

extern bool get_num_consecutive_similar_bits(uint8 *, unsigned int, unsigned int *, uint8 *);

extern void verify_get_next_set_bit_code(set_bit_search_info *);

//////////////////////////////////  S W E E P  /////////////////////////////////////////////////////////////////////////////////////



unsigned int 
Garbage_Collector::sweep_single_object_blocks(block_info * UNREF blocks)
{
    block_info *this_block = _single_object_blocks;
    block_info *block = this_block;
    block_info *new_single_object_blocks = NULL;

    unsigned int reclaimed = 0;

    // ***SOB LOOKUP*** We clear the _blocks_in_block_store[mumble].is_single_object_block field in the link_free_blocks routine..
    
    while (block != NULL) {
        this_block = block;
        block = block->next_free_block;   // Get the next block before we destroy this block.
        assert (this_block->is_single_object_block);

        // SOB blocks never get compacted
        assert(!is_compaction_block(this_block));

        Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)GC_BLOCK_ALLOC_START(this_block);

        if (is_object_marked(p_obj) == false) {
            int blocks_reclaimed = _p_block_store->link_free_blocks (this_block);
            reclaimed += blocks_reclaimed * GC_BLOCK_SIZE_BYTES;
        } else {
            this_block->next_free_block = new_single_object_blocks;
            new_single_object_blocks = this_block;
        }
    }
    _single_object_blocks = new_single_object_blocks;
    return reclaimed;
}




void
Garbage_Collector::prepare_chunks_for_sweeps_during_allocation(bool compaction_has_been_done_this_gc)
{
    unsigned int num_active_chunks = 0;

     // GC can create new chunks so adjust _free_chunk_end_index as needed.
     while (_gc_chunks[_free_chunks_end_index].chunk != NULL) {
         _free_chunks_end_index++;
    }
    for (int chunk_index = 0; chunk_index <= _free_chunks_end_index; chunk_index++) {

        // chunks can be null if they have been returned to the block store previously
        if (_gc_chunks[chunk_index].chunk) {

            block_info *block = _gc_chunks[chunk_index].chunk;

            bool UNUSED free_chunk = (block->nursery_status == free_nursery);
            bool active_chunk = (block->nursery_status == active_nursery);
            bool UNUSED spent_chunk = (block->nursery_status == spent_nursery);

            assert(free_chunk || active_chunk || spent_chunk);
            nursery_state UNUSED chunk_state = block->nursery_status;
            while (block) {

                // Each block in a chunk should have the same status 
                assert(block->nursery_status == chunk_state);

                // Should have been swept in the mutator execution cycle before the current ongoing GC
                // ONLY IF IT WAS SPENT AND RETURNED TO THE SYSTEM
                if (block->nursery_status == spent_nursery) {
                    // Convert it to be allocatable since we will sweep only later
                    block->nursery_status = free_nursery;
                }

                // If any compaction has happened in this GC
                if (compaction_has_been_done_this_gc && is_compaction_block(block)){
                    // Compacted blocks will have already been swept.
                    assert(block->block_has_been_swept == true);
                } else {
                    block->block_has_been_swept = false;
                }
                block = block->next_free_block;
            }

            // Make the chunk allocatable by the mutators by rehooking free_chunk to chunk
            // ONLY IF IT IS NOT OWNED CURRENTLY BY SOME THREAD
            if (!active_chunk) {
                _gc_chunks[chunk_index].free_chunk = _gc_chunks[chunk_index].chunk;
            }

        } else {
            assert(_gc_chunks[chunk_index].free_chunk == NULL);
        }
        
    } // for

#ifdef _DEBUG
    // Make sure that all the other chunk entries are empty.
    for (unsigned int j = _free_chunks_end_index + 1; j < _gc_chunks.size(); j++) {
        assert(_gc_chunks[j].chunk == NULL);
        assert(_gc_chunks[j].free_chunk == NULL);
    }
#endif // _DEBUG

    INFO2("gc.chunks", "num_active_chunks discovered by GC is " << num_active_chunks);
}


// This is needed because some blocks may not get swept during  mutuator execution, but their
// mark bit vectors need to be cleared before this GC can start
void
Garbage_Collector::clear_all_mark_bit_vectors_of_unswept_blocks()
{
    unsigned int num_mark_bit_vectors_cleared = 0;

    for (int chunk_index = 0; chunk_index <= _free_chunks_end_index; chunk_index++) {
        // chunks can be null if they have been returned to the block store previously
        if (_gc_chunks[chunk_index].chunk) {
            block_info *block = _gc_chunks[chunk_index].chunk;
            while (block) {
                assert(block->in_nursery_p);
                if (block->block_has_been_swept == false) {
                    clear_block_mark_bit_vector(block);
                    num_mark_bit_vectors_cleared++;
                }
                block = block->next_free_block;
            }
        } 
    } // for

    if (_los_blocks) {
        block_info *block = _los_blocks;
        while (block) {
            assert(block->in_los_p);
            clear_block_mark_bit_vector(block);
            block = block->next_free_block;
        }
    }

    if (_single_object_blocks) {
        block_info *block = _single_object_blocks;
        while (block) {
            assert(block->is_single_object_block);
            clear_block_mark_bit_vector(block);
            block = block->next_free_block;
        }
    }

    INFO2("gc.mark", "num_mark_bit_vectors_cleared before GC = " << num_mark_bit_vectors_cleared);
}



static inline unsigned int
get_live_object_size_bytes_at_mark_byte_and_bit_index(block_info *block, uint8 *p_byte, unsigned int bit_index)
{
    uint8 *mark_vector_base = &(block->mark_bit_vector[0]);
    unsigned int obj_bit_index = (unsigned int) ((p_byte - mark_vector_base) * GC_NUM_BITS_PER_BYTE + bit_index);
    Partial_Reveal_Object *p_live_obj = (Partial_Reveal_Object *)
            ((POINTER_SIZE_INT)block + GC_BLOCK_INFO_SIZE_BYTES +  (obj_bit_index * GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES));
    unsigned int sz = get_object_size_bytes(p_live_obj);
    return sz;
}



unsigned int
Garbage_Collector::sweep_one_block(block_info *block) 
{
    unsigned int block_final_free_areas_bytes = 0;
    set_bit_search_info info;

    // Clear all the free areas computed during the previous GC
    clear_block_free_areas(block);
    free_area *areas = block->block_free_areas;
    assert(areas);
    unsigned int num_min_free_bits_for_free_area = (GC_MIN_FREE_AREA_SIZE / GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES);
    unsigned int curr_area_index = 0;


    uint8 *mark_vector_base = &(block->mark_bit_vector[0]);
    uint8 *p_ceiling = (uint8 *) ((POINTER_SIZE_INT)mark_vector_base + GC_SIZEOF_MARK_BIT_VECTOR_IN_BYTES);

    // This keeps track of the previous live object if any that was encountered...
    uint8 *p_last_live_byte = NULL;
    unsigned int last_live_byte_bit_index = 0;

    uint8 *p_byte = mark_vector_base;   // start search at the base of the mark bit vector
    unsigned bit_index = 0;
    info.p_ceil_byte = p_ceiling;       // stop searching when we get close the the end of the vector

    while (true) {  // Keep looping until we have linearly reached the end of the block's mark table...

        info.p_start_byte = p_byte;
        info.start_bit_index = bit_index;
        // DO the leg work of finding the next set set bit...from the current position
        get_next_set_bit(&info);
        if (verify_gc) {
            verify_get_next_set_bit_code(&info);
        }  
        uint8 *p_byte_with_some_bit_set = info.p_non_zero_byte;
        unsigned int set_bit_index = info.bit_set_index;
        assert(set_bit_index < GC_NUM_BITS_PER_BYTE);
        // if we found a set bit in some byte downstream, it better be within range and really "set".
        assert((p_byte_with_some_bit_set == NULL) || 
            ((p_byte_with_some_bit_set >= p_byte)) && (p_byte_with_some_bit_set < p_ceiling) && ((*p_byte_with_some_bit_set & (1 << set_bit_index)) != 0));

        unsigned int num_free_bits = 0;

        if (p_byte_with_some_bit_set != NULL) {
            // Some live object was found....make sure this is a valid object and get its size
#ifdef _DEBUG
            unsigned int size = get_live_object_size_bytes_at_mark_byte_and_bit_index(block, p_byte_with_some_bit_set, set_bit_index);
            assert(size >= 8);
#endif // _DEBUG
            // Calculate the size of the free bits initially
            num_free_bits = (unsigned int) ((p_byte_with_some_bit_set - p_byte) * GC_NUM_BITS_PER_BYTE + (set_bit_index) - (bit_index));
        } else {
            // this is the last free region in this block....
            num_free_bits = (unsigned int) ((p_ceiling - p_byte) * GC_NUM_BITS_PER_BYTE - (bit_index));
        }

        if (num_free_bits >= num_min_free_bits_for_free_area) {
            // We have chanced upon a fairly large free area
            if (p_last_live_byte != NULL) {
                // Get the size of the last live object (the one preceding the current free area) and jump 
                // ahead by that many bytes to get to the REAL start of this free region
                unsigned int sz = get_live_object_size_bytes_at_mark_byte_and_bit_index(block, p_last_live_byte, last_live_byte_bit_index);
                assert(sz >= 8);
                unsigned int rem_obj_sz_bits = sz / GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES - 1;  // 1 bit already included...
                // adjust the free area size
                num_free_bits -= rem_obj_sz_bits ;                      
                // roll ahead the byte and bit index beyond the live object..
                p_byte += (rem_obj_sz_bits / GC_NUM_BITS_PER_BYTE);     
                bit_index += (rem_obj_sz_bits % GC_NUM_BITS_PER_BYTE);
                if (bit_index >= GC_NUM_BITS_PER_BYTE) {
                    p_byte++;
                    bit_index -= GC_NUM_BITS_PER_BYTE;
                } 
            }
    
            if (num_free_bits >= num_min_free_bits_for_free_area) {
                // FREE region found -- it begins at (p_byte, bit_index)......
                assert((*p_byte & (1 << bit_index)) == 0);
                unsigned int free_region_bit_index = (unsigned int) ((p_byte - mark_vector_base) * GC_NUM_BITS_PER_BYTE + bit_index);
                void *area_base = block_address_from_mark_bit_index(block, free_region_bit_index);
                assert(area_base >= GC_BLOCK_ALLOC_START(block));
                unsigned int area_size = num_free_bits * GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES;
                assert(area_size >= GC_MIN_FREE_AREA_SIZE);
                areas[curr_area_index].area_base = area_base;
                assert(areas[curr_area_index].area_base < (void *) ((POINTER_SIZE_INT) block + GC_BLOCK_SIZE_BYTES));
                areas[curr_area_index].area_ceiling = (void *) ((POINTER_SIZE_INT)area_base + area_size - 1);
                assert(areas[curr_area_index].area_ceiling < (void *) ((POINTER_SIZE_INT) block + GC_BLOCK_SIZE_BYTES));
                areas[curr_area_index].area_size = area_size;
                areas[curr_area_index].has_been_zeroed = false;
                block_final_free_areas_bytes += areas[curr_area_index].area_size;
                curr_area_index++;
            }
        }

        if (p_byte_with_some_bit_set) {
            // Record the live found at the end of the free region....
            p_last_live_byte = p_byte_with_some_bit_set;
            last_live_byte_bit_index = set_bit_index;
            // Roll ahead the byte and bit pointers ahead of the live that was found to be ready for the new search...(iter)
            p_byte = p_byte_with_some_bit_set;
            bit_index = set_bit_index + 1;
            if (bit_index == GC_NUM_BITS_PER_BYTE) {
                bit_index = 0;
                p_byte++;
            } 
        } else {
            // we went off the edge 
            break;  // DONE
        }

    } // while

    // Done with processing...finish up with the block
    if (curr_area_index == 0) {
        // Didnt find even one free region
        block->num_free_areas_in_block = 0;

        block->current_alloc_area = -1;
        block->curr_free = NULL;
        block->curr_ceiling = NULL;
    } else {
        block->num_free_areas_in_block = curr_area_index;
        // Start allocating at the first one.

        block->current_alloc_area = -1;
        block->curr_free = block->block_free_areas[0].area_base;
        block->curr_ceiling = block->block_free_areas[0].area_ceiling;
    }

    block->block_free_bytes = block_final_free_areas_bytes;

    // Clear the mark bit vector since we have determined allocation regions already
    clear_block_mark_bit_vector(block);

    return block_final_free_areas_bytes;
}
