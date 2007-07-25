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

#ifndef _SSPACE_CHUNK_H_
#define _SSPACE_CHUNK_H_

#include "sspace.h"

enum Chunk_Status {
  CHUNK_NIL = 0,
  CHUNK_FREE = 0x1,
  CHUNK_IN_USE = 0x2,
  CHUNK_USED = 0x4,
  CHUNK_NORMAL = 0x10,
  CHUNK_ABNORMAL = 0x20,
  CHUNK_NEED_ZEROING = 0x100
};

typedef volatile POINTER_SIZE_INT Chunk_Status_t;

typedef struct Chunk_Heaer_Basic {
  Chunk_Heaer_Basic *next;
  Chunk_Status_t status;
  Chunk_Heaer_Basic *adj_prev;  // adjacent previous chunk, for merging continuous free chunks
  Chunk_Heaer_Basic *adj_next;  // adjacent next chunk
} Chunk_Heaer_Basic;

typedef struct Chunk_Header {
  /* Beginning of Chunk_Heaer_Basic */
  Chunk_Header *next;           /* pointing to the next pfc in the pfc pool */
  Chunk_Status_t status;
  Chunk_Heaer_Basic *adj_prev;  // adjacent previous chunk, for merging continuous free chunks
  Chunk_Heaer_Basic *adj_next;  // adjacent next chunk
  /* End of Chunk_Heaer_Basic */
  void *base;
  unsigned int slot_size;
  unsigned int slot_num;
  unsigned int slot_index;      /* the index of which is the first free slot in this chunk */
  POINTER_SIZE_INT table[1];
} Chunk_Header;


#define NORMAL_CHUNK_SHIFT_COUNT    16
#define NORMAL_CHUNK_SIZE_BYTES     (1 << NORMAL_CHUNK_SHIFT_COUNT)
#define NORMAL_CHUNK_LOW_MASK       ((POINTER_SIZE_INT)(NORMAL_CHUNK_SIZE_BYTES - 1))
#define NORMAL_CHUNK_HIGH_MASK      (~NORMAL_CHUNK_LOW_MASK)
#define NORMAL_CHUNK_HEADER(addr)   ((Chunk_Header*)((POINTER_SIZE_INT)(addr) & NORMAL_CHUNK_HIGH_MASK))
#define ABNORMAL_CHUNK_HEADER(addr) ((Chunk_Header*)((POINTER_SIZE_INT)addr & CHUNK_GRANULARITY_HIGH_MASK))

#define MAX_SLOT_INDEX 0xFFffFFff
#define COLOR_BITS_PER_OBJ 2   // should be powers of 2
#define SLOT_NUM_PER_WORD_IN_TABLE  (BITS_PER_WORD /COLOR_BITS_PER_OBJ)

/* Two equations:
 * 1. CHUNK_HEADER_VARS_SIZE_BYTES + NORMAL_CHUNK_TABLE_SIZE_BYTES + slot_size*NORMAL_CHUNK_SLOT_NUM = NORMAL_CHUNK_SIZE_BYTES
 * 2. (BITS_PER_BYTE * NORMAL_CHUNK_TABLE_SIZE_BYTES)/COLOR_BITS_PER_OBJ >= NORMAL_CHUNK_SLOT_NUM
 * ===>
 * NORMAL_CHUNK_SLOT_NUM <= BITS_PER_BYTE*(NORMAL_CHUNK_SIZE_BYTES - CHUNK_HEADER_VARS_SIZE_BYTES) / (BITS_PER_BYTE*slot_size + COLOR_BITS_PER_OBJ)
 * ===>
 * NORMAL_CHUNK_SLOT_NUM = BITS_PER_BYTE*(NORMAL_CHUNK_SIZE_BYTES - CHUNK_HEADER_VARS_SIZE_BYTES) / (BITS_PER_BYTE*slot_size + COLOR_BITS_PER_OBJ)
 */

#define CHUNK_HEADER_VARS_SIZE_BYTES      ((POINTER_SIZE_INT)&(((Chunk_Header*)0)->table))
#define NORMAL_CHUNK_SLOT_AREA_SIZE_BITS  (BITS_PER_BYTE * (NORMAL_CHUNK_SIZE_BYTES - CHUNK_HEADER_VARS_SIZE_BYTES))
#define SIZE_BITS_PER_SLOT(chunk)         (BITS_PER_BYTE * chunk->slot_size + COLOR_BITS_PER_OBJ)

#define NORMAL_CHUNK_SLOT_NUM(chunk)          (NORMAL_CHUNK_SLOT_AREA_SIZE_BITS / SIZE_BITS_PER_SLOT(chunk))
#define NORMAL_CHUNK_TABLE_SIZE_BYTES(chunk)  (((NORMAL_CHUNK_SLOT_NUM(chunk) + SLOT_NUM_PER_WORD_IN_TABLE-1) / SLOT_NUM_PER_WORD_IN_TABLE) * BYTES_PER_WORD)
#define NORMAL_CHUNK_HEADER_SIZE_BYTES(chunk) (CHUNK_HEADER_VARS_SIZE_BYTES + NORMAL_CHUNK_TABLE_SIZE_BYTES(chunk))

