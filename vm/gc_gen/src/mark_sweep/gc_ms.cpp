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

#include "../common/gc_common.h"

#ifdef USE_MARK_SWEEP_GC

#include "gc_ms.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../common/compressed_ref.h"
#include "../thread/marker.h"
#include "../verify/verify_live_heap.h"
#ifdef USE_32BITS_HASHCODE
#include "../common/hashcode.h"
#endif


void gc_ms_initialize(GC_MS *gc_ms, POINTER_SIZE_INT min_heap_size, POINTER_SIZE_INT max_heap_size)
{
  assert(gc_ms);
  
  max_heap_size = round_down_to_size(max_heap_size, SPACE_ALLOC_UNIT);
  min_heap_size = round_up_to_size(min_heap_size, SPACE_ALLOC_UNIT);
  assert(max_heap_size <= max_heap_size_bytes);
  assert(max_heap_size >= min_heap_size_bytes);
  
  void *sspace_base;
  sspace_base = vm_reserve_mem(0, max_heap_size);
  sspace_initialize((GC*)gc_ms, sspace_base, max_heap_size, max_heap_size);
  
  HEAP_NULL = (POINTER_SIZE_INT)sspace_base;
  
  gc_ms->heap_start = sspace_base;
  gc_ms->heap_end = (void*)((POINTER_SIZE_INT)sspace_base + max_heap_size);
  gc_ms->reserved_heap_size = max_heap_size;
  gc_ms->committed_heap_size = max_heap_size;
  gc_ms->num_collections = 0;
  gc_ms->time_collections = 0;
}

void gc_ms_destruct(GC_MS *gc_ms)
{
  Sspace *sspace = gc_ms->sspace;
  void *sspace_start = sspace->heap_start;
  sspace_destruct(sspace);
  gc_ms->sspace = NULL;
  vm_unmap_mem(sspace_start, space_committed_size((Space*)sspace));
}

void gc_ms_reclaim_heap(GC_MS *gc)
{
  if(verify_live_heap) gc_verify_heap((GC*)gc, TRUE);
  
  Sspace *sspace = gc_ms_get_sspace(gc);
  
  sspace_collection(sspace);
  
  sspace_reset_after_collection(sspace);
  
  if(verify_live_heap) gc_verify_heap((GC*)gc, FALSE);
}

void sspace_mark_scan_concurrent(Marker* marker);
void gc_ms_start_concurrent_mark(GC_MS* gc, unsigned int num_markers)
{
  if(gc->num_active_markers == 0)
    pool_iterator_init(gc->metadata->gc_rootset_pool);
  
  marker_execute_task_concurrent((GC*)gc,(TaskType)sspace_mark_scan_concurrent,(Space*)gc->sspace, num_markers);
}

void gc_ms_start_concurrent_mark(GC_MS* gc)
{
  pool_iterator_init(gc->metadata->gc_rootset_pool);
  
  marker_execute_task_concurrent((GC*)gc,(TaskType)sspace_mark_scan_concurrent,(Space*)gc->sspace);
}

void gc_ms_update_space_statistics(GC_MS* gc)
{
  POINTER_SIZE_INT num_live_obj = 0;
  POINTER_SIZE_INT size_live_obj = 0;
  
  Space_Statistics* sspace_stat = gc->sspace->space_statistic;

  unsigned int num_collectors = gc->num_active_collectors;
  Collector** collectors = gc->collectors;
  unsigned int i;
  for(i = 0; i < num_collectors; i++){
    Collector* collector = collectors[i];
    num_live_obj += collector->live_obj_num;
    size_live_obj += collector->live_obj_size;
  }

  sspace_stat->num_live_obj = num_live_obj;
  sspace_stat->size_live_obj = size_live_obj;  
  sspace_stat->last_size_free_space = sspace_stat->size_free_space;
  sspace_stat->size_free_space = gc->committed_heap_size - size_live_obj;/*TODO:inaccurate value.*/
}

void gc_ms_iterate_heap(GC_MS *gc)
{
}

#endif // USE_MARK_SWEEP_GC
