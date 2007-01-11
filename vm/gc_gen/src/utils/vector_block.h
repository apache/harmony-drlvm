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
 * @author Xiao-Feng Li, 2006/10/25
 */
 
#ifndef _VECTOR_BLOCK_H_
#define _VECTOR_BLOCK_H_

typedef struct Vector_Block{
  void* next; /* point to next block */
  unsigned int* head;  /* point to the first filled entry */
  unsigned int* tail;  /* point to the entry after the last filled one */
  unsigned int* heap_end;   /* point to heap_end of the block (right after the last entry) */
  unsigned int entries[1];
}Vector_Block;


/* this size better be 2's power */
#define VECTOR_BLOCK_DATA_SIZE_BYTES (2*KB)

#define VECTOR_BLOCK_HEADER_SIZE_BYTES ((unsigned int)((Vector_Block*)0)->entries)
#define VECTOR_BLOCK_ENTRY_NUM ((VECTOR_BLOCK_DATA_SIZE_BYTES - VECTOR_BLOCK_HEADER_SIZE_BYTES) >> BIT_SHIFT_TO_BYTES_PER_WORD)

inline void vector_block_init(Vector_Block* block, unsigned int size)
{
  block->heap_end = (unsigned int*)((unsigned int)block + size);
  block->head = (unsigned int*)block->entries;
  block->tail = (unsigned int*)block->entries;
  memset(block->entries, 0, (block->heap_end - (unsigned int*)block->entries)*BYTES_PER_WORD);
  return;  
}

inline unsigned int vector_block_entry_count(Vector_Block* block)
{ return (unsigned int)(block->tail - block->head); }

/*
inline Boolean vector_block_is_full(Vector_Block* block)
{ return block->tail == block->heap_end; }

inline Boolean vector_block_is_empty(Vector_Block* block)
{ return block->tail == block->head; }
*/

inline Boolean vector_block_is_full(Vector_Block* block)
{ return (block->tail - block->entries) == VECTOR_BLOCK_ENTRY_NUM; }

inline Boolean vector_block_is_empty(Vector_Block* block)
{ return block->tail == block->entries; }

inline void vector_block_add_entry(Vector_Block* block, unsigned int value)
{
#ifdef _DEBUG 
  assert(value && !*(block->tail));
#endif

  *(block->tail++) = value; 
}

inline void vector_block_clear(Vector_Block* block)
{
  block->head = (unsigned int*)block->entries;
  block->tail = (unsigned int*)block->entries;
#ifdef _DEBUG
  memset(block->entries, 0, (block->heap_end - (unsigned int*)block->entries)*BYTES_PER_WORD);
#endif
}

/* Below is for sequential local access */
inline unsigned int* vector_block_iterator_init(Vector_Block* block)
{  return block->head;  }

inline unsigned int* vector_block_iterator_advance(Vector_Block* block, unsigned int* iter)
{  return ++iter; }

inline Boolean vector_block_iterator_end(Vector_Block* block, unsigned int* iter)
{  return iter == block->tail; }


/* Below is to use Vector_Block as stack (for trace-forwarding DFS order ) */
inline void vector_stack_init(Vector_Block* block)
{ 
  block->tail = block->heap_end;
  block->head = block->heap_end;  
}

inline void vector_stack_clear(Vector_Block* block)
{
  vector_stack_init(block);
#ifdef _DEBUG
  memset(block->entries, 0, (block->heap_end - (unsigned int*)block->entries)*BYTES_PER_WORD);
#endif
}

/*
inline Boolean vector_stack_is_empty(Vector_Block* block)
{  return (block->head == block->tail); }
*/

inline Boolean vector_stack_is_empty(Vector_Block* block)
{ return (block->head - block->entries) == VECTOR_BLOCK_ENTRY_NUM; }

inline Boolean vector_stack_is_full(Vector_Block* block)
{  return (block->head == block->entries); }

inline void vector_stack_push(Vector_Block* block, unsigned int value)
{ 
  block->head--;
#ifdef _DEBUG
  assert(value && !*(block->head));
#endif
  *(block->head) = value;
}

inline unsigned int vector_stack_pop(Vector_Block* block)
{   
  unsigned int value = *block->head;
#ifdef _DEBUG
  *block->head = 0;
#endif
  block->head++;
  return value;
}

inline void vector_block_integrity_check(Vector_Block* block)
{
  unsigned int* iter = vector_block_iterator_init(block);
  while(!vector_block_iterator_end(block, iter)){
    assert(*iter);
    iter = vector_block_iterator_advance(block, iter);
  }    
  return;
}

#endif /* #ifndef _VECTOR_BLOCK_H_ */
