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

#include "sspace_chunk.h"
#include "sspace_mark_sweep.h"


Chunk_Heaer_Basic *volatile next_chunk_for_sweep;


static unsigned int word_set_bit_num(POINTER_SIZE_INT word)
{
  unsigned int count = 0;
  
  while(word){
    word &= word - 1;
    ++count;
  }
  return count;
}

static Chunk_Heaer_Basic *sspace_get_next_sweep_chunk(Collector *collector, Sspace *sspace)
{
  Chunk_Heaer_Basic *cur_sweep_chunk = next_chunk_for_sweep;
  
  Chunk_Heaer_Basic *sspace_ceiling = (Chunk_Heaer_Basic*)space_heap_end((Space*)sspace);
  while(cur_sweep_chunk < sspace_ceiling){
    Chunk_Heaer_Basic *next_sweep_chunk = CHUNK_END(cur_sweep_chunk);
    
    Chunk_Heaer_Basic *temp = (Chunk_Heaer_Basic*)atomic_casptr((volatile void **)&next_chunk_for_sweep, next_sweep_chunk, cur_sweep_chunk);
    if(temp == cur_sweep_chunk){
      if(next_sweep_chunk < sspace_ceiling)
        next_sweep_chunk->adj_prev = cur_sweep_chunk;
      return cur_sweep_chunk;
    }
    cur_sweep_chunk = next_chunk_for_sweep;
  }
  
  return NULL;
}

