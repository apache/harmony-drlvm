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
#include "mark.h"
#include "gc_utils.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.verbose"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern bool cross_block_compaction;
extern bool use_large_pages;

extern void get_next_set_bit(set_bit_search_info *info);
extern void verify_get_next_set_bit_code(set_bit_search_info *);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// The variable force_unaligned_heap and all associated code
// are for measuring the performance benefit of having a 4GB-aligned heap when
// compressed references are used.  Since 4GB alignment practically always
// succeeds on IPF, we need a way to force misalignment.
bool force_unaligned_heap = false;

inline POINTER_SIZE_INT round_to_whole_blocks(POINTER_SIZE_INT size, int block_size) {
    return (size + block_size - 1) & ~(block_size - 1);
}

bool
Block_Store::init_heap(POINTER_SIZE_INT final_heap_size) {
    void *reserved_base = NULL;
    size_t page_size = PORT_VMEM_PAGESIZE_DEFAULT;
    if (use_large_pages)
    {
        size_t *ps = port_vmem_page_sizes();
        if (ps[1]) {
            page_size = PORT_VMEM_PAGESIZE_LARGE;
            final_heap_size = (final_heap_size + ps[1]-1)&
                ~(ps[1]-1); 
        }
    }

    // needed for block alignment
    size_t final_heap_size_extra = _block_size_bytes;

#ifdef PLATFORM_NT
// Getting virtual memory from Windows.  If large_pages is
// specified, we either acquire the entire heap with large pages or we exit.
// If compressed_references is specified, we ask for an additional 4GB and
// then adjust the result so that it is 4GB aligned.  If we don't manage to
// get that much, we default to asking for the original amount and accept that
// it's not 4GB aligned.  We only commit the actual heap size requested even
// though an extra 4GB is reserved, so the memory footprint still appears
// reasonable.
//
// Since VirtualAlloc with large pages seems to require committing up front,
// we actually waste 4GB.
    if (Slot::additional_padding_for_heap_alignment() > final_heap_size_extra) {
        final_heap_size_extra = (size_t) Slot::additional_padding_for_heap_alignment();
    }
#endif

    TRACE("reserve " << (void*)final_heap_size << " + " 
            << (void*)final_heap_size_extra << " = "
            << (void*)(final_heap_size + final_heap_size_extra) << " bytes");
    apr_status_t status = port_vmem_reserve(&allocated_memory, &reserved_base, 
        final_heap_size + final_heap_size_extra, 
        PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE, page_size, aux_pool);

    if (APR_SUCCESS != status) {
        if (final_heap_size_extra > 1024*1024*1024) {
            WARN("Failed to allocate 4GB-aligned memory with large pages.\n"
                << "  Trying non-aligned memory.\n"
                << "  (Error code was " << status << ")");
            // Try again without trying to align at a 4GB boundary.
            // Reduce the alignment restriction to block size
            final_heap_size_extra = _block_size_bytes;
            status = port_vmem_reserve(&allocated_memory, &reserved_base, 
                    final_heap_size + final_heap_size_extra, 
                    PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE, page_size, aux_pool);
        }
        if (APR_SUCCESS != status) {
            TRACE("Error: Garbage Collector failed to reserve "
                << (void *) (final_heap_size_bytes + final_heap_size_extra)
                << " bytes of virtual address space. Error code = "
                << status);
            return false;
        }
    }
    TRACE("reserved base = " << reserved_base);

    void* UNUSED reserved_ceiling = (void*)((char*)reserved_base + final_heap_size + final_heap_size_extra);
    TRACE("reserved ceiling = " << reserved_ceiling);

    void* aligned_base = reserved_base;
    if (final_heap_size_extra > 0) {

        // heap is aligned at 64kb or more
        assert((final_heap_size_extra&0xffff) == 0);

        aligned_base = (void*)(
                ((POINTER_SIZE_INT)reserved_base + final_heap_size_extra - 1) & 
                ~(final_heap_size_extra - 1) );

        if (force_unaligned_heap) {
            // If the original allocation was aligned, bump it forward by the page size.
            // Otherwise, revert to the original value.
            if (reserved_base == aligned_base) {
                assert(page_size < final_heap_size_extra);
                aligned_base = (void *) ((char *)reserved_base + page_size);
            }
            else {
                aligned_base = reserved_base;
            }
        }
    }

    // Must have got base aligned to block size
    assert(((POINTER_SIZE_INT)aligned_base & (_block_size_bytes - 1)) == 0);

    _p_heap_base = aligned_base;
    TRACE("heap base = " << _p_heap_base);
    
    _p_heap_ceiling = (void *) ((POINTER_SIZE_INT) aligned_base + final_heap_size);
    TRACE("heap ceiling = " << _p_heap_ceiling);
    
    return true;
}

