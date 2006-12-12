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
struct Block_Header;

typedef struct Collector{
  /* <-- first couple of fields are overloaded as Allocator */
  void *free;
  void *ceiling;
  void *alloc_block;
  Space* alloc_space;
  GC* gc;
  VmThreadHandle thread_handle;   /* This thread; */
  /* End of Allocator --> */

  /* FIXME:: for testing */
  Space* collect_space;

  Vector_Block *trace_stack;
  
  Vector_Block* rep_set; /* repointed set */
  Vector_Block* rem_set;
  
  Vector_Block *softref_set;
  Vector_Block *weakref_set;
  Vector_Block *phanref_set;
  
  VmEventHandle task_assigned_event;
  VmEventHandle task_finished_event;
  
  Block_Header* cur_compact_block;
  Block_Header* cur_target_block;
  
  /* during compaction, save non-zero obj_info who's overwritten by forwarding pointer */
  ObjectMap*  obj_info_map; 

  void(*task_func)(void*) ;   /* current task */
  
  unsigned int result;
 
}Collector;

void collector_destruct(GC* gc);
void collector_initialize(GC* gc);
void collector_reset(GC* gc);

void collector_execute_task(GC* gc, TaskType task_func, Space* space);

Partial_Reveal_Object* collector_forward_object(Collector* collector, Partial_Reveal_Object* p_obj);

void gc_restore_obj_info(GC* gc);


#endif //#ifndef _COLLECTOR_H_
