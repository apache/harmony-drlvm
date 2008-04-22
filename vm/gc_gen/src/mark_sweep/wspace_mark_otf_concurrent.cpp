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

Boolean obj_is_marked_in_table(Partial_Reveal_Object *obj);

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
    assert(obj_is_mark_black_in_table(p_obj));
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
  scan_weak_reference_direct((Collector*)marker, p_obj, scan_slot);
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

static Boolean concurrent_mark_need_terminating(GC* gc)
{
  GC_Metadata *metadata = gc->metadata;
  return gc_local_dirtyset_is_empty(gc) && pool_is_empty(metadata->gc_dirty_set_pool);
}

/* for marking phase termination detection */
static volatile unsigned int num_active_markers = 0;

void wspace_mark_scan_concurrent(Marker* marker)
{
  marker->time_measurement_start = time_now();
  GC *gc = marker->gc;
  GC_Metadata *metadata = gc->metadata;
  
  /* reset the num_finished_collectors to be 0 by one collector. This is necessary for the barrier later. */
  atomic_inc32(&num_active_markers);
  
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
  
  /*second step: mark dirty object snapshot pool*/
  Vector_Block* dirty_set = pool_get_entry(metadata->gc_dirty_set_pool);

  while(dirty_set){
    POINTER_SIZE_INT* iter = vector_block_iterator_init(dirty_set);
    while(!vector_block_iterator_end(dirty_set,iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      iter = vector_block_iterator_advance(dirty_set,iter);

      assert(p_obj!=NULL); //FIXME: restrict?
      if(obj_mark_gray_in_table(p_obj))
        collector_tracestack_push((Collector*)marker, p_obj);
    } 
    vector_block_clear(dirty_set);
    pool_put_entry(metadata->free_set_pool, dirty_set);
    dirty_set = pool_get_entry(metadata->gc_dirty_set_pool);
  }

    /* put back the last trace_stack task */    
  pool_put_entry(metadata->mark_task_pool, marker->trace_stack);  

  /* third step: iterate over the mark tasks and scan objects */
  /* get a task buf for the mark stack */
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
  
  /* termination condition:  
           1.all thread finished current job.
           2.local snapshot vectors are empty.
           3.global snapshot pool is empty.
    */
  atomic_dec32(&num_active_markers);
  while(num_active_markers != 0 || !concurrent_mark_need_terminating(gc)){
    if(!pool_is_empty(metadata->mark_task_pool) || !pool_is_empty(metadata->gc_dirty_set_pool)){
      atomic_inc32(&num_active_markers);
      goto retry; 
    }else{
      /*grab a block from mutator and begin tracing*/
      POINTER_SIZE_INT thread_num = (POINTER_SIZE_INT)marker->thread_handle;
      Vector_Block* local_dirty_set = gc_get_local_dirty_set(gc, (unsigned int)(thread_num + 1));      
      /*1.  If local_dirty_set has been set full bit, the block is full and will no longer be put into global snapshot pool; 
                  so it should be checked again to see if there're remaining entries unscanned in it. In this case, the 
                  share bit in local_dirty_set should not be cleared, beacause of rescanning exclusively. 
             2.  If local_dirty_set has not been set full bit, the block is used by mutator and has the chance to be put into
                  global snapshot pool. In this case, we simply clear the share bit in local_dirty_set. 
           */
      if(local_dirty_set != NULL){
        atomic_inc32(&num_active_markers);
        do{        
          while(!vector_block_is_empty(local_dirty_set)){ //|| !vector_block_not_full_set_unshared(local_dirty_set)){
            Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*) vector_block_get_entry(local_dirty_set);
            if(!obj_belongs_to_gc_heap(p_obj)) {
              assert(0);
            }
            
            if(obj_mark_gray_in_table(p_obj)){ 
              collector_tracestack_push((Collector*)marker, p_obj);
            }
          }
        }while(!vector_block_not_full_set_unshared(local_dirty_set) && !vector_block_is_empty(local_dirty_set));
        goto retry;
      }
    }
  }
  
  /* put back the last mark stack to the free pool */
  mark_task = (Vector_Block*)marker->trace_stack;
  vector_stack_clear(mark_task);
  pool_put_entry(metadata->free_task_pool, mark_task);
  marker->trace_stack = NULL;
  assert(pool_is_empty(metadata->gc_dirty_set_pool));

  marker->time_measurement_end = time_now();
  marker->time_mark = marker->time_measurement_end - marker->time_measurement_start;
  
  return;
}

void trace_obj_in_ms_concurrent_mark(Collector *collector, void *p_obj)
{
  obj_mark_gray_in_table((Partial_Reveal_Object*)p_obj);
  trace_object((Marker*)collector, (Partial_Reveal_Object *)p_obj);
}





