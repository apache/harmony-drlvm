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

#ifndef _MSC_COLLECT_H_
#define _MSC_COLLECT_H_

#include "mspace.h"

inline Block_Header* mspace_get_first_block_for_nos(Mspace* mspace, unsigned int* target_blk_idx)
{  
  unsigned int index = mspace->free_block_idx;
  *target_blk_idx = index;
  return mspace->block_info[index].block;
}

inline Block_Header* mspace_get_next_block_for_nos(Mspace* mspace, unsigned int* target_blk_idx)
{  
  unsigned int index = *target_blk_idx;

  *target_blk_idx = ++index;
  
  if( index >= mspace->num_current_blocks) return NULL;
  
  return mspace->block_info[index].block; 
}

inline void block_clear_markbits(Block_Header* block)
{
  unsigned int* mark_table = block->mark_table;
  memset(mark_table, 0, MARKBIT_TABLE_SIZE_WORDS*BYTES_PER_WORD);
  return;
}

inline Block_Header* mspace_get_first_compact_block(Mspace* mspace, unsigned int* compact_blk_idx)
{  
  assert( mspace->block_info[0].status != BLOCK_FREE );

  *compact_blk_idx = 0;
  return mspace->block_info[0].block;
}

inline Block_Header* mspace_get_next_compact_block(Mspace* mspace, unsigned int* compact_blk_idx)
{  
  unsigned int index = *compact_blk_idx;
 
  if( ++index == mspace->num_used_blocks ) return NULL;

  *compact_blk_idx = index;
  assert( mspace->block_info[index].status != BLOCK_FREE );

  return mspace->block_info[index].block; 
}

inline Block_Header* mspace_get_first_target_block(Mspace* mspace, unsigned int* target_blk_idx)
{  
  assert( mspace->block_info[0].status != BLOCK_FREE);

  *target_blk_idx = 0;
  return mspace->block_info[0].block;
}

inline Block_Header* mspace_get_next_target_block(Mspace* mspace, unsigned int* target_blk_idx)
{  
  unsigned int index = *target_blk_idx;

  *target_blk_idx = ++index;
  /* trick! free_block_idx is changed after computing target addresses */
  assert( mspace->block_info[index].status != BLOCK_FREE && (index < mspace->free_block_idx));
  
  return mspace->block_info[index].block; 
  
}

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
        /* only valid before compaction: assert(obj_is_marked_in_vt(p_obj)); */
        
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
      /* only valid before compaction: assert(obj_is_marked_in_vt(p_obj)); */
      
      *mark_bit_idx = word_index;
      return p_obj;
    }
    j++;
    k=0;
  }        
  
  *mark_bit_idx = 0;
  return NULL;   

}

#endif /* #ifndef _MSC_COLLECT_H_ */