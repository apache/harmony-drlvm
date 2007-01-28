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
 * @author Ji, Qi, 2006/10/25
 */
 
#ifndef _BIT_OPS_H_
#define _BIT_OPS_H_

#include "../common/gc_common.h"

inline unsigned int word_get_first_set_lsb(unsigned int target_word)
{
  assert(target_word != 0);
  unsigned int bit_offset;

#ifdef _EM64T_

#else /* ifdef _EM64T_*/
#ifdef PLATFORM_POSIX  /* POSIX Platform*/
  __asm__ __volatile__(
    "bsf %1,%0\n"
    :"=r"(bit_offset)
    :"m"(target_word)
  );
#else /*WIN32 Platform*/
  __asm{
    bsf eax, target_word
    mov bit_offset, eax
  }
#endif /* ifdef PLATFORM_POSIX else*/
#endif /* ifdef _EM64T_ else */
  return bit_offset;

}

inline unsigned int words_get_next_set_lsb(unsigned int* words, unsigned int count, unsigned int start_idx)
{
  unsigned int bit_offset;
  
  assert(start_idx < 128);
  
  unsigned int start_word_index = start_idx >> BIT_SHIFT_TO_BITS_PER_WORD;
  unsigned int start_bit_offset = start_idx & BIT_MASK_TO_BITS_PER_WORD;
  
  bit_offset = start_idx - start_bit_offset;
  for(unsigned int i = start_word_index; i < count; i ++ ){
    unsigned int cur_word = *(words + i);
    
    if(start_word_index == i){
      unsigned int mask = ~((1 << start_bit_offset) - 1);
      cur_word = cur_word & mask;
    }
  
     if(cur_word != 0){
       bit_offset += word_get_first_set_lsb(cur_word);
       return bit_offset;
     }
     
     bit_offset += 32;
   }
  
  return bit_offset;
}

inline void words_set_bit(unsigned int* words, unsigned int count, unsigned int start_idx)
{
  assert(start_idx < 128);
  
  unsigned int word_index = start_idx >> BIT_SHIFT_TO_BITS_PER_WORD;  
  unsigned int bit_offset = start_idx & BIT_MASK_TO_BITS_PER_WORD;
  
  if(word_index >= count) return;
  
  unsigned int* p_word = words + word_index;
  unsigned int old_value = *p_word;
  unsigned int mask = 1 << bit_offset;
  unsigned int new_value = old_value|mask;
  
  *p_word = new_value;
  
  return;
}

inline void words_clear_bit(unsigned int* words, unsigned int count, unsigned int start_idx)
{
  assert(start_idx < 128);
  
  unsigned int word_index = start_idx >> BIT_SHIFT_TO_BITS_PER_WORD;
  unsigned int bit_offset = start_idx & BIT_MASK_TO_BITS_PER_WORD;
  
  if(word_index >= count) return;
  
  unsigned int* p_word = words + word_index;
  unsigned int old_value = *p_word;
  unsigned int mask = ~(1 << bit_offset);
  unsigned int new_value = old_value & mask;
  
  *p_word = new_value;
  
  return;
}
#endif
