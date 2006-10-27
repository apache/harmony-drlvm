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

#include "thread_alloc.h"

void* mos_alloc(unsigned size, Allocator *allocator);

/* NOS forward obj to MOS in MINOR_COLLECTION */
Partial_Reveal_Object* collector_forward_object(Collector* collector, Partial_Reveal_Object* p_obj)
{
  Partial_Reveal_VTable* vt = obj_get_vtraw(p_obj);

  /* forwarded by somebody else */
  if ((unsigned int)vt & FORWARDING_BIT_MASK){
     assert(!obj_is_marked_in_vt(p_obj));
     return NULL;
  }
    
  /* else, take the obj by setting the forwarding flag atomically 
     we don't put a simple bit in vt because we need compute obj size later. */
  if ((unsigned int)vt != atomic_cas32((unsigned int*)obj_get_vtraw_addr(p_obj), ((unsigned int)vt|FORWARDING_BIT_MASK), (unsigned int)vt)) {
    /* forwarded by other */
    assert( obj_is_forwarded_in_vt(p_obj) && !obj_is_marked_in_vt(p_obj));
    return NULL;
  }

  /* we hold the object, now forward it */
  unsigned int size = vm_object_size(p_obj);
  Partial_Reveal_Object* p_targ_obj = (Partial_Reveal_Object*)mos_alloc(size, (Allocator*)collector);  
  /* mos should always has enough space to hold nos during collection */
  assert(p_targ_obj); 
  memcpy(p_targ_obj, p_obj, size);

  /* because p_obj has forwarding flag in its vt, we set it here */
  obj_set_forwarding_pointer_in_vt(p_obj, p_targ_obj);
  obj_set_vt(p_targ_obj, (Allocation_Handle)vt);
  
  return p_targ_obj;  
 
}
