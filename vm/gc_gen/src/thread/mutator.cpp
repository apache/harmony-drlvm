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

#include "mutator.h"
#include "../trace_forward/fspace.h"
#include "../mark_sweep/sspace.h"
#include "../finalizer_weakref/finalizer_weakref.h"

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
void mutator_initialize(GC* gc, void *unused_gc_information) 
{
  /* FIXME:: make sure gc_info is cleared */
  Mutator *mutator = (Mutator *)STD_MALLOC(sizeof(Mutator));
  memset(mutator, 0, sizeof(Mutator));
  mutator->alloc_space = gc_get_nos((GC_Gen*)gc);
  mutator->gc = gc;
    
  if(gc_is_gen_mode()){
    mutator->rem_set = free_set_pool_get_entry(gc->metadata);
    assert(vector_block_is_empty(mutator->rem_set));
  }
  
  if(!IGNORE_FINREF )
    mutator->obj_with_fin = finref_get_free_block(gc);
  else
    mutator->obj_with_fin = NULL;

#ifdef ONLY_SSPACE_IN_HEAP
  allocator_init_local_chunks((Allocator*)mutator);
#endif
  
  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  mutator->next = (Mutator *)gc->mutator_list;
  gc->mutator_list = mutator;
  gc->num_mutators++;

  unlock(gc->mutator_list_lock); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  
  gc_set_tls(mutator);
  
  return;
}

void mutator_destruct(GC* gc, void *unused_gc_information)
{

  Mutator *mutator = (Mutator *)gc_get_tls();

  alloc_context_reset((Allocator*)mutator);

#ifdef ONLY_SSPACE_IN_HEAP
  allocactor_destruct_local_chunks((Allocator*)mutator);
#endif

  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  volatile Mutator *temp = gc->mutator_list;
  if (temp == mutator) {  /* it is at the head of the list */
    gc->mutator_list = temp->next;
  } else {
    while (temp->next != mutator) {
      temp = temp->next;
      assert(temp);
    }
    temp->next = mutator->next;
  }
  gc->num_mutators--;

  unlock(gc->mutator_list_lock); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  
  if(gc_is_gen_mode()){ /* put back the remset when a mutator exits */
    pool_put_entry(gc->metadata->mutator_remset_pool, mutator->rem_set);
    mutator->rem_set = NULL;
  }
  
  if(mutator->obj_with_fin){
    pool_put_entry(gc->finref_metadata->obj_with_fin_pool, mutator->obj_with_fin);
    mutator->obj_with_fin = NULL;
  }
  
  //gc_set_tls(NULL);
  
  return;
}

void gc_reset_mutator_context(GC* gc)
{
  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    alloc_context_reset((Allocator*)mutator);    
    mutator = mutator->next;
  }  
  return;
}

void gc_prepare_mutator_remset(GC* gc)
{
  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    mutator->rem_set = free_set_pool_get_entry(gc->metadata);
    mutator = mutator->next;
  }  
  return;
}



