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
 * @author Salikh Zakirov
 * @version $Revision: 1.1.2.2.4.4 $
 */  

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>

// VM interface header files
#include <apr_atomic.h>
#include <apr_file_io.h>
#include "port_malloc.h"
#include "port_sysinfo.h"
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
#include "gc_utils.h"

// TODO: possible optimization: consider heap resize for large object allocation
// before starting collection
void Garbage_Collector::consider_heap_resize(int size_failed) {

    unsigned current_blocks = _p_block_store->get_num_total_blocks_in_block_store();
    unsigned max_blocks = _p_block_store->get_num_max_blocks_in_block_store();
    // "free" means free in block store
    // "empty" means allocated for SmOS, but having no objects in it
    unsigned unused_blocks = _p_block_store->get_num_free_blocks_in_block_store()
                + _p_block_store->get_num_empty_smos_blocks_in_block_store();

    int expand_by = 0;

    // NB: the amount of unused blocks will be around 0 with fixed collector,
    // so this is likely to constantly give false positive and resize the heap.
    float occupancy = (float)(current_blocks - unused_blocks) / current_blocks;
    LOG2("gc.live", "heap occupancy estimated as " << occupancy);

    if (occupancy > 0.5) {

        // We want heap size to be no less than 2x live set
        unsigned target_blocks = (unsigned)(occupancy * current_blocks * 2);

        if (target_blocks > max_blocks) target_blocks = max_blocks;
        if (target_blocks > current_blocks) {
            expand_by = target_blocks - current_blocks;
            INFO("expanding heap from "
                    << mb(current_blocks * GC_BLOCK_SIZE_BYTES)
                    << " Mb to " << mb(target_blocks * GC_BLOCK_SIZE_BYTES) 
                    << " Mb because of occupancy " << occupancy);
        }
    }

    if (number_of_user_threads > _free_chunks_end_index) {
        // We want each thread have at least 2 chunks
        unsigned target_blocks = number_of_user_threads * 2 * chunk_size_hint();
        if (target_blocks > max_blocks) target_blocks = max_blocks;
        if (target_blocks > expand_by + current_blocks) {
            expand_by = target_blocks - current_blocks;
            INFO("expanding heap from "
                    << mb(current_blocks * GC_BLOCK_SIZE_BYTES)
                    << " Mb to " << mb(target_blocks * GC_BLOCK_SIZE_BYTES) 
                    << " Mb because of " << number_of_user_threads << " threads");
        }
    }

    if (size_failed > GC_BLOCK_ALLOC_SIZE) {
        // how many blocks we need?
        int needed_blocks = GC_NUM_BLOCKS_PER_LARGE_OBJECT(size_failed);
        // can we satisy the request now?
        if (!_p_block_store->block_store_can_satisfy_request(needed_blocks)) {
            if (needed_blocks + current_blocks > max_blocks) {
                assert(current_blocks <= max_blocks);
                needed_blocks = max_blocks - current_blocks;
            }
            if (needed_blocks > expand_by) {
                expand_by = needed_blocks;
                unsigned target_blocks = current_blocks + needed_blocks;
                INFO("expanding heap from "
                    << mb(current_blocks * GC_BLOCK_SIZE_BYTES)
                    << " Mb to " << mb(target_blocks * GC_BLOCK_SIZE_BYTES) 
                    << " Mb because of large allocation of " << size_failed << " bytes");
            }
        }
    }

    if (expand_by > 0) {
        assert(current_blocks + expand_by <= max_blocks);
        _p_block_store->expand_heap(expand_by);
    }
}