Block_Store::Block_Store(POINTER_SIZE_INT initial_heap_size, POINTER_SIZE_INT final_heap_size, unsigned int block_size_bytes)
{
    assert(block_size_bytes == GC_BLOCK_SIZE_BYTES);
    assert(GC_BLOCK_INFO_SIZE_BYTES == GC_BLOCK_SIZE_BYTES /(GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES * GC_NUM_BITS_PER_BYTE));
    assert(sizeof(block_info) <= GC_BLOCK_INFO_SIZE_BYTES);

    _block_size_bytes = block_size_bytes;
    previous_compaction_last_destination_block = 0;

    // round the sizes to the whole blocks
    final_heap_size = round_to_whole_blocks(final_heap_size, block_size_bytes);
    initial_heap_size = round_to_whole_blocks(initial_heap_size, block_size_bytes);

    assert(final_heap_size >= block_size_bytes);
    assert(initial_heap_size >= block_size_bytes);
    assert(initial_heap_size <= final_heap_size);
    
    // If compressed vtable pointers are used, and moving GC is used,
    // then the heap size must be within 4GB.  This is because the
    // obj_info field in the object header needs to be used as a
    // forwarding offset.
    extern bool compaction_enabled;
    if (vm_vtable_pointers_are_compressed() && compaction_enabled &&
            final_heap_size > Partial_Reveal_Object::max_supported_heap_size()) {
        DIE("If compressed vtable pointers are used and moving GC is specified,\n"
            << "       the maximum heap size must be " <<
            (Partial_Reveal_Object::max_supported_heap_size() / (1025*1024)) << " Mb or less.");
    }
    
    // If compressed object references are used, then the heap size must be
    // within 4GB.  In the future, this limit could expand to 16GB or 32GB if
    // we exploit the fact that objects are aligned on a 4 or 8 byte boundary.
    if (gc_references_are_compressed && (final_heap_size - 1) > 0xFFFFffff) {
        DIE("If compressed referencess are used,"
            << "the maximum heap size must be less than 4GB.");
    }

    // Get information about the current system. (Page size, num processors, etc.)
    _get_system_info();

    allocated_memory = NULL;    
    apr_pool_create(&aux_pool, 0);

    if (!init_heap(final_heap_size)) {
        int dec = 100 * 1024 * 1024; // 100 mb
        // round heap to be multiply of 100 mb for readability
        final_heap_size = final_heap_size / dec * dec;
        while (!init_heap(final_heap_size)) {
            final_heap_size -= dec;
            VERIFY(final_heap_size > 0, "Are we able to allocate heap");
        }
        LECHO(16, "WARNING: final heap size is too large, reduced to {0} Mb" << mb(final_heap_size));
    }

    if (initial_heap_size > final_heap_size) {
        initial_heap_size = final_heap_size;
        LECHO(17, "WARNING: initial heap size reduced to {0} Mb" << mb(initial_heap_size));
    }

    // Check that ceiling is block aligned
    assert(((POINTER_SIZE_INT)_p_heap_ceiling & (block_size_bytes - 1)) == 0);

    assert(final_heap_size == 
            (POINTER_SIZE_INT)_p_heap_ceiling - (POINTER_SIZE_INT)_p_heap_base);
    
    assert(((POINTER_SIZE_INT)initial_heap_size & (block_size_bytes - 1)) == 0);
    assert(initial_heap_size > 0);

    _initialize_block_tables(_p_heap_base, initial_heap_size, final_heap_size);
    
    // Precommit a little space for VM initialization
    number_of_blocks_committed = 0;
    bool UNUSED r = commit_blocks(GC_MAX_BLOCKS_PER_CHUNK); assert(r);
    
    live_object_iterator_mark_bits_valid = false;

    INFO("java heap initial size " << mb(initial_heap_size) 
            << " Mb, maximum size " << mb(final_heap_size) 
            << " Mb (" << (void*)final_heap_size 
            << "h), addresses range " << _p_heap_base << " - "<< _p_heap_ceiling);
 
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    _heap_compaction_ratio = GC_FRACTION_OF_HEAP_INCREMENTAL_COMPACTED_DURING_EACH_GC;
    min_compaction_increment_size = GC_MIN_COMPACTION_INCREMENT_SIZE;
    const char *compaction_ratio = vm_get_property_value("heap_compaction_ratio");
    if (isdigit(compaction_ratio[0])) {
        _heap_compaction_ratio = strtoul(compaction_ratio, NULL, 0);
        INFO("The heap_compaction_ratio is set to " << _heap_compaction_ratio);
    }
    
    
    
    _compaction_type_for_this_gc = gc_bogus_compaction;
    _compaction_blocks_low_index = 0;
    // Compact an eigth of the heap each GC cycle
    _compaction_blocks_high_index = get_compaction_increment_size();
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    _per_gc_thread_compaction_blocks_low_index = (unsigned int *) STD_MALLOC(get_num_worker_threads() * sizeof(unsigned int));
    if (!(  _per_gc_thread_compaction_blocks_low_index)) {
        DIE("STD_MALLOC failed");
    }
    memset(_per_gc_thread_compaction_blocks_low_index, 0, get_num_worker_threads() * sizeof(unsigned int));
    _per_gc_thread_compaction_blocks_high_index = (unsigned int *) STD_MALLOC(get_num_worker_threads() * sizeof(unsigned int));
    if (!_per_gc_thread_compaction_blocks_high_index) {
        DIE("STD_MALLOC failed");
    }
    memset(_per_gc_thread_compaction_blocks_high_index, 0, get_num_worker_threads() * sizeof(unsigned int));
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // Record the heap base for compressed 32-bit pointers.
    // This requires a single contiguous block of memory, so be sure that
    // this isn't done twice.
    assert(Partial_Reveal_Object::heap_base == 0);
    Partial_Reveal_Object::heap_base = (POINTER_SIZE_INT) _p_heap_base;
    Slot::init(_p_heap_base, _p_heap_ceiling);
    
    return;
}


#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.bs"

Block_Store::~Block_Store()
{
    //
    // Release the memory we obtained for the GC heap, including
    // the ancilliary table(s).
    //
    TRACE("Block_Store::~Block_Store()");
    port_vmem_release(allocated_memory);
    apr_pool_destroy(aux_pool);
    STD_FREE(_per_gc_thread_compaction_blocks_low_index);
    STD_FREE(_per_gc_thread_compaction_blocks_high_index);
    for (unsigned int i = 0; i < _number_of_blocks_in_heap; i++) 
    {
        STD_FREE(_blocks_in_block_store[i].per_thread_slots_into_compaction_block);
#ifdef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
        STD_FREE(_blocks_in_block_store[i].per_thread_live_objects_in_compaction_block);
        STD_FREE(_blocks_in_block_store[i].num_per_thread_live_objects_in_compaction_block);
#endif
    }


}


unsigned int 
Block_Store::get_num_worker_threads()
{
    extern bool machine_is_hyperthreaded;
    extern int force_gc_worker_thread_number;
    
    if (0 != force_gc_worker_thread_number) {
        return force_gc_worker_thread_number;
    }

    if (machine_is_hyperthreaded) {
        return _number_of_active_processors / 2;
    } else {
        return _number_of_active_processors;
    }
}



static volatile void *block_store_lock = (void *)0;


// Gets the lock 
static inline void
get_block_store_lock()
{
    while (apr_atomic_casptr((volatile void **)&block_store_lock, 
        (void *)1, (void *)0) == (void *)1) {
        while (block_store_lock == (void *)1) {
            hythread_yield();
        }
    }
}

static inline void 
release_block_store_lock()
{
    assert (block_store_lock == (void *)1);
    block_store_lock = 0;
}



void Block_Store::initialize_block(unsigned index, void* block_address) {

        // NOTE: the block is cleaned up by system, no need to clean it.
        
        assert(index == get_block_index((block_info*)block_address));
        block_store_info& info = _blocks_in_block_store[index];

        info.block = (block_info *) block_address;

        info.block_is_free = true;
        info.number_of_blocks = 1;
        info.is_compaction_block = false;
        
        // per thread remembered sets/live object lists etc.
        info.per_thread_slots_into_compaction_block = (Remembered_Set **) STD_MALLOC(get_num_worker_threads() * sizeof(void *));
        if (!info.per_thread_slots_into_compaction_block) {
            DIE("STD_MALLOC failed for info.per_thread_slots_into_compaction_block ");
        }
        memset(info.per_thread_slots_into_compaction_block, 0, get_num_worker_threads() * sizeof(void *));

#ifdef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
        info.per_thread_live_objects_in_compaction_block = (live_object **) STD_MALLOC(get_num_worker_threads() * sizeof(void *));
        if (!info.per_thread_slots_into_compaction_block) {
            DIE("STD_MALLOC failed for info.per_thread_slots_into_compaction_block ");
        }
        memset(info.per_thread_live_objects_in_compaction_block , 0, get_num_worker_threads() * sizeof(void *));
        info.num_per_thread_live_objects_in_compaction_block = (unsigned int *) STD_MALLOC(get_num_worker_threads() * sizeof(unsigned int));
        if (!info.num_per_thread_live_objects_in_compaction_block ) {
            DIE("STD_MALLOC failed forinfo.num_per_thread_live_objects_in_compaction_block  ");
        }
        memset(info.num_per_thread_live_objects_in_compaction_block, 0, get_num_worker_threads() * sizeof(unsigned int));
        info.all_live_objects_in_block_fully_sorted = NULL;
#else
        info.total_live_objects_in_this_block = 0;
#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
        
}


