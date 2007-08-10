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

#include "port_sysinfo.h"

#include "gen.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../verify/verify_live_heap.h"
#include "../common/space_tuner.h"
#include "../common/compressed_ref.h"

#ifdef USE_32BITS_HASHCODE
#include "../common/hashcode.h"
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

static unsigned int MINOR_ALGO = 0;
static unsigned int MAJOR_ALGO = 0;

Boolean GEN_NONGEN_SWITCH = FALSE;

Boolean JVMTI_HEAP_ITERATION = true;

#ifndef STATIC_NOS_MAPPING
void* nos_boundary;
#endif

#define RESERVE_BOTTOM ((void*)0x1000000)

static void gc_gen_get_system_info(GC_Gen *gc_gen) 
{
  gc_gen->_machine_page_size_bytes = (unsigned int)port_vmem_page_sizes()[0];
  gc_gen->_num_processors = port_CPUs_number();
  gc_gen->_system_alloc_unit = vm_get_system_alloc_unit();
  SPACE_ALLOC_UNIT = max(gc_gen->_system_alloc_unit, GC_BLOCK_SIZE_BYTES);
}

void* alloc_large_pages(size_t size, const char* hint);

void gc_gen_initialize(GC_Gen *gc_gen, POINTER_SIZE_INT min_heap_size, POINTER_SIZE_INT max_heap_size) 
{
  assert(gc_gen); 
  gc_gen_get_system_info(gc_gen); 

  max_heap_size = round_down_to_size(max_heap_size, SPACE_ALLOC_UNIT);
  min_heap_size = round_up_to_size(min_heap_size, SPACE_ALLOC_UNIT);
  assert(max_heap_size <= max_heap_size_bytes);
  assert(max_heap_size >= min_heap_size_bytes);

  min_nos_size_bytes *=  gc_gen->_num_processors;

  POINTER_SIZE_INT min_nos_size_threshold = min_heap_size>>5;
  if(min_nos_size_bytes  > min_nos_size_threshold){
    min_nos_size_bytes = round_down_to_size(min_nos_size_threshold,SPACE_ALLOC_UNIT);
  }
  
  if( MIN_NOS_SIZE )  min_nos_size_bytes = MIN_NOS_SIZE;

  POINTER_SIZE_INT los_size = min_heap_size >> 7;
  if(INIT_LOS_SIZE) los_size = INIT_LOS_SIZE;
  if(los_size < min_los_size_bytes ) 
    los_size = min_los_size_bytes ;
  
  los_size = round_down_to_size(los_size, SPACE_ALLOC_UNIT);

  /* let's compute and reserve the space for committing */
  
  /* heuristic nos + mos + LOS = max, and nos*ratio = mos */
  POINTER_SIZE_INT nos_reserve_size,  nos_commit_size; 
  POINTER_SIZE_INT mos_reserve_size, mos_commit_size; 
  POINTER_SIZE_INT los_mos_size;
  
  /*Give GC a hint of gc survive ratio. And the last_survive_ratio field is used in heap size adjustment*/
  gc_gen->survive_ratio = 0.2f;

  if(NOS_SIZE){
    los_mos_size = min_heap_size - NOS_SIZE;
    mos_reserve_size = los_mos_size - los_size;  

    nos_commit_size = NOS_SIZE;
    nos_reserve_size = NOS_SIZE;
  
  }else{  
    los_mos_size = min_heap_size;
    mos_reserve_size = max_heap_size_bytes - min_los_size_bytes;
    nos_commit_size = (POINTER_SIZE_INT)(((float)(min_heap_size - los_size))/(1.0f + gc_gen->survive_ratio));
    nos_reserve_size = mos_reserve_size;
  }
    
  nos_commit_size = round_down_to_size(nos_commit_size, SPACE_ALLOC_UNIT);  
  mos_commit_size = min_heap_size - los_size - nos_commit_size;

  /* allocate memory for gc_gen */
  void* reserved_base;
  void* reserved_end;
  void* nos_base;

#ifdef STATIC_NOS_MAPPING

  //FIXME: no large page support in static nos mapping
  assert(large_page_hint==NULL);
  
  assert((POINTER_SIZE_INT)nos_boundary%SPACE_ALLOC_UNIT == 0);
  nos_base = vm_reserve_mem(nos_boundary, nos_reserve_size);
  if( nos_base != nos_boundary ){
    printf("Static NOS mapping: Can't reserve memory at %x for size %x for NOS.\n", nos_boundary, nos_reserve_size);  
    printf("Please not use static NOS mapping by undefining STATIC_NOS_MAPPING, or adjusting NOS_BOUNDARY value.\n");
    exit(0);
  }
  reserved_end = (void*)((POINTER_SIZE_INT)nos_base + nos_reserve_size);

  void* los_mos_base = (void*)((POINTER_SIZE_INT)nos_base - los_mos_size);
  assert(!((POINTER_SIZE_INT)los_mos_base%SPACE_ALLOC_UNIT));
  reserved_base = vm_reserve_mem(los_mos_base, los_mos_size);
  while( !reserved_base || reserved_base >= nos_base){
    los_mos_base = (void*)((POINTER_SIZE_INT)los_mos_base - SPACE_ALLOC_UNIT);
    if(los_mos_base < RESERVE_BOTTOM){
      printf("Static NOS mapping: Can't allocate memory at address %x for specified size %x for MOS", reserved_base, los_mos_size);  
      exit(0);      
    }
    reserved_base = vm_reserve_mem(los_mos_base, los_mos_size);
  }
/* NON_STATIC_NOS_MAPPING */  
#else 

  reserved_base = NULL;
  if(large_page_hint){
    reserved_base = alloc_large_pages(max_heap_size, large_page_hint);
    if(reserved_base == NULL) {
      free(large_page_hint);
      large_page_hint = NULL;
      printf("GC use small pages.\n");
    }else{
      printf("GC use large pages.\n");
    }
  }
  
  if(reserved_base == NULL){
    Boolean max_size_reduced = 0;
    reserved_base = vm_reserve_mem((void*)0, max_heap_size + SPACE_ALLOC_UNIT);
    while( !reserved_base ){
      max_size_reduced = 1;
      max_heap_size -= SPACE_ALLOC_UNIT;
      reserved_base = vm_reserve_mem((void*)0, max_heap_size + SPACE_ALLOC_UNIT);
    }

    if(max_size_reduced){
      printf("Max heap size: can't be reserved, reduced to %d MB according to virtual memory limitation.\n", max_heap_size/MB);
    }

    if(max_heap_size < min_heap_size){
      printf("Heap size: invalid, please reimput a smaller \"ms\" paramenter!\n");  
      exit(0);
    }
    reserved_base = (void*)round_up_to_size((POINTER_SIZE_INT)reserved_base, SPACE_ALLOC_UNIT);
    assert((POINTER_SIZE_INT)reserved_base%SPACE_ALLOC_UNIT == 0);
  }

  reserved_end = (void*)((POINTER_SIZE_INT)reserved_base + max_heap_size);

    
  /* compute first time nos_boundary */
  nos_base = (void*)((POINTER_SIZE_INT)reserved_base + mos_commit_size + los_size);
  /* init nos_boundary if NOS is not statically mapped */
  nos_boundary = nos_base; 

#endif  /* STATIC_NOS_MAPPING else */

  HEAP_NULL = (POINTER_SIZE_INT)reserved_base;

#ifdef STATIC_NOS_MAPPING  
  gc_gen->reserved_heap_size = los_size + nos_reserve_size + mos_reserve_size;
#else
  gc_gen->reserved_heap_size = max_heap_size_bytes;
#endif
  gc_gen->heap_start = reserved_base;
  gc_gen->heap_end = reserved_end;
  gc_gen->blocks = (Block*)reserved_base;
  gc_gen->num_collections = 0;
  gc_gen->time_collections = 0;
  gc_gen->force_major_collect = FALSE;
  gc_gen->force_gen_mode = FALSE;
  
  gc_los_initialize(gc_gen, reserved_base, los_size);

  reserved_base = (void*)((POINTER_SIZE_INT)reserved_base + los_size);
  gc_mos_initialize(gc_gen, reserved_base, mos_reserve_size, mos_commit_size);

  gc_nos_initialize(gc_gen, nos_base, nos_reserve_size, nos_commit_size); 
    
  /* connect mos and nos, so that they can be compacted as one space */
  Blocked_Space* mos = (Blocked_Space*)gc_get_mos(gc_gen);
  Blocked_Space* nos = (Blocked_Space*)gc_get_nos(gc_gen);
  Block_Header* mos_last_block = (Block_Header*)&mos->blocks[mos->num_managed_blocks-1];
  Block_Header* nos_first_block = (Block_Header*)&nos->blocks[0];
  mos_last_block->next = nos_first_block;
  
  nos->collect_algorithm = MINOR_ALGO;
  mos->collect_algorithm = MAJOR_ALGO;

  gc_space_tuner_initialize((GC*)gc_gen);

  gc_gen_mode_adapt_init(gc_gen);
    
  gc_gen->committed_heap_size = space_committed_size((Space*)gc_gen->nos) +
                                space_committed_size((Space*)gc_gen->mos) +
                                space_committed_size((Space*)gc_gen->los);
  
  return;
}

