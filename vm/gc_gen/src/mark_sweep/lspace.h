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

#ifndef _LSPACE_H_
#define _LSPACE_H_

#include "../common/gc_common.h"
#include "../thread/gc_thread.h"
#include "free_area_pool.h"

typedef struct Lspace{
  /* <-- first couple of fields are overloadded as Space */
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  GC* gc;
  Boolean move_object;
  Boolean (*mark_object_func)(Lspace* space, Partial_Reveal_Object* p_obj);
  /* END of Space --> */

//  void* alloc_free;
  Free_Area_Pool* free_pool;
  
  unsigned int* mark_table;

}Lspace;

void lspace_initialize(GC* gc, void* reserved_base, unsigned int lspace_size);
void lspace_destruct(Lspace* lspace);
Managed_Object_Handle lspace_alloc(unsigned int size, Allocator* allocator);
void lspace_sweep(Lspace* lspace);
void lspace_collection(Lspace* lspace);

inline unsigned int lspace_free_memory_size(Lspace* lspace){ /* FIXME:: */ return 0; }


#define LSPACE_SIZE_TO_MARKTABLE_SIZE_BITS(space_size) (((space_size) >> BIT_SHIFT_TO_KILO)+1)
#define LSPACE_SIZE_TO_MARKTABLE_SIZE_BYTES(space_size) ((LSPACE_SIZE_TO_MARKTABLE_SIZE_BITS(space_size)>> BIT_SHIFT_TO_BITS_PER_BYTE)+1) 
#define LSPACE_SIZE_TO_MARKTABLE_SIZE_WORDS(space_size) ((LSPACE_SIZE_TO_MARKTABLE_SIZE_BYTES(space_size)>> BIT_SHIFT_TO_BYTES_PER_WORD)+1) 

/* The assumption is the offset below is always aligned at word size, because both numbers are aligned */
#define ADDRESS_OFFSET_IN_LSPACE_BODY(lspace, p_obj) ((unsigned int)p_obj - (unsigned int)space_heap_start((Space*)lspace))
#define OBJECT_BIT_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj)    (ADDRESS_OFFSET_IN_LSPACE_BODY(lspace, p_obj) >> BIT_SHIFT_TO_KILO)
#define OBJECT_WORD_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj)   (OBJECT_BIT_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj) >> BIT_SHIFT_TO_BITS_PER_WORD)
#define OBJECT_WORD_OFFSET_IN_LSPACE_MARKBIT_TABLE(lspace, p_obj)  (OBJECT_BIT_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj) & BIT_MASK_TO_BITS_PER_WORD)

inline Boolean lspace_object_is_marked(Lspace* lspace, Partial_Reveal_Object* p_obj)
{
  assert( obj_belongs_to_space(p_obj, (Space*)lspace));
  unsigned int word_index = OBJECT_WORD_INDEX_TO_LSPACE_MARKBIT_TABLE(lspace, p_obj);
  unsigned int bit_offset_in_word = OBJECT_WORD_OFFSET_IN_LSPACE_MARKBIT_TABLE(lspace, p_obj);
 
  unsigned int markbits = lspace->mark_table[word_index];
  return markbits & (1<<bit_offset_in_word);
}


inline Partial_Reveal_Object* lspace_get_first_marked_object(Lspace* lspace, unsigned int* mark_bit_idx)
{
  unsigned int* mark_table = lspace->mark_table;
  unsigned int* table_end = mark_table + LSPACE_SIZE_TO_MARKTABLE_SIZE_WORDS(lspace->committed_heap_size);
  
  unsigned j=0;
  unsigned int k=0;
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    while(k<32){
        if( !(markbits& (1<<k)) ){ k++; continue;}
        unsigned int kilo_bytes_index = (j<<BIT_SHIFT_TO_BITS_PER_WORD) + k;
        Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)((char*)lspace->heap_start + kilo_bytes_index * KB);
        *mark_bit_idx = kilo_bytes_index;
        return p_obj;
    }
    j++;
    k=0;
  }          
  *mark_bit_idx = 0;
  return NULL;   
}


inline Partial_Reveal_Object* lspace_get_next_marked_object(Lspace* lspace, unsigned int* mark_bit_idx)
{
  unsigned int* mark_table = lspace->mark_table;
  unsigned int* table_end = mark_table + LSPACE_SIZE_TO_MARKTABLE_SIZE_WORDS(lspace->committed_heap_size);
  unsigned int bit_index = *mark_bit_idx;
  
  unsigned int j = bit_index >> BIT_SHIFT_TO_BITS_PER_WORD;
  unsigned int k = (bit_index & BIT_MASK_TO_BITS_PER_WORD) + 1;  
     
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    while(k<32){
      if( !(markbits& (1<<k)) ){ k++; continue;}
      
      unsigned int kilo_byte_index = (j<<BIT_SHIFT_TO_BITS_PER_WORD) + k;
      Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)((char*)lspace->heap_start + kilo_byte_index *  KB);      
      *mark_bit_idx = kilo_byte_index;
      return p_obj;
    }
    j++;
    k=0;
  }        
  
  *mark_bit_idx = 0;
  return NULL;   

}

Boolean lspace_mark_object(Lspace* lspace, Partial_Reveal_Object* p_obj);

void reset_lspace_after_copy_nursery(Lspace* lspace);

#endif /*_LSPACE_H_ */
