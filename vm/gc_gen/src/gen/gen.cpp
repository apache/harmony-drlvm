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

#include "port_sysinfo.h"

#include "gen.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../verify/verify_live_heap.h"
#include "../common/space_tuner.h"

/* fspace size limit is not interesting. only for manual tuning purpose */
unsigned int min_nos_size_bytes = 16 * MB;
unsigned int max_nos_size_bytes = 256 * MB;
unsigned int NOS_SIZE = 0;
unsigned int MIN_NOS_SIZE = 0;
unsigned int MAX_NOS_SIZE = 0;

static unsigned int MINOR_ALGO = 0;
static unsigned int MAJOR_ALGO = 0;

#ifndef STATIC_NOS_MAPPING
void* nos_boundary;
#endif

#define RESERVE_BOTTOM ((void*)0x1000000)

static void gc_gen_get_system_info(GC_Gen *gc_gen) 
{
  gc_gen->_machine_page_size_bytes = port_vmem_page_sizes()[0];
  gc_gen->_num_processors = port_CPUs_number();
}

void gc_gen_initialize(GC_Gen *gc_gen, unsigned int min_heap_size, unsigned int max_heap_size) 
{
  assert(gc_gen); 

  /*Give GC a hint of gc survive ratio.*/
  gc_gen->survive_ratio = 0.2f;

  /*fixme: max_heap_size should not beyond 448 MB*/
  max_heap_size = round_down_to_size(max_heap_size, SPACE_ALLOC_UNIT);
  min_heap_size = round_up_to_size(min_heap_size, SPACE_ALLOC_UNIT);
  assert(max_heap_size <= max_heap_size_bytes);
  assert(max_heap_size > min_heap_size_bytes);

  gc_gen_get_system_info(gc_gen); 
  min_nos_size_bytes *=  gc_gen->_num_processors;
  
  if( MIN_NOS_SIZE )  min_nos_size_bytes = MIN_NOS_SIZE;

  unsigned int los_size = max_heap_size >> 7;
  if(los_size < GC_MIN_LOS_SIZE) 
    los_size = GC_MIN_LOS_SIZE;
  
  los_size = round_down_to_size(los_size, SPACE_ALLOC_UNIT);

  /* let's compute and reserve the space for committing */
  
  /* heuristic nos + mos + LOS = max, and nos*ratio = mos */
  unsigned int nos_reserve_size,  nos_commit_size; 
  unsigned int mos_reserve_size, mos_commit_size; 
  unsigned int los_mos_size;
  

  if(NOS_SIZE){
    los_mos_size = max_heap_size - NOS_SIZE;
    mos_reserve_size = los_mos_size - los_size;  

    nos_commit_size = NOS_SIZE;
    nos_reserve_size = NOS_SIZE;
  
  }else{  
    los_mos_size = max_heap_size;
    mos_reserve_size = los_mos_size - los_size;
    nos_commit_size = (unsigned int)(((float)(max_heap_size - los_size))/(1.0f + gc_gen->survive_ratio));
    nos_reserve_size = mos_reserve_size;
  }
    
  nos_commit_size = round_down_to_size(nos_commit_size, SPACE_ALLOC_UNIT);  
  mos_commit_size = max_heap_size - los_size - nos_commit_size;

  /* allocate memory for gc_gen */
  void* reserved_base;
  void* reserved_end;
  void* nos_base;

#ifdef STATIC_NOS_MAPPING

  assert((unsigned int)nos_boundary%SPACE_ALLOC_UNIT == 0);
  nos_base = vm_reserve_mem(nos_boundary, nos_reserve_size);
  if( nos_base != nos_boundary ){
    printf("Static NOS mapping: Can't reserve memory at %x for size %x for NOS.\n", nos_boundary, nos_reserve_size);  
    printf("Please not use static NOS mapping by undefining STATIC_NOS_MAPPING, or adjusting NOS_BOUNDARY value.\n");
    exit(0);
  }
  reserved_end = (void*)((unsigned int)nos_base + nos_reserve_size);

  void* los_mos_base = (void*)((unsigned int)nos_base - los_mos_size);
  assert(!((unsigned int)los_mos_base%SPACE_ALLOC_UNIT));
  reserved_base = vm_reserve_mem(los_mos_base, los_mos_size);
  while( !reserved_base || reserved_base >= nos_base){
    los_mos_base = (void*)((unsigned int)los_mos_base - SPACE_ALLOC_UNIT);
    if(los_mos_base < RESERVE_BOTTOM){
      printf("Static NOS mapping: Can't allocate memory at address %x for specified size %x for MOS", reserved_base, los_mos_size);  
      exit(0);      
    }
    reserved_base = vm_reserve_mem(los_mos_base, los_mos_size);
  }
  
#else /* STATIC_NOS_MAPPING */

  reserved_base = vm_reserve_mem(0, max_heap_size);
  while( !reserved_base ){
    printf("Non-static NOS mapping: Can't allocate memory at address %x for specified size %x", reserved_base, max_heap_size);  
    exit(0);      
  }
  reserved_end = (void*)((unsigned int)reserved_base + max_heap_size);
    
  /* compute first time nos_boundary */
  nos_base = (void*)((unsigned int)reserved_base + mos_commit_size + los_size);
  /* init nos_boundary if NOS is not statically mapped */
  nos_boundary = nos_base; 

#endif  /* STATIC_NOS_MAPPING else */

  gc_gen->reserved_heap_size = los_size + nos_reserve_size + mos_reserve_size;
  gc_gen->heap_start = reserved_base;
  gc_gen->heap_end = reserved_end;
  gc_gen->blocks = (Block*)reserved_base;
  gc_gen->num_collections = 0;
  gc_gen->time_collections = 0;
  gc_gen->force_major_collect = FALSE;
  
  gc_los_initialize(gc_gen, reserved_base, los_size);

  reserved_base = (void*)((unsigned int)reserved_base + los_size);
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

  /*Give GC a hint of space survive ratio.*/
  nos->survive_ratio = gc_gen->survive_ratio;
  mos->survive_ratio = gc_gen->survive_ratio;
  gc_space_tuner_initialize((GC*)gc_gen);
    
  gc_gen->committed_heap_size = space_committed_size((Space*)gc_gen->nos) +
                                space_committed_size((Space*)gc_gen->mos) +
                                space_committed_size((Space*)gc_gen->los);
  

  set_native_finalizer_thread_flag(!IGNORE_FINREF);
  set_native_ref_enqueue_thread_flag(!IGNORE_FINREF);
  
  return;
}

