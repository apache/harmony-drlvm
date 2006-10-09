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

/* copying of fspace is only for MAJOR_COLLECTION */

static void slot_update_to_forwarding_pointer(Partial_Reveal_Object **p_ref, Partial_Reveal_Object *p_obj)
{
  /* if obj is marked in MAJOR_COLLECTION, it is alive, and has forwarding pointer */
  if(!obj_is_marked_in_vt(p_obj)) return;
    
  *p_ref = get_forwarding_pointer_in_obj_info(p_obj);  
}

static void object_update_slots(Fspace* space, Partial_Reveal_Object *p_obj) 
{
  if(obj_belongs_to_space(p_obj, (Space*)space)) return;
  
  if(!object_has_slots(p_obj)) return;

  if(object_is_array(p_obj)){
    assert(!obj_is_primitive_array(p_obj));

    int32 array_length = vector_get_length((Vector_Handle) p_obj);
    for (int i = 0; i < array_length; i++) {
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)vector_get_element_address_ref((Vector_Handle) p_obj, i);
      Partial_Reveal_Object* p_obj = *p_ref;
      if(p_obj == NULL) continue;
      if (obj_belongs_to_space(p_obj, (Space*)space)){
        slot_update_to_forwarding_pointer(p_ref, p_obj);
      }
    }
  }else{
    int *offset_scanner = init_object_scanner(p_obj);
    while (true) {
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)offset_get_ref(offset_scanner, p_obj);
      if (p_ref == NULL) break;
      Partial_Reveal_Object* p_obj = *p_ref;
      if(p_obj != NULL && obj_belongs_to_space(p_obj, (Space*)space)){
        slot_update_to_forwarding_pointer(p_ref, p_obj);
      }
      offset_scanner = offset_next_ref(offset_scanner);
    }
  }
}

void space_update_remsets(Fspace* space)
{
  
  for(unsigned int i=0; i< space->remslot_sets->size(); i++) {
    RemslotSet* remslot = (*space->remslot_sets)[i];
    for (unsigned int j = 0; j < remslot->size(); j++) {
      Partial_Reveal_Object** p_ref = (*remslot)[j];
      assert(p_ref);
      Partial_Reveal_Object* p_obj = *p_ref;
      if(p_obj == NULL) continue;  
      if (obj_belongs_to_space(p_obj, (Space*)space)){
        /* if obj is marked in MAJOR_COLLECTION, it is alive, and has forwarding pointer */
        if(obj_is_marked_in_vt(p_obj)){
          *p_ref = get_forwarding_pointer_in_obj_info(p_obj);
        }
      }
    }
    remslot->clear();
  }
  space->remslot_sets->clear();

  for(unsigned int i=0; i<space->remobj_sets->size(); i++) {
    RemobjSet *remobj = (*space->remobj_sets)[i];
    for(unsigned int j=0; j<remobj->size(); j++) {
      Partial_Reveal_Object *obj = (*remobj)[j];
      object_update_slots(space, obj);
    }
    remobj->clear();
  }
  space->remobj_sets->clear();

  return;

}

void fspace_save_reloc(Fspace* fspace, Partial_Reveal_Object** p_ref)
{
    fspace->reloc_table->push_back(p_ref);  
}

void fspace_update_reloc(Fspace* fspace)
{
  SlotVector* reloc_table;
  
  reloc_table = fspace->reloc_table;
  for(unsigned int j=0; j < reloc_table->size(); j++){
    Partial_Reveal_Object** p_ref = (*reloc_table)[j];
    Partial_Reveal_Object* p_target_obj = get_forwarding_pointer_in_obj_info(*p_ref);
    *p_ref = p_target_obj;
  }
  reloc_table->clear();
  return;
}

void fspace_restore_obj_info(Fspace* fspace)
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

#include "../verify/verify_live_heap.h"

void fspace_copy_collect(Collector* collector, Fspace* fspace) 
{  
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
      assert(obj_is_marked_in_vt(p_obj));
      obj_unmark_in_vt(p_obj);
      Partial_Reveal_Object* p_target_obj = get_forwarding_pointer_in_obj_info(p_obj);
      unsigned int obj_size = vm_object_size(p_obj);
      memmove(p_target_obj, p_obj, obj_size);

      if (verify_live_heap){
          /* we forwarded it, we need remember it for verification */
          event_collector_move_obj(p_obj, p_target_obj, collector);
      }
     
      set_obj_info(p_target_obj, 0);
 
      k++;
    }   
    j++;;
  }

  /* different from partial trace_forwarding, we clear the mark bits 
    (both in vt and table) in this function */
  memset(mark_table, 0, fspace->mark_table_size);  
    
  reset_fspace_for_allocation(fspace);  

  return;
}
