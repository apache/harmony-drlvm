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
 * @version $Revision: 1.1.2.3.4.3 $
 */  


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>
using namespace std;

// VM interface header files
#include "open/vm_util.h"
#include "open/vm_gc.h"
#include "open/hythread_ext.h"

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
#include "gc_utils.h"

#include "characterize_heap.h"
#include "mark.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
Garbage_Collector *p_global_gc = NULL;
bool compaction_enabled = true;
bool fullheapcompact_at_forcegc = true;
int incremental_compaction = 2;
#ifdef _DEBUG
bool verify_gc = true;
#else
bool verify_gc = false;
#endif
bool machine_is_hyperthreaded = false;
bool mark_scan_load_balanced = false;
bool verify_live_heap = false;
int force_gc_worker_thread_number = 0;
bool cross_block_compaction = true;
bool use_large_pages = false;
bool sweeps_during_gc = false;
bool ignore_finalizers = false;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern void init_verify_live_heap_data_structures();
extern void take_snapshot_of_lives_before_gc(unsigned int, Object_List *);  
extern void verify_live_heap_before_and_after_gc(unsigned int, Object_List *);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


Garbage_Collector::Garbage_Collector(POINTER_SIZE_INT initial_heap_size, POINTER_SIZE_INT final_heap_size, unsigned int block_size_bytes)
{
    ///////////////////////////////////
    _free_chunks_end_index = -1;
    _los_lock = 0;
    _los_blocks = NULL;
    _single_object_blocks = NULL;
    
    _gc_threads = NULL;
    _gc_num = 0;
    total_gc_time = 0;
    total_user_time = 0;
    last_gc_time = 0;
    last_user_time = 0;
    max_gc_time = 0;
    _gc_end_time = apr_time_now();
    _p_block_store = NULL;
    _gc_thread_work_finished_event_handles = NULL;
    num_roots_added = 0;

    // set default value for hit finalize, false mean that finalization shouldn't run now   
    hint_to_finalize = false;
    
    // set default value for counter of created finalizable objects (nothing is created)
    finalizer_count = 0;
     
    root_set.clear();

    _p_block_store  = new Block_Store(initial_heap_size, final_heap_size, block_size_bytes);
    
    _gc_thread_work_finished_event_handles = (hysem_t *) STD_MALLOC(sizeof(hysem_t ) * get_num_worker_threads());
    
    if (mark_scan_load_balanced) {
        _mark_scan_pool = new Work_Packet_Manager();
    } else {
        _mark_scan_pool= NULL;
    }
    
    //// V E R I F Y /////////////
    _num_live_objects_found_by_first_trace_heap = 0;
    _live_objects_found_by_first_trace_heap = NULL;
    _num_live_objects_found_by_second_trace_heap = 0;
    _live_objects_found_by_second_trace_heap = NULL;
    
    active_thread_gc_info_list = NULL;
    active_thread_gc_info_list_lock = 0;
    number_of_user_threads = 1;

    // 10 * 10 is the "reasonable" number of threads times "reasonable" number of chunks per thread
    //
    _chunk_size_hint = _p_block_store->get_num_total_blocks_in_block_store() / 10 / 10;
    if (_chunk_size_hint > GC_MAX_BLOCKS_PER_CHUNK) _chunk_size_hint = GC_MAX_BLOCKS_PER_CHUNK;
    if (_chunk_size_hint < 1) _chunk_size_hint = 1;
    assert(_chunk_size_hint >= 1 && _chunk_size_hint <= GC_MAX_BLOCKS_PER_CHUNK);

    // initialize pointers
    phantom_references = NULL;
    soft_references = NULL;
    weak_references = NULL;
    references_to_enqueue = NULL;
    m_listFinalize = NULL;

    root_set.reserve(GC_NUM_ROOTS_HINT);
    save_root_set.reserve(GC_NUM_ROOTS_HINT);
    verify_root_set.reserve(GC_NUM_ROOTS_HINT);

    lock_number_of_blocks = 0;
    number_of_to_compaction_blocks = 0;
    number_of_from_compaction_blocks = 0;

    gc_stats = is_info_enabled("gc.stats");

    // object stats are dangerous (may crash), so enable only by a special request
    object_stats = vm_get_property_value_boolean("gc.object_stats", false);
}



Garbage_Collector::~Garbage_Collector()
{
    TRACE2("gc.wrapup", "~Garbage_Collector()");
    if (phantom_references)
        delete phantom_references;
    if (soft_references)
        delete soft_references;
    if (weak_references)
        delete weak_references;
    if (references_to_enqueue)
        delete references_to_enqueue;
    if (m_listFinalize)
        delete m_listFinalize;
    if (_gc_thread_work_finished_event_handles)
        STD_FREE(_gc_thread_work_finished_event_handles);
    if (_live_objects_found_by_second_trace_heap)
        delete _live_objects_found_by_second_trace_heap;
    if (_live_objects_found_by_first_trace_heap)
        delete _live_objects_found_by_first_trace_heap;

    // free blocks memory 
    _p_block_store->init_block_iterator();
    block_store_info *b_info;

    while ((b_info = _p_block_store->get_next_block()))
    {
        if (b_info->block_is_free) continue;
        if (b_info->block->block_free_areas)
            STD_FREE(b_info->block->block_free_areas);
    }

    // free threads memory 
    if (_gc_threads)
    {
        for (unsigned int i = 0; i < get_num_worker_threads() ; i++) {
            if (_gc_threads[i])
            {
                delete _gc_threads[i];
            }
        }       
        STD_FREE(_gc_threads);
    }

    if (_p_block_store)
        delete _p_block_store;
}

// Checks if finalization hint is raised and decide to run finalization or not
void 
Garbage_Collector::check_hint_to_finalize() {
    // if hint say that finalization should be launched...
    if (hint_to_finalize) {

        // clean hint...
        hint_to_finalize = false;

        // trigger finalizers
        vm_hint_finalize();
    }
}

/*
 * If an exception is thrown between the time we acquire the lock and we set it to 0 then
 * the lock will never be freed.  An exception should not occur here so we should be OK
 * but is it ok to use try{}catch{} to make sure?
 */
void 
Garbage_Collector::add_finalize_object(Partial_Reveal_Object *p_object,bool check_for_duplicates) {
    std::list<Partial_Reveal_Object*>::iterator finalizer_iter;

    // take the finalize list lock
    assert(sizeof(unsigned int) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&m_finalize_list_lock, 1, 0) == 1);

    // Sometimes we know that the object can't already be in this list (for example,
    // if we have just allocated it) so we only have to check for duplicates if there is
    // the possibility of a duplication.
    if ( check_for_duplicates ) {
        // iterate through the list of finalizer objects
        for(finalizer_iter  = m_listFinalize->begin();
            finalizer_iter != m_listFinalize->end();
            ++finalizer_iter) {
            // if we found the object identical to the one they are trying to add
            // then the object cannot be added to the list again.  This maintains
            // the invariant that an object can only appear in this list once.
            if (*finalizer_iter == p_object) {
                // release the finalize list lock
                m_finalize_list_lock = 0;
                return;
            }
        }
    }

   
    // works with hint, so synchronization isn't required
    // if hits say that many finalize object are exist...
    if (128 <= finalizer_count) {
        // clean hint...
        finalizer_count = 0;

        INFO2("gc.finalize", "finalizable object count exceeded " << finalizer_count
                << ", hinting finalization");
    
        // trigger finalizers
        hint_to_finalize = true;
    } else {
        finalizer_count++;
    }
    // It must have not been in the list yet so we can safely add it here.
    m_listFinalize->push_back(p_object);
    // release the finalize list lock
    m_finalize_list_lock = 0;
}

