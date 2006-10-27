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

#include "../thread/collector.h"
#include "../gen/gen.h"

static void scan_slot(Collector* collector, Partial_Reveal_Object** p_ref)
{
  Partial_Reveal_Object* p_obj = *p_ref;
  if(p_obj==NULL) return;

  MarkStack* mark_stack = collector->mark_stack;
  Space* obj_space = space_of_addr(collector->gc, p_obj);
  Space* ref_space = space_of_addr(collector->gc, p_ref);

  /* if obj to be moved, its ref slot needs remembering for later update */
  if(obj_space->move_object) 
    ref_space->save_reloc_func(ref_space, p_ref);

  if(obj_space->mark_object_func(obj_space, p_obj))   
    mark_stack->push(p_obj);
    
  return;
}

static void scan_object(Collector* collector, Partial_Reveal_Object *p_obj)
{
  if( !object_has_slots(p_obj) ) return;
  
    /* scan array object */
  if (object_is_array(p_obj)) {
    Partial_Reveal_Object* array = p_obj;
    assert(!obj_is_primitive_array(array));
    
    int32 array_length = vector_get_length((Vector_Handle) array);
    for (int i = 0; i < array_length; i++) {
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)vector_get_element_address_ref((Vector_Handle) array, i);
      scan_slot(collector, p_ref);
    }   
    return;
  }

  /* scan non-array object */
  int *offset_scanner = init_object_scanner(p_obj);
  while (true) {
    Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)offset_get_ref(offset_scanner, p_obj);
    if (p_ref == NULL) break; /* terminating ref slot */
  
    scan_slot(collector, p_ref);
    offset_scanner = offset_next_ref(offset_scanner);
  }

  return;
}

static void scan_root(Collector* collector, Partial_Reveal_Object *p_obj)
{
  assert(p_obj);
  Space* space = space_of_addr(collector->gc, p_obj);
  if( !space->mark_object_func(space, p_obj) ) return;  
      
  MarkStack* mark_stack = collector->mark_stack;
  mark_stack->push(p_obj);
  
  while(!mark_stack->empty()){
  	p_obj = mark_stack->top();
  	mark_stack->pop();
	  scan_object(collector, p_obj);
  }
  
  return;
}

/* NOTE:: Only marking in object header is idempotent */
void mark_scan_heap(Collector* collector)
{
  GC* gc = collector->gc;

  int size = gc->root_set->size();
  
  for(int i=0; i<size; i++){
    Partial_Reveal_Object **p_ref = (*gc->root_set)[i];
	  assert(*p_ref); /* root ref should never by NULL */
	  scan_root(collector, *p_ref);	
  }	
  
  return;
}
