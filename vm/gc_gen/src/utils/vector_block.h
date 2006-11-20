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
  unsigned int* start; /* point to first entry, not needed actually */
  unsigned int* end;   /* point to end of the block (right after the last entry) */
  unsigned int* head;  /* point to the first filled entry */
  unsigned int* tail;  /* point to the entry after the last filled one */
  unsigned int* entries[1];
}Vector_Block;

inline void vector_block_init(Vector_Block* block, unsigned int size)
{
    block->start = (unsigned int*)block->entries;
    block->end = (unsigned int*)((unsigned int)block + size);
    block->head = block->start;
    block->tail = block->start;
    return;  
}

inline unsigned int vector_block_entry_count(Vector_Block* block)
{ return (unsigned int)(block->tail - block->head); }

inline Boolean vector_block_is_full(Vector_Block* block)
{ return block->tail == block->end; }

inline void vector_block_add_entry(Vector_Block* block, unsigned int value)
{ 
  assert(value && !*(block->tail));
  *(block->tail++) = value; 
}

inline void vector_block_clear(Vector_Block* block)
{
#ifdef _DEBUG
  memset(block->start, 0, (block->end - block->start)*BYTES_PER_WORD);
#endif

  block->tail = block->head; 
}

/* Below is for sequential local access */
inline unsigned int* vector_block_iterator_init(Vector_Block* block)
{  return block->head;  }

inline unsigned int* vector_block_iterator_advance(Vector_Block* block, unsigned int* iter)
{  return ++iter; }

inline Boolean vector_block_iterator_end(Vector_Block* block, unsigned int* iter)
{  return iter == block->tail; }

#endif /* #ifndef _VECTOR_BLOCK_H_ */
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
  unsigned int* start; /* point to first entry, not needed actually */
  unsigned int* end;   /* point to end of the block (right after the last entry) */
  unsigned int* head;  /* point to the first filled entry */
  unsigned int* tail;  /* point to the entry after the last filled one */
  unsigned int* entries[1];
}Vector_Block;

inline void vector_block_init(Vector_Block* block, unsigned int size)
{
    block->start = (unsigned int*)block->entries;
    block->end = (unsigned int*)((unsigned int)block + size);
    block->head = block->start;
    block->tail = block->start;
    return;  
}

inline unsigned int vector_block_entry_count(Vector_Block* block)
{ return (unsigned int)(block->tail - block->head); }

inline Boolean vector_block_is_full(Vector_Block* block)
{ return block->tail == block->end; }

inline void vector_block_add_entry(Vector_Block* block, unsigned int value)
{ 
  assert(value && !*(block->tail));
  *(block->tail++) = value; 
}

inline void vector_block_clear(Vector_Block* block)
{
#ifdef _DEBUG
  memset(block->start, 0, (block->end - block->start)*BYTES_PER_WORD);
#endif

  block->tail = block->head; 
}

/* Below is for sequential local access */
inline unsigned int* vector_block_iterator_init(Vector_Block* block)
{  return block->head;  }

inline unsigned int* vector_block_iterator_advance(Vector_Block* block, unsigned int* iter)
{  return ++iter; }

inline Boolean vector_block_iterator_end(Vector_Block* block, unsigned int* iter)
{  return iter == block->tail; }

#endif /* #ifndef _VECTOR_BLOCK_H_ */