void
Block_Store::_initialize_block_tables(void *p_heap_start_address, 
                                        POINTER_SIZE_INT heap_size_in_bytes, 
                                        POINTER_SIZE_INT max_heap_size)
{
    assert((max_heap_size & (GC_BLOCK_SIZE_BYTES - 1)) == 0);
    assert((heap_size_in_bytes & (GC_BLOCK_SIZE_BYTES - 1)) == 0);

    // Start off with small blocks each
    
    max_blocks_in_heap = (unsigned int)(max_heap_size / (POINTER_SIZE_INT)_block_size_bytes);
    _number_of_blocks_in_heap = (unsigned int)(heap_size_in_bytes / (POINTER_SIZE_INT)_block_size_bytes);
    
    if (_number_of_blocks_in_heap  >= GC_MAX_BLOCKS) {
        DIE("Internal error - attempt to allocate a heap larger "
            "than vm build permits, adjust GC_MAX_BLOCKS "
            "and rebuild vm or reduce heap size.");
    }
    
    _blocks_in_block_store.resize(max_blocks_in_heap);

    memset(&_blocks_in_block_store[0], 0, sizeof(block_store_info) * _blocks_in_block_store.size());
    
    void *block_address = p_heap_start_address;
    
    TRACE2("gc", "_number_of_blocks_in_heap = " << _number_of_blocks_in_heap);
    for (unsigned int i = 0; i < _number_of_blocks_in_heap; i++) {

        initialize_block(i, block_address);

        // move to next block
        block_address = (void *) ((POINTER_SIZE_INT)block_address + _block_size_bytes);
    }
    
    assert(block_address == (void *) ((POINTER_SIZE_INT) p_heap_start_address + heap_size_in_bytes));
    _num_free_blocks = _number_of_blocks_in_heap;
    TRACE2("gc.free", "_num_free_blocks = " << _num_free_blocks);
    // Initlialize the hint to the start of block list
    _free_block_search_hint = 0;
}

bool Block_Store::commit_blocks(unsigned target) {
    assert(number_of_blocks_committed < target);
    assert(target <= max_blocks_in_heap);

    POINTER_SIZE_INT commit_size = (target - number_of_blocks_committed) * GC_BLOCK_SIZE_BYTES;
    void* commit_base = (void*)((char*)_p_heap_base + number_of_blocks_committed * GC_BLOCK_SIZE_BYTES);

    void* committed_address = commit_base;
    apr_status_t status = port_vmem_commit(&committed_address, commit_size, allocated_memory);
    if (APR_SUCCESS != status) {
        WARN2("gc.mem", "Can't commit " << (void*)commit_size << " bytes");
        return false;
    }

    assert(committed_address == commit_base);
    LOG2("gc.mem", "committed " << (void*)commit_size << " bytes at " << commit_base);
    number_of_blocks_committed = target;
    return true;
}

void Block_Store::expand_heap(int expand_by)
{
    assert(_number_of_blocks_in_heap + expand_by <= max_blocks_in_heap);
    LOG2("gc", "expanding heap by " << mb(expand_by*GC_BLOCK_SIZE_BYTES) << " Mb");
    
    bool r = commit_blocks(_number_of_blocks_in_heap + expand_by);
    if (!r) {
        WARN("Can't increase the heap size by " 
                << (void*)((POINTER_SIZE_INT)(expand_by * GC_BLOCK_SIZE_BYTES)) 
                << " bytes");
        return;
    }

    void *block_address = (void*)((char*)_p_heap_base + _number_of_blocks_in_heap * GC_BLOCK_SIZE_BYTES);

    unsigned start_index = _number_of_blocks_in_heap;
    _number_of_blocks_in_heap += expand_by;
    _num_free_blocks += expand_by;
    TRACE2("gc.free", "_num_free_blocks = " << _num_free_blocks);

    for (unsigned int i = start_index; i < _number_of_blocks_in_heap; i++) {
        initialize_block(i, block_address);

        // move to next block
        block_address = (void *) ((POINTER_SIZE_INT)block_address + _block_size_bytes);
    }
    assert(block_address == (void *) ((POINTER_SIZE_INT) _p_heap_base + 
        _number_of_blocks_in_heap * GC_BLOCK_SIZE_BYTES));

    LOG2("gc", "java heap current size " 
            << mb(_number_of_blocks_in_heap * GC_BLOCK_SIZE_BYTES)
            << " Mb, maximum size " << mb(max_blocks_in_heap * GC_BLOCK_SIZE_BYTES) << " Mb");
}

void
Block_Store::set_free_status_for_all_sub_blocks_in_block(unsigned int block_number, bool is_free_val)
{
    unsigned int i = 0;
    while (i <  _blocks_in_block_store[block_number].number_of_blocks) {
        // this function changes the sense!!!
        assert(_blocks_in_block_store[block_number + i].block_is_free != is_free_val);
        _blocks_in_block_store[block_number + i].block_is_free = is_free_val;
        i++;
    }
}



bool
Block_Store::block_store_can_satisfy_request(unsigned int blocks_needed) {

    get_block_store_lock();
    unsigned int index = 0;
    
    while(index + blocks_needed <= _number_of_blocks_in_heap) {
        unsigned int number_of_blocks_found = 0;
        unsigned int j = index;
        while (number_of_blocks_found < blocks_needed) {
            assert(_blocks_in_block_store[j].block);
            if (_blocks_in_block_store[j].block_is_free == false) {
                break;
            }
            unsigned int num_blocks = _blocks_in_block_store[j].number_of_blocks;
            assert(num_blocks > 0);
            number_of_blocks_found += num_blocks;
            j += num_blocks;
        }
        if (number_of_blocks_found >= blocks_needed) {
            release_block_store_lock(); 
            return true;
        }
 
        index = j + _blocks_in_block_store[j].number_of_blocks;

    }
    
    release_block_store_lock();
    return false;    
}

//
// If size > GC_BLOCK_ALLOC_SIZE then on needs to use more than one block and the 
// single object block flag needs to be set in the block_store_info struct indexed in the
// _blocks_in_block_store index.
//

