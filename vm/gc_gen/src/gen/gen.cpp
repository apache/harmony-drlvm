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

/**
 * @author Xiao-Feng Li, 2006/10/05
 */

#include "gen.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../verify/verify_live_heap.h"
#include "../common/space_tuner.h"
#include "../common/compressed_ref.h"

#ifdef USE_32BITS_HASHCODE
#include "../common/hashcode.h"
#endif

#ifdef GC_GEN_STATS
#include "gen_stats.h"
#endif
/* fspace size limit is not interesting. only for manual tuning purpose */
POINTER_SIZE_INT min_nos_size_bytes = 16 * MB;
POINTER_SIZE_INT max_nos_size_bytes = 256 * MB;
POINTER_SIZE_INT min_los_size_bytes = 4*MB;
POINTER_SIZE_INT min_none_los_size_bytes = 4*MB;
POINTER_SIZE_INT NOS_SIZE = 0;
POINTER_SIZE_INT INIT_LOS_SIZE = 0;
POINTER_SIZE_INT MIN_NOS_SIZE = 0;
POINTER_SIZE_INT MAX_NOS_SIZE = 0;

/* should clean up */
unsigned int MINOR_ALGO = 0;
static unsigned int MAJOR_ALGO = 0;

Boolean GEN_NONGEN_SWITCH = FALSE;

Boolean JVMTI_HEAP_ITERATION = true;

Boolean gen_mode;

#ifndef STATIC_NOS_MAPPING
void* nos_boundary;
#endif

#define RESERVE_BOTTOM ((void*)0x1000000)

static void determine_min_nos_size(GC_Gen *gc, POINTER_SIZE_INT min_heap_size)
{
  min_nos_size_bytes *=  gc->_num_processors;
  
  POINTER_SIZE_INT min_nos_size_threshold = min_heap_size>>5;
  if(min_nos_size_bytes  > min_nos_size_threshold)
    min_nos_size_bytes = round_down_to_size(min_nos_size_threshold, SPACE_ALLOC_UNIT);
  
  if(MIN_NOS_SIZE) min_nos_size_bytes = MIN_NOS_SIZE;
}

static POINTER_SIZE_INT determine_los_size(POINTER_SIZE_INT min_heap_size)
{
  POINTER_SIZE_INT los_size = min_heap_size >> 7;
  if(INIT_LOS_SIZE) los_size = INIT_LOS_SIZE;
  if(los_size < min_los_size_bytes )
    los_size = min_los_size_bytes;
  
  los_size = round_down_to_size(los_size, SPACE_ALLOC_UNIT);
  return los_size;
}

void *alloc_large_pages(size_t size, const char *hint);

