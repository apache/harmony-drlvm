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

#ifndef _SEMI_SPACE_H_
#define _SEMI_SPACE_H_

#include "../thread/gc_thread.h"

typedef struct Sspace{
  /* <-- first couple of fields are overloadded as Space */
  void* heap_start;
  void* heap_end;
  POINTER_SIZE_INT reserved_heap_size;
  POINTER_SIZE_INT committed_heap_size;
  unsigned int num_collections;
  int64 time_collections;
  float survive_ratio;
  unsigned int collect_algorithm;
  GC* gc;
  Boolean move_object;

  Space_Statistics* space_statistic;

  /* Size allocted since last minor collection. */
  volatile uint64 last_alloced_size;
  /* Size allocted since last major collection. */
  uint64 accumu_alloced_size;
  /* Total size allocated since VM starts. */
  uint64 total_alloced_size;

  /* Size survived from last collection. */
  uint64 last_surviving_size;
  /* Size survived after a certain period. */
  uint64 period_surviving_size;  

  /* END of Space --> */

  Block* blocks; /* short-cut for mpsace blockheader access, not mandatory */
  
  /* FIXME:: the block indices should be replaced with block header addresses */
  unsigned int first_block_idx; /* always pointing to sspace bottom */
  unsigned int ceiling_block_idx; /* tospace ceiling */
  volatile unsigned int free_block_idx; /* tospace cur free block */
  
  unsigned int num_used_blocks;
  unsigned int num_managed_blocks;
  unsigned int num_total_blocks;
  
  volatile Block_Header* block_iterator;
  /* END of Blocked_Space --> */
  
  Block_Header* cur_free_block;
  unsigned int tospace_first_idx;
  void* survivor_area_top;
  void* survivor_area_bottom;

}Sspace;

Sspace *sspace_initialize(GC* gc, void* start, POINTER_SIZE_INT sspace_size, POINTER_SIZE_INT commit_size);
void sspace_destruct(Sspace *sspace);

void* sspace_alloc(unsigned size, Allocator *allocator);
Boolean sspace_alloc_block(Sspace* sspace, Allocator* allocator);

void sspace_collection(Sspace* sspace);
void sspace_prepare_for_collection(Sspace* sspace);
void sspace_reset_after_collection(Sspace* sspace);

void* semispace_alloc(unsigned int size, Allocator* allocator);

void nongen_ss_pool(Collector* collector);
void gen_ss_pool(Collector* collector);

FORCE_INLINE Boolean sspace_has_free_block(Sspace* sspace)
{
  return (sspace->cur_free_block != NULL);
}

FORCE_INLINE Boolean obj_belongs_to_survivor_area(Sspace* sspace, Partial_Reveal_Object* p_obj)
{
  return (p_obj >= sspace->survivor_area_bottom && 
                          p_obj < sspace->survivor_area_top);
}

/* treat semispace alloc as thread local alloc. If it fails or p_obj is old, forward it to MOS */
FORCE_INLINE void* semispace_forward_obj(Partial_Reveal_Object* p_obj, unsigned int size, Allocator* allocator)
{
  void* p_targ_obj = NULL;
  Sspace* sspace = (Sspace*)allocator->alloc_space;
  
  if( !obj_belongs_to_survivor_area(sspace, p_obj) )
    p_targ_obj = semispace_alloc(size, allocator);           
  
  return p_targ_obj;
}

#endif // _FROM_SPACE_H_
