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
#include <apr_atomic.h>
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

#include "compressed_references.h"
#include "characterize_heap.h"
#include "port_malloc.h"

#include "set"

///////////////////////////////////////////////////////////////////////////////////

int num_los_objects = 0;
int num_large_objects = 0;

extern bool sweeps_during_gc;
extern bool ignore_finalizers;

///////////////////////////////////////////////////////////////////////////////////


static inline bool
gc_has_begun()
{
    return (get_global_safepoint_status() == enumerate_the_universe);
}

static inline void * UNUSED get_thread_curr_alloc_block(void *tp)
{
    GC_Thread_Info *info = (struct GC_Thread_Info *) /*vm_get_gc_thread_local()*/ tp;
    return info->curr_alloc_block;
}

static inline void *get_thread_alloc_chunk(void *tp)
{
    GC_Thread_Info *info = (struct GC_Thread_Info *) /*vm_get_gc_thread_local()*/ tp;
    return info->chunk;
}

static inline void set_thread_curr_alloc_block(void *tp, void *data)
{
    GC_Thread_Info *info = (struct GC_Thread_Info *) /*vm_get_gc_thread_local()*/ tp;
    info->curr_alloc_block = data;
}

static inline void set_thread_alloc_chunk(void *tp, void *data)
{
    GC_Thread_Info *info = (struct GC_Thread_Info *) /*vm_get_gc_thread_local()*/ tp;
    info->chunk = data;
}

///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////


int obj_num = 0;


inline Partial_Reveal_Object *fast_gc_malloc_from_thread_chunk_or_null(unsigned size, Allocation_Handle ah, GC_Thread_Info *gc_tls)
{
    Partial_Reveal_Object *frontier = (Partial_Reveal_Object *)gc_tls->tls_current_free;
    POINTER_SIZE_INT new_free = (size + (POINTER_SIZE_INT)frontier);
    // check that the old way and the new way are producing
    // the same pointers. This can go once we have it in place.
    
    if (new_free <= (POINTER_SIZE_INT) gc_tls->tls_current_ceiling) {
        // success...
         frontier->set_vtable(ah);
         // increment free ptr and return object
        gc_tls->tls_current_free = (void *) new_free;
        // Heap characterization is done only in the exported routine
        gc_trace_allocation(frontier, "Allocated in fast_gc_malloc_from_thread_chunk_or_null");
        return frontier;
    } else {
        return NULL;
    }
}