void gc_gen_init_verbose(GC_Gen *gc);
void gc_gen_initialize(GC_Gen *gc_gen, POINTER_SIZE_INT min_heap_size, POINTER_SIZE_INT max_heap_size)
{
  TRACE2("gc.process", "GC: GC_Gen heap init ... \n");
  assert(gc_gen);
  
  max_heap_size = round_down_to_size(max_heap_size, SPACE_ALLOC_UNIT);
  min_heap_size = round_up_to_size(min_heap_size, SPACE_ALLOC_UNIT);
  assert(max_heap_size <= max_heap_size_bytes);
  assert(max_heap_size >= min_heap_size_bytes);
  
  determine_min_nos_size(gc_gen, min_heap_size);
  
  POINTER_SIZE_INT los_size = 0;
  if(MAJOR_ALGO == MAJOR_MARK_SWEEP)
    min_los_size_bytes = 0;
  else
    los_size = determine_los_size(min_heap_size);
  
  /* let's compute and reserve the space for committing */
  
  /* heuristic nos + mos + LOS = max, and nos*ratio = mos */
  POINTER_SIZE_INT nos_reserve_size,  nos_commit_size;
  POINTER_SIZE_INT mos_reserve_size, mos_commit_size;
  POINTER_SIZE_INT los_mos_reserve_size;
  
  /* Give GC a hint of gc survive ratio. And the last_survive_ratio field is used in heap size adjustment */
  gc_gen->survive_ratio = 0.2f;
  
  los_mos_reserve_size = max_heap_size - NOS_SIZE;
  mos_reserve_size = los_mos_reserve_size - min_los_size_bytes;
  if(NOS_SIZE){
    nos_reserve_size = nos_commit_size = NOS_SIZE;
  } else {
    nos_reserve_size = mos_reserve_size;
    nos_commit_size = (POINTER_SIZE_INT)(((float)(min_heap_size - los_size))/(1.0f + gc_gen->survive_ratio));
  }
  nos_commit_size = round_down_to_size(nos_commit_size, SPACE_ALLOC_UNIT);
  mos_commit_size = min_heap_size - los_size - nos_commit_size;
  
  /* Reserve memory for spaces of gc_gen */
  void *reserved_base;
  void *reserved_end;
  void *nos_base;
  void* physical_start;

#ifdef STATIC_NOS_MAPPING

  //FIXME:: no large page support in static nos mapping
  assert(large_page_hint==NULL);
  
  assert(!((POINTER_SIZE_INT)nos_boundary % SPACE_ALLOC_UNIT));
  nos_base = vm_reserve_mem(nos_boundary, nos_reserve_size);
  if( nos_base != nos_boundary ){
    DIE2("gc.base","Warning: Static NOS mapping: Can't reserve memory at address"<<nos_boundary<<" for size "<<nos_reserve_size<<" for NOS.");
    DIE2("gc.base","Please not use static NOS mapping by undefining STATIC_NOS_MAPPING, or adjusting NOS_BOUNDARY value.");
    exit(0);
  }
  reserved_end = (void*)((POINTER_SIZE_INT)nos_base + nos_reserve_size);
  
  void *los_mos_base = (void*)((POINTER_SIZE_INT)nos_base - los_mos_reserve_size);
  assert(!((POINTER_SIZE_INT)los_mos_base % SPACE_ALLOC_UNIT));
  reserved_base = vm_reserve_mem(los_mos_base, los_mos_reserve_size);
  while( !reserved_base || reserved_base >= nos_base){
    los_mos_base = (void*)((POINTER_SIZE_INT)los_mos_base - SPACE_ALLOC_UNIT);
    if(los_mos_base < RESERVE_BOTTOM){
      DIE2("gc.base","Static NOS mapping: Can't reserve memory at address"<<reserved_base<<" for specified size "<<los_mos_size);
      exit(0);      
    }
    reserved_base = vm_reserve_mem(los_mos_base, los_mos_reserve_size);
  }
  physical_start = reserved_base;
  
#else /* NON_STATIC_NOS_MAPPING */

  reserved_base = NULL;
  if(large_page_hint){
    reserved_base = alloc_large_pages(max_heap_size, large_page_hint);
    if(reserved_base){
      WARN2("gc.base","GC use large pages.");
    } else {
      free(large_page_hint);
      large_page_hint = NULL;
      WARN2("gc.base","GC use small pages.");
    }
  }
  
  if(reserved_base == NULL){
    Boolean max_size_reduced = FALSE;
    reserved_base = vm_reserve_mem(NULL, max_heap_size + SPACE_ALLOC_UNIT);
    while( !reserved_base ){
      max_size_reduced = TRUE;
      max_heap_size -= SPACE_ALLOC_UNIT;
      reserved_base = vm_reserve_mem(NULL, max_heap_size + SPACE_ALLOC_UNIT);
    }
    
    physical_start = reserved_base;
    
    if(max_size_reduced){
      WARN2("gc.base","Max heap size: can't be reserved, reduced to "<< max_heap_size/MB<<" MB according to virtual memory limitation.");
    }
    
    if(max_heap_size < min_heap_size){
      DIE2("gc.base","Heap size: invalid, please reimput a smaller \"ms\" paramenter!");
      exit(0);
    }
    reserved_base = (void*)round_up_to_size((POINTER_SIZE_INT)reserved_base, SPACE_ALLOC_UNIT);
    assert(!((POINTER_SIZE_INT)reserved_base % SPACE_ALLOC_UNIT));
  }
  
  reserved_end = (void*)((POINTER_SIZE_INT)reserved_base + max_heap_size);
  
  /* Determine intial nos_boundary while NOS is not statically mapped */
  nos_base = (void*)((POINTER_SIZE_INT)reserved_base + mos_commit_size + los_size);
  nos_boundary = nos_base;

#endif  /* STATIC_NOS_MAPPING else */

  HEAP_NULL = (POINTER_SIZE_INT)reserved_base;
  
  gc_gen->physical_start = physical_start;
  gc_gen->heap_start = reserved_base;
  gc_gen->heap_end = reserved_end;
#ifdef STATIC_NOS_MAPPING
  gc_gen->reserved_heap_size = los_mos_reserve_size + nos_reserve_size;
#else
  gc_gen->reserved_heap_size = max_heap_size;
#endif
  /* Commented out for that the frontmost reserved mem size in los is not counted in los' committed size.
   * gc_gen->committed_heap_size = min_heap_size;
   */
  gc_gen->num_collections = 0;
  gc_gen->time_collections = 0;
  gc_gen->blocks = (Block*)reserved_base;
  gc_gen->force_major_collect = FALSE;
  gc_gen->force_gen_mode = FALSE;

  max_heap_size_bytes = max_heap_size;
  min_heap_size_bytes = min_heap_size;
  
  gc_los_initialize(gc_gen, reserved_base, los_size);
  gc_mos_initialize(gc_gen, (void*)((POINTER_SIZE_INT)reserved_base + los_size), mos_reserve_size, mos_commit_size);
  gc_nos_initialize(gc_gen, nos_base, nos_reserve_size, nos_commit_size);
  
  gc_gen->committed_heap_size = space_committed_size(gc_get_nos(gc_gen))
                                                + space_committed_size(gc_get_mos(gc_gen))
                                                + space_committed_size(gc_get_los(gc_gen));
  
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP){
    Blocked_Space *nos = (Blocked_Space*)gc_get_nos(gc_gen);
    Blocked_Space *mos = (Blocked_Space*)gc_get_mos(gc_gen);
    /* Connect mos and nos, so that they can be compacted as one space */
    Block_Header *mos_last_block = (Block_Header*)&mos->blocks[mos->num_managed_blocks-1];
    Block_Header *nos_first_block = (Block_Header*)&nos->blocks[0];
    mos_last_block->next = nos_first_block;
    
    gc_space_tuner_initialize((GC*)gc_gen);
    gc_gen_mode_adapt_init(gc_gen);
  }
  
#ifdef GC_GEN_STATS
  gc_gen_stats_initialize(gc_gen);
#endif

  gc_gen_init_verbose(gc_gen);
  return;
}

void gc_gen_destruct(GC_Gen *gc_gen)
{
  TRACE2("gc.process", "GC: GC_Gen heap destruct ......");
  
  gc_nos_destruct(gc_gen);
  gc_mos_destruct(gc_gen);
  POINTER_SIZE_INT los_size = 0;
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP){
    los_size = space_committed_size((Space*)gc_gen->los);
    gc_los_destruct(gc_gen);
  }

