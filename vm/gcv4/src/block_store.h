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


#ifndef _block_store_H_
#define _block_store_H_

#include "port_vmem.h"
// From gc_globals.cpp
#include "gc_header.h"
#include "gc_thread.h"
#include "gc_globals.h"
#include "gc_v4.h"
#include "compressed_references.h"
#include <set>
#include <vector>

typedef struct {
    
    //Will be turned on by the GC if this block needs to be sliding compacted.
    // If the block is pinned this had better not be set.
    bool is_compaction_block;
    bool is_single_object_block;

    // Set to true once this block has been evacuated of all of its "from" objects.
    // bool from_block_has_been_evacuated; This is maintained in the block->from_block_has_been_evacuated    

    // Used during mark/scan when slots are inserted on a per block basis
    Remembered_Set **per_thread_slots_into_compaction_block;

#ifdef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
    // Used for base sorting of objects per compacted block
    live_object **per_thread_live_objects_in_compaction_block;
    unsigned int *num_per_thread_live_objects_in_compaction_block;
    Partial_Reveal_Object **all_live_objects_in_block_fully_sorted;
#else 
    unsigned int bit_index_into_all_lives_in_block;
    
#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
    
    unsigned int total_live_objects_in_this_block;
    
    // Is the block free ? i.e. can the block store hand it out to a requesting thread??
    bool block_is_free;
    
    // If this is a super-block, then it means the number of sub-blocks that it holds
    unsigned int number_of_blocks;
    
    // Points to the actual heap block that it points to
    block_info *block;
        
    // Clobbered when blocks are handed out during parallel allocation pointer computation phase
    block_info *block_for_sliding_compaction_allocation_pointer_computation;
    
    // Clobbered when blocks are handed out during parallel slot updates phase
    block_info *block_for_sliding_compaction_slot_updates;
    
    // Clobbered when blocks are handed out during parallel object slides phase
    block_info *block_for_sliding_compaction_object_slides;
    
} block_store_info;





class Block_Store {
public:
    
    Block_Store(POINTER_SIZE_INT initial_heap_size, POINTER_SIZE_INT final_heap_size, unsigned int block_size_bytes);
    
    virtual ~Block_Store();
    
    block_info *p_get_multi_block(unsigned int size, bool for_chunks_pool);

    // used to save pointers to allocated memory for freeing in destructor
    port_vmem_t *allocated_memory; 
    apr_pool_t *aux_pool;
    
    int link_free_blocks (block_info *freed_block);
    
    void coalesce_free_blocks();

    void initialize_block(unsigned index, void* block_address);
    
    inline unsigned int get_num_free_blocks_in_block_store() {
        return _num_free_blocks;
    }
    
    inline unsigned int get_num_total_blocks_in_block_store() {
        return _number_of_blocks_in_heap;
    }

    inline unsigned int get_num_max_blocks_in_block_store() {
        return max_blocks_in_heap;
    }

    /// Returns number of SmOS blocks known to contain no live objects.
    unsigned get_num_empty_smos_blocks_in_block_store();

    int get_compaction_increment_size() {
        unsigned size = _number_of_blocks_in_heap / _heap_compaction_ratio;
        if (size >= min_compaction_increment_size) {
            return size;
        }
        else if (min_compaction_increment_size <= _number_of_blocks_in_heap) {
            return min_compaction_increment_size;
        }
        else {
            return _number_of_blocks_in_heap;
        }
    }

    unsigned get_block_index(block_info *block) {
        return (unsigned int) (((POINTER_SIZE_INT) block - (POINTER_SIZE_INT) _p_heap_base) >> GC_BLOCK_SHIFT_COUNT);
    }

    /// cyclically shifts the _compaction_blocks_high_index
    /// and _compaction_blocks_low_index
    void roll_forward_compaction_limits();
    
    void characterize_blocks_in_heap ();
    
    unsigned int get_num_worker_threads();
    
    block_info *p_get_new_block(bool);
    
