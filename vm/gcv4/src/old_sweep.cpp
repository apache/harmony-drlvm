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

/**
 * @file 
 * Sweep the whole heap during stop-the-world GC on parallel threads.
 */


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
 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool get_num_consecutive_similar_bits(uint8 *, unsigned int, unsigned int *, uint8 *);

//////////////////////////////////  S W E E P  /////////////////////////////////////////////////////////////////////////////////////

void
Garbage_Collector::prepare_to_sweep_heap() 
{
    // Divide all chunks up for the sweep.
    // Also, give each thread some marks to clear

    assert(_free_chunks_end_index > 0);
    int num_chunks_per_thread = (_free_chunks_end_index + 1) / get_num_worker_threads();
    int num_extra_chunks_for_last_thread = (_free_chunks_end_index + 1) % get_num_worker_threads();
    assert(num_chunks_per_thread > 0);

    for (unsigned int i = 0, chunks_sweep_offset = 0; i < get_num_worker_threads(); i++) {
    
        assert(_gc_threads[i]->get_sweep_start_index() == -1);
        assert(_gc_threads[i]->get_num_chunks_to_sweep() == -1);
            
        _gc_threads[i]->set_sweep_start_index(chunks_sweep_offset);
        _gc_threads[i]->set_num_chunks_to_sweep(num_chunks_per_thread);

        chunks_sweep_offset += num_chunks_per_thread;
    }

    if (num_extra_chunks_for_last_thread) {
        _gc_threads[get_num_worker_threads() -1]->add_to_num_chunks_to_sweep(num_extra_chunks_for_last_thread);
    }
}


unsigned int
Garbage_Collector::sweep_heap(GC_Thread *gc_thread)
{
    unsigned int num_blocks_swept = 0;
    unsigned int num_bytes_recovered_for_allocation = 0;

    unsigned int num_active_chunks = 0;

    int start_chunk_index = gc_thread->get_sweep_start_index();
    int end_chunk_index = start_chunk_index + gc_thread->get_num_chunks_to_sweep() - 1;

    // Process each chunk separately
    for (int chunk_index = start_chunk_index; chunk_index <= end_chunk_index; chunk_index++) {

        // continue if empty chunk
        if (_gc_chunks[chunk_index].chunk == NULL) {
            continue;
        }

        unsigned int num_blocks_in_chunk = 0;
        unsigned int num_free_bytes_in_chunk = 0;
        unsigned int num_free_areas_in_chunk = 0;

        // Sweep all blocks in this chunk
        block_info *block = _gc_chunks[chunk_index].chunk;

        bool active_chunk = (block->nursery_status == active_nursery);
        if (active_chunk) {
            assert(_gc_chunks[chunk_index].free_chunk == NULL);
            num_active_chunks++;
        }
        
        bool free_chunk = (block->nursery_status == free_nursery);
        if (free_chunk) {
            assert(_gc_chunks[chunk_index].chunk == _gc_chunks[chunk_index].free_chunk);
        }
        
        // I dont need to sweep a free chunk, DO I??! --- TO DO
        while (block) {
            num_blocks_in_chunk++;
 
            // Sweep only non-compacted blocks
            if (gc_thread->is_compaction_turned_on_during_this_gc() && is_compaction_block(block)) {
                // this automatically gets swept....nothing to do...
                assert(block->block_has_been_swept == true);
            } else {
                num_bytes_recovered_for_allocation += sweep_one_block(block);
            }
        
            num_free_areas_in_chunk += block->num_free_areas_in_block;
            num_free_bytes_in_chunk += block->block_free_bytes;

            if (active_chunk) {
                // each block in an active chunk better be active; since they are recycled together
                assert(block->nursery_status == active_nursery);

            } else if (free_chunk) {
                assert(block->nursery_status == free_nursery);

            } else {
                // It better be spent in this case. We will convert it to FREE now that it has been swept.
                assert(block->nursery_status == spent_nursery);
                block->nursery_status = free_nursery;
            }

            num_blocks_swept++;
        
            // Move to next block in current chunk
            block = block->next_free_block;
        }
#ifdef _DEBUG
        if (num_blocks_in_chunk && num_free_areas_in_chunk) {
            gc_thread->set_chunk_average_number_of_free_areas(chunk_index - start_chunk_index, num_free_areas_in_chunk / num_blocks_in_chunk );
            gc_thread->set_chunk_average_size_per_free_area(chunk_index - start_chunk_index, num_free_bytes_in_chunk / num_free_areas_in_chunk );
        }
#endif
        // Have swept the whole chunk....Now...restore the processed chunk IF it is not held by some thread
        if (active_chunk == false) {
            _gc_chunks[chunk_index].free_chunk = _gc_chunks[chunk_index].chunk;
        }

    } // for

    
    return num_bytes_recovered_for_allocation;
} // sweep_heap
