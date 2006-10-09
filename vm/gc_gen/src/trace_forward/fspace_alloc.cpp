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

static Boolean fspace_alloc_block(Fspace* fspace, Alloc_Context *alloc_ctx)
{
         
  void* old_free_pos = (void*)fspace->alloc_free;
  void* new_free_pos = (void*)((POINTER_SIZE_INT)old_free_pos + fspace->block_size_bytes);
  while ( fspace_has_free_block(fspace) ){
    /* There are enough space to hold a TLB in fspace, try to get it */
    POINTER_SIZE_INT temp = atomic_cas32((volatile uint32 *)&fspace->alloc_free, \
                            (POINTER_SIZE_INT)new_free_pos, (POINTER_SIZE_INT)old_free_pos);
      
    if(temp != (POINTER_SIZE_INT)old_free_pos){
      old_free_pos = (void*)fspace->alloc_free;
      new_free_pos = (void*)((POINTER_SIZE_INT)old_free_pos + fspace->block_size_bytes);
      continue;
    }
    
    alloc_ctx->free = (char*)old_free_pos;
    alloc_ctx->ceiling = (char*)old_free_pos + fspace->block_size_bytes;
    memset(old_free_pos, 0, fspace->block_size_bytes);
    
    return TRUE;
  }
  
  return FALSE;      
}
/* FIXME:: the collection should be seperated from the alloation */
struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
void gc_gen_reclaim_heap(GC_Gen* gc, unsigned int cause);

void* fspace_alloc(unsigned size, Alloc_Context *alloc_ctx) 
{
  void*  p_return = NULL;

  /* First, try to allocate object from TLB (thread local block) */
  p_return = thread_local_alloc(size, alloc_ctx);
  if (p_return)  return p_return;
  
  /* grab another TLB */
  Fspace* fspace = (Fspace*)alloc_ctx->alloc_space;

retry_grab_block:
  if ( !fspace_has_free_block(fspace)) {    /* can not get a new TLB, activate GC */ 
    vm_gc_lock_enum();
    /* after holding lock, try if other thread collected already */
    if ( !fspace_has_free_block(fspace) ) {  
      gc_gen_reclaim_heap((GC_Gen*)alloc_ctx->gc, GC_CAUSE_NOS_IS_FULL); 
    }    
    vm_gc_unlock_enum();  
  }

  Boolean ok = fspace_alloc_block(fspace, alloc_ctx);
  if(ok){
    p_return = thread_local_alloc(size, alloc_ctx);
    assert(p_return);              
    return p_return;
  }

  /* failed to get a TLB atomically */
  goto retry_grab_block;
  
}

