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

#ifndef _COLLECTION_SCHEDULER_H_
#define _COLLECTION_SCHEDULER_H_

#define STATISTICS_SAMPLING_WINDOW_SIZE 5

typedef struct Collection_Scheduler {
  /*common field*/
  GC* gc;
  
  /*mark schedule */
  int64 time_delay_to_start_mark;
  
  int64 last_mutator_time;
  int64 last_collector_time;

  unsigned int last_marker_num;

  unsigned int num_slot_in_window;
  unsigned int last_slot_index_in_window;
  
  float alloc_rate_window[STATISTICS_SAMPLING_WINDOW_SIZE];
  float trace_rate_window[STATISTICS_SAMPLING_WINDOW_SIZE];
  POINTER_SIZE_INT num_obj_traced_window[STATISTICS_SAMPLING_WINDOW_SIZE];
  POINTER_SIZE_INT size_alloced_window[STATISTICS_SAMPLING_WINDOW_SIZE];
} Collection_Scheduler;

void collection_scheduler_initialize(GC* gc);
void collection_scheduler_destruct(GC* gc);

void gc_update_collection_scheduler(GC* gc, int64 mutator_time, int64 mark_time);
Boolean gc_try_schedule_collection(GC* gc, unsigned int gc_cause);
Boolean gc_need_start_concurrent_mark(GC* gc);
unsigned int gc_decide_marker_number(GC* gc);


#endif


