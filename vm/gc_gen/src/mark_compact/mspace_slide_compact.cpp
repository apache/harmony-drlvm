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

//#define VERIFY_SLIDING_COMPACT

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);

#ifdef VERIFY_SLIDING_COMPACT
typedef struct {
  unsigned int addr;
  unsigned int dest_counter;
  unsigned int collector;
  Block_Header *src_list[1021];
} Block_Verify_Info;
static Block_Verify_Info block_info[32*1024][2];
#endif

static volatile Block_Header *last_block_for_dest;

static void mspace_compute_object_target(Collector* collector, Mspace* mspace)
{  
  Block_Header *curr_block = collector->cur_compact_block;
  Block_Header *dest_block = collector->cur_target_block;
  void *dest_addr = dest_block->base;
  Block_Header *last_src;
  
#ifdef VERIFY_SLIDING_COMPACT
  block_info[(Block*)dest_block-mspace->blocks][0].collector = (unsigned int)collector->thread_handle + 1;
#endif
  
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

      unsigned int obj_size = (unsigned int)start_pos - (unsigned int)p_obj;
      
      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
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

#ifdef VERIFY_SLIDING_COMPACT
        block_info[(Block*)dest_block-mspace->blocks][0].collector = (unsigned int)collector->thread_handle + 1;
#endif
      }
      assert(((unsigned int)dest_addr + obj_size) <= (unsigned int)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);

      if( obj_info != 0 ) {
        collector_remset_add_entry(collector, (Partial_Reveal_Object **)dest_addr);
        collector_remset_add_entry(collector, (Partial_Reveal_Object **)obj_info);
      }
      
      obj_set_fw_in_oi(p_obj, dest_addr);
      
      /* FIXME: should use alloc to handle alignment requirement */
      dest_addr = (void *)((unsigned int) dest_addr + obj_size);
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
    }
    next_block_for_dest = cur_dest_block;
  } else {
    cur_dest_block = set_next_block_for_dest(mspace);
  }

//  printf("Getting next dest block:\n");
//  printf("next_block_for_dest: %d\n\n", next_block_for_dest ? next_block_for_dest->block_idx : 0);
  
  unsigned int total_dest_counter = 0;
  Block_Header *last_dest_block = (Block_Header *)last_block_for_dest;
  for(; cur_dest_block <= last_dest_block; cur_dest_block = cur_dest_block->next){
    if(cur_dest_block->status == BLOCK_DEST){
//      printf("idx: %d  DEST  ", cur_dest_block->block_idx);
      continue;
    }
    if(cur_dest_block->dest_counter == 0 && cur_dest_block->src){
//      printf("idx: %d  DEST  FOUND!\n\n", cur_dest_block->block_idx);
      cur_dest_block->status = BLOCK_DEST;
      return cur_dest_block;
    } else if(cur_dest_block->dest_counter == 1 && GC_BLOCK_HEADER(cur_dest_block->src) == cur_dest_block){
//      printf("idx: %d  NON_DEST  FOUND!\n\n", cur_dest_block->block_idx);
      return cur_dest_block;
    } else if(cur_dest_block->dest_counter == 0 && !cur_dest_block->src){
//      printf("idx: %d  NO_SRC  ", cur_dest_block->block_idx);
      cur_dest_block->status = BLOCK_DEST;
    } else {
//      printf("OTHER  ");
      total_dest_counter += cur_dest_block->dest_counter;
    }
  }
  
  if(total_dest_counter){
//    printf("\nNeed refind!\n\n");
    return DEST_NOT_EMPTY;
  }
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
    if(next_src_obj && GC_BLOCK_HEADER(get_obj_info_raw(next_src_obj)) != next_dest_block){
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


#include "../verify/verify_live_heap.h"
extern unsigned int mspace_free_block_idx;

static void mspace_sliding_compact(Collector* collector, Mspace* mspace)
{
  void *start_pos;
  Block_Header *nos_fw_start_block = (Block_Header *)&mspace->blocks[mspace_free_block_idx - mspace->first_block_idx];
  Boolean is_fallback = (collector->gc->collect_kind == FALLBACK_COLLECTION);
  
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
      
      unsigned int obj_size = (unsigned int)start_pos - (unsigned int)p_obj;
      if(p_obj != p_target_obj){
        memmove(p_target_obj, p_obj, obj_size);

        if(verify_live_heap){
          /* we forwarded it, we need remember it for verification */
          if(is_fallback)
            event_collector_doublemove_obj(p_obj, p_target_obj, collector);
          else
            event_collector_move_obj(p_obj, p_target_obj, collector);
        }
      }
      set_obj_info(p_target_obj, 0);
      
      p_obj = block_get_next_marked_obj_after_prefetch(src_block, &start_pos);
      if(!p_obj)
        break;
      p_target_obj = obj_get_fw_in_oi(p_obj);
    
    } while(GC_BLOCK_HEADER(p_target_obj) == dest_block);

#ifdef VERIFY_SLIDING_COMPACT
    printf("dest_block: %x   src_block: %x   collector: %x\n", (unsigned int)dest_block, (unsigned int)src_block, (unsigned int)collector->thread_handle);
#endif

    atomic_dec32(&src_block->dest_counter);
  }

