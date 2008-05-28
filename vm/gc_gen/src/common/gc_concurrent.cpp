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
#include "gc_metadata.h"
#include "../thread/mutator.h"
#include "../thread/marker.h"
#include "../thread/collector.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "interior_pointer.h"
#include "collection_scheduler.h"
#include "gc_concurrent.h"
#include "../common/gc_for_barrier.h"

volatile BOOLEAN concurrent_in_marking  = FALSE;
volatile BOOLEAN concurrent_in_sweeping = FALSE;
volatile BOOLEAN mark_is_concurrent     = FALSE;
volatile BOOLEAN sweep_is_concurrent    = FALSE;

volatile BOOLEAN gc_sweep_global_normal_chunk = FALSE;

static void gc_check_con_mark(GC* gc)
{
  if(!is_mark_finished(gc)){
    lock(gc->lock_con_mark);
    if(gc_is_kind(ALGO_CON_OTF_OBJ)){
      gc_ms_start_con_mark((GC_MS*)gc, MIN_NUM_MARKERS);
    }else if(gc_is_kind(ALGO_CON_OTF_REF)){
      gc_ms_start_con_mark((GC_MS*)gc, MIN_NUM_MARKERS);
    }else if(gc_is_kind(ALGO_CON_MOSTLY)){
      //ignore.   
    }
    unlock(gc->lock_con_mark);
  }
}

static void gc_wait_con_mark_finish(GC* gc)
{
  wait_mark_finish(gc);
  gc_set_barrier_function(WB_REM_NIL);
  gc_set_concurrent_status(gc,GC_CON_STATUS_NIL);
}

unsigned int gc_decide_marker_number(GC* gc);

void gc_start_con_mark(GC* gc)
{
  int disable_count;
  unsigned int num_marker;
  
  if(!try_lock(gc->lock_con_mark) || gc_mark_is_concurrent()) return;
    
  lock(gc->lock_enum);
  disable_count = hythread_reset_suspend_disable();
  int64 pause_start = time_now();
  gc_set_rootset_type(ROOTSET_IS_OBJ);  
  gc_prepare_rootset(gc);
  
  gc_set_concurrent_status(gc, GC_CON_MARK_PHASE);

  num_marker = gc_decide_marker_number(gc);
  
  /*start concurrent mark*/
  if(gc_is_kind(ALGO_CON_OTF_OBJ)){
    gc_set_barrier_function(WB_REM_OBJ_SNAPSHOT);
    gc_ms_start_con_mark((GC_MS*)gc, num_marker);
  }else if(gc_is_kind(ALGO_CON_MOSTLY)){
    gc_set_barrier_function(WB_REM_SOURCE_OBJ);
    gc_ms_start_mostly_con_mark((GC_MS*)gc, num_marker);
  }else if(gc_is_kind(ALGO_CON_OTF_REF)){  
    gc_set_barrier_function(WB_REM_OLD_VAR);
    gc_ms_start_con_mark((GC_MS*)gc, num_marker);
  }

  unlock(gc->lock_enum);      
  INFO2("gc.con.time","[GC][Con]pause(enumeration root):    "<<((unsigned int)((time_now()-pause_start)>>10))<<"  ms ");
  vm_resume_threads_after();    
  assert(hythread_is_suspend_enabled());
  hythread_set_suspend_disable(disable_count);

  unlock(gc->lock_con_mark);
}

void mostly_con_mark_terminate_reset();
void terminate_mostly_con_mark();

