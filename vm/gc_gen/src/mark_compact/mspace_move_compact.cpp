/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
#include "../los/lspace.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#ifdef USE_32BITS_HASHCODE
#include "../common/hashcode.h"
#endif

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);

static void mspace_move_objects(Collector* collector, Mspace* mspace) 
{
  Block_Header* curr_block = collector->cur_compact_block;
  Block_Header* dest_block = collector->cur_target_block;
  Block_Header *local_last_dest = dest_block;
  
  void* dest_sector_addr = dest_block->base;
  Boolean is_fallback = gc_match_kind(collector->gc, FALLBACK_COLLECTION);
  
#ifdef USE_32BITS_HASHCODE
  Hashcode_Buf* old_hashcode_buf = NULL;
  Hashcode_Buf* new_hashcode_buf = hashcode_buf_create();
  hashcode_buf_init(new_hashcode_buf);
#endif  
 
  while( curr_block ){
    void* start_pos;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &start_pos);

    if( !p_obj ){
 #ifdef USE_32BITS_HASHCODE      
      hashcode_buf_clear(curr_block->hashcode_buf);
 #endif
      curr_block = mspace_get_next_compact_block(collector, mspace);
      continue;    
    }
    
    int curr_sector = OBJECT_INDEX_TO_OFFSET_TABLE(p_obj);
    void* src_sector_addr = p_obj;
          
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
      /* we don't check if it's set, since only remaining objs from last NOS partial collection need it. */
      obj_unmark_in_oi(p_obj); 
      
#ifdef USE_32BITS_HASHCODE
      move_compact_process_hashcode(p_obj, curr_block->hashcode_buf, new_hashcode_buf);
#endif 
      
      POINTER_SIZE_INT curr_sector_size = (POINTER_SIZE_INT)start_pos - (POINTER_SIZE_INT)src_sector_addr;

      /* check if dest block is not enough to hold this sector. If yes, grab next one */      
      POINTER_SIZE_INT block_end = (POINTER_SIZE_INT)GC_BLOCK_END(dest_block);
      if( ((POINTER_SIZE_INT)dest_sector_addr + curr_sector_size) > block_end ){
        dest_block->new_free = dest_sector_addr; 
#ifdef USE_32BITS_HASHCODE
        block_swap_hashcode_buf(dest_block, &new_hashcode_buf, &old_hashcode_buf);
#endif        
        dest_block = mspace_get_next_target_block(collector, mspace);
        if(dest_block == NULL){ 
#ifdef USE_32BITS_HASHCODE
          hashcode_buf_rollback_new_entry(old_hashcode_buf);
#endif
          collector->result = FALSE; 
          return; 
        }
#ifdef USE_32BITS_HASHCODE
        hashcode_buf_transfer_new_entry(old_hashcode_buf, new_hashcode_buf);
#endif 
        if((!local_last_dest) || (dest_block->block_idx > local_last_dest->block_idx))
          local_last_dest = dest_block;
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
      assert((sector_distance % GC_OBJECT_ALIGNMENT) == 0);
      curr_block->table[curr_sector] = sector_distance;

      memmove(dest_sector_addr, src_sector_addr, curr_sector_size);

#ifdef USE_32BITS_HASHCODE
      hashcode_buf_refresh_new_entry(new_hashcode_buf, sector_distance);
#endif

      dest_sector_addr = (void*)((POINTER_SIZE_INT)dest_sector_addr + curr_sector_size);
      src_sector_addr = p_obj;
      curr_sector  = OBJECT_INDEX_TO_OFFSET_TABLE(p_obj);
    }
#ifdef USE_32BITS_HASHCODE      
    hashcode_buf_clear(curr_block->hashcode_buf);
 #endif    
    curr_block = mspace_get_next_compact_block(collector, mspace);
  }
  dest_block->new_free = dest_sector_addr;
  collector->cur_target_block = local_last_dest;
 
#ifdef USE_32BITS_HASHCODE
  old_hashcode_buf = block_set_hashcode_buf(dest_block, new_hashcode_buf);
  hashcode_buf_destory(old_hashcode_buf);
#endif
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
static volatile unsigned int num_restoring_collectors = 0;
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

  if(!gc_match_kind(gc, FALLBACK_COLLECTION))
       mark_scan_heap(collector);  
  else
       mark_scan_heap_for_fallback(collector);

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
    if(lspace->move_object) lspace_compute_object_target(collector, lspace);    
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
    if(lspace->move_object)  lspace_sliding_compact(collector, lspace);    
    num_fixing_collectors++; 
  }
  while(num_fixing_collectors != num_active_collectors + 1);


  /* Pass 4: **************************************************
     restore obj_info                                         */
  atomic_cas32( &num_restoring_collectors, 0, num_active_collectors);
  
  collector_restore_obj_info(collector);

  atomic_inc32(&num_restoring_collectors);

  while(num_restoring_collectors != num_active_collectors);

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
