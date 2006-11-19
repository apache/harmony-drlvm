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

#include "gc_metadata.h"
#include "../thread/collector.h"
#include "../gen/gen.h"

static void scan_slot_par(Collector* collector, Partial_Reveal_Object** p_ref)
{
  Partial_Reveal_Object* p_obj = *p_ref;
  if(p_obj==NULL) return;

  Space* obj_space = space_of_addr(collector->gc, p_obj);

  /* if obj to be moved, its ref slot needs remembering for later update */
  if(obj_space->move_object) 
    collector_repset_add_entry(collector, p_ref);

  if(obj_space->mark_object_func(obj_space, p_obj))   
    collector_marktask_add_entry(collector, p_obj);
  
  return;
}

static void scan_object_par(Collector* collector, Partial_Reveal_Object *p_obj)
{
  if( !object_has_slots(p_obj) ) return;
  
    /* scan array object */
  if (object_is_array(p_obj)) {
    Partial_Reveal_Object* array = p_obj;
    assert(!obj_is_primitive_array(array));
    
    int32 array_length = vector_get_length((Vector_Handle) array);
    for (int i = 0; i < array_length; i++) {
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)vector_get_element_address_ref((Vector_Handle) array, i);
      scan_slot_par(collector, p_ref);
    }   
    return;
  }

  /* scan non-array object */
  int *offset_scanner = init_object_scanner(p_obj);
  while (true) {
    Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)offset_get_ref(offset_scanner, p_obj);
    if (p_ref == NULL) break; /* terminating ref slot */
  
    scan_slot_par(collector, p_ref);
    offset_scanner = offset_next_ref(offset_scanner);
  }

  return;
}

extern void scan_object_seq(Collector*, Partial_Reveal_Object *); 

/* for marking phase termination detection */
static volatile unsigned int num_finished_collectors = 0;

/* NOTE:: Only marking in object header is idempotent */
void mark_scan_heap_par(Collector* collector)
{
  GC* gc = collector->gc;
  /* reset the num_finished_collectors to be 0 by one collector. This is necessary for the barrier later. */
  unsigned int num_active_collectors = gc->num_active_collectors;
  atomic_cas32( &num_finished_collectors, 0, num_active_collectors);
  
  GC_Metadata* metadata = gc->metadata;
 
  collector->mark_stack = (MarkStack*)pool_get_entry(metadata->free_task_pool);

  Vector_Block* root_set = pool_iterator_next(metadata->gc_rootset_pool);

  /* first step: copy all root objects to mark tasks. 
      FIXME:: can be done sequentially before coming here to eliminate atomic ops */ 
  while(root_set){
    unsigned int* iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object** )*iter;
      iter = vector_block_iterator_advance(root_set,iter);

      Partial_Reveal_Object* p_obj = *p_ref;
      assert(!p_obj == NULL); /* root ref can't be NULL */
      /* we have to mark the object before put it into marktask, because
         it is possible to have two slots containing a same object. They will
         be scanned twice and their ref slots will be recorded twice. Problem
         occurs after the ref slot is updated first time with new position
         and the second time the value is the ref slot is the old position as expected.
         This can be worked around if we want. 
      */
      Space* space = space_of_addr(gc, p_obj);
      if( !space->mark_object_func(space, p_obj) ) continue;   
    
      collector_marktask_add_entry(collector, p_obj);
    } 
    root_set = pool_iterator_next(metadata->gc_rootset_pool);
  }
      
  pool_put_entry(metadata->mark_task_pool, collector->mark_stack);
  
  /* second step: iterate over the mark tasks and scan objects */
  /* get a task buf to push new tasks */
  collector->mark_stack = (MarkStack*)pool_get_entry(metadata->free_task_pool);
  
retry:
  Vector_Block* mark_task = pool_get_entry(metadata->mark_task_pool);
  while(mark_task){
    unsigned int* iter = vector_block_iterator_init(mark_task);
    while(!vector_block_iterator_end(mark_task,iter)){
      Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)*iter;
      iter = vector_block_iterator_advance(mark_task,iter);

      scan_object_par(collector, p_obj);
    } 
    /* run out one task, put back to the pool and grab another task */
   vector_block_clear(mark_task);
   pool_put_entry(metadata->free_task_pool, mark_task);
   mark_task = pool_get_entry(metadata->mark_task_pool);      
  }
  
  /* termination detection. This is also a barrier.
     NOTE:: actually we don't need this complexity. We can simply
     spin waiting for num_finished_collectors, because each generated new
     task would surely be processed by its generating collector eventually. 
     So code below is for load balance. */
  atomic_inc32(&num_finished_collectors);
  while(num_finished_collectors != num_active_collectors){
    if( !pool_is_empty(metadata->mark_task_pool)){
      atomic_dec32(&num_finished_collectors);
      goto retry;  
    }
  }
  
  /* up to now, we donot have any tasks in task_pool, but
     each collector has remaining tasks in its local mark_stack. */
     
  /* Lets process remaining tasks.  
     NOTE:: this is the key difference from work-stealing, which uses
     same stack for both working and sharing. So it has no problem
     with remaining tasks in the shared stack. */

  /* to simplify the processing, we turn back to use a single stack for
     the remaining objects scanning. The assumption is, there are only limited
     tasks for processing, no need to share the tasks. 
     FIXME:: a better way is to reduce the task block size into half till 
     the size becomes one, then the collectors actually share a same stack */      
     
  mark_task = (Vector_Block*)collector->mark_stack;
  MarkStack* mark_stack = new MarkStack();
 
  unsigned int* iter = vector_block_iterator_init(mark_task);
  while(!vector_block_iterator_end(mark_task,iter)){
    Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)*iter;
    iter = vector_block_iterator_advance(mark_task,iter);
    mark_stack->push(p_obj);
  }
  /* put back the last task to the free pool */
  vector_block_clear(mark_task);
  pool_put_entry(metadata->free_task_pool, mark_task);
   
  collector->mark_stack = mark_stack;
  while(!mark_stack->empty()){
    Partial_Reveal_Object* p_obj = mark_stack->top();
    mark_stack->pop();
    scan_object_seq(collector, p_obj);
  } 
  
  delete mark_stack;
  collector->mark_stack = NULL;
  
  /* put back last repointed refs set recorded during marking */
  pool_put_entry(metadata->collector_repset_pool, collector->rep_set);
  collector->rep_set = NULL;

  return;
}
