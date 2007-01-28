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
 * @author Chunrong Lai, 2006/12/25
 */

#include "mspace_collect_compact.h"
#include "../trace_forward/fspace.h"
#include "../mark_sweep/lspace.h"
#include "../finalizer_weakref/finalizer_weakref.h"

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);

#include "../verify/verify_live_heap.h"

static void mspace_move_objects(Collector* collector, Mspace* mspace) 
{
  Block_Header* curr_block = collector->cur_compact_block;
  Block_Header* dest_block = collector->cur_target_block;
  
  void* dest_sector_addr = dest_block->base;
  Boolean is_fallback = (collector->gc->collect_kind == FALLBACK_COLLECTION);
  
 
  while( curr_block ){
    void* start_pos;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &start_pos);

    if( !p_obj ){
      curr_block = mspace_get_next_compact_block(collector, mspace);
      continue;    
    }
    
    int curr_sector = OBJECT_INDEX_TO_OFFSET_TABLE(p_obj);
    void* src_sector_addr = p_obj;
          
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
      /* we don't check if it's set, since only remaining objs from last NOS partial collection need it. */
      obj_unmark_in_oi(p_obj); 
      
      POINTER_SIZE_INT curr_sector_size = (POINTER_SIZE_INT)start_pos - (POINTER_SIZE_INT)src_sector_addr;

      /* check if dest block is not enough to hold this sector. If yes, grab next one */      
      POINTER_SIZE_INT block_end = (POINTER_SIZE_INT)GC_BLOCK_END(dest_block);
      if( ((POINTER_SIZE_INT)dest_sector_addr + curr_sector_size) > block_end ){
        dest_block->new_free = dest_sector_addr; 
        dest_block = mspace_get_next_target_block(collector, mspace);
        if(dest_block == NULL){ 
          collector->result = FALSE; 
          return; 
        }
        block_end = (POINTER_SIZE_INT)GC_BLOCK_END(dest_block);
        dest_sector_addr = dest_block->base;
      }
        
      assert(((POINTER_SIZE_INT)dest_sector_addr + curr_sector_size) <= block_end );

      /* check if current sector has no more sector. If not, loop back. FIXME:: we should add a condition for block check */      
      p_obj =  block_get_next_marked_object(curr_block, &start_pos);
      if ((p_obj != NULL) && (OBJECT_INDEX_TO_OFFSET_TABLE(p_obj) == curr_sector))
        continue;

      /* current sector is done, let's move it. */
      POINTER_SIZE_INT sector_distance = (POINTER_SIZE_INT)src_sector_addr - (POINTER_SIZE_INT)dest_sector_addr;
      curr_block->table[curr_sector] = sector_distance;

      if (verify_live_heap) {
           Partial_Reveal_Object *rescan_obj = (Partial_Reveal_Object *)src_sector_addr;
           void *rescan_pos = (Partial_Reveal_Object *)((POINTER_SIZE_INT)rescan_obj + vm_object_size(rescan_obj));
           while ((POINTER_SIZE_INT)rescan_obj < (POINTER_SIZE_INT)src_sector_addr + curr_sector_size) {
            Partial_Reveal_Object* targ_obj = (Partial_Reveal_Object *)((POINTER_SIZE_INT)rescan_obj- sector_distance);
             if(is_fallback)
               event_collector_doublemove_obj(rescan_obj, targ_obj, collector);
             else
               event_collector_move_obj(rescan_obj, targ_obj, collector);
              rescan_obj = block_get_next_marked_object(curr_block, &rescan_pos);  
              if(rescan_obj == NULL) break;
           }
      }
         
      memmove(dest_sector_addr, src_sector_addr, curr_sector_size);

      dest_sector_addr = (void*)((POINTER_SIZE_INT)dest_sector_addr + curr_sector_size);
      src_sector_addr = p_obj;
      curr_sector  = OBJECT_INDEX_TO_OFFSET_TABLE(p_obj);
    }
    curr_block = mspace_get_next_compact_block(collector, mspace);
  }
  dest_block->new_free = dest_sector_addr;
 
  return;
}

