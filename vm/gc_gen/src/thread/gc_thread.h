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

#include "../common/gc_block.h"
#include "../common/gc_metadata.h"

extern unsigned int tls_gc_offset;

inline void* gc_get_tls()
{ 
  void* tls_base = vm_thread_local();
  return (void*)*(unsigned int*)((char*)tls_base + tls_gc_offset);
}

inline void gc_set_tls(void* gc_tls_info)
{ 
  void* tls_base = vm_thread_local();
  *(unsigned int*)((char*)tls_base + tls_gc_offset) = (unsigned int)gc_tls_info;
}

/* NOTE:: don't change the position of free/ceiling, because the offsets are constants for inlining */
typedef struct Allocator{
  void *free;
  void *ceiling;
  Block *alloc_block;
  Space* alloc_space;
  GC   *gc;
  VmThreadHandle thread_handle;   /* This thread; */
}Allocator;

inline Partial_Reveal_Object* thread_local_alloc(unsigned int size, Allocator* allocator)
{
    void* free = allocator->free;
    void* ceiling = allocator->ceiling;
    
    void* new_free = (void*)((unsigned int)free + size);
    
    if (new_free <= ceiling){
    	allocator->free= new_free;
    	return (Partial_Reveal_Object*)free;
    }

    return NULL;
}

inline void alloc_context_reset(Allocator* allocator)
{
  allocator->free = NULL;
  allocator->ceiling = NULL;
  allocator->alloc_block = NULL;
  
  return;
}

#endif /* #ifndef _GC_THREAD_H_ */
