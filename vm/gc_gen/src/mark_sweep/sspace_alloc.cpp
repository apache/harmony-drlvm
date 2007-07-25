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

#include "sspace.h"
#include "sspace_chunk.h"
#include "sspace_mark_sweep.h"
#include "gc_ms.h"
#include "../gen/gen.h"

static Boolean slot_is_alloc_in_table(POINTER_SIZE_INT *table, unsigned int slot_index)
{
  unsigned int color_bits_index = slot_index * COLOR_BITS_PER_OBJ;
  unsigned int word_index = color_bits_index / BITS_PER_WORD;
  unsigned int index_in_word = color_bits_index % BITS_PER_WORD;
  
  return (Boolean)(table[word_index] & (cur_alloc_color << index_in_word));
}

static void alloc_slot_in_table(POINTER_SIZE_INT *table, unsigned int slot_index)
{
  assert(!slot_is_alloc_in_table(table, slot_index));
  
  unsigned int color_bits_index = slot_index * COLOR_BITS_PER_OBJ;
  unsigned int word_index = color_bits_index / BITS_PER_WORD;
  unsigned int index_in_word = color_bits_index % BITS_PER_WORD;
  
  table[word_index] |= cur_alloc_color << index_in_word;
}

static unsigned int first_free_index_in_color_word(POINTER_SIZE_INT word)
{
  unsigned int index = 0;
  
  while(index < BITS_PER_WORD){
    if(!(word & (cur_mark_color << index)))
      return index;
    index += COLOR_BITS_PER_OBJ;
  }
  
  assert(0);  /* There must be a free obj in this table word */
  return MAX_SLOT_INDEX;
}

static Boolean next_free_index_in_color_word(POINTER_SIZE_INT word, unsigned int &index)
{
  while(index < BITS_PER_WORD){
    if(!(word & (cur_alloc_color << index)))
      return TRUE;
    index += COLOR_BITS_PER_OBJ;
  }
  return FALSE;
}

static unsigned int composed_slot_index(unsigned int word_index, unsigned int index_in_word)
{
  unsigned int color_bits_index = word_index*BITS_PER_WORD + index_in_word;
  return color_bits_index/COLOR_BITS_PER_OBJ;
}

static unsigned int next_free_slot_index_in_table(POINTER_SIZE_INT *table, unsigned int slot_index, unsigned int slot_num)
{
  assert(slot_is_alloc_in_table(table, slot_index));
  
  unsigned int max_word_index = ((slot_num-1) * COLOR_BITS_PER_OBJ) / BITS_PER_WORD;
  Boolean found = FALSE;
  
  unsigned int color_bits_index = slot_index * COLOR_BITS_PER_OBJ;
  unsigned int word_index = color_bits_index / BITS_PER_WORD;
  unsigned int index_in_word = color_bits_index % BITS_PER_WORD;
  
  while(word_index < max_word_index){
    found = next_free_index_in_color_word(table[word_index], index_in_word);
    if(found)
      return composed_slot_index(word_index, index_in_word);
    ++word_index;
    index_in_word = 0;
   }
  
  index_in_word = 0;
  found = next_free_index_in_color_word(table[word_index], index_in_word);
  if(found)
    return composed_slot_index(word_index, index_in_word);
  
  return MAX_SLOT_INDEX;
}

/* Used for collecting pfc */
void chunk_set_slot_index(Chunk_Header* chunk, unsigned int first_free_word_index)
{
  unsigned int index_in_word = first_free_index_in_color_word(chunk->table[first_free_word_index]);
  assert(index_in_word != MAX_SLOT_INDEX);
  chunk->slot_index = composed_slot_index(first_free_word_index, index_in_word);
}


/* 1. No need of synchronization. This is a allocator local chunk no matter it is a small or medium obj chunk.
 * 2. If this chunk runs out of space, clear the chunk pointer.
 *    So it is important to give an argument which is a local chunk pointer of a allocator while invoking this func.
 */