void gc_finish_con_mark(GC* gc, BOOLEAN need_STW)
{
  gc_check_con_mark(gc);
  
  if(gc_is_kind(ALGO_CON_MOSTLY))
    terminate_mostly_con_mark();
  
  gc_wait_con_mark_finish(gc);

  int disable_count;   
  if(need_STW){
    /*suspend the mutators.*/   
    lock(gc->lock_enum);
    if(gc_is_kind(ALGO_CON_MOSTLY)){
      /*In mostly concurrent algorithm, there's a final marking pause. 
            Prepare root set for final marking.*/
      disable_count = hythread_reset_suspend_disable();      
      gc_set_rootset_type(ROOTSET_IS_OBJ);
      gc_prepare_rootset(gc);
    }else{
      disable_count = vm_suspend_all_threads();
    }
  }

  if(gc_is_kind(ALGO_CON_MOSTLY)){
    /*In mostly concurrent algorithm, there's a final marking pause. 
          Suspend the mutators once again and finish the marking phase.*/

    /*prepare dirty object*/
    gc_prepare_dirty_set(gc);
    
    gc_set_weakref_sets(gc);
        
    /*start STW mark*/
    gc_ms_start_mostly_con_final_mark((GC_MS*)gc, MIN_NUM_MARKERS);
    
    mostly_con_mark_terminate_reset();
    gc_clear_dirty_set(gc);
  }

  gc_reset_dirty_set(gc);
  
  if(need_STW){
    unlock(gc->lock_enum);
    if(gc_is_kind(ALGO_CON_MOSTLY)){
      vm_resume_threads_after();    
      assert(hythread_is_suspend_enabled());
      hythread_set_suspend_disable(disable_count);
    }else{
      vm_resume_all_threads(disable_count);
    }
  }
  
}

void gc_reset_con_mark(GC* gc)
{
  gc->num_active_markers = 0;
  gc_mark_unset_concurrent();
}

int64 gc_get_con_mark_time(GC* gc)
{
  int64 time_mark = 0;
  Marker** markers = gc->markers;
  unsigned int i;
  for(i = 0; i < gc->num_active_markers; i++){
    Marker* marker = markers[i];
    if(marker->time_mark > time_mark){
      time_mark = marker->time_mark;
    }
    marker->time_mark = 0;
  }
  return time_mark;
}

void gc_start_con_sweep(GC* gc)
{
  if(!try_lock(gc->lock_con_sweep) || gc_sweep_is_concurrent()) return;

  /*FIXME: enable finref*/
  if(!IGNORE_FINREF ){ 
    gc_set_obj_with_fin(gc);
    Collector* collector = gc->collectors[0];
    collector_identify_finref(collector);
#ifndef BUILD_IN_REFERENT
  }else{
    gc_set_weakref_sets(gc);
    gc_update_weakref_ignore_finref(gc);
#endif
  }

  gc_set_concurrent_status(gc, GC_CON_SWEEP_PHASE);

  gc_set_weakref_sets(gc);

  /*Note: We assumed that adding entry to weakroot_pool is happened in STW rootset enumeration.
      So, when this assumption changed, we should modified the below function.*/
  gc_identify_dead_weak_roots(gc);
  
  /*start concurrent mark*/
  gc_ms_start_con_sweep((GC_MS*)gc, MIN_NUM_MARKERS);

  unlock(gc->lock_con_sweep);
}

void gc_reset_con_sweep(GC* gc)
{
  gc->num_active_collectors = 0;
  gc_sweep_unset_concurrent();
}

void gc_wait_con_sweep_finish(GC* gc)
{
  wait_collection_finish(gc);  
  gc_set_concurrent_status(gc,GC_CON_STATUS_NIL);
}

void gc_finish_con_sweep(GC * gc)
{
  gc_wait_con_sweep_finish(gc);
}

void gc_try_finish_con_phase(GC * gc)
{
  /*Note: we do not finish concurrent mark here if we do not want to start concurrent sweep.*/
  if(gc_con_is_in_marking(gc) && is_mark_finished(gc)){
    /*Although all conditions above are satisfied, we can not guarantee concurrent marking is finished.
          Because, sometimes, the concurrent marking has not started yet. We check the concurrent mark lock
          here to guarantee this occasional case.*/
    if(try_lock(gc->lock_con_mark)){
      unlock(gc->lock_con_mark);
      gc_finish_con_mark(gc, TRUE);
    }
  }

  if(gc_con_is_in_sweeping(gc) && is_collector_finished(gc)){
    //The reason is same as concurrent mark above.
    if(try_lock(gc->lock_con_sweep)){
      unlock(gc->lock_con_sweep);
      gc_finish_con_sweep(gc);
    }
  }
}

