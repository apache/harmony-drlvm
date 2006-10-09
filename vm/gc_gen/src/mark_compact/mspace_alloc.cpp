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

#include "mspace_collect.h"

static Boolean mspace_alloc_block(Mspace* mspace, Alloc_Context* alloc_ctx)
{
  int old_free_idx = mspace->free_block_idx;
  int new_free_idx = old_free_idx+1;

  Block_Info* curr_alloc_block = (Block_Info* )alloc_ctx->curr_alloc_block;
  if(curr_alloc_block != NULL){ /* it is NULL at first time */
    assert(curr_alloc_block->status == BLOCK_IN_USE);
    curr_alloc_block->status = BLOCK_USED;
    curr_alloc_block->block->free = alloc_ctx->free;
  }

  while( mspace_has_free_block(mspace)){
    
    unsigned int allocated_idx = atomic_cas32(&mspace->free_block_idx, new_free_idx, old_free_idx);
      
    if(allocated_idx != (POINTER_SIZE_INT)old_free_idx){
      old_free_idx = mspace->free_block_idx;
      new_free_idx = old_free_idx+1;
      continue;
    }
  
    Block_Info* curr_alloc_block = &(mspace->block_info[allocated_idx]);

    assert(curr_alloc_block->status == BLOCK_FREE);
    curr_alloc_block->status = BLOCK_IN_USE;
    mspace->num_used_blocks++;

    alloc_ctx->curr_alloc_block = curr_alloc_block; 
    Block_Header* block = curr_alloc_block->block;
    memset(block->free, 0, (unsigned int)block->ceiling - (unsigned int)block->free);
    alloc_ctx->free = block->free;
    alloc_ctx->ceiling = block->ceiling;
    
    return TRUE;
  }
  
  /* FIXME:: collect Mspace if for mutator, else assert(0) */
  assert(0);
  return FALSE;
  
}

struct GC_Gen;
Space* gc_get_mos(GC_Gen* gc);
void* mspace_alloc(unsigned int size, Alloc_Context* alloc_ctx)
{
  void *p_return = NULL;
 
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)alloc_ctx->gc);
  
  /* All chunks of data requested need to be multiples of GC_OBJECT_ALIGNMENT */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert( size <= GC_OBJ_SIZE_THRESHOLD );

  /* check if collector local alloc block is ok. If not, grab a new block */
  p_return = thread_local_alloc(size, alloc_ctx);
  if(p_return) return p_return;
  
  /* grab a new block */
  Boolean ok = mspace_alloc_block(mspace, alloc_ctx);
  assert(ok);
  
  p_return = thread_local_alloc(size, alloc_ctx);
  assert(p_return);
    
  return p_return;
}