block_info *
Block_Store::p_get_multi_block(unsigned int size, bool for_chunks_pool) 
{
    // Control the number of blocks that go into block chunks pool if
    // I run below the watermark and that is what the request is for 
    // This way we can satisfy LOS and muti-block requests even if block 
    // store is dangerously low without doing a collection
    
    if (for_chunks_pool && (_num_free_blocks <= 0)) {
        return NULL;
    }
   
    unsigned int number_of_blocks_needed = 1;
    
    if (size > GC_BLOCK_ALLOC_SIZE) {
        number_of_blocks_needed = GC_NUM_BLOCKS_PER_LARGE_OBJECT(size);
        assert (number_of_blocks_needed > 1);
        assert ((GC_BLOCK_ALLOC_SIZE + GC_BLOCK_INFO_SIZE_BYTES) == GC_BLOCK_SIZE_BYTES);
    }
    
    get_block_store_lock();
    
    bool search_from_hint_failed = false;
    unsigned int index = 0;
    
retry_p_get_multi_block_label:
    if (search_from_hint_failed) {
        index = 0;
    } else {
        index = _free_block_search_hint;
    }
    
    while ((index + number_of_blocks_needed) <= _number_of_blocks_in_heap) {
        
        if (_blocks_in_block_store[index].block_is_free == true) {
            
            unsigned int number_of_blocks_found = _blocks_in_block_store[index].number_of_blocks;
            assert(number_of_blocks_found > 0);
            unsigned int j = index + _blocks_in_block_store[index].number_of_blocks;
            unsigned int k = index;
            
            while (number_of_blocks_found < number_of_blocks_needed) {
                if (_blocks_in_block_store[j].block_is_free == false) {
                    break;
                }
                unsigned int num_blocks = _blocks_in_block_store[j].number_of_blocks;
                assert(num_blocks > 0);
                number_of_blocks_found += num_blocks;
                k = j;  
                j += num_blocks;
            }
            
            if (number_of_blocks_found >= number_of_blocks_needed) {
                // Found the exact or greater number of blocks (than) needed
                
                unsigned int check_debug = _blocks_in_block_store[index].number_of_blocks;
                
                assert(_blocks_in_block_store[index].block);
                bool r = ensure_committed(index);
                if (!r) {
                    WARN("Cannot commit blocks");
                    return NULL;
                }
                
                // clear out the block_info before returning the block
                memset(_blocks_in_block_store[index].block, 0, GC_BLOCK_INFO_SIZE_BYTES);
                
                set_free_status_for_all_sub_blocks_in_block(index, false);

                unsigned int z = index + _blocks_in_block_store[index].number_of_blocks;
                while (z <= k) {
                    // This is a multi-block..... Zero out info for ancillary blocks (uptil index k)
                    assert(number_of_blocks_found > 1);
                    assert(_blocks_in_block_store[z].block_is_free == true);
                    
                    // make the entire super-block not-STD_FREE
                    set_free_status_for_all_sub_blocks_in_block(z, false);
                    assert(_blocks_in_block_store[z].block);
                    if (!ensure_committed(z)) {
                        WARN("Can't commit additional blocks");
                        return NULL;
                    }

                    // clear out the block info before collapsing this block into the bigger one
                    memset(_blocks_in_block_store[z].block, 0, GC_BLOCK_INFO_SIZE_BYTES);
                    
                    _blocks_in_block_store[z].block = NULL;
                    
                    check_debug += _blocks_in_block_store[z].number_of_blocks;
                    ;
                    // Block at "index" will keep track of the count of this
                    unsigned int save_z = z;
                    z += _blocks_in_block_store[z].number_of_blocks;
                     _blocks_in_block_store[save_z].number_of_blocks = 0;
                }
                
                // now block at "index" has either exactly equal or more blocks than needed
                assert(check_debug == number_of_blocks_found);
                
                if (number_of_blocks_found > number_of_blocks_needed) {
                    // chop off remaining and return exactly what is needed.
                    POINTER_SIZE_INT chopped_free_index = index + number_of_blocks_needed;
                    assert(_blocks_in_block_store[chopped_free_index].block == NULL);
                    
                    void *block_address = (void *) ((POINTER_SIZE_INT)_p_heap_base + (chopped_free_index << GC_BLOCK_SHIFT_COUNT));
                    POINTER_SIZE_INT UNUSED check_index = get_block_index((block_info*)block_address);
                    assert(check_index == chopped_free_index);
                    // Give the chopped portion a new address and set it FREE!!!
                    _blocks_in_block_store[chopped_free_index].block = (block_info *) block_address;
                    _blocks_in_block_store[chopped_free_index].number_of_blocks = number_of_blocks_found - number_of_blocks_needed;
                    set_free_status_for_all_sub_blocks_in_block((unsigned int)chopped_free_index, true);
                    _blocks_in_block_store[index].number_of_blocks = number_of_blocks_needed;
                    
                } else {
                    assert(number_of_blocks_found == number_of_blocks_needed);
                    _blocks_in_block_store[index].number_of_blocks = number_of_blocks_needed;
                }
                
                assert(_blocks_in_block_store[index].number_of_blocks == number_of_blocks_needed);      
                
                // points back to the block store info
                _blocks_in_block_store[index].block->block_store_info_index = index;
                // Decrement STD_FREE count
                _num_free_blocks -= number_of_blocks_needed;
                TRACE2("gc.free", "_num_free_blocks = " << (_num_free_blocks + number_of_blocks_needed)
                        << " -> " << _num_free_blocks);
                // Update hint for next search
                _free_block_search_hint = index;
                
                if (number_of_blocks_needed > 1) { 
                    // ***SOB LOOKUP*** We need to set the is_single_object_block to true....
                    unsigned int sob_index;
                    for (sob_index = 0; sob_index < number_of_blocks_needed; sob_index++) {
                        assert (_blocks_in_block_store[sob_index + index].is_single_object_block == false);
                        _blocks_in_block_store[sob_index + index].is_single_object_block = true;
                    }
                }            
                release_block_store_lock();
                gc_trace_block(_blocks_in_block_store[index].block, "p_get_multi_block returns this block");
                
                if (ensure_committed(index))
                    return _blocks_in_block_store[index].block;
                else
                    return NULL;
            } 
        }
        // Jump ahead of this STD_FREE chunk of small size or used chunk of any size
        index += _blocks_in_block_store[index].number_of_blocks;
        
    } // while 
    
    if (search_from_hint_failed == false) {
        // Try again...exactly once more....this time from index 0;
        search_from_hint_failed = true;
        goto retry_p_get_multi_block_label;
    }
    
    LOG2("gc", "p_get_multi_block() returns NULL, "
        "_num_free_blocks is " << _num_free_blocks 
        << ", requested " << number_of_blocks_needed);
    
    release_block_store_lock();
    
    return NULL;
}




block_info *
Block_Store::p_get_new_block(bool for_chunks_pool)
{
    TRACE("p_get_new_block calls p_get_multi_block");
    return p_get_multi_block (GC_BLOCK_ALLOC_SIZE, for_chunks_pool);
}