/*
 * If an exception is thrown between the time we acquire the lock and we set it to 0 then
 * the lock will never be freed.  An exception should not occur here so we should be OK
 * but is it ok to use try{}catch{} to make sure?
 */
void 
Garbage_Collector::remove_finalize_object(Partial_Reveal_Object *p_object) {
    std::list<Partial_Reveal_Object*>::iterator finalizer_iter;

    // take the finalize list lock
    assert(sizeof(unsigned int) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&m_finalize_list_lock, 1, 0) == 1);

    // iterate through the list of finalizer objects
    for(finalizer_iter  = m_listFinalize->begin();
        finalizer_iter != m_listFinalize->end();
        ++finalizer_iter) {
        // if we found the object that needs to be removed
        if (*finalizer_iter == p_object) {
            // remove the object from the list
            m_listFinalize->erase(finalizer_iter);
            // INVARIANT: add_finalize_object() checks to make sure only one
            // of each object can be on the finalizer list at a time so if
            // we find one here we can be guaranteed there are no more and it
            // is safe to return.
            // release the finalize list lock
            m_finalize_list_lock = 0;
            return;
        }
    }

    // release the finalize list lock
    m_finalize_list_lock = 0;
}


void
Garbage_Collector::gc_v4_init()
{
    TRACE2("gc.init", "Garbage_Collector::gc_v4_init()");
    assert(_p_block_store);
    
    // Let's allocate some chunks for starts
    unsigned int num_chunks_to_allocate = 1;
    // at maximum, each block can be separate chunks + NULL terminator
    unsigned int num_chunks_to_reserve = _p_block_store->get_num_max_blocks_in_block_store() + 1;
    if (num_chunks_to_allocate > num_chunks_to_reserve) {
        num_chunks_to_allocate = num_chunks_to_reserve;
    }
    
    // resize to max chunks could be allocated on current heap
    _gc_chunks.resize(num_chunks_to_reserve);
    memset(&(_gc_chunks[0]), 0, sizeof(chunk_info)*_gc_chunks.size());
    LOG2("gc.chunks", "reserving " << num_chunks_to_reserve << " chunks");
    
    init_gc_threads();
    
    roots_init();
    
    gc_add_fresh_chunks(num_chunks_to_allocate);
    
    if (characterize_heap) {
        heapTraceInit();
    }
    
    m_listFinalize = new std::list<Partial_Reveal_Object*>;
    m_finalize_list_lock = 0;

    reference_list_lock = 0;
    phantom_references = new std::list<Partial_Reveal_Object*>;
    soft_references = new std::list<Partial_Reveal_Object*>;
    weak_references = new std::list<Partial_Reveal_Object*>;

    references_to_enqueue = new std::list<Partial_Reveal_Object*>;
    referent_offset = 0;
}

//
// Retrieves a fresh chunk from the block store. If if is for object placement then 
// stay_above_water_line will be false, if it is for a chunk to be allocated
// stay_above_water_line will the true.
//

// GC_FUSE added the stay_above_water_line that is passed to p_get_new_block without
// wrapping it in an #ifdef GC_FUSE.

block_info *
Garbage_Collector::get_fresh_chunk_from_block_store(bool stay_above_water_line)
{
    block_info *chunk_start = NULL;
    TRACE2("gc.chunks", "get_fresh_chunk_from_block_store(" 
            << (stay_above_water_line ? "above waterline" : "below waterline") << ")");
    
    unsigned j;
    for (j = 0; j < chunk_size_hint(); j++) {
        
        block_info *block = _p_block_store->p_get_new_block(stay_above_water_line);
        if (block == NULL) { // Block store couldnt satisfy request.
            // Return partial chunk if a full chunk cannot be allocated. -salikh
            break;
        }   
        
        gc_trace_block (block, " get_fresh_chunk_from_block_store gets this block");
        block->in_nursery_p = true;
        block->in_los_p = false;
        block->is_single_object_block = false;
        
        block->nursery_status = free_nursery; 
        
        // Allocate free areas per block if not already allocated
        if (!block->block_free_areas)  {
            block->block_free_areas = (free_area *)STD_MALLOC(sizeof(free_area) * GC_MAX_FREE_AREAS_PER_BLOCK);
            assert(block->block_free_areas);
        }
        
      
        // don't clean markbits before first GC, allocated pages already cleaned
        // up by OS
        if (_gc_num != 0)
            memset(block->mark_bit_vector, 0, GC_SIZEOF_MARK_BIT_VECTOR_IN_BYTES);

        // It is fully allocatable
        // Initialize free areas for block....
        gc_trace_block (block, " calling clear_block_free_areas in get_fresh_chunk_from_block_store.");
        clear_block_free_areas(block);
        
        // JUST ONE LARGE FREE AREA initially
        free_area *area = &(block->block_free_areas[0]);
        area->area_base = GC_BLOCK_ALLOC_START(block);
        area->area_ceiling = (void *)((POINTER_SIZE_INT)GC_BLOCK_ALLOC_START(block) + (POINTER_SIZE_INT)GC_BLOCK_ALLOC_SIZE - 1);
        area->area_size = GC_BLOCK_ALLOC_SIZE;
        
        area->has_been_zeroed = (_gc_num == 0);
        
        block->num_free_areas_in_block = 1;
        
        block->current_alloc_area = -1;         
        
        //      if (!sweeps_during_gc) {
        // Needed....This block is fresh from the block store...its ONLY allocation area has 
        // been determined above. Allocator should not try to sweep it since it is meaningless...
        // (since it was not collectable during the previous GC cycle
        block->block_has_been_swept = true;
        //      }
        
        // Start allocation in this block at the base of the first and only area
        block->curr_free = area->area_base;
        block->curr_ceiling = area->area_ceiling;
        
        // Insert new block into chunk
        block->next_free_block = chunk_start;
        chunk_start = block;
    }
    
    LOG2("gc.chunks", "allocated chunk of " << j << " blocks");
    return chunk_start;
}


// used as a hint and shared by all threads...
static volatile unsigned int chunk_index_hint = 0;


block_info *
Garbage_Collector::get_free_chunk_from_global_gc_chunks()
{
    block_info *fresh_chunk = NULL;
    bool search_from_hint_failed = false;
    int chunk_index = 0;
    
    // FIXME: move this code out as a function
retry_get_free_chunk_label:
    if (!search_from_hint_failed) {
        chunk_index = chunk_index_hint;
    } else {
        chunk_index = 0;
    }
    // Search available chunks and grab one atomically
    while (chunk_index <= _free_chunks_end_index) {
        chunk_info *curr_chunk = &(_gc_chunks[chunk_index]); 
        if (curr_chunk->chunk) {
            void *free_val = curr_chunk->free_chunk;
            fresh_chunk = NULL;
            if (free_val != NULL) {
                // Duplicate pointers to the same free chunk
                ASSERT(free_val == curr_chunk->chunk,
                        "chunk index = " << chunk_index 
                        << ", chunk = " << curr_chunk->chunk
                        << ", free = " << curr_chunk->free_chunk
                        << ", free_val = " << free_val);
                // attempt to grab free chunk
                fresh_chunk = (block_info *)apr_atomic_casptr(
                    (volatile void **)&(curr_chunk->free_chunk),
                    (void *) NULL,
                    (void *) free_val
                    );
                if (fresh_chunk == (block_info *)free_val) {
                    // success.. I own chunk at chunk_index.
                    // update my hint for my search next time...
                    chunk_index_hint = chunk_index;
                    break;
                } 
            }
        }
        chunk_index++;
    }
    
    if ((fresh_chunk == NULL) && (search_from_hint_failed == false)) {
        // Try again...exactly once more....this time from index 0;
        search_from_hint_failed = true;
        goto retry_get_free_chunk_label;
    }
    
    // Better have got some chunk...otherwise may need to cause GC
    if (fresh_chunk) {
        LOG("allocated chunk " << fresh_chunk << " with index " << chunk_index);
        assert(chunk_index <= _free_chunks_end_index);
        // I OWN chunk at "chunk_index"
        assert(fresh_chunk == _gc_chunks[chunk_index].chunk);
        // It better be free
        assert(fresh_chunk->nursery_status == free_nursery);
    }
    return fresh_chunk;
}



