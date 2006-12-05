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
#include "../mark_compact/mspace.h"
#include "../mark_sweep/lspace.h"
#include "../thread/collector.h"

static volatile Block_Header* current_copy_block;
static volatile Block_Header* current_target_block;

static Block_Header* fspace_get_first_copy_block(Fspace* fspace)
{  return (Block_Header*)fspace->blocks; }

static Block_Header* fspace_get_next_copy_block(Fspace* fspace)
{  
  /* FIXME::FIXME:: this only works for full space copying */
  Block_Header* cur_copy_block = (Block_Header*)current_copy_block;
  
  while(cur_copy_block != NULL){
    Block_Header* next_copy_block = current_copy_block->next;

    Block_Header* temp = (Block_Header*)atomic_casptr((volatile void **)&current_copy_block, next_copy_block, cur_copy_block);
    if(temp == cur_copy_block)
      return cur_copy_block;
      
    cur_copy_block = (Block_Header*)current_copy_block;
  }
  /* run out fspace blocks for copying */
  return NULL;
}


/* copying of fspace is only for MAJOR_COLLECTION or non-generational partial copy collection */
static Block_Header* mspace_get_first_target_block_for_nos(Mspace* mspace)
{  
  return (Block_Header*)&mspace->blocks[mspace->free_block_idx-mspace->first_block_idx];
}

static Block_Header* mspace_get_next_target_block_for_nos(Mspace* mspace)
{ 
  Block_Header* mspace_heap_end = (Block_Header*)space_heap_end((Space*)mspace);
  Block_Header* cur_target_block = (Block_Header*)current_target_block;
  Block_Header* next_target_block = current_target_block->next;
  
  while(cur_target_block < mspace_heap_end){
    Block_Header* temp = (Block_Header*)atomic_casptr((volatile void **)&current_target_block, next_target_block, cur_target_block);
    if(temp == cur_target_block)
      return cur_target_block;
      
    cur_target_block = (Block_Header*)current_target_block;
    next_target_block = current_target_block->next;     
  }
  /* mos is always able to hold nos in minor collection */
  assert(0);
  return NULL;
}

struct GC_Gen;
Space* gc_get_mos(GC_Gen* gc);

Boolean fspace_compute_object_target(Collector* collector, Fspace* fspace)
{  
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)collector->gc);
  Block_Header* dest_block = mspace_get_next_target_block_for_nos(mspace);    
  Block_Header* curr_block = fspace_get_next_copy_block(fspace);

  assert(dest_block->status == BLOCK_FREE);
  dest_block->status = BLOCK_USED;
  void* dest_addr = GC_BLOCK_BODY(dest_block);
  
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
            
      unsigned int obj_size = vm_object_size(p_obj);
      
      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
        dest_block->free = dest_addr;
        dest_block = mspace_get_next_target_block_for_nos(mspace);
        if(dest_block == NULL) return FALSE;
        assert(dest_block->status == BLOCK_FREE);
        dest_block->status = BLOCK_USED;
        dest_addr = GC_BLOCK_BODY(dest_block);
      }
      assert(((unsigned int)dest_addr + obj_size) <= (unsigned int)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);
      if( obj_info != 0 ) {
        collector->obj_info_map->insert(ObjectMap::value_type((Partial_Reveal_Object*)dest_addr, obj_info));
      }
      set_forwarding_pointer_in_obj_info(p_obj, dest_addr);

      /* FIXME: should use alloc to handle alignment requirement */
      dest_addr = (void *) WORD_SIZE_ROUND_UP((unsigned int) dest_addr + obj_size);
      p_obj = block_get_next_marked_object(curr_block, &mark_bit_idx);
  
    }
    curr_block = fspace_get_next_copy_block(fspace);
  }
    
  return TRUE;
}   

#include "../verify/verify_live_heap.h"

void fspace_copy_collect(Collector* collector, Fspace* fspace) 
{  
  Block_Header* curr_block = fspace_get_next_copy_block(fspace);
  
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
      obj_unmark_in_vt(p_obj);
      
      unsigned int obj_size = vm_object_size(p_obj);
      Partial_Reveal_Object *p_target_obj = get_forwarding_pointer_in_obj_info(p_obj);
      memmove(p_target_obj, p_obj, obj_size);

      if (verify_live_heap)
        /* we forwarded it, we need remember it for verification */
        event_collector_move_obj(p_obj, p_target_obj, collector);

      set_obj_info(p_target_obj, 0);
 
      p_obj = block_get_next_marked_object(curr_block, &mark_bit_idx);  
    }
        
    curr_block = fspace_get_next_copy_block(fspace);
  }
    
  return;
}

void gc_update_repointed_refs(Collector* collector);

static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_installing_collectors = 0;

void mark_copy_fspace(Collector* collector) 
{  
  GC* gc = collector->gc;
  Fspace* fspace = (Fspace*)collector->collect_space;
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)gc);

  unsigned int num_active_collectors = gc->num_active_collectors;
  
  /* Pass 1: mark all live objects in heap, and save all the slots that 
             have references  that are going to be repointed */
  atomic_cas32( &num_marking_collectors, 0, num_active_collectors+1);
             
  mark_scan_heap(collector);

  unsigned int old_num = atomic_inc32(&num_marking_collectors);
  if( ++old_num == num_active_collectors ){
    /* world for single thread, e.g., verification of last phase, and preparation of next phase */
    current_copy_block = fspace_get_first_copy_block(fspace);
    current_target_block = mspace_get_first_target_block_for_nos(mspace);    
    /* let other collectors go */
    num_marking_collectors++; 
  }
  
  while(num_marking_collectors != num_active_collectors + 1);

  /* Pass 2: assign each live fspace object a new location */
  atomic_cas32( &num_installing_collectors, 0, num_active_collectors+1);

  fspace_compute_object_target(collector, fspace);  

  old_num = atomic_inc32(&num_installing_collectors);
  if( ++old_num == num_active_collectors){
    /* nothing to do in this single thread region */
    mspace->free_block_idx = current_target_block->block_idx;
    num_installing_collectors++; 
  }
  
  while(num_installing_collectors != num_active_collectors + 1);

  /* FIXME:: temporary. let only one thread go forward */
  if( collector->thread_handle != 0 ) return;
  
  gc_update_repointed_refs(collector);

  /* FIXME:: Pass 2 and 3 can be merged into one pass */
  /* Pass 3: copy live fspace object to new location */
  current_copy_block = fspace_get_first_copy_block(fspace);
  fspace_copy_collect(collector, fspace);
          
  /* FIXME:: should be collector_restore_obj_info(collector) */
  gc_restore_obj_info(gc);
  
  reset_fspace_for_allocation(fspace);  
    
  return;
}
