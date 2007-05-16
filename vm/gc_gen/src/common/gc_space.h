/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Xiao-Feng Li, 2006/10/05
 */

#ifndef _GC_SPACE_H_
#define _GC_SPACE_H_

#include "gc_block.h"

extern unsigned int SPACE_ALLOC_UNIT;

struct GC;
/* all Spaces inherit this Space structure */
typedef struct Space{
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
  /*Size allocted after last collection. */
  POINTER_SIZE_INT alloced_size;
  /*For_statistic*/  
  POINTER_SIZE_INT surviving_size;
}Space;

inline POINTER_SIZE_INT space_committed_size(Space* space){ return space->committed_heap_size;}
inline void* space_heap_start(Space* space){ return space->heap_start; }
inline void* space_heap_end(Space* space){ return space->heap_end; }

inline Boolean address_belongs_to_space(void* addr, Space* space) 
{
  return (addr >= space_heap_start(space) && addr < space_heap_end(space));
}

inline Boolean obj_belongs_to_space(Partial_Reveal_Object *p_obj, Space* space)
{
  return address_belongs_to_space((Partial_Reveal_Object*)p_obj, space);
}


typedef struct Blocked_Space {
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
  /*Size allocted after last collection. */
  POINTER_SIZE_INT alloced_size;
  /*For_statistic*/  
  POINTER_SIZE_INT surviving_size;
  /* END of Space --> */

  Block* blocks; /* short-cut for mpsace blockheader access, not mandatory */
  
  /* FIXME:: the block indices should be replaced with block header addresses */
  unsigned int first_block_idx;
  unsigned int ceiling_block_idx;
  volatile unsigned int free_block_idx;
  
  unsigned int num_used_blocks;
  unsigned int num_managed_blocks;
  unsigned int num_total_blocks;
  /* END of Blocked_Space --> */
}Blocked_Space;

inline Boolean space_has_free_block(Blocked_Space* space){ return space->free_block_idx <= space->ceiling_block_idx; }
inline unsigned int space_free_memory_size(Blocked_Space* space){ return GC_BLOCK_SIZE_BYTES * (space->ceiling_block_idx - space->free_block_idx + 1);  }
inline Boolean space_used_memory_size(Blocked_Space* space){ return GC_BLOCK_SIZE_BYTES * (space->free_block_idx - space->first_block_idx); }

inline void space_init_blocks(Blocked_Space* space)
{ 
  Block* blocks = (Block*)space->heap_start; 
  Block_Header* last_block = (Block_Header*)blocks;
  unsigned int start_idx = space->first_block_idx;
  for(unsigned int i=0; i < space->num_managed_blocks; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block_init(block);
    block->block_idx = i + start_idx;
    last_block->next = block;
    last_block = block;
  }
  last_block->next = NULL;
  space->blocks = blocks;
   
  return;
}

inline void space_desturct_blocks(Blocked_Space* space)
{
  Block* blocks = (Block*)space->heap_start; 
  unsigned int i=0;
  for(; i < space->num_managed_blocks; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block_destruct(block);
  }
}

inline void blocked_space_shrink(Blocked_Space* space, unsigned int changed_size)
{
  unsigned int block_dec_count = changed_size >> GC_BLOCK_SHIFT_COUNT;
  void* new_base = (void*)&(space->blocks[space->num_managed_blocks - block_dec_count]);
 
  void* decommit_base = (void*)round_down_to_size((POINTER_SIZE_INT)new_base, SPACE_ALLOC_UNIT);
  
  assert( ((Block_Header*)decommit_base)->block_idx >= space->free_block_idx);
  
  void* old_end = (void*)&space->blocks[space->num_managed_blocks];
  POINTER_SIZE_INT decommit_size = (POINTER_SIZE_INT)old_end - (POINTER_SIZE_INT)decommit_base;
  assert(decommit_size && !(decommit_size%GC_BLOCK_SIZE_BYTES));
  
  Boolean result = vm_decommit_mem(decommit_base, decommit_size);
  assert(result == TRUE);
  
  space->committed_heap_size = (POINTER_SIZE_INT)decommit_base - (POINTER_SIZE_INT)space->heap_start;
  space->num_managed_blocks = (unsigned int)(space->committed_heap_size >> GC_BLOCK_SHIFT_COUNT);
  
  Block_Header* new_last_block = (Block_Header*)&space->blocks[space->num_managed_blocks - 1];
  space->ceiling_block_idx = new_last_block->block_idx;
  new_last_block->next = NULL;
}

inline void blocked_space_extend(Blocked_Space* space, unsigned int changed_size)
{
  unsigned int block_inc_count = changed_size >> GC_BLOCK_SHIFT_COUNT;
  
  void* old_base = (void*)&space->blocks[space->num_managed_blocks];
  void* commit_base = (void*)round_down_to_size((POINTER_SIZE_INT)old_base, SPACE_ALLOC_UNIT);
  unsigned int block_diff_count = (unsigned int)(((POINTER_SIZE_INT)old_base - (POINTER_SIZE_INT)commit_base) >> GC_BLOCK_SHIFT_COUNT);
  block_inc_count += block_diff_count;
  
  POINTER_SIZE_INT commit_size = block_inc_count << GC_BLOCK_SHIFT_COUNT;
  void* result = vm_commit_mem(commit_base, commit_size);
  assert(result == commit_base);

  void* new_end = (void*)((POINTER_SIZE_INT)commit_base + commit_size);
  space->committed_heap_size = (POINTER_SIZE_INT)new_end - (POINTER_SIZE_INT)space->heap_start;
  
  /* init the grown blocks */
  Block_Header* block = (Block_Header*)commit_base;
  Block_Header* last_block = (Block_Header*)((Block*)block -1);
  unsigned int start_idx = last_block->block_idx + 1;
  unsigned int i;
  for(i=0; block < new_end; i++){
    block_init(block);
    block->block_idx = start_idx + i;
    last_block->next = block;
    last_block = block;
    block = (Block_Header*)((Block*)block + 1);  
  }
  last_block->next = NULL;
  space->ceiling_block_idx = last_block->block_idx;
  space->num_managed_blocks = (unsigned int)(space->committed_heap_size >> GC_BLOCK_SHIFT_COUNT);
}

#endif //#ifndef _GC_SPACE_H_
