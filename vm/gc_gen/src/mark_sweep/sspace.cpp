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

#include "sspace.h"
#include "sspace_chunk.h"
#include "../gen/gen.h"
#include "../common/gc_space.h"
#include "sspace_verify.h"

struct GC_Gen;

void sspace_initialize(GC *gc, void *start, unsigned int sspace_size, unsigned int commit_size)
{
  /* With sspace in the heap, the heap must be composed of a single sspace or a sspace and a NOS.
   * In either case, the reserved size and committed size of sspace must be the same.
   * Because sspace has only mark-sweep collection, it is not possible to shrink sspace.
   * So there is no need to use dynamic space resizing.
   */
  assert(sspace_size == commit_size);
  
  Sspace *sspace = (Sspace*)STD_MALLOC(sizeof(Sspace));
  assert(sspace);
  memset(sspace, 0, sizeof(Sspace));
  
  sspace->reserved_heap_size = sspace_size;
  
  void *reserved_base = start;
  
  /* commit sspace mem */
  if(!large_page_hint)
    vm_commit_mem(reserved_base, commit_size);
  memset(reserved_base, 0, commit_size);
  sspace->committed_heap_size = commit_size;
  
  sspace->heap_start = reserved_base;
  sspace->heap_end = (void *)((POINTER_SIZE_INT)reserved_base + sspace_size);
    
  sspace->num_collections = 0;
  sspace->time_collections = 0;
  sspace->survive_ratio = 0.2f;

  sspace->move_object = FALSE;
  sspace->gc = gc;
  
  sspace_init_chunks(sspace);
  
  gc_set_pos((GC_Gen*)gc, (Space*)sspace);
#ifdef SSPACE_VERIFY
  sspace_verify_init(gc);
#endif
  return;
}

static void sspace_destruct_chunks(Sspace *sspace) { return; }

void sspace_destruct(Sspace *sspace)
{
  //FIXME:: when map the to-half, the decommission start address should change
  sspace_destruct_chunks(sspace);
  STD_FREE(sspace);
}

void mutator_init_small_chunks(Mutator *mutator)
{
  unsigned int size = sizeof(Chunk_Header*) * (SMALL_LOCAL_CHUNK_NUM + MEDIUM_LOCAL_CHUNK_NUM);
  Chunk_Header **chunks = (Chunk_Header**)STD_MALLOC(size);
  memset(chunks, 0, size);
  mutator->small_chunks = chunks;
  mutator->medium_chunks = chunks + SMALL_LOCAL_CHUNK_NUM;
}

extern void mark_sweep_sspace(Collector *collector);

void sspace_collection(Sspace *sspace) 
{
  GC *gc = sspace->gc;
  sspace->num_collections++;
  
#ifdef SSPACE_ALLOC_INFO
  sspace_alloc_info_summary();
#endif
#ifdef SSPACE_CHUNK_INFO
  sspace_chunks_info(sspace, TRUE);
#endif

#ifdef SSPACE_VERIFY
  sspace_verify_vtable_mark(gc);
#endif

#ifdef SSPACE_TIME
  sspace_gc_time(gc, TRUE);
#endif

  pool_iterator_init(gc->metadata->gc_rootset_pool);
  sspace_clear_chunk_list(gc);
  
  collector_execute_task(gc, (TaskType)mark_sweep_sspace, (Space*)sspace);

#ifdef SSPACE_TIME
  sspace_gc_time(gc, FALSE);
#endif

#ifdef SSPACE_CHUNK_INFO
  sspace_chunks_info(sspace, FALSE);
#endif

}