#ifdef VERIFY_SLIDING_COMPACT
  static unsigned int fax = 0;
  fax++;
  printf("\n\n\nCollector %d   Sliding compact ends!   %d  \n\n\n", (unsigned int)collector->thread_handle, fax);
#endif

}

#ifdef VERIFY_SLIDING_COMPACT

static void verify_sliding_compact(Mspace *mspace, Boolean before)
{
  unsigned int i, j, k;
  Block_Header *header;
  
  if(before)
    j = 0;
  else
    j = 1;
  
  for(i = 0, header = (Block_Header *)mspace->blocks;
      header;
      header=header->next, ++i)
  {
    block_info[i][j].addr = (unsigned int)header;
    block_info[i][j].dest_counter = header->dest_counter;
    if(header->src){
      Partial_Reveal_Object *src_obj = header->src;
      k = 0;
      printf("\nHeader: %x %x Collector: %x  ", (unsigned int)header, block_info[i][j].dest_counter, block_info[i][j].collector);
      Block_Header *dest_header = GC_BLOCK_HEADER(obj_get_fw_in_oi(src_obj));
      while(dest_header == header){
        block_info[i][j].src_list[k] = dest_header;
        Block_Header *src_header = GC_BLOCK_HEADER(src_obj);
        printf("%x %x ", (unsigned int)src_header, src_header->dest_counter);
        src_obj = src_header->next_src;
        if(!src_obj)
          break;
        dest_header = GC_BLOCK_HEADER(obj_get_fw_in_oi(src_obj));
        if(++k >= 1021)
          assert(0);
      }
    }
  }
  
  if(!before){
    for(i = 0, header = (Block_Header *)mspace->blocks;
        header;
        header=header->next, ++i)
    {
      Boolean correct = TRUE;
      if(block_info[i][0].addr != block_info[i][1].addr)
        correct = FALSE;
      if(block_info[i][0].dest_counter != block_info[i][1].dest_counter)
        correct = FALSE;
      for(k = 0; k < 1021; k++){
        if(block_info[i][0].src_list[k] != block_info[i][1].src_list[k]){
          correct = FALSE;
          break;
        }
      }
      if(!correct)
        printf("header: %x %x   dest_counter: %x %x   src: %x %x",
                block_info[i][0].addr, block_info[i][1].addr,
                block_info[i][0].dest_counter, block_info[i][1].dest_counter,
                block_info[i][0].src_list[k], block_info[i][1].src_list[k]);
    }
    
    unsigned int *array = (unsigned int *)block_info;
    memset(array, 0, 1024*32*1024*2);
  }
}
#endif

/*
#define OI_RESTORING_THRESHOLD 8
static volatile Boolean parallel_oi_restoring;
unsigned int mspace_saved_obj_info_size(GC*gc){ return pool_size(gc->metadata->collector_remset_pool);} 
*/

static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_repointing_collectors = 0;
static volatile unsigned int num_fixing_collectors = 0;
static volatile unsigned int num_moving_collectors = 0;
static volatile unsigned int num_restoring_collectors = 0;
static volatile unsigned int num_extending_collectors = 0;

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
      update_ref_ignore_finref(collector);
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
      assert(0);    // Now we should not be out of mem here. mspace_extend_compact() is backing up for this case.
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

    if(!IGNORE_FINREF )
      gc_put_finref_to_vm(gc);
      
#ifdef VERIFY_SLIDING_COMPACT
    verify_sliding_compact(mspace, TRUE);
#endif
    
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
