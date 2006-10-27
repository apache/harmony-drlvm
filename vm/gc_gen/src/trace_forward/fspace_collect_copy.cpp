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

/* copying of fspace is only for MAJOR_COLLECTION or non-generational partial copy collection */
static Block_Header* mspace_get_first_target_block_for_nos(Mspace* mspace)
{  
  return (Block_Header*)&mspace->blocks[mspace->free_block_idx-mspace->first_block_idx];
}

static Block_Header* mspace_get_next_target_block_for_nos(Mspace* mspace, Block_Header* block)
{ return block->next; }

static void fspace_restore_obj_info(Fspace* fspace)
{
  ObjectMap* objmap = fspace->obj_info_map;
  ObjectMap::iterator obj_iter;
  for( obj_iter=objmap->begin(); obj_iter!=objmap->end(); obj_iter++){
    Partial_Reveal_Object* p_target_obj = obj_iter->first;
    Obj_Info_Type obj_info = obj_iter->second;
    set_obj_info(p_target_obj, obj_info);     
  }
  objmap->clear();
  return;  
}

struct GC_Gen;
Space* gc_get_mos(GC_Gen* gc);

Boolean fspace_compute_object_target(Collector* collector, Fspace* fspace)
{  
  Mspace* mspace = (Mspace*)gc_get_mos((GC_Gen*)collector->gc);
  Block_Header* dest_block = mspace_get_first_target_block_for_nos(mspace);    
  Block_Header* curr_block = fspace_get_first_copy_block(fspace);

  void* dest_addr = GC_BLOCK_BODY(dest_block);
 
  while( curr_block ){
    unsigned int mark_bit_idx;
    Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &mark_bit_idx);
    
    while( p_obj ){
      assert( obj_is_marked_in_vt(p_obj));
            
      unsigned int obj_size = vm_object_size(p_obj);
      
      if( ((unsigned int)dest_addr + obj_size) > (unsigned int)GC_BLOCK_END(dest_block)){
        dest_block->free = dest_addr;
        dest_block = mspace_get_next_target_block_for_nos(mspace, dest_block);
        if(dest_block == NULL) return FALSE;
        dest_addr = GC_BLOCK_BODY(dest_block);
      }
      assert(((unsigned int)dest_addr + obj_size) <= (unsigned int)GC_BLOCK_END(dest_block));
      
      Obj_Info_Type obj_info = get_obj_info(p_obj);
      if( obj_info != 0 ) {
        fspace->obj_info_map->insert(ObjectMap::value_type((Partial_Reveal_Object*)dest_addr, obj_info));
      }
      set_forwarding_pointer_in_obj_info(p_obj, dest_addr);

      /* FIXME: should use alloc to handle alignment requirement */
      dest_addr = (void *) WORD_SIZE_ROUND_UP((unsigned int) dest_addr + obj_size);
      p_obj = block_get_next_marked_object(curr_block, &mark_bit_idx);
  
    }
    curr_block = fspace_get_next_copy_block(fspace, curr_block);
  }
  
  mspace->free_block_idx = dest_block->block_idx+1;
  
  return TRUE;
}   

#include "../verify/verify_live_heap.h"

void fspace_copy_collect(Collector* collector, Fspace* fspace) 
{  
  Block_Header* curr_block = fspace_get_first_copy_block(fspace);
  
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
        
    curr_block = fspace_get_next_copy_block(fspace, curr_block);
  }
  
  fspace_restore_obj_info(fspace);
  reset_fspace_for_allocation(fspace);  
  
  return;
}

void gc_gen_update_repointed_refs(Collector* collector);

void mark_copy_fspace(Collector* collector) 
{  
  GC* gc = collector->gc;
  Fspace* fspace = (Fspace*)collector->collect_space;
  
  /* FIXME:: Single-threaded mark-copying for fspace currently */

  /* Pass 1: mark all live objects in heap, and save all the slots that 
             have references  that are going to be repointed */
  mark_scan_heap(collector);

  /* Pass 2: assign each live fspace object a new location */
  fspace_compute_object_target(collector, fspace);  

  gc_gen_update_repointed_refs(collector);

  /* FIXME:: Pass 2 and 3 can be merged into one pass */
  /* Pass 3: copy live fspace object to new location */
  fspace_copy_collect(collector, fspace);        
    
  return;
}
