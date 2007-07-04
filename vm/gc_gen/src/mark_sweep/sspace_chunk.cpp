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

/* PFC stands for partially free chunk */
#define SMALL_PFC_POOL_NUM    SMALL_LOCAL_CHUNK_NUM
#define MEDIUM_PFC_POOL_NUM   MEDIUM_LOCAL_CHUNK_NUM
#define LARGE_PFC_POOL_NUM    ((SUPER_OBJ_THRESHOLD - LARGE_OBJ_THRESHOLD) >> LARGE_GRANULARITY_BITS)
#define NUM_ALIGNED_FREE_CHUNK_BUCKET   (HYPER_OBJ_THRESHOLD >> NORMAL_CHUNK_SHIFT_COUNT)
#define NUM_UNALIGNED_FREE_CHUNK_BUCKET (HYPER_OBJ_THRESHOLD >> CHUNK_GRANULARITY_BITS)


/* PFC stands for partially free chunk */
static Pool  *small_pfc_pools[SMALL_PFC_POOL_NUM];
static Pool  *medium_pfc_pools[MEDIUM_PFC_POOL_NUM];
static Pool  *large_pfc_pools[LARGE_PFC_POOL_NUM];
static Free_Chunk_List  aligned_free_chunk_lists[NUM_ALIGNED_FREE_CHUNK_BUCKET];
static Free_Chunk_List  unaligned_free_chunk_lists[NUM_UNALIGNED_FREE_CHUNK_BUCKET];
static Free_Chunk_List  hyper_free_chunk_list;

static Boolean  small_pfc_steal_flags[SMALL_PFC_POOL_NUM];
static Boolean  medium_pfc_steal_flags[MEDIUM_PFC_POOL_NUM];
static Boolean  large_pfc_steal_flags[LARGE_PFC_POOL_NUM];

void sspace_init_chunks(Sspace *sspace)
{
  unsigned int i;
  
  /* Init small obj partially free chunk pools */
  for(i=SMALL_PFC_POOL_NUM; i--;){
    small_pfc_steal_flags[i] = FALSE;
    small_pfc_pools[i] = sync_pool_create();
  }
  
  /* Init medium obj partially free chunk pools */
  for(i=MEDIUM_PFC_POOL_NUM; i--;){
    medium_pfc_steal_flags[i] = FALSE;
    medium_pfc_pools[i] = sync_pool_create();
  }
  
  /* Init large obj partially free chunk pools */
  for(i=LARGE_PFC_POOL_NUM; i--;){
    large_pfc_steal_flags[i] = FALSE;
    large_pfc_pools[i] = sync_pool_create();
  }
  
  /* Init aligned free chunk lists */
  for(i=NUM_ALIGNED_FREE_CHUNK_BUCKET; i--;)
    free_chunk_list_init(&aligned_free_chunk_lists[i]);
  
  /* Init nonaligned free chunk lists */
  for(i=NUM_UNALIGNED_FREE_CHUNK_BUCKET; i--;)
    free_chunk_list_init(&unaligned_free_chunk_lists[i]);
  
  /* Init super free chunk lists */
  free_chunk_list_init(&hyper_free_chunk_list);
    
  /* Init Sspace struct's chunk fields */
  sspace->small_pfc_pools = small_pfc_pools;
  sspace->medium_pfc_pools = medium_pfc_pools;
  sspace->large_pfc_pools = large_pfc_pools;
  sspace->aligned_free_chunk_lists = aligned_free_chunk_lists;
  sspace->unaligned_free_chunk_lists = unaligned_free_chunk_lists;
  sspace->hyper_free_chunk_list = &hyper_free_chunk_list;
  
  /* Init the first free chunk: from heap start to heap end */
  Free_Chunk *free_chunk = (Free_Chunk*)sspace->heap_start;
  free_chunk->adj_next = (Chunk_Heaer_Basic*)sspace->heap_end;
  POINTER_SIZE_INT chunk_size = sspace->reserved_heap_size;
  assert(chunk_size > CHUNK_GRANULARITY && !(chunk_size % CHUNK_GRANULARITY));
  sspace_put_free_chunk(sspace, free_chunk);
}

