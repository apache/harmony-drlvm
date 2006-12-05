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
 * @author Xiao-Feng Li, 2006/12/3
 */

#include "gc_common.h"
#include "gc_metadata.h"
#include "../thread/mutator.h"
#include "../verify/verify_live_heap.h"

extern Boolean NEED_BARRIER;
extern unsigned int NUM_COLLECTORS;
extern Boolean GC_VERIFY;
extern unsigned int NOS_SIZE;
extern Boolean NOS_PARTIAL_FORWARD;

unsigned int HEAP_SIZE_DEFAULT = 256 * MB;
unsigned int min_heap_size_bytes = 32 * MB;
unsigned int max_heap_size_bytes = 0;

static int get_int_property(const char *property_name)
{
    assert(property_name);
    char *value = get_property(property_name, VM_PROPERTIES);
    int return_value;
    if (NULL != value)
    {
        return_value = atoi(value);
        destroy_property_value(value);
    }else{
        printf("property value %s is not set\n", property_name);
        exit(0);
    }
      
    return return_value;
}

static Boolean get_boolean_property(const char *property_name)
{
  assert(property_name);
  char *value = get_property(property_name, VM_PROPERTIES);
  if (NULL == value){
    printf("property value %s is not set\n", property_name);
    exit(0);
  }
  
  Boolean return_value;
  if (0 == strcmp("no", value)
      || 0 == strcmp("off", value)
      || 0 == strcmp("false", value)
      || 0 == strcmp("0", value))
  {
    return_value = FALSE;
  }
  else if (0 == strcmp("yes", value)
           || 0 == strcmp("on", value)
           || 0 == strcmp("true", value)
           || 0 == strcmp("1", value))
  {
    return_value = TRUE;
  }else{
    printf("property value %s is not properly set\n", property_name);
    exit(0);
  }
    
  destroy_property_value(value);
  return return_value;
}

static size_t get_size_property(const char* name) 
{
  char* size_string = get_property(name, VM_PROPERTIES);
  size_t size = atol(size_string);
  int sizeModifier = tolower(size_string[strlen(size_string) - 1]);
  destroy_property_value(size_string);

  size_t unit = 1;
  switch (sizeModifier) {
  case 'k': unit = 1024; break;
  case 'm': unit = 1024 * 1024; break;
  case 'g': unit = 1024 * 1024 * 1024;break;
  }

  size_t res = size * unit;
  if (res / unit != size) {
    /* overflow happened */
    return 0;
  }
  return res;
}

void gc_parse_options() 
{
  unsigned int max_heap_size = HEAP_SIZE_DEFAULT;
  unsigned int min_heap_size = min_heap_size_bytes;
  
  if (is_property_set("gc.mx", VM_PROPERTIES) == 1) {
    max_heap_size = get_size_property("gc.mx");

    if (max_heap_size < min_heap_size)
      max_heap_size = min_heap_size;
    if (0 == max_heap_size) 
      max_heap_size = HEAP_SIZE_DEFAULT;
 
    min_heap_size = max_heap_size / 10;
    if (min_heap_size < min_heap_size_bytes) min_heap_size = min_heap_size_bytes;
  }

  if (is_property_set("gc.ms", VM_PROPERTIES) == 1) {
    min_heap_size = get_size_property("gc.ms");
    if (min_heap_size < min_heap_size_bytes) 
      min_heap_size = min_heap_size_bytes;
  }

  if (min_heap_size > max_heap_size)
    max_heap_size = min_heap_size;

  min_heap_size_bytes = min_heap_size;
  max_heap_size_bytes = max_heap_size;

  if (is_property_set("gc.nos_size", VM_PROPERTIES) == 1) {
    NOS_SIZE = get_size_property("gc.nos_size");
  }

  if (is_property_set("gc.num_collectors", VM_PROPERTIES) == 1) {
    unsigned int num = get_int_property("gc.num_collectors");
    NUM_COLLECTORS = (num==0)? NUM_COLLECTORS:num;
  }

  if (is_property_set("gc.gen_mode", VM_PROPERTIES) == 1) {
    NEED_BARRIER = get_boolean_property("gc.gen_mode");
  }

  if (is_property_set("gc.nos_partial_forward", VM_PROPERTIES) == 1) {
    NOS_PARTIAL_FORWARD = get_boolean_property("gc.nos_partial_forward");
  }

  if (is_property_set("gc.verify", VM_PROPERTIES) == 1) {
    GC_VERIFY = get_boolean_property("gc.verify");
  }
  
  return;  
}

struct GC_Gen;
void gc_gen_reclaim_heap(GC_Gen* gc);
unsigned int gc_decide_collection_kind(GC_Gen* gc, unsigned int gc_cause);

void gc_reclaim_heap(GC* gc, unsigned int gc_cause)
{ 
  gc->num_collections++;

  gc->collect_kind = gc_decide_collection_kind((GC_Gen*)gc, gc_cause);
  //gc->collect_kind = MAJOR_COLLECTION;

  gc_metadata_verify(gc, TRUE);
  
  /* Stop the threads and collect the roots. */
  gc_reset_rootset(gc);  
  vm_enumerate_root_set_all_threads();
  gc_set_rootset(gc); 
    
  if(verify_live_heap) gc_verify_heap(gc, TRUE);

  gc_gen_reclaim_heap((GC_Gen*)gc);  
  
  if(verify_live_heap) gc_verify_heap(gc, FALSE);
  
  gc_metadata_verify(gc, FALSE);
    
  gc_reset_mutator_context(gc);
  vm_resume_threads_after();

  return;
}

