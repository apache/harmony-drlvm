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

#include "wspace_mark_sweep.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../thread/marker.h"

volatile BOOLEAN need_terminate_mostly_con_mark;

BOOLEAN obj_is_marked_in_table(Partial_Reveal_Object *obj);

static FORCE_INLINE void scan_slot(Collector* marker, REF *p_ref)
{
  Partial_Reveal_Object *p_obj = read_slot(p_ref);
  if( p_obj == NULL) return;
  
  assert(address_belongs_to_gc_heap(p_obj, marker->gc));
  if(obj_mark_gray_in_table(p_obj)){
    assert(p_obj);
    collector_tracestack_push((Collector*)marker, p_obj);
  }  
}

static FORCE_INLINE void scan_object(Marker* marker, Partial_Reveal_Object *p_obj)
{
  assert((((POINTER_SIZE_INT)p_obj) % GC_OBJECT_ALIGNMENT) == 0);
  if(obj_is_dirty_in_table(p_obj)){ 
    return;
  }

  if(!object_has_ref_field(p_obj)) return;

  REF *p_ref;
  
  if(object_is_array(p_obj)){   /* scan array object */
    Partial_Reveal_Array *array = (Partial_Reveal_Array*)p_obj;
    unsigned int array_length = array->array_len;
    
    p_ref = (REF *)((POINTER_SIZE_INT)array + (int)array_first_element_offset(array));
    for (unsigned int i = 0; i < array_length; i++)
      scan_slot((Collector*)marker, p_ref+i);
    
    return;
  }
  
  /* scan non-array object */
  unsigned int num_refs = object_ref_field_num(p_obj);
  int *ref_iterator = object_ref_iterator_init(p_obj);
  
  for(unsigned int i=0; i<num_refs; i++){
    p_ref = object_ref_iterator_get(ref_iterator+i, p_obj);
    scan_slot((Collector*)marker, p_ref);
  }

#ifndef BUILD_IN_REFERENT
  scan_weak_reference((Collector*)marker, p_obj, scan_slot);
#endif

}

static void trace_object(Marker* marker, Partial_Reveal_Object *p_obj)
{
  scan_object(marker, p_obj);
  obj_mark_black_in_table(p_obj);
  
  Vector_Block *trace_stack = marker->trace_stack;
  while(!vector_stack_is_empty(trace_stack)){
    p_obj = (Partial_Reveal_Object*)vector_stack_pop(trace_stack);
    scan_object(marker, p_obj);
    obj_mark_black_in_table(p_obj);    
    trace_stack = marker->trace_stack;
  }
}

/* for marking phase termination detection */
void mostly_con_mark_terminate_reset()
{ need_terminate_mostly_con_mark = FALSE; }

void terminate_mostly_con_mark()
{ need_terminate_mostly_con_mark = TRUE; }

static BOOLEAN concurrent_mark_need_terminating(GC* gc)
{
  if(need_terminate_mostly_con_mark) return TRUE;
    
  GC_Metadata *metadata = gc->metadata;
  return pool_is_empty(metadata->gc_dirty_set_pool);
}

static volatile unsigned int num_active_markers = 0;

void wspace_mark_scan_mostly_concurrent(Marker* marker)
{
  int64 time_mark_start = time_now();
  GC *gc = marker->gc;
  GC_Metadata *metadata = gc->metadata;
  
  /* reset the num_finished_collectors to be 0 by one collector. This is necessary for the barrier later. */
  unsigned int current_thread_id = atomic_inc32(&num_active_markers);

  unsigned int num_dirtyset_slot = 0;  
  
  marker->trace_stack = free_task_pool_get_entry(metadata);
  
  Vector_Block *root_set = pool_iterator_next(metadata->gc_rootset_pool);
  
  /* first step: copy all root objects to mark tasks.*/
  while(root_set){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      iter = vector_block_iterator_advance(root_set,iter);
      
      assert(p_obj!=NULL);
      assert(address_belongs_to_gc_heap(p_obj, gc));
      if(obj_mark_gray_in_table(p_obj))
        collector_tracestack_push((Collector*)marker, p_obj);
    }
    root_set = pool_iterator_next(metadata->gc_rootset_pool);
  }
  /* put back the last trace_stack task */
  pool_put_entry(metadata->mark_task_pool, marker->trace_stack);
  
  marker->trace_stack = free_task_pool_get_entry(metadata);

retry:

  /*second step: mark dirty pool*/

  Vector_Block* dirty_set = pool_get_entry(metadata->gc_dirty_set_pool);

  while(dirty_set){
    POINTER_SIZE_INT* iter = vector_block_iterator_init(dirty_set);
    while(!vector_block_iterator_end(dirty_set,iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      iter = vector_block_iterator_advance(dirty_set,iter);

      assert(p_obj!=NULL); //FIXME: restrict condition?
      
      obj_clear_dirty_in_table(p_obj);
      obj_clear_mark_in_table(p_obj);

      if(obj_mark_gray_in_table(p_obj))
        collector_tracestack_push((Collector*)marker, p_obj);

      num_dirtyset_slot ++;
    } 
    vector_block_clear(dirty_set);
    pool_put_entry(metadata->free_set_pool, dirty_set);
    dirty_set = pool_get_entry(metadata->gc_dirty_set_pool);
  }

   /* put back the last trace_stack task */    
  pool_put_entry(metadata->mark_task_pool, marker->trace_stack);  

  /* third step: iterate over the mark tasks and scan objects */
   marker->trace_stack = free_task_pool_get_entry(metadata);

  
  Vector_Block *mark_task = pool_get_entry(metadata->mark_task_pool);
  
  while(mark_task){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(mark_task);
    while(!vector_block_iterator_end(mark_task,iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object*)*iter;
      iter = vector_block_iterator_advance(mark_task,iter);
      trace_object(marker, p_obj);      
    }
    /* run out one task, put back to the pool and grab another task */
    vector_stack_clear(mark_task);
    pool_put_entry(metadata->free_task_pool, mark_task);
    mark_task = pool_get_entry(metadata->mark_task_pool);
  }

  if(current_thread_id == 0){
    gc_prepare_dirty_set(marker->gc);
  }

  /* conditions to terminate mark: 
           1.All thread finished current job.
           2.Flag is set to terminate concurrent mark.
    */
  atomic_dec32(&num_active_markers);
  while(num_active_markers != 0 || !concurrent_mark_need_terminating(gc)){
    if(!pool_is_empty(metadata->mark_task_pool) || !pool_is_empty(metadata->gc_dirty_set_pool)){
      atomic_inc32(&num_active_markers);
      goto retry;
    }
  }
  
  /* put back the last mark stack to the free pool */
  mark_task = (Vector_Block*)marker->trace_stack;
  vector_stack_clear(mark_task);
  pool_put_entry(metadata->free_task_pool, mark_task);
  marker->trace_stack = NULL;
  
  int64 time_mark = time_now() - time_mark_start;
  marker->time_mark += time_mark;
  marker->num_dirty_slots_traced += num_dirtyset_slot;
  return;
}

void trace_obj_in_ms_mostly_concurrent_mark(Collector *collector, void *p_obj)
{
  obj_mark_gray_in_table((Partial_Reveal_Object*)p_obj);
  trace_object((Marker*)collector, (Partial_Reveal_Object *)p_obj);
}