static void *alloc_in_chunk(Chunk_Header* &chunk)
{
  POINTER_SIZE_INT *table = chunk->table;
  unsigned int slot_index = chunk->slot_index;
  
  void *p_obj = (void*)((POINTER_SIZE_INT)chunk->base + ((POINTER_SIZE_INT)chunk->slot_size * slot_index));
  alloc_slot_in_table(table, slot_index);
  if(chunk->status & CHUNK_NEED_ZEROING)
    memset(p_obj, 0, chunk->slot_size);
#ifdef SSPACE_VERIFY
  sspace_verify_free_area((POINTER_SIZE_INT*)p_obj, chunk->slot_size);
#endif
  
  chunk->slot_index = next_free_slot_index_in_table(table, slot_index, chunk->slot_num);
  if(chunk->slot_index == MAX_SLOT_INDEX){
    chunk->status = CHUNK_USED | CHUNK_NORMAL;
    chunk = NULL;
  }
  
  return p_obj;
}

/* alloc small without-fin object in sspace without getting new free chunk */
void *sspace_thread_local_alloc(unsigned size, Allocator *allocator)
{
  if(size > SUPER_OBJ_THRESHOLD) return NULL;
  
  Sspace *sspace = gc_get_sspace(allocator->gc);
  void *p_obj = NULL;
  
  unsigned int seg_index = 0;
  Size_Segment *size_seg = sspace->size_segments[0];
  
  for(; seg_index < SIZE_SEGMENT_NUM; ++seg_index, ++size_seg)
    if(size <= size_seg->size_max) break;
  assert(seg_index < SIZE_SEGMENT_NUM);
  
  size = NORMAL_SIZE_ROUNDUP(size, size_seg);
  unsigned int index = NORMAL_SIZE_TO_INDEX(size, size_seg);
  Boolean local_alloc = size_seg->local_alloc;
  Chunk_Header *chunk = NULL;
  
  if(local_alloc){
    Chunk_Header **chunks = allocator->local_chunks[seg_index];
    chunk = chunks[index];
    if(!chunk){
      chunk = sspace_get_pfc(sspace, seg_index, index);
      //if(!chunk) chunk = sspace_steal_pfc(sspace, seg_index, index);
      if(!chunk) return NULL;
      chunk->status |= CHUNK_IN_USE;
      chunks[index] = chunk;
    }
    p_obj = alloc_in_chunk(chunks[index]);
  } else {
    chunk = sspace_get_pfc(sspace, seg_index, index);
    //if(!chunk) chunk = sspace_steal_pfc(sspace, seg_index, index);
    if(!chunk) return NULL;
    p_obj = alloc_in_chunk(chunk);
    if(chunk)
      sspace_put_pfc(sspace, chunk);
  }
  
  assert(p_obj);

#ifdef SSPACE_ALLOC_INFO
  sspace_alloc_info(size);
#endif
#ifdef SSPACE_VERIFY
  sspace_verify_alloc(p_obj, size);
#endif

  return p_obj;
}
static void *sspace_alloc_normal_obj(Sspace *sspace, unsigned size, Allocator *allocator)
{
  void *p_obj = NULL;
  
  unsigned int seg_index = 0;
  Size_Segment *size_seg = sspace->size_segments[0];
  
  for(; seg_index < SIZE_SEGMENT_NUM; ++seg_index, ++size_seg)
    if(size <= size_seg->size_max) break;
  assert(seg_index < SIZE_SEGMENT_NUM);
  
  size = NORMAL_SIZE_ROUNDUP(size, size_seg);
  unsigned int index = NORMAL_SIZE_TO_INDEX(size, size_seg);
  Boolean local_alloc = size_seg->local_alloc;
  Chunk_Header *chunk = NULL;
  
  if(local_alloc){
    Chunk_Header **chunks = allocator->local_chunks[seg_index];
    chunk = chunks[index];
    if(!chunk){
      chunk = sspace_get_pfc(sspace, seg_index, index);
      if(!chunk){
        chunk = (Chunk_Header*)sspace_get_normal_free_chunk(sspace);
        if(chunk) normal_chunk_init(chunk, size);
      }
      //if(!chunk) chunk = sspace_steal_pfc(sspace, seg_index, index);
      if(!chunk) return NULL;
      chunk->status |= CHUNK_IN_USE;
      chunks[index] = chunk;
    }
    p_obj = alloc_in_chunk(chunks[index]);
  } else {
    chunk = sspace_get_pfc(sspace, seg_index, index);
    if(!chunk){
      chunk = (Chunk_Header*)sspace_get_normal_free_chunk(sspace);
      if(chunk) normal_chunk_init(chunk, size);
    }
    //if(!chunk) chunk = sspace_steal_pfc(sspace, seg_index, index);
    if(!chunk) return NULL;
    p_obj = alloc_in_chunk(chunk);
    if(chunk)
      sspace_put_pfc(sspace, chunk);
  }
  
  return p_obj;
}

