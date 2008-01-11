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

/**
 * @author Xiao-Feng Li, 2006/10/05
 */

#include "../gen/gen.h"
#include "../thread/mutator.h"
#include "gc_for_barrier.h"
#include "../mark_sweep/wspace_mark_sweep.h"
#include "../common/gc_concurrent.h"
#include "../finalizer_weakref/finalizer_weakref.h"


/* All the write barrier interfaces need cleanup */

volatile unsigned int write_barrier_function;

/* The implementations are only temporary */
static void write_barrier_rem_source_slot(Managed_Object_Handle *p_slot, 
                      Managed_Object_Handle p_target) 
{
  if(p_target >= nos_boundary && p_slot < nos_boundary){

    Mutator *mutator = (Mutator *)gc_get_tls();
    assert( addr_belongs_to_nos(p_target) && !addr_belongs_to_nos(p_slot)); 
            
    mutator_remset_add_entry(mutator, (REF*)p_slot);
  }
  return;
}

static void write_barrier_rem_source_obj(Managed_Object_Handle p_obj_holding_ref)
{
  if(obj_need_remember((Partial_Reveal_Object*)p_obj_holding_ref)){
    Mutator *mutator = (Mutator *)gc_get_tls();

    //FIXME: Release lock.
    lock(mutator->dirty_set_lock);
    obj_dirty_in_table((Partial_Reveal_Object *) p_obj_holding_ref);
    mutator_dirtyset_add_entry(mutator, (Partial_Reveal_Object*)p_obj_holding_ref);
    unlock(mutator->dirty_set_lock);
  }
}

static void gc_object_write_barrier(Managed_Object_Handle p_object) 
{
  
  if( addr_belongs_to_nos(p_object)) return;

  Mutator *mutator = (Mutator *)gc_get_tls();
  
  REF* p_slot; 
  /* scan array object */
  if (object_is_array((Partial_Reveal_Object*)p_object)) {
    Partial_Reveal_Object* array = (Partial_Reveal_Object*)p_object;
    assert(!obj_is_primitive_array(array));
    
    int32 array_length = vector_get_length((Vector_Handle) array);
    for (int i = 0; i < array_length; i++) {
      p_slot = (REF*)vector_get_element_address_ref((Vector_Handle) array, i);
      if( read_slot(p_slot) != NULL && addr_belongs_to_nos(read_slot(p_slot))){
        mutator_remset_add_entry(mutator, p_slot);
      }
    }   
    return;
  }

  /* scan non-array object */
  Partial_Reveal_Object* p_obj =  (Partial_Reveal_Object*)p_object;   
  unsigned int num_refs = object_ref_field_num(p_obj);
  int *ref_iterator = object_ref_iterator_init(p_obj);
            
  for(unsigned int i=0; i<num_refs; i++){
    p_slot = object_ref_iterator_get(ref_iterator+i, p_obj);        
    if( addr_belongs_to_nos(read_slot(p_slot))){
      mutator_remset_add_entry(mutator, p_slot);
    }
  }

  return;
}


/*This function is for concurrent mark.*/
static void write_barrier_rem_obj_snapshot(Managed_Object_Handle p_obj_holding_ref)
{
  Mutator *mutator = (Mutator *)gc_get_tls();
  REF* p_obj_slot; 
  if(obj_need_take_snapshot((Partial_Reveal_Object*)p_obj_holding_ref)){
    if (object_is_array((Partial_Reveal_Object*)p_obj_holding_ref)) {
      Partial_Reveal_Object* array = (Partial_Reveal_Object*)p_obj_holding_ref;
      assert(!obj_is_primitive_array(array));

      Partial_Reveal_Object* obj_to_snapshot; 
      
      int32 array_length = vector_get_length((Vector_Handle) array);
      for (int i = 0; i < array_length; i++) {
        p_obj_slot = (REF*)vector_get_element_address_ref((Vector_Handle) array, i);
        obj_to_snapshot = (Partial_Reveal_Object*)read_slot(p_obj_slot);
        if (obj_to_snapshot != NULL)  
          mutator_dirtyset_add_entry(mutator, obj_to_snapshot);
      }   
    }else{
      /* scan non-array object */
      Partial_Reveal_Object* p_obj =  (Partial_Reveal_Object*)p_obj_holding_ref;   
      unsigned int num_refs = object_ref_field_num(p_obj);
      int *ref_iterator = object_ref_iterator_init(p_obj);
      
      Partial_Reveal_Object* obj_to_snapshot; 

      for(unsigned int i=0; i<num_refs; i++){
        p_obj_slot = object_ref_iterator_get(ref_iterator+i, p_obj);        
        obj_to_snapshot = (Partial_Reveal_Object*)read_slot(p_obj_slot);
        if (obj_to_snapshot != NULL)  
          mutator_dirtyset_add_entry(mutator, obj_to_snapshot);
      }

      if(is_reference_obj(p_obj)){
        REF* p_referent_field = obj_get_referent_field(p_obj);
        obj_to_snapshot = (Partial_Reveal_Object*)read_slot(p_referent_field);
        if (obj_to_snapshot != NULL)  
          mutator_dirtyset_add_entry(mutator, obj_to_snapshot);
      }
    }
    obj_mark_black_in_table((Partial_Reveal_Object *) p_obj_holding_ref);
    obj_dirty_in_table((Partial_Reveal_Object *) p_obj_holding_ref);
  }
}

