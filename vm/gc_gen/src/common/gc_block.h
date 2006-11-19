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

#include "../common/gc_common.h"

#define GC_BLOCK_SHIFT_COUNT 15
#define GC_BLOCK_SIZE_BYTES (1 << GC_BLOCK_SHIFT_COUNT)

enum Block_Status {
  BLOCK_NIL = 0,
  BLOCK_FREE = 0x1,
  BLOCK_IN_USE = 0x2,
  BLOCK_USED = 0x4,
  BLOCK_IN_COMPACT = 0x8,
  BLOCK_COMPACTED = 0x10,
  BLOCK_TARGET = 0x20
};

typedef struct Block_Header {
  void* base;                       
  void* free;                       
  void* ceiling;                    
  unsigned int block_idx;           
  unsigned int status;
  Block_Header* next;
  unsigned int mark_table[1];  /* entry num == MARKBIT_TABLE_SIZE_WORDS */
}Block_Header;

typedef union Block{
    Block_Header header;
    unsigned char raw_bytes[GC_BLOCK_SIZE_BYTES];
}Block;

#define GC_BLOCK_HEADER_VARS_SIZE_BYTES (unsigned int)&(((Block_Header*)0)->mark_table)

/* BlockSize - MarkbitTable*32 = HeaderVars + MarkbitTable
   => MarkbitTable = (BlockSize - HeaderVars)/33 */
#define MARKBIT_TABLE_COMPUTE_DIVISOR 33
/* +1 to round up*/
#define MARKBIT_TABLE_COMPUTED_SIZE_BYTE ((GC_BLOCK_SIZE_BYTES-GC_BLOCK_HEADER_VARS_SIZE_BYTES)/MARKBIT_TABLE_COMPUTE_DIVISOR + 1)
#define MARKBIT_TABLE_SIZE_WORDS ((MARKBIT_TABLE_COMPUTED_SIZE_BYTE + MASK_OF_BYTES_PER_WORD)&~MASK_OF_BYTES_PER_WORD)
#define MARKBIT_TABLE_SIZE_BYTES (MARKBIT_TABLE_SIZE_WORDS * BYTES_PER_WORD)

#define GC_BLOCK_HEADER_SIZE_BYTES (MARKBIT_TABLE_SIZE_BYTES + GC_BLOCK_HEADER_VARS_SIZE_BYTES)
#define GC_BLOCK_BODY_SIZE_BYTES (GC_BLOCK_SIZE_BYTES - GC_BLOCK_HEADER_SIZE_BYTES)
#define GC_BLOCK_BODY(block) ((void*)((unsigned int)(block) + GC_BLOCK_HEADER_SIZE_BYTES))
#define GC_BLOCK_END(block) ((void*)((unsigned int)(block) + GC_BLOCK_SIZE_BYTES))

#define GC_BLOCK_LOW_MASK ((unsigned int)(GC_BLOCK_SIZE_BYTES - 1))
#define GC_BLOCK_HIGH_MASK (~GC_BLOCK_LOW_MASK)
#define GC_BLOCK_HEADER(addr) ((Block_Header *)((unsigned int)(addr) & GC_BLOCK_HIGH_MASK))
#define GC_BLOCK_INDEX(addr) ((unsigned int)(GC_BLOCK_HEADER(addr)->block_idx))
#define GC_BLOCK_INDEX_FROM(heap_start, addr) ((unsigned int)(((unsigned int)(addr)-(unsigned int)(heap_start)) >> GC_BLOCK_SHIFT_COUNT))

#define ADDRESS_OFFSET_TO_BLOCK_HEADER(addr) ((unsigned int)((unsigned int)addr&GC_BLOCK_LOW_MASK))
#define ADDRESS_OFFSET_IN_BLOCK_BODY(addr) ((unsigned int)(ADDRESS_OFFSET_TO_BLOCK_HEADER(addr)- GC_BLOCK_HEADER_SIZE_BYTES))

