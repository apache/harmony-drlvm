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
/*
 * @author Salikh Zakirov
 * @version $Revision: 1.1.2.1.4.3 $
 */
 
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

#include "characterize_heap.h"
#include "mark.h"

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.verbose"

inline int64 mb(int64 size) {
    return (size + 1048576/2 - 1) / 1048576;
}

#undef STATS
#define STATS(message) INFO2("gc.stats.chunk", message)

void Garbage_Collector::dump_chunk_statistics()
{
    if (!is_info_enabled("gc.stats.chunk")) return;
    int chunks = 0;
    int free_chunks = 0;
    int blocks = 0;
    unsigned i;
    for (i = 0; i < _gc_chunks.size(); i++) {
        if (NULL != _gc_chunks[i].chunk) {
            chunks++;
            if (NULL != _gc_chunks[i].free_chunk) free_chunks++;
            block_info* block = _gc_chunks[i].chunk;
            while (NULL != block) {
                blocks++;
                block = block->next_free_block;
            }
        }
    }
    STATS("=== chunk statistics ===");
    STATS("chunks = " << chunks << ", free = " << free_chunks);
    if (chunks > 0) {
        STATS("chunk blocks = " << blocks << ", average size = " << (blocks / chunks));
    }
    STATS("");
}

#undef STATS
#define STATS(message) INFO2("gc.stats.blocks", message)

