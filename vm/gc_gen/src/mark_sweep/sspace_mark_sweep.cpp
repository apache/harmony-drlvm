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

#include "sspace_alloc.h"
#include "sspace_mark_sweep.h"
#include "sspace_verify.h"
#include "gc_ms.h"
#include "../gen/gen.h"
#include "../thread/collector.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../common/fix_repointed_refs.h"
#include "../common/gc_concurrent.h"

POINTER_SIZE_INT cur_alloc_mask = (~MARK_MASK_IN_TABLE) & FLIP_COLOR_MASK_IN_TABLE;
POINTER_SIZE_INT cur_mark_mask = MARK_MASK_IN_TABLE;
POINTER_SIZE_INT cur_alloc_color = OBJ_COLOR_WHITE;
POINTER_SIZE_INT cur_mark_gray_color = OBJ_COLOR_GRAY;
POINTER_SIZE_INT cur_mark_black_color = OBJ_COLOR_BLACK;

static Chunk_Header_Basic *volatile next_chunk_for_fixing;

static void ops_color_flip(void)
{
  POINTER_SIZE_INT temp = cur_alloc_color;
  cur_alloc_color = cur_mark_black_color;
  cur_mark_black_color = temp;
  cur_alloc_mask = (~cur_alloc_mask) & FLIP_COLOR_MASK_IN_TABLE;
  cur_mark_mask = (~cur_mark_mask) & FLIP_COLOR_MASK_IN_TABLE;
}

void collector_init_free_chunk_list(Collector *collector)
{
  Free_Chunk_List *list = (Free_Chunk_List*)STD_MALLOC(sizeof(Free_Chunk_List));
  free_chunk_list_init(list);
  collector->free_chunk_list = list;
}

/* Argument need_construct stands for whether or not the dual-directon list needs constructing */
Chunk_Header_Basic *sspace_grab_next_chunk(Sspace *sspace, Chunk_Header_Basic *volatile *shared_next_chunk, Boolean need_construct)
{
  Chunk_Header_Basic *cur_chunk = *shared_next_chunk;
  
  Chunk_Header_Basic *sspace_ceiling = (Chunk_Header_Basic*)space_heap_end((Space*)sspace);
  while(cur_chunk < sspace_ceiling){
    Chunk_Header_Basic *next_chunk = CHUNK_END(cur_chunk);
    
    Chunk_Header_Basic *temp = (Chunk_Header_Basic*)atomic_casptr((volatile void**)shared_next_chunk, next_chunk, cur_chunk);
    if(temp == cur_chunk){
      if(need_construct && next_chunk < sspace_ceiling)
        next_chunk->adj_prev = cur_chunk;
      return cur_chunk;
    }
    cur_chunk = *shared_next_chunk;
  }
  
  return NULL;
}


static void sspace_init_chunk_for_ref_fixing(Sspace *sspace)
{
  next_chunk_for_fixing = (Chunk_Header_Basic*)space_heap_start((Space*)sspace);
  next_chunk_for_fixing->adj_prev = NULL;
}

static void nos_init_block_for_forwarding(GC_Gen *gc_gen)
{ blocked_space_block_iterator_init((Blocked_Space*)gc_get_nos(gc_gen)); }

static inline void block_forward_live_objects(Collector *collector, Sspace *sspace, Block_Header *cur_block)
{
  void *start_pos;
  Partial_Reveal_Object *p_obj = block_get_first_marked_object(cur_block, &start_pos);
  
  while(p_obj ){
    assert(obj_is_marked_in_vt(p_obj));
    obj_clear_dual_bits_in_vt(p_obj);
    Partial_Reveal_Object *p_target_obj = collector_forward_object(collector, p_obj); /* Could be implemented with a optimized function */
    if(!p_target_obj){
      assert(collector->gc->collect_result == FALSE);
      printf("Out of mem in forwarding nos!\n");
      exit(0);
    }
    p_obj = block_get_next_marked_object(cur_block, &start_pos);
  }
}

