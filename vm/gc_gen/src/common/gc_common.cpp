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
 * @author Xiao-Feng Li, 2006/12/3
 */

#include "gc_common.h"
#include "gc_metadata.h"
#include "../thread/mutator.h"
#include "../thread/marker.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "../common/space_tuner.h"
#include "interior_pointer.h"
#include "collection_scheduler.h"
#include "gc_concurrent.h"

unsigned int Cur_Mark_Bit = 0x1;
unsigned int Cur_Forward_Bit = 0x2;

unsigned int SPACE_ALLOC_UNIT;

extern char* GC_VERIFY;

extern POINTER_SIZE_INT NOS_SIZE;
extern POINTER_SIZE_INT MIN_NOS_SIZE;
extern POINTER_SIZE_INT INIT_LOS_SIZE;

extern Boolean FORCE_FULL_COMPACT;
extern Boolean MINOR_ALGORITHM;
extern Boolean MAJOR_ALGORITHM;

extern unsigned int NUM_MARKERS;
extern unsigned int NUM_COLLECTORS;
extern unsigned int MINOR_COLLECTORS;
extern unsigned int MAJOR_COLLECTORS;

extern Boolean IGNORE_VTABLE_TRACING;

POINTER_SIZE_INT HEAP_SIZE_DEFAULT = 256 * MB;
POINTER_SIZE_INT min_heap_size_bytes = 16 * MB;
POINTER_SIZE_INT max_heap_size_bytes = 0;

extern Boolean JVMTI_HEAP_ITERATION ;

extern Boolean IS_MOVE_COMPACT;
extern Boolean USE_CONCURRENT_GC;

#if defined(ALLOC_ZEROING) && defined(ALLOC_PREFETCH)
POINTER_SIZE_INT PREFETCH_DISTANCE = 1024;
POINTER_SIZE_INT ZEROING_SIZE = 256;
POINTER_SIZE_INT PREFETCH_STRIDE = 64;
Boolean PREFETCH_ENABLED = FALSE;
#endif

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
    DIE2("gc.base","Warning: property value "<<property_name<<"is not set!");
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
    DIE2("gc.base","Warning: property value "<<property_name<<"is not set!");
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

void gc_parse_options(GC* gc) 
{
  TRACE2("gc.process", "GC: parse options ...\n");
  if (!get_boolean_property("vm.assert_dialog", TRUE, VM_PROPERTIES))
    disable_assert_dialogs();
  
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
//      printf("Min heap size: too small, reset to %d MB! \n", min_heap_size/MB);
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

  /* GC algorithm decision */
  /* Step 1: */
  char* minor_algo = NULL;
  char* major_algo = NULL;
  
  if (is_property_set("gc.minor_algorithm", VM_PROPERTIES) == 1) {
    minor_algo = get_property("gc.minor_algorithm", VM_PROPERTIES);
  }
  
  if (is_property_set("gc.major_algorithm", VM_PROPERTIES) == 1) {
    major_algo = get_property("gc.major_algorithm", VM_PROPERTIES);
  }
  
  gc_decide_collection_algorithm((GC_Gen*)gc, minor_algo, major_algo);
  gc->generate_barrier = gc_is_gen_mode();

  if( minor_algo) destroy_property_value(minor_algo);
  if( major_algo) destroy_property_value(major_algo);

  /* Step 2: */
  /* NOTE:: this has to stay after above!! */
  if (is_property_set("gc.force_major_collect", VM_PROPERTIES) == 1) {
    FORCE_FULL_COMPACT = get_boolean_property("gc.force_major_collect");
    if(FORCE_FULL_COMPACT){
      gc_disable_gen_mode();
      gc->generate_barrier = FALSE;
    }
  }

  /* Step 3: */
  /* NOTE:: this has to stay after above!! */
  if (is_property_set("gc.generate_barrier", VM_PROPERTIES) == 1) {
    Boolean generate_barrier = get_boolean_property("gc.generate_barrier");
    gc->generate_barrier = generate_barrier || gc->generate_barrier;
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
    USE_CONCURRENT_GC= get_boolean_property("gc.concurrent_gc");
  }
  
#if defined(ALLOC_ZEROING) && defined(ALLOC_PREFETCH)
  if(is_property_set("gc.prefetch",VM_PROPERTIES) ==1) {
  	PREFETCH_ENABLED=get_boolean_property("gc.prefetch");
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
  	if(!PREFETCH_ENABLED) {
  		WARN2("gc.zeroing_size","Warning: Zeroing size set with Prefetch disabled!");
  	}	
  }	 
#endif

  return;
}

