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

static Block_Header* mspace_get_first_compact_block(Mspace* mspace)
{ return (Block_Header*)mspace->blocks; }

static Block_Header* mspace_get_next_compact_block(Mspace* mspace, Block_Header* block)
{ return block->next; }

static Block_Header* mspace_get_first_target_block(Mspace* mspace)
{ return (Block_Header*)mspace->blocks; }

static Block_Header* mspace_get_next_target_block(Mspace* mspace, Block_Header* block)
{ return block->next; }

void mspace_save_reloc(Mspace* mspace, Partial_Reveal_Object** p_ref)
{
  Block_Header* block = GC_BLOCK_HEADER(p_ref);
  block->reloc_table->push_back(p_ref);
  return;
}

void  mspace_update_reloc(Mspace* mspace)
{
  SlotVector* reloc_table;
  /* update refs in mspace */
  Block* blocks = mspace->blocks;
  for(unsigned int i=0; i < mspace->num_used_blocks; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    reloc_table = block->reloc_table;
    for(unsigned int j=0; j < reloc_table->size(); j++){
      Partial_Reveal_Object** p_ref = (*reloc_table)[j];
      Partial_Reveal_Object* p_target_obj = get_forwarding_pointer_in_obj_info(*p_ref);
      *p_ref = p_target_obj;
    }
    reloc_table->clear();
  }
  
  return;  
}  

Boolean mspace_mark_object(Mspace* mspace, Partial_Reveal_Object *p_obj)
{  
  obj_mark_in_vt(p_obj);

  unsigned int obj_word_index = OBJECT_WORD_INDEX_TO_MARKBIT_TABLE(p_obj);
  unsigned int obj_offset_in_word = OBJECT_WORD_OFFSET_IN_MARKBIT_TABLE(p_obj); 	
	
  unsigned int *p_word = &(GC_BLOCK_HEADER(p_obj)->mark_table[obj_word_index]);
  unsigned int word_mask = (1<<obj_offset_in_word);
	
  unsigned int result = (*p_word)|word_mask;
	
  if( result==(*p_word) ) return FALSE;
  
  *p_word = result; 
  
   return TRUE;
}

Boolean mspace_object_is_marked(Partial_Reveal_Object *p_obj, Mspace* mspace)
{
  assert(p_obj);
  
#ifdef _DEBUG //TODO:: Cleanup
  unsigned int obj_word_index = OBJECT_WORD_INDEX_TO_MARKBIT_TABLE(p_obj);
  unsigned int obj_offset_in_word = OBJECT_WORD_OFFSET_IN_MARKBIT_TABLE(p_obj); 	
	
  unsigned int *p_word = &(GC_BLOCK_HEADER(p_obj)->mark_table[obj_word_index]);
  unsigned int word_mask = (1<<obj_offset_in_word);
	
  unsigned int result = (*p_word)|word_mask;
	
  if( result==(*p_word) )
    assert( obj_is_marked_in_vt(p_obj));
  else 
    assert(!obj_is_marked_in_vt(p_obj));
    
#endif

  return (obj_is_marked_in_vt(p_obj));
    
}

static Boolean mspace_compute_object_target(Mspace* mspace)
{  
  Block_Header* dest_block = mspace_get_first_target_block(mspace);    
  Block_Header* curr_block = mspace_get_first_compact_block(mspace);

  void* dest_addr = GC_BLOCK_BODY(dest_block);
 
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
            
      unsigned int obj_size = vm_object_size(p_obj);
      
      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
        dest_block->free = dest_addr;
        dest_block = mspace_get_next_target_block(mspace, dest_block);
        dest_addr = GC_BLOCK_BODY(dest_block);
      }
      assert(((unsigned int)dest_addr + obj_size) <= (unsigned int)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);
      if( obj_info != 0 ) {
        mspace->obj_info_map->insert(ObjectMap::value_type((Partial_Reveal_Object*)dest_addr, obj_info));
      }
      
      assert( (unsigned int) p_obj >= (unsigned int)dest_addr );
      set_forwarding_pointer_in_obj_info(p_obj, dest_addr);

      /* FIXME: should use alloc to handle alignment requirement */
      dest_addr = (void *) WORD_SIZE_ROUND_UP((unsigned int) dest_addr + obj_size);
      p_obj = block_get_next_marked_object(curr_block, &mark_bit_idx);
  
    }
    curr_block = mspace_get_next_compact_block(mspace, curr_block);
  }


  mspace->free_block_idx = dest_block->block_idx+1;

  /* fail to evacuate any room, FIXME:: do nothing at the moment */
  if( mspace->free_block_idx == mspace->first_block_idx + mspace->num_used_blocks) 
    return FALSE;
  
  return TRUE;
}   

static void mspace_restore_obj_info(Mspace* mspace)
{
  ObjectMap* objmap = mspace->obj_info_map;
  ObjectMap::iterator obj_iter;
  for( obj_iter=objmap->begin(); obj_iter!=objmap->end(); obj_iter++){
    Partial_Reveal_Object* p_target_obj = obj_iter->first;
    Obj_Info_Type obj_info = obj_iter->second;
    set_obj_info(p_target_obj, obj_info);     
  }
  objmap->clear();
  return;  
}
  
static void reset_mspace_after_compaction(Mspace* mspace)
{ 
  unsigned int old_num_used = mspace->num_used_blocks;
  unsigned int new_num_used = mspace->free_block_idx - mspace->first_block_idx;
  unsigned int num_used = old_num_used>new_num_used? old_num_used:new_num_used;
  
  Block* blocks = mspace->blocks;
  for(unsigned int i=0; i < num_used; i++){
    Block_Header* block = (Block_Header*)&(blocks[i]);
    block_clear_mark_table(block); 
    block->status = BLOCK_USED;

    if(i >= new_num_used){
      block->status = BLOCK_FREE; 
      block->free = GC_BLOCK_BODY(block);
    }
  }
  mspace->num_used_blocks = new_num_used;
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
        
    curr_block = mspace_get_next_compact_block(mspace, curr_block);
  }

  mspace_restore_obj_info(mspace);
  reset_mspace_after_compaction(mspace);
  
  return;
} 

void gc_gen_update_repointed_refs(Collector* collector);

static void mark_compact_mspace(Collector* collector) 
{
  GC_Gen* gc = (GC_Gen*)collector->gc;
  Mspace* mspace = (Mspace*)gc_get_mos(gc);
  Fspace* fspace = (Fspace*)gc_get_nos(gc);

  /* FIXME:: Single-threaded mark-compaction for mspace currently */

  /* Pass 1: mark all live objects in heap, and save all the slots that 
             have references  that are going to be repointed */
  mark_scan_heap(collector);
  
  /* Pass 2: assign target addresses for all to-be-moved objects */
  Boolean ok;
  ok = mspace_compute_object_target(mspace); 
  assert(ok); /* free at least one block */
  ok = fspace_compute_object_target(collector, fspace); 
  assert(ok); /* FIXME:: throw out-of-memory exception if not ok */
  
  /* Pass 3: update all references whose objects are to be moved */  
  gc_gen_update_repointed_refs(collector);
    
  /* Pass 4: do the compaction and reset blocks */  
  mspace_sliding_compact(collector, mspace);
  fspace_copy_collect(collector, fspace);
     
  return;
}

void mspace_collection(Mspace* mspace) 
{
  mspace->num_collections++;

  GC* gc = mspace->gc;  

  collector_execute_task(gc, (TaskType)mark_compact_mspace, (Space*)mspace);
  
  return;  
} 
