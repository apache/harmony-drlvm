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

#include "lspace.h"

void* los_boundary = NULL;
struct GC_Gen;
Space* gc_get_los(GC_Gen* gc);
void gc_set_los(GC_Gen* gc, Space* lspace);
void* lspace_alloc(unsigned int size, Allocator *allocator)
{  
  Lspace* lspace = (Lspace*)gc_get_los((GC_Gen*)allocator->gc);
  
  unsigned int old_free = (unsigned int)lspace->alloc_free;
  unsigned int new_free = old_free + size;

  while(new_free <= (unsigned int)lspace->heap_end){
    unsigned int temp = atomic_cas32((volatile unsigned int *)&lspace->alloc_free, new_free, old_free);
    if (temp != old_free) {
      old_free = (unsigned int)lspace->alloc_free;
      new_free = old_free + size;
      continue;
    }
    /* successfully allocate an object */
    Partial_Reveal_Object* p_return_object = (Partial_Reveal_Object*)old_free;
    lspace->alloc_free = (void*)new_free;

    /* TODO: should moved to better location */
    memset(p_return_object, 0, size);
    
    return (void*)old_free;
  }

  /* FIXME:: trigger collection */
  assert(0);
  return NULL;

}

void lspace_initialize(GC* gc, void* start, unsigned int lspace_size)
{
  Lspace* lspace = (Lspace*)STD_MALLOC( sizeof(Lspace));
  assert(lspace);
  memset(lspace, 0, sizeof(Lspace));
  
  void* reserved_base = start;
  unsigned int committed_size = lspace_size >> 1;
  int status = port_vmem_commit(&reserved_base, committed_size, gc->allocated_memory); 
  assert(status == APR_SUCCESS && reserved_base == start);
  
  memset(reserved_base, 0, committed_size);
  lspace->committed_heap_size = committed_size;
  lspace->reserved_heap_size = lspace_size - committed_size;
  lspace->heap_start = reserved_base;
  lspace->heap_end = (void *)((unsigned int)reserved_base + committed_size);
  lspace->alloc_free = reserved_base;
  
  unsigned int num_bits = lspace_size >> BIT_SHIFT_TO_KILO;
  unsigned int num_words = (num_bits >> BIT_SHIFT_TO_BITS_PER_WORD)+1;
  lspace->mark_table = (unsigned int*)STD_MALLOC( num_words*BYTES_PER_WORD );
  memset(lspace->mark_table, 0, num_words*BYTES_PER_WORD);
  
  lspace->reloc_table = new SlotVector();
  lspace->mark_object_func = lspace_mark_object;
  lspace->save_reloc_func = lspace_save_reloc;
  lspace->update_reloc_func = lspace_update_reloc;
  
  lspace->move_object = FALSE;
  lspace->gc = gc;
  gc_set_los((GC_Gen*)gc, (Space*)lspace);
  
  los_boundary = start;
  
  return;
}

void lspace_destruct(Lspace* lspace)
{
  //FIXME:: decommit lspace space
  STD_FREE(lspace->mark_table);
  STD_FREE(lspace);
  lspace = NULL;
  return;
}

void lspace_collection(Lspace* lspace)
{
  /* FIXME:: collection */
  unsigned int used_size = (unsigned int)lspace->alloc_free - (unsigned int)lspace->heap_start;
  memset(lspace->mark_table, 0, (used_size>>BIT_SHIFT_TO_KILO)>>BIT_SHIFT_TO_BITS_PER_BYTE );
  
  return;
}

void reset_lspace_after_copy_nursery(Lspace* lspace)
{
  unsigned int used_size = (unsigned int)lspace->alloc_free - (unsigned int)lspace->heap_start;
  memset(lspace->mark_table, 0, (used_size>>BIT_SHIFT_TO_KILO)>>BIT_SHIFT_TO_BITS_PER_BYTE );
  return;  
}


Boolean lspace_mark_object(Lspace* lspace, Partial_Reveal_Object* p_obj)
{
  assert( obj_belongs_to_space(p_obj, (Space*)lspace));
  unsigned int word_index = OBJECT_WORD_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj);
  unsigned int bit_offset_in_word = OBJECT_WORD_OFFSET_IN_LSPACE_MARKBIT_TABLE(lspace, p_obj);
 
  unsigned int* p_markbits = &(lspace->mark_table[word_index]);
  unsigned int word_mask = (1<<bit_offset_in_word);
	
  unsigned int result = (*p_markbits)|word_mask;
	
  if( result==(*p_markbits) ) return FALSE;
  
  *p_markbits = result; 
      
  return TRUE;
}

void lspace_save_reloc(Lspace* lspace, Partial_Reveal_Object** p_ref)
{
  lspace->reloc_table->push_back(p_ref);
}

void lspace_update_reloc(Lspace* lspace)
{
  SlotVector* reloc_table;
  
  reloc_table = lspace->reloc_table;
  for(unsigned int j=0; j < reloc_table->size(); j++){
    Partial_Reveal_Object** p_ref = (*reloc_table)[j];
    Partial_Reveal_Object* p_target_obj = get_forwarding_pointer_in_obj_info(*p_ref);
    *p_ref = p_target_obj;
  }
  reloc_table->clear();
  return;
}