#ifndef STATIC_NOS_MAPPING
  /* without static mapping, the heap is release as a whole. 
     We cannot use reserve_heap_size because perhaps only part of it is committed.  */
  vm_unmap_mem(gc_gen->physical_start, gc_gen_total_memory_size(gc_gen));

#else  /* otherwise, release the spaces separately */

  int mos_size = space_committed_size((Space*)gc_gen->mos);
  int nos_size = space_committed_size((Space*)gc_gen->nos);
  vm_unmap_mem(gc_gen->physical_start, los_size + mos_size);  /* los+mos */
  vm_unmap_mem(nos_boundary, nos_size);  /* nos */

#endif /* !STATIC_NOS_MAPPING */

#ifdef GC_GEN_STATS
  gc_gen_stats_destruct(gc_gen);
#endif

  return;  
}

Space *gc_get_nos(GC_Gen *gc){ return gc->nos; }
Space *gc_get_mos(GC_Gen *gc){ return gc->mos; }
Space *gc_get_los(GC_Gen *gc){ return gc->los; }

void gc_set_nos(GC_Gen *gc, Space *nos){ gc->nos = nos; }
void gc_set_mos(GC_Gen *gc, Space *mos){ gc->mos = mos; }
void gc_set_los(GC_Gen *gc, Space *los){ gc->los = los; }

Space_Alloc_Func nos_alloc;
Space_Alloc_Func mos_alloc;
Space_Alloc_Func los_alloc;

void* los_try_alloc(POINTER_SIZE_INT size, GC* gc){  return lspace_try_alloc((Lspace*)((GC_Gen*)gc)->los, size); }

void gc_nos_initialize(GC_Gen *gc, void *start, POINTER_SIZE_INT nos_size, POINTER_SIZE_INT commit_size)
{
  Space *nos;
  if(MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL){
    nos = (Space*)sspace_initialize((GC*)gc, start, nos_size, commit_size);
    nos_alloc = sspace_alloc;
  }else{
    nos = (Space*)fspace_initialize((GC*)gc, start, nos_size, commit_size);
    nos_alloc = fspace_alloc;
  }
  
  gc_set_nos(gc, nos);
  nos->collect_algorithm = MINOR_ALGO;
}

void gc_nos_destruct(GC_Gen *gc)
{ fspace_destruct((Fspace*)gc->nos); }

void gc_mos_initialize(GC_Gen *gc, void *start, POINTER_SIZE_INT mos_size, POINTER_SIZE_INT commit_size)
{
  Space *mos;
  if(MAJOR_ALGO == MAJOR_MARK_SWEEP){
    mos = (Space*)wspace_initialize((GC*)gc, start, mos_size, commit_size);
    mos_alloc = wspace_alloc;
  } else {
    mos = (Space*)mspace_initialize((GC*)gc, start, mos_size, commit_size);
    mos_alloc = mspace_alloc;
  }
  gc_set_mos(gc, mos);
  mos->collect_algorithm = MAJOR_ALGO;
}

void gc_mos_destruct(GC_Gen *gc)
{
  if(MAJOR_ALGO == MAJOR_MARK_SWEEP)
    wspace_destruct((Wspace*)gc->mos);
  else
    mspace_destruct((Mspace*)gc->mos);
}

void gc_los_initialize(GC_Gen *gc, void *start, POINTER_SIZE_INT los_size)
{
  Space *los;
  if(MAJOR_ALGO == MAJOR_MARK_SWEEP){
    assert(los_size == 0);
    los = NULL;
    los_alloc = wspace_alloc;
  } else {
    los = (Space*)lspace_initialize((GC*)gc, start, los_size);
    los_alloc = lspace_alloc;
  }
  gc_set_los(gc, los);
}

void gc_los_destruct(GC_Gen *gc)
{
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
    lspace_destruct((Lspace*)gc->los);
}


Boolean FORCE_FULL_COMPACT = FALSE;
Boolean IGNORE_VTABLE_TRACING = FALSE;
Boolean TRACE_JLC_VIA_VTABLE = FALSE;

unsigned int gc_next_collection_kind(GC_Gen* gc)
{
  if(gc->force_major_collect || FORCE_FULL_COMPACT)
    return MAJOR_COLLECTION;
  else
    return MINOR_COLLECTION;
}


void gc_decide_collection_kind(GC_Gen* gc, unsigned int cause)
{
  /* this is for debugging. */
  gc->last_collect_kind = gc->collect_kind;
#if defined(USE_MARK_SWEEP_GC)
  gc->collect_kind = MS_COLLECTION;
#elif defined(USE_UNIQUE_MOVE_COMPACT_GC)
  gc->collect_kind = MC_COLLECTION;
#else
  if(gc->force_major_collect || cause== GC_CAUSE_LOS_IS_FULL || FORCE_FULL_COMPACT)
    gc->collect_kind = NORMAL_MAJOR_COLLECTION;
  else
    gc->collect_kind = MINOR_COLLECTION;
    
  if(IGNORE_VTABLE_TRACING || (gc->collect_kind == MINOR_COLLECTION))
    TRACE_JLC_VIA_VTABLE = FALSE;
  else
    TRACE_JLC_VIA_VTABLE = TRUE;
#endif
  return;
}

