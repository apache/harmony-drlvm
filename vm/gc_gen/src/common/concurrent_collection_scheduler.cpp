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
#include "../mark_sweep/wspace.h"
#include "collection_scheduler.h"
#include "concurrent_collection_scheduler.h"
#include "gc_concurrent.h"
#include "../thread/marker.h"
#include "../verify/verify_live_heap.h"

#define NUM_TRIAL_COLLECTION 2
#define MIN_DELAY_TIME 0x0
#define MAX_DELAY_TIME 0x7fFfFfFf
#define MAX_TRACING_RATE 100
#define MIN_TRACING_RATE 1
#define MAX_SPACE_THRESHOLD (POINTER_SIZE_INT)((POINTER_SIZE_INT)1<<(BITS_OF_POINTER_SIZE_INT-1))
#define MIN_SPACE_THRESHOLD 0

enum CC_Scheduler_Kind{
  SCHEDULER_NIL = 0x00,
  TIME_BASED_SCHEDULER = 0x01,
  SPACE_BASED_SCHEDULER = 0x02
};

static unsigned int cc_scheduler_kind = SCHEDULER_NIL;

void gc_enable_time_scheduler()
{ cc_scheduler_kind |= TIME_BASED_SCHEDULER; }

void gc_enable_space_scheduler()
{ cc_scheduler_kind |= SPACE_BASED_SCHEDULER; }

Boolean gc_use_time_scheduler()
{ return cc_scheduler_kind & TIME_BASED_SCHEDULER; }

Boolean gc_use_space_scheduler()
{ return cc_scheduler_kind & SPACE_BASED_SCHEDULER; }

static int64 time_delay_to_start_mark = MAX_DELAY_TIME;
static POINTER_SIZE_INT space_threshold_to_start_mark = MAX_SPACE_THRESHOLD;

void con_collection_scheduler_initialize(GC* gc)
{
  Con_Collection_Scheduler* cc_scheduler = (Con_Collection_Scheduler*) STD_MALLOC(sizeof(Con_Collection_Scheduler));
  assert(cc_scheduler);
  memset(cc_scheduler, 0, sizeof(Con_Collection_Scheduler));
  
  cc_scheduler->gc = gc;
  gc->collection_scheduler = (Collection_Scheduler*)cc_scheduler;
  time_delay_to_start_mark = MAX_DELAY_TIME;
  space_threshold_to_start_mark = MAX_SPACE_THRESHOLD;
  
  return;
}

void con_collection_scheduler_destruct(GC* gc)
{
  STD_FREE(gc->collection_scheduler);
}

void gc_decide_cc_scheduler_kind(char* cc_scheduler)
{
  string_to_upper(cc_scheduler);
  if(!strcmp(cc_scheduler, "time")){
    gc_enable_time_scheduler();
  }else if(!strcmp(cc_scheduler, "space")){
    gc_enable_space_scheduler();
  }else if(!strcmp(cc_scheduler, "all")){
    gc_enable_time_scheduler();
    gc_enable_space_scheduler();
  }
}

void gc_set_default_cc_scheduler_kind()
{
  gc_enable_time_scheduler();
}

static Boolean time_to_start_mark(GC* gc)
{
  if(!gc_use_time_scheduler()) return FALSE;
  
  int64 time_current = time_now();
  return (time_current - get_collection_end_time()) > time_delay_to_start_mark;
}

static Boolean space_to_start_mark(GC* gc)
{
  if(!gc_use_space_scheduler()) return FALSE;

  POINTER_SIZE_INT size_new_obj = gc_get_new_object_size(gc,FALSE);
  return (size_new_obj > space_threshold_to_start_mark); 
}

static Boolean gc_need_start_con_mark(GC* gc)
{
  if(!gc_is_specify_con_mark() || gc_mark_is_concurrent()) return FALSE;
  
  if(time_to_start_mark(gc) || space_to_start_mark(gc)) 
    return TRUE;
  else 
    return FALSE;
}