#define NORMAL_CHUNK_BASE(chunk)    ((void*)((POINTER_SIZE_INT)(chunk) + NORMAL_CHUNK_HEADER_SIZE_BYTES(chunk)))
#define ABNORMAL_CHUNK_BASE(chunk)  ((void*)((POINTER_SIZE_INT)(chunk) + sizeof(Chunk_Header)))

#define CHUNK_END(chunk)  ((chunk)->adj_next)
#define CHUNK_SIZE(chunk) ((POINTER_SIZE_INT)chunk->adj_next - (POINTER_SIZE_INT)chunk)

inline void *slot_index_to_addr(Chunk_Header *chunk, unsigned int index)
{ return (void*)((POINTER_SIZE_INT)chunk->base + chunk->slot_size * index); }

inline unsigned int slot_addr_to_index(Chunk_Header *chunk, void *addr)
{ return (unsigned int)(((POINTER_SIZE_INT)addr - (POINTER_SIZE_INT)chunk->base) / chunk->slot_size); }

typedef struct Free_Chunk {
  /* Beginning of Chunk_Heaer_Basic */
  Free_Chunk *next;             /* pointing to the next free Free_Chunk */
  Chunk_Status_t status;
  Chunk_Heaer_Basic *adj_prev;  // adjacent previous chunk, for merging continuous free chunks
  Chunk_Heaer_Basic *adj_next;  // adjacent next chunk
  /* End of Chunk_Heaer_Basic */
  Free_Chunk *prev;             /* pointing to the prev free Free_Chunk */
} Free_Chunk;

typedef struct Free_Chunk_List {
  Free_Chunk *head;  /* get new free chunk from head */
  Free_Chunk *tail;  /* put free chunk to tail */
  unsigned int chunk_num;
  SpinLock lock;
} Free_Chunk_List;

/*
typedef union Chunk{
  Chunk_Header   header;
  Free_Chunk     free_chunk;
  unsigned char  raw_bytes[NORMAL_CHUNK_SIZE_BYTES];
} Chunk;
*/

inline void free_chunk_list_init(Free_Chunk_List *list)
{
  list->head = NULL;
  list->tail = NULL;
  list->chunk_num = 0;
  list->lock = FREE_LOCK;
}

inline void free_chunk_list_clear(Free_Chunk_List *list)
{
  list->head = NULL;
  list->tail = NULL;
  list->chunk_num = 0;
  assert(list->lock == FREE_LOCK);
}

/* Padding the last index word in table to facilitate allocation */
inline void chunk_pad_last_index_word(Chunk_Header *chunk, POINTER_SIZE_INT alloc_mask)
{
  unsigned int ceiling_index_in_last_word = (chunk->slot_num * COLOR_BITS_PER_OBJ) % BITS_PER_WORD;
  if(!ceiling_index_in_last_word)
    return;
  POINTER_SIZE_INT padding_mask = ~((1 << ceiling_index_in_last_word) - 1);
  padding_mask &= alloc_mask;
  unsigned int last_word_index = (chunk->slot_num-1) / SLOT_NUM_PER_WORD_IN_TABLE;
  chunk->table[last_word_index] |= padding_mask;
}

extern POINTER_SIZE_INT alloc_mask_in_table;
/* Used for allocating a fixed-size chunk from free area lists */
inline void normal_chunk_init(Chunk_Header *chunk, unsigned int slot_size)
{
  assert(chunk->status == CHUNK_FREE);
  assert((POINTER_SIZE_INT)chunk->adj_next == (POINTER_SIZE_INT)chunk + NORMAL_CHUNK_SIZE_BYTES);
  
  chunk->next = NULL;
  chunk->status = CHUNK_NORMAL | CHUNK_NEED_ZEROING;
  chunk->slot_size = slot_size;
  chunk->slot_num = NORMAL_CHUNK_SLOT_NUM(chunk);
  chunk->slot_index = 0;
  chunk->base = NORMAL_CHUNK_BASE(chunk);
  memset(chunk->table, 0, NORMAL_CHUNK_TABLE_SIZE_BYTES(chunk));//memset table
  chunk_pad_last_index_word(chunk, alloc_mask_in_table);
}

/* Used for allocating a chunk for large object from free area lists */
inline void abnormal_chunk_init(Chunk_Header *chunk, unsigned int chunk_size, unsigned int obj_size)
{
  assert(chunk->status == CHUNK_FREE);
  assert((POINTER_SIZE_INT)chunk->adj_next == (POINTER_SIZE_INT)chunk + chunk_size);
  
  chunk->next = NULL;
  chunk->status = CHUNK_IN_USE | CHUNK_ABNORMAL;
  chunk->slot_size = obj_size;
  chunk->slot_num = 1;
  chunk->slot_index = 0;
  chunk->base = ABNORMAL_CHUNK_BASE(chunk);
}


#ifdef POINTER64
  #define GC_OBJECT_ALIGNMENT_BITS    3
#else
  #define GC_OBJECT_ALIGNMENT_BITS    2
#endif