#define OBJECT_BIT_INDEX_TO_MARKBIT_TABLE(p_obj)    (ADDRESS_OFFSET_IN_BLOCK_BODY(p_obj) >> 2)
#define OBJECT_WORD_INDEX_TO_MARKBIT_TABLE(p_obj)   (OBJECT_BIT_INDEX_TO_MARKBIT_TABLE(p_obj) >> BIT_SHIFT_TO_BITS_PER_WORD)
#define OBJECT_WORD_OFFSET_IN_MARKBIT_TABLE(p_obj)  (OBJECT_BIT_INDEX_TO_MARKBIT_TABLE(p_obj) & BIT_MASK_TO_BITS_PER_WORD)

inline Partial_Reveal_Object* block_get_first_marked_object(Block_Header* block, unsigned int* mark_bit_idx)
{
  unsigned int* mark_table = block->mark_table;
  unsigned int* table_end = mark_table + MARKBIT_TABLE_SIZE_WORDS;
  
  unsigned j=0;
  unsigned int k=0;
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    while(k<32){
        if( !(markbits& (1<<k)) ){ k++; continue;}
        unsigned int word_index = (j<<BIT_SHIFT_TO_BITS_PER_WORD) + k;
        Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)((unsigned int*)GC_BLOCK_BODY(block) + word_index);
        assert(obj_is_marked_in_vt(p_obj)); 
        
        *mark_bit_idx = word_index;
      return p_obj;
    }
    j++;
    k=0;
  }          
  *mark_bit_idx = 0;
  return NULL;   
}

inline Partial_Reveal_Object* block_get_next_marked_object(Block_Header* block, unsigned int* mark_bit_idx)
{
  unsigned int* mark_table = block->mark_table;
  unsigned int* table_end = mark_table + MARKBIT_TABLE_SIZE_WORDS;
  unsigned int bit_index = *mark_bit_idx;
  
  unsigned int j = bit_index >> BIT_SHIFT_TO_BITS_PER_WORD;
  unsigned int k = (bit_index & BIT_MASK_TO_BITS_PER_WORD) + 1;  
     
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    while(k<32){
      if( !(markbits& (1<<k)) ){ k++; continue;}
      
      unsigned int word_index = (j<<BIT_SHIFT_TO_BITS_PER_WORD) + k;
      Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)((unsigned int*)GC_BLOCK_BODY(block) + word_index);      
      assert(obj_is_marked_in_vt(p_obj));
      
      *mark_bit_idx = word_index;
      return p_obj;
    }
    j++;
    k=0;
  }        
  
  *mark_bit_idx = 0;
  return NULL;   

}

inline void block_clear_mark_table(Block_Header* block)
{
  unsigned int* mark_table = block->mark_table;
  memset(mark_table, 0, MARKBIT_TABLE_SIZE_BYTES);
  return;
}

inline void block_clear_markbits(Block_Header* block)
{
  unsigned int* mark_table = block->mark_table;
  unsigned int* table_end = mark_table + MARKBIT_TABLE_SIZE_WORDS;
  
  unsigned j=0;
  while( (mark_table + j) < table_end){
    unsigned int markbits = *(mark_table+j);
    if(!markbits){ j++; continue; }
    unsigned int k=0;
    while(k<32){
        if( !(markbits& (1<<k)) ){ k++; continue;}
        unsigned int word_index = (j<<BIT_SHIFT_TO_BITS_PER_WORD) + k;
        Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)((unsigned int*)GC_BLOCK_BODY(block) + word_index);
        assert(obj_is_marked_in_vt(p_obj));
        obj_unmark_in_vt(p_obj);
        k++;
    }
    j++;
  } 

  block_clear_mark_table(block);
  return;     
}

typedef struct Blocked_Space {
  /* <-- first couple of fields are overloadded as Space */
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  GC* gc;
  Boolean move_object;
  Boolean (*mark_object_func)(Space* space, Partial_Reveal_Object* p_obj);
  /* END of Space --> */

  Block* blocks; /* short-cut for mpsace blockheader access, not mandatory */
  
  /* FIXME:: the block indices should be replaced with block header addresses */
  unsigned int first_block_idx;
  unsigned int ceiling_block_idx;
  volatile unsigned int free_block_idx;
  
  unsigned int num_used_blocks;
  unsigned int num_managed_blocks;
  unsigned int num_total_blocks;
  /* END of Blocked_Space --> */
}Blocked_Space;

#endif //#ifndef _BLOCK_H_