void gc_reset_after_collection(GC* gc, int64 time_mutator, int64 time_collection);

void gc_reset_after_con_collect(GC* gc)
{
  assert(gc_is_specify_con_gc());
  
  int64 time_mutator = gc_get_mutator_time(gc);
  int64 time_collection = gc_get_collector_time(gc) + gc_get_marker_time(gc);

  gc_reset_interior_pointer_table();

  gc_reset_after_collection(gc, time_mutator, time_collection);
  
  if(gc_mark_is_concurrent()){
    gc_reset_con_mark(gc);    
  }

  if(gc_sweep_is_concurrent()){
    gc_reset_con_sweep(gc);
  }
}

void gc_finish_con_GC(GC* gc, int64 time_mutator)
{
  int64 time_collection_start = time_now();
  
  gc->num_collections++;

  lock(gc->lock_enum);

  int disable_count = hythread_reset_suspend_disable();  
  gc_set_rootset_type(ROOTSET_IS_REF);
  gc_prepare_rootset(gc);
  unlock(gc->lock_enum);
  
  if(gc_sweep_is_concurrent()){
    if(gc_con_is_in_sweeping())
      gc_finish_con_sweep(gc);
  }else{
    if(gc_con_is_in_marking()){
      gc_finish_con_mark(gc, FALSE);
    }
    gc->in_collection = TRUE;
    gc_reset_mutator_context(gc);
    if(!IGNORE_FINREF ) gc_set_obj_with_fin(gc);
    gc_ms_reclaim_heap((GC_MS*)gc);
  }
  
  int64 time_collection = 0;
  if(gc_mark_is_concurrent()){
    time_collection = gc_get_con_mark_time(gc);
    gc_reset_con_mark(gc);
  }else{
    time_collection = time_now()-time_collection_start;
  }

  if(gc_sweep_is_concurrent()){
    gc_reset_con_sweep(gc);
  }

  gc_reset_after_collection(gc, time_mutator, time_collection);
  
  gc_start_mutator_time_measure(gc);
  
  vm_resume_threads_after();
  assert(hythread_is_suspend_enabled());
  hythread_set_suspend_disable(disable_count);  
  int64 pause_time = time_now()-time_collection_start;

  if(GC_CAUSE_RUNTIME_FORCE_GC == gc->cause){
    INFO2("gc.con.time","[GC][Con]pause(   Forcing GC   ):    "<<((unsigned int)(pause_time>>10))<<"  ms ");
  }else{
    INFO2("gc.con.time","[GC][Con]pause( Heap exhuasted ):    "<<((unsigned int)(pause_time>>10))<<"  ms ");
  }
  return;
}

void gc_set_default_con_algo()
{
  assert((GC_PROP & ALGO_CON_MASK) == 0);
  GC_PROP |= ALGO_CON_OTF_OBJ;
}

void gc_decide_con_algo(char* concurrent_algo)
{
  string_to_upper(concurrent_algo);
  GC_PROP &= ~ALGO_CON_MASK;
  if(!strcmp(concurrent_algo, "OTF_OBJ")){ 
    GC_PROP |= ALGO_CON_OTF_OBJ;
  }else if(!strcmp(concurrent_algo, "MOSTLY_CON")){
    GC_PROP |= ALGO_CON_MOSTLY;
  }else if(!strcmp(concurrent_algo, "OTF_SLOT")){
    GC_PROP |= ALGO_CON_OTF_REF;
  }
}