    inline unsigned int get_block_store_low_watermark_free_blocks() {
        unsigned int reserve;
        
            // 0.5% of the total heap blocks....but at least 16 blocks ** was .005 ***
            reserve = (unsigned int) (.005 * (float) _number_of_blocks_in_heap); 

        return (reserve >= 16) ? reserve : 16;
    }
    
    inline unsigned int get_block_size_bytes() {
        return _block_size_bytes;
    }
    
    bool block_store_can_satisfy_request(unsigned int);

    /// Commits blocks as necessary to have target number of blocks
    bool commit_blocks(unsigned target_number_of_blocks);

    /// Returns true if it succeeded to commit blocks [0..index] inclusively
    /// or the blocks were already committed, and returns false otherwise.
    bool ensure_committed(unsigned index) {
        unsigned target = index + _blocks_in_block_store[index].number_of_blocks;
        assert(target > index);
        assert(target <= max_blocks_in_heap);
        if (target <= number_of_blocks_committed) {
            return true;
        }
        else {
            return commit_blocks(target);
        }
    }
    
    //////////////////// S L I D I N G   C O M P A C T I O N ///////////////////////////////////
    
    inline void set_compaction_type_for_this_gc(gc_compaction_type comp_type) {
        _compaction_type_for_this_gc = comp_type;
    }
    
    void determine_compaction_areas_for_this_gc(std::set<block_info *> &pinned_blocks);
    
    void reset_compaction_areas_after_this_gc();
    
    inline void set_is_compaction_block_and_all_lives(block_info *block) {
#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
        unsigned int i = get_block_index(block);
        
        assert(i == block->block_store_info_index);
        
        _blocks_in_block_store[i].is_compaction_block = true;
        
#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK 
    }
    
    inline bool is_compaction_block(block_info *block) {
#ifdef _DEBUG
        unsigned int block_store_block_number = get_block_index(block);
        assert(block_store_block_number == block->block_store_info_index);
        assert(_blocks_in_block_store[block_store_block_number].is_compaction_block == block->is_compaction_block);
#endif // _DEBUG
        return block->is_compaction_block;
    }
    
#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
    
    void init_live_object_iterator_for_block(block_info *block);
    
    Partial_Reveal_Object *get_next_live_object_in_block(block_info *block);
    
#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
    
    
    inline void add_slot_to_compaction_block(Slot p_slot, block_info *p_obj_block, unsigned int gc_thread_id) {
        assert(GC_BLOCK_INFO(p_slot.dereference()) == p_obj_block);
        if (p_slot.is_null()) {
            return;
        }
        unsigned int block_store_block_number = get_block_index(p_obj_block);
        assert(_blocks_in_block_store[block_store_block_number].is_compaction_block);   // we are collecting slots into compacted block
        Remembered_Set *slots_list = 
            _blocks_in_block_store[block_store_block_number].per_thread_slots_into_compaction_block[gc_thread_id];
        if (slots_list == NULL) {
            _blocks_in_block_store[block_store_block_number].per_thread_slots_into_compaction_block[gc_thread_id] = slots_list = 
                new Remembered_Set();
        }
        slots_list->add_entry(p_slot);
    }

    inline unsigned int get_total_live_objects_in_this_block(block_info *block) {
        unsigned int block_store_block_number = get_block_index(block);
        assert(block_store_block_number == block->block_store_info_index);
        return _blocks_in_block_store[block_store_block_number].total_live_objects_in_this_block;
    }

    block_info *get_block_for_sliding_compaction_allocation_pointer_computation(unsigned int, block_info *);

    block_info *get_block_for_fix_slots_to_compaction_live_objects(unsigned int, block_info *);

    Remembered_Set *get_slots_into_compaction_block(block_info *);

    block_info *get_block_for_slide_compact_live_objects(unsigned int, block_info *);

    block_info *iter_get_next_compaction_block_for_gc_thread(unsigned int, block_info *);

    //////////////////////////////// V E R I F I C A T I O N //////////////////////////////////////////////

