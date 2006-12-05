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
#include "../thread/collector.h"
#include "../trace_forward/fspace.h"

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);

static void reset_mspace_after_compaction(Mspace* mspace)
{ 
  unsigned int old_num_used = mspace->num_used_blocks;
  unsigned int new_num_used = mspace->free_block_idx - mspace->first_block_idx;
  unsigned int num_used = old_num_used>new_num_used? old_num_used:new_num_used;
  
  Block* blocks = mspace->blocks;
  unsigned int i;
  for(i=0; i < num_used; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block_clear_mark_table(block); 
    block->status = BLOCK_USED;

    if(i >= new_num_used){
      block->status = BLOCK_FREE; 
      block->free = GC_BLOCK_BODY(block);
    }
  }
  mspace->num_used_blocks = new_num_used;
  
  /* we should clear the remaining blocks which are set to be BLOCK_COMPACTED or BLOCK_TARGET */
  for(; i < mspace->num_managed_blocks; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    assert(block->status& (BLOCK_COMPACTED|BLOCK_TARGET));
    block->status = BLOCK_FREE;
  }
}

static volatile Block_Header* next_block_for_compact;
static volatile Block_Header* next_block_for_target;

static void gc_reset_block_for_collectors(GC* gc, Mspace* mspace)
{
  unsigned int free_blk_idx = mspace->first_block_idx;
  for(unsigned int i=0; i<gc->num_active_collectors; i++){
    Collector* collector = gc->collectors[i];
    unsigned int collector_target_idx = collector->cur_target_block->block_idx;
    if(collector_target_idx > free_blk_idx)
      free_blk_idx = collector_target_idx;
    collector->cur_target_block = NULL;
    collector->cur_compact_block = NULL;
  }
  mspace->free_block_idx = free_blk_idx+1;
  return;
}

static void gc_init_block_for_collectors(GC* gc, Mspace* mspace)
{
  unsigned int i;
  Block_Header* block;
  for(i=0; i<gc->num_active_collectors; i++){
    Collector* collector = gc->collectors[i];
    block = (Block_Header*)&mspace->blocks[i];
    collector->cur_target_block = block;
    collector->cur_compact_block = block;
    block->status = BLOCK_TARGET;
  }
  
  block = (Block_Header*)&mspace->blocks[i];
  next_block_for_target = block;
  next_block_for_compact = block;
  return;
}

static Boolean gc_collection_result(GC* gc)
{
  Boolean result = TRUE;
  for(unsigned i=0; i<gc->num_active_collectors; i++){
    Collector* collector = gc->collectors[i];
    result &= collector->result;
  }  
  return result;
}

static Block_Header* mspace_get_first_compact_block(Mspace* mspace)
{ return (Block_Header*)mspace->blocks; }

static Block_Header* mspace_get_first_target_block(Mspace* mspace)
{ return (Block_Header*)mspace->blocks; }


static Block_Header* mspace_get_next_compact_block1(Mspace* mspace, Block_Header* block)
{  return block->next; }

static Block_Header* mspace_get_next_compact_block(Collector* collector, Mspace* mspace)
{ 
  /* firstly put back the compacted block. If it's not BLOCK_TARGET, it will be set to BLOCK_COMPACTED */
  unsigned int block_status = collector->cur_compact_block->status;
  assert( block_status & (BLOCK_IN_COMPACT|BLOCK_TARGET));
  if( block_status == BLOCK_IN_COMPACT)
    collector->cur_compact_block->status = BLOCK_COMPACTED;

  Block_Header* cur_compact_block = (Block_Header*)next_block_for_compact;
  
  while(cur_compact_block != NULL){
    Block_Header* next_compact_block = cur_compact_block->next;

    Block_Header* temp = (Block_Header*)atomic_casptr((volatile void **)&next_block_for_compact, next_compact_block, cur_compact_block);
    if(temp != cur_compact_block){
      cur_compact_block = (Block_Header*)next_block_for_compact;
      continue;
    }
    /* got it, set its state to be BLOCK_IN_COMPACT. It must be the first time touched by compactor */
    block_status = cur_compact_block->status;
    assert( !(block_status & (BLOCK_IN_COMPACT|BLOCK_COMPACTED|BLOCK_TARGET)));
    cur_compact_block->status = BLOCK_IN_COMPACT;
    collector->cur_compact_block = cur_compact_block;
    return cur_compact_block;
      
  }
  /* run out space blocks for compacting */
  return NULL;
}

