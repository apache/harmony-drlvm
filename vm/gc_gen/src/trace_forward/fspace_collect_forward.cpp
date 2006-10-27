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

static Boolean fspace_object_to_be_forwarded(Partial_Reveal_Object *p_obj, Fspace *fspace)
{
  assert(obj_belongs_to_space(p_obj, (Space*)fspace));  
  return forward_first_half? (p_obj < object_forwarding_boundary):(p_obj>=object_forwarding_boundary);
}

static void scan_slot(Collector* collector, Partial_Reveal_Object **p_ref) 
{
  Partial_Reveal_Object *p_obj = *p_ref;
  TraceStack *ts = collector->trace_stack;

  if (p_obj == NULL) return;  
    
  /* the slot can be in tspace or fspace, we don't care.
     we care only if the reference in the slot is pointing to fspace */
  if (obj_belongs_to_space(p_obj, collector->collect_space)) {
    ts->push(p_ref);
  } 

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
  assert(obj_belongs_to_space(p_obj, space));

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
      collector->this_cycle_remset->push_back(p_ref); 

    if(fspace_mark_object((Fspace*)space, p_obj)) 
      scan_object(collector, p_obj);
    
    return;
  }
    
  /* following is the logic for forwarding */  
  Partial_Reveal_Object* p_target_obj = collector_forward_object(collector, p_obj);
  
  /* if it is forwarded by other already, it is ok */
  if(!p_target_obj){
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

/* trace the root references from root set and remember sets */
void trace_root(Collector* collector, Partial_Reveal_Object **ref) 
{   
  assert(*ref); 
  assert(obj_belongs_to_space(*ref, collector->collect_space));

  TraceStack *ts = collector->trace_stack;   
  ts->push(ref);

  while(!ts->empty()) {
    Partial_Reveal_Object **p_ref = ts->top();
    ts->pop();
    assert(p_ref);
    trace_object(collector, p_ref);   
  }
}

static void collector_trace_remsets(Collector* collector)
{
  Fspace* fspace = (Fspace*)collector->collect_space;
  
  HashSet remslot_hash;

  /* find root slots saved by 1. active mutators, 2. exited mutators, 3. last cycle collectors */
  for(unsigned int i=0; i< fspace->remslot_sets->size(); i++) {
    RemslotSet* remslot = (*fspace->remslot_sets)[i];
    for (unsigned int j = 0; j < remslot->size(); j++) {
      Partial_Reveal_Object **ref = (*remslot)[j];
      assert(ref);
      if(*ref == NULL) continue;  
      if (obj_belongs_to_space(*ref, (Space*)fspace)) {
        if (remslot_hash.find(ref) == remslot_hash.end()) {
          remslot_hash.insert(ref);
          trace_root(collector, ref);
        }
      }
    }
    remslot->clear();  
  }
  fspace->remslot_sets->clear();
    
  return;
}

void update_rootset_interior_pointer();

static void update_relocated_refs(Collector* collector)
{
  update_rootset_interior_pointer();
}

void trace_forward_fspace(Collector* collector) 
{  
  GC* gc = collector->gc;
  Fspace* space = (Fspace*)collector->collect_space;
  
  /* FIXME:: Single-threaded trace-forwarding for fspace currently */

  space->remslot_sets->push_back(gc->root_set);
  collector_trace_remsets(collector);

  update_relocated_refs(collector);
  reset_fspace_for_allocation(space);  

  return;
  
}