int
Block_Store::link_free_blocks (block_info *freed_block)
{
    gc_trace_block(freed_block, " in link_free_block");
    
    assert(freed_block < _p_heap_ceiling);
    assert(freed_block >= _p_heap_base);
    
    get_block_store_lock();
    
    unsigned int block_store_block_number = get_block_index(freed_block);
    
    // points back to the block store info
    assert(freed_block->block_store_info_index == block_store_block_number);

    unsigned int num_blocks = _blocks_in_block_store[block_store_block_number].number_of_blocks;
    
    assert(_blocks_in_block_store[block_store_block_number].block == freed_block);
    assert(_blocks_in_block_store[block_store_block_number].block_is_free == false);
    
    for (unsigned int i = block_store_block_number; i < (num_blocks + block_store_block_number) ; i++) {
        _blocks_in_block_store[i].total_live_objects_in_this_block = 0;
        _blocks_in_block_store[i].is_compaction_block = false; // Clear is compaction block field

        _blocks_in_block_store[i].block_for_sliding_compaction_allocation_pointer_computation = NULL;       
        _blocks_in_block_store[i].block_for_sliding_compaction_slot_updates = NULL;     
        _blocks_in_block_store[i].block_for_sliding_compaction_object_slides = NULL;        
        assert(_blocks_in_block_store[i].per_thread_slots_into_compaction_block);
        for (unsigned int cpu = 0; cpu < get_num_worker_threads(); cpu++) {
            assert(_blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu] == NULL);
            if (_blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu]) {
                delete _blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu];
                _blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu] = NULL;
            }
        }
        if (freed_block->block_free_areas)  {   // These get recreated when they go back to the chunk pool or whatever.
            STD_FREE(freed_block->block_free_areas);
        }
        freed_block->block_free_areas = NULL;
        
        // ***SOB LOOKUP*** Clear the is_single_object_block field as we release the blocks.
        _blocks_in_block_store[i].is_single_object_block = false;
    }
    
    // check integrity of sub-blocks if any
    if (num_blocks > 1) {
        for (unsigned int i = block_store_block_number + 1; i < (num_blocks + block_store_block_number) ; i++) {
            assert(_blocks_in_block_store[i].block == NULL);
            assert(_blocks_in_block_store[i].number_of_blocks == 0);
            assert(_blocks_in_block_store[i].block_is_free == false);
        }
    }
    
    // clear out the block info of the returned block
    memset(freed_block, 0, GC_BLOCK_INFO_SIZE_BYTES);
    
    // Make the entire block STD_FREE
    set_free_status_for_all_sub_blocks_in_block(block_store_block_number, true);
    
    // Increment STD_FREE count
    _num_free_blocks += num_blocks;
    TRACE2("gc.free", "_num_free_blocks = " << _num_free_blocks);
    
    // block has been returned...nothing more to do.
    
    release_block_store_lock();
    return num_blocks;
}


void Block_Store::coalesce_free_blocks()
{
    get_block_store_lock();
    
    unsigned int number_of_blocks_coalesced = 0;
    
    // Dont expect any multiblocks for now....
    unsigned int index = 0;
    
    while (index < _number_of_blocks_in_heap) {
        
        if (_blocks_in_block_store[index].block_is_free == true) {
            
            // Points to valid block
            assert(_blocks_in_block_store[index].block);
            
            unsigned int j = index + _blocks_in_block_store[index].number_of_blocks;
            
            while ((j < _number_of_blocks_in_heap) && (_blocks_in_block_store[j].block_is_free == true)) {
                
                assert(_blocks_in_block_store[j].block);                    
                bool r = ensure_committed(j);
                if (!r) {
                    WARN("Can't commit blocks");
                    return;
                }
                // clear out the block info
                memset(_blocks_in_block_store[j].block, 0, GC_BLOCK_INFO_SIZE_BYTES);
                // Coalesced and gone
                _blocks_in_block_store[j].block = NULL;
                // Combine with number of blocks with one at index
                _blocks_in_block_store[index].number_of_blocks += _blocks_in_block_store[j].number_of_blocks;
                number_of_blocks_coalesced += _blocks_in_block_store[j].number_of_blocks;
                // Skip ahead by the size of this region
                unsigned int save_size_at_j = _blocks_in_block_store[j].number_of_blocks;
                // This block loses its identity and count now
                _blocks_in_block_store[j].number_of_blocks = 0;
                j += save_size_at_j;
            }
        }
        // check if _free_block_search_hint was subsumed by this coalesce
        if ((_free_block_search_hint > index) && (_free_block_search_hint < (index + _blocks_in_block_store[index].number_of_blocks))) {
            _free_block_search_hint = index;    // Adjust to first block
        }
        
        // Jump ahead of the coalesced region and start looking after that ends
        index += _blocks_in_block_store[index].number_of_blocks;
    }

    release_block_store_lock();
    INFO2("gc.blocks", "coalesced " << number_of_blocks_coalesced << " blocks");
}



