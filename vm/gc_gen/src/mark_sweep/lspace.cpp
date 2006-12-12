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
 * @author Ji Qi, 2006/10/05
 */

#include "lspace.h"

void* los_boundary = NULL;
struct GC_Gen;
void gc_set_los(GC_Gen* gc, Space* lspace);

void lspace_initialize(GC* gc, void* start, unsigned int lspace_size)
{
  Lspace* lspace = (Lspace*)STD_MALLOC( sizeof(Lspace));
  assert(lspace);
  memset(lspace, 0, sizeof(Lspace));

  void* reserved_base = start;
  unsigned int committed_size = lspace_size;
  int status = port_vmem_commit(&reserved_base, committed_size, gc->allocated_memory); 
  assert(status == APR_SUCCESS && reserved_base == start);

  memset(reserved_base, 0, committed_size);
  lspace->committed_heap_size = committed_size;
  lspace->reserved_heap_size = lspace_size - committed_size;
  lspace->heap_start = reserved_base;
  lspace->heap_end = (void *)((unsigned int)reserved_base + committed_size);

  /*Treat with mark bit table*/
  unsigned int num_words = LSPACE_SIZE_TO_MARKTABLE_SIZE_WORDS(lspace_size);
  lspace->mark_table = (unsigned int*)STD_MALLOC( num_words*BYTES_PER_WORD );
  memset(lspace->mark_table, 0, num_words*BYTES_PER_WORD);
  lspace->mark_object_func = lspace_mark_object;
  lspace->move_object = FALSE;
  lspace->gc = gc;

  /*Treat with free area buddies*/
  lspace->free_pool = (Free_Area_Pool*)STD_MALLOC(sizeof(Free_Area_Pool));
  free_area_pool_init(lspace->free_pool);
  Free_Area* initial_fa = (Free_Area*)lspace->heap_start;
  initial_fa->size = lspace->committed_heap_size;
  free_pool_add_area(lspace->free_pool, initial_fa);

  gc_set_los((GC_Gen*)gc, (Space*)lspace);
  los_boundary = lspace->heap_end;

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

Boolean lspace_mark_object(Lspace* lspace, Partial_Reveal_Object* p_obj)
{
  assert( obj_belongs_to_space(p_obj, (Space*)lspace));
  unsigned int word_index = OBJECT_WORD_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj);
  unsigned int bit_offset_in_word = OBJECT_WORD_OFFSET_IN_LSPACE_MARKBIT_TABLE(lspace, p_obj);

  unsigned int* p_word = &(lspace->mark_table[word_index]);
  unsigned int word_mask = (1<<bit_offset_in_word);

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

void reset_lspace_after_copy_nursery(Lspace* lspace)
{
  unsigned int marktable_size = LSPACE_SIZE_TO_MARKTABLE_SIZE_BYTES(lspace->committed_heap_size);
  memset(lspace->mark_table, 0, marktable_size); 
  return;  
}

void lspace_collection(Lspace* lspace)
{
  /* heap is marked already, we need only sweep here. */
  lspace_sweep(lspace);
  unsigned int marktable_size = LSPACE_SIZE_TO_MARKTABLE_SIZE_BYTES(lspace->committed_heap_size);
  memset(lspace->mark_table, 0, marktable_size); 
  return;
}