static void pfc_pool_set_steal_flag(Pool *pool, unsigned int steal_threshold, unsigned int &steal_flag)
{
  Chunk_Header *chunk = (Chunk_Header*)pool_get_entry(pool);
  while(chunk){
    steal_threshold--;
    if(!steal_threshold)
      break;
    chunk = chunk->next;
  }
  steal_flag = steal_threshold ? FALSE : TRUE;
}

static void empty_pool(Pool *pool)
{
  pool->top = (Stack_Top)NULL;
  pool->cur = NULL;
}

void sspace_clear_chunk_list(GC *gc)
{
  unsigned int i;
  unsigned int collector_num = gc->num_collectors;
  unsigned int steal_threshold;
  
  steal_threshold = collector_num << SMALL_PFC_STEAL_THRESHOLD;
  for(i=SMALL_PFC_POOL_NUM; i--;){
    Pool *pool = small_pfc_pools[i];
    pfc_pool_set_steal_flag(pool, steal_threshold, small_pfc_steal_flags[i]);
    empty_pool(pool);
  }
  
  steal_threshold = collector_num << MEDIUM_PFC_STEAL_THRESHOLD;
  for(i=MEDIUM_PFC_POOL_NUM; i--;){
    Pool *pool = medium_pfc_pools[i];
    pfc_pool_set_steal_flag(pool, steal_threshold, medium_pfc_steal_flags[i]);
    empty_pool(pool);
  }
  
  steal_threshold = collector_num << LARGE_PFC_STEAL_THRESHOLD;
  for(i=LARGE_PFC_POOL_NUM; i--;){
    Pool *pool = large_pfc_pools[i];
    pfc_pool_set_steal_flag(pool, steal_threshold, large_pfc_steal_flags[i]);
    empty_pool(pool);
  }
  
  for(i=NUM_ALIGNED_FREE_CHUNK_BUCKET; i--;)
    free_chunk_list_clear(&aligned_free_chunk_lists[i]);
  
  for(i=NUM_UNALIGNED_FREE_CHUNK_BUCKET; i--;)
    free_chunk_list_clear(&unaligned_free_chunk_lists[i]);
  
  free_chunk_list_clear(&hyper_free_chunk_list);
  
  /* release small obj chunks of each mutator */
  Mutator *mutator = gc->mutator_list;
  while(mutator){
    Chunk_Header **chunks = mutator->small_chunks;
    for(i=SMALL_LOCAL_CHUNK_NUM; i--;)
      chunks[i] = NULL;
    chunks = mutator->medium_chunks;
    for(i=MEDIUM_LOCAL_CHUNK_NUM; i--;)
      chunks[i] = NULL;
    mutator = mutator->next;
  }
}

/* Simply put the free chunk to the according list
 * Don't merge continuous free chunks
 * The merging job is taken by sweeping
 */
static void list_put_free_chunk(Free_Chunk_List *list, Free_Chunk *chunk)
{
  chunk->status = CHUNK_FREE;
  chunk->adj_prev = NULL;
  chunk->prev = NULL;

  lock(list->lock);
  chunk->next = list->head;
  if(list->head)
    list->head->prev = chunk;
  list->head = chunk;
  assert(list->chunk_num < ~((unsigned int)0));
  ++list->chunk_num;
  unlock(list->lock);
}

static Free_Chunk *free_list_get_head(Free_Chunk_List *list)
{
  lock(list->lock);
  Free_Chunk *chunk = list->head;
  if(chunk){
    list->head = chunk->next;
    if(list->head)
      list->head->prev = NULL;
    assert(list->chunk_num);
    --list->chunk_num;
    assert(chunk->status == CHUNK_FREE);
  }
  unlock(list->lock);
  return chunk;
}

