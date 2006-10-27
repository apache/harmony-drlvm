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

#ifndef _GC_GEN_H_
#define _GC_GEN_H_

#include "../common/gc_common.h"
#include "../thread/thread_alloc.h"
#include "../trace_forward/fspace.h"
#include "../mark_compact/mspace.h"
#include "../mark_sweep/lspace.h"
  
enum Write_Barrier_Kind{
  WRITE_BARRIER_NIL,  
  WRITE_BARRIER_SLOT,  
  WRITE_BARRIER_OBJECT,
  WRITE_BARRIER_UPDATE      
}; 

/* some globals */

/* heap size limit is not interesting. only for manual tuning purpose */
extern unsigned int min_heap_size_bytes;
extern unsigned int max_heap_size_bytes;

/* fspace size is variable, adjusted adaptively within the range */
extern unsigned int min_nos_size_bytes;
extern unsigned int max_nos_size_bytes;

typedef struct GC_Gen {
  /* <-- First couple of fields overloadded as GC */
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  
  /* mutation related info */
  Mutator *mutator_list;
  SpinLock mutator_list_lock;
  unsigned int num_mutators;

  /* collection related info */    
  Collector** collectors;
  unsigned int num_collectors;
  unsigned int num_active_collectors; /* not all collectors are working */
  
  /* rootsets for collection (FIXME:: should be distributed to collectors) */
  RootSet* root_set;
  unsigned int collect_kind; /* MAJOR or MINOR */
  
  /* mem info */
  apr_pool_t *aux_pool;
  port_vmem_t *allocated_memory;
  /* END of GC --> */
  
  Block* blocks;
  Fspace *nos;
  Mspace *mos;
  Lspace *los;
    
  /* system info */ 
  unsigned int _machine_page_size_bytes;
  unsigned int _num_processors;
  
} GC_Gen;

//////////////////////////////////////////////////////////////////////////////////////////

void gc_gen_initialize(GC_Gen *gc, unsigned int initial_heap_size, unsigned int final_heap_size);
void gc_gen_destruct(GC_Gen *gc);
                        
inline unsigned int gc_gen_free_memory_size(GC_Gen* gc)
{  return fspace_free_memory_size(gc->nos) +
         mspace_free_memory_size(gc->mos) +
         lspace_free_memory_size(gc->los);  }
                        
void gc_gen_reclaim_heap(GC_Gen* gc, unsigned int cause);
void gc_gen_update_repointed_refs(Collector* collector);

/////////////////////////////////////////////////////////////////////////////////////////

inline void gc_nos_initialize(GC_Gen* gc, void* start, unsigned int nos_size)
{ fspace_initialize((GC*)gc, start, nos_size); }

inline void gc_nos_destruct(GC_Gen* gc)
{	fspace_destruct(gc->nos); }

inline void gc_mos_initialize(GC_Gen* gc, void* start, unsigned int mos_size)
{ mspace_initialize((GC*)gc, start, mos_size); }

inline void gc_mos_destruct(GC_Gen* gc)
{ mspace_destruct(gc->mos); }

inline void gc_los_initialize(GC_Gen* gc, void* start, unsigned int los_size)
{ lspace_initialize((GC*)gc, start, los_size); }

inline void gc_los_destruct(GC_Gen* gc)
{	lspace_destruct(gc->los); }

inline Boolean address_belongs_to_nursery(void* addr, GC_Gen* gc)
{ return address_belongs_to_space(addr, (Space*)gc->nos); }

extern void* nos_boundary;
extern void* los_boundary;

inline Space* space_of_addr(GC* gc, void* addr)
{
  if( addr < nos_boundary) return (Space*)((GC_Gen*)gc)->nos;
  if( addr < los_boundary) return (Space*)((GC_Gen*)gc)->mos;
  return (Space*)((GC_Gen*)gc)->los;
}

void* mos_alloc(unsigned size, Allocator *allocator);
void* nos_alloc(unsigned size, Allocator *allocator);
void* los_alloc(unsigned size, Allocator *allocator);
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);
void gc_set_nos(GC_Gen* gc, Space* nos);
void gc_set_mos(GC_Gen* gc, Space* mos);
void gc_set_los(GC_Gen* gc, Space* los);
unsigned int gc_get_processor_num(GC_Gen* gc);

void gc_preprocess_mutator(GC_Gen* gc);
void gc_postprocess_mutator(GC_Gen* gc);
void gc_preprocess_collector(Collector* collector);
void gc_postprocess_collector(Collector* collector);

#endif /* ifndef _GC_GEN_H_ */

