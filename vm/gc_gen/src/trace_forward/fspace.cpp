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

#include "platform.h"

#include "fspace.h"

float NURSERY_OBJECT_FORWARDING_RATIO = FORWARD_ALL;
//float NURSERY_OBJECT_FORWARDING_RATIO = FORWARD_HALF;

void* nos_boundary = null; /* this is only for speeding up write barrier */

Boolean forward_first_half;;
void* object_forwarding_boundary=NULL;


void fspace_destruct(Fspace *fspace) 
{
  port_vmem_decommit(fspace->heap_start, fspace->committed_heap_size, fspace->gc->allocated_memory);
  STD_FREE(fspace->mark_table);
  STD_FREE(fspace);  
 
}

struct GC_Gen;
void gc_set_nos(GC_Gen* gc, Space* space);
void fspace_initialize(GC* gc, void* start, unsigned int fspace_size) 
{    
  assert( (fspace_size%GC_BLOCK_SIZE_BYTES) == 0 );
  Fspace* fspace = (Fspace *)STD_MALLOC(sizeof(Fspace));
  assert(fspace);
  memset(fspace, 0, sizeof(Fspace));

  void* reserved_base = start;
  int status = port_vmem_commit(&reserved_base, fspace_size, gc->allocated_memory); 
  assert(status == APR_SUCCESS && reserved_base == start);
  
  memset(reserved_base, 0, fspace_size);

  fspace->block_size_bytes = GC_BLOCK_SIZE_BYTES;
  fspace->committed_heap_size = fspace->reserved_heap_size = fspace_size;
  
  fspace->heap_start = (void *)start;
  fspace->heap_end = (void *)((POINTER_SIZE_INT) start + fspace_size);

  fspace->num_collections = 0;
    
  fspace->alloc_free = fspace->heap_start;
  fspace->alloc_ceiling = fspace->heap_end;
  forward_first_half = TRUE;
  object_forwarding_boundary = (void *)((((POINTER_SIZE_INT)start + (unsigned int)(fspace_size*NURSERY_OBJECT_FORWARDING_RATIO))>>7)<<7);

  /* mark_table: 1. MAJOR_COLLECTION mark and copy nursery, 2. MINOR_COLLECTION mark non-forwarded part */
  fspace->mark_table_size = fspace_size>>5;
  fspace->mark_table = (unsigned int*)STD_MALLOC(fspace->mark_table_size); 
  fspace->mark_table_half_boundary = fspace->mark_table + (((unsigned int)(fspace_size*NURSERY_OBJECT_FORWARDING_RATIO))>>7);
  memset(fspace->mark_table, 0, fspace->mark_table_size);

  fspace->mark_object_func = fspace_mark_object;
  fspace->save_reloc_func = fspace_save_reloc;
  fspace->update_reloc_func = fspace_update_reloc;

  fspace->remslot_sets = new std::vector<RemslotSet *>();
  fspace->remobj_sets = new std::vector<RemobjSet *>();
  fspace->rem_sets_lock = FREE_LOCK;

  fspace->reloc_table = new SlotVector();
  fspace->obj_info_map = new ObjectMap();

  fspace->move_object = TRUE;
  fspace->gc = gc;
  gc_set_nos((GC_Gen*)gc, (Space*)fspace);

  nos_boundary = fspace->heap_end;

  return;
}

Boolean fspace_mark_object(Fspace* fspace, Partial_Reveal_Object *p_obj)
{
  assert(obj_belongs_to_space(p_obj, (Space*)fspace));
  
  //offset in byte
  unsigned int offset = (POINTER_SIZE_INT)p_obj - (POINTER_SIZE_INT)fspace->heap_start; 
  assert( ((offset>>2)<<2)==offset);
  
  offset >>= 2; //offset in word
  unsigned int bit_off_in_word = offset - ((offset>>5)<<5);
  unsigned int* pos = fspace->mark_table + (offset>>5);

  if( NURSERY_OBJECT_FORWARDING_RATIO != FORWARD_ALL && fspace->gc->collect_kind != MAJOR_COLLECTION )
    assert(forward_first_half?(pos >= fspace->mark_table_half_boundary):(pos<fspace->mark_table_half_boundary));
  
  unsigned int word = *pos;
  unsigned int result = word | (1<<bit_off_in_word);
  if(word == result) return FALSE;

  *pos = result;
  obj_mark_in_vt(p_obj);
  
  return TRUE;
}

void fspace_clear_mark_bits(Fspace* fspace)
{
  unsigned int* mark_table = fspace->mark_table;
  unsigned int* fspace_marked_half = (unsigned int*)fspace->heap_start;
  unsigned int* table_end = fspace->mark_table_half_boundary;
  Partial_Reveal_Object* p_obj = NULL;
  
  if( forward_first_half ){
    //forward frist half, so the second half is marked
    table_end = (unsigned int*)((POINTER_SIZE_INT)mark_table + fspace->mark_table_size);
    mark_table = fspace->mark_table_half_boundary; 
    fspace_marked_half = (unsigned int*)object_forwarding_boundary;
  }
  
  unsigned j=0;
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    unsigned int k=0;
    while(k<32){
      if( !(markbits& (1<<k)) ){ k++; continue;}
      p_obj = (Partial_Reveal_Object*)(fspace_marked_half + (j<<5) + k);
      
      Partial_Reveal_VTable* vt = obj_get_vtraw(p_obj);
      Allocation_Handle ah = (Allocation_Handle)vt;
      assert(!(ah & FORWARDING_BIT_MASK));
      assert(ah & MARK_BIT_MASK);
      
      obj_unmark_in_vt(p_obj);
      //*(mark_table+j) &= ~(1<<k); //we do it in chunck
      
      k++;
    }   
    j++;;
  }

  memset(mark_table, 0, (POINTER_SIZE_INT)table_end-(POINTER_SIZE_INT)mark_table);
}
 
void reset_fspace_for_allocation(Fspace* fspace)
{ 
  if( NURSERY_OBJECT_FORWARDING_RATIO == FORWARD_ALL ||
            fspace->gc->collect_kind == MAJOR_COLLECTION ){
    fspace->alloc_free = fspace->heap_start;
    fspace->alloc_ceiling = fspace->heap_end;    
    forward_first_half = TRUE; /* only useful for not-FORWARD_ALL*/
  }else{    
    if(forward_first_half){
      fspace->alloc_free = fspace->heap_start;
      fspace->alloc_ceiling = object_forwarding_boundary;
    }else{
      fspace->alloc_free = object_forwarding_boundary;
      fspace->alloc_ceiling = fspace->heap_end;
    }
    forward_first_half = ~forward_first_half;
    /* clear all the marking bits of the remaining objects in fspace */
    fspace_clear_mark_bits(fspace); 
  }
}
