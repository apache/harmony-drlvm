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

#ifndef _LSPACE_H_
#define _LSPACE_H_

#include "../common/gc_common.h"
#include "../thread/thread_alloc.h"

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
  void (*save_reloc_func)(Lspace* space, Partial_Reveal_Object** p_ref);
  void (*update_reloc_func)(Lspace* space);
  /* END of Space --> */

  void* alloc_free;
    
  unsigned int* mark_table;

  /* support other space moving collection */
  SlotVector* reloc_table;

}Lspace;

void lspace_initialize(GC* gc, void* reserved_base, unsigned int lspace_size);
void lspace_destruct(Lspace* lspace);
Managed_Object_Handle lspace_alloc(unsigned int size, Allocator* allocator);
void lspace_collection(Lspace* lspace);

inline unsigned int lspace_free_memory_size(Lspace* lspace){ /* FIXME:: */ return 0; }

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

Boolean lspace_mark_object(Lspace* lspace, Partial_Reveal_Object* p_obj);
void lspace_save_reloc(Lspace* space, Partial_Reveal_Object** p_ref);
void lspace_update_reloc(Lspace* lspace);

void reset_lspace_after_copy_nursery(Lspace* lspace);

#endif /*_LSPACE_H_ */
