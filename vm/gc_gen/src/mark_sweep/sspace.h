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

#ifndef _SWEEP_SPACE_H_
#define _SWEEP_SPACE_H_

#include "../thread/gc_thread.h"
#include "../thread/collector_alloc.h"
#include "../thread/mutator.h"
#include "../common/gc_common.h"

/*
 * The sweep space accomodates objects collected by mark-sweep
 */

#define ONLY_SSPACE_IN_HEAP

struct Free_Chunk_List;

typedef struct Sspace {
  /* <-- first couple of fields are overloadded as Space */
  void *heap_start;
  void *heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  int64 time_collections;
  float survive_ratio;
  unsigned int collect_algorithm;
  GC *gc;
  Boolean move_object;
  /* Size allocted after last collection. Not available in fspace now. */
  unsigned int alloced_size;
  /* For_statistic: not available now for fspace */
  unsigned int surviving_size;
  /* END of Space --> */
  
  Pool **small_pfc_pools;
  Pool **medium_pfc_pools;
  Pool **large_pfc_pools;
  Free_Chunk_List *aligned_free_chunk_lists;
  Free_Chunk_List *unaligned_free_chunk_lists;
  Free_Chunk_List *hyper_free_chunk_list;
} Sspace;

void sspace_initialize(GC *gc, void *start, unsigned int sspace_size, unsigned int commit_size);
void sspace_destruct(Sspace *sspace);

void *sspace_fast_alloc(unsigned size, Allocator *allocator);
void *sspace_alloc(unsigned size, Allocator *allocator);

void sspace_reset_for_allocation(Sspace *sspace);

void sspace_collection(Sspace *sspace);

void mutator_init_small_chunks(Mutator *mutator);
void collector_init_free_chunk_list(Collector *collector);

POINTER_SIZE_INT sspace_free_memory_size(Sspace *sspace);

#endif // _SWEEP_SPACE_H_
