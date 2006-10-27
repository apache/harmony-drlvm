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
  Block_Header* alloc_block = (Block_Header* )allocator->alloc_block;
  /* put back the used block */
  if(alloc_block != NULL){ /* it is NULL at first time */
    assert(alloc_block->status == BLOCK_IN_USE);
    alloc_block->status = BLOCK_USED;
    alloc_block->free = allocator->free;
  }

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
    alloc_block = (Block_Header*)&(mspace->blocks[allocated_idx - mspace->first_block_idx]);
    assert(alloc_block->status == BLOCK_FREE);
    alloc_block->status = BLOCK_IN_USE;
    mspace->num_used_blocks++;
    memset(alloc_block->free, 0, GC_BLOCK_BODY_SIZE_BYTES);
    
    /* set allocation context */
    allocator->free = alloc_block->free;
    allocator->ceiling = alloc_block->ceiling;
    allocator->alloc_block = (Block*)alloc_block; 
    
    return TRUE;
  }

  /* if Mspace is used for mutator allocation, here a collection should be triggered. 
     else if this is only for collector allocation, when code goes here, it means 
     Mspace is not enough to hold Nursery live objects, so the invoker of this routine 
     should throw out-of-memory exception.
     But because in our design, we don't do any Mspace allocation during collection, this
     path should never be reached. That's why we assert(0) here. */  
  assert(0);
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
  assert(ok);
  
  p_return = thread_local_alloc(size, allocator);
  assert(p_return);
    
  return p_return;
}