void gc_gen_destruct(GC_Gen *gc_gen) 
{
  Space* nos = (Space*)gc_gen->nos;
  Space* mos = (Space*)gc_gen->mos;
  Space* los = (Space*)gc_gen->los;

  POINTER_SIZE_INT nos_size = space_committed_size(nos);
  POINTER_SIZE_INT mos_size = space_committed_size(mos);
  POINTER_SIZE_INT los_size = space_committed_size(los);

  void* nos_start = nos->heap_start;
  void* mos_start = mos->heap_start;
  void* los_start = los->heap_start;
  
  gc_nos_destruct(gc_gen);
  gc_gen->nos = NULL;
  
  gc_mos_destruct(gc_gen);  
  gc_gen->mos = NULL;

  gc_los_destruct(gc_gen);  
  gc_gen->los = NULL;
  
  vm_unmap_mem(nos_start, nos_size);
  vm_unmap_mem(mos_start, mos_size);
  vm_unmap_mem(los_start, los_size);

  return;  
}

Space* gc_get_nos(GC_Gen* gc){ return (Space*)gc->nos;}
Space* gc_get_mos(GC_Gen* gc){ return (Space*)gc->mos;}
Space* gc_get_los(GC_Gen* gc){ return (Space*)gc->los;}

void gc_set_nos(GC_Gen* gc, Space* nos){ gc->nos = (Fspace*)nos;}
void gc_set_mos(GC_Gen* gc, Space* mos){ gc->mos = (Mspace*)mos;}
void gc_set_los(GC_Gen* gc, Space* los){ gc->los = (Lspace*)los;}