void 
Block_Store::_get_system_info()
{
    _machine_page_size_bytes = port_vmem_page_sizes()[0];
    _number_of_active_processors = port_CPUs_number();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// S L I D I N G   C O M P A C T I O N //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
Block_Store::init_live_objects_in_heap_iterator() {
    live_object_iterator_current_block = NULL;
    init_block_iterator();
    assert(live_object_iterator_mark_bits_valid);
}

Partial_Reveal_Object*
Block_Store::get_next_live_object() {
    assert(live_object_iterator_mark_bits_valid);

    // searching live object in current block (live_object_iterator_current_block)
    while(true) {

        // if no current block, lets get one
        while(!live_object_iterator_current_block) {

            block_store_info *info = get_next_block();

            if (!info) return NULL;

            // we need the block which is not free: let's go to next block
            if (info->block_is_free) continue;

            if (info->block->is_single_object_block) {
                // well, the block is single object block: is it contains live
                // object
                Partial_Reveal_Object *obj = (Partial_Reveal_Object *)GC_BLOCK_ALLOC_START(info->block);

                if (is_object_marked(obj)) {
                    // live object in single object block detected
                    return obj;
                }

                // no live object in single object block: lets try next block
                continue;
            }

            // block with small objects found, setting up iterator
            live_object_iterator_current_block = info->block;
            init_live_object_iterator_for_block(info->block);
        }

        // block is not NULL here and it is nursery
        assert(live_object_iterator_current_block->in_nursery_p
                || live_object_iterator_current_block->in_los_p);

        // let's get next object in the block
        Partial_Reveal_Object *obj = get_next_live_object_in_block(live_object_iterator_current_block);
        if (obj) return obj;

        // no more objects in the block, goto next block
        live_object_iterator_current_block = NULL;
    }
}

inline void
Block_Store::get_compaction_limits_based_on_compaction_type_for_this_gc(unsigned int *low, unsigned int *high)
{
    if (_compaction_type_for_this_gc == gc_full_heap_sliding_compaction) {
        *low = 0;
        *high = _number_of_blocks_in_heap;
        
        // A force gc may cause this while the other gcs are all gc_incremental_sliding_compaction gcs.
        // If this is so we want to treat this as an incremental compaction of the first part of the heap. 
        roll_forward_compaction_limits();
    } else if (_compaction_type_for_this_gc == gc_incremental_sliding_compaction) {
        *low = _compaction_blocks_low_index;
        *high = _compaction_blocks_high_index;      
    } else {
        // Only these two types are supported for now
        ABORT("Unsupported compaction type");
    }
}

/*
 * pinned_blocks = set of blocks that cannot be compacted because some pinned root is pointing into that block.
 */
void
Block_Store::determine_compaction_areas_for_this_gc(std::set<block_info *> &pinned_blocks)
{
    unsigned int low_index = 0;
    unsigned int high_index = 0;
    
    if (_compaction_type_for_this_gc == gc_no_compaction) {
        // nothing
        return;
    }
    
    get_compaction_limits_based_on_compaction_type_for_this_gc(&low_index, &high_index);
    INFO2("gc.compact", "Compaction limits for this GC (#" << low_index << " - #" << high_index-1 << ")");
    assert((low_index < high_index) && (high_index <= _number_of_blocks_in_heap));
    
    int skipped_blocks = 0;
    for (unsigned int i = low_index; i < high_index; i++) {
        
        // Set for compaction all single blocks
        if ( (_blocks_in_block_store[i].number_of_blocks == 1) &&   // Only simple blocks
             (_blocks_in_block_store[i].block_is_free == false) &&  // Only used blocks
             (_blocks_in_block_store[i].block->in_los_p == false) &&    // If they are LOS, they will not be compacted
             (_blocks_in_block_store[i].block->is_single_object_block == false) &&  // If they are single object block, they will not be compacted
             (pinned_blocks.find(_blocks_in_block_store[i].block) == pinned_blocks.end()) && // If the block is not in the list of pinned blocks
             (_blocks_in_block_store[i].block->pin_count == 0)          // Only blocks without pinned objects
            ) {
            
            assert(_blocks_in_block_store[i].block->in_nursery_p);
            assert(_blocks_in_block_store[i].is_compaction_block == false);
            
            _blocks_in_block_store[i].is_compaction_block = true;
            
            assert(_blocks_in_block_store[i].block);
            _blocks_in_block_store[i].block->is_compaction_block = true;
            gc_trace_block(_blocks_in_block_store[i].block, "This block in compaction area.");
            
#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
            
#else
            for (unsigned int j = 0; j < get_num_worker_threads(); j++) {
                _blocks_in_block_store[i].per_thread_slots_into_compaction_block[j] = 0;
                _blocks_in_block_store[i].per_thread_live_objects_in_compaction_block[j] = 0;
                _blocks_in_block_store[i].num_per_thread_live_objects_in_compaction_block[j] = 0;
            }
            // the list of sorted objects is freed by thread that slides objects finally...so this assert will not hold
            _blocks_in_block_store[i].all_live_objects_in_block_fully_sorted = NULL;
#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
            
            _blocks_in_block_store[i].total_live_objects_in_this_block = 0;
                _blocks_in_block_store[i].block->from_block_has_been_evacuated = false;
            _blocks_in_block_store[i].block->is_to_block = false; 
                gc_trace_block (  _blocks_in_block_store[i].block, "from_block_has_been_evacuated and is_to_block set to false.");
            _blocks_in_block_store[i].block_for_sliding_compaction_allocation_pointer_computation = 
                _blocks_in_block_store[i].block;
            _blocks_in_block_store[i].block_for_sliding_compaction_slot_updates = 
                _blocks_in_block_store[i].block;
            _blocks_in_block_store[i].block_for_sliding_compaction_object_slides = 
                _blocks_in_block_store[i].block;
        } else {
            LOG2("gc.compact", "excluding block " << (void*)_blocks_in_block_store[i].block 
                    << " from compaction because it " <<
                    (_blocks_in_block_store[i].number_of_blocks != 1 ? "is multiblock" :
                     (_blocks_in_block_store[i].block_is_free ? "is free" :
                      (_blocks_in_block_store[i].block->in_los_p ? "is in los" :
                       (_blocks_in_block_store[i].block->is_single_object_block ? "is single object block" :
                        ((pinned_blocks.find(_blocks_in_block_store[i].block) != pinned_blocks.end()) ? "has pinned roots" :
                         (_blocks_in_block_store[i].block->pin_count > 0 ? "has pinned objects" : "can't be")))))));
            skipped_blocks++;
        }
    }

    INFO2("gc.compact", skipped_blocks << " blocks will not be compacted this time");
    
    if (cross_block_compaction) {
        // Setup regions for each GC thread to operate on
        unsigned int min_compact_blocks_per_gc_thread = (high_index - low_index) / get_num_worker_threads();
        unsigned int extra_tail_for_last_gc_thread = (high_index - low_index) % get_num_worker_threads();
        unsigned int gc_thread = 0;
        unsigned int low_index_curr = low_index;
        unsigned int high_index_curr = low_index + min_compact_blocks_per_gc_thread;
        while (gc_thread < get_num_worker_threads()) {
            _per_gc_thread_compaction_blocks_low_index[gc_thread] =  low_index_curr;    
            _per_gc_thread_compaction_blocks_high_index[gc_thread] = high_index_curr;
            low_index_curr = high_index_curr;
            high_index_curr += min_compact_blocks_per_gc_thread;
            gc_thread++;
        }
        _per_gc_thread_compaction_blocks_high_index[get_num_worker_threads() - 1] += extra_tail_for_last_gc_thread;
        assert(_per_gc_thread_compaction_blocks_low_index[0] == low_index);
        assert(_per_gc_thread_compaction_blocks_high_index[get_num_worker_threads() - 1] == high_index);
    } // if
}

extern int incremental_compaction;

void Block_Store::roll_forward_compaction_limits()
{
    if (incremental_compaction == 2) {
        unsigned &last = previous_compaction_last_destination_block;

        assert(last <= _number_of_blocks_in_heap);
        _compaction_blocks_low_index = 0;
        _compaction_blocks_high_index = _number_of_blocks_in_heap;

        if (last < _number_of_blocks_in_heap / 2) {
            _compaction_blocks_low_index = last;
        }
        last = 0;
        return;
    }
    unsigned number_of_blocks_in_increment = get_compaction_increment_size();
    assert(number_of_blocks_in_increment > 0);
    assert(number_of_blocks_in_increment <= _number_of_blocks_in_heap);
    // Roll forward compaction area range for next GC cycle
    _compaction_blocks_low_index = _compaction_blocks_low_index + number_of_blocks_in_increment;
    _compaction_blocks_high_index = _compaction_blocks_low_index + number_of_blocks_in_increment;
    assert(_compaction_blocks_low_index < _compaction_blocks_high_index);

    if (_compaction_blocks_low_index >= _number_of_blocks_in_heap) {
        // need to rotate back to beginning of heap since we just finished with last chunk of heap
        _compaction_blocks_low_index = 0;
        _compaction_blocks_high_index = number_of_blocks_in_increment;
    } else if (_compaction_blocks_high_index >= _number_of_blocks_in_heap) {
        // final section of heap incremental compaction. compact till the very end of the heap.
        _compaction_blocks_high_index = _number_of_blocks_in_heap;
    } 
    assert(_compaction_blocks_low_index <= _number_of_blocks_in_heap);
    assert(_compaction_blocks_high_index <= _number_of_blocks_in_heap);
    assert(_compaction_blocks_low_index < _compaction_blocks_high_index);
}

void
Block_Store::reset_compaction_areas_after_this_gc() 
{
    unsigned int low_index = 0;
    unsigned int high_index = 0;
    
    if (_compaction_type_for_this_gc == gc_no_compaction) {
        // nothing
        return;
    }
    
    get_compaction_limits_based_on_compaction_type_for_this_gc(&low_index, &high_index);
    for (unsigned int i = low_index; i < high_index; i++) {
        // Set for compaction all single blocks
        if (    (_blocks_in_block_store[i].number_of_blocks == 1) && 
            (_blocks_in_block_store[i].block_is_free == false) &&
            (_blocks_in_block_store[i].block->in_los_p == false)&&
            (_blocks_in_block_store[i].block->is_single_object_block == false) 
            && (is_compaction_block(_blocks_in_block_store[i].block))
            ) {
            
            gc_trace_block(_blocks_in_block_store[i].block, "This block has compaction area reset.");

            assert(_blocks_in_block_store[i].block->in_nursery_p);
            assert(_blocks_in_block_store[i].is_compaction_block == true);
            // From block had better have been evacuated by now.
           
            assert(_blocks_in_block_store[i].block->from_block_has_been_evacuated); // This block has not been evacuated yet.
            

            _blocks_in_block_store[i].is_compaction_block = false;
            _blocks_in_block_store[i].block->is_compaction_block = false;

            assert(_blocks_in_block_store[i].per_thread_slots_into_compaction_block);
            for (unsigned int cpu = 0; cpu < get_num_worker_threads(); cpu++) {
                assert(_blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu] == NULL);
                if (_blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu]) {
                    delete _blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu];
                    _blocks_in_block_store[i].per_thread_slots_into_compaction_block[cpu] = NULL;
                }
            }

#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
            _blocks_in_block_store[i].bit_index_into_all_lives_in_block = 0;
#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
            _blocks_in_block_store[i].total_live_objects_in_this_block = 0;
            assert(_blocks_in_block_store[i].block);

            if (!cross_block_compaction) {
                assert(!_blocks_in_block_store[i].block_for_sliding_compaction_allocation_pointer_computation); 
                assert(!_blocks_in_block_store[i].block_for_sliding_compaction_slot_updates); 
                assert(!_blocks_in_block_store[i].block_for_sliding_compaction_object_slides); 
            }
            // compacted blocks get swept automatically
            assert(_blocks_in_block_store[i].block->block_has_been_swept);
        }
    }
    
    if (_compaction_type_for_this_gc != gc_full_heap_sliding_compaction) {
        roll_forward_compaction_limits();
    }
    // Just clear the compaction type...this needs to be re-initialized at the beginning of next GC
    _compaction_type_for_this_gc = gc_bogus_compaction;
    LOG2("gc.compact", "Next compaction limits are #" << _compaction_blocks_low_index
            << " - #" << _compaction_blocks_high_index);
    LOG2("gc.compact", "compact increment size = " << get_compaction_increment_size());
}


