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

#include "mspace.h"

#include "../common/gc_space.h"

static void mspace_destruct_blocks(Mspace* mspace)
{   
  return;
}

struct GC_Gen;
extern void gc_set_mos(GC_Gen* gc, Space* space);
extern Space* gc_get_nos(GC_Gen* gc);

void mspace_initialize(GC* gc, void* start, POINTER_SIZE_INT mspace_size, POINTER_SIZE_INT commit_size)
{
  Mspace* mspace = (Mspace*)STD_MALLOC( sizeof(Mspace));
  assert(mspace);
  memset(mspace, 0, sizeof(Mspace));
  
  mspace->reserved_heap_size = mspace_size;
  mspace->num_total_blocks = (unsigned int)(mspace_size >> GC_BLOCK_SHIFT_COUNT);

  void* reserved_base = start;
  /* commit mspace mem */
  if(!large_page_hint)
    vm_commit_mem(reserved_base, commit_size);
  memset(reserved_base, 0, commit_size);
  
  mspace->committed_heap_size = commit_size;
  mspace->heap_start = reserved_base;
  
#ifdef STATIC_NOS_MAPPING
  mspace->heap_end = (void *)((POINTER_SIZE_INT)reserved_base + mspace_size);
#else
  mspace->heap_end = (void *)((POINTER_SIZE_INT)reserved_base + commit_size);
#endif

  mspace->num_managed_blocks = (unsigned int)(commit_size >> GC_BLOCK_SHIFT_COUNT);
  
  mspace->first_block_idx = GC_BLOCK_INDEX_FROM(gc->heap_start, reserved_base);
  mspace->ceiling_block_idx = mspace->first_block_idx + mspace->num_managed_blocks - 1;
  
  mspace->num_used_blocks = 0;
  mspace->free_block_idx = mspace->first_block_idx;
  
  space_init_blocks((Blocked_Space*)mspace);

  mspace->num_collections = 0;
  mspace->time_collections = 0;
  mspace->survive_ratio = 0.2f;

  mspace->move_object = TRUE;
  mspace->gc = gc;

  /*For_LOS adaptive: The threshold is initiated by half of MOS + NOS commit size.*/
  mspace->expected_threshold = (unsigned int)( ( (float)mspace->committed_heap_size * (1.f + 1.f / gc->survive_ratio) ) * 0.5f );

  gc_set_mos((GC_Gen*)gc, (Space*)mspace);

  return;
}


void mspace_destruct(Mspace* mspace)
{
  //FIXME:: when map the to-half, the decommission start address should change
  mspace_destruct_blocks(mspace);
  STD_FREE(mspace);  
}

void mspace_block_iterator_init_free(Mspace* mspace)
{
  mspace->block_iterator = (Block_Header*)&mspace->blocks[mspace->free_block_idx - mspace->first_block_idx];
}

//For_LOS_extend
#include "../common/space_tuner.h"
void mspace_block_iterator_init(Mspace* mspace)
{
  GC* gc = mspace->gc;
  if(gc->tuner->kind == TRANS_FROM_MOS_TO_LOS){
    unsigned int tuning_blocks = (unsigned int)((mspace->gc)->tuner->tuning_size >> GC_BLOCK_SHIFT_COUNT);
    mspace->block_iterator = (Block_Header*)&(mspace->blocks[tuning_blocks]);
    return;
  }
  
  mspace->block_iterator = (Block_Header*)mspace->blocks;
  return;
}


Block_Header* mspace_block_iterator_get(Mspace* mspace)
{
  return (Block_Header*)mspace->block_iterator;
}

Block_Header* mspace_block_iterator_next(Mspace* mspace)
{
  Block_Header* cur_block = (Block_Header*)mspace->block_iterator;
  
  while(cur_block != NULL){
    Block_Header* next_block = cur_block->next;

    Block_Header* temp = (Block_Header*)atomic_casptr((volatile void **)&mspace->block_iterator, next_block, cur_block);
    if(temp != cur_block){
      cur_block = (Block_Header*)mspace->block_iterator;
      continue;
    }
    return cur_block;
  }
  /* run out space blocks */
  return NULL;  
}

#include "../common/fix_repointed_refs.h"

void mspace_fix_after_copy_nursery(Collector* collector, Mspace* mspace)
{
  //the first block is not set yet
  Block_Header* curr_block = mspace_block_iterator_next(mspace);
  unsigned int first_block_idx = mspace->first_block_idx;
  unsigned int old_num_used = mspace->num_used_blocks;
  unsigned int old_free_idx = first_block_idx + old_num_used;
  unsigned int new_free_idx = mspace->free_block_idx;
  
  /* for NOS copy, we are sure about the last block for fixing */
  Block_Header* space_end = (Block_Header*)&mspace->blocks[new_free_idx-first_block_idx];  
  
  while( curr_block < space_end){
    assert(curr_block->status == BLOCK_USED);
    if( curr_block->block_idx < old_free_idx)
      /* for blocks used before nos copy */
      block_fix_ref_after_marking(curr_block); 
  
    else  /* for blocks used for nos copy */
      block_fix_ref_after_copying(curr_block); 
         
    curr_block = mspace_block_iterator_next(mspace);
  }
   
  return;  
}

/*For_LOS adaptive.*/
void mspace_set_expected_threshold(Mspace* mspace, POINTER_SIZE_INT threshold)
{
    mspace->expected_threshold = threshold;
    return;
}

POINTER_SIZE_INT mspace_get_expected_threshold(Mspace* mspace)
{
    return mspace->expected_threshold;
}


