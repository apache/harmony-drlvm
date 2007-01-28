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

#ifndef _BLOCK_H_
#define _BLOCK_H_

#include "gc_common.h"

#define GC_BLOCK_SHIFT_COUNT 15
#define GC_BLOCK_SIZE_BYTES (1 << GC_BLOCK_SHIFT_COUNT)

enum Block_Status {
  BLOCK_NIL = 0,
  BLOCK_FREE = 0x1,
  BLOCK_IN_USE = 0x2,
  BLOCK_USED = 0x4,
  BLOCK_IN_COMPACT = 0x8,
  BLOCK_COMPACTED = 0x10,
  BLOCK_TARGET = 0x20,
  BLOCK_DEST = 0x40
};

typedef struct Block_Header {
  void* base;                       
  void* free;                       
  void* ceiling;                    
  void* new_free; /* used only during compaction */
  unsigned int block_idx;           
  volatile unsigned int status;
  volatile unsigned int dest_counter;
  Partial_Reveal_Object* src;
  Partial_Reveal_Object* next_src;
  Block_Header* next;
  POINTER_SIZE_INT table[1]; /* entry num == OFFSET_TABLE_SIZE_WORDS */
}Block_Header;

typedef union Block{
    Block_Header header;
    unsigned char raw_bytes[GC_BLOCK_SIZE_BYTES];
}Block;

#define GC_BLOCK_HEADER_VARS_SIZE_BYTES (POINTER_SIZE_INT)&(((Block_Header*)0)->table)

#define SECTOR_SIZE_SHIFT_COUNT  8
#define SECTOR_SIZE_BYTES        (1 << SECTOR_SIZE_SHIFT_COUNT)
#define SECTOR_SIZE_WORDS        (SECTOR_SIZE_BYTES >> BIT_SHIFT_TO_BYTES_PER_WORD)
/* one offset_table word maps to one SECTOR_SIZE_WORDS sector */

/* BlockSize - OffsetTableSize*SECTOR_SIZE_WORDS = HeaderVarsSize + OffsetTableSize
   => OffsetTableSize = (BlockSize - HeaderVars)/(SECTOR_SIZE_WORDS+1) */
#define OFFSET_TABLE_COMPUTE_DIVISOR       (SECTOR_SIZE_WORDS + 1)
#define OFFSET_TABLE_COMPUTED_SIZE_BYTE ((GC_BLOCK_SIZE_BYTES-GC_BLOCK_HEADER_VARS_SIZE_BYTES)/OFFSET_TABLE_COMPUTE_DIVISOR + 1)
#define OFFSET_TABLE_SIZE_BYTES ((OFFSET_TABLE_COMPUTED_SIZE_BYTE + MASK_OF_BYTES_PER_WORD)&~MASK_OF_BYTES_PER_WORD)
#define OFFSET_TABLE_SIZE_WORDS (OFFSET_TABLE_SIZE_BYTES >> BIT_SHIFT_TO_BYTES_PER_WORD)
#define OBJECT_INDEX_TO_OFFSET_TABLE(p_obj)   (ADDRESS_OFFSET_IN_BLOCK_BODY(p_obj) >> SECTOR_SIZE_SHIFT_COUNT)

#define GC_BLOCK_HEADER_SIZE_BYTES (OFFSET_TABLE_SIZE_BYTES + GC_BLOCK_HEADER_VARS_SIZE_BYTES)
#define GC_BLOCK_BODY_SIZE_BYTES (GC_BLOCK_SIZE_BYTES - GC_BLOCK_HEADER_SIZE_BYTES)
#define GC_BLOCK_BODY(block) ((void*)((POINTER_SIZE_INT)(block) + GC_BLOCK_HEADER_SIZE_BYTES))
#define GC_BLOCK_END(block) ((void*)((POINTER_SIZE_INT)(block) + GC_BLOCK_SIZE_BYTES))

#define GC_BLOCK_LOW_MASK ((POINTER_SIZE_INT)(GC_BLOCK_SIZE_BYTES - 1))
#define GC_BLOCK_HIGH_MASK (~GC_BLOCK_LOW_MASK)
#define GC_BLOCK_HEADER(addr) ((Block_Header *)((POINTER_SIZE_INT)(addr) & GC_BLOCK_HIGH_MASK))
#define GC_BLOCK_INDEX(addr) ((unsigned int)(GC_BLOCK_HEADER(addr)->block_idx))
#define GC_BLOCK_INDEX_FROM(heap_start, addr) ((unsigned int)(((POINTER_SIZE_INT)(addr)-(POINTER_SIZE_INT)(heap_start)) >> GC_BLOCK_SHIFT_COUNT))

#define ADDRESS_OFFSET_TO_BLOCK_HEADER(addr) ((unsigned int)((POINTER_SIZE_INT)addr&GC_BLOCK_LOW_MASK))
#define ADDRESS_OFFSET_IN_BLOCK_BODY(addr) ((unsigned int)(ADDRESS_OFFSET_TO_BLOCK_HEADER(addr)- GC_BLOCK_HEADER_SIZE_BYTES))

inline void block_init(Block_Header* block)
{
  block->free = (void*)((POINTER_SIZE_INT)block + GC_BLOCK_HEADER_SIZE_BYTES);
  block->ceiling = (void*)((POINTER_SIZE_INT)block + GC_BLOCK_SIZE_BYTES); 
  block->base = block->free;
  block->new_free = block->free;
  block->status = BLOCK_FREE;
  block->dest_counter = 0;
  block->src = NULL;
  block->next_src = NULL;
}

inline Partial_Reveal_Object *obj_end(Partial_Reveal_Object *obj)
{
  return (Partial_Reveal_Object *)((POINTER_SIZE_INT)obj + vm_object_size(obj));
}