int
Garbage_Collector::gc_add_fresh_chunks(unsigned int num_chunks_to_add)
{
    LOG2("gc.chunks", "gc_add_fresh_chunks(" << num_chunks_to_add << ")");
    // This function is called from only from gc_v4_init() or under gc lock
    // (vm_gc_lock_enum()) so it is thread safe. It gets new blocks from the
    // block store and creates chunks out of them and adds them to _gc_chunks[]
    //
    // Multiple threads cant try to add fresh chunks from 
    // the block store at the same time. The first one that fails would cause GC,
    // and the rest of the threads that block on the lock would retry to get a 
    // chunk before doing anything else....
    unsigned int empty_chunk_index = 0;
    unsigned int num_chunks_added = 0;


    // the last element in chunks array is reserved for NULL termination
    
    while (num_chunks_added < num_chunks_to_add && empty_chunk_index < _gc_chunks.size()-1) {
        
        // look for the next empty slot in _gc_chunks
        while (_gc_chunks[empty_chunk_index].chunk != NULL && empty_chunk_index < (_gc_chunks.size()-2)) {
            empty_chunk_index++;
        }

        // break if there's no more space in _gc_chunks
        if (_gc_chunks[empty_chunk_index].chunk != NULL) break;

        block_info *chunk = get_fresh_chunk_from_block_store(false);
        if (NULL == chunk) break;
        
        assert(_gc_chunks[empty_chunk_index].chunk == NULL);
        assert(_gc_chunks[empty_chunk_index].free_chunk == NULL);
        assert(empty_chunk_index < _gc_chunks.size()-1);
        _gc_chunks[empty_chunk_index].chunk = _gc_chunks[empty_chunk_index].free_chunk = chunk;
        num_chunks_added++;
        empty_chunk_index++;
    }   
        
    if (0 == num_chunks_added) {
        LOG2("gc.chunks", "gc_add_fresh_chunks failed to add any chunks");
        return num_chunks_added;
    }

    assert(_gc_chunks[_gc_chunks.size() - 1].chunk == NULL);
    while (empty_chunk_index < _gc_chunks.size()-1 && _gc_chunks[empty_chunk_index].chunk != NULL) {
        empty_chunk_index++;
    }
    assert(empty_chunk_index < _gc_chunks.size());

    if ((int)empty_chunk_index > _free_chunks_end_index) {
        _free_chunks_end_index = empty_chunk_index;
        LOG2("gc.chunks", "#allocatable chunks = " << _free_chunks_end_index + 1);
    }

    assert(_free_chunks_end_index < (int)_gc_chunks.size());
    LOG2("gc.chunks", "added " << num_chunks_added << " chunks");
    return num_chunks_added;
}


static inline void change_chunk_status(block_info* chunk, 
                                        nursery_state old_state,
                                        nursery_state new_state)
{
    block_info *block = chunk;
    while (block) {
        assert(block);
        assert(block->nursery_status == old_state);
        block->nursery_status = new_state;
        block = block->next_free_block;
    }
}

//
// The Thread just ran out of another chunk. Give it a fresh one.
//
block_info *
Garbage_Collector::p_cycle_chunk(block_info *used_chunk)
{
    change_chunk_status(used_chunk, active_nursery, spent_nursery);
    block_info *new_chunk = get_free_chunk_from_global_gc_chunks();
    change_chunk_status(new_chunk, free_nursery, active_nursery);
    return new_chunk;
}



////////////////////////////////// void return_free_blocks_to_block_store(); ////////////////////


// T O DO ---->>>>> XXXXXXXXXXXXX  ------>>> Return only SO MANY BLOCKS  <<<<<<<<<<<,

unsigned int
Garbage_Collector::return_free_blocks_to_block_store(int blocks_to_return, bool to_coalesce_block_store, bool compaction_has_been_done_this_gc)
{
    unsigned int num_blocks_returned = 0;
    unsigned int num_empty_chunks = 0;
    
    for (int chunk_index = 0; chunk_index <= _free_chunks_end_index; chunk_index++) {
        
        chunk_info *this_chunk = &_gc_chunks[chunk_index];
        assert(this_chunk);
        
        
        if ((this_chunk->chunk) && (this_chunk->chunk->nursery_status != active_nursery)) {
            // Will return only free blocks (no nursery is spent after a GC..either it is active or it is free
            assert(this_chunk->chunk->nursery_status == free_nursery);
            
            block_info *block = this_chunk->chunk;
            assert(block);
            
            block_info *new_chunk_start = NULL;
            
            while (block) {
                gc_trace_block (block, "in return_free_blocks_to_block_store looking for free blocks");
                // If any compaction has happened in this GC
                if (compaction_has_been_done_this_gc && is_compaction_block(block)){
                    // block is then automatically swept
                    assert(block->block_has_been_swept == true);
                } else {
                    if (sweeps_during_gc) {
                        assert(block->block_has_been_swept == true);
                    } else {
                        if (block->block_has_been_swept == false) {
                            // First sweep it and check if it is fully free...we need to do this since as of now all blocks are setup for sweeps during allocation
                            sweep_one_block(block);
                            block->block_has_been_swept = true;
                        }
                    }
                }
                if (block->block_free_areas[0].area_size != GC_BLOCK_ALLOC_SIZE) {
                    assert (block->block_free_areas[0].area_size < GC_BLOCK_ALLOC_SIZE);
                    
                    block_info *next_block = block->next_free_block;
                    // Relink onto the new chunk
                    block->next_free_block = new_chunk_start;
                    new_chunk_start = block;
                    block = next_block;        
                } else {
                    gc_trace_block (block, "in return_free_blocks_to_block_store returing free block to block store.");
                    // Fully free block means.....NO live data...can go back to the block store
                    // Return it to the block store
                    block_info *next_block = block->next_free_block;
                    assert(block->nursery_status == free_nursery);
                    num_blocks_returned += _p_block_store->link_free_blocks (block);
                    block = next_block;
                }
            }
            if (new_chunk_start == NULL) {
                num_empty_chunks++;
            }
            this_chunk->chunk = this_chunk->free_chunk = new_chunk_start;   
            
        }
        
        // check if we are done...
        if ((blocks_to_return != -1) && ((int) num_blocks_returned >= blocks_to_return)) {
            // Stop with this chunk
            break;
        }
    }
    
    assert(to_coalesce_block_store == false);
    
    INFO2("gc.blocks", "return_free_blocks_to_block_store() returned " 
            << num_blocks_returned << " to the block store");
    
    return num_blocks_returned;
    
} //return_free_blocks_to_block_store


///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////// C O L L E C T I O N  ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


void
Garbage_Collector::get_vm_live_references()
{
    assert(num_roots_added == 0);
    LOG("calling vm_enumerate_root_set_all_threads()");
    vm_enumerate_root_set_all_threads();
}


void
Garbage_Collector::resume_vm()
{
    vm_resume_threads_after();
}


