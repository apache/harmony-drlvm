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

#ifndef _COLLECTOR_H_
#define _COLLECTOR_H_

#include "../common/gc_common.h"

typedef struct Collector{
  /* <-- first couple of fields are overloaded as Alloc_Context */
  void *free;
  void *ceiling;
  void *curr_alloc_block;
  Space* alloc_space;
  GC* gc;
  VmThreadHandle thread_handle;   /* This thread; */
  /* End of Alloc_Context --> */

  Space* collect_space;
  /* collector has remsets to remember those stored during copying */  
  RemslotSet* last_cycle_remset;   /* remembered in last cycle, used in this cycle as roots */
  RemslotSet* this_cycle_remset;   /* remembered in this cycle, will switch with last_remslot */

  TraceStack *trace_stack;
  MarkStack *mark_stack;
  
  VmEventHandle task_assigned_event;
  VmEventHandle task_finished_event;
  
  void(*task_func)(void*) ;   /* current task */
 
}Collector;

void collector_destruct(GC* gc);
void collector_initialize(GC* gc);
void collector_reset(GC* gc);

void collector_execute_task(GC* gc, TaskType task_func, Space* space);

Partial_Reveal_Object* collector_forward_object(Collector* collector, Partial_Reveal_Object* p_obj);

#endif //#ifndef _COLLECTOR_H_