void gc_decide_collection_algorithm(GC_Gen* gc, char* minor_algo, char* major_algo)
{
  if(!minor_algo){
    MINOR_ALGO = MINOR_NONGEN_FORWARD_POOL;      
    gc_disable_gen_mode();
  
  }else{
    string_to_upper(minor_algo);
     
    if(!strcmp(minor_algo, "MINOR_NONGEN_FORWARD_POOL")){  
      MINOR_ALGO = MINOR_NONGEN_FORWARD_POOL;
      gc_disable_gen_mode();
      
    }else if(!strcmp(minor_algo, "MINOR_GEN_FORWARD_POOL")){
      MINOR_ALGO = MINOR_GEN_FORWARD_POOL;
      gc_enable_gen_mode();
    
    }else if(!strcmp(minor_algo, "MINOR_NONGEN_SEMISPACE_POOL")){
      MINOR_ALGO = MINOR_NONGEN_SEMISPACE_POOL;
      gc_disable_gen_mode();
    
    }else {
      WARN2("gc.base","\nWarning: GC algorithm setting incorrect. Will use default value.\n");
    
    }
  }
  
  if(!major_algo){
    MAJOR_ALGO = MAJOR_COMPACT_MOVE;
    
  }else{
    string_to_upper(major_algo);

    if(!strcmp(major_algo, "MAJOR_COMPACT_SLIDE")){
      MAJOR_ALGO = MAJOR_COMPACT_SLIDE;
      
    }else if(!strcmp(major_algo, "MAJOR_COMPACT_MOVE")){
      MAJOR_ALGO = MAJOR_COMPACT_MOVE;
    
    }else if(!strcmp(major_algo, "MAJOR_MARK_SWEEP")){
      MAJOR_ALGO = MAJOR_MARK_SWEEP;
      is_collector_local_alloc = FALSE;
    }else{
     WARN2("gc.base","\nWarning: GC algorithm setting incorrect. Will use default value.\n");
      
    }
  }
  
  return;
  
}

static Boolean nos_alloc_block(Space* space, Allocator* allocator)
{
  Boolean result;
  if(MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL)
    result = sspace_alloc_block((Sspace*)space, allocator); 
  else
    result = fspace_alloc_block((Fspace*)space, allocator);   
 
  return result;   
}

/* assign a free area to the mutator who triggers the collection */
void gc_gen_assign_free_area_to_mutators(GC_Gen* gc)
{
  if(gc->cause == GC_CAUSE_LOS_IS_FULL){
    Lspace* los = (Lspace*)gc->los;
    los->success_ptr = los_try_alloc(los->failure_size, (GC*)gc);      
    los->failure_size = 0;
     
  }else{ 
    /* it is possible that NOS has no free block,
       because MOS takes all space after fallback or LOS extension.
       Allocator should be cleared. */
    Allocator *allocator = (Allocator *)gc_get_tls();   
    Boolean ok = nos_alloc_block(gc->nos, allocator);
    /* we don't care about the return value. If no block available, that means,
       first allocation after mutator resumption will probably trigger OOME. */
  }

  return;     
}

static void gc_gen_adjust_heap_size(GC_Gen* gc)
{
  if(gc_match_kind((GC*)gc, MINOR_COLLECTION)) return;
  if(gc->committed_heap_size == max_heap_size_bytes - LOS_HEAD_RESERVE_FOR_HEAP_NULL) return;
  
  Mspace* mos = (Mspace*)gc->mos;
  Fspace* nos = (Fspace*)gc->nos;
  Lspace* los = (Lspace*)gc->los;
  /* We can not tolerate gc->survive_ratio be greater than threshold twice continuously.
   * Or, we must adjust heap size
   */
  static unsigned int tolerate = 0;

  POINTER_SIZE_INT heap_total_size = los->committed_heap_size + mos->committed_heap_size + nos->committed_heap_size;
  assert(heap_total_size == gc->committed_heap_size);

  assert(nos->last_surviving_size == 0);  
  POINTER_SIZE_INT heap_surviving_size = (POINTER_SIZE_INT)(mos->period_surviving_size + los->period_surviving_size);
  assert(heap_total_size > heap_surviving_size);

  float heap_survive_ratio = (float)heap_surviving_size / (float)heap_total_size;
  float non_los_survive_ratio = (float)mos->period_surviving_size / (float)(mos->committed_heap_size + nos->committed_heap_size);
  float threshold_survive_ratio = 0.3f;
  float regular_survive_ratio = 0.125f;

  POINTER_SIZE_INT new_heap_total_size = 0;
  POINTER_SIZE_INT adjust_size = 0;

  if( (heap_survive_ratio < threshold_survive_ratio) && (non_los_survive_ratio < threshold_survive_ratio) )return;

  if(++tolerate < 2) return;
  tolerate = 0;
  
  new_heap_total_size = max((POINTER_SIZE_INT)((float)heap_surviving_size / regular_survive_ratio), 
                                            (POINTER_SIZE_INT)((float)mos->period_surviving_size / regular_survive_ratio + los->committed_heap_size));
  new_heap_total_size = round_down_to_size(new_heap_total_size, SPACE_ALLOC_UNIT);


  if(new_heap_total_size <= heap_total_size) return;
  /*If there is only small piece of area left not committed, we just merge it into the heap at once*/
  if(new_heap_total_size + (max_heap_size_bytes >> 5) > max_heap_size_bytes - LOS_HEAD_RESERVE_FOR_HEAP_NULL) 
    new_heap_total_size = max_heap_size_bytes - LOS_HEAD_RESERVE_FOR_HEAP_NULL;

  adjust_size = new_heap_total_size - heap_total_size;
  assert( !(adjust_size % SPACE_ALLOC_UNIT) );
  if(adjust_size == 0) return;
  
#ifdef STATIC_NOS_MAPPING
  /*Fixme: Static mapping have other bugs to be fixed first.*/
  assert(!large_page_hint);
  return;
#else
  assert(!large_page_hint);
  POINTER_SIZE_INT old_nos_size = nos->committed_heap_size;
  INFO2("gc.process", "GC: gc_gen heap extension after GC["<<gc->num_collections<<"] ...");
  blocked_space_extend(nos, (unsigned int)adjust_size);
  INFO2("gc.space","GC: heap extension: from "<<heap_total_size/MB<<"MB  to  "<<new_heap_total_size/MB<<"MB\n");
  if (!NOS_SIZE) {
    nos->survive_ratio = (float)old_nos_size * nos->survive_ratio / (float)nos->committed_heap_size;
    if( NOS_PARTIAL_FORWARD )
      object_forwarding_boundary = (void*)&nos->blocks[nos->num_managed_blocks >>1 ];
    else
      object_forwarding_boundary = (void*)&nos->blocks[nos->num_managed_blocks];
  }
  else {
    /*if user specified NOS_SIZE, adjust mos and nos size to keep nos size as an constant*/
    old_nos_size = nos->committed_heap_size;
    nos_boundary = (void*)((POINTER_SIZE_INT)nos->heap_end - NOS_SIZE);
    nos->committed_heap_size = NOS_SIZE;
    nos->heap_start = nos_boundary;
    nos->blocks = (Block*)nos_boundary;
    nos->first_block_idx = ((Block_Header*)nos_boundary)->block_idx;
    nos->num_managed_blocks = (unsigned int)(NOS_SIZE >> GC_BLOCK_SHIFT_COUNT);
    nos->num_total_blocks = nos->num_managed_blocks;
    nos->free_block_idx = nos->first_block_idx;
    if( NOS_PARTIAL_FORWARD )
      object_forwarding_boundary = (void*)&nos->blocks[nos->num_managed_blocks >>1 ];
    else
      object_forwarding_boundary = (void*)&nos->blocks[nos->num_managed_blocks];

    mos->heap_end = nos_boundary;
    mos->committed_heap_size += old_nos_size-NOS_SIZE;
    mos->num_managed_blocks = (unsigned int)(mos->committed_heap_size >> GC_BLOCK_SHIFT_COUNT);
    mos->num_total_blocks = mos->num_managed_blocks;
    mos->ceiling_block_idx = ((Block_Header*)nos_boundary)->block_idx - 1;

    mos->survive_ratio = (float) mos->last_surviving_size / (float)mos->committed_heap_size;
  }

  /*Fixme: gc fields should be modified according to nos extend*/
  gc->committed_heap_size += adjust_size;
  //debug_adjust
  assert(gc->committed_heap_size == los->committed_heap_size + mos->committed_heap_size + nos->committed_heap_size);
#endif
  
// printf("heap_size: %x MB , heap_survive_ratio: %f\n", gc->committed_heap_size/MB, heap_survive_ratio);

}