static Boolean gc_need_start_con_sweep(GC* gc)
{
  if(!gc_is_specify_con_sweep() || gc_sweep_is_concurrent()) return FALSE;

  /*if mark is concurrent and STW GC has not started, we should start concurrent sweep*/
  if(gc_mark_is_concurrent() && !gc_con_is_in_marking(gc))
    return TRUE;
  else
    return FALSE;
}

static Boolean gc_need_reset_after_con_collect(GC* gc)
{
  if(gc_sweep_is_concurrent() && !gc_con_is_in_sweeping(gc))
    return TRUE;
  else
    return FALSE;
}

static Boolean gc_need_start_con_enum(GC* gc)
{
  /*TODO: support on-the-fly root set enumeration.*/
  return FALSE;
}

#define SPACE_UTIL_RATIO_CORRETION 0.2f
#define TIME_CORRECTION_OTF_MARK 0.65f
#define TIME_CORRECTION_OTF_MARK_SWEEP 1.0f
#define TIME_CORRECTION_MOSTLY_MARK 0.5f

static void con_collection_scheduler_update_stat(GC* gc, int64 time_mutator, int64 time_collection)
{  
  Space* space = NULL;
  Con_Collection_Scheduler* cc_scheduler = (Con_Collection_Scheduler*)gc->collection_scheduler;

#ifdef USE_UNIQUE_MARK_SWEEP_GC
  space = (Space*) gc_get_wspace(gc);
#endif  
  if(!space) return;

  Space_Statistics* space_stat = space->space_statistic;
  
  unsigned int slot_index = cc_scheduler->last_window_index;
  unsigned int num_slot   = cc_scheduler->num_window_slots;
  
  cc_scheduler->trace_load_window[slot_index] = space_stat->num_live_obj;
  cc_scheduler->alloc_load_window[slot_index] = space_stat->size_new_obj;
  cc_scheduler->space_utilization_ratio[slot_index] = space_stat->space_utilization_ratio;

  cc_scheduler->last_mutator_time = time_mutator;
  cc_scheduler->last_collector_time = time_collection;
  
  if(NUM_TRIAL_COLLECTION == 0 || gc->num_collections < NUM_TRIAL_COLLECTION)
    return;
  
  cc_scheduler->alloc_rate_window[slot_index] 
    = time_mutator == 0 ? 0 : (float)cc_scheduler->alloc_load_window[slot_index] / time_mutator; 

  if(gc_mark_is_concurrent()){
    cc_scheduler->trace_rate_window[slot_index]
      = time_collection == 0 ? MAX_TRACING_RATE : (float)cc_scheduler->trace_load_window[slot_index] / time_collection;
  }else{
    cc_scheduler->trace_rate_window[slot_index] = MIN_TRACING_RATE;
  }

  cc_scheduler->num_window_slots = num_slot >= STAT_SAMPLE_WINDOW_SIZE ? num_slot : (++num_slot);
  cc_scheduler->last_window_index = (++slot_index)% STAT_SAMPLE_WINDOW_SIZE;  
}