void* mos_alloc(unsigned size, Allocator *allocator){return mspace_alloc(size, allocator);}
void* nos_alloc(unsigned size, Allocator *allocator){return fspace_alloc(size, allocator);}
void* los_alloc(unsigned size, Allocator *allocator){return lspace_alloc(size, allocator);}
void* los_try_alloc(POINTER_SIZE_INT size, GC* gc){  return lspace_try_alloc((Lspace*)((GC_Gen*)gc)->los, size); }


unsigned int gc_get_processor_num(GC_Gen* gc){ return gc->_num_processors;}

Boolean FORCE_FULL_COMPACT = FALSE;

void gc_decide_collection_kind(GC_Gen* gc, unsigned int cause)
{
  /* this is for debugging. */
  gc->last_collect_kind = gc->collect_kind;
  
  if(gc->force_major_collect || cause== GC_CAUSE_LOS_IS_FULL || FORCE_FULL_COMPACT)
    gc->collect_kind = MAJOR_COLLECTION;
  else
    gc->collect_kind = MINOR_COLLECTION;

#ifdef ONLY_SSPACE_IN_HEAP
  gc->collect_kind = UNIQUE_SWEEP_COLLECTION;
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
    
    }else{
      printf("\nGC algorithm setting incorrect. Will use default value.\n");  
    
    }
  }
  
  if(!major_algo){
    MAJOR_ALGO= MAJOR_COMPACT_MOVE;
    
  }else{
    string_to_upper(major_algo);

    if(!strcmp(major_algo, "MAJOR_COMPACT_SLIDE")){
     MAJOR_ALGO= MAJOR_COMPACT_SLIDE;
      
    }else if(!strcmp(major_algo, "MAJOR_COMPACT_MOVE")){
     MAJOR_ALGO= MAJOR_COMPACT_MOVE;
    
    }else{
     printf("\nGC algorithm setting incorrect. Will use default algorithm.\n");  
      
    }
  }
  
  return;
  
}

