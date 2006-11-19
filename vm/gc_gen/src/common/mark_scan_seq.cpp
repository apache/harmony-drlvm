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

static void scan_slot_seq(Collector* collector, Partial_Reveal_Object** p_ref)
{
  Partial_Reveal_Object* p_obj = *p_ref;
  if(p_obj==NULL) return;

  MarkStack* mark_stack = (MarkStack*)collector->mark_stack;
  Space* obj_space = space_of_addr(collector->gc, p_obj);

  /* if obj to be moved, its ref slot needs remembering for later update */
  if(obj_space->move_object) 
    collector_repset_add_entry(collector, p_ref);

  if(obj_space->mark_object_func(obj_space, p_obj))   
    mark_stack->push(p_obj);
    
  return;
}

void scan_object_seq(Collector* collector, Partial_Reveal_Object *p_obj)
{
  if( !object_has_slots(p_obj) ) return;
  
    /* scan array object */
  if (object_is_array(p_obj)) {
    Partial_Reveal_Object* array = p_obj;
    assert(!obj_is_primitive_array(array));
    
    int32 array_length = vector_get_length((Vector_Handle) array);
    for (int i = 0; i < array_length; i++) {
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)vector_get_element_address_ref((Vector_Handle) array, i);
      scan_slot_seq(collector, p_ref);
    }   
    return;
  }

  /* scan non-array object */
  int *offset_scanner = init_object_scanner(p_obj);
  while (true) {
    Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)offset_get_ref(offset_scanner, p_obj);
    if (p_ref == NULL) break; /* terminating ref slot */
  
    scan_slot_seq(collector, p_ref);
    offset_scanner = offset_next_ref(offset_scanner);
  }

  return;
}

/* NOTE:: Only marking in object header is idempotent */
void mark_scan_heap_seq(Collector* collector)
{
  GC* gc = collector->gc;
  MarkStack* mark_stack = new MarkStack();
  collector->mark_stack = mark_stack;

  GC_Metadata* metadata = gc->metadata;
 
  pool_iterator_init(metadata->gc_rootset_pool);
  Vector_Block* root_set = pool_iterator_next(metadata->gc_rootset_pool);
  
  while(root_set){
    unsigned int* iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object** )*iter;
      iter = vector_block_iterator_advance(root_set,iter);

      Partial_Reveal_Object* p_obj = *p_ref;
      assert(!p_obj == NULL); /* root ref can't be NULL */
  
      Space* space = space_of_addr(collector->gc, p_obj);
      if( !space->mark_object_func(space, p_obj) ) continue;   
      mark_stack->push(p_obj);
    }
    root_set = pool_iterator_next(metadata->gc_rootset_pool);
  } 

  while(!mark_stack->empty()){
    Partial_Reveal_Object* p_obj = mark_stack->top();
    mark_stack->pop();
    scan_object_seq(collector, p_obj);
  }
  
  return;
}
