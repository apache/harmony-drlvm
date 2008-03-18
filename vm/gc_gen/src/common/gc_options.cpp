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

#include "gc_common.h"

extern char* GC_VERIFY;
extern POINTER_SIZE_INT NOS_SIZE;
extern POINTER_SIZE_INT MIN_NOS_SIZE;
extern POINTER_SIZE_INT INIT_LOS_SIZE;
extern POINTER_SIZE_INT TOSPACE_SIZE;
extern POINTER_SIZE_INT MOS_RESERVE_SIZE;

extern Boolean GEN_NONGEN_SWITCH;

extern Boolean FORCE_FULL_COMPACT;

extern unsigned int NUM_MARKERS;
extern unsigned int NUM_COLLECTORS;
extern unsigned int MINOR_COLLECTORS;
extern unsigned int MAJOR_COLLECTORS;

extern Boolean IGNORE_VTABLE_TRACING;
extern Boolean IGNORE_FINREF;

extern Boolean JVMTI_HEAP_ITERATION ;

extern Boolean USE_CONCURRENT_GC;
extern Boolean USE_CONCURRENT_ENUMERATION;
extern Boolean USE_CONCURRENT_MARK;
extern Boolean USE_CONCURRENT_SWEEP;


POINTER_SIZE_INT HEAP_SIZE_DEFAULT = 256 * MB;
POINTER_SIZE_INT min_heap_size_bytes = 16 * MB;
POINTER_SIZE_INT max_heap_size_bytes = 0;


unsigned int GC_PROP;

GC* gc_mc_create();
GC* gc_ms_create();

static GC* gc_decide_collection_algo(char* unique_algo, Boolean has_los)
{
  /* if unique_algo is not set, gc_gen_decide_collection_algo is called. */
  assert(unique_algo);
  
  GC_PROP = ALGO_POOL_SHARE | ALGO_DEPTH_FIRST;
  
  assert(!has_los); /* currently unique GCs don't use LOS */
  if(has_los) 
    GC_PROP |= ALGO_HAS_LOS;
  
  Boolean use_default = FALSE;

  GC* gc;
  
  string_to_upper(unique_algo);
   
  if(!strcmp(unique_algo, "MOVE_COMPACT")){
    GC_PROP |= ALGO_COMPACT_MOVE;
    gc = gc_mc_create();  

  }else if(!strcmp(unique_algo, "MARK_SWEEP")){
    GC_PROP |= ALGO_MS_NORMAL;
    gc = gc_ms_create();
  }else{
    WARN2("gc.base","\nWarning: GC algorithm setting incorrect. Will use default value.\n");
    GC_PROP |= ALGO_COMPACT_MOVE;
    gc = gc_mc_create();  
  }

  return gc;
}

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
        DIE2("gc.base","Warning: property value "<<property_name<<"is not set!");
        exit(0);
    }
      
    return return_value;
}