void gc_gen_assign_free_area_to_mutators(GC_Gen* gc)
{
  if(gc->cause == GC_CAUSE_LOS_IS_FULL){
    Lspace* los = gc->los;
    los->success_ptr = los_try_alloc(los->failure_size, (GC*)gc);      
    los->failure_size = 0;
     
  }else{ 
    Blocked_Space* nos = (Blocked_Space*)gc->nos;
    if(nos->num_managed_blocks == 0) return;

    Mutator *mutator = (Mutator *)gc_get_tls();   
    allocator_init_free_block((Allocator*)mutator, (Block_Header*)nos->blocks);
    nos->free_block_idx++;
  }
    
  return;     
}

void gc_gen_adjust_heap_size(GC_Gen* gc, int64 pause_time)
{
  if(gc_match_kind((GC*)gc, MINOR_COLLECTION)) return;
  if(gc->committed_heap_size == max_heap_size_bytes - LOS_HEAD_RESERVE_FOR_HEAP_NULL) return;
  
  Mspace* mos = gc->mos;
  Fspace* nos = gc->nos;
  Lspace* los = gc->los;
  /*We can not tolerate gc->survive_ratio be greater than threshold twice continuously.
   *Or, we must adjust heap size */
  static unsigned int tolerate = 0;

  POINTER_SIZE_INT heap_total_size = los->committed_heap_size + mos->committed_heap_size + nos->committed_heap_size;
  assert(heap_total_size == gc->committed_heap_size);

  assert(nos->last_surviving_size == 0);  
  POINTER_SIZE_INT heap_surviving_size = (POINTER_SIZE_INT)(mos->period_surviving_size + los->period_surviving_size);
  assert(heap_total_size > heap_surviving_size);

  float heap_survive_ratio = (float)heap_surviving_size / (float)heap_total_size;
  float threshold_survive_ratio = 0.3f;
  float regular_survive_ratio = 0.125f;

  POINTER_SIZE_INT new_heap_total_size = 0;
  POINTER_SIZE_INT adjust_size = 0;

  if(heap_survive_ratio < threshold_survive_ratio) return;

  if(++tolerate < 2) return;
  tolerate = 0;
  
  new_heap_total_size = (POINTER_SIZE_INT)((float)heap_surviving_size / regular_survive_ratio);
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
  blocked_space_extend(nos, (unsigned int)adjust_size);
  nos->survive_ratio = (float)old_nos_size * nos->survive_ratio / (float)nos->committed_heap_size;
  /*Fixme: gc fields should be modified according to nos extend*/
  gc->committed_heap_size += adjust_size;
  //debug_adjust
  assert(gc->committed_heap_size == los->committed_heap_size + mos->committed_heap_size + nos->committed_heap_size);
#endif
  
// printf("heap_size: %x MB , heap_survive_ratio: %f\n", gc->committed_heap_size/MB, heap_survive_ratio);

}

