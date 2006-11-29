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
 * The data and methods of the garbage collector instance.
 */

#ifndef _GARBAGE_COLLECTOR_H
#define _GARBAGE_COLLECTOR_H

#include <assert.h>

// Portlib interface header files
#include "port_atomic.h"
#include "open/hythread_ext.h"

// GC header files
#include "hash_table.h"
#include "object_list.h"
#include "remembered_set.h"
#include "block_store.h"
#include "work_packet_manager.h"
#include "vector_set.h"
#include <list>
#include <stack>
#include "tl/memory_pool.h"

typedef std::stack<Partial_Reveal_Object*> MarkStack;
typedef vector_set<Partial_Reveal_Object**> RootSet;

class Garbage_Collector
{
public:

    Garbage_Collector(POINTER_SIZE_INT, POINTER_SIZE_INT, unsigned int);

    virtual ~Garbage_Collector();

    void gc_v4_init();

    int gc_add_fresh_chunks(unsigned int);

    /// Considers statistics and makes a decision to resize the heap.
    void consider_heap_resize(int size_failed);

    block_info *p_cycle_chunk(block_info *);

    void reclaim_full_heap(unsigned int size_failed, bool full_compaction);

    void gc_internal_add_root_set_entry(Partial_Reveal_Object **);

    void gc_internal_add_weak_root_set_entry(Partial_Reveal_Object **,
        Boolean is_short_weak);

    void gc_internal_block_contains_pinned_root(block_info * p_block_info)
    {
        // Add this block to the set of blocks that are pinned for this GC.
        m_pinned_blocks.insert(p_block_info);
    }

    Partial_Reveal_Object **get_fresh_root_to_trace(unsigned &hint);
    Partial_Reveal_Object **get_root_atomic(unsigned index);

    inline unsigned int get_num_worker_threads()
    {
        return _p_block_store->get_num_worker_threads();
    }

    inline unsigned int get_gc_num()
    {
        return _gc_num;
    }

    apr_time_t get_time_since_last_gc()
    {
        return (apr_time_now() - _gc_end_time);
    }

/////////////////////////////////////////////////////////////
    // NOT DONE!!!!
    // NEED TO HIDE THE LOS from allocation.cpp -- need to make it private with an interface
    volatile unsigned int _los_lock;
    // LOS
    block_info *_los_blocks;
    block_info *get_new_los_block();
/////////////////////////////////////////////////////////////


    Partial_Reveal_Object *create_single_object_blocks(unsigned,
        Allocation_Handle);

    inline void coalesce_free_blocks()
    {
        _p_block_store->coalesce_free_blocks();
    }

/////////////////////////////////////////////////////////////

    unsigned int sweep_one_block(block_info *);

/////////////////////////////////////////////////////////////

    unsigned int sweep_heap(GC_Thread *);

/////////////////////////////////////////////////////////////////////////////////////////////////////

    void setup_mark_scan_pools();

    // A particular GC thread wants to do mark/scan work....how can the GC assist it?!
    void mark_scan_pools(GC_Thread *);
    void mark_scan_roots(GC_Thread *);
    void mark_scan_heap(GC_Thread *);

    void scan_slot(Slot, GC_Mark_Activity *);

