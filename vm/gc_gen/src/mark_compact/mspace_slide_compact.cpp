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

#include "mspace_collect_compact.h"
#include "../trace_forward/fspace.h"
#include "../mark_sweep/lspace.h"
#include "../finalizer_weakref/finalizer_weakref.h"


struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);

static volatile Block_Header *last_block_for_dest;

static void mspace_compute_object_target(Collector* collector, Mspace* mspace)
{  
  Block_Header *curr_block = collector->cur_compact_block;
  Block_Header *dest_block = collector->cur_target_block;
  void *dest_addr = dest_block->base;
  Block_Header *last_src;
  
  assert(!collector->rem_set);
  collector->rem_set = free_set_pool_get_entry(collector->gc->metadata);
  
  while( curr_block ){
    void* start_pos;
    Partial_Reveal_Object *first_obj = block_get_first_marked_obj_prefetch_next(curr_block, &start_pos);
    if(first_obj){
      ++curr_block->dest_counter;
      if(!dest_block->src)
        dest_block->src = first_obj;
      else
        last_src->next_src = first_obj;
      last_src = curr_block;
    }
    Partial_Reveal_Object* p_obj = first_obj;
 
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));

      unsigned int obj_size = (unsigned int)((POINTER_SIZE_INT)start_pos - (POINTER_SIZE_INT)p_obj);
      
      if( ((POINTER_SIZE_INT)dest_addr + obj_size) > (POINTER_SIZE_INT)GC_BLOCK_END(dest_block)){
        dest_block->new_free = dest_addr;
        dest_block = mspace_get_next_target_block(collector, mspace);
        if(dest_block == NULL){ 
          collector->result = FALSE; 
          return; 
        }
        dest_addr = dest_block->base;
        dest_block->src = p_obj;
        last_src = curr_block;
        if(p_obj != first_obj)
          ++curr_block->dest_counter;
      }
      assert(((POINTER_SIZE_INT)dest_addr + obj_size) <= (POINTER_SIZE_INT)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);

      if( obj_info != 0 ) {
        collector_remset_add_entry(collector, (Partial_Reveal_Object **)dest_addr);
        collector_remset_add_entry(collector, (Partial_Reveal_Object **)(POINTER_SIZE_INT)obj_info);
      }
      
      obj_set_fw_in_oi(p_obj, dest_addr);
      
      /* FIXME: should use alloc to handle alignment requirement */
      dest_addr = (void *)((POINTER_SIZE_INT) dest_addr + obj_size);
      p_obj = block_get_next_marked_obj_prefetch_next(curr_block, &start_pos);
    }
    
    curr_block = mspace_get_next_compact_block(collector, mspace);
  
  }
  
  pool_put_entry(collector->gc->metadata->collector_remset_pool, collector->rem_set);
  collector->rem_set = NULL;
  dest_block->new_free = dest_addr;
  
  Block_Header *cur_last_dest = (Block_Header *)last_block_for_dest;
  while(dest_block > last_block_for_dest){
    atomic_casptr((volatile void **)&last_block_for_dest, dest_block, cur_last_dest);
    cur_last_dest = (Block_Header *)last_block_for_dest;
  }
  
  return;
}   

#include "../common/fix_repointed_refs.h"

static void mspace_fix_repointed_refs(Collector* collector, Mspace* mspace)
{
  Block_Header* curr_block = mspace_block_iterator_next(mspace);
  
  /* for MAJOR_COLLECTION, we must iterate over all compact blocks */
  while( curr_block){
    block_fix_ref_after_repointing(curr_block); 
    curr_block = mspace_block_iterator_next(mspace);
  }

  return;
}

typedef struct{
  volatile Block_Header *block;
  SpinLock lock;
} Cur_Dest_Block;

static Cur_Dest_Block current_dest_block;
static volatile Block_Header *next_block_for_dest;

static inline Block_Header *set_next_block_for_dest(Mspace *mspace)
{
  assert(!next_block_for_dest);
  
  Block_Header *block = mspace_block_iterator_get(mspace);
  
  if(block->status != BLOCK_DEST)
    return block;
  
  while(block->status == BLOCK_DEST)
    block = block->next;
  next_block_for_dest = block;
  return block;
}

#define DEST_NOT_EMPTY ((Block_Header *)0xFF)