inline Partial_Reveal_Object *
gc_malloc_from_thread_chunk_or_null(unsigned size, Allocation_Handle ah, GC_Thread_Info *tls_for_gc)
{
    // Try to allocate an object from the current alloc block.
    assert(size);
    assert (ah);

    // Get the gc tls structure. Review of what each piece of the structure is used for.
    // chunk - This holds a list of blocks linked throuh the  next_free_block fields
    // Each block is an alloc_block and it has various areas where allocation can happen.
    // The block might not have been swept which means that the allocation areas have
    // not be initialized. 
    // The tls holds the tls_current_free and the tls_current_ceiling which are used
    // to allocate within a given allocation area within a given allocation block within
    // a chunk.
    
    // The tls ceiling and free are set to zero to zero and all the blocks in the 
    // allocation chunk are either swept by the GC or have their block_has_been_swept
    // set to false by the GC.
    
    // The fast case just grabs the tls free pointer and increments it by size. If it is
    // less than ceiling then the object fits and we install the vtable.
    
    // If the object does not fit, then we go the the tls and grab the alloc_block and
    // see if it has another area available. If it doesn't we move on to the next block in the
    // chunk. If we run our of blocks in the chunk we return NULL;
    
    // The corner case is what happens right after a GC. Since free and ceiling are set to 0
    // the fast case will add size to 0 and compare against a 0 for ceiling and fall through.
    // The GC will have reset alloc block to the first block in the chunk and that is where
    // we will look for a new area to allocate in.
    
    // Someday this will be part of the interface.
    
    Partial_Reveal_Object *p_return_object = NULL;
    
    p_return_object = fast_gc_malloc_from_thread_chunk_or_null(size, ah, tls_for_gc);
    if (p_return_object) {
        // fast case was successful.
        // Heap characterization is done only in the exported routine        
        gc_trace_allocation(p_return_object, "Returned from gc_malloc_from_thread_chunk_or_null");
        return p_return_object;
    }
    
    // We need a new allocation area. Get the current alloc_block
    block_info *alloc_block = (block_info *)tls_for_gc->curr_alloc_block;
    
    // If the object size is less than the minimum alloc area just return NULL, If we have a free area we
    // want to know that we can allocate any object that makes it past here in it.....
    if (size > GC_MAX_CHUNK_BLOCK_OBJECT_SIZE) {
        // PINNED????
        return NULL;
    }
    // Loop through the alloc blocks to see if we can find another allocation area.
    while (alloc_block) {
        
        // We will sweep blocks only right before we start using it. This seems to have good cache benefits
        
        if (!sweeps_during_gc) {
            // Sweep the block
            if (alloc_block->block_has_been_swept == false) {
                // Determine allocation areas in the block
                p_global_gc->sweep_one_block(alloc_block);
                alloc_block->block_has_been_swept = true;
            }
        }
        
        // current_alloc_area will be -1 if the first area has not been used and if it exists is available. 
        if ( (alloc_block->num_free_areas_in_block == 0) || ((alloc_block->current_alloc_area + 1) == alloc_block->num_free_areas_in_block) ){
            // No areas left in this block get the next one.
            alloc_block = alloc_block->next_free_block; // Get the next block and loop
        } else {
            assert (alloc_block->current_alloc_area < alloc_block->num_free_areas_in_block);
            break; // This block has been swept and has an untouched alloc block.
        }
    } // end while (alloc_block)
    
    if (alloc_block == NULL) {
        // ran through the end of the list of blocks in the chunk
        tls_for_gc->tls_current_ceiling = NULL;
        tls_for_gc->tls_current_free = NULL;
        tls_for_gc->curr_alloc_block = NULL; // Indicating that we are at the end of the alloc blocks for this chunk.
        // This is the last place we can return NULL from.
        return NULL;
    }
    
    assert(alloc_block->num_free_areas_in_block > 0);
    assert(alloc_block->block_has_been_swept);
    assert(alloc_block->current_alloc_area < alloc_block->num_free_areas_in_block);
    
    // We have a block that has been swept and has at least one allocation area big enough to fit this object so we will be
    // successful..
    
    alloc_block->current_alloc_area++; // Get the next currenct area. If it is the first one it will be 0.
    
    //XXX assert part of GC_SLOW_ALLOC routines.
    assert (alloc_block->current_alloc_area != -1);
    unsigned int curr_area = alloc_block->current_alloc_area;
    
    
    if (alloc_block->block_free_areas[curr_area].has_been_zeroed == false) {
        gc_trace_block(alloc_block, " Clearing the curr_area in this block.");
        // CLEAR allocation areas just before you start using them
        memset(alloc_block->block_free_areas[curr_area].area_base, 0, alloc_block->block_free_areas[curr_area].area_size);
        alloc_block->block_free_areas[curr_area].has_been_zeroed = true;
    }
    
    tls_for_gc->tls_current_free = alloc_block->block_free_areas[curr_area].area_base;
    // ? 2003-05-23.  If references are compressed, then the heap base should never be
    // given out as a valid object pointer, since it is being used as the
    // representation for a managed null.
    assert(tls_for_gc->tls_current_free != Slot::managed_null());
    tls_for_gc->tls_current_ceiling = alloc_block->block_free_areas[curr_area].area_ceiling;
    tls_for_gc->curr_alloc_block = alloc_block;
    
    p_return_object = fast_gc_malloc_from_thread_chunk_or_null(size, ah, tls_for_gc);

    // Heap characterization is done only in the exported routine
    gc_trace_allocation(p_return_object, "Returned from gc_malloc_from_thread_chunk_or_null at bottom");

    return p_return_object;
} // gc_malloc_from_thread_chunk_or_null

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


