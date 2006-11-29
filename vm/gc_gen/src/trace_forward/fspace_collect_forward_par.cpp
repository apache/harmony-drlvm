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

#include "fspace.h"
#include "../thread/collector.h"
#include "../common/gc_metadata.h"

static Boolean fspace_object_to_be_forwarded(Partial_Reveal_Object *p_obj, Fspace *fspace)
{
  assert(obj_belongs_to_space(p_obj, (Space*)fspace));  
  return forward_first_half? (p_obj < object_forwarding_boundary):(p_obj>=object_forwarding_boundary);
}

static void scan_slot(Collector* collector, Partial_Reveal_Object **p_ref) 
{
  Partial_Reveal_Object *p_obj = *p_ref;
  if (p_obj == NULL) return;  
    
  /* the slot can be in tspace or fspace, we don't care.
     we care only if the reference in the slot is pointing to fspace */
  if (obj_belongs_to_space(p_obj, collector->collect_space))
    collector_tracetask_add_entry(collector, p_ref); 

  return;
}

static void scan_object(Collector* collector, Partial_Reveal_Object *p_obj) 
{
  if (!object_has_slots(p_obj)) return;
  
  void *slot;
  
  /* scan array object */
  if (object_is_array(p_obj)) {
    Partial_Reveal_Object* array = p_obj;
    assert(!obj_is_primitive_array(array));
    
    int32 array_length = vector_get_length((Vector_Handle) array);
    for (int i = 0; i < array_length; i++) {
      slot = vector_get_element_address_ref((Vector_Handle) array, i);
      scan_slot(collector, (Partial_Reveal_Object **)slot);
    }   
    return;
  }

  /* scan non-array object */
  int *offset_scanner = init_object_scanner(p_obj);
  while (true) {
    slot = offset_get_ref(offset_scanner, p_obj);
    if (slot == NULL) break;
  
    scan_slot(collector, (Partial_Reveal_Object **)slot);
    offset_scanner = offset_next_ref(offset_scanner);
  }

  return;
}

/*  At this point, p_ref can be in anywhere like root, and other spaces,  
 *  but *p_ref must be in fspace, since only slot which points to 
 *  object in fspace could be added into TraceStack */
#include "../verify/verify_live_heap.h"

static void trace_object(Collector* collector, Partial_Reveal_Object **p_ref) 
{
  Space* space = collector->collect_space; 
  Partial_Reveal_Object *p_obj = *p_ref;

  assert(p_obj); 
  /* this assert is no longer valid for parallel forwarding, because remset may have duplicate p_refs that
     are traced by difference collectors, and right after both check the p_obj is in fspace, and put into
     trace_stack, one thread forwards it quickly before the other runs to this assert.
   assert(obj_belongs_to_space(p_obj, space)); */

  /* Fastpath: object has already been forwarded, update the ref slot */
  if(obj_is_forwarded_in_vt(p_obj)) {
    assert(!obj_is_marked_in_vt(p_obj));
    *p_ref = obj_get_forwarding_pointer_in_vt(p_obj);    
    return;
  }

  /* only mark the objects that will remain in fspace */
  if (!fspace_object_to_be_forwarded(p_obj, (Fspace*)space)) {
    assert(!obj_is_forwarded_in_vt(p_obj));
    /* this obj remains in fspace, remember its ref slot for next GC. */
    if( !address_belongs_to_space(p_ref, space) )
      collector_remset_add_entry(collector, p_ref); 
    
    if(fspace_mark_object((Fspace*)space, p_obj)) 
      scan_object(collector, p_obj);
    
    return;
  }
    
  /* following is the logic for forwarding */  
  Partial_Reveal_Object* p_target_obj = collector_forward_object(collector, p_obj);
  
  /* if it is forwarded by other already, it is ok */
  if( p_target_obj == NULL ){
    *p_ref = obj_get_forwarding_pointer_in_vt(p_obj);  
     return;
  }  
  /* otherwise, we successfully forwarded */
  *p_ref = p_target_obj;  

  /* we forwarded it, we need remember it for verification. FIXME:: thread id */
  if(verify_live_heap) {
    event_collector_move_obj(p_obj, p_target_obj, collector);
  }

  scan_object(collector, p_target_obj); 
  return;
}