static Block_Header* mspace_get_next_target_block(Collector* collector, Mspace* mspace)
{    
  Block_Header* cur_target_block = (Block_Header*)next_block_for_target;
  
  /* firstly, we bump the next_block_for_target global var to the first non BLOCK_TARGET block
     This need not atomic op, because the global var is only a hint. */
  while(cur_target_block->status == BLOCK_TARGET){
      cur_target_block = cur_target_block->next;
  }
  next_block_for_target = cur_target_block;

  /* cur_target_block has to be BLOCK_IN_COMPACT|BLOCK_COMPACTED|BLOCK_TARGET. Reason: 
     Any block after it must be either BLOCK_TARGET, or: 
     1. Since cur_target_block < cur_compact_block, we at least can get cur_compact_block as target.
     2. For a block that is >=cur_target_block and <cur_compact_block. 
        Since it is before cur_compact_block, we know it must be a compaction block of some thread. 
        So it is either BLOCK_IN_COMPACT or BLOCK_COMPACTED. 
     We care only the BLOCK_COMPACTED block or own BLOCK_IN_COMPACT. But I can't make the assert
     as below because of a race condition where the block status is not yet updated by other thread.
    assert( cur_target_block->status & (BLOCK_IN_COMPACT|BLOCK_COMPACTED|BLOCK_TARGET)); 
  */

  /* nos is higher than mos, we cant use nos block for compaction target */
  Block_Header* mspace_heap_end = (Block_Header*)space_heap_end((Space*)mspace);
  while( cur_target_block < mspace_heap_end ){
    assert( cur_target_block <= collector->cur_compact_block);
    Block_Header* next_target_block = cur_target_block->next;
    volatile unsigned int* p_block_status = &cur_target_block->status;
    unsigned int block_status = cur_target_block->status;
    //assert( block_status & (BLOCK_IN_COMPACT|BLOCK_COMPACTED|BLOCK_TARGET));

    /* if it is not BLOCK_COMPACTED, let's move on to next except it's own cur_compact_block */
    if(block_status != BLOCK_COMPACTED){
      if(cur_target_block == collector->cur_compact_block){
        assert( block_status == BLOCK_IN_COMPACT);
        *p_block_status = BLOCK_TARGET;
        collector->cur_target_block = cur_target_block;
        return cur_target_block;
      }
      /* it's not my own cur_compact_block, it can be BLOCK_TARGET or other's cur_compact_block */
      cur_target_block = next_target_block;
      continue;
    }    
    /* else, find a BLOCK_COMPACTED before own cur_compact_block */    
    unsigned int temp = atomic_cas32(p_block_status, BLOCK_TARGET, BLOCK_COMPACTED);
    if(temp == BLOCK_COMPACTED){
      collector->cur_target_block = cur_target_block;
      return cur_target_block;
    }
    /* missed it, it must be set by other into BLOCK_TARGET */
    assert(temp == BLOCK_TARGET); 
    cur_target_block = next_target_block;     
  }
  /* mos is run out for major collection */
  return NULL;  
}

Boolean mspace_mark_object(Mspace* mspace, Partial_Reveal_Object *p_obj)
{  
#ifdef _DEBUG 
  if( obj_is_marked_in_vt(p_obj)) return FALSE;
#endif

  obj_mark_in_vt(p_obj);

  unsigned int obj_word_index = OBJECT_WORD_INDEX_TO_MARKBIT_TABLE(p_obj);
  unsigned int obj_offset_in_word = OBJECT_WORD_OFFSET_IN_MARKBIT_TABLE(p_obj);   
  
  unsigned int *p_word = &(GC_BLOCK_HEADER(p_obj)->mark_table[obj_word_index]);
  unsigned int word_mask = (1<<obj_offset_in_word);
  
  unsigned int old_value = *p_word;
  unsigned int new_value = old_value|word_mask;
  
  while(old_value != new_value){
    unsigned int temp = atomic_cas32(p_word, new_value, old_value);
    if(temp == old_value) return TRUE;
    old_value = *p_word;
    new_value = old_value|word_mask;
  }
  return FALSE;
}

