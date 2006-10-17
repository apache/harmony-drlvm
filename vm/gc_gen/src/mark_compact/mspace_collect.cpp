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
#include "../thread/collector.h"
#include "../trace_forward/fspace.h"
#include "../common/interior_pointer.h"
struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);

void mspace_save_reloc(Mspace* mspace, Partial_Reveal_Object** p_ref)
{
  Block_Info* block_info = GC_BLOCK_INFO_ADDRESS(mspace, p_ref);
  block_info->reloc_table->push_back(p_ref);
  return;
}

void  mspace_update_reloc(Mspace* mspace)
{
  SlotVector* reloc_table;
  /* update refs in mspace */
  for(unsigned int i=0; i < mspace->num_used_blocks; i++){
    Block_Info* block_info = &(mspace->block_info[i]);
    reloc_table = block_info->reloc_table;
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
  unsigned int target_blk_idx;
  Block_Header* dest_block = mspace_get_first_target_block(mspace, &target_blk_idx);    

  unsigned int compact_blk_idx;
  Block_Header* curr_block = mspace_get_first_compact_block(mspace, &compact_blk_idx);

  void* dest_addr = GC_BLOCK_BODY(dest_block);
 
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
            
      unsigned int obj_size = vm_object_size(p_obj);
      
      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
        dest_block->free = dest_addr;
        dest_block = mspace_get_next_target_block(mspace, &target_blk_idx);
        dest_addr = GC_BLOCK_BODY(dest_block);
      }
      assert(((unsigned int)dest_addr + obj_size) <= (unsigned int)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);
      if( obj_info != 0 ) {
        mspace->obj_info_map->insert(ObjectMap::value_type((Partial_Reveal_Object*)dest_addr, obj_info));
      }
      
      //FIXME: should use alloc to handle alignment requirement
      set_forwarding_pointer_in_obj_info(p_obj, dest_addr);

      dest_addr = (void *) WORD_SIZE_ROUND_UP((unsigned int) dest_addr + obj_size);
      p_obj = block_get_next_marked_object(curr_block, &mark_bit_idx);
  
    }
    curr_block = mspace_get_next_compact_block(mspace, &compact_blk_idx);
  }


  mspace->free_block_idx = dest_block->block_idx+1;

  /* fail to evacuate any room, FIXME:: do nothing at the moment */
  if( mspace->free_block_idx == mspace->num_current_blocks) 
    return FALSE;
  
  return TRUE;
}   

static Boolean fspace_compute_object_target(Collector* collector, Fspace* fspace)
{  
  Mspace* mspace = (Mspace*)collector->collect_space;
  unsigned int target_blk_idx;
  Block_Header* dest_block = mspace_get_first_block_for_nos(mspace, &target_blk_idx);    
  void* dest_addr = GC_BLOCK_BODY(dest_block);

  unsigned int* mark_table = fspace->mark_table;
  unsigned int* table_end = (unsigned int*)((POINTER_SIZE_INT)mark_table + fspace->mark_table_size);
  unsigned int* fspace_start = (unsigned int*)fspace->heap_start;
  Partial_Reveal_Object* p_obj = NULL;
    
  unsigned j=0;
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    unsigned int k=0;
    while(k<32){
      if( !(markbits& (1<<k)) ){ k++; continue;}
      p_obj = (Partial_Reveal_Object*)(fspace_start + (j<<5) + k);
      assert( obj_is_marked_in_vt(p_obj));
      
      unsigned int obj_size = vm_object_size(p_obj);

      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
        dest_block->free = dest_addr;
        dest_block = mspace_get_next_block_for_nos(mspace, &target_blk_idx);
        
        if(dest_block == NULL) return FALSE;
        dest_addr = GC_BLOCK_BODY(dest_block);
      }
      assert(((unsigned int)dest_addr + obj_size) <= (unsigned int)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);
      if( obj_info != 0 ) {
        fspace->obj_info_map->insert(ObjectMap::value_type((Partial_Reveal_Object*)dest_addr, obj_info));
      }
      
      //FIXME: should use alloc to handle alignment requirement
      set_forwarding_pointer_in_obj_info(p_obj, dest_addr);

      dest_addr = (void *) WORD_SIZE_ROUND_UP((unsigned int) dest_addr + obj_size);
      
      k++;
    }   
    j++;;
  }

  mspace->free_block_idx = dest_block->block_idx+1;
  
  return TRUE;
}   