// Use by all threads as hint
static volatile unsigned int alloc_ptr_block_index_hint = 0;



block_info *
Block_Store::get_block_for_sliding_compaction_allocation_pointer_computation(unsigned int thread_id, block_info *curr_block)
{
    if (cross_block_compaction) {
        return iter_get_next_compaction_block_for_gc_thread(thread_id, curr_block);
    }
    
    unsigned int low_index = 0;
    unsigned int high_index = 0;
    
    get_compaction_limits_based_on_compaction_type_for_this_gc(&low_index, &high_index);
    
    bool search_from_hint_failed = false;
    unsigned int index = 0;
    
retry_get_alloc_ptr_block_label:
    
    if (!search_from_hint_failed) {
        index = (alloc_ptr_block_index_hint > low_index) ? alloc_ptr_block_index_hint : low_index;
    } else {
        index = low_index;
    }
    
    for (unsigned int i = index; i < high_index; i++) {
        if (_blocks_in_block_store[i].is_compaction_block == true) {
            if (_blocks_in_block_store[i].block_for_sliding_compaction_allocation_pointer_computation) {
                block_info *block = _blocks_in_block_store[i].block_for_sliding_compaction_allocation_pointer_computation;
                if (block) {
                    void * ret = apr_atomic_casptr( 
                        (volatile void **)&(_blocks_in_block_store[i].block_for_sliding_compaction_allocation_pointer_computation),
                        (void *)NULL,
                        (void *)block
                        );
                    if (ret) {
                        // got it.
                        assert(ret == block);
                        alloc_ptr_block_index_hint = i;
                        return (block_info *) ret;
                    } 
                } 
            } 
        } 
    } // for
    if (search_from_hint_failed == false) {
        // Try again...exactly once more....this time from index 0;
        search_from_hint_failed = true;
        goto retry_get_alloc_ptr_block_label;
    }
    return NULL;
}



// Use by all threads as hint
static volatile unsigned int fix_slots_block_index_hint = 0;


block_info *
Block_Store::get_block_for_fix_slots_to_compaction_live_objects(unsigned int thread_id, block_info *curr_block)
{
    if (cross_block_compaction) {
        return iter_get_next_compaction_block_for_gc_thread(thread_id, curr_block);
    }
    
    unsigned int low_index = 0;
    unsigned int high_index = 0;
    
    get_compaction_limits_based_on_compaction_type_for_this_gc(&low_index, &high_index);
    
    bool search_from_hint_failed = false;
    unsigned int index = 0;
    
retry_get_fix_slots_block_label:
    
    if (!search_from_hint_failed) {
        index = (fix_slots_block_index_hint > low_index) ? fix_slots_block_index_hint : low_index;
    } else {
        index = low_index;
    }
    
    for (unsigned int i = index; i < high_index; i++) {
        if (_blocks_in_block_store[i].is_compaction_block == true) {
            
            if (_blocks_in_block_store[i].block_for_sliding_compaction_slot_updates) {
                block_info *block = _blocks_in_block_store[i].block_for_sliding_compaction_slot_updates;
                
                if (block) {
                    void * ret = apr_atomic_casptr( 
                        (volatile void **)&(_blocks_in_block_store[i].block_for_sliding_compaction_slot_updates),
                        (void *)NULL,
                        (void *)block
                        );
                    if (ret) {
                        // got it.
                        assert(ret == block);
                        fix_slots_block_index_hint = i;
                        return (block_info *) ret;
                    }
                }
            }
        }
    } // for
    if (search_from_hint_failed == false) {
        // Try again...exactly once more....this time from index 0;
        search_from_hint_failed = true;
        goto retry_get_fix_slots_block_label;
    }
    return NULL;
}


// Use by all threads as hint
static volatile unsigned int slide_block_index_hint = 0;