void sspace_put_free_chunk(Sspace *sspace, Free_Chunk *chunk)
{
  POINTER_SIZE_INT chunk_size = CHUNK_SIZE(chunk);
  assert(!(chunk_size % CHUNK_GRANULARITY));
  
  if(chunk_size > HYPER_OBJ_THRESHOLD)
    list_put_free_chunk(sspace->hyper_free_chunk_list, chunk);
  else if(!((POINTER_SIZE_INT)chunk & NORMAL_CHUNK_LOW_MASK) && !(chunk_size & NORMAL_CHUNK_LOW_MASK))
    list_put_free_chunk(&sspace->aligned_free_chunk_lists[ALIGNED_CHUNK_SIZE_TO_INDEX(chunk_size)], chunk);
  else
    list_put_free_chunk(&sspace->unaligned_free_chunk_lists[UNALIGNED_CHUNK_SIZE_TO_INDEX(chunk_size)], chunk);
}

static Free_Chunk *partition_normal_free_chunk(Sspace *sspace, Free_Chunk *chunk)
{
  assert(CHUNK_SIZE(chunk) > NORMAL_CHUNK_SIZE_BYTES);
  
  Chunk_Heaer_Basic *adj_next = chunk->adj_next;
  Free_Chunk *normal_chunk = (Free_Chunk*)(((POINTER_SIZE_INT)chunk + NORMAL_CHUNK_SIZE_BYTES-1) & NORMAL_CHUNK_HIGH_MASK);
  
  if(chunk != normal_chunk){
    assert(chunk < normal_chunk);
    chunk->adj_next = (Chunk_Heaer_Basic*)normal_chunk;
    sspace_put_free_chunk(sspace, chunk);
  }
  normal_chunk->adj_next = (Chunk_Heaer_Basic*)((POINTER_SIZE_INT)normal_chunk + NORMAL_CHUNK_SIZE_BYTES);
  if(normal_chunk->adj_next != adj_next){
    assert(normal_chunk->adj_next < adj_next);
    Free_Chunk *back_chunk = (Free_Chunk*)normal_chunk->adj_next;
    back_chunk->adj_next = adj_next;
    sspace_put_free_chunk(sspace, back_chunk);
  }
  
  normal_chunk->status = CHUNK_FREE;
  return normal_chunk;
}

/* Partition the free chunk to two free chunks:
 * the first one's size is chunk_size
 * the second will be inserted into free chunk list according to its size
 */
static void partition_abnormal_free_chunk(Sspace *sspace,Free_Chunk *chunk, unsigned int chunk_size)
{
  assert(CHUNK_SIZE(chunk) > chunk_size);
  
  Free_Chunk *back_chunk = (Free_Chunk*)((POINTER_SIZE_INT)chunk + chunk_size);
  back_chunk->adj_next = chunk->adj_next;
  chunk->adj_next = (Chunk_Heaer_Basic*)back_chunk;
  sspace_put_free_chunk(sspace, back_chunk);
}

Free_Chunk *sspace_get_normal_free_chunk(Sspace *sspace)
{
  Free_Chunk_List *aligned_lists = sspace->aligned_free_chunk_lists;
  Free_Chunk_List *unaligned_lists = sspace->unaligned_free_chunk_lists;
  Free_Chunk_List *list = NULL;
  Free_Chunk *chunk = NULL;
  
  /* Search in aligned chunk lists first */
  unsigned int index = 0;
  while(index < NUM_ALIGNED_FREE_CHUNK_BUCKET){
    list = &aligned_lists[index];
    if(list->head)
      chunk = free_list_get_head(list);
    if(chunk){
      if(CHUNK_SIZE(chunk) > NORMAL_CHUNK_SIZE_BYTES)
        chunk = partition_normal_free_chunk(sspace, chunk);
      //zeroing_free_chunk(chunk);
      return chunk;
    }
    index++;
  }
  assert(!chunk);
  
  /* Search in unaligned chunk lists with larger chunk.
     (NORMAL_CHUNK_SIZE_BYTES + (NORMAL_CHUNK_SIZE_BYTES-CHUNK_GRANULARITY))
     is the smallest size which can guarantee the chunk includes a normal chunk.
  */
  index = UNALIGNED_CHUNK_SIZE_TO_INDEX((NORMAL_CHUNK_SIZE_BYTES<<1) - CHUNK_GRANULARITY);
  while(index < NUM_UNALIGNED_FREE_CHUNK_BUCKET){
    list = &unaligned_lists[index];
    if(list->head)
      chunk = free_list_get_head(list);
    if(chunk){
      chunk = partition_normal_free_chunk(sspace, chunk);
      assert(!((POINTER_SIZE_INT)chunk & NORMAL_CHUNK_LOW_MASK));
      //zeroing_free_chunk(chunk);
      return chunk;
    }
    index++;
  }
  assert(!chunk);
  
  /* search in the hyper free chunk list */
  chunk = sspace_get_hyper_free_chunk(sspace, NORMAL_CHUNK_SIZE_BYTES, TRUE);
  assert(!((POINTER_SIZE_INT)chunk & NORMAL_CHUNK_LOW_MASK));
  
  return chunk;
}

