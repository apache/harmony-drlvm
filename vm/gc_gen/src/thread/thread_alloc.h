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

#ifndef _THREAD_ALLOC_H_
#define _THREAD_ALLOC_H_

#include "../common/gc_block.h"
#include "../common/gc_metadata.h"

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
    Partial_Reveal_Object* p_return_obj=(Partial_Reveal_Object*)allocator->free;
    unsigned int new_free = size+(unsigned int)p_return_obj;
		    
    if (new_free <= (unsigned int)allocator->ceiling){
    	allocator->free=(void*)new_free;
    	return p_return_obj;
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

#endif /* #ifndef _THREAD_ALLOC_H_ */