static Block_Header *get_next_dest_block(Mspace *mspace)
{
  Block_Header *cur_dest_block;
  
  if(next_block_for_dest){
    cur_dest_block = (Block_Header*)next_block_for_dest;
    while(cur_dest_block->status == BLOCK_DEST){
      cur_dest_block = cur_dest_block->next;
      if(!cur_dest_block) break;
    }
    next_block_for_dest = cur_dest_block;
  } else {
    cur_dest_block = set_next_block_for_dest(mspace);
  }
  
  unsigned int total_dest_counter = 0;
  Block_Header *last_dest_block = (Block_Header *)last_block_for_dest;
  for(; cur_dest_block <= last_dest_block; cur_dest_block = cur_dest_block->next){
    if(!cur_dest_block)  return NULL;
    if(cur_dest_block->status == BLOCK_DEST){
      continue;
    }
    if(cur_dest_block->dest_counter == 0 && cur_dest_block->src){
      cur_dest_block->status = BLOCK_DEST;
      return cur_dest_block;
    } else if(cur_dest_block->dest_counter == 1 && GC_BLOCK_HEADER(cur_dest_block->src) == cur_dest_block){
      return cur_dest_block;
    } else if(cur_dest_block->dest_counter == 0 && !cur_dest_block->src){
      cur_dest_block->status = BLOCK_DEST;
    } else {
      total_dest_counter += cur_dest_block->dest_counter;
    }
  }
  
  if(total_dest_counter)
    return DEST_NOT_EMPTY;
  
  return NULL;
}

static Block_Header *check_dest_block(Mspace *mspace)
{
  Block_Header *cur_dest_block;
  
  if(next_block_for_dest){
    cur_dest_block = (Block_Header*)next_block_for_dest;
    while(cur_dest_block->status == BLOCK_DEST){
      cur_dest_block = cur_dest_block->next;
    }
  } else {
    cur_dest_block = set_next_block_for_dest(mspace);
  }

  unsigned int total_dest_counter = 0;
  Block_Header *last_dest_block = (Block_Header *)last_block_for_dest;
  for(; cur_dest_block < last_dest_block; cur_dest_block = cur_dest_block->next){
    if(cur_dest_block->status == BLOCK_DEST)
      continue;
    if(cur_dest_block->dest_counter == 0 && cur_dest_block->src){
      return cur_dest_block;
    } else if(cur_dest_block->dest_counter == 1 && GC_BLOCK_HEADER(cur_dest_block->src) == cur_dest_block){
      return cur_dest_block;
    } else if(cur_dest_block->dest_counter == 0 && !cur_dest_block->src){
      cur_dest_block->status = BLOCK_DEST;
    } else {
      total_dest_counter += cur_dest_block->dest_counter;
    }
  }
  
  if(total_dest_counter) return DEST_NOT_EMPTY;
  return NULL;
}

static inline Partial_Reveal_Object *get_next_first_src_obj(Mspace *mspace)
{
  Partial_Reveal_Object *first_src_obj;
  
  while(TRUE){
    lock(current_dest_block.lock);
    Block_Header *next_dest_block = (Block_Header *)current_dest_block.block;
    
    if (!next_dest_block || !(first_src_obj = next_dest_block->src)){
      next_dest_block = get_next_dest_block(mspace);
      if(!next_dest_block){
        unlock(current_dest_block.lock);
        return NULL;
      } else if(next_dest_block == DEST_NOT_EMPTY){
        unlock(current_dest_block.lock);
        while(check_dest_block(mspace)==DEST_NOT_EMPTY);
        continue;
      }
      first_src_obj = next_dest_block->src;
      if(next_dest_block->status == BLOCK_DEST){
        assert(!next_dest_block->dest_counter);
        current_dest_block.block = next_dest_block;
      }
    }
    
    Partial_Reveal_Object *next_src_obj = GC_BLOCK_HEADER(first_src_obj)->next_src;
    if(next_src_obj && GC_BLOCK_HEADER(ref_to_obj_ptr((REF)get_obj_info_raw(next_src_obj))) != next_dest_block){
      next_src_obj = NULL;
    }
    next_dest_block->src = next_src_obj;
    unlock(current_dest_block.lock);
    return first_src_obj;
  }
}

static inline void gc_init_block_for_sliding_compact(GC *gc, Mspace *mspace)
{
  /* initialize related static variables */
  next_block_for_dest = NULL;
  current_dest_block.block = NULL;
  current_dest_block.lock = FREE_LOCK;
  mspace_block_iterator_init(mspace);

  return;
}

extern unsigned int mspace_free_block_idx;

static void mspace_sliding_compact(Collector* collector, Mspace* mspace)
{
  void *start_pos;
  Block_Header *nos_fw_start_block = (Block_Header *)&mspace->blocks[mspace_free_block_idx - mspace->first_block_idx];
  Boolean is_fallback = gc_match_kind(collector->gc, FALLBACK_COLLECTION);
  
  while(Partial_Reveal_Object *p_obj = get_next_first_src_obj(mspace)){
    Block_Header *src_block = GC_BLOCK_HEADER(p_obj);
    assert(src_block->dest_counter);
    
    Partial_Reveal_Object *p_target_obj = obj_get_fw_in_oi(p_obj);
    Block_Header *dest_block = GC_BLOCK_HEADER(p_target_obj);
    
    /* We don't set start_pos as p_obj in case that memmove of this obj may overlap itself.
     * In that case we can't get the correct vt and obj_info.
     */
    start_pos = obj_end(p_obj);
    
    do {
      assert(obj_is_marked_in_vt(p_obj));
      obj_unmark_in_vt(p_obj);
      
      unsigned int obj_size = (unsigned int)((POINTER_SIZE_INT)start_pos - (POINTER_SIZE_INT)p_obj);
      if(p_obj != p_target_obj){
        memmove(p_target_obj, p_obj, obj_size);
      }
      set_obj_info(p_target_obj, 0);
      
      p_obj = block_get_next_marked_obj_after_prefetch(src_block, &start_pos);
      if(!p_obj)
        break;
      p_target_obj = obj_get_fw_in_oi(p_obj);
    
    } while(GC_BLOCK_HEADER(p_target_obj) == dest_block);
    
    atomic_dec32(&src_block->dest_counter);
  }

}