void 
Garbage_Collector::init_gc_threads()
{
    assert(_gc_threads == NULL);
    
    _gc_threads = (GC_Thread **) STD_MALLOC(sizeof(GC_Thread *) * get_num_worker_threads());
    
    assert(_gc_threads);
    
    for (unsigned int i = 0; i < get_num_worker_threads() ; i++) {
        
        // Point the GC thread to the GC object
        _gc_threads[i] = new GC_Thread(this, i);
        assert(_gc_threads[i]);
    }       
    
    // Grab the work done handles of the GC worker threads so that we know when the work is done by each thread
    for (unsigned int x = 0; x < get_num_worker_threads(); x++) {
        _gc_thread_work_finished_event_handles[x] = _gc_threads[x]->get_gc_thread_work_done_event_handle();
    }
}


void 
Garbage_Collector::reset_gc_threads(bool compaction_gc)
{
    for (unsigned int i = 0; i < get_num_worker_threads() ; i++) {
        assert(_gc_threads[i]);
        _gc_threads[i]->reset(compaction_gc);
    }       
}

void 
Garbage_Collector::get_gc_threads_to_begin_task(gc_thread_action task) 
{
    for (unsigned int y = 0; y < get_num_worker_threads(); y++) {
        
        _gc_threads[y]->set_task_to_do(task);
        IDATA UNUSED sstat = hysem_post(_gc_threads[y]->get_gc_thread_start_work_event_handle());
        assert(sstat == TM_ERROR_NONE);
    }
}


void 
Garbage_Collector::wait_for_gc_threads_to_complete_assigned_task()
{
    IDATA UNUSED sstat = wait_for_multiple_semaphores( get_num_worker_threads(), _gc_thread_work_finished_event_handles);
    assert(sstat == TM_ERROR_NONE);
}

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.finalize"


// APN 20051207
// Moves all finalizeble object to queue for finalize
void
Garbage_Collector::finalize_on_exit() {
    // take the finalize list lock
    assert(sizeof(unsigned int) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&m_finalize_list_lock, 1, 0) == 1);

    std::list<Partial_Reveal_Object*>::iterator finalize_iter = m_listFinalize->begin();

     // loop through all of the objects in the finalizer set
    for(; finalize_iter != m_listFinalize->end(); ++finalize_iter) {
        // Add the object to those that will be moved to the finalizable queue.
        vm_finalize_object((Managed_Object_Handle)*finalize_iter);
    }
    m_listFinalize->clear();
    // release the finalize list lock
    m_finalize_list_lock = 0;
}

// 20040304 finalization support
// Moved unreachable objects to the finalizable queue.
void
Garbage_Collector::add_objects_to_finalizable_queue()
{
    TRACE("add_objects_to_finalizable_queue");

    // Call the finalizers in the reversed allocation ordering, in order to
    // make sure that within the pack of object, which became f-reachable at
    // the same time, older objects will get finalized later than younger
    // objects
    std::list<Partial_Reveal_Object*>::reverse_iterator finalize_iter = m_unmarked_objects.rbegin();

    // Iterate through each object in the finalize list.
    for(; finalize_iter != m_unmarked_objects.rend(); ++finalize_iter) {

        TRACE("asking vm to finalize object " << (*finalize_iter)
                << " (" << (*finalize_iter)->class_name() << ")");
        vm_finalize_object((Managed_Object_Handle)*finalize_iter);
    }
    // We do not need the objects in m_unmarked_objects anymore.
    m_unmarked_objects.clear();
}

// 20040304 finalization support
// Identify unreachable objects and recursively marks roots from those objects being moved.
void
Garbage_Collector::identify_and_mark_objects_moving_to_finalizable_queue(bool compaction_this_gc)
{
    LOG("identify_and_mark_objects_moving_to_finalizable_queue");
    // take the finalize list lock
    assert(sizeof(unsigned int) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&m_finalize_list_lock, 1, 0) == 1);

    std::list<Partial_Reveal_Object*>::iterator finalize_iter = m_listFinalize->begin();

    // Make sure the set of objects added to the finalizable queue in the 
    // previous GC has been emptied.
    m_unmarked_objects.clear();

    TRACE("identify_and_mark_objects_moving_to_finalizable_queue()");
    // loop through all of the objects in the finalizer set
    for(; finalize_iter != m_listFinalize->end(); ++finalize_iter) {
        // get the object * from the iterator
        Partial_Reveal_Object *p_obj = *finalize_iter;
        TRACE("considering " << p_obj);
        assert(p_obj);

        std::list<Partial_Reveal_Object*>::iterator insert_location;
        bool object_was_finalizable = false;
        if(!is_object_marked(p_obj)) {
            TRACE("finalizable object " << p_obj << " is not marked, hence moved to finalization queue");
            // Add the object to those that will be moved to the finalizable queue.
            m_unmarked_objects.push_back(p_obj);
            insert_location = m_unmarked_objects.end();
            --insert_location;
            // remove this object from the finalize set
            m_listFinalize->erase(finalize_iter--);
            // remove and use !m_unmarked_objects.empty()
            object_was_finalizable   = true;
        } else {
            TRACE("finalizable object " << p_obj << " is still alive");
        }
            
        // The eumeration needs to provide a set which means no duplicates. Remove duplicates here.
        
        Partial_Reveal_Object **slot = NULL;
        if (object_was_finalizable) {
            // Add a root that could be modified later from m_unmarked_objects list.
            slot = &(*insert_location);
        } else {
            // Add a root from the m_listFinalize list.
            slot = &(*finalize_iter);
        }

        TRACE("adding " << *slot << "(at " << slot << ")" << " to root array");
        root_set.insert(slot);

        // save away this root as well so that it can be updated
        if(compaction_this_gc) {
            save_root_set.push_back(slot);
        }
        // Because the post-GC trace heap uses verify_root_set
        verify_root_set.push_back(slot);
    }

    INFO("Finalizable objects queue length = " << m_listFinalize->size());
    INFO("Objects to finalize = " << m_unmarked_objects.size());

    // release the finalize list lock
    m_finalize_list_lock = 0;

    if (!m_unmarked_objects.empty()) {
        // One or more objects are being revived and will be placed onto the finalizable queue.
        // Initiate a mark/scan phase for those new objects.
        get_gc_threads_to_begin_task(GC_MARK_SCAN_TASK);
        wait_for_gc_threads_to_complete_assigned_task();
    }
}

// 20040309 weak root support
// Scan the short or long weak root list and set each pointer
// to NULL if it is not reachable and otherwise adds it to
// the list of roots that will get repointed later.
//
// Our current approach is to enumerate the weak roots, store them in a list and then
// scan them later.  This function does the scanning.  An alternate but more complicated
// approach would be to enumerate and process them simultaneously.  If the extra storage
// cost of long weak roots or in the effect their second scanning is too slow then the
// latter approach may be employed later.
void 
Garbage_Collector::process_weak_roots(bool process_short_roots,bool compaction_this_gc) 
{
    std::list<Partial_Reveal_Object**> *cur_list = NULL;
    if (process_short_roots) { 
        cur_list = &m_short_weak_roots;
    } else { 
        cur_list = &m_long_weak_roots;
    }

    // Process all the roots in the given list.
    while (!cur_list->empty()) {
        // Get the first item in the list into root_slot.
        Partial_Reveal_Object **root_slot  = cur_list->front();
        Partial_Reveal_Object *root_object = *root_slot;

        // Remove the first item from the list.
        cur_list->pop_front();
        
        // No need to do anything if the root is already a NULL pointer.
        if (!root_object) {
            continue;
        }

        // The object is marked so from now on we treat it like any other root so
        // that it will get updated if necessary.
        if (is_object_marked(root_object)) {
            
            root_set.insert(root_slot);
            
            // save away this root as well so that it can be updated
            if(compaction_this_gc) {
                save_root_set.push_back(root_slot);
            }
            verify_root_set.push_back(root_slot);
        } else {
            // The object is not marked so by the definition of a weak root we set the root to NULL.
            *root_slot = NULL;
        }
    }
}

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.roots"

