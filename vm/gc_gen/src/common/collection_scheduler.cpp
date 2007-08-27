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

#include "gc_common.h"
#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "../mark_sweep/sspace.h"
#include "collection_scheduler.h"
#include "gc_concurrent.h"
static int64 time_delay_to_start_mark = 0;

void collection_scheduler_initialize(GC* gc)
{
  
  Collection_Scheduler* collection_scheduler = (Collection_Scheduler*) STD_MALLOC(sizeof(Collection_Scheduler));
  assert(collection_scheduler);
  memset(collection_scheduler, 0, sizeof(Collection_Scheduler));
  
  collection_scheduler->gc = gc;
  gc->collection_scheduler = collection_scheduler;
  time_delay_to_start_mark = 0;
  
  return;
}
void collection_scheduler_destruct(GC* gc)
{
  STD_FREE(gc->collection_scheduler);
}

Boolean gc_need_start_concurrent_mark(GC* gc)
{
  if(!USE_CONCURRENT_GC) return FALSE;
  //FIXME: GEN mode also needs the support of starting mark after thread resume.
#ifdef USE_MARK_SWEEP_GC
  if(gc_is_concurrent_mark_phase() ) return FALSE;

  int64 time_current = time_now();
  if( time_current - get_collection_end_time() > time_delay_to_start_mark) 
    return TRUE;
  else return FALSE;
#else
  /*FIXME: concurrent mark is not support in GC_GEN*/
  assert(0);
  if(gc_next_collection_kind((GC_Gen*)gc) == MAJOR_COLLECTION)
    return TRUE;
  else 
    return FALSE;
#endif
}



void gc_update_collection_scheduler(GC* gc, int64 mutator_time, int64 mark_time)
{
  //FIXME: GEN GC should be supportted.
#ifdef USE_MARK_SWEEP_GC

  Collection_Scheduler* collection_scheduler = gc->collection_scheduler;   
  Space* space = NULL;

  space = (Space*) gc_get_sspace(gc);

  Space_Statistics* sspace_stat = space->space_statistic;
  
  unsigned int slot_index = collection_scheduler->last_slot_index_in_window;
  unsigned int num_slot   = collection_scheduler->num_slot_in_window;
  
  collection_scheduler->num_obj_traced_window[slot_index] = sspace_stat->num_live_obj;
  collection_scheduler->size_alloced_window[slot_index] = sspace_stat->last_size_free_space;

  int64 time_mutator = mutator_time;
  int64 time_mark = mark_time;
  
  collection_scheduler->alloc_rate_window[slot_index] 
    = time_mutator == 0 ? 0 : (float)collection_scheduler->size_alloced_window[slot_index] / time_mutator; 
      
  collection_scheduler->trace_rate_window[slot_index]
    = time_mark == 0 ? 0 : (float)collection_scheduler->num_obj_traced_window[slot_index] / time_mark;
  
  collection_scheduler->num_slot_in_window = num_slot >= STATISTICS_SAMPLING_WINDOW_SIZE ? num_slot : (++num_slot);
  collection_scheduler->last_slot_index_in_window = (++slot_index)% STATISTICS_SAMPLING_WINDOW_SIZE;

  float sum_alloc_rate = 0;
  float sum_trace_rate = 0;

  unsigned int i;
  for(i = 0; i < collection_scheduler->num_slot_in_window; i++){
    sum_alloc_rate += collection_scheduler->alloc_rate_window[i];
    sum_trace_rate += collection_scheduler->trace_rate_window[i];
    
  }

  float average_alloc_rate = sum_alloc_rate / collection_scheduler->num_slot_in_window;
  float average_trace_rate = sum_trace_rate / collection_scheduler->num_slot_in_window;

  if(average_alloc_rate == 0 || average_trace_rate == 0){
    time_delay_to_start_mark = 0;
  }else{
    float expected_time_alloc = sspace_stat->size_free_space / average_alloc_rate;
    float expected_time_trace = sspace_stat->num_live_obj / average_trace_rate;


    if(expected_time_alloc > expected_time_trace)
    collection_scheduler->time_delay_to_start_mark = (int64)((expected_time_alloc - expected_time_trace)*0.7);
    else
    collection_scheduler->time_delay_to_start_mark = 0;

    time_delay_to_start_mark = collection_scheduler->time_delay_to_start_mark;
  }  
#endif  
  return;
  
}