Free_Chunk *sspace_get_abnormal_free_chunk(Sspace *sspace, unsigned int chunk_size)
{
  assert(chunk_size > CHUNK_GRANULARITY);
  assert(!(chunk_size % CHUNK_GRANULARITY));
  assert(chunk_size <= HYPER_OBJ_THRESHOLD);
  
  Free_Chunk_List *unaligned_lists = sspace->unaligned_free_chunk_lists;
  Free_Chunk_List *list = NULL;
  Free_Chunk *chunk = NULL;
  unsigned int index = 0;
  
  /* Search in the list with chunk size of multiple chunk_size */
  unsigned int search_size = chunk_size;
  while(search_size <= HYPER_OBJ_THRESHOLD){
    index = UNALIGNED_CHUNK_SIZE_TO_INDEX(search_size);
    list = &unaligned_lists[index];
    if(list->head)
      chunk = free_list_get_head(list);
    if(chunk){
      if(search_size > chunk_size)
        partition_abnormal_free_chunk(sspace, chunk, chunk_size);
      zeroing_free_chunk(chunk);
      return chunk;
    }
    search_size += chunk_size;
  }
  assert(!chunk);
  
  /* search in the hyper free chunk list */
  chunk = sspace_get_hyper_free_chunk(sspace, chunk_size, FALSE);
  if(chunk) return chunk;
  
  /* Search again in abnormal chunk lists */
  index = UNALIGNED_CHUNK_SIZE_TO_INDEX(chunk_size);
  while(index < NUM_UNALIGNED_FREE_CHUNK_BUCKET){
    list = &unaligned_lists[index];
    if(list->head)
      chunk = free_list_get_head(list);
    if(chunk){
      if(index > UNALIGNED_CHUNK_SIZE_TO_INDEX(chunk_size))
        partition_abnormal_free_chunk(sspace, chunk, chunk_size);
      zeroing_free_chunk(chunk);
      return chunk;
    }
    ++index;
  }
  
  return chunk;
}

Free_Chunk *sspace_get_hyper_free_chunk(Sspace *sspace, unsigned int chunk_size, Boolean is_normal_chunk)
{
  assert(chunk_size >= CHUNK_GRANULARITY);
  assert(!(chunk_size % CHUNK_GRANULARITY));
  
  Free_Chunk_List *list = sspace->hyper_free_chunk_list;
  lock(list->lock);
  Free_Chunk **p_next = &list->head;
  Free_Chunk *chunk = list->head;
  while(chunk){
    if(CHUNK_SIZE(chunk) >= chunk_size){
      Free_Chunk *next_chunk = chunk->next;
      *p_next = next_chunk;
      if(next_chunk){
        if(chunk != list->head)
          next_chunk->prev = (Free_Chunk *)p_next;  /* utilize an assumption: next is the first field of Free_Chunk */
        else
          next_chunk->prev = NULL;
      }
      break;
    }
    p_next = &chunk->next;
    chunk = chunk->next;
  }
  unlock(list->lock);
  
  if(chunk){
    if(is_normal_chunk)
      chunk = partition_normal_free_chunk(sspace, chunk);
    else if(CHUNK_SIZE(chunk) > chunk_size)
      partition_abnormal_free_chunk(sspace, chunk, chunk_size);
    if(!is_normal_chunk)
      zeroing_free_chunk(chunk);
  }
  
  return chunk;
}