    inline bool wait_till_there_is_work_or_no_work()
    {
        return _mark_scan_pool->wait_till_there_is_work_or_no_work();
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////

    inline void set_is_compaction_block_and_all_lives(block_info * block)
    {
        _p_block_store->set_is_compaction_block_and_all_lives(block);
    }

    inline bool is_compaction_block(block_info * block)
    {
        return _p_block_store->is_compaction_block(block);
    }

    inline block_info
        * get_block_for_sliding_compaction_allocation_pointer_computation
        (unsigned int id, block_info * curr)
    {
        return _p_block_store->
            get_block_for_sliding_compaction_allocation_pointer_computation
            (id, curr);
    }

    inline block_info
        * get_block_for_fix_slots_to_compaction_live_objects(unsigned int id,
        block_info * curr)
    {
        return _p_block_store->
            get_block_for_fix_slots_to_compaction_live_objects(id, curr);
    }

    inline block_info *get_block_for_slide_compact_live_objects(unsigned int
        id, block_info * curr)
    {
        return _p_block_store->get_block_for_slide_compact_live_objects(id,
            curr);
    }

    inline Remembered_Set *get_slots_into_compaction_block(block_info * block)
    {
        return _p_block_store->get_slots_into_compaction_block(block);
    }

#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

    inline void init_live_object_iterator_for_block(block_info * block)
    {
        _p_block_store->init_live_object_iterator_for_block(block);
    }
    inline Partial_Reveal_Object *get_next_live_object_in_block(block_info *
        block)
    {
        return _p_block_store->get_next_live_object_in_block(block);
    }

#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

    inline unsigned int get_total_live_objects_in_this_block(block_info *
        block)
    {
        return _p_block_store->get_total_live_objects_in_this_block(block);
    }

    inline block_info *iter_get_next_compaction_block_for_gc_thread(unsigned
        int thread_id, block_info * curr_block)
    {
        return _p_block_store->
            iter_get_next_compaction_block_for_gc_thread(thread_id,
            curr_block);
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void *get_gc_heap_base_address()
    {
        return _p_block_store->get_gc_heap_base_address();
    }

    inline void *get_gc_heap_ceiling_address()
    {
        return _p_block_store->get_gc_heap_ceiling_address();
    }

    bool obj_belongs_in_single_object_blocks(Partial_Reveal_Object *);

    bool block_is_invalid_or_free_in_block_store(block_info * block)
    {
        return _p_block_store->block_is_invalid_or_free_in_block_store(block);
    }

    // Indicate that this object will need to be finalized when it is no longer
    // reachable. 
    void add_finalize_object(Partial_Reveal_Object * p_object,
        bool check_for_duplicates = false);

    // Indicates that this object should not be finalized when it goes out of scope
    // even though it has a finalizer.
    void remove_finalize_object(Partial_Reveal_Object * p_object);

    // Checks if finalization hint is raised and decide to run finalization or not
    void check_hint_to_finalize();
    
    //
    // Starting with a pointer in a group of block holding a single object
    // fumble back through the blocks until you find the one that starts
    // this group of blocks.
    //
    block_info *find_start_of_multiblock(void
        *pointer_somewhere_in_multiblock)
    {
        // find where the start of the block would be
        block_info *pBlockInfo =
            GC_BLOCK_INFO(pointer_somewhere_in_multiblock);
        // keep going backwards from block to block until you find one that starts the multiblock
        while (pBlockInfo->block_store_info_index >= GC_MAX_BLOCKS ||
                pBlockInfo != _p_block_store->get_block_info(pBlockInfo->block_store_info_index)) {
            // any subtraction will do that puts us in the previous block
            pBlockInfo = GC_BLOCK_INFO(pBlockInfo - 1);
            assert(pBlockInfo >= _p_block_store->get_gc_heap_base_address());
        }
        return pBlockInfo;
    }

    void dump_chunk_statistics();
    void dump_object_statistics();

/////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    block_info * get_fresh_chunk_from_block_store(bool stay_above_waterline);

    block_info *get_free_chunk_from_global_gc_chunks();

    unsigned int return_free_blocks_to_block_store(int, bool, bool);

    void get_vm_live_references();

    void resume_vm();

    void init_gc_threads();

    void reset_gc_threads(bool);

    unsigned int sweep_single_object_blocks(block_info *);

    void prepare_to_sweep_heap();

    void prepare_chunks_for_sweeps_during_allocation(bool);

    void clear_roots();

    void roots_init();

    void get_gc_threads_to_begin_task(gc_thread_action);

    void wait_for_gc_threads_to_complete_assigned_task();

    void clear_all_mark_bit_vectors_of_unswept_blocks();

    // add a root slot to root set array
    void add_root_slot(Partial_Reveal_Object ** slot,
        bool compaction_this_gc);

    // add a heap slot for update during compaction
    void add_heap_slot(Slot & slot);

    unsigned get_num_bytes_recovered_by_sweep();

public:
    /// called from gc_thread_init()
    void notify_thread_init() 
    {
        number_of_user_threads++;
        // The number of unused nurseries before collection
        // is bound by (thread * blocks per chunk).
        // We aim to keep this number below 10% of the total number of blocks
        if (number_of_user_threads * _chunk_size_hint * 10 > 
                _p_block_store->get_num_total_blocks_in_block_store()
                && _chunk_size_hint > 1) {
            _chunk_size_hint--;
        }
    }

    /// called from gc_thread_kill()
    void notify_thread_kill()
    {
        number_of_user_threads--;
        assert(number_of_user_threads >= 0);
        if (number_of_user_threads * _chunk_size_hint * 10 <
                _p_block_store->get_num_total_blocks_in_block_store()
                && _chunk_size_hint < GC_MAX_BLOCKS_PER_CHUNK) {
            _chunk_size_hint++;
        }
    }

    /// The suggested size of the chunk
    unsigned chunk_size_hint()
    {
        return _chunk_size_hint;
    }

    void notify_gc_shutdown();
    void notify_gc_start();
    void notify_gc_end();

private:
    /////////////////////////////////// C O M P A C T I O N  //////////////////////////////////////////////

    void repoint_all_roots_into_compacted_areas();
    void repoint_all_roots_with_offset_into_compacted_areas();

    //////////////////////////////// V E R I F I C A T I O N //////////////////////////////////////////////

    void verify_marks_for_all_lives();



    // DEBUG TRACING ...
    // Trace and verify the entire heap
    unsigned int trace_verify_heap(bool);
    // Trace and verify all objects reachable from this object
    void trace_verify_sub_heap(Partial_Reveal_Object *, bool, MarkStack &);

    /////////////////////////// W E A K   P O I N T E R S /////////////////////////////////////////////////

    // weak root support
    // The bool parameter is true if you want to process short weak roots, false for long weak roots.
    // compactions_this_gc is true if the GC will be compacting.
    void process_weak_roots(bool process_short_roots,
        bool compactions_this_gc);

    /////////////////////////// F I N A L I Z A T I O N ///////////////////////////////////////////////////

    // APN 2005-12-07
public:
    void finalize_on_exit();
private:
    void identify_and_mark_objects_moving_to_finalizable_queue(bool);
    void add_objects_to_finalizable_queue();

    /////////////////////////// W E A K   R E F E R E N C E S ////////////////////////////////////////////

    // salikh 2005-05-25
public:
    int referent_offset;
    void add_phantom_reference(Partial_Reveal_Object * obj);
    void add_soft_reference(Partial_Reveal_Object * obj);
    void add_weak_reference(Partial_Reveal_Object * obj);
private:
    void add_reference_to_list(Partial_Reveal_Object * obj,
            std::list<Partial_Reveal_Object*> *reference_list);
    void process_reference_list(bool compaction_this_gc,
            std::list<Partial_Reveal_Object*> *reference_list);
    void process_phantom_references(bool compaction_this_gc);
    void process_soft_references(bool compaction_this_gc);
    void process_weak_references(bool compaction_this_gc);
    void enqueue_references();
    void register_referent_slots(std::list<Partial_Reveal_Object*> *reference_list, 
            bool compaction_this_gc);

    ///////////////////////// F R E E   A N D   T O T A L   M E M O R Y ///////////////////////////////////

public:
    int64 total_memory()
    {
        return (int64) GC_BLOCK_SIZE_BYTES
            * _p_block_store->get_num_total_blocks_in_block_store();
    }

    int64 max_memory()
    {
        return (int64) GC_BLOCK_SIZE_BYTES
            * _p_block_store->get_num_max_blocks_in_block_store();
    }

    int64 free_block_memory()
    {
        return (int64) GC_BLOCK_SIZE_BYTES
            * _p_block_store->get_num_free_blocks_in_block_store();
    }

    int64 free_chunk_memory();

    ///////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    void _verify_gc_threads_state();

    ///////////////////////////////////////////////////////////////////////////////////////////////////////

public:
    Block_Store * _p_block_store;

private:
    // CHUNKS
    std::vector<chunk_info> _gc_chunks;

    // CHUNKS limit marker
    volatile int _free_chunks_end_index;

    // SOB -- Single Object Blocks
    block_info *_single_object_blocks;

    // GC THREADS
    GC_Thread **_gc_threads;

    RootSet root_set;
    std::vector<Partial_Reveal_Object**> save_root_set;

    // statistics counter
    unsigned int num_roots_added;
    
    // some hint to run finalizer threads 
    bool hint_to_finalize;
    
    // hint to calculete number of created finalizable objects
    int finalizer_count;
 
    // list of short weak roots
    std::list<Partial_Reveal_Object**> m_short_weak_roots;
    // list of long weak roots
    std::list<Partial_Reveal_Object**> m_long_weak_roots;

    volatile int reference_list_lock;
    std::list<Partial_Reveal_Object*> *phantom_references;
    std::list<Partial_Reveal_Object*> *soft_references;
    std::list<Partial_Reveal_Object*> *weak_references;

    std::list<Partial_Reveal_Object*> *references_to_enqueue;

// temporary workaround to fix the live heap verification to take finalizable objects into account
public:

    // list of objects that will eventually need to be finalized
    std::list<Partial_Reveal_Object*> *m_listFinalize;
    // Lock to protect m_listFinalize
    volatile int m_finalize_list_lock;

    // list of objects that have been identified for finalization but not added to the finalizable queue yet.
    std::list<Partial_Reveal_Object*> m_unmarked_objects;
private:



    // set of blocks that contain at least one pinned root
    std::set<block_info*> m_pinned_blocks;

    hysem_t *_gc_thread_work_finished_event_handles;

    Work_Packet_Manager *_mark_scan_pool;

    // Keeping track of time in the GC
    unsigned int _gc_num;
    apr_time_t total_gc_time;
    apr_time_t total_user_time;
    apr_time_t last_gc_time;
    apr_time_t last_user_time;
    apr_time_t max_gc_time;

    apr_time_t _start_time, _end_time;
    apr_time_t _gc_start_time, _gc_end_time;

    ///////////////////////////////////////////////////////////////////////
    unsigned int _num_live_objects_found_by_first_trace_heap;
    Object_List *_live_objects_found_by_first_trace_heap;
    unsigned int _num_live_objects_found_by_second_trace_heap;
    Object_List *_live_objects_found_by_second_trace_heap;
    std::vector<Partial_Reveal_Object**> verify_root_set;

    // Valid only just after trace_verify_heap() returns.
    int64 live_size;

    int64 bytes_allocated;      /// the sum of sizes of allocated objects
    int64 objects_allocated;    /// the number of object allocated
    int64 bytes_spent;          /// the sum of sizes of used allocation areas 
    ///////////////////////////////////////////////////////////////////////

    bool gc_stats;          // true if gc stats logging is enabled
    bool object_stats;      // true if object stats logging is enabled

public:
    // globals refactoring
    volatile GC_Thread_Info *active_thread_gc_info_list;
    volatile long active_thread_gc_info_list_lock;

    /// approximate number of user threads
    int number_of_user_threads;
    unsigned _chunk_size_hint;

    /// number of blocks filled by compaction during last collection
    int number_of_to_compaction_blocks;

    /// number of compaction blocks of last collection
    int number_of_from_compaction_blocks;

    /// a lock guarding the variables
    /// number_of_from_compaction_blocks and number_of_to_compaction_blocks
    volatile int lock_number_of_blocks;

};

extern Garbage_Collector *p_global_gc;

static inline void get_active_thread_gc_info_list_lock()
{
    while (apr_atomic_casptr( 
                (volatile void **) &p_global_gc->active_thread_gc_info_list_lock,
                (void *) 1, (void *) 0) 
            != (void *) 0) {
        while (p_global_gc->active_thread_gc_info_list_lock == 1) {
            ;   // Loop until it is 0 and try again.
        }
    }
}

static inline void release_active_thread_gc_info_list_lock()
{
    p_global_gc->active_thread_gc_info_list_lock = 0;
}


#endif // _GARBAGE_COLLECTOR_H