static void *sspace_alloc_super_obj(Sspace *sspace, unsigned size, Allocator *allocator)
{
  assert(size > SUPER_OBJ_THRESHOLD);

  unsigned int chunk_size = SUPER_SIZE_ROUNDUP(size);
  assert(chunk_size > SUPER_OBJ_THRESHOLD);
  assert(!(chunk_size & CHUNK_GRANULARITY_LOW_MASK));
  
  Chunk_Header *chunk;
  if(chunk_size <= HYPER_OBJ_THRESHOLD)
    chunk = (Chunk_Header*)sspace_get_abnormal_free_chunk(sspace, chunk_size);
  else
    chunk = (Chunk_Header*)sspace_get_hyper_free_chunk(sspace, chunk_size, FALSE);
  
  if(!chunk) return NULL;
  abnormal_chunk_init(chunk, chunk_size, size);
  chunk->table[0] = cur_alloc_color;
  set_super_obj_mask(chunk->base);
  assert(get_obj_info_raw((Partial_Reveal_Object*)chunk->base) & SUPER_OBJ_MASK);
  //printf("Obj: %x  size: %x\t", (POINTER_SIZE_INT)chunk->base, size);
  return chunk->base;
}

static void *sspace_try_alloc(unsigned size, Allocator *allocator)
{
  Sspace *sspace = gc_get_sspace(allocator->gc);
  void *p_obj = NULL;
  
  if(size <= SUPER_OBJ_THRESHOLD)
    p_obj = sspace_alloc_normal_obj(sspace, size, allocator);
  else
    p_obj = sspace_alloc_super_obj(sspace, size, allocator);

#ifdef SSPACE_ALLOC_INFO
  if(p_obj) sspace_alloc_info(size);
#endif
#ifdef SSPACE_VERIFY
  if(p_obj) sspace_verify_alloc(p_obj, size);
#endif

  return p_obj;
}

/* FIXME:: the collection should be seperated from the alloation */
void *sspace_alloc(unsigned size, Allocator *allocator)
{
  void *p_obj = NULL;
  
  /* First, try to allocate object from TLB (thread local chunk) */
  p_obj = sspace_try_alloc(size, allocator);
  if(p_obj)  return p_obj;
  
  vm_gc_lock_enum();
  /* after holding lock, try if other thread collected already */
  p_obj = sspace_try_alloc(size, allocator);
  if(p_obj){
    vm_gc_unlock_enum();
    return p_obj;
  }
  gc_reclaim_heap(allocator->gc, GC_CAUSE_POS_IS_FULL);
  vm_gc_unlock_enum();

#ifdef SSPACE_CHUNK_INFO
  printf("Failure size: %x\n", size);
#endif

  p_obj = sspace_try_alloc(size, allocator);
  
  return p_obj;
}