static void write_barrier_rem_slot_oldvar(Managed_Object_Handle* p_slot)
{
  Mutator *mutator = (Mutator *)gc_get_tls();  
  REF* p_obj_slot = (REF*) p_slot ;
  Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*)read_slot(p_obj_slot);
  if(p_obj && obj_need_remember_oldvar(p_obj)){
    mutator_dirtyset_add_entry(mutator, p_obj);
  }
}

//===========================================

/* The following routines were supposed to be the only way to alter any value in gc heap. */
void gc_heap_write_ref (Managed_Object_Handle p_obj_holding_ref, unsigned offset, Managed_Object_Handle p_target) 
{  assert(0); }

void gc_heap_wrote_object (Managed_Object_Handle p_obj_written)
{
  /*Concurrent Mark: Since object clone and array copy do not modify object slots, 
      we treat it as an new object. It has already been marked when dest object was created.
      We use WRITE_BARRIER_REM_SOURCE_OBJ function here to debug.
    */  
  Mutator *mutator = (Mutator *)gc_get_tls();  
  mutator_post_signal(mutator,MUTATOR_ENTER_BARRIER);

  if(WRITE_BARRIER_REM_SOURCE_OBJ == write_barrier_function){
    lock(mutator->dirty_set_lock);
    
    obj_dirty_in_table((Partial_Reveal_Object *) p_obj_written);
    mutator_dirtyset_add_entry(mutator, (Partial_Reveal_Object*)p_obj_written);
    
    unlock(mutator->dirty_set_lock);
  }
  mutator_post_signal(mutator,MUTATOR_EXIT_BARRIER);

  if( !gc_is_gen_mode() ) return;
  if( object_has_ref_field((Partial_Reveal_Object*)p_obj_written)){
    /* for array copy and object clone */
    gc_object_write_barrier(p_obj_written); 
  }
}

/* FIXME:: this is not the right interface for write barrier */
void gc_heap_slot_write_ref (Managed_Object_Handle p_obj_holding_ref,Managed_Object_Handle *p_slot, Managed_Object_Handle p_target)
{ 
  Mutator *mutator = (Mutator *)gc_get_tls();  
  mutator_post_signal(mutator,MUTATOR_ENTER_BARRIER);

  switch(write_barrier_function){
    case WRITE_BARRIER_REM_NIL:
      *p_slot = p_target;
      break;
    case WRITE_BARRIER_REM_SOURCE_REF:
      *p_slot = p_target;
      write_barrier_rem_source_slot(p_slot, p_target); 
      break;      
    case WRITE_BARRIER_REM_SOURCE_OBJ:
      *p_slot = p_target;
      write_barrier_rem_source_obj(p_obj_holding_ref);
      break;
    case WRITE_BARRIER_REM_OBJ_SNAPSHOT:
      write_barrier_rem_obj_snapshot(p_obj_holding_ref);
      *p_slot = p_target;
      break;
    case WRITE_BARRIER_REM_OLD_VAR:
      write_barrier_rem_slot_oldvar(p_slot);      
      *p_slot = p_target;
      break;
    default:
      assert(0);
      return;
  }

  mutator_post_signal(mutator,MUTATOR_EXIT_BARRIER);
  return;
}

/* this is used for global object update, e.g., strings. */
void gc_heap_write_global_slot(Managed_Object_Handle *p_slot,Managed_Object_Handle p_target)
{
  /*Concurrent Mark & Generational Mode: 
      Global objects are roots. After root set enumeration, this objects will be touched by GC. No barrier here.
    */

  *p_slot = p_target;
}