void Block_Store::characterize_blocks_in_heap ()
{
    if (!is_info_enabled("gc.stats.blocks")) return;

    unsigned info_total_blocks = 0;
    int free_blocks = 0;
    int used_blocks = 0;
    int single_blocks = 0;
    int multi_block_chains = 0;
    int free_multi_block_chains = 0;
    int blocks_in_multi_blocks = 0;
    int free_blocks_in_multi_blocks = 0;
    
    int info_nurseries = 0;
    int info_unused_nurseries = 0;
    int info_full_nurseries = 0;
    int info_unswept_nurseries = 0;
    int info_used_nurseries = 0;
    int info_empty_nurseries = 0;
    int info_nonempty_nurseries = 0;

    block_store_info* info;
    init_block_iterator();

    for (info = get_next_block(); NULL != info; info = get_next_block()) {

        info_total_blocks += info->number_of_blocks;

        if (info->block_is_free) {

            free_blocks += info->number_of_blocks;

            if (info->number_of_blocks != 1) {
                free_multi_block_chains++;
                free_blocks_in_multi_blocks += info->number_of_blocks;
            }

        } else {

            used_blocks += info->number_of_blocks;

            if (info->number_of_blocks != 1) {
                multi_block_chains++;
                blocks_in_multi_blocks += info->number_of_blocks;
            } else {
                single_blocks++;
                block_info *block = info->block;
        
                if (block->in_nursery_p) {
                    info_nurseries++;
                    if (block->block_has_been_swept) {
                        if (block->current_alloc_area == -1) {
                            if (block->num_free_areas_in_block > 0) {
                                // free areas were not used yet
                                info_unused_nurseries++;
                                if (1 == block->num_free_areas_in_block
                                        && block->block_free_areas[0].area_base == GC_BLOCK_ALLOC_START(block)
                                        && block->block_free_areas[0].area_size == GC_BLOCK_ALLOC_SIZE) {
                                    info_empty_nurseries++;
                                } else {
                                    info_nonempty_nurseries++;
                                }
                            } else {
                                // there were no free areas in the block,
                                // i.e. the block is filled up
                                info_full_nurseries++;
                            }
                        } else {
                            info_used_nurseries++;
                        }
                    } else {
                        info_unused_nurseries++;
                        info_unswept_nurseries++;
                    }
                }
            }
        }
    }

    STATS("=== block statistics ===");
    if (info_total_blocks != _number_of_blocks_in_heap) {
        WARN("Incorrect number of blocks " << info_total_blocks << ", but total = " << _number_of_blocks_in_heap);
    }
    STATS("Total blocks = " << _number_of_blocks_in_heap 
            << " (" << mb(_number_of_blocks_in_heap * GC_BLOCK_SIZE_BYTES) << " Mb)");
    STATS("used blocks = " << used_blocks << " (" << mb(used_blocks * GC_BLOCK_SIZE_BYTES) << " Mb)");
    STATS(">used single blocks = " << single_blocks << " (" << mb(single_blocks * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">used multi blocks = " << blocks_in_multi_blocks << " in " << multi_block_chains << " chains");
    if (multi_block_chains > 0) {
        STATS(">>average used multi block size = " << (blocks_in_multi_blocks / multi_block_chains));
    }
    STATS("");
    STATS("free blocks = " << free_blocks << " (" << mb(free_blocks * GC_BLOCK_SIZE_BYTES) << " Mb)");
    STATS(">free multi blocks = " << free_blocks_in_multi_blocks << " in " << free_multi_block_chains << " chains");
    if (free_multi_block_chains > 0) {
        STATS(">>average free multi block size = " << (free_blocks_in_multi_blocks / free_multi_block_chains));
    }
    STATS("");
    STATS("nurseries = " << info_nurseries << " (" << mb(info_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">full nurseries = " << info_full_nurseries 
            << " (" << mb(info_full_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">used nurseries = " << info_used_nurseries 
            << " (" << mb(info_used_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">unused nurseries = " << info_unused_nurseries 
            << " (" << mb(info_unused_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">>unswept nurseries = " << info_unswept_nurseries 
            << " (" << mb(info_unswept_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">>empty nurseries = " << info_empty_nurseries 
            << " (" << mb (info_empty_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS(">>non-empty nurseries = " << info_nonempty_nurseries 
            << " (" << mb (info_nonempty_nurseries * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    if (info_unused_nurseries != info_empty_nurseries 
            + info_nonempty_nurseries + info_unswept_nurseries) {
        WARN("Incorrect statistics on unused nurseries");
    }
    if (info_nurseries != info_full_nurseries 
            + info_used_nurseries + info_unused_nurseries) {
        WARN("Incorrect statistics on nurseries");
    }
    STATS("");
}

#undef STATS
#define STATS(message) INFO2("gc.stats.objects", message)

static int free_in_block(block_info *block)
{
    int free = 0;
    int j;
    for (j = block->current_alloc_area + 1; j < block->num_free_areas_in_block; j++) {
        free_area& area = block->block_free_areas[j];
        // ceiling is the last free byte, thus + 1 is needed to count it correctly
        free += (char*)area.area_ceiling - (char*)area.area_base + 1;
    }
    return free;
}

static int free_in_chunk(block_info* chunk)
{
    int free = 0;
    block_info* block = chunk;
    while (NULL != block) {
        free += free_in_block(block);
        block = block->next_free_block;
    }
    return free;
}

///////////////////////////////////////////////////////////////////////////
//
// Object iteration in block.
//
// This code is brittle! (2005-10-08 -salikh)
//
// Makes several assumptions about objects:
//  * objects are located contiguously, without free spaces between
//  * all objects have correct VTable pointer
//  * the space after the last object is zeroed out (next object vtable pointer is zero)
//
static void* count_objects_in_area(void* start, void* limit, int& object_count, int& used)
{
    object_count = 0;
    used = 0;
    Partial_Reveal_Object* object = (Partial_Reveal_Object*)start;
    while (object < limit && NULL != object->vtraw()) {
        object_count++;
        int size = get_object_size_bytes(object);
        used += size;
        object = (Partial_Reveal_Object*)((char*)object + size);
    } 
    return object;
}

static void count_objects_in_block(block_info *block, int& object_count, int& used, int& lost, int& nonfree)
{
    used = 0;
    object_count = 0;
    lost = 0;
    nonfree = 0;
    void* base = (Partial_Reveal_Object*)GC_BLOCK_ALLOC_START(block);
    void* last_object; // the byte just after the last object
    void* limit;
    int ai = 0;
    int objects_in_area = 0;
    int used_in_area = 0;
    for (ai = 0; ai < block->num_free_areas_in_block; ai++) {

        // count objects before allocation area
        limit = block->block_free_areas[ai].area_base;
        last_object = count_objects_in_area(base, limit, objects_in_area, used_in_area);
        object_count += objects_in_area;
        used += used_in_area;
        lost += (char*)limit - (char*)last_object;

        // count objects in allocation area (it may have been used already)
        limit = block->block_free_areas[ai].area_ceiling;
        last_object = count_objects_in_area(block->block_free_areas[ai].area_base, limit, objects_in_area, used_in_area);
        object_count += objects_in_area;
        used += used_in_area;
        nonfree += (char*)last_object - (char*)block->block_free_areas[ai].area_base;

        base = (Partial_Reveal_Object*)((char*)block->block_free_areas[ai].area_ceiling + 1);
    }

    // count objects after last allocation area
    limit = (void*)((char*)(GC_BLOCK_ALLOC_START(block)) + GC_BLOCK_ALLOC_SIZE);
    last_object = count_objects_in_area(base, limit, objects_in_area, used_in_area);
    object_count += objects_in_area;
    used += used_in_area;
    lost = (char*)limit - (char*)last_object;
}

void Garbage_Collector::dump_object_statistics() 
{
    if (!is_info_enabled("gc.stats.objects")) return;

    int single_object_used_blocks = 0;
    int64 single_objects = 0;
    int64 los_object_bytes = 0;
    int64 single_object_used = 0;

    
    block_store_info* info;
    _p_block_store->init_block_iterator();

    for (info = _p_block_store->get_next_block(); NULL != info; info = _p_block_store->get_next_block()) {
        if (info->block_is_free) continue;
        block_info *block = info->block;
        if (!block->is_single_object_block) continue;

        single_objects++;
        los_object_bytes += block->los_object_size;
        single_object_used_blocks += info->number_of_blocks;
        single_object_used += info->number_of_blocks * GC_BLOCK_SIZE_BYTES;
    }

    STATS("=== object statistics ===");
    STATS("Single objects = " << single_objects);
    STATS("Single object bytes = " << los_object_bytes
            << " (" << mb(los_object_bytes) << " Mb)");
    STATS("Single objects used = " << single_object_used << " (" << mb(single_object_used) << " Mb)");
    STATS("");

    int64 smos_free = 0;
    int64 smos_used = 0;
    int64 smos_lost = 0;
    int64 smos_objects = 0;
    int max_objects_in_block = 0;
    int max_used_in_block = 0;
    int min_objects_in_block = 100000;  // greater than possible maximum = 60k/4 = 15360
    int min_used_in_block = 65536;      // greater than possible maximum = 60k = 61440
    int avg_used_in_block = 0;
    int avg_objects_in_block = 0;
    int avg_blocks_count = 0;
    int smos_empty_blocks = 0;
    int smos_blocks = 0;
    unsigned i;
    for (i = 0; i < _gc_chunks.size(); i++) {
        block_info *block = _gc_chunks[i].chunk;
        while (NULL != block) {
            smos_blocks++;

            int free_bytes = free_in_block(block);
            smos_free += free_bytes;

            int used_in_block, objects_in_block, lost_in_block, nonfree_in_block;
            count_objects_in_block(block, objects_in_block, used_in_block, lost_in_block, nonfree_in_block);

            smos_free -= nonfree_in_block;
            smos_objects += objects_in_block;
            smos_used += used_in_block;
            smos_lost += lost_in_block;

            if (0 == objects_in_block) smos_empty_blocks++;
            if (0 != objects_in_block && min_objects_in_block > objects_in_block) 
                min_objects_in_block = objects_in_block;
            if (max_objects_in_block < objects_in_block)
                max_objects_in_block = objects_in_block;

            if (0 != objects_in_block && min_used_in_block > used_in_block)
                min_used_in_block = used_in_block;
            if (max_used_in_block < used_in_block)
                max_used_in_block = used_in_block;

            if (0 != objects_in_block) {
                avg_used_in_block += used_in_block;
                avg_objects_in_block += objects_in_block;
                avg_blocks_count++;
            }

            block = block->next_free_block;
        }
    }
    STATS("SmOS capacity = " << smos_blocks * GC_BLOCK_ALLOC_SIZE 
            << " (" <<mb(smos_blocks * GC_BLOCK_ALLOC_SIZE) << " Mb)");
    STATS("SmOS objects = " << smos_objects);
    STATS("SmOS used = " << smos_used
            << " (" << mb(smos_used) << " Mb)");
    STATS("SmOS free = " << smos_free
            << " (" << mb(smos_free) << " Mb)");
    STATS("SmOS lost = " << smos_lost
            << " (" << mb(smos_lost) << " Mb)");
    STATS("");
    STATS("SmOS min used in block = " << min_used_in_block);
    STATS("SmOS max used in block = " << max_used_in_block);
    STATS("SmOS min objects in block = " << min_objects_in_block);
    STATS("SmOS max objects in block = " << max_objects_in_block);
    if (0 != avg_blocks_count) {
        STATS("SmOS avg used in block = " << (avg_used_in_block/avg_blocks_count));
        STATS("SmOS avg objects in block = " << (avg_objects_in_block/avg_blocks_count));
    }
    STATS("");

    int64 total_bytes = los_object_bytes + smos_used;
    int64 total_objects = single_objects + smos_objects;
    STATS("Single object blocks = " << single_object_used_blocks);
    STATS("SmOS blocks = " << smos_blocks << ", empty = " << smos_empty_blocks); 
    STATS("free blocks = " << _p_block_store->get_num_free_blocks_in_block_store());
    STATS("total blocks = " << _p_block_store->get_num_total_blocks_in_block_store());
    STATS("");


    STATS("total objects = " << total_objects);
    STATS("total object bytes = " << total_bytes
            << " (" << mb(total_bytes) << " Mb)");
    STATS("");
}

// end of object iteration code
///////////////////////////////////////////////////////////////////////////

int64 Garbage_Collector::free_chunk_memory()
{
    unsigned i;
    int64 free = 0;
    for (i = 0; i < _gc_chunks.size(); i++) {
        block_info* chunk = _gc_chunks[i].free_chunk;
        // account only for free chunks
        if (chunk != NULL) {
            free += free_in_chunk(chunk);
        }
    }
    return free;
}

/**
 * Returns the amount of memory currently used for the java heap.
 * Typically used to implement java.lang.Runtime.totalMemory().
 */
int64 gc_total_memory() 
{
    return p_global_gc->total_memory();
}

/**
 * Returns the amount of memory GC will use at maximum
 * for the java heap.
 * Typically used to implement java.lang.Runtime.maxMemory().
 */
int64 gc_max_memory()
{
    return p_global_gc->max_memory();
}

/**
 * Returns an approximate amount of the free space, available for the current thread.
 * Typically used to implement java.lang.Runtime.freeMemory().
 */
int64 gc_free_memory() 
{
    GC_Thread_Info* info = (GC_Thread_Info*)vm_get_gc_thread_local();
    return p_global_gc->free_block_memory() // free blocks
        + p_global_gc->free_chunk_memory()  // free in chunks
        + free_in_chunk((block_info*)info->chunk) // free in current chunk
        + ((char*)info->tls_current_ceiling 
                - (char*)info->tls_current_free); // free in current allocation area
}

void Garbage_Collector::notify_gc_shutdown()
{
    total_user_time += gc_time_since_last_gc();
    if (_gc_num > 0) {
        INFO("GC: "
            << _gc_num << " time(s), "
            << "avg " << (total_gc_time/_gc_num/1000) << " ms, "
            << "max " << (max_gc_time/1000) << " ms, "
            << "total " << total_gc_time/1000 << " ms, "
            << "gc/user " << (total_user_time != 0 ? total_gc_time*100/total_user_time : 0) << " %"
        );
    }
}

void Garbage_Collector::notify_gc_start()
{
    // print GC logging separator using different category to keep
    // '-verbose:gc' output (logging category gc.verbose) cleaner
    INFO2("gc.s", "==============================GC[" << _gc_num << "]======================================");

    _gc_num++;
    last_gc_time = 0;
}

void Garbage_Collector::notify_gc_end()
{
    if (last_gc_time > max_gc_time) max_gc_time = last_gc_time;
    total_gc_time += last_gc_time;
    total_user_time += last_user_time;
    INFO("GC[" << (_gc_num - 1) << "]: " << last_gc_time/1000 << " ms, "
        "User " << last_user_time/1000 << " ms, "
        "Total GC " << total_gc_time/1000 << " ms, "
        "Total User " << total_user_time/1000 << " ms"
    );
}
