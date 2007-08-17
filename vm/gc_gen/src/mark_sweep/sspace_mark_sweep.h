/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _SSPACE_MARK_SWEEP_H_
#define _SSPACE_MARK_SWEEP_H_

#include "sspace_chunk.h"
#include "sspace_verify.h"

#define PFC_REUSABLE_RATIO 0.1
#define SSPACE_COMPACT_RATIO 0.15

inline Boolean chunk_is_reusable(Chunk_Header *chunk)
{ return (float)(chunk->slot_num-chunk->alloc_num)/chunk->slot_num > PFC_REUSABLE_RATIO; }

enum Obj_Color {
  OBJ_COLOR_BLUE = 0x0,
  OBJ_COLOR_WHITE = 0x1,
  OBJ_COLOR_BLACK = 0x2,
  OBJ_COLOR_GRAY = 0x3,
  OBJ_COLOR_MASK = 0x3
};

#ifdef POINTER64
  #define BLACK_MASK_IN_TABLE  ((POINTER_SIZE_INT)0xAAAAAAAAAAAAAAAA)
#else
  #define BLACK_MASK_IN_TABLE  ((POINTER_SIZE_INT)0xAAAAAAAA)
#endif

extern POINTER_SIZE_INT cur_alloc_color;
extern POINTER_SIZE_INT cur_mark_color;
extern POINTER_SIZE_INT cur_alloc_mask;
extern POINTER_SIZE_INT cur_mark_mask;

inline Boolean is_super_obj(Partial_Reveal_Object *obj)
{
  //return get_obj_info_raw(obj) & SUPER_OBJ_MASK;/*
  if(vm_object_size(obj) > SUPER_OBJ_THRESHOLD){
    return TRUE;
  } else {
    return FALSE;
  }
}

inline POINTER_SIZE_INT *get_color_word_in_table(Partial_Reveal_Object *obj, unsigned int &index_in_word)
{
  Chunk_Header *chunk;
  unsigned int index;
  
  if(is_super_obj(obj)){
    chunk = ABNORMAL_CHUNK_HEADER(obj);
    index = 0;
  } else {
    chunk = NORMAL_CHUNK_HEADER(obj);
    index = slot_addr_to_index(chunk, obj);
  }
  unsigned int word_index = index / SLOT_NUM_PER_WORD_IN_TABLE;
  index_in_word = COLOR_BITS_PER_OBJ * (index % SLOT_NUM_PER_WORD_IN_TABLE);
  
  return &chunk->table[word_index];
}

/* Accurate marking: TRUE stands for being marked by this collector, and FALSE for another collector */
inline Boolean obj_mark_in_table(Partial_Reveal_Object *obj)
{
  volatile POINTER_SIZE_INT *p_color_word;
  unsigned int index_in_word;
  p_color_word = get_color_word_in_table(obj, index_in_word);
  assert(p_color_word);
  
  POINTER_SIZE_INT color_bits_mask = ~(OBJ_COLOR_MASK << index_in_word);
  POINTER_SIZE_INT mark_color = cur_mark_color << index_in_word;
  
  POINTER_SIZE_INT old_word = *p_color_word;
  POINTER_SIZE_INT new_word = (old_word & color_bits_mask) | mark_color;
  while(new_word != old_word) {
    POINTER_SIZE_INT temp = (POINTER_SIZE_INT)atomic_casptr((volatile void**)p_color_word, (void*)new_word, (void*)old_word);
    if(temp == old_word){
#ifdef SSPACE_VERIFY
#ifndef SSPACE_VERIFY_FINREF
      assert(obj_is_marked_in_vt(obj));
#endif
      obj_unmark_in_vt(obj);
      sspace_record_mark(obj, vm_object_size(obj));
#endif
      return TRUE;
    }
    old_word = *p_color_word;
    new_word = (old_word & color_bits_mask) | mark_color;
  }
  
  return FALSE;
}

inline void collector_add_free_chunk(Collector *collector, Free_Chunk *chunk)
{
  Free_Chunk_List *list = collector->free_chunk_list;
  
  chunk->status = CHUNK_FREE | CHUNK_TO_MERGE;
  chunk->next = list->head;
  chunk->prev = NULL;
  if(list->head)
    list->head->prev = chunk;
  else
    list->tail = chunk;
  list->head = chunk;
}


extern void sspace_mark_scan(Collector *collector);
extern void gc_init_chunk_for_sweep(GC *gc, Sspace *sspace);
extern void sspace_sweep(Collector *collector, Sspace *sspace);
extern void compact_sspace(Collector *collector, Sspace *sspace);
extern void gc_collect_free_chunks(GC *gc, Sspace *sspace);
extern Chunk_Header_Basic *sspace_grab_next_chunk(Sspace *sspace, Chunk_Header_Basic *volatile *shared_next_chunk, Boolean need_construct);

extern void pfc_set_slot_index(Chunk_Header *chunk, unsigned int first_free_word_index, POINTER_SIZE_INT alloc_color);
extern void pfc_reset_slot_index(Chunk_Header *chunk);

#endif // _SSPACE_MARK_SWEEP_H_
