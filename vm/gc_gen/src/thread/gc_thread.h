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

#ifndef _GC_THREAD_H_
#define _GC_THREAD_H_

#include "../common/gc_space.h"
#include "../common/gc_metadata.h"

#define ALLOC_ZEROING
#define ZEROING_SIZE  2*KB

extern POINTER_SIZE_INT tls_gc_offset;

inline void* gc_get_tls()
{ 
  void* tls_base = vm_thread_local();
  return (void*)*(POINTER_SIZE_INT*)((char*)tls_base + tls_gc_offset);
}

inline void gc_set_tls(void* gc_tls_info)
{ 
  void* tls_base = vm_thread_local();
  *(POINTER_SIZE_INT*)((char*)tls_base + tls_gc_offset) = (POINTER_SIZE_INT)gc_tls_info;
}

/* NOTE:: don't change the position of free/ceiling, because the offsets are constants for inlining */
typedef struct Allocator{
  void *free;
  void *ceiling;
  void* end;
  Block *alloc_block;
  Space* alloc_space;
  GC   *gc;
  VmThreadHandle thread_handle;   /* This thread; */
}Allocator;

inline void thread_local_unalloc(unsigned int size, Allocator* allocator)
{
  void* free = allocator->free;    
  allocator->free = (void*)((POINTER_SIZE_INT)free - size);
  return;
}

#ifdef ALLOC_ZEROING

inline Partial_Reveal_Object* thread_local_alloc_zeroing(unsigned int size, Allocator* allocator)
{
  POINTER_SIZE_INT free = (POINTER_SIZE_INT)allocator->free;
  POINTER_SIZE_INT ceiling = (POINTER_SIZE_INT)allocator->ceiling;
  
  POINTER_SIZE_INT new_free = free + size;
  
  POINTER_SIZE_INT block_ceiling = (POINTER_SIZE_INT)allocator->end;
  if( new_free > block_ceiling) 
    return NULL;

  POINTER_SIZE_INT new_ceiling;
  new_ceiling =  new_free + ZEROING_SIZE;
  if( new_ceiling > block_ceiling )
    new_ceiling = block_ceiling;

  allocator->ceiling = (void*)new_ceiling;
  allocator->free = (void*)new_free;
  memset((void*)ceiling, 0, new_ceiling - ceiling);
  return (Partial_Reveal_Object*)free;

}

#endif /* ALLOC_ZEROING */

inline Partial_Reveal_Object* thread_local_alloc(unsigned int size, Allocator* allocator)
{
  POINTER_SIZE_INT free = (POINTER_SIZE_INT)allocator->free;
  POINTER_SIZE_INT ceiling = (POINTER_SIZE_INT)allocator->ceiling;
  
  POINTER_SIZE_INT new_free = free + size;
    
  if (new_free <= ceiling){
  	allocator->free= (void*)new_free;
    return (Partial_Reveal_Object*)free;
  }

#ifndef ALLOC_ZEROING
  
  return NULL;

#else

  return thread_local_alloc_zeroing(size, allocator);

#endif /* #ifndef ALLOC_ZEROING */

}

inline void allocator_init_free_block(Allocator* allocator, Block_Header* alloc_block)
{
    assert(alloc_block->status == BLOCK_FREE);
    alloc_block->status = BLOCK_IN_USE;
    
    /* set allocation context */
    void* new_free = alloc_block->free;
    allocator->free = new_free;

#ifndef ALLOC_ZEROING

    allocator->ceiling = alloc_block->ceiling;
    memset(new_free, 0, GC_BLOCK_BODY_SIZE_BYTES);

#else
    /* the first-time zeroing area includes block header, to make subsequent allocs page aligned */
    unsigned int zeroing_size = ZEROING_SIZE - GC_BLOCK_HEADER_SIZE_BYTES;
    allocator->ceiling = (void*)((POINTER_SIZE_INT)new_free + zeroing_size);
    memset(new_free, 0, zeroing_size);

#endif /* #ifndef ALLOC_ZEROING */

    allocator->end = alloc_block->ceiling;
    allocator->alloc_block = (Block*)alloc_block; 
    
    return;
}

inline void alloc_context_reset(Allocator* allocator)
{
  Block_Header* block = (Block_Header*)allocator->alloc_block;
  /* it can be NULL if GC happens before the mutator resumes, or called by collector */
  if( block != NULL ){ 
    assert(block->status == BLOCK_IN_USE);
    block->free = allocator->free;
    block->status = BLOCK_USED;
    allocator->alloc_block = NULL;
  }
    
   allocator->free = NULL;
   allocator->ceiling = NULL;
   allocator->end = NULL;
  
  return;
}

#endif /* #ifndef _GC_THREAD_H_ */
