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

static void mspace_destruct_blocks(Mspace* mspace)
{ 
  Block* blocks = (Block*)mspace->blocks; 
  for(unsigned int i=0; i < mspace->num_managed_blocks; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    delete block->reloc_table;
    block->reloc_table = NULL;
  }
  
  return;
}

static void mspace_init_blocks(Mspace* mspace)
{ 
  Block* blocks = (Block*)mspace->heap_start; 
  Block_Header* last_block = (Block_Header*)blocks;
  unsigned int start_idx = mspace->first_block_idx;
  for(unsigned int i=0; i < mspace->num_managed_blocks; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block->free = (void*)((unsigned int)block + GC_BLOCK_HEADER_SIZE_BYTES);
    block->ceiling = (void*)((unsigned int)block + GC_BLOCK_SIZE_BYTES); 
    block->base = block->free;
    block->block_idx = i + start_idx;
    block->status = BLOCK_FREE;  
    block->reloc_table = new SlotVector();
    last_block->next = block;
    last_block = block;
  }
  last_block->next = NULL;
  mspace->blocks = blocks;
   
  return;
}

struct GC_Gen;
extern void gc_set_mos(GC_Gen* gc, Space* space);
void mspace_initialize(GC* gc, void* start, unsigned int mspace_size)
{
  Mspace* mspace = (Mspace*)STD_MALLOC( sizeof(Mspace));
  assert(mspace);
  memset(mspace, 0, sizeof(Mspace));
  
  mspace->reserved_heap_size = mspace_size;
  mspace->num_total_blocks = mspace_size >> GC_BLOCK_SHIFT_COUNT;

  void* reserved_base = start;
  int status = port_vmem_commit(&reserved_base, mspace_size, gc->allocated_memory); 
  assert(status == APR_SUCCESS && reserved_base == start);
  
  memset(reserved_base, 0, mspace_size);
  mspace->committed_heap_size = mspace_size;
  mspace->heap_start = reserved_base;
  mspace->heap_end = (void *)((unsigned int)reserved_base + mspace->reserved_heap_size);
  mspace->num_managed_blocks = mspace_size >> GC_BLOCK_SHIFT_COUNT;
  
  mspace->first_block_idx = GC_BLOCK_INDEX_FROM(gc->heap_start, reserved_base);
  mspace->ceiling_block_idx = mspace->first_block_idx + mspace->num_managed_blocks - 1;
  
  mspace->num_used_blocks = 0;
  mspace->free_block_idx = mspace->first_block_idx;
  
  mspace_init_blocks(mspace);
  
  mspace->obj_info_map = new ObjectMap();
  mspace->mark_object_func = mspace_mark_object;
  mspace->save_reloc_func = mspace_save_reloc;
  mspace->update_reloc_func = mspace_update_reloc;

  mspace->move_object = TRUE;
  mspace->gc = gc;
  gc_set_mos((GC_Gen*)gc, (Space*)mspace);

  return;
}


void mspace_destruct(Mspace* mspace)
{
  //FIXME:: when map the to-half, the decommission start address should change
  mspace_destruct_blocks(mspace);
  port_vmem_decommit(mspace->heap_start, mspace->committed_heap_size, mspace->gc->allocated_memory);
  STD_FREE(mspace);  
}

  /* for non-gen MINOR_COLLECTION, mspace has both obj and marktable to be cleared,
     because the marking phase will mark them, but then never touch them 
     
     FIXME:: the marking choice between header and mark table has to be decided.
     Obj header marking has advantage of idempotent, while table marking can prefetch 
     If we choose only one, we will not have the two version clearings: one after
     MAJOR_COLLECTION, one after non-gen MINOR_COLLECTION */
     
void reset_mspace_after_copy_nursery(Mspace* mspace)
{ 
  /* for major collection we do nothing, the reset is done there */
  assert( mspace->gc->collect_kind == MINOR_COLLECTION );

  unsigned int new_num_used = mspace->free_block_idx - mspace->first_block_idx;
  unsigned int old_num_used = mspace->num_used_blocks;

  /* At the moment, for MINOR_COLLECTION, only non-gen collection does copying.
     The generational version does forwarding */
  assert( !gc_requires_barriers());
  
  Block* blocks = mspace->blocks;
  for(unsigned int i=0; i < old_num_used; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block_clear_markbits(block); 
  }

  for(unsigned int i=old_num_used; i < new_num_used; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block->status = BLOCK_USED;
  }

  mspace->num_used_blocks = new_num_used;  
  return;
}