static void update_relocated_refs(Collector* collector)
{
  GC_Gen* gc = (GC_Gen*)collector->gc;
  Space* space;
  space = gc_get_nos(gc);  space->update_reloc_func(space);
  space = gc_get_mos(gc);  space->update_reloc_func(space);
  space = gc_get_los(gc);  space->update_reloc_func(space);

  gc_update_rootset((GC*)gc);  

  space_update_remsets((Fspace*)gc_get_nos(gc));
  
  /* FIXME:: interior table */
  update_rootset_interior_pointer();
  return;
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

static void restore_saved_obj_info(Collector* collector)
{
  GC_Gen* gc = (GC_Gen*)collector->gc;
  Space* space;
  space = gc_get_nos(gc);  fspace_restore_obj_info((Fspace*)space);
  space = gc_get_mos(gc);  mspace_restore_obj_info((Mspace*)space);

  return;
}   

static void reset_mspace_for_allocation(Mspace* mspace)
{ 
  unsigned int num_used = mspace->num_used_blocks;
  unsigned int num_free = mspace->free_block_idx;
  unsigned int index = num_used>num_free? num_used:num_free;
  
  for(unsigned int i=0; i < index; i++){
    Block_Info* block_info = &(mspace->block_info[i]);
    block_clear_markbits(block_info->block); /* only needed for i<num_used_blocks */
    block_info->status = BLOCK_USED;

    if(i >= mspace->free_block_idx){
      block_info->status = BLOCK_FREE; 
      block_info->block->free = GC_BLOCK_BODY(block_info->block);
    }
  }
  mspace->num_used_blocks = mspace->free_block_idx;
}

#include "../verify/verify_live_heap.h"

static void mspace_sliding_compact(Collector* collector, Mspace* mspace)
{
  unsigned int compact_blk_idx;
  Block_Header* curr_block = mspace_get_first_compact_block(mspace, &compact_blk_idx);
  
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
        
    curr_block = mspace_get_next_compact_block(mspace, &compact_blk_idx);
  }

  reset_mspace_for_allocation(mspace);
  
  return;
} 

static void mark_compact_mspace(Collector* collector) 
{
  /* Pass 1: mark all live objects in heap, and save all the outgoing pointers */
  mark_scan_heap(collector);
  
  /* Pass 2: assign target addresses for all to-be-moved objects */
  Boolean ok;
  ok = mspace_compute_object_target((Mspace*)gc_get_mos((GC_Gen*)collector->gc)); 
  assert(ok); /* free at least one block */

  ok = fspace_compute_object_target(collector, (Fspace*)gc_get_nos((GC_Gen*)collector->gc)); 
  assert(ok); /* FIXME:: throw out-of-memory exception if not ok */
  
  /* Pass 3: update all references whose objects are to be moved */  
  update_relocated_refs(collector);
    
  /* Pass 4: do the compaction and reset blocks */  
  mspace_sliding_compact(collector, (Mspace*)collector->collect_space);
  GC_Gen* gc = (GC_Gen*)collector->gc;
  fspace_copy_collect(collector, (Fspace*)gc_get_nos(gc));
  
  restore_saved_obj_info(collector);
  
  return;
}

void mspace_collection(Mspace* mspace) 
{
  mspace->num_collections++;

  GC* gc = mspace->gc;  

  collector_execute_task(gc, (TaskType)mark_compact_mspace, (Space*)mspace);
  
  return;  
} 
