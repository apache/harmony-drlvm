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

#include "gc_thread.h"

#include "../gen/gen.h"

#include "../finalizer_weakref/finalizer_weakref.h"

Managed_Object_Handle gc_alloc(unsigned size, Allocation_Handle ah, void *unused_gc_tls) 
{
  Managed_Object_Handle p_obj = NULL;
 
  /* All requests for space should be multiples of 4 (IA32) or 8(IPF) */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert(ah);

  Allocator* allocator = (Allocator*)gc_get_tls();
   
  if ( size > GC_OBJ_SIZE_THRESHOLD )
    p_obj = (Managed_Object_Handle)los_alloc(size, allocator);
  else{
    p_obj = (Managed_Object_Handle)nos_alloc(size, allocator);
  }
  
  if( p_obj == NULL ) return NULL;
    
  obj_set_vt((Partial_Reveal_Object*)p_obj, ah);
  
  if(!IGNORE_FINREF && type_has_finalizer((Partial_Reveal_VTable *)ah))
    mutator_add_finalizer((Mutator*)allocator, (Partial_Reveal_Object*)p_obj);
    
  return (Managed_Object_Handle)p_obj;
}


Managed_Object_Handle gc_alloc_fast (unsigned size, Allocation_Handle ah, void *unused_gc_tls) 
{
  /* All requests for space should be multiples of 4 (IA32) or 8(IPF) */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert(ah);
  
  if(type_has_finalizer((Partial_Reveal_VTable *)ah))
    return NULL;
  
  /* object should be handled specially */
  if ( size > GC_OBJ_SIZE_THRESHOLD ) return NULL;
 
  Allocator* allocator = (Allocator*)gc_get_tls();
 
  /* Try to allocate an object from the current Thread Local Block */
  Managed_Object_Handle p_obj;
  p_obj = (Managed_Object_Handle)thread_local_alloc(size, allocator);
  if(p_obj == NULL) return NULL;
   
  obj_set_vt((Partial_Reveal_Object*)p_obj, ah);
  
  return p_obj;
}