void gc_gen_start_concurrent_mark(GC_Gen* gc)
{
  assert(0);
}

static inline void nos_collection(Space *nos)
{ 
  if(MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL)
    sspace_collection((Sspace*)nos); 
  else
    fspace_collection((Fspace*)nos); 
}

static inline void mos_collection(Space *mos)
{
  if(MAJOR_ALGO == MAJOR_MARK_SWEEP)
    wspace_collection((Wspace*)mos);
  else
    mspace_collection((Mspace*)mos);
}

static inline void los_collection(Space *los)
{
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
    lspace_collection((Lspace*)los);
}

static void gc_gen_update_space_info_before_gc(GC_Gen *gc)
{
  Fspace *nos = (Fspace*)gc->nos;
  Mspace *mos = (Mspace*)gc->mos;
  Lspace *los = (Lspace*)gc->los;
  
  /* Update before every GC to avoid the atomic operation in every fspace_alloc_block */
  assert( nos->free_block_idx >= nos->first_block_idx );
  nos->num_used_blocks = nos->free_block_idx - nos->first_block_idx;
  nos->last_alloced_size = GC_BLOCK_SIZE_BYTES * nos->num_used_blocks;
  nos->accumu_alloced_size += nos->last_alloced_size;
  
  mos->num_used_blocks = mos->free_block_idx - mos->first_block_idx;
  
  if(los){
    assert(MAJOR_ALGO != MAJOR_MARK_SWEEP);
    los->accumu_alloced_size += los->last_alloced_size;
  }
}

static void gc_gen_update_space_info_after_gc(GC_Gen *gc)
{
  Space *nos = gc_get_nos(gc);
  Space *mos = gc_get_mos(gc);
  Space *los = gc_get_los(gc);
  
  /* Minor collection, but also can be every n minor collections, use fspace->num_collections to identify. */
  if (gc_match_kind((GC*)gc, MINOR_COLLECTION)){
    mos->accumu_alloced_size += mos->last_alloced_size;
    /* The alloced_size reset operation of mos and nos is not necessary, because they are not accumulated.
     * But los->last_alloced_size must be reset, because it is accumulated. */
    if(los){
      assert(MAJOR_ALGO != MAJOR_MARK_SWEEP);
      los->last_alloced_size = 0;
    }
  /* Major collection, but also can be every n major collections, use mspace->num_collections to identify. */
  } else {
    mos->total_alloced_size += mos->accumu_alloced_size;
    mos->last_alloced_size = 0;
    mos->accumu_alloced_size = 0;
    
    nos->total_alloced_size += nos->accumu_alloced_size;
    nos->last_alloced_size = 0;
    nos->accumu_alloced_size = 0;
    
    if(los){
      assert(MAJOR_ALGO != MAJOR_MARK_SWEEP);
      los->total_alloced_size += los->accumu_alloced_size;
      los->last_alloced_size = 0;
      los->accumu_alloced_size = 0;
    }
  }
}
 