Managed_Object_Handle gc_alloc_fast (unsigned size, Allocation_Handle ah, void *tp)
{
    // All requests for space should be multiples of 4 (IA32) or 8(IPF)
    assert((size % GC_OBJECT_ALIGNMENT) == 0);
 
    //
    // Try to allocate an object from the current chunk.
    // This can fail if the object won't fit in what is left of the 
    // chunk. 
    
    assert(tp == vm_get_gc_thread_local());
    
    bool is_finalizable = !ignore_finalizers && class_is_finalizable(allocation_handle_get_class(ah));
    Partial_Reveal_Object *p_return_object = 
            gc_malloc_from_thread_chunk_or_null(size, ah, (GC_Thread_Info *)tp);

   if (p_return_object) {  
        assert(p_return_object->vt());
    }

    if (characterize_heap) {
        if (p_return_object) {
            assert(ah);
            assert(size);
            heapTraceAllocation(p_return_object, size);
        }
    }    

    // Put the object in the finalize set if it needs a finalizer run eventually.
    if (p_return_object && is_finalizable) {
        gc_trace(p_return_object, "adding to finalization queue");
        TRACE2("gc.finalize", "adding " << p_return_object << " ("
                << p_return_object->class_name() << ") to finalization queue");
        p_global_gc->add_finalize_object(p_return_object);
    }
    
    gc_trace_allocation(p_return_object, "gc_alloc_fast returns this object");
    
    //
    // return the object we just allocated. It may be NULL and we fall into
    // the slow code of gc_alloc.
    //
    
    return p_return_object;
} // gc_alloc_fast

static inline void reset_thread_alloc_area(void* tp) {
    GC_Thread_Info *tls_for_gc = (GC_Thread_Info *)tp;
    
    // Set to NULL so allocation code will find the new block.
    tls_for_gc->tls_current_free = NULL;
    tls_for_gc->tls_current_ceiling = NULL;
}

// If there are no alignment or pinning constraints but possible finalization 
// or weak ref constraints then one can use this routine.

Managed_Object_Handle gc_malloc_slow_no_constraints (unsigned size, Allocation_Handle ah, void *tp)
{
    Partial_Reveal_Object *p_return_object = NULL;

    // See if this is a large object which needs special treatment:
    if (size >= GC_MAX_CHUNK_BLOCK_OBJECT_SIZE) {
        p_return_object = gc_pinned_malloc(size, ah, false, false);
        // Characterization done in exported caller 
        gc_trace_allocation(p_return_object, "gc_malloc_slow_no_constraints() returns this object");
        return p_return_object;
    }

    GC_Thread_Info *tls_for_gc = (GC_Thread_Info *)tp;

    int attempts = 2;
     
    do { 
        block_info* chunk = (block_info *)get_thread_alloc_chunk(tp);
        int chunk_count = 0;
        while (NULL != chunk) {
            chunk_count += 1;

            p_return_object = gc_malloc_from_thread_chunk_or_null(size, ah, tls_for_gc);
            if (p_return_object) {
                // Characterization done in exported caller 
                gc_trace_allocation (p_return_object, "gc_malloc_slow_no_constraints returns this object.");
                return p_return_object;
            }

            set_thread_alloc_chunk(tp, NULL);
            chunk = p_global_gc->p_cycle_chunk(chunk);
            set_thread_alloc_chunk(tp, chunk);
            set_thread_curr_alloc_block(tp, chunk);
            reset_thread_alloc_area(tp);
        };
        // No more chunks available, collect garbage

        if (chunk_count > 0) {
            LOG2("gc", "Tried " << chunk_count << " chunks to allocate " << size << " bytes, but failed");
        }


        // Other threads may do a collection while we are blocked here
        vm_gc_lock_enum();

        // Check to see if someone has beaten me to this and 
        // added free chunks to the global chunk store
        chunk = p_global_gc->p_cycle_chunk(NULL); 
        if (NULL == chunk) {

            int free_blocks = p_global_gc->_p_block_store->get_num_free_blocks_in_block_store();
            int chunks_added = 0;
            if (free_blocks > 0) {
                // gc_add_fresh_chunks must be called under gc lock!
                chunks_added = p_global_gc->gc_add_fresh_chunks(1);
            }

            if (chunks_added > 0) {
                // 1. We managed to add new chunk
                LOG2("gc", "Added fresh chunk, retrying");
            } else if (attempts > 1) {
                // 2. Then do a regular collection (incremental compaction)
                INFO2("gc", "Attempting normal collection");
                p_global_gc->reclaim_full_heap(0, false);
                attempts -= 1;
            } else if (attempts > 0) {
                // 3. Finally fall back to a full heap collection
                INFO2("gc", "Attempting full heap collection");
                p_global_gc->reclaim_full_heap(0, true);
                attempts -= 1;
            } else {
                INFO2("gc", "Giving up");
                attempts -= 1;
            }
            chunk = p_global_gc->p_cycle_chunk(chunk);
        }

        set_thread_alloc_chunk(tp, chunk);
        set_thread_curr_alloc_block(tp, chunk);
        reset_thread_alloc_area(tp);

        if (NULL == chunk) {
            LOG2("gc", "Could not allocate chunk even under gc lock");
        }

        vm_gc_unlock_enum();

    } while (attempts >= 0);
    
    return NULL;

} // gc_malloc_slow_no_constraints

