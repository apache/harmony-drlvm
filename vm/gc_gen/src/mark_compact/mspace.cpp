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

static void mspace_init_blocks(Mspace* mspace)
{
  int size = sizeof(Block_Info)* mspace->num_total_blocks;
  Block_Info* block_info =  (Block_Info*)STD_MALLOC(size);
  memset(block_info, 0, size);
  
  for(unsigned int i=0; i < mspace->num_current_blocks; i++){
    Block_Header* block = (Block_Header*)((unsigned int)space_heap_start((Space*)mspace) + i*mspace->block_size_bytes);
    block->free = (void*)((unsigned int)block + GC_BLOCK_HEADER_SIZE_BYTES);
    block->ceiling = (void*)((unsigned int)block + mspace->block_size_bytes); 
    block->base = block->free;
    block->block_idx = i;
    
    block_info[i].block = block;
    block_info[i].status = BLOCK_FREE;  
    block_info[i].reloc_table = new SlotVector();
  }
  
  mspace->block_info = block_info;
  
  return;
}

struct GC_Gen;
extern void gc_set_mos(GC_Gen* gc, Space* space);
void mspace_initialize(GC* gc, void* start, unsigned int mspace_size)
{
    Mspace* mspace = (Mspace*)STD_MALLOC( sizeof(Mspace));
    assert(mspace);
    memset(mspace, 0, sizeof(Mspace));
    
    mspace->block_size_bytes = GC_BLOCK_SIZE_BYTES;
    mspace->reserved_heap_size = mspace_size;
    mspace->num_total_blocks = mspace_size/mspace->block_size_bytes;

    mspace_size >>= 1;   
    void* reserved_base = start;
    int status = port_vmem_commit(&reserved_base, mspace_size, gc->allocated_memory); 
    assert(status == APR_SUCCESS && reserved_base == start);
    
    memset(reserved_base, 0, mspace_size);
    mspace->committed_heap_size = mspace_size;
    mspace->heap_start = reserved_base;
    mspace->heap_end = (void *)((unsigned int)reserved_base + mspace->reserved_heap_size);
    mspace->num_current_blocks = mspace_size/mspace->block_size_bytes;
    
    mspace->num_used_blocks = 0;
    mspace->free_block_idx = 0;
    
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
  /* inverse of initialize */
}