static void con_collection_scheduler_update_start_point(GC* gc, int64 time_mutator, int64 time_collection)
{
  if(NUM_TRIAL_COLLECTION == 0 || gc->num_collections < NUM_TRIAL_COLLECTION)
    return;

  Space* space = NULL;
#ifdef USE_UNIQUE_MARK_SWEEP_GC
  space = (Space*) gc_get_wspace(gc);
#endif  
  if(!space) return;

  Space_Statistics* space_stat = space->space_statistic;

  float sum_alloc_rate = 0;
  float sum_trace_rate = 0;
  float sum_space_util_ratio = 0;

  Con_Collection_Scheduler* cc_scheduler = (Con_Collection_Scheduler*)gc->collection_scheduler;   
  
  int64 time_this_collection_correction = 0;
#if 0
  float space_util_ratio = space_stat->space_utilization_ratio;
  if(space_util_ratio > (1-SPACE_UTIL_RATIO_CORRETION)){
    time_this_collection_correction = 0;
  }else{
    time_this_collection_correction 
      = (int64)(((1 - space_util_ratio - SPACE_UTIL_RATIO_CORRETION)/(space_util_ratio))* time_mutator);
  }
#endif

  unsigned int i;
  for(i = 0; i < cc_scheduler->num_window_slots; i++){
    sum_alloc_rate += cc_scheduler->alloc_rate_window[i];
    sum_trace_rate += cc_scheduler->trace_rate_window[i];
    sum_space_util_ratio += cc_scheduler->space_utilization_ratio[i];
  }

  TRACE2("gc.con.cs","Allocation Rate: ");
  for(i = 0; i < cc_scheduler->num_window_slots; i++){
    TRACE2("gc.con.cs",i+1<<"--"<<cc_scheduler->alloc_rate_window[i]);
  }

  TRACE2("gc.con.cs","Tracing Rate: ");
  for(i = 0; i < cc_scheduler->num_window_slots; i++){
    TRACE2("gc.con.cs",i+1<<"--"<<cc_scheduler->trace_rate_window[i]);
  }

  float average_alloc_rate = sum_alloc_rate / cc_scheduler->num_window_slots;
  float average_trace_rate = sum_trace_rate / cc_scheduler->num_window_slots;
  float average_space_util_ratio = sum_space_util_ratio / cc_scheduler->num_window_slots;

  TRACE2("gc.con.cs","averAllocRate: "<<average_alloc_rate<<"averTraceRate: "<<average_trace_rate<<"  average_space_util_ratio: "<<average_space_util_ratio<<" ");

  if(average_alloc_rate == 0 ){
    time_delay_to_start_mark = MIN_DELAY_TIME;
    space_threshold_to_start_mark = MIN_SPACE_THRESHOLD;
  }else if(average_trace_rate == 0){
    time_delay_to_start_mark = MAX_DELAY_TIME;
    space_threshold_to_start_mark = MAX_SPACE_THRESHOLD;
  }else{
    float time_alloc_expected = (space_stat->size_free_space * average_space_util_ratio) / average_alloc_rate;
    float time_trace_expected = space_stat->num_live_obj / average_trace_rate;
    TRACE2("gc.con.cs","[GC][Con] expected alloc time "<<time_alloc_expected<<"  expected collect time  "<<time_trace_expected<<" ");

    if(time_alloc_expected > time_trace_expected){
      if(gc_is_kind(ALGO_CON_OTF_OBJ)||gc_is_kind(ALGO_CON_OTF_REF)){
        float time_correction = gc_sweep_is_concurrent()? TIME_CORRECTION_OTF_MARK_SWEEP : TIME_CORRECTION_OTF_MARK;
        cc_scheduler->time_delay_to_start_mark = (int64)((time_alloc_expected - time_trace_expected)*time_correction);
      }else if(gc_is_kind(ALGO_CON_MOSTLY)){
        cc_scheduler->time_delay_to_start_mark = (int64)(time_mutator* TIME_CORRECTION_MOSTLY_MARK);
      }
    }else{
      cc_scheduler->time_delay_to_start_mark = MIN_DELAY_TIME;
    }

    cc_scheduler->space_threshold_to_start_mark = 
      (POINTER_SIZE_INT)(space_stat->size_free_space * ((time_alloc_expected - time_trace_expected) / time_alloc_expected));

    time_delay_to_start_mark = cc_scheduler->time_delay_to_start_mark + time_this_collection_correction;
    space_threshold_to_start_mark = cc_scheduler->space_threshold_to_start_mark;
  }
  TRACE2("gc.con.cs","[GC][Con] concurrent marking will delay "<<(unsigned int)(time_delay_to_start_mark>>10)<<" ms ");
  TRACE2("gc.con.cs","[GC][Con] time correction "<<(unsigned int)(time_this_collection_correction>>10)<<" ms ");

}