//For_LOS_extend
void mspace_restore_block_chain(Mspace* mspace)
{
  GC* gc = mspace->gc;
  Fspace* fspace = (Fspace*)gc_get_nos((GC_Gen*)gc);
  if(gc->tuner->kind == TRANS_FROM_MOS_TO_LOS) {
      Block_Header* fspace_last_block = (Block_Header*)&fspace->blocks[fspace->num_managed_blocks - 1];
      fspace_last_block->next = NULL;
  }
}

static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_repointing_collectors = 0;
static volatile unsigned int num_fixing_collectors = 0;
static volatile unsigned int num_moving_collectors = 0;
static volatile unsigned int num_restoring_collectors = 0;
static volatile unsigned int num_extending_collectors = 0;

void slide_compact_mspace(Collector* collector) 
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

  if(gc_match_kind(gc, FALLBACK_COLLECTION))
    fallback_mark_scan_heap(collector);
  else if(gc->cause == GC_CAUSE_LOS_IS_FULL)
    los_extention_mark_scan_heap(collector);
  else
    mark_scan_heap(collector);
  
  old_num = atomic_inc32(&num_marking_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
    if(gc->cause == GC_CAUSE_LOS_IS_FULL)
      retune_los_size(gc);
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
    
    last_block_for_dest = NULL;
    
    /* let other collectors go */
    num_marking_collectors++; 
  }
  while(num_marking_collectors != num_active_collectors + 1);

  /* Pass 2: **************************************************
     assign target addresses for all to-be-moved objects */
  atomic_cas32( &num_repointing_collectors, 0, num_active_collectors+1);

  mspace_compute_object_target(collector, mspace);
  
  old_num = atomic_inc32(&num_repointing_collectors);
  if( ++old_num == num_active_collectors ){
    /* single thread world */
    gc->collect_result = gc_collection_result(gc);
    if(!gc->collect_result){
      num_repointing_collectors++;
      return;
    }
    
    gc_reset_block_for_collectors(gc, mspace);
    mspace_block_iterator_init(mspace);
    num_repointing_collectors++; 
  }
  while(num_repointing_collectors != num_active_collectors + 1);
  if(!gc->collect_result) return;

  /* Pass 3: **************************************************
     update all references whose objects are to be moved */  
  old_num = atomic_cas32( &num_fixing_collectors, 0, num_active_collectors+1);
  mspace_fix_repointed_refs(collector, mspace);
  old_num = atomic_inc32(&num_fixing_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
    lspace_fix_repointed_refs(collector, lspace);
    gc_fix_rootset(collector);
    gc_init_block_for_sliding_compact(gc, mspace);
    num_fixing_collectors++; 
  }
  while(num_fixing_collectors != num_active_collectors + 1);

  /* Pass 4: **************************************************
     move objects                                             */
  atomic_cas32( &num_moving_collectors, 0, num_active_collectors);
  
  mspace_sliding_compact(collector, mspace); 
  
  atomic_inc32(&num_moving_collectors);
  while(num_moving_collectors != num_active_collectors);

  /* Pass 5: **************************************************
     restore obj_info                                         */
  atomic_cas32( &num_restoring_collectors, 0, num_active_collectors+1);
  
  collector_restore_obj_info(collector);
  
  old_num = atomic_inc32(&num_restoring_collectors);
  if( ++old_num == num_active_collectors ){

    update_mspace_info_for_los_extension(mspace);
    
    num_restoring_collectors++;
  }
  while(num_restoring_collectors != num_active_collectors + 1);

  /* Dealing with out of memory in mspace */
  if(mspace->free_block_idx > fspace->first_block_idx){
    atomic_cas32( &num_extending_collectors, 0, num_active_collectors);
    
    mspace_extend_compact(collector);
    
    atomic_inc32(&num_extending_collectors);
    while(num_extending_collectors != num_active_collectors);
  }
  if( collector->thread_handle != 0 )
    return;
  
  /* Leftover: **************************************************
   */
  
  mspace_reset_after_compaction(mspace);
  fspace_reset_for_allocation(fspace);

  //For_LOS_extend
  mspace_restore_block_chain(mspace);

  gc_set_pool_clear(gc->metadata->gc_rootset_pool);
  
  return;
}