block_info *
Block_Store::get_block_for_slide_compact_live_objects(unsigned int thread_id, block_info *curr_block)
{
    if (cross_block_compaction) {
        return iter_get_next_compaction_block_for_gc_thread(thread_id, curr_block);
    }
    
    unsigned int low_index = 0;
    unsigned int high_index = 0;
    
    get_compaction_limits_based_on_compaction_type_for_this_gc(&low_index, &high_index);
    
    bool search_from_hint_failed = false;
    unsigned int index = 0;
    
retry_get_slide_block_label:
    
    if (!search_from_hint_failed) {
        index = (slide_block_index_hint > low_index) ? slide_block_index_hint : low_index;
    } else {
        index = low_index;
    }
    
    for (unsigned int i = index; i < high_index; i++) {
        if (_blocks_in_block_store[i].is_compaction_block == true) {
            
            if (_blocks_in_block_store[i].block_for_sliding_compaction_object_slides) {
                block_info *block = _blocks_in_block_store[i].block_for_sliding_compaction_object_slides;
                
                if (block) {
                    void * ret = apr_atomic_casptr( 
                        (volatile void **)&(_blocks_in_block_store[i].block_for_sliding_compaction_object_slides),
                        (void *)NULL,
                        (void *)block
                        );
                    if (ret) {
                        // got it.
                        assert(ret == block);
                        slide_block_index_hint = i;
                        return (block_info *) ret;
                    }
                }
            }
        }
    } // for
    if (search_from_hint_failed == false) {
        // Try again...exactly once more....this time from index 0;
        search_from_hint_failed = true;
        goto retry_get_slide_block_label;
    }
    return NULL;
}


// Iterator to get next block for compaction for current GC thread. This is idempotent and returns the
// block following cur_block.

block_info *
Block_Store::iter_get_next_compaction_block_for_gc_thread(unsigned int gc_thread_id, block_info *curr_block)
{
    assert(gc_thread_id < get_num_worker_threads());
    // compute range for thread and find next compaction block and return
    unsigned int low_index = _per_gc_thread_compaction_blocks_low_index[gc_thread_id];
    unsigned int high_index = _per_gc_thread_compaction_blocks_high_index[gc_thread_id];
    // default is first block in thread's range
    unsigned int index = low_index;
    if (curr_block) {
        assert(curr_block->block_store_info_index >= low_index);
        assert(curr_block->block_store_info_index < high_index);
        // integrity of block_store_info_index
        assert(curr_block->block_store_info_index == get_block_index(curr_block));
        assert(_blocks_in_block_store[curr_block->block_store_info_index].is_compaction_block);
        index = curr_block->block_store_info_index + 1;
    } 
    while (index < high_index) {
        if (_blocks_in_block_store[index].is_compaction_block) {
            assert(_blocks_in_block_store[index].block->is_compaction_block && _blocks_in_block_store[index].block->in_nursery_p);
            assert(_blocks_in_block_store[index].number_of_blocks == 1);
            return _blocks_in_block_store[index].block;
        }
        index++;
    }
    assert(index == high_index);
    // Ran through last block for this thread
    return NULL;
}



Remembered_Set *
Block_Store::get_slots_into_compaction_block(block_info *block)
{
    assert(block->block_store_info_index == get_block_index(block));
    
    unsigned int block_number = block->block_store_info_index;
    
    for (unsigned int i = 0; i < get_num_worker_threads(); i++) {
        Remembered_Set *thr_slots = _blocks_in_block_store[block_number].per_thread_slots_into_compaction_block[i]; 
        if (thr_slots) {
            _blocks_in_block_store[block_number].per_thread_slots_into_compaction_block[i] = NULL;
            // The remembered set will be freed by the GC thread that works on these slots.
            return thr_slots;
        }
    }
    return NULL;
}




#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

void 
Block_Store::init_live_object_iterator_for_block(block_info *block) {
    unsigned int block_store_block_number = get_block_index(block);
    _blocks_in_block_store[block_store_block_number].bit_index_into_all_lives_in_block = 0;
}

#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK



#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

Partial_Reveal_Object *
Block_Store::get_next_live_object_in_block(block_info *block) {
    
    set_bit_search_info info;
    
    uint8 *mark_vector_base = &(block->mark_bit_vector[0]);
    
    // stop searching when we get close the the end of the vector
    info.p_ceil_byte = (uint8 *) ((POINTER_SIZE_INT)mark_vector_base + GC_SIZEOF_MARK_BIT_VECTOR_IN_BYTES);
    
    assert(get_block_index(block) == block->block_store_info_index);
    
    unsigned int block_store_block_number = block->block_store_info_index;
    
    unsigned int bit_index = _blocks_in_block_store[block_store_block_number].bit_index_into_all_lives_in_block;
    
    info.p_start_byte = (uint8 *)((POINTER_SIZE_INT) mark_vector_base + (bit_index / GC_NUM_BITS_PER_BYTE));
    info.start_bit_index = (bit_index % GC_NUM_BITS_PER_BYTE);  // convert this to a subtract later...XXX
    
    get_next_set_bit(&info);
    
    if (verify_gc) {
        verify_get_next_set_bit_code(&info);
    }  
    
    if (info.p_non_zero_byte == NULL) {
        _blocks_in_block_store[block_store_block_number].bit_index_into_all_lives_in_block = 0;
        return NULL;
    }
    
    // Compute the address of the live object
    unsigned int obj_bit_index = (unsigned int) ((info.p_non_zero_byte - mark_vector_base) * GC_NUM_BITS_PER_BYTE + info.bit_set_index);
    Partial_Reveal_Object *p_live_obj = (Partial_Reveal_Object *) ((POINTER_SIZE_INT)block + GC_BLOCK_INFO_SIZE_BYTES +  (obj_bit_index * GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES));
    
    // Next time we need to start searching from the next bit onwards
    _blocks_in_block_store[block_store_block_number].bit_index_into_all_lives_in_block = obj_bit_index + 1;
    
    return p_live_obj;
}

#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK




void 
Block_Store::dump_heap(int file_num) {
    char bufname[100];
    sprintf(bufname, "heapdump.%d", file_num);
    apr_pool_t *pool;
    apr_pool_create(&pool, 0);
    apr_file_t* fh;
    if (APR_SUCCESS != apr_file_open(&fh, bufname, 
        APR_FOPEN_BINARY | APR_FOPEN_WRITE | APR_FOPEN_CREATE, 
        APR_FPROT_UWRITE, pool)) {

        WARN("OPEN FAILED");
        apr_pool_destroy(pool);
        return;
    }
    
    uint8 addr[8];
    *((void **)addr) = _p_heap_base;
    size_t nbytes = 8;
    apr_file_write(fh, addr, &nbytes);
    
    uint8 integer[4];
    uint32 *p = (uint32 *) _p_heap_base;
    
    while (p != _p_heap_ceiling) {
        *((uint32 *)integer) = (*((uint32 *)p));
        apr_file_write(fh, integer, &nbytes);
        p++;
    }
    apr_file_close(fh);
    apr_pool_destroy(pool);
}

unsigned Block_Store::get_num_empty_smos_blocks_in_block_store() {
    unsigned c = 0;
    init_block_iterator();
    for (block_store_info* info = get_next_block(); info != NULL; info = get_next_block()) {
        // skip free blocks
        if (info->block_is_free) continue; 
        // skip multiblocks
        if (info->number_of_blocks > 1) continue;

        // NB empty SmOS blocks can only appear as a result of compaction,
        // because sweep is delayed until allocation time
        // so with '-Xgc fixed' this function will report 0 most of the time

        // count completely empty SmOS blocks. 
        if (info->block->block_has_been_swept && 1 == info->block->num_free_areas_in_block
                && GC_BLOCK_ALLOC_SIZE == info->block->block_free_areas[0].area_size) {
            c++;
        }
    }

    return c;
}