#define min_value(x, y) (((x) < (y)) ? (x) : (y))

Chunk_Header *sspace_steal_small_pfc(Sspace *sspace, unsigned int index)
{
  Chunk_Header *pfc = NULL;
  unsigned int max_index = min_value(index + SMALL_PFC_STEAL_NUM + 1, SMALL_PFC_POOL_NUM);
  ++index;
  for(; index < max_index; ++index){
    if(!small_pfc_steal_flags[index]) continue;
    pfc = sspace_get_small_pfc(sspace, index);
    if(pfc) return pfc;
  }
  return NULL;
}
Chunk_Header *sspace_steal_medium_pfc(Sspace *sspace, unsigned int index)
{
  Chunk_Header *pfc = NULL;
  unsigned int max_index = min_value(index + MEDIUM_PFC_STEAL_NUM + 1, MEDIUM_PFC_POOL_NUM);
  ++index;
  for(; index < max_index; ++index){
    if(!medium_pfc_steal_flags[index]) continue;
    pfc = sspace_get_medium_pfc(sspace, index);
    if(pfc) return pfc;
  }
  return NULL;
}
Chunk_Header *sspace_steal_large_pfc(Sspace *sspace, unsigned int index)
{
  Chunk_Header *pfc = NULL;
  unsigned int max_index = min_value(index + LARGE_PFC_STEAL_NUM + 1, LARGE_PFC_POOL_NUM);
  ++index;
  for(; index < max_index; ++index){
    if(!large_pfc_steal_flags[index]) continue;
    pfc = sspace_get_large_pfc(sspace, index);
    if(pfc) return pfc;
  }
  return NULL;
}

/* Because this computation doesn't use lock, its result is not accurate. And it is enough. */
POINTER_SIZE_INT sspace_free_memory_size(Sspace *sspace)
{
  POINTER_SIZE_INT free_size = 0;
  
  vm_gc_lock_enum();
  
  for(unsigned int i=NUM_ALIGNED_FREE_CHUNK_BUCKET; i--;)
    free_size += NORMAL_CHUNK_SIZE_BYTES * (i+1) * sspace->aligned_free_chunk_lists[i].chunk_num;
  
  for(unsigned int i=NUM_UNALIGNED_FREE_CHUNK_BUCKET; i--;)
    free_size += CHUNK_GRANULARITY * (i+1) * sspace->unaligned_free_chunk_lists[i].chunk_num;
  
  Free_Chunk *hyper_chunk = sspace->hyper_free_chunk_list->head;
  while(hyper_chunk){
    free_size += CHUNK_SIZE(hyper_chunk);
    hyper_chunk = hyper_chunk->next;
  }
  
  vm_gc_unlock_enum();
  
  return free_size;
}


#ifdef SSPACE_CHUNK_INFO

extern POINTER_SIZE_INT alloc_mask_in_table;
static POINTER_SIZE_INT free_mem_size;

static unsigned int word_set_bit_num(POINTER_SIZE_INT word)
{
  unsigned int count = 0;
  
  while(word){
    word &= word - 1;
    ++count;
  }
  return count;
}

static unsigned int pfc_info(Chunk_Header *chunk, Boolean before_gc)
{
  POINTER_SIZE_INT *table = ((Chunk_Header*)chunk)->table;
  unsigned int slot_num = chunk->slot_num;
  unsigned int live_num = 0;
  
  unsigned int index_word_num = (slot_num + SLOT_NUM_PER_WORD_IN_TABLE - 1) / SLOT_NUM_PER_WORD_IN_TABLE;
  for(unsigned int i=0; i<index_word_num; ++i){
    table[i] &= alloc_mask_in_table;
    unsigned int live_num_in_word = (table[i] == alloc_mask_in_table) ? SLOT_NUM_PER_WORD_IN_TABLE : word_set_bit_num(table[i]);
    live_num += live_num_in_word;
  }
  if(before_gc){
    unsigned int slot_num_in_last_word = slot_num % SLOT_NUM_PER_WORD_IN_TABLE;
    if(slot_num_in_last_word){
      unsigned int fake_live_num_in_last_word = SLOT_NUM_PER_WORD_IN_TABLE - slot_num_in_last_word;
      assert(live_num >= fake_live_num_in_last_word);
      live_num -= fake_live_num_in_last_word;
    }
  }
  assert(live_num <= slot_num);
  return live_num;
}

