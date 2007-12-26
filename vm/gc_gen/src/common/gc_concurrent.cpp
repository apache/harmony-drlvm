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
#include "../gen/gc_for_barrier.h"

Boolean USE_CONCURRENT_GC           = FALSE;
Boolean USE_CONCURRENT_ENUMERATION  = FALSE;
Boolean USE_CONCURRENT_MARK         = FALSE;
Boolean USE_CONCURRENT_SWEEP        = FALSE;

volatile Boolean concurrent_mark_phase  = FALSE;
volatile Boolean mark_is_concurrent     = FALSE;
volatile Boolean concurrent_sweep_phase = FALSE;
volatile Boolean sweep_is_concurrent    = FALSE;

volatile Boolean gc_sweeping_global_normal_chunk = FALSE;

unsigned int CONCURRENT_ALGO = 0; 

static void gc_check_concurrent_mark(GC* gc)
{
  if(!is_mark_finished(gc)){
    lock(gc->concurrent_mark_lock);
#ifndef USE_MARK_SWEEP_GC
    gc_gen_start_concurrent_mark((GC_Gen*)gc);
#else
    if(gc_concurrent_match_algorithm(OTF_REM_OBJ_SNAPSHOT_ALGO)){
      gc_ms_start_concurrent_mark((GC_MS*)gc, MIN_NUM_MARKERS);
    }else if(gc_concurrent_match_algorithm(OTF_REM_NEW_TARGET_ALGO)){  
      gc_ms_start_concurrent_mark((GC_MS*)gc, MIN_NUM_MARKERS);
    }else if(gc_concurrent_match_algorithm(MOSTLY_CONCURRENT_ALGO)){
      //ignore.       
    }
#endif  
    unlock(gc->concurrent_mark_lock);
  }
}

static void gc_wait_concurrent_mark_finish(GC* gc)
{
  wait_mark_finish(gc);
  gc_set_barrier_function(WRITE_BARRIER_REM_NIL);
  gc_set_concurrent_status(gc,GC_CONCURRENT_STATUS_NIL);
}

void gc_start_concurrent_mark(GC* gc)
{
  int disable_count;
  
  if(!try_lock(gc->concurrent_mark_lock) || gc_mark_is_concurrent()) return;
    
  /*prepare rootset*/
  if(TRUE){
    lock(gc->enumerate_rootset_lock);
    gc_metadata_verify(gc, TRUE);
    gc_reset_rootset(gc);
    disable_count = hythread_reset_suspend_disable();
    vm_enumerate_root_set_all_threads();
    gc_copy_interior_pointer_table_to_rootset();
    gc_set_rootset(gc); 
  }else{
    gc_clear_remset((GC*)gc); 
    if(!IGNORE_FINREF){
      gc_copy_finaliable_obj_to_rootset(gc);
    }
    gc->root_set = NULL;
  }
  gc_set_concurrent_status(gc, GC_CONCURRENT_MARK_PHASE);

  gc_decide_collection_kind((GC_Gen*)gc, GC_CAUSE_NIL);
  
  /*start concurrent mark*/
#ifndef USE_MARK_SWEEP_GC
  gc_gen_start_concurrent_mark((GC_Gen*)gc);
#else
  if(gc_concurrent_match_algorithm(OTF_REM_OBJ_SNAPSHOT_ALGO)){
    gc_set_barrier_function(WRITE_BARRIER_REM_OBJ_SNAPSHOT);
    gc_ms_start_concurrent_mark((GC_MS*)gc, MIN_NUM_MARKERS);
  }else if(gc_concurrent_match_algorithm(MOSTLY_CONCURRENT_ALGO)){
    gc_set_barrier_function(WRITE_BARRIER_REM_SOURCE_OBJ);
    gc_ms_start_most_concurrent_mark((GC_MS*)gc, MIN_NUM_MARKERS);
  }else if(gc_concurrent_match_algorithm(OTF_REM_NEW_TARGET_ALGO)){  
    gc_set_barrier_function(WRITE_BARRIER_REM_OLD_VAR);
    gc_ms_start_concurrent_mark((GC_MS*)gc, MIN_NUM_MARKERS);
  }
#endif

  if(TRUE){ 
    unlock(gc->enumerate_rootset_lock);
    vm_resume_threads_after();    
    assert(hythread_is_suspend_enabled());
    hythread_set_suspend_disable(disable_count);
  }

  unlock(gc->concurrent_mark_lock);
}

void wspace_mark_scan_mostly_concurrent_reset();
void wspace_mark_scan_mostly_concurrent_terminate();

void gc_finish_concurrent_mark(GC* gc, Boolean is_STW)
{
  gc_check_concurrent_mark(gc);
  
  if(gc_concurrent_match_algorithm(MOSTLY_CONCURRENT_ALGO))
    wspace_mark_scan_mostly_concurrent_terminate();
  
  gc_wait_concurrent_mark_finish(gc);
  
  if(gc_concurrent_match_algorithm(MOSTLY_CONCURRENT_ALGO)){
    /*If gc use mostly concurrent algorithm, there's a final marking pause. 
          Suspend the mutators once again and finish the marking phase.*/
    int disable_count;   
    if(!is_STW){
      /*suspend the mutators.*/   
      lock(gc->enumerate_rootset_lock);
      gc_metadata_verify(gc, TRUE);
      gc_reset_rootset(gc);    
      disable_count = hythread_reset_suspend_disable();
      vm_enumerate_root_set_all_threads();
      gc_copy_interior_pointer_table_to_rootset();
      gc_set_rootset(gc); 
    }

    /*prepare dirty object*/
    gc_prepare_dirty_set(gc);
    
    gc_set_weakref_sets(gc);
        
    /*start STW mark*/
#ifndef USE_MARK_SWEEP_GC
    assert(0);
#else
    gc_ms_start_final_mark_after_concurrent((GC_MS*)gc, MIN_NUM_MARKERS);
#endif
    
    wspace_mark_scan_mostly_concurrent_reset();
    gc_clear_dirty_set(gc);
    if(!is_STW){
      unlock(gc->enumerate_rootset_lock);
      vm_resume_threads_after();    
      assert(hythread_is_suspend_enabled());
      hythread_set_suspend_disable(disable_count);
    }
  }
  gc_reset_dirty_set(gc);
}

