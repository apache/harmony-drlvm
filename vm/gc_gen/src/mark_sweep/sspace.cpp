/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
#include "sspace_verify.h"
#include "gc_ms.h"
#include "../gen/gen.h"

struct GC_Gen;

Sspace *sspace_initialize(GC *gc, void *start, POINTER_SIZE_INT sspace_size, POINTER_SIZE_INT commit_size)
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

  sspace->space_statistic = (Space_Statistics*)STD_MALLOC(sizeof(Space_Statistics));
  assert(sspace->space_statistic);
  memset(sspace->space_statistic, 0, sizeof(Space_Statistics));

#ifdef USE_MARK_SWEEP_GC
  gc_ms_set_sspace((GC_MS*)gc, sspace);
#else
  gc_set_mos((GC_Gen*)gc, (Space*)sspace);
#endif

#ifdef SSPACE_VERIFY
  sspace_verify_init(gc);
#endif
  return sspace;
}

static void sspace_destruct_chunks(Sspace *sspace) { return; }

void sspace_destruct(Sspace *sspace)
{
  //FIXME:: when map the to-half, the decommission start address should change
  sspace_destruct_chunks(sspace);
  STD_FREE(sspace);
}

void sspace_reset_after_collection(Sspace *sspace)
{
  sspace->move_object = FALSE;
  sspace->need_compact = FALSE;
  sspace->need_fix = FALSE;
}

void allocator_init_local_chunks(Allocator *allocator)
{
  Sspace *sspace = gc_get_sspace(allocator->gc);
  Size_Segment **size_segs = sspace->size_segments;
  
  /* Alloc mem for size segments (Chunk_Header**) */
  unsigned int seg_size = sizeof(Chunk_Header**) * SIZE_SEGMENT_NUM;
  Chunk_Header ***local_chunks = (Chunk_Header***)STD_MALLOC(seg_size);
  memset(local_chunks, 0, seg_size);
  
  /* Alloc mem for local chunk pointers */
  unsigned int chunk_ptr_size = 0;
  for(unsigned int i = SIZE_SEGMENT_NUM; i--;){
    if(size_segs[i]->local_alloc){
      chunk_ptr_size += size_segs[i]->chunk_num;
    }
  }
  chunk_ptr_size *= sizeof(Chunk_Header*);
  Chunk_Header **chunk_ptrs = (Chunk_Header**)STD_MALLOC(chunk_ptr_size);
  memset(chunk_ptrs, 0, chunk_ptr_size);
  
  for(unsigned int i = 0; i < SIZE_SEGMENT_NUM; ++i){
    if(size_segs[i]->local_alloc){
      local_chunks[i] = chunk_ptrs;
      chunk_ptrs += size_segs[i]->chunk_num;
    }
  }
  
  allocator->local_chunks = local_chunks;
}

void allocator_clear_local_chunks(Allocator *allocator, Boolean reuse_pfc)
{
  Sspace *sspace = gc_get_sspace(allocator->gc);
  Size_Segment **size_segs = sspace->size_segments;
  Chunk_Header ***local_chunks = allocator->local_chunks;
  
  for(unsigned int i = SIZE_SEGMENT_NUM; i--;){
    if(!size_segs[i]->local_alloc){
      assert(!local_chunks[i]);
      continue;
    }
    Chunk_Header **chunks = local_chunks[i];
    assert(chunks);
    for(unsigned int j = size_segs[i]->chunk_num; j--;){
      if(chunks[j] && reuse_pfc)
        sspace_put_pfc(sspace, chunks[j]);
      chunks[j] = NULL;
    }
  }
}

void allocactor_destruct_local_chunks(Allocator *allocator)
{
  Sspace *sspace = gc_get_sspace(allocator->gc);
  Size_Segment **size_segs = sspace->size_segments;
  Chunk_Header ***local_chunks = allocator->local_chunks;
  Chunk_Header **chunk_ptrs = NULL;
  unsigned int chunk_ptr_num = 0;
  
  /* Find local chunk pointers' head and their number */
  for(unsigned int i = 0; i < SIZE_SEGMENT_NUM; ++i){
    if(size_segs[i]->local_alloc){
      chunk_ptr_num += size_segs[i]->chunk_num;
      assert(local_chunks[i]);
      if(!chunk_ptrs)
        chunk_ptrs = local_chunks[i];
    }
  }
  
  /* Put local pfc to the according pools */
  for(unsigned int i = 0; i < chunk_ptr_num; ++i){
    if(chunk_ptrs[i])
      sspace_put_pfc(sspace, chunk_ptrs[i]);
  }
  
  /* Free mem for local chunk pointers */
  STD_FREE(chunk_ptrs);
  
  /* Free mem for size segments (Chunk_Header**) */
  STD_FREE(local_chunks);
}

#ifdef USE_MARK_SWEEP_GC
void sspace_set_space_statistic(Sspace *sspace)
{
  GC_MS* gc = (GC_MS*)sspace->gc;

  for(unsigned int i=0; i<gc->num_collectors; ++i){
    sspace->surviving_obj_num += gc->collectors[i]->live_obj_num;
    sspace->surviving_obj_size += gc->collectors[i]->live_obj_size;
  }
}
#endif

extern void sspace_decide_compaction_need(Sspace *sspace);
extern void mark_sweep_sspace(Collector *collector);

void sspace_collection(Sspace *sspace) 
{
  GC *gc = sspace->gc;
  sspace->num_collections++;
  
#ifdef SSPACE_ALLOC_INFO
  sspace_alloc_info_summary();
#endif
#ifdef SSPACE_CHUNK_INFO
  sspace_chunks_info(sspace, FALSE);
#endif

  sspace_decide_compaction_need(sspace);
  if(sspace->need_compact && gc_match_kind(gc, MARK_SWEEP_GC)){
    assert(gc_match_kind(gc, MS_COLLECTION));
    gc->collect_kind = MS_COMPACT_COLLECTION;
  }
  if(sspace->need_compact || gc_match_kind(gc, MAJOR_COLLECTION))
    sspace->need_fix = TRUE;
  //printf("\n\n>>>>>>>>%s>>>>>>>>>>>>\n\n", sspace->need_compact ? "SWEEP COMPACT" : "MARK SWEEP");
#ifdef SSPACE_VERIFY
  sspace_verify_before_collection(gc);
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
