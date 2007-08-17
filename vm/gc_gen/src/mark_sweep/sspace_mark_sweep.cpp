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

#include "sspace_mark_sweep.h"
#include "sspace_verify.h"
#include "gc_ms.h"
#include "../gen/gen.h"
#include "../thread/collector.h"
#include "../finalizer_weakref/finalizer_weakref.h"


POINTER_SIZE_INT cur_alloc_color = OBJ_COLOR_WHITE;
POINTER_SIZE_INT cur_mark_color = OBJ_COLOR_BLACK;
POINTER_SIZE_INT cur_alloc_mask = ~BLACK_MASK_IN_TABLE;
POINTER_SIZE_INT cur_mark_mask = BLACK_MASK_IN_TABLE;

static void ops_color_flip(void)
{
  POINTER_SIZE_INT temp = cur_alloc_color;
  cur_alloc_color = cur_mark_color;
  cur_mark_color = temp;
  cur_alloc_mask = ~cur_alloc_mask;
  cur_mark_mask = ~cur_mark_mask;
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


static volatile unsigned int num_marking_collectors = 0;
static volatile unsigned int num_sweeping_collectors = 0;

void mark_sweep_sspace(Collector *collector)
{
  GC *gc = collector->gc;
  Sspace *sspace = gc_get_sspace(gc);
  
  unsigned int num_active_collectors = gc->num_active_collectors;
  
  /* Pass 1: **************************************************
     mark all live objects in heap ****************************/
  atomic_cas32(&num_marking_collectors, 0, num_active_collectors+1);
  
  sspace_mark_scan(collector);
  
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
    gc_init_chunk_for_sweep(gc, sspace);
    /* let other collectors go */
    num_marking_collectors++;
  }
  while(num_marking_collectors != num_active_collectors + 1);
  
  /* Pass 2: **************************************************
     sweep dead objects ***************************************/
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
    if(sspace->need_compact){
      sspace_init_pfc_pool_iterator(sspace);
    }
    /* let other collectors go */
    num_sweeping_collectors++;
  }
  while(num_sweeping_collectors != num_active_collectors + 1);
  
  if(sspace->need_compact)
    compact_sspace(collector, sspace);
  
  if( collector->thread_handle != 0 )
    return;
  
  if(sspace->need_compact){
    gc_fix_rootset(collector);
#ifdef SSPACE_TIME
    sspace_fix_time(FALSE);
#endif
  }
  
  gc_collect_free_chunks(gc, sspace);
#ifdef SSPACE_TIME
  sspace_merge_time(FALSE);
#endif

  /* Leftover: ************************************************ */
  
  gc->root_set = NULL;  // FIXME:: should be placed to a more appopriate place
  gc_set_pool_clear(gc->metadata->gc_rootset_pool);

#ifdef SSPACE_VERIFY
  sspace_verify_after_collection(gc);
#endif
}