void gc_gen_destruct(GC_Gen *gc_gen) 
{
  gc_nos_destruct(gc_gen);
  gc_gen->nos = NULL;
  
  gc_mos_destruct(gc_gen);  
  gc_gen->mos = NULL;

  gc_los_destruct(gc_gen);  
  gc_gen->los = NULL;

  Space* nos = (Space*)gc_gen->nos;
  Space* mos = (Space*)gc_gen->mos;
  Space* los = (Space*)gc_gen->los;

  vm_unmap_mem(nos->heap_start, space_committed_size(nos));
  vm_unmap_mem(mos->heap_start, space_committed_size(mos));
  vm_unmap_mem(los->heap_start, space_committed_size(los));

  return;  
}

void* mos_alloc(unsigned size, Allocator *allocator){return mspace_alloc(size, allocator);}
void* nos_alloc(unsigned size, Allocator *allocator){return fspace_alloc(size, allocator);}
void* los_alloc(unsigned size, Allocator *allocator){return lspace_alloc(size, allocator);}
Space* gc_get_nos(GC_Gen* gc){ return (Space*)gc->nos;}
Space* gc_get_mos(GC_Gen* gc){ return (Space*)gc->mos;}
Space* gc_get_los(GC_Gen* gc){ return (Space*)gc->los;}
void gc_set_nos(GC_Gen* gc, Space* nos){ gc->nos = (Fspace*)nos;}
void gc_set_mos(GC_Gen* gc, Space* mos){ gc->mos = (Mspace*)mos;}
void gc_set_los(GC_Gen* gc, Space* los){ gc->los = (Lspace*)los;}
unsigned int gc_get_processor_num(GC_Gen* gc){ return gc->_num_processors;}


static Boolean major_collection_needed(GC_Gen* gc)
{
  return space_used_memory_size((Blocked_Space*)gc->nos)*gc->survive_ratio > (space_free_memory_size((Blocked_Space*)gc->mos));
}

Boolean FORCE_FULL_COMPACT = FALSE;

void gc_decide_collection_kind(GC_Gen* gc, unsigned int cause)
{
  /* this is for debugging. */
  gc->last_collect_kind = gc->collect_kind;
  
  if(gc->force_major_collect || cause== GC_CAUSE_LOS_IS_FULL || FORCE_FULL_COMPACT)
    gc->collect_kind = MAJOR_COLLECTION;
  else
    gc->collect_kind = MINOR_COLLECTION;

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
    MAJOR_ALGO= MAJOR_COMPACT_SLIDE;
    
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

Boolean IS_FALLBACK_COMPACTION = FALSE; /* only for debugging, don't use it. */

void gc_gen_reclaim_heap(GC_Gen* gc)
{ 
  if(verify_live_heap) gc_verify_heap((GC*)gc, TRUE);

  int64 start_time = time_now();

  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  mspace->num_used_blocks = mspace->free_block_idx - mspace->first_block_idx;
  fspace->num_used_blocks = fspace->free_block_idx - fspace->first_block_idx;

  gc->collect_result = TRUE;
  
  if(gc->collect_kind == MINOR_COLLECTION){
    /* FIXME:: move_object is only useful for nongen_slide_copy */
    gc->mos->move_object = FALSE;
    
    fspace_collection(gc->nos);
    
    gc->mos->move_object = TRUE;      

      
  }else{

    /* process mos and nos together in one compaction */
    mspace_collection(gc->mos); /* fspace collection is included */
    lspace_collection(gc->los);

  }

  if(gc->collect_result == FALSE && gc->collect_kind == MINOR_COLLECTION){
    
    if(gc_is_gen_mode())
      gc_clear_remset((GC*)gc);  
    
    /* runout mspace in minor collection */
    assert(mspace->free_block_idx == mspace->ceiling_block_idx + 1);
    mspace->num_used_blocks = mspace->num_managed_blocks;

    IS_FALLBACK_COMPACTION = TRUE;

    gc_reset_collect_result((GC*)gc);
    gc->collect_kind = FALLBACK_COLLECTION;    

    mspace_collection(gc->mos); /* fspace collection is included */
    lspace_collection(gc->los);
    
    IS_FALLBACK_COMPACTION = FALSE;
    
  }
  
  if( gc->collect_result == FALSE){
    printf("Out of Memory!\n");
    assert(0);
    exit(0);
  }
  
  int64 pause_time = time_now() - start_time;
  
  gc->time_collections += pause_time;
  
  if(verify_live_heap) gc_verify_heap((GC*)gc, FALSE);

  gc_gen_adapt(gc, pause_time);

  return;
}
