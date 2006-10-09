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

static void collector_reset_thread(Collector *collector) 
{
  collector->task_func = NULL;

	vm_reset_event(collector->task_assigned_event);
	vm_reset_event(collector->task_finished_event);
	
	alloc_context_reset((Alloc_Context*)collector);
	
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

void gc_preprocess_collector(Collector*);
void gc_postprocess_collector(Collector*);
static void assign_collector_with_task(GC* gc, TaskType task_func, Space* space)
{
  unsigned int num_collectors_to_activate = gc->num_collectors;
  for(unsigned int i=0; i<num_collectors_to_activate; i++)
  {
    Collector* collector = gc->collectors[i];
    
    gc_preprocess_collector(collector);
    collector_reset_thread(collector);
    collector->task_func = task_func;
    collector->collect_space = space;
    notify_collector_to_work(collector);
  }
  gc->num_active_collectors = num_collectors_to_activate;

}

static void wait_collection_finish(GC* gc)
{
  unsigned int num_active_collectors = gc->num_active_collectors;
  for(unsigned int i=0; i<num_active_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    wait_collector_to_finish(collector);
    gc_postprocess_collector(collector);
  }
  gc->num_active_collectors = 0;

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
    
		collector_notify_work_done(collector);
	}

	return 0;
}

static void collector_init_thread(Collector *collector) 
{
	collector->trace_stack = new TraceStack(); /* only for MINOR_COLLECTION */
	collector->mark_stack = new MarkStack(); /* only for MAJOR_COLLECTION */

	collector->last_cycle_remset = new RemslotSet();
  collector->last_cycle_remset->reserve(GC_NUM_ROOTS_HINT);
	collector->last_cycle_remset->clear();

	collector->this_cycle_remset = new RemslotSet();
  collector->this_cycle_remset->reserve(GC_NUM_ROOTS_HINT);
	collector->this_cycle_remset->clear();

	int status = vm_create_event(&collector->task_assigned_event,0,1);
	assert(status == THREAD_OK);

	status = vm_create_event(&collector->task_finished_event,0,1);
	assert(status == THREAD_OK);

	status = (unsigned int)vm_create_thread(NULL,
                            			0, 0, 0,
                            			collector_thread_func,
                            			(void*)collector);

	assert(status == THREAD_OK);
	
	return;
}

static void collector_terminate_thread(Collector* collector)
{
  collector->task_func = NULL; /* NULL to notify thread exit */
	notify_collector_to_work(collector);
  vm_thread_yield(); /* give collector time to die */
  
  delete collector->trace_stack;
	delete collector->last_cycle_remset;
	delete collector->this_cycle_remset;
	
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

struct GC_Gen;
unsigned int gc_get_processor_num(GC_Gen*);
void collector_initialize(GC* gc)
{
 	unsigned int nthreads = gc_get_processor_num((GC_Gen*)gc);
	
	gc->num_collectors = 1; //FIXME:: nthreads;
	gc->collectors = (Collector **) STD_MALLOC(sizeof(Collector *) * nthreads);	
	assert(gc->collectors);

	for (unsigned int i = 0; i < nthreads; i++) {
		Collector* collector = (Collector *)STD_MALLOC(sizeof(Collector));
		assert(collector);
		
		collector->gc = gc;
		collector_init_thread(collector);
		
		gc->collectors[i] = collector;
	}

  return;
}

void collector_execute_task(GC* gc, TaskType task_func, Space* space)
{
  assign_collector_with_task(gc, task_func, space);
  wait_collection_finish(gc);
  
  return;
}