#include "../common/fix_repointed_refs.h"

static void mspace_fix_repointed_refs(Collector *collector, Mspace *mspace)
{
  Block_Header* curr_block = mspace_block_iterator_next(mspace);
  
  while( curr_block){
    if(curr_block->block_idx >= mspace->free_block_idx) break;    
    curr_block->free = curr_block->new_free; //
    block_fix_ref_after_marking(curr_block);
    curr_block = mspace_block_iterator_next(mspace);
  }
  
  return;
}
      
static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_fixing_collectors = 0;
static volatile unsigned int num_moving_collectors = 0;
static volatile unsigned int num_extending_collectors = 0;

void move_compact_mspace(Collector* collector) 
{
  GC* gc = collector->gc;
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)gc);
  Fspace* fspace = (Fspace*)gc_get_nos((GC_Gen*)gc);
  Lspace* lspace = (Lspace*)gc_get_los((GC_Gen*)gc);
  
  unsigned int num_active_collectors = gc->num_active_collectors;
  
  /* Pass 1: **************************************************
     mark all live objects in heap, and save all the slots that 
            have references  that are going to be repointed */
  unsigned int old_num = atomic_cas32( &num_marking_collectors, 0, num_active_collectors+1);

  if(gc->collect_kind != FALLBACK_COLLECTION)    
       mark_scan_heap(collector);  
  else
       fallback_mark_scan_heap(collector);    

  old_num = atomic_inc32(&num_marking_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
    /* prepare for next phase */
    gc_init_block_for_collectors(gc, mspace); 
    
    if(!IGNORE_FINREF )
      collector_identify_finref(collector);
#ifndef BUILD_IN_REFERENT
    else {
      gc_set_weakref_sets(gc);
      gc_update_weakref_ignore_finref(gc);
    }
#endif
    
    /* let other collectors go */
    num_marking_collectors++; 
  }
  while(num_marking_collectors != num_active_collectors + 1);
  
  /* Pass 2: **************************************************
     move object and set the forwarding offset table */
  atomic_cas32( &num_moving_collectors, 0, num_active_collectors+1);

  mspace_move_objects(collector, mspace);   
  
  old_num = atomic_inc32(&num_moving_collectors);
  if( ++old_num == num_active_collectors ){
    /* single thread world */
    gc->collect_result = gc_collection_result(gc);
    if(!gc->collect_result){
      num_moving_collectors++; 
      return;
    }
 
    gc_reset_block_for_collectors(gc, mspace);
    mspace_block_iterator_init(mspace);
    num_moving_collectors++; 
  }
  while(num_moving_collectors != num_active_collectors + 1);
  if(!gc->collect_result) return;
    
  /* Pass 3: **************************************************
     update all references whose pointed objects were moved */  
  old_num = atomic_cas32( &num_fixing_collectors, 0, num_active_collectors+1);

  mspace_fix_repointed_refs(collector, mspace);

  old_num = atomic_inc32(&num_fixing_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
    lspace_fix_repointed_refs(collector, lspace);   
    gc_fix_rootset(collector);
    update_mspace_info_for_los_extension(mspace);
    num_fixing_collectors++; 
  }
  while(num_fixing_collectors != num_active_collectors + 1);

   /* Dealing with out of memory in mspace */  
  if(mspace->free_block_idx > fspace->first_block_idx){    
     atomic_cas32( &num_extending_collectors, 0, num_active_collectors);        
     mspace_extend_compact(collector);        
     atomic_inc32(&num_extending_collectors);    
     while(num_extending_collectors != num_active_collectors);  
  }
  
  /* Leftover: **************************************************
   */
  if( collector->thread_handle != 0 ) return;

  mspace_reset_after_compaction(mspace);
  fspace_reset_for_allocation(fspace);

  gc_set_pool_clear(gc->metadata->gc_rootset_pool);  
  
  return;
}