    void verify_no_duplicate_slots_into_compaction_areas();

    bool block_is_invalid_or_free_in_block_store(block_info *block) {
        unsigned int block_store_block_number = get_block_index(block);

        if (block_store_block_number != block->block_store_info_index) {
            // This isn't even a valid block so return that it is free.
            return true;
        }
        return _blocks_in_block_store[block_store_block_number].block_is_free;
    }
    
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void dump_heap(int );
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    
    inline void *get_gc_heap_base_address() {
        return _p_heap_base;
    } 
    
    inline void *get_gc_heap_ceiling_address() {
        return _p_heap_ceiling;
    }
    
    inline bool is_single_object_block(unsigned int i) {
        return _blocks_in_block_store[i].is_single_object_block;
    }

    block_info * get_block_info(unsigned int block_store_info_index) {
        return _blocks_in_block_store[block_store_info_index].block;
    }

    inline unsigned int get_number_of_blocks_in_block(unsigned int i) {
        return _blocks_in_block_store[i].number_of_blocks;        
    }
    // Block iterator code, 
    // This can be used in a stop the world setting to iterate through all ther block sequentially.
    void init_block_iterator() {
        current_block_index = 0;
    }

    // This retrieves the next block after init_block_iterator is called or NULL if there are no more blocks.
    block_store_info *get_next_block() {
        if (current_block_index >= _number_of_blocks_in_heap) {
            return NULL;
        }
        block_store_info *store_info = &_blocks_in_block_store[current_block_index];
        current_block_index += store_info->number_of_blocks;
        return store_info;
    }

    // iterator for each live object in heap
    void init_live_objects_in_heap_iterator();
    Partial_Reveal_Object* get_next_live_object();

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    bool live_object_iterator_mark_bits_valid;

    void expand_heap(int number_of_blocks_to_commit);


    unsigned int previous_compaction_last_destination_block;
    
private:
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool init_heap(POINTER_SIZE_INT size);

    void get_compaction_limits_based_on_compaction_type_for_this_gc(unsigned int *, unsigned int *);
    
    void _initialize_block_tables(void *p_start, POINTER_SIZE_INT initial_heap_size, POINTER_SIZE_INT max_heap_size);
    
    void set_free_status_for_all_sub_blocks_in_block(unsigned int, bool);
    
    //
    // Get relevant information about the underlying machine/os
    // (ex: page size, number of active processors, etc..)
    //
    void _get_system_info();
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // data members
    
    unsigned int _block_size_bytes;
    
    unsigned int max_blocks_in_heap;            // maximum (-Xmx) number of blocks in the heap
    unsigned int _number_of_blocks_in_heap;     // current number of blocks in the heap
    unsigned int number_of_blocks_committed;    // current number of committed blocks
    
    std::vector<block_store_info > _blocks_in_block_store;

    unsigned int _num_free_blocks;
    
    unsigned int _free_block_search_hint;
    
    void *_p_heap_base;
    
    void *_p_heap_ceiling;
    
    //
    // The machine (OS) page size.
    //
    unsigned int _machine_page_size_bytes;
    //
    // The ENABLED processors on this machine.
    //
    unsigned int _number_of_active_processors;
    
    
    ////////// C O M P A C T I O N  S T A T E ////////////////////////////////////////////////////////////////////////////////////
    
    unsigned int _heap_compaction_ratio;
    unsigned min_compaction_increment_size;
    
    gc_compaction_type _compaction_type_for_this_gc;
    
    unsigned int _compaction_blocks_low_index;
    unsigned int _compaction_blocks_high_index;
    
    // NEW for cross-block compaction
    unsigned int *_per_gc_thread_compaction_blocks_low_index;
    unsigned int *_per_gc_thread_compaction_blocks_high_index;

    // Block iterator code
    unsigned int current_block_index;

    // Live object iterator
    block_info *live_object_iterator_current_block;
};

#endif // _Block_Store_H_
