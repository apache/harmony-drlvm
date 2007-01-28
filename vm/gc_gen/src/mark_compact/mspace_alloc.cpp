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

static Boolean mspace_alloc_block(Mspace* mspace, Allocator* allocator)
{
  alloc_context_reset(allocator);

  /* now try to get a new block */
  unsigned int old_free_idx = mspace->free_block_idx;
  unsigned int new_free_idx = old_free_idx+1;
  while( old_free_idx <= mspace->ceiling_block_idx ){   
    unsigned int allocated_idx = atomic_cas32(&mspace->free_block_idx, new_free_idx, old_free_idx);
    if(allocated_idx != old_free_idx){
      old_free_idx = mspace->free_block_idx;
      new_free_idx = old_free_idx+1;
      continue;
    }
    /* ok, got one */
    Block_Header* alloc_block = (Block_Header*)&(mspace->blocks[allocated_idx - mspace->first_block_idx]);
    assert(alloc_block->status == BLOCK_FREE);
    alloc_block->status = BLOCK_IN_USE;
    /*For_statistic mos allocation infomation*/
    mspace->alloced_size += GC_BLOCK_SIZE_BYTES;
    
    /* set allocation context */
    void* new_free = alloc_block->free;
    allocator->free = new_free;

#ifndef ALLOC_ZEROING

    allocator->ceiling = alloc_block->ceiling;
    memset(new_free, 0, GC_BLOCK_BODY_SIZE_BYTES);

#else

    /* the first-time zeroing area includes block header, to make subsequent allocs page aligned */
    unsigned int zeroing_size = ZEROING_SIZE - GC_BLOCK_HEADER_SIZE_BYTES;
    allocator->ceiling = (void*)((POINTER_SIZE_INT)new_free + zeroing_size);
    memset(new_free, 0, zeroing_size);

#endif /* #ifndef ALLOC_ZEROING */

    allocator->end = alloc_block->ceiling;
    allocator->alloc_block = (Block*)alloc_block; 
    
    return TRUE;
  }

  /* Mspace is out, a collection should be triggered. It can be caused by mutator allocation
     And it can be caused by collector allocation during nos forwarding. */
  return FALSE;
  
}

struct GC_Gen;
Space* gc_get_mos(GC_Gen* gc);
void* mspace_alloc(unsigned int size, Allocator* allocator)
{
  void *p_return = NULL;
 
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)allocator->gc);
  
  /* All chunks of data requested need to be multiples of GC_OBJECT_ALIGNMENT */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert( size <= GC_OBJ_SIZE_THRESHOLD );

  /* check if collector local alloc block is ok. If not, grab a new block */
  p_return = thread_local_alloc(size, allocator);
  if(p_return) return p_return;
  
  /* grab a new block */
  Boolean ok = mspace_alloc_block(mspace, allocator);
  if(!ok) return NULL; 
  
  p_return = thread_local_alloc(size, allocator);
  assert(p_return);
    
  return p_return;
}

