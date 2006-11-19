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

#include "mutator.h"
#include "../trace_forward/fspace.h"

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
void mutator_initialize(GC* gc, void *gc_information) 
{
  /* FIXME:: make sure gc_info is cleared */
  Mutator *mutator = (Mutator *) gc_information;
  mutator->free = NULL;
  mutator->ceiling = NULL;
  mutator->alloc_block = NULL;
  mutator->alloc_space = gc_get_nos((GC_Gen*)gc);
  mutator->gc = gc;
    
  if(gc_requires_barriers()){
    mutator->rem_set = pool_get_entry(gc->metadata->free_set_pool);
  }
       
  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  mutator->next = (Mutator *)gc->mutator_list;
  gc->mutator_list = mutator;

  unlock(gc->mutator_list_lock); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  
  gc->num_mutators++;
  return;
}

void mutator_destruct(GC* gc, void *gc_information)
{

  Mutator *mutator = (Mutator *)gc_information;

  if(gc_requires_barriers()){ /* put back the remset when a mutator exits */
    pool_put_entry(gc->metadata->gc_rootset_pool, mutator->rem_set);
    mutator->rem_set = NULL;
  }

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

  unlock(gc->mutator_list_lock); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  gc->num_mutators--;
  return;
}