static void mspace_compute_object_target(Collector* collector, Mspace* mspace)
{  
  Block_Header* curr_block = collector->cur_compact_block;
  Block_Header* dest_block = collector->cur_target_block;

  void* dest_addr = GC_BLOCK_BODY(dest_block);
 
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
            
      unsigned int obj_size = vm_object_size(p_obj);
      
      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
        dest_block->free = dest_addr;
        dest_block = mspace_get_next_target_block(collector, mspace);
        if(dest_block == NULL){ 
          collector->result = FALSE; 
          return; 
        }
        
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
    curr_block = mspace_get_next_compact_block(collector, mspace);
  }
  
  return;
}   

#include "../verify/verify_live_heap.h"

static void mspace_sliding_compact(Collector* collector, Mspace* mspace)
{
  Block_Header* curr_block = mspace_get_first_compact_block(mspace);
  
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
      obj_unmark_in_vt(p_obj);
      
      unsigned int obj_size = vm_object_size(p_obj);
      Partial_Reveal_Object *p_target_obj = get_forwarding_pointer_in_obj_info(p_obj);
      if( p_obj != p_target_obj){
        memmove(p_target_obj, p_obj, obj_size);

        if (verify_live_heap)
          /* we forwarded it, we need remember it for verification */
          event_collector_move_obj(p_obj, p_target_obj, collector);
      }
     
      set_obj_info(p_target_obj, 0);
 
      p_obj = block_get_next_marked_object(curr_block, &mark_bit_idx);  
    }
        
    curr_block = mspace_get_next_compact_block1(mspace, curr_block);
  }

  return;
} 

void gc_update_repointed_refs(Collector* collector);

static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_installing_collectors = 0;

static void mark_compact_mspace(Collector* collector) 
{
  GC* gc = collector->gc;
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)gc);
  Fspace* fspace = (Fspace*)gc_get_nos((GC_Gen*)gc);

  /* Pass 1: mark all live objects in heap, and save all the slots that 
             have references  that are going to be repointed */
  unsigned int num_active_collectors = gc->num_active_collectors;
  
  /* Pass 1: mark all live objects in heap, and save all the slots that 
             have references  that are going to be repointed */
  unsigned int old_num = atomic_cas32( &num_marking_collectors, 0, num_active_collectors+1);

  mark_scan_heap(collector);

  old_num = atomic_inc32(&num_marking_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
    /* prepare for next phase */
    gc_init_block_for_collectors(gc, mspace); 
    /* let other collectors go */
    num_marking_collectors++; 
  }
  
  while(num_marking_collectors != num_active_collectors + 1);
  
  /* Pass 2: assign target addresses for all to-be-moved objects */
  atomic_cas32( &num_installing_collectors, 0, num_active_collectors+1);

  mspace_compute_object_target(collector, mspace);   
  
  old_num = atomic_inc32(&num_installing_collectors);
  if( ++old_num == num_active_collectors ){
    /* single thread world */
    if(!gc_collection_result(gc)){
      printf("Out of Memory!\n");
      assert(0); /* mos is out. FIXME:: throw exception */
    }
    gc_reset_block_for_collectors(gc, mspace);
    num_installing_collectors++; 
  }
  
  while(num_installing_collectors != num_active_collectors + 1);

  /* FIXME:: temporary. let only one thread go forward */
  if( collector->thread_handle != 0 ) return;
    
  /* Pass 3: update all references whose objects are to be moved */  
  gc_update_repointed_refs(collector);
    
  /* Pass 4: do the compaction and reset blocks */  
  next_block_for_compact = mspace_get_first_compact_block(mspace);
  mspace_sliding_compact(collector, mspace);
  /* FIXME:: should be collector_restore_obj_info(collector) */
  gc_restore_obj_info(gc);

  reset_mspace_after_compaction(mspace);
  reset_fspace_for_allocation(fspace);
  
  return;
}

void mspace_collection(Mspace* mspace) 
{
  mspace->num_collections++;

  GC* gc = mspace->gc;  

  pool_iterator_init(gc->metadata->gc_rootset_pool);

  collector_execute_task(gc, (TaskType)mark_compact_mspace, (Space*)mspace);
  
  return;  
} 