void gc_assign_free_area_to_mutators(GC* gc)
{
#ifndef USE_MARK_SWEEP_GC
  gc_gen_assign_free_area_to_mutators((GC_Gen*)gc);
#endif
}

void gc_copy_interior_pointer_table_to_rootset();

/*used for computing collection time and mutator time*/
static int64 collection_start_time = time_now();
static int64 collection_end_time = time_now();

int64 get_collection_end_time()
{ return collection_end_time; }

void gc_reclaim_heap(GC* gc, unsigned int gc_cause)
{
  INFO2("gc.process", "\nGC: GC start ...\n");
  
  collection_start_time = time_now();
  int64 mutator_time = collection_start_time - collection_end_time;
  
  /* FIXME:: before mutators suspended, the ops below should be very careful
     to avoid racing with mutators. */
  gc->num_collections++;
  gc->cause = gc_cause;
  gc_decide_collection_kind((GC_Gen*)gc, gc_cause);

#ifdef MARK_BIT_FLIPPING
  if(gc_match_kind(gc, MINOR_COLLECTION)) mark_bit_flip();
#endif

  if(!USE_CONCURRENT_GC){
    gc_metadata_verify(gc, TRUE);
#ifndef BUILD_IN_REFERENT
    gc_finref_metadata_verify((GC*)gc, TRUE);
#endif
  }
  /* Stop the threads and collect the roots. */
  lock(gc->enumerate_rootset_lock);
  INFO2("gc.process", "GC: stop the threads and enumerate rootset ...\n");
  gc_clear_rootset(gc);
  gc_reset_rootset(gc);
  int disable_count = hythread_reset_suspend_disable();
  vm_enumerate_root_set_all_threads();
  gc_copy_interior_pointer_table_to_rootset();
  gc_set_rootset(gc);
  unlock(gc->enumerate_rootset_lock);
  
  if(USE_CONCURRENT_GC && gc_mark_is_concurrent()){
    gc_finish_concurrent_mark(gc);
  }
  
  /* this has to be done after all mutators are suspended */
  gc_reset_mutator_context(gc);
  
  if(!IGNORE_FINREF ) gc_set_obj_with_fin(gc);

#ifndef USE_MARK_SWEEP_GC
  gc_gen_reclaim_heap((GC_Gen*)gc, collection_start_time);
#else
  gc_ms_reclaim_heap((GC_MS*)gc);
#endif

  /* FIXME:: clear root set here to support verify. */
#ifdef COMPRESS_REFERENCE
  gc_set_pool_clear(gc->metadata->gc_uncompressed_rootset_pool);
#endif

  gc_reset_interior_pointer_table();
  
  gc_metadata_verify(gc, FALSE);
  
  collection_end_time = time_now(); 

#ifndef USE_MARK_SWEEP_GC
  gc_gen_collection_verbose_info((GC_Gen*)gc, collection_end_time - collection_start_time, mutator_time);
  gc_gen_space_verbose_info((GC_Gen*)gc);
#endif

  if(gc_is_gen_mode()) gc_prepare_mutator_remset(gc);
  
  int64 mark_time = 0;
  if(USE_CONCURRENT_GC && gc_mark_is_concurrent()){
    gc_reset_concurrent_mark(gc);
    mark_time = gc_get_concurrent_mark_time(gc);
  }

#ifndef USE_MARK_SWEEP_GC
  if(USE_CONCURRENT_GC && gc_need_start_concurrent_mark(gc))
    gc_start_concurrent_mark(gc);
#endif

  if(!IGNORE_FINREF ){
    INFO2("gc.process", "GC: finref process after collection ...\n");
    gc_put_finref_to_vm(gc);
    gc_reset_finref_metadata(gc);
    gc_activate_finref_threads((GC*)gc);
#ifndef BUILD_IN_REFERENT
  } else {
    gc_clear_weakref_pools(gc);
#endif
  }

#ifdef USE_MARK_SWEEP_GC
  gc_ms_update_space_statistics((GC_MS*)gc);
#endif

  gc_assign_free_area_to_mutators(gc);
  
  if(USE_CONCURRENT_GC) gc_update_collection_scheduler(gc, mutator_time, mark_time);
  
  vm_reclaim_native_objs();
  vm_resume_threads_after();
  assert(hythread_is_suspend_enabled());
  hythread_set_suspend_disable(disable_count);
  INFO2("gc.process", "GC: GC end\n");
  return;
}