void gc_update_con_collection_scheduler(GC* gc, int64 time_mutator, int64 time_collection)
{
  assert(gc_is_specify_con_gc());
  if(GC_CAUSE_RUNTIME_FORCE_GC == gc->cause) return;
  
  con_collection_scheduler_update_stat(gc, time_mutator, time_collection);
  con_collection_scheduler_update_start_point(gc, time_mutator, time_collection);

  return;
}

Boolean gc_sched_con_collection(GC* gc, unsigned int gc_cause)
{
  if(!try_lock(gc->lock_collect_sched)) return FALSE;
  vm_gc_lock_enum();    

  gc_try_finish_con_phase(gc);

  if(gc_need_start_con_enum(gc)){
    /*TODO:Concurrent rootset enumeration.*/
    assert(0);
  }
  
  if(gc_need_start_con_mark(gc)){
    INFO2("gc.con.info", "[GC][Con] concurrent mark start ...");
    gc_start_con_mark(gc);
    vm_gc_unlock_enum();
    unlock(gc->lock_collect_sched);
    return TRUE;
  }

  if(gc_need_start_con_sweep(gc)){
    gc->num_collections++;
    INFO2("gc.con.info", "[GC][Con] collection number:"<< gc->num_collections<<" ");
    gc_start_con_sweep(gc);
    vm_gc_unlock_enum();
    unlock(gc->lock_collect_sched);
    return TRUE;
  }

  if(gc_need_reset_after_con_collect(gc)){
    int64 pause_start = time_now();
    int disable_count = vm_suspend_all_threads();
    gc_reset_after_con_collect(gc);
    gc_start_mutator_time_measure(gc);
    set_collection_end_time();
    vm_resume_all_threads(disable_count);
    vm_gc_unlock_enum();
    INFO2("gc.con.time","[GC][Con]pause(reset collection):    "<<((unsigned int)((time_now()-pause_start)>>10))<<"  ms ");
    unlock(gc->lock_collect_sched);
    return TRUE;
  }
  vm_gc_unlock_enum();
  unlock(gc->lock_collect_sched);
  return FALSE;
}

extern unsigned int NUM_MARKERS;

unsigned int gc_decide_marker_number(GC* gc)
{
  unsigned int num_active_marker;
  Con_Collection_Scheduler* cc_scheduler = (Con_Collection_Scheduler*)gc->collection_scheduler;   

  /*If the number of markers is specfied, just return the specified value.*/
  if(NUM_MARKERS != 0) return NUM_MARKERS;

  /*If the number of markers isn't specified, we decide the value dynamically.*/
  if(NUM_TRIAL_COLLECTION == 0 || gc->num_collections < NUM_TRIAL_COLLECTION){
    /*Start trial cycle, collection set to 1 in trial cycle and */
    num_active_marker = 1;
  }else{
    num_active_marker = cc_scheduler->last_marker_num;
    int64 c_time = cc_scheduler->last_collector_time;
    int64 m_time = cc_scheduler->last_mutator_time;
    int64 d_time = cc_scheduler->time_delay_to_start_mark;

    if(num_active_marker == 0) num_active_marker = 1;

    if((c_time + d_time) > m_time || (float)d_time < (m_time * 0.25)){      
      TRACE2("gc.con.cs","[GC][Con] increase marker number.");
      num_active_marker ++;
      if(num_active_marker > gc->num_markers) num_active_marker = gc->num_markers;
    }else if((float)d_time > (m_time * 0.6)){
      TRACE2("gc.con.cs","[GC][Con] decrease marker number.");
      num_active_marker --;
      if(num_active_marker == 0)  num_active_marker = 1;
    }
    
    TRACE2("gc.con.cs","[GC][Con] ctime  "<<(unsigned)(c_time>>10)<<"  mtime  "<<(unsigned)(m_time>>10)<<"  dtime  "<<(unsigned)(d_time>>10));
    TRACE2("gc.con.cs","[GC][Con] marker num : "<<num_active_marker<<" ");
  }

  cc_scheduler->last_marker_num = num_active_marker;
  return num_active_marker;
}

