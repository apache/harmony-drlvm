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

#include <cxxlog.h>
#include "vm_threads.h"

#include "../gen/gen.h"
#include "interior_pointer.h"
#include "../thread/collector.h"
#include "../verify/verify_live_heap.h"

static GC* p_global_gc = NULL;

void gc_tls_init();

void gc_init() 
{  
  gc_parse_options();
    
  assert(p_global_gc == NULL);
  GC* gc = (GC*)STD_MALLOC(sizeof(GC_Gen));
  assert(gc);
  memset(gc, 0, sizeof(GC));  
  p_global_gc = gc;
  gc_tls_init();
  
  gc_gen_initialize((GC_Gen*)gc, min_heap_size_bytes, max_heap_size_bytes);

  gc_metadata_initialize(gc); /* root set and mark stack */
  collector_initialize(gc);
  gc_init_heap_verification(gc);

  return;
}

void gc_wrapup() 
{ 
  GC* gc =  p_global_gc;
  gc_gen_destruct((GC_Gen*)gc);
  gc_metadata_destruct(gc); /* root set and mark stack */
  collector_destruct(gc);

  if( verify_live_heap ){
    gc_terminate_heap_verification(gc);
  }

  STD_FREE(p_global_gc);

  p_global_gc = NULL;
}

/* this interface need reconsidering. is_pinned is unused. */
void gc_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned) 
{   
  Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)ref;
  if (*p_ref == NULL) return;
  assert( !obj_is_marked_in_vt(*p_ref));
  assert( !obj_is_forwarded_in_vt(*p_ref) && !obj_is_forwarded_in_obj_info(*p_ref)); 
  assert( obj_is_in_gc_heap(*p_ref));
  gc_rootset_add_entry(p_global_gc, p_ref);
} 

void gc_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned) 
{  
  add_root_set_entry_interior_pointer(slot, offset, is_pinned); 
}

/* VM to force GC */
void gc_force_gc() 
{
  vm_gc_lock_enum();
  gc_reclaim_heap(p_global_gc, GC_CAUSE_RUNTIME_FORCE_GC);  
  vm_gc_unlock_enum();
}

void* gc_heap_base_address() 
{  return gc_heap_base(p_global_gc); }

void* gc_heap_ceiling_address() 
{  return gc_heap_ceiling(p_global_gc); }

/* this is a contract between vm and gc */
void mutator_initialize(GC* gc, void* tls_gc_info);
void mutator_destruct(GC* gc, void* tls_gc_info); 
void gc_thread_init(void* gc_info)
{  mutator_initialize(p_global_gc, gc_info);  }

void gc_thread_kill(void* gc_info)
{  mutator_destruct(p_global_gc, gc_info);  }

int64 gc_free_memory() 
{
  return (int64)gc_gen_free_memory_size((GC_Gen*)p_global_gc);
}

/* java heap size.*/
int64 gc_total_memory() 
{
  return (int64)((POINTER_SIZE_INT)gc_heap_ceiling(p_global_gc) - (POINTER_SIZE_INT)gc_heap_base(p_global_gc)); 
}

void gc_vm_initialized()
{ return; }

Boolean gc_is_object_pinned (Managed_Object_Handle obj)
{  return 0; }

void gc_pin_object (Managed_Object_Handle* p_object) 
{  return; }

void gc_unpin_object (Managed_Object_Handle* p_object) 
{  return; }

Managed_Object_Handle gc_get_next_live_object(void *iterator) 
{  assert(0); return NULL; }

unsigned int gc_time_since_last_gc()
{  assert(0); return 0; }