Partial_Reveal_Object *
Garbage_Collector::create_single_object_blocks(unsigned size, Allocation_Handle ah) 
{
    num_large_objects++;
    
    Partial_Reveal_Object *result = NULL;
    // ***SOB LOOKUP*** p_get_multi_block sets the _blocks_in_block_store[sob_index + index].is_single_object_block to true.
    block_info *block = _p_block_store->p_get_multi_block (size, false);   // Do not extend the heap yet.

    if (block == NULL) {
        return NULL;
    }
    assert(block->block_free_areas == NULL);
    
    block->in_los_p = false;    
    block->in_nursery_p = false;
    block->is_single_object_block = true;
    
    block->next_free_block = _single_object_blocks;
    _single_object_blocks = block;

    // Use this to keep track....block->number_of_blocks is total together
    block->los_object_size = size;
    Partial_Reveal_Object *p_obj_start = (Partial_Reveal_Object *)GC_BLOCK_ALLOC_START(block);
    memset (p_obj_start, 0, size); // Clear the new object.
    result = p_obj_start;
    result->set_vtable(ah);
    assert (result);

    
    block->curr_free = (void *) ((POINTER_SIZE_INT)p_obj_start + size);
    block->curr_ceiling = (void *) ((POINTER_SIZE_INT)(GC_BLOCK_INFO (block->curr_free)) + GC_BLOCK_SIZE_BYTES - 1);

    // Characterization done in exported caller 
    gc_trace_allocation (result, "create_single_object_blocks returns this object.");

    return result;
}

block_info *
Garbage_Collector::get_new_los_block()
{
    block_info *block = _p_block_store->p_get_new_block (false);
    
    if (block == NULL) {
        return NULL;
    }
    block->in_nursery_p = false;
    block->in_los_p = true;
    block->is_single_object_block = false;
    
    // Blocks in BS always dont have this
    assert(block->block_free_areas == NULL);
    
    // Allocate free areas per block
    block->block_free_areas = (free_area *)STD_MALLOC(sizeof(free_area) * GC_MAX_FREE_AREAS_PER_BLOCK);
    if (!(block->block_free_areas)) {
        DIE("STD_MALLOC failed");
    }
    
    gc_trace_block (block, " calling clear_block_free_areas in get_new_los_block.");
    // Initialize free areas for block....
    clear_block_free_areas(block);
    
    // JUST ONE LARGE FREE AREA initially
    free_area *area = &(block->block_free_areas[0]);
    area->area_base = GC_BLOCK_ALLOC_START(block);
    area->area_ceiling = (void *)((POINTER_SIZE_INT)GC_BLOCK_ALLOC_START(block) + (POINTER_SIZE_INT)GC_BLOCK_ALLOC_SIZE - 1);
    area->area_size = (unsigned int)((POINTER_SIZE_INT) area->area_ceiling - (POINTER_SIZE_INT) area->area_base + 1);
    block->num_free_areas_in_block = 1;
    block->current_alloc_area = 0;  
    
    // Start allocation in this block at the base of the first and only area
    block->curr_free = area->area_base;
    block->curr_ceiling = area->area_ceiling;
    
    return block;
}

// May cause GC if it cant get an LOS block or a multi block from the block store.

