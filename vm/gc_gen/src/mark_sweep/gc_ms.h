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

#ifndef _GC_MS_H_
#define _GC_MS_H_

#ifdef ONLY_SSPACE_IN_HEAP

#include "sspace.h"


/* heap size limit is not interesting. only for manual tuning purpose */
extern POINTER_SIZE_INT min_heap_size_bytes;
extern POINTER_SIZE_INT max_heap_size_bytes;

typedef struct GC_MS {
  /* <-- First couple of fields overloaded as GC */
  void *heap_start;
  void *heap_end;
  POINTER_SIZE_INT reserved_heap_size;
  POINTER_SIZE_INT committed_heap_size;
  unsigned int num_collections;
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
  Vector_Block *uncompressed_root_set;
  
  //For_LOS_extend
  Space_Tuner *tuner;
  /* END of GC --> */
  
  Sspace *sspace;
  
  /* system info */
  unsigned int _system_alloc_unit;
  unsigned int _machine_page_size_bytes;
  unsigned int _num_processors;
  
} GC_MS;

//////////////////////////////////////////////////////////////////////////////////////////

inline void *gc_ms_fast_alloc(unsigned size, Allocator *allocator)
{ return sspace_thread_local_alloc(size, allocator); }

inline void *gc_ms_alloc(unsigned size, Allocator *allocator)
{ return sspace_alloc(size, allocator); }

inline Sspace *gc_ms_get_sspace(GC_MS *gc)
{ return gc->sspace; }

inline void gc_ms_set_sspace(GC_MS *gc, Sspace *sspace)
{ gc->sspace = sspace; }

inline POINTER_SIZE_INT gc_ms_free_memory_size(GC_MS *gc)
{ return sspace_free_memory_size(gc_ms_get_sspace(gc)); }

inline POINTER_SIZE_INT gc_ms_total_memory_size(GC_MS *gc)
{ return space_committed_size((Space*)gc_ms_get_sspace(gc)); }

/////////////////////////////////////////////////////////////////////////////////////////

void gc_ms_initialize(GC_MS *gc, POINTER_SIZE_INT initial_heap_size, POINTER_SIZE_INT final_heap_size);
void gc_ms_destruct(GC_MS *gc);
void gc_ms_reclaim_heap(GC_MS *gc);
void gc_ms_iterate_heap(GC_MS *gc);
unsigned int gc_ms_get_processor_num(GC_MS *gc);


#endif // ONLY_SSPACE_IN_HEAP

#endif // _GC_MS_H_