enum Obj_Type {
  SMALL_OBJ,
  MEDIUM_OBJ,
  LARGE_OBJ
};
static unsigned int index_to_size(unsigned int index, Obj_Type type)
{
  if(type == SMALL_OBJ)
    return SMALL_INDEX_TO_SIZE(index);
  if(type == MEDIUM_OBJ)
    return MEDIUM_INDEX_TO_SIZE(index);
  assert(type == LARGE_OBJ);
  return LARGE_INDEX_TO_SIZE(index);
}

static void pfc_pools_info(Sspace *sspace, Pool **pools, unsigned int pool_num, Obj_Type type, Boolean before_gc)
{
  unsigned int index;
  
  for(index = 0; index < pool_num; ++index){
    Pool *pool = pools[index];
    Chunk_Header *chunk = NULL;
    unsigned int chunk_counter = 0;
    unsigned int slot_num = 0;
    unsigned int live_num = 0;
    pool_iterator_init(pool);
    while(chunk = (Chunk_Header*)pool_iterator_next(pool)){
      ++chunk_counter;
      slot_num += chunk->slot_num;
      live_num += pfc_info(chunk, before_gc);
    }
    if(slot_num){
      printf("Size: %x\tchunk num: %d\tlive obj: %d\ttotal obj: %d\tLive Ratio: %f\n", index_to_size(index, type), chunk_counter, live_num, slot_num, (float)live_num/slot_num);
      assert(live_num < slot_num);
      free_mem_size += index_to_size(index, type) * (slot_num-live_num);
      assert(free_mem_size < sspace->committed_heap_size);
    }
  }
}

enum Chunk_Type {
  ALIGNED_CHUNK,
  UNALIGNED_CHUNK
};
static unsigned int chunk_index_to_size(unsigned int index, Chunk_Type type)
{
  if(type == ALIGNED_CHUNK)
    return ALIGNED_CHUNK_INDEX_TO_SIZE(index);
  assert(type == UNALIGNED_CHUNK);
  return UNALIGNED_CHUNK_INDEX_TO_SIZE(index);
}

static void free_lists_info(Sspace *sspace, Free_Chunk_List *lists, unsigned int list_num, Chunk_Type type)
{
  unsigned int index;
  
  for(index = 0; index < list_num; ++index){
    Free_Chunk *chunk = lists[index].head;
    unsigned int chunk_counter = 0;
    while(chunk){
      ++chunk_counter;
      unsigned int chunk_size = CHUNK_SIZE(chunk);
      assert(chunk_size <= HYPER_OBJ_THRESHOLD);
      free_mem_size += chunk_size;
      assert(free_mem_size < sspace->committed_heap_size);
      chunk = chunk->next;
    }
    printf("Free Size: %x\tnum: %d\n", chunk_index_to_size(index, type), chunk_counter);
  }
}

void sspace_chunks_info(Sspace *sspace, Boolean before_gc)
{
  if(!before_gc) return;
  
  printf("\n\nSMALL PFC INFO:\n\n");
  pfc_pools_info(sspace, small_pfc_pools, SMALL_PFC_POOL_NUM, SMALL_OBJ, before_gc);
  
  printf("\n\nMEDIUM PFC INFO:\n\n");
  pfc_pools_info(sspace, medium_pfc_pools, MEDIUM_PFC_POOL_NUM, MEDIUM_OBJ, before_gc);
  
  printf("\n\nLARGE PFC INFO:\n\n");
  pfc_pools_info(sspace, large_pfc_pools, LARGE_PFC_POOL_NUM, LARGE_OBJ, before_gc);
  
  printf("\n\nALIGNED FREE CHUNK INFO:\n\n");
  free_lists_info(sspace, aligned_free_chunk_lists, NUM_ALIGNED_FREE_CHUNK_BUCKET, ALIGNED_CHUNK);
  
  printf("\n\nUNALIGNED FREE CHUNK INFO:\n\n");
  free_lists_info(sspace, unaligned_free_chunk_lists, NUM_UNALIGNED_FREE_CHUNK_BUCKET, UNALIGNED_CHUNK);
  
  printf("\n\nSUPER FREE CHUNK INFO:\n\n");
  Free_Chunk_List *list = &hyper_free_chunk_list;
  Free_Chunk *chunk = list->head;
  while(chunk){
    printf("Size: %x\n", CHUNK_SIZE(chunk));
    free_mem_size += CHUNK_SIZE(chunk);
    assert(free_mem_size < sspace->committed_heap_size);
    chunk = chunk->next;
  }
  printf("\n\nFree mem ratio: %f\n\n", (float)free_mem_size / sspace->committed_heap_size);
  free_mem_size = 0;
}

