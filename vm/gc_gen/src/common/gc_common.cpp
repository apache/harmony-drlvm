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
 * @author Xiao-Feng Li, 2006/12/3
 */

#include "gc_common.h"
#include "gc_metadata.h"
#include "../thread/mutator.h"
#include "../thread/marker.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "../move_compact/gc_mc.h"
#include "../common/space_tuner.h"
#include "interior_pointer.h"
#include "collection_scheduler.h"
#include "gc_concurrent.h"

unsigned int Cur_Mark_Bit = 0x1;
unsigned int Cur_Forward_Bit = 0x2;

unsigned int SPACE_ALLOC_UNIT;

void gc_assign_free_area_to_mutators(GC* gc)
{
#if !defined(USE_UNIQUE_MARK_SWEEP_GC) && !defined(USE_UNIQUE_MOVE_COMPACT_GC)
  gc_gen_assign_free_area_to_mutators((GC_Gen*)gc);
#endif
}

void gc_init_collector_alloc(GC* gc, Collector* collector)
{
#ifndef USE_UNIQUE_MARK_SWEEP_GC
  gc_gen_init_collector_alloc((GC_Gen*)gc, collector);
#else    
  gc_init_collector_free_chunk_list(collector);
#endif
}

void gc_reset_collector_alloc(GC* gc, Collector* collector)
{
#if !defined(USE_UNIQUE_MARK_SWEEP_GC) && !defined(USE_UNIQUE_MOVE_COMPACT_GC)
  gc_gen_reset_collector_alloc((GC_Gen*)gc, collector);
#endif    
}

void gc_destruct_collector_alloc(GC* gc, Collector* collector)
{
#ifndef USE_UNIQUE_MARK_SWEEP_GC
  gc_gen_destruct_collector_alloc((GC_Gen*)gc, collector);
#endif    
}

void gc_copy_interior_pointer_table_to_rootset();

/*used for computing collection time and mutator time*/
static int64 collection_start_time = time_now();
static int64 collection_end_time = time_now();

int64 get_collection_end_time()
{ return collection_end_time; }

void set_collection_end_time()
{ collection_end_time = time_now(); }

void gc_decide_collection_kind(GC* gc, unsigned int cause)
{
  /* this is for debugging and for gen-nongen-switch. */
  gc->last_collect_kind = GC_PROP;

#if !defined(USE_UNIQUE_MARK_SWEEP_GC) && !defined(USE_UNIQUE_MOVE_COMPACT_GC)
  
  gc_gen_decide_collection_kind((GC_Gen*)gc, cause);

#endif

}

void gc_prepare_rootset(GC* gc)
{
  /* Stop the threads and collect the roots. */
  lock(gc->enumerate_rootset_lock);
  INFO2("gc.process", "GC: stop the threads and enumerate rootset ...\n");
  gc_clear_rootset(gc);
  gc_reset_rootset(gc);
  vm_enumerate_root_set_all_threads();
  gc_copy_interior_pointer_table_to_rootset();
  gc_set_rootset(gc);
  unlock(gc->enumerate_rootset_lock);

}