static void nos_reset_after_collection(Space *nos)
{
  if(MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL)
    sspace_reset_after_collection((Sspace*)nos);
  else
    fspace_reset_after_collection((Fspace*)nos);
}

static void nos_prepare_for_collection(Space *nos)
{
  if(MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL)
    sspace_prepare_for_collection((Sspace*)nos);
}

static void mos_reset_after_collection(Space *mos)
{
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
    mspace_reset_after_collection((Mspace*)mos);
  else
    wspace_reset_after_collection((Wspace*)mos);
}

Boolean IS_FALLBACK_COMPACTION = FALSE; /* only for debugging, don't use it. */
void gc_gen_stats_verbose(GC_Gen* gc);

void gc_gen_reclaim_heap(GC_Gen *gc, int64 gc_start_time)
{ 
  INFO2("gc.process", "GC: start GC_Gen ...\n");
  
  Space *nos = gc->nos;
  Space *mos = gc->mos;
  Space *los = gc->los;
  
  
  if(verify_live_heap && (MAJOR_ALGO != MAJOR_MARK_SWEEP))
    gc_verify_heap((GC*)gc, TRUE);
  
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP){
    gc_gen_update_space_info_before_gc(gc);
    gc_compute_space_tune_size_before_marking((GC*)gc);
  }
  
  gc->collect_result = TRUE;
#ifdef GC_GEN_STATS
  gc_gen_stats_reset_before_collection(gc);
  gc_gen_collector_stats_reset(gc);
#endif

  nos_prepare_for_collection(nos);

  if(gc_match_kind((GC*)gc, MINOR_COLLECTION)){

    INFO2("gc.process", "GC: start minor collection ...\n");

    /* FIXME:: move_object is only useful for nongen_slide_copy */
    mos->move_object = FALSE;
    
    /* This is for compute mos->last_alloced_size */
    unsigned int mos_used_blocks_before_minor, mos_used_blocks_after_minor; /* only used for non MAJOR_MARK_SWEEP collection */
    if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
      mos_used_blocks_before_minor = ((Blocked_Space*)mos)->free_block_idx - ((Blocked_Space*)mos)->first_block_idx;
    
    nos_collection(nos);

#ifdef GC_GEN_STATS
    gc_gen_collector_stats_verbose_minor_collection(gc);
#endif

    if(MAJOR_ALGO != MAJOR_MARK_SWEEP){
      mos_used_blocks_after_minor = ((Blocked_Space*)mos)->free_block_idx - ((Blocked_Space*)mos)->first_block_idx;
      assert( mos_used_blocks_before_minor <= mos_used_blocks_after_minor );
      ((Blocked_Space*)mos)->last_alloced_size = GC_BLOCK_SIZE_BYTES * ( mos_used_blocks_after_minor - mos_used_blocks_before_minor );
    }
    
    /* If the minor collection failed, i.e. there happens a fallback, we should not do the minor sweep of LOS. */
    if(gc->collect_result != FALSE && !gc_is_gen_mode()) {
#ifdef GC_GEN_STATS
      gc->stats->num_minor_collections++;
#endif
      los_collection(los);
    }
    
    mos->move_object = TRUE;

    INFO2("gc.process", "GC: end of minor collection ...\n");

  } else {

    INFO2("gc.process", "GC: start major collection ...\n");

    if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
      los->move_object = TRUE;
    
    mos_collection(mos); /* collect mos and nos  together */
    los_collection(los);
    
    if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
      los->move_object = FALSE;

#ifdef GC_GEN_STATS
    gc->stats->num_major_collections++;
    gc_gen_collector_stats_verbose_major_collection(gc);
#endif

    INFO2("gc.process", "GC: end of major collection ...\n");
  }
  
  if(gc->collect_result == FALSE && gc_match_kind((GC*)gc, MINOR_COLLECTION)){
    
    INFO2("gc.process", "GC: Minor collection failed, transform to fallback collection ...");
    
    if(gc_is_gen_mode()) gc_clear_remset((GC*)gc);
    
    /* runout mos in minor collection */
    if(MAJOR_ALGO != MAJOR_MARK_SWEEP){
      assert(((Blocked_Space*)mos)->free_block_idx == ((Blocked_Space*)mos)->ceiling_block_idx + 1);
      ((Blocked_Space*)mos)->num_used_blocks = ((Blocked_Space*)mos)->num_managed_blocks;
    }
    
    IS_FALLBACK_COMPACTION = TRUE;
    gc_reset_collect_result((GC*)gc);
    gc->collect_kind = FALLBACK_COLLECTION;

#ifdef GC_GEN_STATS
    /*since stats is changed in minor collection, we need to reset stats before fallback collection*/
    gc_gen_stats_reset_before_collection((GC_Gen*)gc);
    gc_gen_collector_stats_reset((GC_Gen*)gc);
#endif

    if(verify_live_heap && (MAJOR_ALGO != MAJOR_MARK_SWEEP))
      event_gc_collect_kind_changed((GC*)gc);
    
    if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
      los->move_object = TRUE;

    mos_collection(mos); /* collect both mos and nos */
    los_collection(los);
    if(MAJOR_ALGO != MAJOR_MARK_SWEEP)
      los->move_object = FALSE;
    
    IS_FALLBACK_COMPACTION = FALSE;

#ifdef GC_GEN_STATS
    gc->stats->num_fallback_collections++;
    gc_gen_collector_stats_verbose_major_collection(gc);
#endif

    INFO2("gc.process", "GC: end of fallback collection ...");

  }
  
  if( gc->collect_result == FALSE){
    DIE2("gc.collect", "Out of Memory!\n");
    assert(0);
    exit(0);
  }
  
  nos_reset_after_collection(nos);
  if(gc_match_kind((GC*)gc, MAJOR_COLLECTION))
    mos_reset_after_collection(mos);
  
  if(verify_live_heap && (MAJOR_ALGO != MAJOR_MARK_SWEEP))
    gc_verify_heap((GC*)gc, FALSE);
  
  assert(MAJOR_ALGO == MAJOR_MARK_SWEEP || !los->move_object);
  
  if(MAJOR_ALGO != MAJOR_MARK_SWEEP && MINOR_ALGO != MINOR_NONGEN_SEMISPACE_POOL){
    gc_gen_adjust_heap_size(gc);
    
    int64 pause_time = time_now() - gc_start_time;
    gc->time_collections += pause_time;
    gc_gen_adapt(gc, pause_time);
    gc_space_tuner_reset((GC*)gc);
  }
  
  gc_gen_update_space_info_after_gc(gc);