#define MEDIUM_OBJ_THRESHOLD  (128)
#define LARGE_OBJ_THRESHOLD   (256)
#define SUPER_OBJ_THRESHOLD   (1024)
#define HYPER_OBJ_THRESHOLD   (128*KB)

#define SMALL_GRANULARITY_BITS  (GC_OBJECT_ALIGNMENT_BITS)
#define MEDIUM_GRANULARITY_BITS (SMALL_GRANULARITY_BITS + 1)
#define LARGE_GRANULARITY_BITS  7
#define CHUNK_GRANULARITY_BITS  10

#define CHUNK_GRANULARITY       (1 << CHUNK_GRANULARITY_BITS)
#define CHUNK_GRANULARITY_LOW_MASK    ((POINTER_SIZE_INT)(CHUNK_GRANULARITY-1))
#define CHUNK_GRANULARITY_HIGH_MASK   (~CHUNK_GRANULARITY_LOW_MASK)

#define SMALL_IS_LOCAL_ALLOC   TRUE
#define MEDIUM_IS_LOCAL_ALLOC  TRUE
#define LARGE_IS_LOCAL_ALLOC  FALSE

#define NORMAL_SIZE_ROUNDUP(size, seg)  (((size) + seg->granularity-1) & seg->gran_high_mask)
#define SUPER_OBJ_TOTAL_SIZE(size)  (sizeof(Chunk_Header) + (size))
#define SUPER_SIZE_ROUNDUP(size)    ((SUPER_OBJ_TOTAL_SIZE(size) + CHUNK_GRANULARITY-1) & CHUNK_GRANULARITY_HIGH_MASK)

#define NORMAL_SIZE_TO_INDEX(size, seg) ((((size)-(seg)->size_min) >> (seg)->gran_shift_bits) - 1)
#define ALIGNED_CHUNK_SIZE_TO_INDEX(size)     (((size) >> NORMAL_CHUNK_SHIFT_COUNT) - 1)
#define UNALIGNED_CHUNK_SIZE_TO_INDEX(size)   (((size) >> CHUNK_GRANULARITY_BITS) - 1)

#define NORMAL_INDEX_TO_SIZE(index, seg)  ((((index) + 1) << (seg)->gran_shift_bits) + (seg)->size_min)
#define ALIGNED_CHUNK_INDEX_TO_SIZE(index)    (((index) + 1) << NORMAL_CHUNK_SHIFT_COUNT)
#define UNALIGNED_CHUNK_INDEX_TO_SIZE(index)  (((index) + 1) << CHUNK_GRANULARITY_BITS)


#define PFC_STEAL_NUM   3
#define PFC_STEAL_THRESHOLD   3

#define SIZE_SEGMENT_NUM  3
typedef struct Size_Segment {
  unsigned int size_min;
  unsigned int size_max;
  unsigned int seg_index;
  Boolean local_alloc;
  unsigned int chunk_num;
  unsigned int gran_shift_bits;
  POINTER_SIZE_INT granularity;
  POINTER_SIZE_INT gran_low_mask;
  POINTER_SIZE_INT gran_high_mask;
} Size_Segment;


inline Chunk_Header *sspace_get_pfc(Sspace *sspace, unsigned int seg_index, unsigned int index)
{
  Pool *pfc_pool = sspace->pfc_pools[seg_index][index];
  Chunk_Header *chunk = (Chunk_Header*)pool_get_entry(pfc_pool);
  assert(!chunk || chunk->status == (CHUNK_NORMAL | CHUNK_NEED_ZEROING));
  return chunk;
}

inline void sspace_put_pfc(Sspace *sspace, Chunk_Header *chunk)
{
  unsigned int size = chunk->slot_size;
  assert(chunk && (size <= SUPER_OBJ_THRESHOLD));
  
  Size_Segment **size_segs = sspace->size_segments;
  chunk->status = CHUNK_NORMAL | CHUNK_NEED_ZEROING;
  
  for(unsigned int i = 0; i < SIZE_SEGMENT_NUM; ++i){
    if(size <= size_segs[i]->size_max){
      assert(!(size & size_segs[i]->gran_low_mask));
      assert(size > size_segs[i]->size_min);
      unsigned int index = NORMAL_SIZE_TO_INDEX(size, size_segs[i]);
      Pool *pfc_pool = sspace->pfc_pools[i][index];
      pool_put_entry(pfc_pool, chunk);
      return;
    }
  }
}


extern void sspace_init_chunks(Sspace *sspace);
extern void sspace_clear_chunk_list(GC *gc);
extern void sspace_put_free_chunk(Sspace *sspace, Free_Chunk *chunk);
extern Free_Chunk *sspace_get_normal_free_chunk(Sspace *sspace);
extern Free_Chunk *sspace_get_abnormal_free_chunk(Sspace *sspace, unsigned int chunk_size);
extern Free_Chunk *sspace_get_hyper_free_chunk(Sspace *sspace, unsigned int chunk_size, Boolean is_normal_chunk);
extern Chunk_Header *sspace_steal_pfc(Sspace *sspace, unsigned int index);

extern void zeroing_free_chunk(Free_Chunk *chunk);


#endif //#ifndef _SSPACE_CHUNK_H_