void gc_reclaim_heap(GC* gc, unsigned int gc_cause)
{
  INFO2("gc.process", "\nGC: GC start ...\n");
  
  collection_start_time = time_now();
  int64 mutator_time = collection_start_time - collection_end_time;

  /* FIXME:: before mutators suspended, the ops below should be very careful
     to avoid racing with mutators. */
  gc->num_collections++;
  gc->cause = gc_cause;

  gc_decide_collection_kind(gc, gc_cause);

#ifdef MARK_BIT_FLIPPING
  if(collect_is_minor()) mark_bit_flip();
#endif

  if(!USE_CONCURRENT_GC){
    gc_metadata_verify(gc, TRUE);
#ifndef BUILD_IN_REFERENT
    gc_finref_metadata_verify((GC*)gc, TRUE);
#endif
  }
  int disable_count = hythread_reset_suspend_disable();
  /* Stop the threads and collect the roots. */
  gc_prepare_rootset(gc);
  
  if(USE_CONCURRENT_GC && gc_sweep_is_concurrent()){
    if(gc_is_concurrent_sweep_phase())
      gc_finish_concurrent_sweep(gc);
  }else{
    if(USE_CONCURRENT_GC && gc_is_concurrent_mark_phase()){
      gc_finish_concurrent_mark(gc, TRUE);
    }  
  
    gc->in_collection = TRUE;
    
    /* this has to be done after all mutators are suspended */
    gc_reset_mutator_context(gc);
    
    if(!IGNORE_FINREF ) gc_set_obj_with_fin(gc);

#if defined(USE_UNIQUE_MARK_SWEEP_GC)
    gc_ms_reclaim_heap((GC_MS*)gc);
#elif defined(USE_UNIQUE_MOVE_COMPACT_GC)
    gc_mc_reclaim_heap((GC_MC*)gc);
#else
    gc_gen_reclaim_heap((GC_Gen*)gc, collection_start_time);
#endif

  }

  collection_end_time = time_now(); 

#if !defined(USE_UNIQUE_MARK_SWEEP_GC)&&!defined(USE_UNIQUE_MOVE_COMPACT_GC)
  gc_gen_collection_verbose_info((GC_Gen*)gc, collection_end_time - collection_start_time, mutator_time);
  gc_gen_space_verbose_info((GC_Gen*)gc);
#endif

  if(gc_is_gen_mode()) gc_prepare_mutator_remset(gc);
  
  int64 collection_time = 0;
  if(USE_CONCURRENT_GC && gc_mark_is_concurrent()){
    collection_time = gc_get_concurrent_mark_time(gc);
    gc_reset_concurrent_mark(gc);
  }else{
    collection_time = time_now()-collection_start_time;
   }

  if(USE_CONCURRENT_GC && gc_sweep_is_concurrent()){
    gc_reset_concurrent_sweep(gc);
  }

#if !defined(USE_UNIQUE_MARK_SWEEP_GC)&&!defined(USE_UNIQUE_MOVE_COMPACT_GC)
  if(USE_CONCURRENT_GC && gc_need_start_concurrent_mark(gc))
    gc_start_concurrent_mark(gc);
#endif

  /* Clear rootset pools here rather than in each collection algorithm */
  gc_clear_rootset(gc);
  
  gc_metadata_verify(gc, FALSE);
  
  if(!IGNORE_FINREF ){
    INFO2("gc.process", "GC: finref process after collection ...\n");
    gc_put_finref_to_vm(gc);
    gc_reset_finref_metadata(gc);
    gc_activate_finref_threads((GC*)gc);
#ifndef BUILD_IN_REFERENT
  } else {
    gc_clear_weakref_pools(gc);
    gc_clear_finref_repset_pool(gc);
#endif
  }

#ifdef USE_UNIQUE_MARK_SWEEP_GC
  gc_ms_update_space_statistics((GC_MS*)gc);
#endif

  gc_assign_free_area_to_mutators(gc);
  
  if(USE_CONCURRENT_GC) gc_update_collection_scheduler(gc, mutator_time, collection_time);

#ifdef USE_UNIQUE_MARK_SWEEP_GC
  gc_ms_reset_space_statistics((GC_MS*)gc);
#endif

  vm_reclaim_native_objs();
  gc->in_collection = FALSE;

  gc_reset_collector_state(gc);

  gc_clear_dirty_set(gc);
  
  vm_resume_threads_after();
  assert(hythread_is_suspend_enabled());
  hythread_set_suspend_disable(disable_count);
  INFO2("gc.process", "GC: GC end\n");
  int64 pause_time = time_now()-collection_start_time;
  INFO2("gc.con","pause time:  "<<((unsigned int)(pause_time>>10))<<"  ms \n");
  return;
}