Boolean IS_FALLBACK_COMPACTION = FALSE; /* only for debugging, don't use it. */
static unsigned int mspace_num_used_blocks_before_minor;
static unsigned int mspace_num_used_blocks_after_minor;
void gc_gen_reclaim_heap(GC_Gen* gc)
{ 
  if(verify_live_heap) gc_verify_heap((GC*)gc, TRUE);

  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  mspace->num_used_blocks = mspace->free_block_idx - mspace->first_block_idx;
  fspace->num_used_blocks = fspace->free_block_idx - fspace->first_block_idx;

  gc->collect_result = TRUE;
  
  if(gc_match_kind((GC*)gc, MINOR_COLLECTION)){
    /* FIXME:: move_object is only useful for nongen_slide_copy */
    gc->mos->move_object = 0;
    /* This is for compute mspace->last_alloced_size */

    mspace_num_used_blocks_before_minor = mspace->free_block_idx - mspace->first_block_idx;
    fspace_collection(gc->nos);
    mspace_num_used_blocks_after_minor = mspace->free_block_idx - mspace->first_block_idx;
    assert( mspace_num_used_blocks_before_minor <= mspace_num_used_blocks_after_minor );
    mspace->last_alloced_size = GC_BLOCK_SIZE_BYTES * ( mspace_num_used_blocks_after_minor - mspace_num_used_blocks_before_minor );

    /*If the current minor collection failed, i.e. there happens a fallback, we should not do the minor sweep of LOS*/
    if(gc->collect_result != FALSE && !gc_is_gen_mode())
      lspace_collection(gc->los);

    gc->mos->move_object = 1;      
  }else{
    /* process mos and nos together in one compaction */
    gc->los->move_object = 1;

    mspace_collection(gc->mos); /* fspace collection is included */
    lspace_collection(gc->los);

    gc->los->move_object = 0;
  }

  if(gc->collect_result == FALSE && gc_match_kind((GC*)gc, MINOR_COLLECTION)){
    if(gc_is_gen_mode()) gc_clear_remset((GC*)gc);  
    
    /* runout mspace in minor collection */
    assert(mspace->free_block_idx == mspace->ceiling_block_idx + 1);
    mspace->num_used_blocks = mspace->num_managed_blocks;

    IS_FALLBACK_COMPACTION = TRUE;
    gc_reset_collect_result((GC*)gc);
    gc->collect_kind = FALLBACK_COLLECTION;    

    if(verify_live_heap) event_gc_collect_kind_changed((GC*)gc);
    
    gc->los->move_object = 1;
    mspace_collection(gc->mos); /* fspace collection is included */
    lspace_collection(gc->los);
    gc->los->move_object = 0;    

    IS_FALLBACK_COMPACTION = FALSE;
  }
  
  if( gc->collect_result == FALSE){
    printf("Out of Memory!\n");
    assert(0);
    exit(0);
  }
  
  if(verify_live_heap) gc_verify_heap((GC*)gc, FALSE);

  /* FIXME:: clear root set here to support verify. */
#ifdef COMPRESS_REFERENCE
  gc_set_pool_clear(gc->metadata->gc_uncompressed_rootset_pool);
#endif
  assert(!gc->los->move_object);
  return;
}

void gc_gen_update_space_before_gc(GC_Gen *gc)
{
  /* Update before every GC to avoid the atomic operation in every fspace_alloc_block */
  assert( gc->nos->free_block_idx >= gc->nos->first_block_idx );
  gc->nos->last_alloced_size = GC_BLOCK_SIZE_BYTES * ( gc->nos->free_block_idx - gc->nos->first_block_idx );

  gc->nos->accumu_alloced_size += gc->nos->last_alloced_size;
  gc->los->accumu_alloced_size += gc->los->last_alloced_size;
}

void gc_gen_update_space_after_gc(GC_Gen *gc)
{
  /* Minor collection, but also can be every n minor collections, use fspace->num_collections to identify. */
  if (gc_match_kind((GC*)gc, MINOR_COLLECTION)){
    gc->mos->accumu_alloced_size += gc->mos->last_alloced_size;
    /* The alloced_size reset operation of mos and nos is not necessary, because they are not accumulated.
     * But los->last_alloced_size must be reset, because it is accumulated. */
    gc->los->last_alloced_size = 0;
  /* Major collection, but also can be every n major collections, use mspace->num_collections to identify. */
  }else{
    gc->mos->total_alloced_size += gc->mos->accumu_alloced_size;
    gc->mos->last_alloced_size = 0;
    gc->mos->accumu_alloced_size = 0;

    gc->nos->total_alloced_size += gc->nos->accumu_alloced_size;
    gc->nos->last_alloced_size = 0;
    gc->nos->accumu_alloced_size = 0;

    gc->los->total_alloced_size += gc->los->accumu_alloced_size;
    gc->los->last_alloced_size = 0;
    gc->los->accumu_alloced_size = 0;
    
  }
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

  Mspace* mspace = gc->mos;
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
  
  Fspace* fspace = gc->nos;
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

  Lspace* lspace = gc->los;
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