Partial_Reveal_Object  *
gc_pinned_malloc(unsigned size, 
                 Allocation_Handle ah,
                 bool UNREF return_null_on_fail,
                 bool UNREF double_align
                 )
{
    
    
    while (apr_atomic_cas32((uint32 *)(&p_global_gc->_los_lock), 1, 0) == 1) {
        // spin on the lock until we have the los lock
        // A thread may have caused GC in here if the LOS couldnt get a new block or a LARGE block.
        // So check if GC has begun, enable GC and block on GC lock if needed.
        
        if (gc_has_begun()) {
            vm_gc_lock_enum();
            vm_gc_unlock_enum();
        }
    }
    
    if (size > GC_MAX_CHUNK_BLOCK_OBJECT_SIZE) {
        
        Partial_Reveal_Object *p_obj = NULL;

        int count = 0;

        while (p_obj == NULL) {
            
            p_obj = p_global_gc->create_single_object_blocks(size, ah);
            if (p_obj == NULL) {
                count++;    // increase garbage collection counter
                if (count>2) {
                    // already tried to collect garbage twice
                    // and still could not get block
                    INFO("out of memory, can't allocate large object, size = " << size);
                    p_global_gc->_los_lock = 0;
                    return NULL;
                }

                // Force full compaction after first failure
                bool full_compaction = (count > 1);

                // Do a collection
                vm_gc_lock_enum();
                p_global_gc->reclaim_full_heap(size, full_compaction);
                vm_gc_unlock_enum();
            }
        }
        p_global_gc->_los_lock = 0;

        // heap characterization done in exported caller
        gc_trace_allocation (p_obj, "gc_pinned_malloc returns this object.");
        return p_obj;
    }
    
    num_los_objects++;
    
    if (p_global_gc->_los_blocks == NULL) {
        p_global_gc->_los_blocks = p_global_gc->get_new_los_block();
    }
    
    if (!p_global_gc->_los_blocks) {
        LOG("out of memory, can't get new los block");
        p_global_gc->_los_lock = 0;
        return NULL;
    }
    
    Partial_Reveal_Object *p_return_object = NULL;
    
    while (p_return_object == NULL) {
        
        block_info *los_block = p_global_gc->_los_blocks;
        /// XXX GC_SLOW_ALLOC
        if (los_block->current_alloc_area == -1){
            los_block->current_alloc_area = 0;
        }
        /// XXXXXXXX GC_SLOW_ALLOC
        assert(los_block->current_alloc_area != -1);
        if ((los_block->num_free_areas_in_block == 0) || 
            (los_block->current_alloc_area == los_block->num_free_areas_in_block)) { // Topped out on this block
            // Get new LOS block and hook it in
            block_info *block = NULL;
            int count = 0;  ///< a number of times garbage collection was triggered
            while (block == NULL) {
                block = p_global_gc->get_new_los_block();
                if (block == NULL) {                
                    
                    count++;    // increase garbage collection counter
                    if (count>2) {
                        // already tried to collect garbage twice
                        // and still could not get block
                        INFO("out of memory, can't allocate small fixed object");
                        p_global_gc->_los_lock = 0;
                        return NULL;
                    }
                    
                    // do a collection since can't get a block
                    vm_gc_lock_enum();
                    
                    // force full compaction after first failure
                    bool full_compaction = (count > 1);

                    p_global_gc->reclaim_full_heap(size, full_compaction);
                    vm_gc_unlock_enum();
                }
            }   
            
            block->next_free_block = p_global_gc->_los_blocks;
            p_global_gc->_los_blocks = block;
            // Go ahead with the new block
            los_block = p_global_gc->_los_blocks;
            assert(los_block->num_free_areas_in_block > 0);
        }
        
        assert(los_block->curr_free);
        assert(los_block->curr_ceiling);
        // = is possible if the previously allocated object exactly finished off the region
        assert(los_block->curr_free <= los_block->curr_ceiling);
        // XXXXXXXXXXXX GC_SLOW_ALLOC
        if (los_block->current_alloc_area == -1) {
            los_block->current_alloc_area = 0;
        }
        
        do {
            POINTER_SIZE_INT new_free = (size + (POINTER_SIZE_INT)los_block->curr_free);
            
            if (new_free <= (POINTER_SIZE_INT) los_block->curr_ceiling) {
                // success...

                p_return_object = (Partial_Reveal_Object *) los_block->curr_free;
                memset(p_return_object, 0, size); // This costs me 2%
                p_return_object->set_vtable(ah);
                // increment free ptr and return object
                los_block->curr_free = (void *) new_free;
                // Free LOS lock and leave
                p_global_gc->_los_lock = 0;
                // heap characterization done in exported caller
                gc_trace_allocation (p_return_object, "gc_pinned_malloc returns this object.");

                return p_return_object; 
                
            } else {
                // Move to next allocation area in block
                los_block->current_alloc_area++;
                los_block->curr_free = los_block->block_free_areas[los_block->current_alloc_area].area_base;
                los_block->curr_ceiling = los_block->block_free_areas[los_block->current_alloc_area].area_ceiling;
            }
            
        } while (los_block->current_alloc_area < los_block->num_free_areas_in_block);
        
    } // while 
    
    ABORT("Can't get here");
    // FREE the los lock and leave
    p_global_gc->_los_lock = 0;
    gc_trace_allocation (p_return_object, "gc_pinned_malloc returns this object.");
    return p_return_object;
}
