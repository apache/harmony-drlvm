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
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "interior_pointer.h"
#include "collection_scheduler.h"
#include "gc_concurrent.h"

Boolean USE_CONCURRENT_GC = FALSE;

volatile Boolean concurrent_mark_phase = FALSE;
volatile Boolean mark_is_concurrent = FALSE;


static void gc_check_concurrent_mark(GC* gc)
{
  if(!is_mark_finished(gc)){
    lock(gc->concurrent_mark_lock);
#ifndef USE_MARK_SWEEP_GC
    gc_gen_start_concurrent_mark((GC_Gen*)gc);
#else
    gc_ms_start_concurrent_mark((GC_MS*)gc, MAX_NUM_MARKERS);
#endif  
    unlock(gc->concurrent_mark_lock);
  }
}

static void gc_wait_concurrent_mark_finish(GC* gc)
{
  wait_mark_finish(gc);
  gc_set_concurrent_status(gc,GC_CONCURRENT_STATUS_NIL);
  gc_reset_snaptshot(gc);
}

void gc_start_concurrent_mark(GC* gc)
{
  if(!try_lock(gc->concurrent_mark_lock) || gc_mark_is_concurrent()) return;
    
  /*prepare rootset*/
  if(TRUE){
    lock(gc->enumerate_rootset_lock);
    gc_metadata_verify(gc, TRUE);
    gc_reset_rootset(gc);
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
  gc_ms_start_concurrent_mark((GC_MS*)gc, MIN_NUM_MARKERS);
#endif

  if(TRUE){ 
    unlock(gc->enumerate_rootset_lock);
    vm_resume_threads_after();    
  }

  unlock(gc->concurrent_mark_lock);
}

void gc_finish_concurrent_mark(GC* gc)
{
  gc_check_concurrent_mark(gc);
  gc_wait_concurrent_mark_finish(gc);
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
  }
  return time_mark;
}


