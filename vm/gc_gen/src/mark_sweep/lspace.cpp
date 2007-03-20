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
 * @author Ji Qi, 2006/10/05
 */

#include "lspace.h"

void* los_boundary = NULL;
struct GC_Gen;
void gc_set_los(GC_Gen* gc, Space* lspace);

/*Fixme: This macro is for handling HEAP_NULL issues caused by JIT OPT*/
#ifdef COMPRESS_REFERENCE
  #define LOS_HEAD_RESERVE_FOR_HEAP_NULL (4*KB)
#else
  #define LOS_HEAD_RESERVE_FOR_HEAP_NULL (0*KB)
#endif

void lspace_initialize(GC* gc, void* start, POINTER_SIZE_INT lspace_size)
{
  Lspace* lspace = (Lspace*)STD_MALLOC( sizeof(Lspace));
  assert(lspace);
  memset(lspace, 0, sizeof(Lspace));

  /* commit mspace mem */    
  void* reserved_base = start;
  unsigned int committed_size = lspace_size;
  if(!large_page_hint)
    vm_commit_mem(reserved_base, lspace_size);
  memset(reserved_base, 0, lspace_size);

  lspace->committed_heap_size = committed_size - LOS_HEAD_RESERVE_FOR_HEAP_NULL;
  lspace->reserved_heap_size = committed_size - LOS_HEAD_RESERVE_FOR_HEAP_NULL;
  lspace->heap_start = (void*)((POINTER_SIZE_INT)reserved_base + LOS_HEAD_RESERVE_FOR_HEAP_NULL);
  lspace->heap_end = (void *)((POINTER_SIZE_INT)reserved_base + committed_size);

  lspace->move_object = FALSE;
  lspace->gc = gc;

  /*Treat with free area buddies*/
  lspace->free_pool = (Free_Area_Pool*)STD_MALLOC(sizeof(Free_Area_Pool));
  free_area_pool_init(lspace->free_pool);
  Free_Area* initial_fa = (Free_Area*)lspace->heap_start;
  initial_fa->size = lspace->committed_heap_size;
  free_pool_add_area(lspace->free_pool, initial_fa);

  lspace->num_collections = 0;
  lspace->time_collections = 0;
  lspace->survive_ratio = 0.5f;

  gc_set_los((GC_Gen*)gc, (Space*)lspace);
  los_boundary = lspace->heap_end;

  return;
}

void lspace_destruct(Lspace* lspace)
{
  STD_FREE(lspace);
  lspace = NULL;
  return;
}

#include "../common/fix_repointed_refs.h"

/* this is minor collection, lspace is not swept, so we need clean markbits */
void lspace_fix_after_copy_nursery(Collector* collector, Lspace* lspace)
{
  unsigned int mark_bit_idx = 0;
  Partial_Reveal_Object* p_obj = lspace_get_first_marked_object(lspace, &mark_bit_idx);
  while( p_obj){
    assert(obj_is_marked_in_vt(p_obj));
    obj_unmark_in_vt(p_obj);
    object_fix_ref_slots(p_obj);
    p_obj = lspace_get_next_marked_object(lspace, &mark_bit_idx);
  }
}

void lspace_fix_repointed_refs(Collector* collector, Lspace* lspace)
{
  unsigned int start_pos = 0;
  Partial_Reveal_Object* p_obj = lspace_get_first_marked_object(lspace, &start_pos);
  while( p_obj){
    assert(obj_is_marked_in_vt(p_obj));
    object_fix_ref_slots(p_obj);
    p_obj = lspace_get_next_marked_object(lspace, &start_pos);
  }
}

void lspace_collection(Lspace* lspace)
{
  /* heap is marked already, we need only sweep here. */
  lspace->num_collections ++;
  lspace_reset_after_collection(lspace);  
  lspace_sweep(lspace);
  return;
}

unsigned int lspace_get_failure_size(Lspace* lspace)
{
  return lspace->failure_size;
}