/** Adds a slot to roots array. */
void Garbage_Collector::add_root_slot(Partial_Reveal_Object** slot, bool compaction_this_gc) {
    TRACE("add_root_slot " << (*slot) << " at " << slot);
        
    TRACE("adding " << *slot << "(at " << slot << ")" << " to root array");
    root_set.insert(slot);

    // save away this root as well so that it can be updated
    if(compaction_this_gc) {
        save_root_set.push_back(slot);
    }
    verify_root_set.push_back(slot);
}

void Garbage_Collector::add_heap_slot(Slot& slot)
{
    TRACE("add_heap_slot()");
    Partial_Reveal_Object* obj = slot.dereference();
    block_info* info = GC_BLOCK_INFO(obj);
    // XXX salikh: not sure this is correct, but add_slot_to_compaction_block
    // requires a GC_Thread to be specified, so take the first one.
    GC_Thread* gc_thread = _gc_threads[0];
    if (is_compaction_block(info)) {
        _p_block_store->add_slot_to_compaction_block(slot, info, gc_thread->get_id());
        if (stats_gc) {
            gc_thread->increment_num_slots_collected();
        }
    }
}

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.ref"

////////////////////////////////////////////////
// weak references support, salikh 2005-05-25
//
void Garbage_Collector::process_soft_references(bool compaction_this_gc)
{
    TRACE2("gc.ref", "process_soft_references(" << compaction_this_gc << ")");
    // FIXME salikh: implement some euristics to decide whether we need to
    // reset soft references
    bool reset_soft_references = true;
    if (reset_soft_references) {
        process_reference_list(compaction_this_gc, soft_references);
    } else {
        // FIXME salikh: mark soft referents and restart MARKSCAN process
    }
}

void Garbage_Collector::process_weak_references(bool compaction_this_gc)
{
    TRACE2("gc.ref", "process_weak_references(" << compaction_this_gc << ")");
    process_reference_list(compaction_this_gc, weak_references);
}

void Garbage_Collector::process_phantom_references(bool compaction_this_gc)
{
    TRACE2("gc.ref", "process_phantom_references(" << compaction_this_gc << ")");
    process_reference_list(compaction_this_gc, phantom_references);
}

void Garbage_Collector::process_reference_list(bool UNREF compaction_this_gc, 
        std::list<Partial_Reveal_Object *>* reference_list)
{
    int UNUSED reference_list_lock_copy = reference_list_lock;
    assert(0 == reference_list_lock_copy || 1 == reference_list_lock_copy);
    // take the weak references list lock
    assert(sizeof(int) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&reference_list_lock, 1, 0) == 1);

    std::list<Partial_Reveal_Object*>::iterator i = reference_list->begin();

    // loop through all of the objects in the finalizer set
    for(; i != reference_list->end(); ++i) {
        Partial_Reveal_Object* ref = *i;
        gc_trace(ref, "weak reference is being considered");
        
        Slot referent((char *)ref + referent_offset);
        Partial_Reveal_Object* obj = referent.dereference();

        if (Slot::managed_null() == obj) {
            // the reference is not dropped as User still can assign 
            // non-null value to the referent field -salikh
            TRACE("someone has already cleared the reference " 
                << ref << " -> " << obj << ", so skip it");
            continue;
        }

        if (!is_object_marked(obj)) {
            gc_trace(ref, "weak reference is cleared");
            gc_trace(obj, "weak referent is cleared");
            TRACE("weak referent " << obj << " is not marked, clearing the reference"); 
            referent.update((Partial_Reveal_Object *)Slot::managed_null());
            if (!is_object_marked(ref)) {
                TRACE("weak reference at " << ref << " is not marked, so don't enqueue it");
                continue;
            }
            reference_list->erase(i--);
            assert(is_object_marked(ref));
            references_to_enqueue->push_back(ref);
        } else {
            TRACE2("gc.ref.alive", "weak referent " << obj << " is still alive");
        }
    }

    // release the weak references list lock
    reference_list_lock = 0;
}

void Garbage_Collector::enqueue_references()
{
    LOG("enqueue_references()");
    std::list<Partial_Reveal_Object*>::iterator i = references_to_enqueue->begin();

    for(; i != references_to_enqueue->end(); ++i) {
        TRACE("asking vm to enqueue reference " << (*i));
        vm_enqueue_reference((Managed_Object_Handle)*i);
    }
    references_to_enqueue->clear();
}

void Garbage_Collector::add_soft_reference(Partial_Reveal_Object *obj) {
    TRACE2("gc.ref.alive", "add_soft_reference("<< obj << ")");
    add_reference_to_list(obj, soft_references);
}

void Garbage_Collector::add_phantom_reference(Partial_Reveal_Object *obj) {
    TRACE2("gc.ref.alive", "add_phantom_reference("<< obj << ")");
    add_reference_to_list(obj, phantom_references);
}

void Garbage_Collector::add_weak_reference(Partial_Reveal_Object *obj) {
    TRACE2("gc.ref.alive", "add_weak_reference("<< obj << ")");
    add_reference_to_list(obj, weak_references);
}

void Garbage_Collector::add_reference_to_list(Partial_Reveal_Object* obj, 
        std::list<Partial_Reveal_Object*>* reference_list)
{
    std::list<Partial_Reveal_Object*>::iterator i;

    int UNUSED reference_list_lock_copy = reference_list_lock;
    assert(0 == reference_list_lock_copy || 1 == reference_list_lock_copy);
    // take the reference lock
    assert(sizeof(int) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&reference_list_lock, 1, 0) == 1);

    /* Debug check:
    // iterate through the list of finalizer objects
    for(i = reference_list->begin();
        i != reference_list->end();
        ++i) {
        // if we found the object identical to the one they are trying to add
        // then the object cannot be added to the list again.  This maintains
        // the invariant that an object can only appear in this list once.
        if (*i == obj) {
            TRACE("found duplicate of " << obj);
            // release the finalize list lock
            reference_list_lock = 0;
            return;
        }
    }
    */

    // It must have not been in the list yet so we can safely add it here.
    reference_list->push_back(obj);
    // release the reference lock
    reference_list_lock = 0;
}

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.ref.slots"

void Garbage_Collector::register_referent_slots(std::list<Partial_Reveal_Object*>* reference_list, bool compaction_this_gc) {
    TRACE("register_referent_slots(" << compaction_this_gc << ")");

    int UNUSED reference_list_lock_copy = reference_list_lock;
    assert(0 == reference_list_lock_copy || 1 == reference_list_lock_copy);
    // take the weak references list lock
    while (apr_atomic_cas32((volatile uint32 *)&reference_list_lock, 1, 0) == 1);
    
    std::list<Partial_Reveal_Object*>::iterator i = reference_list->begin();

    for (; i != reference_list->end(); ++i) {
        Partial_Reveal_Object* ref = *i;
        gc_trace(ref, "weak referent slots are being considered");

        // can do this here because no more object revival will be performed -salikh
        if (!is_object_marked(ref)) {
            Slot referent((char *)ref + referent_offset);
            TRACE2("gc.ref.drop", "weak reference at " 
                << ref << " -> " << referent.dereference() << " is not marked, so drop it");
            reference_list->erase(i--);
            continue;
        }

        assert(is_object_marked(ref));
        Partial_Reveal_Object **ref_slot = &(*i);
        add_root_slot(ref_slot, compaction_this_gc);

        Slot referent((char *)ref + referent_offset);
        Partial_Reveal_Object* obj = referent.dereference();

        if (Slot::managed_null() == obj) {
            TRACE("someone has already cleared the reference " 
                << ref << " -> " << obj << ", so skip it");
            continue;
        } else if (is_object_marked(obj)) {
            if (compaction_this_gc) {
                TRACE("registering weak referent slot " << referent.get_address() << " -> " << obj);
                add_heap_slot(referent);
            }
        } else {
            WARN("reference " << ref << " to dead object " << obj << " is not cleared, clearing");
            referent.update((Partial_Reveal_Object *)Slot::managed_null());
        }

    }

    // release the weak references list lock
    reference_list_lock = 0;
}

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.compact.repoint"

