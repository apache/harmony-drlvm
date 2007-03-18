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

#include "open/vm_util.h"

#include "collector.h"
#include "../mark_compact/mspace.h"
#include "../finalizer_weakref/finalizer_weakref.h"

unsigned int MINOR_COLLECTORS = 0;
unsigned int MAJOR_COLLECTORS = 0;

void collector_restore_obj_info(Collector* collector)
{
  Pool *remset_pool = collector->gc->metadata->collector_remset_pool;
  Pool *free_pool = collector->gc->metadata->free_set_pool;
  assert(!collector->rem_set);
  
  while(Vector_Block *oi_block = pool_get_entry(remset_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(oi_block);
    while(!vector_block_iterator_end(oi_block, iter)){
      Partial_Reveal_Object *p_target_obj = (Partial_Reveal_Object *)*iter;
      iter = vector_block_iterator_advance(oi_block, iter);
      Obj_Info_Type obj_info = (Obj_Info_Type)*iter;
      iter = vector_block_iterator_advance(oi_block, iter);
      set_obj_info(p_target_obj, obj_info);
    }
    vector_block_clear(oi_block);
    pool_put_entry(free_pool, oi_block);
  }
}

static void collector_reset_thread(Collector *collector) 
{
  collector->task_func = NULL;

  /*
  vm_reset_event(collector->task_assigned_event);
  vm_reset_event(collector->task_finished_event);
  */
    
  GC_Metadata* metadata = collector->gc->metadata;

/* TO_REMOVE

  assert(collector->rep_set==NULL);
  if( !gc_is_gen_mode() || collector->gc->collect_kind != MINOR_COLLECTION){
    collector->rep_set = free_set_pool_get_entry(metadata);
  }
*/
  
  if(gc_is_gen_mode() && collector->gc->collect_kind==MINOR_COLLECTION && NOS_PARTIAL_FORWARD){
    assert(collector->rem_set==NULL);
    collector->rem_set = free_set_pool_get_entry(metadata);
  }
  
#ifndef BUILD_IN_REFERENT
  collector_reset_weakref_sets(collector);
#endif

  collector->result = TRUE;
  return;
}

static void wait_collector_to_finish(Collector *collector) 
{
  vm_wait_event(collector->task_finished_event);
}

static void notify_collector_to_work(Collector* collector)
{
  vm_set_event(collector->task_assigned_event);  
}

static void collector_wait_for_task(Collector *collector) 
{
  vm_wait_event(collector->task_assigned_event);
}

static void collector_notify_work_done(Collector *collector) 
{
  vm_set_event(collector->task_finished_event);
}

static void assign_collector_with_task(GC* gc, TaskType task_func, Space* space)
{
  /* FIXME:: to adaptively identify the num_collectors_to_activate */
  if( MINOR_COLLECTORS && gc->collect_kind == MINOR_COLLECTION){
    gc->num_active_collectors = MINOR_COLLECTORS;      
  }else if ( MAJOR_COLLECTORS && gc->collect_kind != MINOR_COLLECTION){
    gc->num_active_collectors = MAJOR_COLLECTORS;  
  }else{
    gc->num_active_collectors = gc->num_collectors;
  }
  
  for(unsigned int i=0; i<gc->num_active_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    
    collector_reset_thread(collector);
    collector->task_func = task_func;
    collector->collect_space = space;
    notify_collector_to_work(collector);
  }
  return;
}

static void wait_collection_finish(GC* gc)
{
  unsigned int num_active_collectors = gc->num_active_collectors;
  for(unsigned int i=0; i<num_active_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    wait_collector_to_finish(collector);
  }  
  return;
}

static int collector_thread_func(void *arg) 
{
  Collector *collector = (Collector *)arg;
  assert(collector);
  
  while(true){
    /* Waiting for newly assigned task */
    collector_wait_for_task(collector); 
    
    /* waken up and check for new task */
    TaskType task_func = collector->task_func;
    if(task_func == NULL) return 1;
      
    task_func(collector);

    alloc_context_reset((Allocator*)collector);

    collector_notify_work_done(collector);
  }

  return 0;
}

static void collector_init_thread(Collector *collector) 
{
  collector->rem_set = NULL;
  collector->rep_set = NULL;

  int status = vm_create_event(&collector->task_assigned_event);
  assert(status == THREAD_OK);

  status = vm_create_event(&collector->task_finished_event);
  assert(status == THREAD_OK);

  status = (unsigned int)vm_create_thread(collector_thread_func, (void*)collector);

  assert(status == THREAD_OK);
  
  return;
}

static void collector_terminate_thread(Collector* collector)
{
  collector->task_func = NULL; /* NULL to notify thread exit */
  notify_collector_to_work(collector);
  vm_thread_yield(); /* give collector time to die */
  
  delete collector->trace_stack;  
  return;
}

void collector_destruct(GC* gc) 
{
  for(unsigned int i=0; i<gc->num_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    collector_terminate_thread(collector);
    STD_FREE(collector);
   
  }
  
  STD_FREE(gc->collectors);
  return;
}

unsigned int NUM_COLLECTORS = 0;

struct GC_Gen;
unsigned int gc_get_processor_num(GC_Gen*);

void collector_initialize(GC* gc)
{
  //FIXME::
  unsigned int num_processors = gc_get_processor_num((GC_Gen*)gc);
  
  unsigned int nthreads = max( max( MAJOR_COLLECTORS, MINOR_COLLECTORS), max(NUM_COLLECTORS, num_processors)); 

  unsigned int size = sizeof(Collector *) * nthreads;
  gc->collectors = (Collector **) STD_MALLOC(size); 
  memset(gc->collectors, 0, size);

  size = sizeof(Collector);
  for (unsigned int i = 0; i < nthreads; i++) {
    Collector* collector = (Collector *)STD_MALLOC(size);
    memset(collector, 0, size);
    
    /* FIXME:: thread_handle is for temporary control */
    collector->thread_handle = (VmThreadHandle)i;
    collector->gc = gc;
    collector_init_thread(collector);
    
    gc->collectors[i] = collector;
  }

  gc->num_collectors = NUM_COLLECTORS? NUM_COLLECTORS:num_processors; 

  return;
}

void collector_execute_task(GC* gc, TaskType task_func, Space* space)
{
  assign_collector_with_task(gc, task_func, space);
  wait_collection_finish(gc);
    
  return;
}