inline Partial_Reveal_Object *next_marked_obj_in_block(Partial_Reveal_Object *cur_obj, Partial_Reveal_Object *block_end)
{
  while(cur_obj < block_end){
    if( obj_is_marked_in_vt(cur_obj))
      return cur_obj;
    cur_obj = obj_end(cur_obj);
  }
  
  return NULL;
}

inline Partial_Reveal_Object* block_get_first_marked_object(Block_Header* block, void** start_pos)
{
  Partial_Reveal_Object* cur_obj = (Partial_Reveal_Object*)block->base;
  Partial_Reveal_Object* block_end = (Partial_Reveal_Object*)block->free;

  Partial_Reveal_Object* first_marked_obj = next_marked_obj_in_block(cur_obj, block_end);
  if(!first_marked_obj)
    return NULL;
  
  *start_pos = obj_end(first_marked_obj);
  
  return first_marked_obj;
}

inline Partial_Reveal_Object* block_get_next_marked_object(Block_Header* block, void** start_pos)
{
  Partial_Reveal_Object* cur_obj = *(Partial_Reveal_Object**)start_pos;
  Partial_Reveal_Object* block_end = (Partial_Reveal_Object*)block->free;

  Partial_Reveal_Object* next_marked_obj = next_marked_obj_in_block(cur_obj, block_end);
  if(!next_marked_obj)
    return NULL;
  
  *start_pos = obj_end(next_marked_obj);
  
  return next_marked_obj;
}

inline Partial_Reveal_Object *block_get_first_marked_obj_prefetch_next(Block_Header *block, void **start_pos)
{
  Partial_Reveal_Object *cur_obj = (Partial_Reveal_Object *)block->base;
  Partial_Reveal_Object *block_end = (Partial_Reveal_Object *)block->free;
  
  Partial_Reveal_Object *first_marked_obj = next_marked_obj_in_block(cur_obj, block_end);
  if(!first_marked_obj)
    return NULL;
  
  Partial_Reveal_Object *next_obj = obj_end(first_marked_obj);
  *start_pos = next_obj;
  
  if(next_obj >= block_end)
    return first_marked_obj;
  
  Partial_Reveal_Object *next_marked_obj = next_marked_obj_in_block(next_obj, block_end);
  
  if(next_marked_obj){
    if(next_marked_obj != next_obj)
      set_obj_info(next_obj, (Obj_Info_Type)next_marked_obj);
  } else {
    set_obj_info(next_obj, 0);
  }
  
  return first_marked_obj;
}

inline Partial_Reveal_Object *block_get_first_marked_obj_after_prefetch(Block_Header *block, void **start_pos)
{
  return block_get_first_marked_object(block, start_pos);
}

inline Partial_Reveal_Object *block_get_next_marked_obj_prefetch_next(Block_Header *block, void **start_pos)
{
  Partial_Reveal_Object *cur_obj = *(Partial_Reveal_Object **)start_pos;
  Partial_Reveal_Object *block_end = (Partial_Reveal_Object *)block->free;

  if(cur_obj >= block_end)
    return NULL;
  
  Partial_Reveal_Object *cur_marked_obj;
  
  if(obj_is_marked_in_vt(cur_obj))
    cur_marked_obj = cur_obj;
  else
    cur_marked_obj = (Partial_Reveal_Object *)get_obj_info_raw(cur_obj);
  
  if(!cur_marked_obj)
    return NULL;
  
  Partial_Reveal_Object *next_obj = obj_end(cur_marked_obj);
  *start_pos = next_obj;
  
  if(next_obj >= block_end)
    return cur_marked_obj;
  
  Partial_Reveal_Object *next_marked_obj = next_marked_obj_in_block(next_obj, block_end);
  
  if(next_marked_obj){
    if(next_marked_obj != next_obj)
      set_obj_info(next_obj, (Obj_Info_Type)next_marked_obj);
  } else {
    set_obj_info(next_obj, 0);
  }
  
  return cur_marked_obj;  
}

inline Partial_Reveal_Object *block_get_next_marked_obj_after_prefetch(Block_Header *block, void **start_pos)
{
  Partial_Reveal_Object *cur_obj = *(Partial_Reveal_Object **)start_pos;
  Partial_Reveal_Object *block_end = (Partial_Reveal_Object *)block->free;

  if(cur_obj >= block_end)
    return NULL;
  
  Partial_Reveal_Object *cur_marked_obj;
  
  if(obj_is_marked_in_vt(cur_obj) || obj_is_fw_in_oi(cur_obj))
    cur_marked_obj = cur_obj;
  else
    cur_marked_obj = (Partial_Reveal_Object *)get_obj_info_raw(cur_obj);
  
  if(!cur_marked_obj)
    return NULL;
  
  Partial_Reveal_Object *next_obj = obj_end(cur_marked_obj);
  *start_pos = next_obj;
  
  return cur_marked_obj;
}

inline Partial_Reveal_Object * obj_get_fw_in_table(Partial_Reveal_Object *p_obj)
{
  /* only for inter-sector compaction */
  unsigned int index    = OBJECT_INDEX_TO_OFFSET_TABLE(p_obj);
  Block_Header *curr_block = GC_BLOCK_HEADER(p_obj);
  return (Partial_Reveal_Object *)(((POINTER_SIZE_INT)p_obj) - curr_block->table[index]);
}

inline void block_clear_table(Block_Header* block)
{
  POINTER_SIZE_INT* table = block->table;
  memset(table, 0, OFFSET_TABLE_SIZE_BYTES);
  return;
}


#endif //#ifndef _BLOCK_H_
