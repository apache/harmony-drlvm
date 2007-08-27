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

#include "sspace_chunk.h"
#include "sspace_mark_sweep.h"


static Chunk_Header_Basic *volatile next_chunk_for_sweep;


void gc_init_chunk_for_sweep(GC *gc, Sspace *sspace)
{
  next_chunk_for_sweep = (Chunk_Header_Basic*)space_heap_start((Space*)sspace);
  next_chunk_for_sweep->adj_prev = NULL;
  
  unsigned int i = gc->num_collectors;
  while(i--){
    Free_Chunk_List *list = gc->collectors[i]->free_chunk_list;
    assert(!list->head);
    assert(!list->tail);
    assert(list->lock == FREE_LOCK);
  }
}

static unsigned int word_set_bit_num(POINTER_SIZE_INT word)
{
  unsigned int count = 0;
  
  while(word){
    word &= word - 1;
    ++count;
  }
  return count;
}

void zeroing_free_chunk(Free_Chunk *chunk)
{
  assert(chunk->status == CHUNK_FREE);
  
  void *start = (void*)((POINTER_SIZE_INT)chunk + sizeof(Free_Chunk));
  POINTER_SIZE_INT size = CHUNK_SIZE(chunk) - sizeof(Free_Chunk);
  memset(start, 0, size);
}

/* Zeroing should be optimized to do it while sweeping index word */
static void zeroing_free_areas_in_pfc(Chunk_Header *chunk, unsigned int live_num)
{
  assert(live_num);
  
  assert(chunk->status & CHUNK_NORMAL);
  unsigned int slot_num = chunk->slot_num;
  unsigned int slot_size = chunk->slot_size;
  POINTER_SIZE_INT chunk_base = (POINTER_SIZE_INT)chunk->base;
  POINTER_SIZE_INT *table = chunk->table;
  
  POINTER_SIZE_INT base = (POINTER_SIZE_INT)NULL;
  assert(slot_num >= live_num);
  unsigned int free_slot_num = slot_num - live_num;
  unsigned int cur_free_slot_num = 0;
  unsigned int slot_index = chunk->slot_index;
  unsigned int word_index = slot_index / SLOT_NUM_PER_WORD_IN_TABLE;
  assert(live_num >= slot_index);
  live_num -= slot_index;
  POINTER_SIZE_INT index_word = table[word_index];
  POINTER_SIZE_INT mark_color = cur_mark_black_color << (COLOR_BITS_PER_OBJ * (slot_index % SLOT_NUM_PER_WORD_IN_TABLE));
  for(; slot_index < slot_num; ++slot_index){
    assert(!(index_word & ~cur_mark_mask));
    if(index_word & mark_color){
      if(cur_free_slot_num){
        memset((void*)base, 0, slot_size*cur_free_slot_num);
        assert(free_slot_num >= cur_free_slot_num);
        free_slot_num -= cur_free_slot_num;
        cur_free_slot_num = 0;
        if(!free_slot_num) break;
      }
      assert(live_num);
      --live_num;
    } else {
      if(cur_free_slot_num){
        ++cur_free_slot_num;
      } else {
        base = chunk_base + slot_size * slot_index;
        cur_free_slot_num = 1;
        if(!live_num) break;
      }
    }
    mark_color <<= COLOR_BITS_PER_OBJ;
    if(!mark_color){
      mark_color = cur_mark_black_color;
      ++word_index;
      index_word = table[word_index];
      while(index_word == cur_mark_mask && cur_free_slot_num == 0 && slot_index < slot_num){
        slot_index += SLOT_NUM_PER_WORD_IN_TABLE;
        ++word_index;
        index_word = table[word_index];
        assert(live_num >= SLOT_NUM_PER_WORD_IN_TABLE);
        live_num -= SLOT_NUM_PER_WORD_IN_TABLE;
      }
      while(index_word == 0 && cur_free_slot_num > 0 && slot_index < slot_num){
        slot_index += SLOT_NUM_PER_WORD_IN_TABLE;
        ++word_index;
        index_word = table[word_index];
        cur_free_slot_num += SLOT_NUM_PER_WORD_IN_TABLE;
      }
    }
  }
  assert((cur_free_slot_num>0 && live_num==0) || (cur_free_slot_num==0 && live_num>0));
  if(cur_free_slot_num)
    memset((void*)base, 0, slot_size*free_slot_num);
}

static void collector_sweep_normal_chunk(Collector *collector, Sspace *sspace, Chunk_Header *chunk)
{
  unsigned int slot_num = chunk->slot_num;
  unsigned int live_num = 0;
  unsigned int first_free_word_index = MAX_SLOT_INDEX;
  POINTER_SIZE_INT *table = chunk->table;
  
  unsigned int index_word_num = (slot_num + SLOT_NUM_PER_WORD_IN_TABLE - 1) / SLOT_NUM_PER_WORD_IN_TABLE;
  for(unsigned int i=0; i<index_word_num; ++i){
    table[i] &= cur_mark_mask;
    unsigned int live_num_in_word = (table[i] == cur_mark_mask) ? SLOT_NUM_PER_WORD_IN_TABLE : word_set_bit_num(table[i]);
    live_num += live_num_in_word;
    if((first_free_word_index == MAX_SLOT_INDEX) && (live_num_in_word < SLOT_NUM_PER_WORD_IN_TABLE)){
      first_free_word_index = i;
      pfc_set_slot_index((Chunk_Header*)chunk, first_free_word_index, cur_mark_black_color);
    }
  }
  assert(live_num <= slot_num);
  chunk->alloc_num = live_num;
  collector->live_obj_size += live_num * chunk->slot_size;
  collector->live_obj_num += live_num;

  if(!live_num){  /* all objects in this chunk are dead */
    collector_add_free_chunk(collector, (Free_Chunk*)chunk);
  } else if(chunk_is_reusable(chunk)){  /* most objects in this chunk are swept, add chunk to pfc list*/
    chunk->alloc_num = live_num;
    chunk_pad_last_index_word((Chunk_Header*)chunk, cur_mark_mask);
    sspace_put_pfc(sspace, chunk);
  }
  /* the rest: chunks with free rate < PFC_REUSABLE_RATIO. we don't use them */
}