static void collector_add_free_chunk(Collector *collector, Free_Chunk *chunk)
{
  Free_Chunk_List *list = collector->free_chunk_list;
  
  chunk->status = CHUNK_FREE | CHUNK_IN_USE;
  chunk->next = list->head;
  chunk->prev = NULL;
  if(list->head)
    list->head->prev = chunk;
  else
    list->tail = chunk;
  list->head = chunk;
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
  POINTER_SIZE_INT mark_color = cur_mark_color << (COLOR_BITS_PER_OBJ * (slot_index % SLOT_NUM_PER_WORD_IN_TABLE));
  for(; slot_index < slot_num; ++slot_index){
    assert(!(index_word & ~mark_mask_in_table));
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
      mark_color = cur_mark_color;
      ++word_index;
      index_word = table[word_index];
      while(index_word == mark_mask_in_table && cur_free_slot_num == 0 && slot_index < slot_num){
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
    table[i] &= mark_mask_in_table;
    unsigned int live_num_in_word = (table[i] == mark_mask_in_table) ? SLOT_NUM_PER_WORD_IN_TABLE : word_set_bit_num(table[i]);
    live_num += live_num_in_word;
    if((first_free_word_index == MAX_SLOT_INDEX) && (live_num_in_word < SLOT_NUM_PER_WORD_IN_TABLE)){
      first_free_word_index = i;
      chunk_set_slot_index((Chunk_Header*)chunk, first_free_word_index);
    }
  }
  assert(live_num <= slot_num);
#ifdef SSPACE_VERIFY
  collector->live_obj_num += live_num;
  //printf("Chunk: %x  live obj: %d slot num: %d\n", (POINTER_SIZE_INT)chunk, live_num, slot_num);
#endif
  if(!live_num){  /* all objects in this chunk are dead */
    collector_add_free_chunk(collector, (Free_Chunk*)chunk);
  } else if((float)(slot_num-live_num)/slot_num > PFC_REUSABLE_RATIO){  /* most objects in this chunk are swept, add chunk to pfc list*/
#ifdef SSPACE_VERIFY
    //zeroing_free_areas_in_pfc((Chunk_Header*)chunk, live_num);
#endif
    chunk_pad_last_index_word((Chunk_Header*)chunk, mark_mask_in_table);
    sspace_put_pfc(sspace, chunk, chunk->slot_size);
  }
  /* the rest: chunks with free rate < 0.1. we don't use them */
#ifdef SSPACE_VERIFY
  //else// if(live_num < slot_num)
    //zeroing_free_areas_in_pfc((Chunk_Header*)chunk, live_num);
#endif
}

void sspace_sweep(Collector *collector, Sspace *sspace)
{
  Chunk_Heaer_Basic *chunk;
#ifdef SSPACE_VERIFY
  collector->live_obj_num = 0;
#endif

  chunk = sspace_get_next_sweep_chunk(collector, sspace);
  while(chunk){
    /* chunk is free before GC */
    if(chunk->status == CHUNK_FREE){
      collector_add_free_chunk(collector, (Free_Chunk*)chunk);
    } else if(chunk->status & CHUNK_NORMAL){   /* chunk is used as a normal sized obj chunk */
      collector_sweep_normal_chunk(collector, sspace, (Chunk_Header*)chunk);
    } else {  /* chunk is used as a super obj chunk */
      assert(chunk->status & (CHUNK_IN_USE | CHUNK_ABNORMAL));
      POINTER_SIZE_INT *table = ((Chunk_Header*)chunk)->table;
      table[0] &= mark_mask_in_table;
      if(!table[0]){
        collector_add_free_chunk(collector, (Free_Chunk*)chunk);
      }
#ifdef SSPACE_VERIFY
      else {
        collector->live_obj_num++;
      }
#endif
    }
    
    chunk = sspace_get_next_sweep_chunk(collector, sspace);
  }
}

static void free_list_detach_chunk(Free_Chunk_List *list, Free_Chunk *chunk)
{
  if(chunk->prev)
    chunk->prev->next = chunk->next;
  else  // chunk is the head
    list->head = chunk->next;
  if(chunk->next)
    chunk->next->prev = chunk->prev;
}

void gc_collect_free_chunks(GC *gc, Sspace *sspace)
{
  Free_Chunk *sspace_ceiling = (Free_Chunk*)space_heap_end((Space*)sspace);
  
  Free_Chunk_List free_chunk_list;
  free_chunk_list.head = NULL;
  free_chunk_list.tail = NULL;
  
  /* Collect free chunks from collectors to one list */
  for(unsigned int i=0; i<gc->num_collectors; ++i){
    Free_Chunk_List *list = gc->collectors[i]->free_chunk_list;
    if(free_chunk_list.tail){
      free_chunk_list.head->prev = list->tail;
    } else {
      free_chunk_list.tail = list->tail;
    }
    if(list->head){
      list->tail->next = free_chunk_list.head;
      free_chunk_list.head = list->head;
    }
    list->head = NULL;
    list->tail = NULL;
  }
  
  Free_Chunk *chunk = free_chunk_list.head;
  while(chunk){
    assert(chunk->status == (CHUNK_FREE | CHUNK_IN_USE));
    /* Remove current chunk from the chunk list */
    free_chunk_list.head = chunk->next;
    if(free_chunk_list.head)
      free_chunk_list.head->prev = NULL;
    /* Check if the back adjcent chunks are free */
    Free_Chunk *back_chunk = (Free_Chunk*)chunk->adj_next;
    while(back_chunk < sspace_ceiling && back_chunk->status == (CHUNK_FREE | CHUNK_IN_USE)){
      /* Remove back_chunk from list */
      free_list_detach_chunk(&free_chunk_list, back_chunk);
      chunk->adj_next = back_chunk->adj_next;
      back_chunk = (Free_Chunk*)chunk->adj_next;
    }
    /* Check if the prev adjacent chunks are free */
    Free_Chunk *prev_chunk = (Free_Chunk*)chunk->adj_prev;
    while(prev_chunk && prev_chunk->status == (CHUNK_FREE | CHUNK_IN_USE)){
      /* Remove prev_chunk from list */
      free_list_detach_chunk(&free_chunk_list, prev_chunk);
      prev_chunk->adj_next = chunk->adj_next;
      chunk = prev_chunk;
      prev_chunk = (Free_Chunk*)chunk->adj_prev;
    }
    
    //zeroing_free_chunk(chunk);
    
    /* put the free chunk to the according free chunk list */
    sspace_put_free_chunk(sspace, chunk);
    
    chunk = free_chunk_list.head;
  }
}
