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

#ifndef _MARKER_H_
#define _MARKER_H_

#include "../common/gc_space.h"
#include "../mark_sweep/wspace_chunk.h"

typedef struct Marker{
  /* <-- first couple of fields are overloaded as Allocator */
  void *free;
  void *ceiling;
  void *end;
  void *alloc_block;
  Chunk_Header ***local_chunks;
  Space* alloc_space;
  GC* gc;
  VmThreadHandle thread_handle;   /* This thread; */
  unsigned int handshake_signal; /*Handshake is used in concurrent GC.*/  
  unsigned int num_alloc_blocks; /* the number of allocated blocks in this collection. */
  /* End of Allocator --> */

  /* FIXME:: for testing */
  Space* mark_space;
  
  /* backup allocator in case there are two target copy spaces, such as semispace GC */
  Allocator* backup_allocator;

  Vector_Block *trace_stack;
  
  Vector_Block* rep_set; /* repointed set */
  Vector_Block* rem_set;
#ifdef USE_32BITS_HASHCODE
  Vector_Block* hashcode_set;
#endif
  
  Vector_Block *softref_set;
  Vector_Block *weakref_set;
  Vector_Block *phanref_set;
  
  VmEventHandle task_assigned_event;
  VmEventHandle task_finished_event;
  
  Block_Header* cur_compact_block;
  Block_Header* cur_target_block;
  
  Free_Chunk_List *free_chunk_list;

  POINTER_SIZE_INT live_obj_size;
  POINTER_SIZE_INT live_obj_num;
  
  void(*task_func)(void*) ;   /* current task */
  
  POINTER_SIZE_INT non_los_live_obj_size;
  POINTER_SIZE_INT los_live_obj_size;
  POINTER_SIZE_INT segment_live_size[NORMAL_SIZE_SEGMENT_NUM];
  unsigned int result;

  Boolean marker_is_active;

  VmEventHandle markroot_finished_event;

  int64 time_mark;
  Marker* next;
  unsigned int num_dirty_slots_traced;
} Marker;

typedef Marker* Marker_List;

#define MAX_NUM_MARKERS 0xff
#define MIN_NUM_MARKERS 0x01

void marker_initialize(GC* gc);
void marker_destruct(GC* gc);

void marker_execute_task(GC* gc, TaskType task_func, Space* space);
void marker_execute_task_concurrent(GC* gc, TaskType task_func, Space* space, unsigned int num_markers);
void marker_execute_task_concurrent(GC* gc, TaskType task_func, Space* space);

void marker_notify_mark_root_done(Marker* marker);
void wait_mark_finish(GC* gc);
Boolean is_mark_finished(GC* gc);



#endif //_MARKER_H_