void trace_object_seq(Collector* collector, Partial_Reveal_Object **p_ref);
 
/* for tracing phase termination detection */
static volatile unsigned int num_finished_collectors = 0;

static void collector_trace_rootsets(Collector* collector)
{
  GC* gc = collector->gc;
  GC_Metadata* metadata = gc->metadata;
  
  Space* space = collector->collect_space;
  collector->trace_stack = (TraceStack*)pool_get_entry(metadata->free_set_pool);
  //collector->trace_stack = new TraceStack();

  unsigned int num_active_collectors = gc->num_active_collectors;
  atomic_cas32( &num_finished_collectors, 0, num_active_collectors);

retry:
  /* find root slots saved by 1. active mutators, 2. exited mutators, 3. last cycle collectors */
  Vector_Block* root_set = pool_get_entry(metadata->gc_rootset_pool);
  
  while(root_set){    
    unsigned int* iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object** )*iter;
      iter = vector_block_iterator_advance(root_set,iter);

      assert(p_ref);
      if(*p_ref == NULL) continue;  
      /* in sequential version, we only trace same object once, but we were using a local hashset,
         which couldn't catch the repetition between multiple collectors. This is subject to more study. */
      if (obj_belongs_to_space(*p_ref, space)) 
          trace_object(collector, p_ref);
    }
    vector_block_clear(root_set);
    pool_put_entry(metadata->free_set_pool, root_set);
    root_set = pool_get_entry(metadata->gc_rootset_pool);
    
  }
  
  atomic_inc32(&num_finished_collectors);
  while(num_finished_collectors != num_active_collectors){
    if( !pool_is_empty(metadata->gc_rootset_pool)){
      atomic_dec32(&num_finished_collectors);
      goto retry;  
    }
  }


  /* now we are done, but each collector has a private task block to deal with */  
  Vector_Block* trace_task = (Vector_Block*)collector->trace_stack;
  TraceStack* trace_stack = new TraceStack();
 
  unsigned int* iter = vector_block_iterator_init(trace_task);
  while(!vector_block_iterator_end(trace_task,iter)){
    Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)*iter;
    iter = vector_block_iterator_advance(trace_task,iter);
    trace_stack->push(p_ref);
  }

  /* put back the last task to the free pool */
  vector_block_clear(trace_task);
  pool_put_entry(metadata->free_set_pool, trace_task);
   
  collector->trace_stack = trace_stack;
  while(!trace_stack->empty()){
    Partial_Reveal_Object** p_ref = trace_stack->top();
    trace_stack->pop();
    trace_object_seq(collector, p_ref);
  } 
  
  delete trace_stack;
  collector->trace_stack = NULL;
  
  return;
}

void update_rootset_interior_pointer();

static void update_relocated_refs(Collector* collector)
{
  update_rootset_interior_pointer();
}

static volatile unsigned int num_marking_collectors = 0;

void trace_forward_fspace(Collector* collector) 
{  
  GC* gc = collector->gc;
  Fspace* space = (Fspace*)collector->collect_space;
 
  unsigned int num_active_collectors = gc->num_active_collectors;  
  unsigned int old_num = atomic_cas32( &num_marking_collectors, 0, num_active_collectors+1);

  collector_trace_rootsets(collector);

  old_num = atomic_inc32(&num_marking_collectors);
  if( ++old_num == num_active_collectors ){
    /* last collector's world here */
    /* prepare for next phase */ /* let other collectors go */
    num_marking_collectors++; 
  }
  while(num_marking_collectors != num_active_collectors + 1);

  /* the rest work is not enough for parallelization, so let only one thread go */
  if( collector->thread_handle != 0 ) return;

  update_relocated_refs(collector);
  reset_fspace_for_allocation(space);  

  return;
  
}