static inline void collector_sweep_abnormal_chunk(Collector *collector, Sspace *sspace, Chunk_Header *chunk)
{
  assert(chunk->status == CHUNK_ABNORMAL);
  POINTER_SIZE_INT *table = chunk->table;
  table[0] &= cur_mark_mask;
  if(!table[0]){
    collector_add_free_chunk(collector, (Free_Chunk*)chunk);
  }
  else {
    collector->live_obj_size += CHUNK_SIZE(chunk);
    collector->live_obj_num++;
  }
}

void sspace_sweep(Collector *collector, Sspace *sspace)
{
  Chunk_Header_Basic *chunk;
  collector->live_obj_size = 0;
  collector->live_obj_num = 0;

  chunk = sspace_grab_next_chunk(sspace, &next_chunk_for_sweep, TRUE);
  while(chunk){
    /* chunk is free before GC */
    if(chunk->status == CHUNK_FREE){
      collector_add_free_chunk(collector, (Free_Chunk*)chunk);
    } else if(chunk->status & CHUNK_NORMAL){   /* chunk is used as a normal sized obj chunk */
      collector_sweep_normal_chunk(collector, sspace, (Chunk_Header*)chunk);
    } else {  /* chunk is used as a super obj chunk */
      collector_sweep_abnormal_chunk(collector, sspace, (Chunk_Header*)chunk);
    }
    
    chunk = sspace_grab_next_chunk(sspace, &next_chunk_for_sweep, TRUE);
  }
}

/************ For merging free chunks in sspace ************/

static void merge_free_chunks_in_list(Sspace *sspace, Free_Chunk_List *list)
{
  Free_Chunk *sspace_ceiling = (Free_Chunk*)space_heap_end((Space*)sspace);
  Free_Chunk *chunk = list->head;
  
  while(chunk){
    assert(chunk->status == (CHUNK_FREE | CHUNK_TO_MERGE));
    /* Remove current chunk from the chunk list */
    list->head = chunk->next;
    if(list->head)
      list->head->prev = NULL;
    /* Check if the prev adjacent chunks are free */
    Free_Chunk *prev_chunk = (Free_Chunk*)chunk->adj_prev;
    while(prev_chunk && prev_chunk->status == (CHUNK_FREE | CHUNK_TO_MERGE)){
      assert(prev_chunk < chunk);
      /* Remove prev_chunk from list */
      free_list_detach_chunk(list, prev_chunk);
      prev_chunk->adj_next = chunk->adj_next;
      chunk = prev_chunk;
      prev_chunk = (Free_Chunk*)chunk->adj_prev;
    }
    /* Check if the back adjcent chunks are free */
    Free_Chunk *back_chunk = (Free_Chunk*)chunk->adj_next;
    while(back_chunk < sspace_ceiling && back_chunk->status == (CHUNK_FREE | CHUNK_TO_MERGE)){
      assert(chunk < back_chunk);
      /* Remove back_chunk from list */
      free_list_detach_chunk(list, back_chunk);
      back_chunk = (Free_Chunk*)back_chunk->adj_next;
      chunk->adj_next = (Chunk_Header_Basic*)back_chunk;
    }
    if(back_chunk < sspace_ceiling)
      back_chunk->adj_prev = (Chunk_Header_Basic*)chunk;
    
    /* put the free chunk to the according free chunk list */
    sspace_put_free_chunk(sspace, chunk);
    
    chunk = list->head;
  }
}

void sspace_merge_free_chunks(GC *gc, Sspace *sspace)
{
  Free_Chunk_List free_chunk_list;
  free_chunk_list.head = NULL;
  free_chunk_list.tail = NULL;
  
  /* Collect free chunks from collectors to one list */
  for(unsigned int i=0; i<gc->num_collectors; ++i){
    Free_Chunk_List *list = gc->collectors[i]->free_chunk_list;
    move_free_chunks_between_lists(&free_chunk_list, list);
  }
  
  merge_free_chunks_in_list(sspace, &free_chunk_list);
}

void sspace_remerge_free_chunks(GC *gc, Sspace *sspace)
{
  Free_Chunk_List free_chunk_list;
  free_chunk_list.head = NULL;
  free_chunk_list.tail = NULL;
  
  /* Collect free chunks from sspace free chunk lists to one list */
  sspace_collect_free_chunks_to_list(sspace, &free_chunk_list);
  
  /* Collect free chunks from collectors to one list */
  for(unsigned int i=0; i<gc->num_collectors; ++i){
    Free_Chunk_List *list = gc->collectors[i]->free_chunk_list;
    move_free_chunks_between_lists(&free_chunk_list, list);
  }
  
  merge_free_chunks_in_list(sspace, &free_chunk_list);
}
