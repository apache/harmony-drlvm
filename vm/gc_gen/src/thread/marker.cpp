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

#include "marker.h"
#include "../finalizer_weakref/finalizer_weakref.h"

#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"

unsigned int NUM_MARKERS = 0;
static volatile unsigned int live_marker_num = 0;

void notify_marker_to_work(Marker* marker)
{
  vm_set_event(marker->task_assigned_event);
}

void wait_marker_to_finish(Marker* marker)
{
  vm_wait_event(marker->task_finished_event);
}

void marker_wait_for_task(Marker* marker)
{
  vm_wait_event(marker->task_assigned_event);
}

void marker_notify_work_done(Marker* marker)
{
  vm_set_event(marker->task_finished_event);
}

void wait_marker_finish_mark_root(Marker* marker)
{
  vm_wait_event(marker->markroot_finished_event);
}

void marker_notify_mark_root_done(Marker* marker)
{
  vm_set_event(marker->markroot_finished_event);
}

static int marker_thread_func(void *arg) 
{
  Marker* marker = (Marker *)arg;
  assert(marker);

  while(true){
    /* Waiting for newly assigned task */
    marker_wait_for_task(marker); 
    marker->marker_is_active = TRUE;

   /* waken up and check for new task */
    TaskType task_func = marker->task_func;
    if(task_func == NULL){
      atomic_dec32(&live_marker_num);
      return 1;
    }

    task_func(marker);

    marker_notify_work_done(marker);

    marker->marker_is_active = FALSE;
  }

  return 0;
}

static void marker_init_thread(Marker* marker) 
{
  int status = vm_create_event(&marker->task_assigned_event);
  assert(status == THREAD_OK);

  status = vm_create_event(&marker->task_finished_event);
  assert(status == THREAD_OK);

  status = vm_create_event(&marker->markroot_finished_event);
  assert(status == THREAD_OK);

  status = (unsigned int)vm_create_thread(marker_thread_func, (void*)marker);

  assert(status == THREAD_OK);
  
  return;
}

void marker_initialize(GC* gc)
{  
  unsigned int num_processors = gc_get_processor_num(gc);

  unsigned int nthreads = max(NUM_MARKERS,num_processors); 

  unsigned int size = sizeof(Marker*) * nthreads;
  gc->markers = (Marker **) STD_MALLOC(size); 
  memset(gc->markers, 0, size);

  size = sizeof(Marker);
  for (unsigned int i = 0; i < nthreads; i++) {
   Marker* marker = (Marker *)STD_MALLOC(size);
   memset(marker, 0, size);
   
   /* FIXME:: thread_handle is for temporary control */
   marker->thread_handle = (VmThreadHandle)(POINTER_SIZE_INT)i;
   marker->gc = gc;
   marker->marker_is_active = FALSE;
   marker_init_thread(marker);
   
   gc->markers[i] = marker;
  }
  
  gc->num_markers = NUM_MARKERS? NUM_MARKERS:num_processors;
  live_marker_num = NUM_MARKERS;
  
   return;

}

void marker_terminate_thread(Marker* marker)
{
  assert(live_marker_num);
  unsigned int old_live_marker_num = live_marker_num;
  marker->task_func = NULL; /* NULL to notify thread exit */
  if(marker->marker_is_active) wait_marker_to_finish(marker);
  notify_marker_to_work(marker);
  while(old_live_marker_num == live_marker_num)
    vm_thread_yield(); /* give marker time to die */
  
  delete marker->trace_stack;  
  return;

}

void marker_destruct(GC* gc)
{
  for(unsigned int i=0; i<gc->num_markers; i++)
  {
    Marker* marker = gc->markers[i];
    marker_terminate_thread(marker);
    STD_FREE(marker);
   
  }
  assert(live_marker_num == 0);

  STD_FREE(gc->markers);
  return;
}

void wait_mark_finish(GC* gc)
{
  unsigned int num_active_marker = gc->num_active_markers;
  for(unsigned int i=0; i<num_active_marker; i++){
    Marker* marker = gc->markers[i];
    wait_marker_to_finish(marker);
  }  
  return;
}

Boolean is_mark_finished(GC* gc)
{
  unsigned int num_active_marker = gc->num_active_markers;
  unsigned int i = 0;
  for(; i<num_active_marker; i++){
    Marker* marker = gc->markers[i];
    if(marker->marker_is_active){
      return FALSE;
    }
  }
  return TRUE;
}


void marker_reset_thread(Marker* marker)
{
    marker->task_func = NULL;
    
#ifndef BUILD_IN_REFERENT
    collector_reset_weakref_sets((Collector*)marker);
#endif

    return;

}

void assign_marker_with_task(GC* gc, TaskType task_func, Space* space)
{
  gc->num_active_markers = gc->num_markers;
  for(unsigned int i=0; i<gc->num_markers; i++)
  {
    Marker* marker = gc->markers[i];
    
    marker_reset_thread(marker);
    marker->task_func = task_func;
    marker->mark_space= space;
    notify_marker_to_work(marker);
  }
  return;
}

void assign_marker_with_task(GC* gc, TaskType task_func, Space* space, unsigned int num_markers)
{
  unsigned int i = gc->num_active_markers;
  gc->num_active_markers += num_markers;
  for(; i < gc->num_active_markers; i++)
  {
    //printf("start mark thread %d \n", i);
    
    Marker* marker = gc->markers[i];
    
    marker_reset_thread(marker);
    marker->task_func = task_func;
    marker->mark_space= space;
    notify_marker_to_work(marker);
  }
  return;
}

void wait_mark_root_finish(GC* gc)
{
  unsigned int num_marker = gc->num_active_markers;
  for(unsigned int i=0; i<num_marker; i++)
  {
    Marker* marker = gc->markers[i];
    wait_marker_finish_mark_root(marker);
  }  
  return;
}

void wait_mark_root_finish(GC* gc, unsigned int num_markers)
{
  unsigned int num_active_marker = gc->num_active_markers;
  unsigned int i= num_active_marker - num_markers;
  for(; i < num_active_marker; i++)
  {
    Marker* marker = gc->markers[i];
    wait_marker_finish_mark_root(marker);
  }  
  return;
}

void marker_execute_task(GC* gc, TaskType task_func, Space* space)
{
  assign_marker_with_task(gc, task_func, space);  
  wait_mark_root_finish(gc);
  wait_mark_finish(gc);    
  return;
}

void marker_execute_task_concurrent(GC* gc, TaskType task_func, Space* space)
{
  assign_marker_with_task(gc, task_func, space);
  wait_mark_root_finish(gc);
  return;
}

void marker_execute_task_concurrent(GC* gc, TaskType task_func, Space* space, unsigned int num_markers)
{
  unsigned int num_free_markers = gc->num_markers - gc->num_active_markers;
  if(num_markers > num_free_markers) 
    num_markers = num_free_markers;
  assign_marker_with_task(gc, task_func, space,num_markers);
  wait_mark_root_finish(gc, num_markers);
  return;
}