static Boolean get_boolean_property(const char *property_name)
{
  assert(property_name);
  char *value = get_property(property_name, VM_PROPERTIES);
  if (NULL == value){
    DIE2("gc.base","Warning: property value "<<property_name<<" is not set!");
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
    DIE2("gc.base","Warning: property value "<<property_name<<" is not set! Use upper case?");
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

void gc_decide_concurrent_algorithm(char* concurrent_algo);
GC* gc_gen_decide_collection_algo(char* minor_algo, char* major_algo, Boolean has_los);
void gc_set_gen_mode(Boolean status);

GC* gc_parse_options() 
{
  TRACE2("gc.process", "GC: parse options ...\n");

  GC* gc;
  
  /* GC algorithm decision */
  /* Step 1: */
  char* minor_algo = NULL;
  char* major_algo = NULL;
  char* unique_algo = NULL;
  
  if (is_property_set("gc.minor_algorithm", VM_PROPERTIES) == 1) {
    minor_algo = get_property("gc.minor_algorithm", VM_PROPERTIES);
  }
  
  if (is_property_set("gc.major_algorithm", VM_PROPERTIES) == 1) {
    major_algo = get_property("gc.major_algorithm", VM_PROPERTIES);
  }
    
  if (is_property_set("gc.uniqe_algorithm", VM_PROPERTIES) == 1) {
    unique_algo = get_property("gc.unique_algorithm", VM_PROPERTIES);
  }

  Boolean has_los = FALSE;
  if (is_property_set("gc.has_los", VM_PROPERTIES) == 1) {
    has_los = get_boolean_property("gc.has_los");
  }

  if(unique_algo){
    if(minor_algo || major_algo){
      WARN2("gc.base","Warning: generational options cannot be set with unique_algo, ignored.");
    }
    gc = gc_decide_collection_algo(unique_algo, has_los);
    destroy_property_value(unique_algo);  
  
  }else{ /* default */
    gc = gc_gen_decide_collection_algo(minor_algo, major_algo, has_los);
    if( minor_algo) destroy_property_value(minor_algo);
    if( major_algo) destroy_property_value(major_algo);
  }
  
  if (is_property_set("gc.gen_mode", VM_PROPERTIES) == 1) {
    Boolean gen_mode = get_boolean_property("gc.gen_mode");
    gc_set_gen_mode(gen_mode);
  }

  /* Step 2: */

  /* NOTE:: this has to stay after above!! */
  if (is_property_set("gc.force_major_collect", VM_PROPERTIES) == 1) {
    FORCE_FULL_COMPACT = get_boolean_property("gc.force_major_collect");
    if(FORCE_FULL_COMPACT){
      gc_set_gen_mode(FALSE);
    }
  }

  /* Step 3: */
  /* NOTE:: this has to stay after above!! */
  gc->generate_barrier = gc_is_gen_mode();
  
  if (is_property_set("gc.generate_barrier", VM_PROPERTIES) == 1) {
    Boolean generate_barrier = get_boolean_property("gc.generate_barrier");
    gc->generate_barrier = (generate_barrier || gc->generate_barrier);
  }
  
/* ///////////////////////////////////////////////////   */
  
  POINTER_SIZE_INT max_heap_size = HEAP_SIZE_DEFAULT;
  POINTER_SIZE_INT min_heap_size = min_heap_size_bytes;
  
  if (is_property_set("gc.mx", VM_PROPERTIES) == 1) {
    max_heap_size = get_size_property("gc.mx");

    if (max_heap_size < min_heap_size){
      max_heap_size = min_heap_size;
      WARN2("gc.base","Warning: Max heap size you set is too small, reset to "<<max_heap_size/MB<<" MB!");
    }
    if (0 == max_heap_size){
      max_heap_size = HEAP_SIZE_DEFAULT;
      WARN2("gc.base","Warning: Max heap size you set euqals to zero, reset to "<<max_heap_size/MB<<" MB!");
    }
 
    min_heap_size = max_heap_size / 10;
    if (min_heap_size < min_heap_size_bytes){
      min_heap_size = min_heap_size_bytes;
      //printf("Min heap size: too small, reset to %d MB! \n", min_heap_size/MB);
    }
  }

  if (is_property_set("gc.ms", VM_PROPERTIES) == 1) {
    min_heap_size = get_size_property("gc.ms");
    if (min_heap_size < min_heap_size_bytes){
      min_heap_size = min_heap_size_bytes;
      WARN2("gc.base","Warning: Min heap size you set is too small, reset to "<<min_heap_size/MB<<" MB!");
    } 
  }

  if (min_heap_size > max_heap_size){
    max_heap_size = min_heap_size;
    WARN2("gc.base","Warning: Max heap size is too small, reset to "<<max_heap_size/MB<<" MB!");
  }

  min_heap_size_bytes = min_heap_size;
  max_heap_size_bytes = max_heap_size;

  if (is_property_set("gc.nos_size", VM_PROPERTIES) == 1) {
    NOS_SIZE = get_size_property("gc.nos_size");
  }

  if (is_property_set("gc.min_nos_size", VM_PROPERTIES) == 1) {
    MIN_NOS_SIZE = get_size_property("gc.min_nos_size");
  }

  if (is_property_set("gc.init_los_size", VM_PROPERTIES) == 1) {
    INIT_LOS_SIZE = get_size_property("gc.init_los_size");
  }  

  if (is_property_set("gc.num_collectors", VM_PROPERTIES) == 1) {
    unsigned int num = get_int_property("gc.num_collectors");
    NUM_COLLECTORS = (num==0)? NUM_COLLECTORS:num;
  }

  if (is_property_set("gc.num_markers", VM_PROPERTIES) == 1) {
    unsigned int num = get_int_property("gc.num_markers");
    NUM_MARKERS = (num==0)? NUM_MARKERS:num;
  }

  if (is_property_set("gc.tospace_size", VM_PROPERTIES) == 1) {
    TOSPACE_SIZE = get_size_property("gc.tospace_size");
  }

  if (is_property_set("gc.mos_reserve_size", VM_PROPERTIES) == 1) {
    MOS_RESERVE_SIZE = get_size_property("gc.mos_reserve_size");
  }

  if (is_property_set("gc.nos_partial_forward", VM_PROPERTIES) == 1) {
    NOS_PARTIAL_FORWARD = get_boolean_property("gc.nos_partial_forward");
  }
    
  if (is_property_set("gc.minor_collectors", VM_PROPERTIES) == 1) {
    MINOR_COLLECTORS = get_int_property("gc.minor_collectors");
  }

  if (is_property_set("gc.major_collectors", VM_PROPERTIES) == 1) {
    MAJOR_COLLECTORS = get_int_property("gc.major_collectors");
  }

  if (is_property_set("gc.ignore_finref", VM_PROPERTIES) == 1) {
    IGNORE_FINREF = get_boolean_property("gc.ignore_finref");
  }

  if (is_property_set("gc.verify", VM_PROPERTIES) == 1) {
    char* value = get_property("gc.verify", VM_PROPERTIES);
    GC_VERIFY = strdup(value);
    destroy_property_value(value);
  }

  if (is_property_set("gc.gen_nongen_switch", VM_PROPERTIES) == 1){
    GEN_NONGEN_SWITCH= get_boolean_property("gc.gen_nongen_switch");
    gc->generate_barrier = TRUE;
  }

  if (is_property_set("gc.heap_iteration", VM_PROPERTIES) == 1) {
    JVMTI_HEAP_ITERATION = get_boolean_property("gc.heap_iteration");
  }

  if (is_property_set("gc.ignore_vtable_tracing", VM_PROPERTIES) == 1) {
    IGNORE_VTABLE_TRACING = get_boolean_property("gc.ignore_vtable_tracing");
  }

  if (is_property_set("gc.use_large_page", VM_PROPERTIES) == 1){
    char* value = get_property("gc.use_large_page", VM_PROPERTIES);
    large_page_hint = strdup(value);
    destroy_property_value(value);
  }

  if (is_property_set("gc.concurrent_gc", VM_PROPERTIES) == 1){
    Boolean use_all_concurrent_phase= get_boolean_property("gc.concurrent_gc");
    if(use_all_concurrent_phase){
      USE_CONCURRENT_ENUMERATION = TRUE;
      USE_CONCURRENT_MARK = TRUE;
      USE_CONCURRENT_SWEEP = TRUE;
      gc->generate_barrier = TRUE;
    }
  }

  if (is_property_set("gc.concurrent_enumeration", VM_PROPERTIES) == 1){
    USE_CONCURRENT_ENUMERATION= get_boolean_property("gc.concurrent_enumeration");
    if(USE_CONCURRENT_ENUMERATION){
      USE_CONCURRENT_GC = TRUE;      
      gc->generate_barrier = TRUE;
    }
  }

  if (is_property_set("gc.concurrent_mark", VM_PROPERTIES) == 1){
    USE_CONCURRENT_MARK= get_boolean_property("gc.concurrent_mark");
    if(USE_CONCURRENT_MARK){
      USE_CONCURRENT_GC = TRUE;      
      gc->generate_barrier = TRUE;
    }
  }

  if (is_property_set("gc.concurrent_sweep", VM_PROPERTIES) == 1){
    USE_CONCURRENT_SWEEP= get_boolean_property("gc.concurrent_sweep");
    if(USE_CONCURRENT_SWEEP){
      USE_CONCURRENT_GC = TRUE;
    }
  }

  char* concurrent_algo = NULL;
  
  if (is_property_set("gc.concurrent_algorithm", VM_PROPERTIES) == 1) {
    concurrent_algo = get_property("gc.concurrent_algorithm", VM_PROPERTIES);
  }
  
  gc_decide_concurrent_algorithm(concurrent_algo);

#if defined(ALLOC_ZEROING) && defined(ALLOC_PREFETCH)
  if(is_property_set("gc.prefetch",VM_PROPERTIES) ==1) {
    PREFETCH_ENABLED = get_boolean_property("gc.prefetch");
  }

  if(is_property_set("gc.prefetch_distance",VM_PROPERTIES)==1) {
    PREFETCH_DISTANCE = get_size_property("gc.prefetch_distance");
    if(!PREFETCH_ENABLED) {
      WARN2("gc.prefetch_distance","Warning: Prefetch distance set with Prefetch disabled!");
    }
  }

  if(is_property_set("gc.prefetch_stride",VM_PROPERTIES)==1) {
    PREFETCH_STRIDE = get_size_property("gc.prefetch_stride");
    if(!PREFETCH_ENABLED) {
      WARN2("gc.prefetch_stride","Warning: Prefetch stride set  with Prefetch disabled!");
    }  
  }
  
  if(is_property_set("gc.zeroing_size",VM_PROPERTIES)==1) {
    ZEROING_SIZE = get_size_property("gc.zeroing_size");
  }   
#endif

#ifdef PREFETCH_SUPPORTED
  if(is_property_set("gc.mark_prefetch",VM_PROPERTIES) ==1) {
    mark_prefetch = get_boolean_property("gc.mark_prefetch");
  }  
#endif

  return gc;
}