void 
Garbage_Collector::repoint_all_roots_into_compacted_areas()
{
    unsigned int roots_repointed = 0;
    std::vector<Partial_Reveal_Object**>::iterator i;
    for (i = save_root_set.begin(); i != save_root_set.end(); ++i) {
        Partial_Reveal_Object **root_slot = *i;
        TRACE("root_slot " << (void *)root_slot);
        assert(root_slot);
        Partial_Reveal_Object *root_object = *root_slot;
        if (root_object->get_obj_info() & FORWARDING_BIT_MASK) { // has been forwarded
            TRACE("root_object " << (void *)root_object);
            // only objects in compaction blocks get forwarded
            assert(is_compaction_block(GC_BLOCK_INFO(root_object)));
            
            Partial_Reveal_Object *new_root_object = root_object->get_forwarding_pointer();
            TRACE("new_root_object " << (void *)new_root_object);
            // Update root slot
            *root_slot = new_root_object;
            roots_repointed++;
        }
    }
    INFO2("gc.roots", "roots_repointed = " << roots_repointed);
}


void 
Garbage_Collector::repoint_all_roots_with_offset_into_compacted_areas()
{
    unsigned int roots_with_offset_repointed = 0;
    for (interior_pointer_table->rewind(); interior_pointer_table->available(); interior_pointer_table->next())
    {
        void **root_slot = interior_pointer_table->get_slot();
        Partial_Reveal_Object *root_base = interior_pointer_table->get_base();
        POINTER_SIZE_INT root_offset = interior_pointer_table->get_offset();
        void *new_slot_contents = (void *)((Byte*)root_base + root_offset);
        if (new_slot_contents != *root_slot)
        {
            *root_slot = new_slot_contents;
            roots_with_offset_repointed ++;
        }
    }
    
    for (compressed_pointer_table->rewind(); compressed_pointer_table->available(); compressed_pointer_table->next())
    {
        uint32 *root_slot = (uint32 *)compressed_pointer_table->get_slot();
        Partial_Reveal_Object *root_base = compressed_pointer_table->get_base();
        POINTER_SIZE_INT root_offset = compressed_pointer_table->get_offset();
        uint32 new_slot_contents = (uint32)(POINTER_SIZE_INT)((Byte*)root_base - root_offset);
        if (new_slot_contents != *root_slot)
        {
            *root_slot = new_slot_contents;
            roots_with_offset_repointed ++;
        }
    }
    
    INFO2("gc.roots", "roots_with_offset_repointed = " 
            << roots_with_offset_repointed);
    interior_pointer_table->reset();
    compressed_pointer_table->reset();
}



unsigned Garbage_Collector::get_num_bytes_recovered_by_sweep() {    
    unsigned recovered = 0;
    for (unsigned n = 0; n < get_num_worker_threads(); n++) {
        recovered += _gc_threads[n]->get_num_bytes_recovered_by_sweep();
    }
    return recovered;
} 

///////////////////////////////////////////////////////////////////////////////////////////////////////
#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.verbose"

