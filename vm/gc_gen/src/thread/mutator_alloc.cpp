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

#include "../gen/gen.h"

Managed_Object_Handle gc_alloc(unsigned size, Allocation_Handle ah, void *gc_tls) 
{
  Managed_Object_Handle p_obj = NULL;
 
  /* All requests for space should be multiples of 4 (IA32) or 8(IPF) */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert(gc_tls == vm_get_gc_thread_local());
  assert(ah);

  if ( size > GC_OBJ_SIZE_THRESHOLD )
    p_obj = (Managed_Object_Handle)los_alloc(size, (Alloc_Context*)gc_tls);
  else
    p_obj = (Managed_Object_Handle)nos_alloc(size, (Alloc_Context*)gc_tls);
  
  assert(p_obj);
  obj_set_vt((Partial_Reveal_Object*)p_obj, ah);
    
  return (Managed_Object_Handle)p_obj;
}


Managed_Object_Handle gc_alloc_fast (unsigned size, Allocation_Handle ah, void *gc_tls) 
{
  /* All requests for space should be multiples of 4 (IA32) or 8(IPF) */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert(gc_tls == vm_get_gc_thread_local());
  assert(ah);
  
  /* object shoud be handled specially */
  if ( size > GC_OBJ_SIZE_THRESHOLD ) return NULL;
  
  /* Try to allocate an object from the current Thread Local Block */
  Managed_Object_Handle p_obj = NULL;
  p_obj = (Managed_Object_Handle)thread_local_alloc(size, (Alloc_Context*)gc_tls);
  if(p_obj == NULL) return NULL;
   
  obj_set_vt((Partial_Reveal_Object*)p_obj, ah);
  
  return p_obj;
}