static void collector_forward_nos_to_sspace(Collector *collector, Sspace *sspace)
{
  Blocked_Space *nos = (Blocked_Space*)gc_get_nos((GC_Gen*)collector->gc);
  Block_Header *cur_block = blocked_space_block_iterator_next(nos);
  
  /* We must iterate over all nos blocks to forward live objects in them */
  while(cur_block){
    block_forward_live_objects(collector, sspace, cur_block);
    cur_block = blocked_space_block_iterator_next(nos);
  }
}

static void normal_chunk_fix_repointed_refs(Chunk_Header *chunk)
{
  /* Init field slot_index and depad the last index word in table for fixing */
  chunk->slot_index = 0;
  chunk_depad_last_index_word(chunk);
  
  unsigned int alloc_num = chunk->alloc_num;
  assert(alloc_num);
  
  /* After compaction, many chunks are filled with objects.
   * For these chunks, we needn't find the allocated slot one by one by calling next_alloc_slot_in_chunk.
   * That is a little time consuming.
   * We'd like to fix those objects by incrementing their addr to find the next.
   */
  if(alloc_num == chunk->slot_num){  /* Filled with objects */
    unsigned int slot_size = chunk->slot_size;
    Partial_Reveal_Object *p_obj = (Partial_Reveal_Object*)slot_index_to_addr(chunk, 0);
    for(unsigned int i = alloc_num; i--;){
      object_fix_ref_slots(p_obj);
#ifdef SSPACE_VERIFY
      sspace_verify_fix_in_compact();
#endif
      p_obj = (Partial_Reveal_Object*)((POINTER_SIZE_INT)p_obj + slot_size);
    }
  } else {  /* Chunk is not full */
    while(alloc_num){
      Partial_Reveal_Object *p_obj = next_alloc_slot_in_chunk(chunk);
      assert(p_obj);
      object_fix_ref_slots(p_obj);
#ifdef SSPACE_VERIFY
      sspace_verify_fix_in_compact();
#endif
      --alloc_num;
    }
  }
  
  if(chunk->alloc_num != chunk->slot_num){
    chunk_pad_last_index_word(chunk, cur_alloc_mask);
    pfc_reset_slot_index(chunk);
  }
}

static void abnormal_chunk_fix_repointed_refs(Chunk_Header *chunk)
{
  object_fix_ref_slots((Partial_Reveal_Object*)chunk->base);
#ifdef SSPACE_VERIFY
  sspace_verify_fix_in_compact();
#endif
}

static void sspace_fix_repointed_refs(Collector *collector, Sspace *sspace)
{
  Chunk_Header_Basic *chunk = sspace_grab_next_chunk(sspace, &next_chunk_for_fixing, TRUE);
  
  while(chunk){
    if(chunk->status & CHUNK_NORMAL)
      normal_chunk_fix_repointed_refs((Chunk_Header*)chunk);
    else if(chunk->status & CHUNK_ABNORMAL)
      abnormal_chunk_fix_repointed_refs((Chunk_Header*)chunk);
    
    chunk = sspace_grab_next_chunk(sspace, &next_chunk_for_fixing, TRUE);
  }
}


static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_sweeping_collectors = 0;
static volatile unsigned int num_compacting_collectors = 0;
static volatile unsigned int num_forwarding_collectors = 0;
static volatile unsigned int num_fixing_collectors = 0;