// The central garbage collection function
void 
Garbage_Collector::reclaim_full_heap(unsigned int size_failed, bool full_heap_compaction) 
{
    notify_gc_start();

    if (size_failed > 0) 
        INFO2("gc.stats", "GC caused by the failed allocation of size " << size_failed << " ("
                << ((size_failed + GC_BLOCK_INFO_SIZE_BYTES + GC_BLOCK_SIZE_BYTES - 1) / GC_BLOCK_SIZE_BYTES)
                << " blocks)");
    if (full_heap_compaction && compaction_enabled)
        INFO2("gc.stats", "Forcing full heap compaction");
    
    if (verify_live_heap) {
        init_verify_live_heap_data_structures();
    }

    // Current compaction policy is as follows
    //      * regular collections use incremental compaction by default (collect all heap, compact only 1/16th) 
    //      * System.gc() collections use full heap compaction by default
    //      * -Dgc.no_full_compaction_at_force_gc=1 makes System.gc() use incremental compaction
    //      * -Dgc.incremental_compaction=0 makes all compactions non-incremental (full heap)
    //      * -Dgc.fixed=1 (or -Xgc fixed) makes all collections non-compacting
    //
    // FIXME salikh: make property names more readable and consistent

    // Are we doing compaction this GC?
    // compaction_this_gc is valid for this collection only.
    bool compaction_this_gc = compaction_enabled;

    LOG("compaction_this_gc = " << compaction_this_gc);
    
    if (compaction_this_gc) {
        if (full_heap_compaction || !incremental_compaction) {
            // full_heap_compaction forces the full heap compaction 
            // !incremental_compaction disables incremental compaction
            _p_block_store->set_compaction_type_for_this_gc(gc_full_heap_sliding_compaction);
            LOG("gc_full_heap_sliding_compaction");
        } else {
            // incremental compaction is default
            _p_block_store->set_compaction_type_for_this_gc(gc_incremental_sliding_compaction);
            LOG("gc_incremental_sliding_compaction");
        }
    } else {
        _p_block_store->set_compaction_type_for_this_gc(gc_no_compaction);
        LOG("gc_no_compaction");
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    gc_time_start_hook(&_start_time);
    // Initialize all GC threads for this cycle...tell GC threads if compaction is happening in this GC
    reset_gc_threads(compaction_this_gc);
    gc_time_end_hook("Reset GC Threads", &_start_time, &_end_time);
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    gc_time_start_hook(&_start_time);
    clear_roots();
    if (compaction_this_gc) {
        save_root_set.clear();
        number_of_to_compaction_blocks = 0;
        number_of_from_compaction_blocks = 0;
    }
    gc_time_end_hook("Reset root containers", &_start_time, &_end_time);
    
    // Stop-the-world begins just now!!!
    gc_time_start_hook(&_gc_start_time);
    last_user_time = _gc_start_time - _gc_end_time;
    
    gc_time_start_hook(&_start_time);

    // Stop the threads and collect the roots.
    get_vm_live_references(); 
    
    unsigned int total_LOB_bytes_recovered = 0;
    volatile GC_Thread_Info *temp = NULL;

    INFO2("gc.roots", "Number of roots enumerated by VM = " << num_roots_added);
    gc_time_end_hook("Root Set Enum", &_start_time, &_end_time);
    num_roots_added = 0; // zero out for the next GC cycle
    if (compaction_this_gc) {
        // Save roots away for repointing if they point to compacted areas
        save_root_set = root_set.vector();
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    if (characterize_heap) {
        heapTraceBegin(true);
    }
    
    // All the roots have been obtained from VM.
    if (verify_gc || verify_live_heap) {
        gc_time_start_hook(&_start_time);
        verify_root_set = root_set.vector();
        unsigned int lives_before_gc = trace_verify_heap(true);
        INFO(lives_before_gc << "  live objects, " << live_size << " live bytes "
            "before starting GC[" << _gc_num-1 << "]");
        if (verify_live_heap) {
            take_snapshot_of_lives_before_gc(
                _num_live_objects_found_by_first_trace_heap, 
                _live_objects_found_by_first_trace_heap);
        }
        gc_time_end_hook("verification trace before collection", &_start_time, &_end_time);
    }
    
    if (gc_stats) {
        dump_chunk_statistics();
        _p_block_store->characterize_blocks_in_heap();
    }
    if (object_stats) {
        dump_object_statistics();
    }
        
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    gc_time_start_hook(&_start_time);
    clear_all_mark_bit_vectors_of_unswept_blocks();
    gc_time_end_hook("clear_all_mark_bit_vectors_of_unswept_blocks", &_start_time, &_end_time);
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (compaction_this_gc) {
        gc_time_start_hook(&_start_time);
        // Pick blocks to compact during this GC excluding any in the m_pinned_blocks set.
        _p_block_store->determine_compaction_areas_for_this_gc(m_pinned_blocks);
        gc_time_end_hook("determine_compaction_areas_for_this_gc", &_start_time, &_end_time);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (mark_scan_load_balanced) {
        gc_time_start_hook(&_start_time);
        setup_mark_scan_pools();
        gc_time_end_hook("setup_mark_scan_pools", &_start_time, &_end_time);
    }
    
    // Lets begin the MARK SCAN PHASE
    
    gc_time_start_hook(&_start_time);
    // Wake up GC threads and get them to start doing marking and scanning work
    get_gc_threads_to_begin_task(GC_MARK_SCAN_TASK);
    wait_for_gc_threads_to_complete_assigned_task();
    // All marking of the heap should be done.
    gc_time_end_hook("MarkScan", &_start_time, &_end_time);

    _p_block_store->live_object_iterator_mark_bits_valid = true;

    // Various kinds of weak reachability are implemented as a many-step process:
    // 1 first all strongly reachable objects are marked
    // 2 soft references are considered, and either
    //   * soft references are reset
    //   * soft references are kept untouched and 
    //     referents of kept soft references are revived by marking them.
    //     Marked objects do not participate in the subsequent steps of the process.
    // 3 weak references are considered

    // Since soft references are most strong in the sense of keeping objects
    // alive, we should process them first. Thus if soft reference is not reset,
    // then other kinds of weak references aren't concerned at all.
    LOG("processing soft references");
    gc_time_start_hook(&_start_time);
    process_soft_references(compaction_this_gc);
    gc_time_end_hook("processing soft references", &_start_time, &_end_time);

    // Weak references are processed after soft references, but before
    // finalizers or phantom references. This should be performed before 
    // objects are placed to finalizer queue, because objects are "revived" (marked)
    // during finalizers considerations, in order to prevent objects from being 
    // collected before finalization is complete. An if the object is marked,
    // then we don't have a chance to know whether we should reset weak reference,
    // hence we need to process weak references before finalizers.
    LOG("processing weak references");
    gc_time_start_hook(&_start_time);
    process_weak_references(compaction_this_gc);
    gc_time_end_hook("processing weak references", &_start_time, &_end_time);

    // Since short weak roots don't track an object into or while on the finalizable
    // queue, we process short weak roots here.  Those roots will be NULLed (if not
    // marked) or added to the list of roots to potentially be updated later.
    // See comments before process_weak_roots();
    LOG("processing short weak roots before finalization");
    gc_time_start_hook(&_start_time);
    process_weak_roots(true,compaction_this_gc);
    gc_time_end_hook("processing short weak roots", &_start_time, &_end_time);

    // Identify the objects that need to be moved to the finalizable queue.
    // Mark and scan any such objects.
    LOG("processing finalizable queue");
    gc_time_start_hook(&_start_time);
    identify_and_mark_objects_moving_to_finalizable_queue(compaction_this_gc);
    gc_time_end_hook("processing finalizable queue", &_start_time, &_end_time);

    // Since long weak roots DO track an object into or while on the finalizable
    // queue, we process long weak roots here.  Those roots will be NULLed (if not
    // marked) or added to the list of roots to potentially be updated later.
    // See comments before process_weak_roots();
    LOG("processing long weak roots after finalization");
    gc_time_start_hook(&_start_time);
    process_weak_roots(false,compaction_this_gc);
    gc_time_end_hook("processing long weak roots", &_start_time, &_end_time);

    // Phantom references are processed last, in order to reset them only in case
    // all other kinds of references were cleared and finalizers run.
    gc_time_start_hook(&_start_time);
    process_phantom_references(compaction_this_gc);
    gc_time_end_hook("processing phantom references", &_start_time, &_end_time);

    gc_time_start_hook(&_start_time);
    // Finally register referent slots in all live references for update during compaction
    register_referent_slots(soft_references, compaction_this_gc);
    register_referent_slots(weak_references, compaction_this_gc);
    register_referent_slots(phantom_references, compaction_this_gc);
    register_referent_slots(references_to_enqueue, compaction_this_gc);
    gc_time_end_hook("registering referent slots", &_start_time, &_end_time);

    gc_time_start_hook(&_start_time);
    // class unloading support
    _p_block_store->init_live_objects_in_heap_iterator();
    vm_classloader_iterate_objects((void*)_p_block_store);
    gc_time_end_hook("iterating live objects to VM", &_start_time, &_end_time);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (verify_gc) {
        // NEED to verify that no duplicate slots are collected by mark/scan phase....
        _p_block_store->verify_no_duplicate_slots_into_compaction_areas();
    }
    ////////////////////////// M A R K S   C H E C K   A F T E R    M A R K / S C A N ///////////////////////////////////////////
    
    LOG("verifying marks for live objects");
    verify_marks_for_all_lives();
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (mark_scan_load_balanced) {
        _mark_scan_pool->verify_after_gc();
        _verify_gc_threads_state();
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    if (compaction_this_gc) {
        LOG("starting compaction");
        
        gc_time_start_hook(&_start_time);
        get_gc_threads_to_begin_task(GC_ALLOCATE_FORWARDING_POINTERS_FOR_COMPACTION_LIVE_OBJECTS_TASK);
        wait_for_gc_threads_to_complete_assigned_task();
        gc_time_end_hook("GC_ALLOCATE_FORWARDING_POINTERS_FOR_COMPACTION_LIVE_OBJECTS_TASK", &_start_time, &_end_time);
        
        gc_time_start_hook(&_start_time);
        get_gc_threads_to_begin_task(GC_FIX_SLOTS_TO_COMPACTION_LIVE_OBJECTS_TASK);
        wait_for_gc_threads_to_complete_assigned_task();
        gc_time_end_hook("GC_FIX_SLOTS_TO_COMPACTION_LIVE_OBJECTS_TASK", &_start_time, &_end_time);
        
        gc_time_start_hook(&_start_time);
        // Now repoint all root slots if the root objects that they pointed to will be slide compacted.
        // this should be small and we should be able to do this as part of the main thread with little overhead
        repoint_all_roots_into_compacted_areas();
        repoint_all_roots_with_offset_into_compacted_areas();

        gc_time_end_hook("Repoint root slots during compaction", &_start_time, &_end_time);

        _p_block_store->live_object_iterator_mark_bits_valid = false;

        gc_time_start_hook(&_start_time);
        get_gc_threads_to_begin_task(GC_SLIDE_COMPACT_LIVE_OBJECTS_IN_COMPACTION_BLOCKS);
        wait_for_gc_threads_to_complete_assigned_task();
        gc_time_end_hook("GC_SLIDE_COMPACT_LIVE_OBJECTS_IN_COMPACTION_BLOCKS", &_start_time, &_end_time);
       
        gc_time_start_hook(&_start_time);
        get_gc_threads_to_begin_task(GC_RESTORE_HIJACKED_HEADERS);
        wait_for_gc_threads_to_complete_assigned_task();
        gc_time_end_hook("GC_RESTORE_HIJACKED_HEADERS", &_start_time, &_end_time);
    }
    
////////////////////////////////////////// NOW SWEEP THE ENTIRE HEAP/////////////////////////////////////////////////////////
        if (sweeps_during_gc) {
            LOG("sweeping entire heap");
            gc_time_start_hook(&_start_time);
            // Divide all managed chunks equally among all threads and get them to sweep all of them.
            prepare_to_sweep_heap();
            // Wake up GC threads and get them to start doing sweep work
            get_gc_threads_to_begin_task(GC_SWEEP_TASK);
            wait_for_gc_threads_to_complete_assigned_task();
            INFO2("gc.sweep", "Chunks Sweep recovered total bytes -- " <<
                get_num_bytes_recovered_by_sweep());
            gc_time_end_hook("Chunks Sweep", &_start_time, &_end_time);
        } else {
            gc_time_start_hook(&_start_time);
            prepare_chunks_for_sweeps_during_allocation(compaction_this_gc);
            gc_time_end_hook("Prepare chunks for Allocation Sweeps", &_start_time, &_end_time);
        }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////// RESET the gc TLS state. /////////////////////////////////////////////////////////////

    //
    // Reset the gc_information fields for each thread that is active. This includes setting the current free and ceiling to NULL
    // and reset alloc block to the start of the chuck.
    // The gc m_malloc code should be able to deal with seeing free set to 0 and know that it needs to just go to the alloca block and
    // get the next free alloc area.
    //
    
    temp = p_global_gc->active_thread_gc_info_list;
    while (temp) {
        temp->tls_current_ceiling = NULL;     // Set to 0 so in allocation routine free (0) + size will be > ceiling (0);
        temp->tls_current_free = NULL;
        temp->curr_alloc_block = temp->chunk; // Restart using all the blocks in the chunk.
        temp = temp->p_active_gc_thread_info; // Do it for all the threads.
    }
    
    // Concurrent GC needs to do something else.
    
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    
    LOG("sweeping the LOS");
    // Sweep the LOS
    gc_time_start_hook(&_start_time);
    if (_los_blocks) {
        block_info *block = _los_blocks;
        // LOS blocks never get compacted
        assert(!is_compaction_block(block));
        while (block) {
            // XXX -- TO DO....change LOS allocation to use existing blocks...reorder them or something
            assert(block->in_los_p);
            total_LOB_bytes_recovered += sweep_one_block(block);
            block = block->next_free_block;
        }
    }
    // Sweep the "single object blocks"
    if (_single_object_blocks) {
        total_LOB_bytes_recovered += sweep_single_object_blocks(_single_object_blocks);
    }
    INFO2("gc.sweep", "total_LOB_bytes_recovered = " 
            << total_LOB_bytes_recovered);
    gc_time_end_hook("LOS Sweep", &_start_time, &_end_time);
    
    gc_time_start_hook(&_start_time);
    
    if (size_failed < GC_MAX_CHUNK_BLOCK_OBJECT_SIZE) {
        // do not force returning SmOS blocks to block store

        // Refresh block store if needed
        if (_p_block_store->get_num_free_blocks_in_block_store() < _p_block_store->get_block_store_low_watermark_free_blocks()) {
            return_free_blocks_to_block_store(_p_block_store->get_block_store_low_watermark_free_blocks(), false, compaction_this_gc);
        }
    } else {
        // Large object allocation failed, we need to return all free blocks to block store
        return_free_blocks_to_block_store(_p_block_store->get_num_total_blocks_in_block_store(), false, compaction_this_gc);
    }
    LOG("Block store has " << _p_block_store->get_num_free_blocks_in_block_store() << " free blocks now\n"
        << "#allocatable chunks = " << _free_chunks_end_index + 1);
    if (size_failed > GC_MAX_CHUNK_BLOCK_OBJECT_SIZE) {
        // a single object block allocation failed...see if we can satisfy request based on what we have in the BS
        int count = 0;
        while (true) {
            count++;
            if (count > 2) {
                INFO("can't allocate large object of size " << size_failed);
                break;
            }
            // Keep doing below until we can satisfy the request that failed and thus caused GC
            _p_block_store->coalesce_free_blocks();
            unsigned int number_of_blocks_needed = (size_failed / _p_block_store->get_block_size_bytes()) + 1;
            if (_p_block_store->block_store_can_satisfy_request(number_of_blocks_needed) == false) {
                // return twice as many blocks to BS
                return_free_blocks_to_block_store(number_of_blocks_needed * 2, false, compaction_this_gc);
            } else {
                INFO2("gc.chunks", "Block store has after recycling and coalescing as needed " 
                    << _p_block_store->get_num_free_blocks_in_block_store() << " free blocks now"
                    << "#allocatable chunks = " << _free_chunks_end_index + 1);
                break;
            }
        }
    }
    gc_time_end_hook("Block store management after GC", &_start_time, &_end_time);
    
    if (verify_gc || verify_live_heap) {
        gc_time_start_hook(&_start_time);
        unsigned int lives_after_gc = trace_verify_heap(false);
        INFO(lives_after_gc << "  live objects, " << live_size << " live bytes "
            "after GC[" << _gc_num - 1 << "]");
        if (verify_live_heap) {
            verify_live_heap_before_and_after_gc(_num_live_objects_found_by_second_trace_heap, _live_objects_found_by_second_trace_heap);
        }
        gc_time_end_hook("verification trace after collection", &_start_time, &_end_time);
    }
  
    if (characterize_heap) {
        heapTraceEnd(false);    
    }

    gc_time_start_hook(&_start_time);
    // Earlier, we may have added objects to a temporary list of objects that require finalization.
    // We did this so that we could take pointers to the interior of this list and use those as roots
    // that could be updated if compaction is enabled.
    // Now that we know we have (potentially) updated roots, we can added them to the finalizable queue.
    add_objects_to_finalizable_queue();

    // The same goes for enqueued references
    enqueue_references();

    gc_time_end_hook("transferring finalizable and reference queue to VM", &_start_time, &_end_time);

    if (gc_stats) {
        dump_chunk_statistics();
        _p_block_store->characterize_blocks_in_heap();
    }
    if (object_stats) {
        dump_object_statistics();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Restart mutators

    _p_block_store->live_object_iterator_mark_bits_valid = false;
    resume_vm();
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////
    last_gc_time = gc_time_end_hook("GC time", &_gc_start_time, &_gc_end_time);
    notify_gc_end();
    ////////////////////////////////////////////////////////////////////////
    
    gc_time_start_hook(&_start_time);
    consider_heap_resize(size_failed);
    gc_time_end_hook("Heap resize ", &_start_time, &_end_time);

    if (compaction_this_gc) {
        // Zero out live bit lists and reverse compaction information in BS...this can be done concurrently since we hold the GC lock right now.
        gc_time_start_hook(&_start_time);
        _p_block_store->reset_compaction_areas_after_this_gc();
        gc_time_end_hook("Reset compaction areas after GC", &_start_time, &_end_time);
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // set hint to launch finalization
    hint_to_finalize = true;
} // ::Garbage_Collector::reclaim_full_heap