#ifdef GC_GEN_STATS
  gc_gen_stats_update_after_collection(gc);
  gc_gen_stats_verbose(gc);
#endif

  INFO2("gc.process", "GC: end of GC_Gen\n");
  
  return;
}

void gc_gen_iterate_heap(GC_Gen *gc)
{
  /** the function is called after stoped the world **/
  Mutator *mutator = gc->mutator_list;
  bool cont = true;   
  while (mutator) {
    Block_Header* block = (Block_Header*)mutator->alloc_block;
  	if(block != NULL) block->free = mutator->free;
  	mutator = mutator->next;
  }

  Blocked_Space *mspace = (Blocked_Space*)gc->mos;
  Block_Header *curr_block = (Block_Header*)mspace->blocks;
  Block_Header *space_end = (Block_Header*)&mspace->blocks[mspace->free_block_idx - mspace->first_block_idx];
  while(curr_block < space_end) {
    POINTER_SIZE_INT p_obj = (POINTER_SIZE_INT)curr_block->base;
    POINTER_SIZE_INT block_end = (POINTER_SIZE_INT)curr_block->free;
    unsigned int hash_extend_size = 0;
    while(p_obj < block_end){
      cont = vm_iterate_object((Managed_Object_Handle)p_obj);
      if (!cont) return;
#ifdef USE_32BITS_HASHCODE
      hash_extend_size  = (hashcode_is_attached((Partial_Reveal_Object*)p_obj))?GC_OBJECT_ALIGNMENT:0;
#endif
      p_obj = p_obj + vm_object_size((Partial_Reveal_Object *)p_obj) + hash_extend_size;
    }
    curr_block = curr_block->next;
    if(curr_block == NULL) break;
  }
  
  Blocked_Space *fspace = (Blocked_Space*)gc->nos;
  curr_block = (Block_Header*)fspace->blocks;
  space_end = (Block_Header*)&fspace->blocks[fspace->free_block_idx - fspace->first_block_idx];
  while(curr_block < space_end) {
   	POINTER_SIZE_INT p_obj = (POINTER_SIZE_INT)curr_block->base;
    POINTER_SIZE_INT block_end = (POINTER_SIZE_INT)curr_block->free;
    while(p_obj < block_end){
      cont = vm_iterate_object((Managed_Object_Handle)p_obj);
      if (!cont) return;
      p_obj = p_obj + vm_object_size((Partial_Reveal_Object *)p_obj);
    }
    	curr_block = curr_block->next;
      if(curr_block == NULL) break;
    }

  Lspace *lspace = (Lspace*)gc->los;
  POINTER_SIZE_INT lspace_obj = (POINTER_SIZE_INT)lspace->heap_start;
  POINTER_SIZE_INT lspace_end = (POINTER_SIZE_INT)lspace->heap_end;
  unsigned int hash_extend_size = 0;
  while (lspace_obj < lspace_end) {
    if(!*((unsigned int *)lspace_obj)){
      lspace_obj = lspace_obj + ((Free_Area*)lspace_obj)->size;
    }else{
      cont = vm_iterate_object((Managed_Object_Handle)lspace_obj);
      if (!cont) return;
#ifdef USE_32BITS_HASHCODE
      hash_extend_size  = (hashcode_is_attached((Partial_Reveal_Object *)lspace_obj))?GC_OBJECT_ALIGNMENT:0;
#endif
      unsigned int obj_size = (unsigned int)ALIGN_UP_TO_KILO(vm_object_size((Partial_Reveal_Object *)lspace_obj)+hash_extend_size);
      lspace_obj = lspace_obj + obj_size;
    }
  }
}