void mark_sweep_sspace(Collector *collector)
{
  GC *gc = collector->gc;
  Sspace *sspace = gc_get_sspace(gc);
  Space *nos = NULL;
  if(gc_match_kind(gc, MAJOR_COLLECTION))
    nos = gc_get_nos((GC_Gen*)gc);
  
  unsigned int num_active_collectors = gc->num_active_collectors;
  
  /* Pass 1: **************************************************
     Mark all live objects in heap ****************************/
  atomic_cas32(&num_marking_collectors, 0, num_active_collectors+1);
  
  if(gc_match_kind(gc, FALLBACK_COLLECTION))
    sspace_fallback_mark_scan(collector, sspace);
  else
    sspace_mark_scan(collector, sspace);
  
  unsigned int old_num = atomic_inc32(&num_marking_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
#ifdef SSPACE_TIME
    sspace_mark_time(FALSE);
#endif
    if(!IGNORE_FINREF )
      collector_identify_finref(collector);
#ifndef BUILD_IN_REFERENT
    else {
      gc_set_weakref_sets(gc);
      gc_update_weakref_ignore_finref(gc);
    }
#endif
    identify_dead_weak_roots(gc, gc->metadata->weak_roots_pool);
    gc_init_chunk_for_sweep(gc, sspace);
    /* let other collectors go */
    num_marking_collectors++;
  }
  while(num_marking_collectors != num_active_collectors + 1);
  
  /* Pass 2: **************************************************
     Sweep dead objects ***************************************/
  atomic_cas32( &num_sweeping_collectors, 0, num_active_collectors+1);
  
  sspace_sweep(collector, sspace);
  
  old_num = atomic_inc32(&num_sweeping_collectors);
  if( ++old_num == num_active_collectors ){
#ifdef SSPACE_TIME
    sspace_sweep_time(FALSE, sspace->need_compact);
#endif
    ops_color_flip();
#ifdef SSPACE_CHUNK_INFO
    sspace_chunks_info(sspace, TRUE);
#endif
#ifdef SSPACE_VERIFY
    sspace_verify_after_sweep(gc);
#endif

    sspace_merge_free_chunks(gc, sspace);
    
    if(gc_match_kind(gc, MAJOR_COLLECTION))
      nos_init_block_for_forwarding((GC_Gen*)gc);
    if(sspace->need_compact)
      sspace_init_pfc_pool_iterator(sspace);
    if(sspace->need_fix)
      sspace_init_chunk_for_ref_fixing(sspace);
    /* let other collectors go */
    num_sweeping_collectors++;
  }
  while(num_sweeping_collectors != num_active_collectors + 1);
  
  /* Optional Pass: *******************************************
     Forward live obj in nos to mos (sspace) ******************/
  if(gc_match_kind(gc, MAJOR_COLLECTION)){
    atomic_cas32( &num_forwarding_collectors, 0, num_active_collectors);
    
    collector_forward_nos_to_sspace(collector, sspace);
    
    atomic_inc32(&num_forwarding_collectors);
    while(num_forwarding_collectors != num_active_collectors);
  }
  
  /* Optional Pass: *******************************************
     Compact pfcs with the same size **************************/
  if(sspace->need_compact){
    atomic_cas32(&num_compacting_collectors, 0, num_active_collectors+1);
    
    sspace_compact(collector, sspace);
    
    /* If we need forward nos to mos, i.e. in major collection, an extra fixing phase after compaction is needed. */
    old_num = atomic_inc32(&num_compacting_collectors);
    if( ++old_num == num_active_collectors ){
      sspace_remerge_free_chunks(gc, sspace);
      /* let other collectors go */
      num_compacting_collectors++;
    }
    while(num_compacting_collectors != num_active_collectors + 1);
  }
  
  /* Optional Pass: *******************************************
     Fix repointed refs ***************************************/
  if(sspace->need_fix){
    atomic_cas32( &num_fixing_collectors, 0, num_active_collectors);
    
    sspace_fix_repointed_refs(collector, sspace);
    
    atomic_inc32(&num_fixing_collectors);
    while(num_fixing_collectors != num_active_collectors);
  }
  
  if( collector->thread_handle != 0 )
    return;
  
  /* Leftover: *************************************************/
  
  if(sspace->need_fix){
    gc_fix_rootset(collector);
#ifdef SSPACE_TIME
    sspace_fix_time(FALSE);
#endif
  }
  
  //gc->root_set = NULL;  // FIXME:: should be placed to a more appopriate place
  gc_set_pool_clear(gc->metadata->gc_rootset_pool);
#ifdef USE_MARK_SWEEP_GC
  sspace_set_space_statistic(sspace);
#endif 

  if(gc_match_kind(gc, MAJOR_COLLECTION))
    gc_clear_collector_local_chunks(gc);

#ifdef SSPACE_VERIFY
  sspace_verify_after_collection(gc);
#endif
}
