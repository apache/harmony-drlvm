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

#include "fspace.h"

static Boolean fspace_alloc_block(Fspace* fspace, Allocator* allocator)
{
  alloc_context_reset(allocator);

  /* now try to get a new block */
  unsigned int old_free_idx = fspace->free_block_idx;
  unsigned int new_free_idx = old_free_idx+1;
  while(old_free_idx <= fspace->ceiling_block_idx){   
    unsigned int allocated_idx = atomic_cas32(&fspace->free_block_idx, new_free_idx, old_free_idx);
    if(allocated_idx != old_free_idx){     /* if failed */  
      old_free_idx = fspace->free_block_idx;
      new_free_idx = old_free_idx+1;
      continue;
    }
    /* ok, got one */
    Block_Header* alloc_block = (Block_Header*)&(fspace->blocks[allocated_idx - fspace->first_block_idx]);
    assert(alloc_block->status == BLOCK_FREE);
    alloc_block->status = BLOCK_IN_USE;
    
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

  return FALSE;
  
}

/* FIXME:: the collection should be separated from the allocation */
void* fspace_alloc(unsigned size, Allocator *allocator) 
{
  void*  p_return = NULL;

  /* First, try to allocate object from TLB (thread local block) */
  p_return = thread_local_alloc(size, allocator);
  if (p_return)  return p_return;

  /* ran out local block, grab a new one*/  
  Fspace* fspace = (Fspace*)allocator->alloc_space;
  int attempts = 0;
  while( !fspace_alloc_block(fspace, allocator)){
    vm_gc_lock_enum();
    /* after holding lock, try if other thread collected already */
    if ( !space_has_free_block((Blocked_Space*)fspace) ) {  
        if(attempts < 2) {
          gc_reclaim_heap(allocator->gc, GC_CAUSE_NOS_IS_FULL); 
          attempts++;
        }else{
          vm_gc_unlock_enum();  
          return NULL;
        }
    }    
    vm_gc_unlock_enum();  
  }
  
  p_return = thread_local_alloc(size, allocator);
  
  return p_return;
  
}


