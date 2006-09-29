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
#ifndef _gc_v4_H_
#define _gc_v4_H_

#include <apr_time.h>

#include "gc_header.h"
#include "open/vm.h"

#include "gc_cout.h"

#define GC_MAX_CHUNKS 32 * 1024         // 512K per chunk, so this gives us 16GB heap max if needed

#define GC_OBJECT_LOW_MARK_INDEX(P_OBJ, SIZE)  ((((POINTER_SIZE_INT)P_OBJ & GC_BLOCK_LOW_MASK) >> GC_MARK_SHIFT_COUNT) - GC_NUM_MARK_BYTES_FOR_BLOCK_INFO_SIZE_BYTES)
#define GC_OBJECT_HIGH_MARK_INDEX(P_OBJ, SIZE) (((((POINTER_SIZE_INT)P_OBJ + SIZE - 1) & GC_BLOCK_LOW_MASK) >> GC_MARK_SHIFT_COUNT) - GC_NUM_MARK_BYTES_FOR_BLOCK_INFO_SIZE_BYTES)
#define GC_OBJ_MARK(P_OBJ, INDEX) GC_BLOCK_INFO(P_OBJ)->gc_mark_table[INDEX]
#define GC_BLOCK_ADDR_FROM_MARK_INDEX(BLOCK_INFO, INDEX) (((INDEX + GC_NUM_MARK_BYTES_FOR_BLOCK_INFO_SIZE_BYTES) << GC_MARK_SHIFT_COUNT) + (POINTER_SIZE_INT) BLOCK_INFO)

#define GC_NUM_ROOTS_HINT    10000

// What should a GC thread do???
enum gc_thread_action {
    
    GC_MARK_SCAN_TASK = 0,
    GC_SWEEP_TASK, 
    GC_OBJECT_HEADERS_CLEAR_TASK,
////////////////////////////// new tasks ////////////////////
    GC_INSERT_COMPACTION_LIVE_OBJECTS_INTO_COMPACTION_BLOCKS_TASK,
    GC_ALLOCATE_FORWARDING_POINTERS_FOR_COMPACTION_LIVE_OBJECTS_TASK,
    GC_FIX_SLOTS_TO_COMPACTION_LIVE_OBJECTS_TASK,
    GC_SLIDE_COMPACT_LIVE_OBJECTS_IN_COMPACTION_BLOCKS,
    GC_RESTORE_HIJACKED_HEADERS,
////////////////////////////// //////////////////////////////
    GC_BOGUS_TASK
};

typedef struct {
    uint8 *p_start_byte;
    unsigned int start_bit_index;
    uint8 *p_non_zero_byte;
    unsigned int bit_set_index;
    uint8 *p_ceil_byte;
} set_bit_search_info;



typedef struct {

    block_info *free_chunk;
    block_info *chunk;

} chunk_info;


#ifdef _DEBUG
typedef struct {

    unsigned int average_number_of_free_areas;
    unsigned int average_size_per_free_area;

} chunk_sweep_stats;
#endif





//////////////////////////////////////////////////////////////////////////////////////////////
class GC_Thread;
class GC_Mark_Activity;
//////////////////////////////////////////////////////////////////////////////////////////////


#ifndef PLATFORM_POSIX
extern void clear_block_free_areas(block_info *);
#endif // PLATFORM_POSIX
block_info *p_get_new_block(bool );
Partial_Reveal_Object  *gc_pinned_malloc(unsigned, Allocation_Handle, bool, bool);

void verify_object (Partial_Reveal_Object *p_object, POINTER_SIZE_INT );
void verify_block (block_info *);
void verify_marks_for_all_live_objects();

#ifndef PLATFORM_POSIX
bool mark_object(Partial_Reveal_Object *);
#endif // PLATFORM_POSIX
void scan_root(Partial_Reveal_Object *p_obj, GC_Mark_Activity *gc_thread);

void verify_marks_for_live_object(Partial_Reveal_Object *);



//////////////////////////// V A R I A B L E S /////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////



static inline void 
clear_block_free_areas(block_info *block)
{
    for (int i = 0; i < GC_MAX_FREE_AREAS_PER_BLOCK; i++) {
        memset(&(block->block_free_areas[i]), 0, sizeof(free_area));
    }
}


#ifdef _DEBUG
static inline void 
zero_out_sweep_stats(chunk_sweep_stats *stats) {
    assert(stats);
    memset(stats, 0, sizeof(chunk_sweep_stats) * GC_MAX_CHUNKS);
}
#endif


////////////////////////////////// M E A S U R E M E N T /////////////////////////////////////////////////////////////////////


static inline void
gc_time_start_hook(apr_time_t *start_time)
{   
    *start_time = apr_time_now();
}


static inline apr_time_t
gc_time_end_hook(const char *event, apr_time_t *start_time, apr_time_t *end_time)
{   
    *end_time = apr_time_now();
    apr_time_t time =  *end_time - *start_time;
    INFO2("gc.time", time/1000 << " ms, " << event);
    return time;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Centralize all mallocs done in the GC to this function.
//

void *malloc_or_die(unsigned int size);

///////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // _gc_v4_H_
