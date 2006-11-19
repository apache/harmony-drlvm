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

unsigned int HEAP_SIZE_DEFAULT = 256 * MB;

/* heap size limit is not interesting. only for manual tuning purpose */
unsigned int min_heap_size_bytes = 32 * MB;
unsigned int max_heap_size_bytes = 256 * MB;

static size_t parse_size_string(const char* size_string) 
{
  size_t len = strlen(size_string);
  size_t unit = 1;
  if (tolower(size_string[len - 1]) == 'k') {
    unit = 1024;
  } else if (tolower(size_string[len - 1]) == 'm') {
    unit = 1024 * 1024;
  } else if (tolower(size_string[len - 1]) == 'g') {
    unit = 1024 * 1024 * 1024;
  }
  size_t size = atol(size_string);
  size_t res = size * unit;
  if (res / unit != size) {
    /* overflow happened */
    return 0;
  }
  return res;
}

static bool get_property_value_boolean(char* name) 
{
  const char* value = vm_get_property_value(name);
  
  return (strcmp("0", value) != 0
    && strcmp("off", value) != 0 
    && strcmp("false", value) != 0);
}

static int get_property_value_int(char* name) 
{
  const char* value = vm_get_property_value(name);
  return (NULL == value) ? 0 : atoi(value);
}

static bool is_property_set(char* name) 
{
  const char* value = vm_get_property_value(name);
  return (NULL != value && 0 != value[0]);
}

static void parse_configuration_properties() 
{
  unsigned int max_heap_size = HEAP_SIZE_DEFAULT;
  unsigned int min_heap_size = min_heap_size_bytes;
  
  if (is_property_set("gc.mx")) {
    max_heap_size = parse_size_string(vm_get_property_value("gc.mx"));

    if (max_heap_size < min_heap_size)
      max_heap_size = min_heap_size;
    if (0 == max_heap_size) 
      max_heap_size = HEAP_SIZE_DEFAULT;
 
    min_heap_size = max_heap_size / 10;
    if (min_heap_size < min_heap_size_bytes) min_heap_size = min_heap_size_bytes;
  }

  if (is_property_set("gc.ms")) {
    min_heap_size = parse_size_string(vm_get_property_value("gc.ms"));
    if (min_heap_size < min_heap_size_bytes) 
      min_heap_size = min_heap_size_bytes;
  }

  if (min_heap_size > max_heap_size)
    max_heap_size = min_heap_size;

  min_heap_size_bytes = min_heap_size;
  max_heap_size_bytes = max_heap_size;

  if (is_property_set("gc.num_collectors")) {
    unsigned int num = get_property_value_int("gc.num_collectors");
    NUM_COLLECTORS = (num==0)? NUM_COLLECTORS:num;
  }

  if (is_property_set("gc.gen_mode")) {
    NEED_BARRIER = get_property_value_boolean("gc.gen_mode");
  }
  
  return;  
}

static GC* p_global_gc = NULL;

void gc_init() 
{  
  parse_configuration_properties();
  
  assert(p_global_gc == NULL);
  GC* gc = (GC*)STD_MALLOC(sizeof(GC_Gen));
  assert(gc);
  memset(gc, 0, sizeof(GC));  
  p_global_gc = gc;
  gc_gen_initialize((GC_Gen*)gc, min_heap_size_bytes, max_heap_size_bytes);
  
  return;
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
  gc_gen_reclaim_heap((GC_Gen*)p_global_gc, GC_CAUSE_RUNTIME_FORCE_GC);  
  vm_gc_unlock_enum();
}

void gc_wrapup() 
{  
  gc_gen_destruct((GC_Gen*)p_global_gc);
  p_global_gc = NULL;
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