void gc_gen_collection_verbose_info(GC_Gen *gc, int64 pause_time, int64 mutator_time)
{

#ifdef GC_GEN_STATS
  GC_Gen_Stats* stats = ((GC_Gen*)gc)->stats;
  stats->total_mutator_time += mutator_time;
  stats->total_pause_time += pause_time;
#endif

  INFO2("gc.collect","GC: GC_Gen Collection Info:"
    <<"\nGC: GC id: GC["<<gc->num_collections<<"]"
    <<"\nGC: current collection num: "<<gc->num_collections);

  switch(gc->collect_kind) {
  case MINOR_COLLECTION:
    INFO2("gc.collect","GC: collection type: minor");
#ifdef GC_GEN_STATS
    INFO2("gc.collect","GC: current minor collection num: "<<gc->stats->num_minor_collections);
#endif
    break;
  case NORMAL_MAJOR_COLLECTION:
    INFO2("gc.collect","GC: collection type: normal major");
#ifdef GC_GEN_STATS
    INFO2("gc.collect","GC: current normal major collection num: "<<gc->stats->num_major_collections);
#endif
    break;
  case FALLBACK_COLLECTION:
    INFO2("gc.collect","GC: collection type: fallback");
#ifdef GC_GEN_STATS
    INFO2("gc.collect","GC: current fallback collection num: "<<gc->stats->num_fallback_collections);
#endif
  }

  switch(gc->cause) {
  case GC_CAUSE_NOS_IS_FULL:
    INFO2("gc.collect","GC: collection cause: nursery object space is full");
    break;
  case GC_CAUSE_LOS_IS_FULL:
    INFO2("gc.collect","GC: collection cause: large object space is full");
    break;
  case GC_CAUSE_RUNTIME_FORCE_GC:
    INFO2("gc.collect","GC: collection cause: runtime force gc");
  }

  INFO2("gc.collect","GC: pause time: "<<(pause_time>>10)<<"ms"
    <<"\nGC: mutator time from last collection: "<<(mutator_time>>10)<<"ms\n");

}

void gc_gen_space_verbose_info(GC_Gen *gc)
{
  INFO2("gc.space","GC: Heap info after GC["<<gc->num_collections<<"]:"
    <<"\nGC: Heap size: "<<verbose_print_size(gc->committed_heap_size)<<", free size:"<<verbose_print_size(gc_gen_free_memory_size(gc))
    <<"\nGC: LOS size: "<<verbose_print_size(gc->los->committed_heap_size)<<", free size:"<<verbose_print_size(lspace_free_memory_size((Lspace*)gc->los))
    <<"\nGC: MOS size: "<<verbose_print_size(gc->mos->committed_heap_size)<<", free size:"<<verbose_print_size(blocked_space_free_mem_size((Blocked_Space*)gc->mos))
    <<"\nGC: NOS size: "<<verbose_print_size(gc->nos->committed_heap_size)<<", free size:"<<verbose_print_size(blocked_space_free_mem_size((Blocked_Space*)gc->nos))<<"\n");
}

inline void gc_gen_init_verbose(GC_Gen *gc)
{
  INFO2("gc.base","GC_Gen initial:"
    <<"\nmax heap size: "<<verbose_print_size(max_heap_size_bytes)
    <<"\nmin heap size: "<<verbose_print_size(min_heap_size_bytes)
    <<"\ninitial heap size: "<<verbose_print_size(gc->committed_heap_size)
    <<"\ninitial num collectors: "<<gc->num_collectors
    <<"\ninitial nos size: "<<verbose_print_size(gc->nos->committed_heap_size)
    <<"\nnos collection algo: "
    <<((gc->nos->collect_algorithm==MINOR_NONGEN_FORWARD_POOL)?"nongen forward":"gen forward")
    <<"\ninitial mos size: "<<verbose_print_size(gc->mos->committed_heap_size)
    <<"\nmos collection algo: "
    <<((gc->mos->collect_algorithm==MAJOR_COMPACT_MOVE)?"move compact":"slide compact")
    <<"\ninitial los size: "<<verbose_print_size(gc->los->committed_heap_size)<<"\n");
}

void gc_gen_wrapup_verbose(GC_Gen* gc)
{
#ifdef GC_GEN_STATS
  GC_Gen_Stats* stats = gc->stats;

  INFO2("gc.base", "GC: All Collection info: "
    <<"\nGC: total nos alloc obj size: "<<verbose_print_size(stats->total_size_nos_alloc)
    <<"\nGC: total los alloc obj num: "<<stats->obj_num_los_alloc
    <<"\nGC: total los alloc obj size:"<<verbose_print_size(stats->total_size_los_alloc)
    <<"\nGC: total collection num: "<<gc->num_collections
    <<"\nGC: minor collection num: "<<stats->num_minor_collections
    <<"\nGC: major collection num: "<<stats->num_major_collections
    <<"\nGC: total collection time: "<<stats->total_pause_time
    <<"\nGC: total appliction execution time: "<<stats->total_mutator_time<<"\n");
#endif
}

/* init collector alloc_space */
void gc_gen_init_collector_alloc(GC_Gen* gc, Collector* collector)
{
  if(MAJOR_ALGO == MAJOR_MARK_SWEEP){
    allocator_init_local_chunks((Allocator*)collector);
    gc_init_collector_free_chunk_list(collector);
  }

  Allocator* allocator = (Allocator*)collector;
  
  if( MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL || MINOR_ALGO == MINOR_GEN_SEMISPACE_POOL){
    allocator->alloc_space = gc->nos; 
    /* init backup allocator */
    unsigned int size = sizeof(Allocator);
    allocator = (Allocator*)STD_MALLOC(size);  //assign its alloc_space below.
    memset(allocator, 0, size);  
    collector->backup_allocator = allocator;
  }
    
  allocator->alloc_space = gc->mos;
}

void gc_gen_reset_collector_alloc(GC_Gen* gc, Collector* collector)
{
  alloc_context_reset((Allocator*)collector);
  if( MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL || MINOR_ALGO == MINOR_GEN_SEMISPACE_POOL){
    alloc_context_reset(collector->backup_allocator);
  }      
}

void gc_gen_destruct_collector_alloc(GC_Gen* gc, Collector* collector)
{
  if( MINOR_ALGO == MINOR_NONGEN_SEMISPACE_POOL || MINOR_ALGO == MINOR_GEN_SEMISPACE_POOL){
    STD_FREE(collector->backup_allocator);  
  }
}