#endif

#ifdef SSPACE_ALLOC_INFO

#define MEDIUM_THRESHOLD 256
#define LARGE_THRESHOLD (1024)
#define SUPER_THRESHOLD (6*KB)
#define HYPER_THRESHOLD (64*KB)

#define SMALL_OBJ_ARRAY_NUM  (MEDIUM_THRESHOLD >> 2)
#define MEDIUM_OBJ_ARRAY_NUM (LARGE_THRESHOLD >> 4)
#define LARGE_OBJ_ARRAY_NUM  (SUPER_THRESHOLD >> 6)
#define SUPER_OBJ_ARRAY_NUM  (HYPER_THRESHOLD >> 10)

volatile unsigned int small_obj_num[SMALL_OBJ_ARRAY_NUM];
volatile unsigned int medium_obj_num[MEDIUM_OBJ_ARRAY_NUM];
volatile unsigned int large_obj_num[LARGE_OBJ_ARRAY_NUM];
volatile unsigned int super_obj_num[SUPER_OBJ_ARRAY_NUM];
volatile unsigned int hyper_obj_num;

void sspace_alloc_info(unsigned int size)
{
  if(size <= MEDIUM_THRESHOLD)
    atomic_inc32(&small_obj_num[(size>>2)-1]);
  else if(size <= LARGE_THRESHOLD)
    atomic_inc32(&medium_obj_num[(size>>4)-1]);
  else if(size <= SUPER_THRESHOLD)
    atomic_inc32(&large_obj_num[(size>>6)-1]);
  else if(size <= HYPER_THRESHOLD)
    atomic_inc32(&super_obj_num[(size>>10)-1]);
  else
    atomic_inc32(&hyper_obj_num);
}

void sspace_alloc_info_summary(void)
{
  unsigned int i;
  
  printf("\n\nNORMAL OBJ\n\n");
  for(i = 0; i < SMALL_OBJ_ARRAY_NUM; i++){
    printf("Size: %x\tnum: %d\n", (i+1)<<2, small_obj_num[i]);
    small_obj_num[i] = 0;
  }
  
  i = ((MEDIUM_THRESHOLD + (1<<4))>>4) - 1;
  for(; i < MEDIUM_OBJ_ARRAY_NUM; i++){
    printf("Size: %x\tnum: %d\n", (i+1)<<4, medium_obj_num[i]);
    medium_obj_num[i] = 0;
  }
  
  i = ((LARGE_THRESHOLD + (1<<6))>>6) - 1;
  for(; i < LARGE_OBJ_ARRAY_NUM; i++){
    printf("Size: %x\tnum: %d\n", (i+1)<<6, large_obj_num[i]);
    large_obj_num[i] = 0;
  }
  
  i = ((SUPER_THRESHOLD + (1<<10))>>10) - 1;
  for(; i < SUPER_OBJ_ARRAY_NUM; i++){
    printf("Size: %x\tnum: %d\n", (i+1)<<10, super_obj_num[i]);
    super_obj_num[i] = 0;
  }
  
  printf("\n\nHYPER OBJ\n\n");
  printf("num: %d\n", hyper_obj_num);
  hyper_obj_num = 0;
}

#endif
