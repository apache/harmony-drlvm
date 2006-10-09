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

#include "../gen/gen.h"

#include "../thread/mutator.h"

/* All the write barrier interfaces need cleanup */

Boolean gc_requires_barriers() 
{   return 1; }

/* The implementations are only temporary */
static void gc_write_barrier_generic(Managed_Object_Handle p_obj_holding_ref, Managed_Object_Handle *p_slot, 
                      Managed_Object_Handle p_target, unsigned int kind) 
{
  Mutator *mutator = (Mutator *)vm_get_gc_thread_local();
  GC_Gen* gc = (GC_Gen*)mutator->gc;
  if(kind == WRITE_BARRIER_SLOT ){
    if( address_belongs_to_nursery((void *)p_target, gc) && 
         !address_belongs_to_nursery((void *)p_slot, gc)) 
    {
      mutator->remslot->push_back((Partial_Reveal_Object **)p_slot);
    }
  }else if( kind == WRITE_BARRIER_OBJECT ){
    if( !address_belongs_to_nursery((void *)p_obj_holding_ref, gc) ) {
      mutator->remobj->push_back((Partial_Reveal_Object *)p_obj_holding_ref);
    }    
  }else{  
    assert(kind == WRITE_BARRIER_UPDATE ); 
    *p_slot = p_target;
    if( address_belongs_to_nursery((void *)p_target, gc) && 
         !address_belongs_to_nursery((void *)p_slot, gc)) 
    {
      mutator->remslot->push_back((Partial_Reveal_Object **)p_slot);
    }      
  }
}

/* temporary write barriers, need reconsidering */
void gc_write_barrier(Managed_Object_Handle p_obj_holding_ref)
{
  gc_write_barrier_generic(p_obj_holding_ref, NULL, NULL, WRITE_BARRIER_OBJECT); 
}

/* for array copy and object clone */
void gc_heap_wrote_object (Managed_Object_Handle p_obj_holding_ref)
{
  gc_write_barrier_generic(p_obj_holding_ref, NULL, NULL, WRITE_BARRIER_OBJECT); 
}

/* The following routines were supposed to be the only way to alter any value in gc heap. */
void gc_heap_write_ref (Managed_Object_Handle p_obj_holding_ref, unsigned offset, Managed_Object_Handle p_target) 
{  assert(0); }

void gc_heap_slot_write_ref (Managed_Object_Handle p_obj_holding_ref,Managed_Object_Handle *p_slot, Managed_Object_Handle p_target)
{  
  gc_write_barrier_generic(NULL, p_slot, p_target, WRITE_BARRIER_UPDATE); 
}

/* this is used for global object update, e.g., strings. Since globals are roots, no barrier here */
void gc_heap_write_global_slot(Managed_Object_Handle *p_slot,Managed_Object_Handle p_target)
{
  *p_slot = p_target;
}