void gc_reset_concurrent_mark(GC* gc)
{
  gc->num_active_markers = 0;
  gc_mark_unset_concurrent();
}

int64 gc_get_concurrent_mark_time(GC* gc)
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

void gc_start_concurrent_sweep(GC* gc)
{

  if(!try_lock(gc->concurrent_sweep_lock) || gc_sweep_is_concurrent()) return;

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

  gc_set_concurrent_status(gc, GC_CONCURRENT_SWEEP_PHASE);

  gc_set_weakref_sets(gc);

  /*Note: We assumed that adding entry to weakroot_pool is happened in STW rootset enumeration.
      So, when this assumption changed, we should modified the below function.*/
  gc_identify_dead_weak_roots(gc);
  
  /*start concurrent mark*/
#ifndef USE_MARK_SWEEP_GC
  assert(0);
#else
  gc_ms_start_concurrent_sweep((GC_MS*)gc, MIN_NUM_MARKERS);
#endif

  unlock(gc->concurrent_sweep_lock);
}

void gc_reset_concurrent_sweep(GC* gc)
{
  gc->num_active_collectors = 0;
  gc_sweep_unset_concurrent();
}

void gc_wait_concurrent_sweep_finish(GC* gc)
{
  wait_collection_finish(gc);  
  gc_set_concurrent_status(gc,GC_CONCURRENT_STATUS_NIL);
}

void gc_finish_concurrent_sweep(GC * gc)
{
  gc_wait_concurrent_sweep_finish(gc);
}

void gc_check_concurrent_phase(GC * gc)
{
  /*Note: we do not finish concurrent mark here if we do not want to start concurrent sweep.*/
  if(gc_is_concurrent_mark_phase(gc) && is_mark_finished(gc) && USE_CONCURRENT_SWEEP){
    /*Although all conditions above are satisfied, we can not guarantee concurrent marking is finished.
          Because, sometimes, the concurrent marking has not started yet. We check the concurrent mark lock
          here to guarantee this occasional case.*/
    if(try_lock(gc->concurrent_mark_lock)){
      unlock(gc->concurrent_mark_lock);
      gc_finish_concurrent_mark(gc, FALSE);
    }
  }

  if(gc_is_concurrent_sweep_phase(gc) && is_collector_finished(gc)){
    //The reason is same as concurrent mark above.
    if(try_lock(gc->concurrent_sweep_lock)){
      unlock(gc->concurrent_sweep_lock);
      gc_finish_concurrent_sweep(gc);
    }
  }
}


void gc_reset_after_concurrent_collection(GC* gc)
{
  /*FIXME: enable concurrent GEN mode.*/
  gc_reset_interior_pointer_table();
  if(gc_is_gen_mode()) gc_prepare_mutator_remset(gc);
  
  /* Clear rootset pools here rather than in each collection algorithm */
  gc_clear_rootset(gc);
    
  if(!IGNORE_FINREF ){
    INFO2("gc.process", "GC: finref process after collection ...\n");
    gc_put_finref_to_vm(gc);
    gc_reset_finref_metadata(gc);
    gc_activate_finref_threads((GC*)gc);
#ifndef BUILD_IN_REFERENT
  } else {
    gc_clear_weakref_pools(gc);
#endif
  }

#ifdef USE_MARK_SWEEP_GC
  gc_ms_update_space_statistics((GC_MS*)gc);
#endif

  gc_clear_dirty_set(gc);

  vm_reclaim_native_objs();
  gc->in_collection = FALSE;

  gc_reset_collector_state(gc);
  
  if(USE_CONCURRENT_GC && gc_mark_is_concurrent()){
    gc_reset_concurrent_mark(gc);    
  }

  if(USE_CONCURRENT_GC && gc_sweep_is_concurrent()){
    gc_reset_concurrent_sweep(gc);
  }
}

void gc_decide_concurrent_algorithm(GC* gc, char* concurrent_algo)
{
  if(!concurrent_algo){
    CONCURRENT_ALGO = OTF_REM_OBJ_SNAPSHOT_ALGO;
  }else{
    string_to_upper(concurrent_algo);
     
    if(!strcmp(concurrent_algo, "OTF_OBJ")){  
      CONCURRENT_ALGO = OTF_REM_OBJ_SNAPSHOT_ALGO;
      
    }else if(!strcmp(concurrent_algo, "MOSTLY_CON")){
      CONCURRENT_ALGO = MOSTLY_CONCURRENT_ALGO;  
    }else if(!strcmp(concurrent_algo, "OTF_SLOT")){
      CONCURRENT_ALGO = OTF_REM_NEW_TARGET_ALGO;  
    }
  }
}


