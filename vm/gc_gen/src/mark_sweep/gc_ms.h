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

#ifndef _GC_MS_H_
#define _GC_MS_H_

#ifdef USE_MARK_SWEEP_GC

#include "wspace.h"


/* heap size limit is not interesting. only for manual tuning purpose */
extern POINTER_SIZE_INT min_heap_size_bytes;
extern POINTER_SIZE_INT max_heap_size_bytes;

typedef struct GC_MS {
  /* <-- First couple of fields overloaded as GC */
  void* physical_start;
  void *heap_start;
  void *heap_end;
  POINTER_SIZE_INT reserved_heap_size;
  POINTER_SIZE_INT committed_heap_size;
  unsigned int num_collections;
  Boolean in_collection;
  int64 time_collections;
  float survive_ratio;
  
  /* mutation related info */
  Mutator *mutator_list;
  SpinLock mutator_list_lock;
  unsigned int num_mutators;
  
  /* collection related info */
  Collector **collectors;
  unsigned int num_collectors;
  unsigned int num_active_collectors; /* not all collectors are working */
  
  Marker** markers;
  unsigned int num_markers;
  unsigned int num_active_markers;
  
  /* metadata is the pool for rootset, markstack, etc. */
  GC_Metadata *metadata;
  Finref_Metadata *finref_metadata;
  
  unsigned int collect_kind; /* MAJOR or MINOR */
  unsigned int last_collect_kind;
  unsigned int cause; /*GC_CAUSE_LOS_IS_FULL, GC_CAUSE_NOS_IS_FULL, or GC_CAUSE_RUNTIME_FORCE_GC*/
  Boolean collect_result; /* succeed or fail */
  
  Boolean generate_barrier;
  
  /* FIXME:: this is wrong! root_set belongs to mutator */
  Vector_Block *root_set;
  Vector_Block *weakroot_set;
  Vector_Block *uncompressed_root_set;
  
  //For_LOS_extend
  Space_Tuner *tuner;

  unsigned int gc_concurrent_status;
  Collection_Scheduler* collection_scheduler;

  SpinLock concurrent_mark_lock;
  SpinLock enumerate_rootset_lock;
  SpinLock concurrent_sweep_lock;
  SpinLock collection_scheduler_lock;
  
  /* system info */
  unsigned int _system_alloc_unit;
  unsigned int _machine_page_size_bytes;
  unsigned int _num_processors;
  /* END of GC --> */
  
  Wspace *wspace;
  
} GC_MS;

//////////////////////////////////////////////////////////////////////////////////////////

inline void *gc_ms_fast_alloc(unsigned size, Allocator *allocator)
{ return wspace_thread_local_alloc(size, allocator); }

inline void *gc_ms_alloc(unsigned size, Allocator *allocator)
{ return wspace_alloc(size, allocator); }

inline Wspace *gc_ms_get_wspace(GC_MS *gc)
{ return gc->wspace; }

inline void gc_ms_set_wspace(GC_MS *gc, Wspace *wspace)
{ gc->wspace = wspace; }

inline POINTER_SIZE_INT gc_ms_free_memory_size(GC_MS *gc)
{ return wspace_free_memory_size(gc_ms_get_wspace(gc)); }

inline POINTER_SIZE_INT gc_ms_total_memory_size(GC_MS *gc)
{ return space_committed_size((Space*)gc_ms_get_wspace(gc)); }

/////////////////////////////////////////////////////////////////////////////////////////

void gc_ms_initialize(GC_MS *gc, POINTER_SIZE_INT initial_heap_size, POINTER_SIZE_INT final_heap_size);
void gc_ms_destruct(GC_MS *gc);
void gc_ms_reclaim_heap(GC_MS *gc);
void gc_ms_iterate_heap(GC_MS *gc);

void gc_ms_start_concurrent_mark(GC_MS* gc);
void gc_ms_start_concurrent_mark(GC_MS* gc, unsigned int num_markers);
void gc_ms_update_space_statistics(GC_MS* gc);
void gc_ms_start_concurrent_sweep(GC_MS* gc, unsigned int num_collectors);
void gc_ms_start_most_concurrent_mark(GC_MS* gc, unsigned int num_markers);
void gc_ms_start_final_mark_after_concurrent(GC_MS* gc, unsigned int num_markers);



#endif // USE_MARK_SWEEP_GC

#endif // _GC_MS_H_
