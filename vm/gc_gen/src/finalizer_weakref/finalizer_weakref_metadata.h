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
 * @author Li-Gang Wang, 2006/11/29
 */

#ifndef _FINALIZER_WEAKREF_METADATA_H_
#define _FINALIZER_WEAKREF_METADATA_H_

#include "../common/gc_common.h"
#include "../utils/vector_block.h"
#include "../utils/sync_pool.h"

#define POOL_SEGMENT_NUM 256

typedef struct Finalizer_Weakref_Metadata{
  void *pool_segments[POOL_SEGMENT_NUM];  // malloced free pool segments' addresses array
  unsigned int next_segment_pos;          // next available position in pool_segments array
  
  Pool *free_pool;                        // list of free buffers for the five pools below
  
  Pool *objects_with_finalizer_pool;      // list of objects that have finalizer;
                                          // these objects are added in when they are allocated
  Pool *finalizable_objects_pool;         // temporary buffer for finalizable objects identified during one single GC
  
  Pool *softref_set_pool;                 // temporary buffer for soft references identified during one single GC
  Pool *weakref_set_pool;                 // temporary buffer for weak references identified during one single GC
  Pool *phanref_set_pool;                 // temporary buffer for phantom references identified during one single GC
  
  Pool *repset_pool;                      // repointed reference slot sets
  
  Vector_Block *finalizable_objects;      // buffer for finalizable_objects_pool
  Vector_Block *repset;                   // buffer for repset_pool
  
  Boolean pending_finalizers;             // there are objects waiting to be finalized
  Boolean pending_weak_references;        // there are weak references waiting to be enqueued
  
  unsigned int gc_referent_offset;        // the referent field's offset in Reference Class
}Finalizer_Weakref_Metadata;

extern unsigned int get_gc_referent_offset(void);
extern void set_gc_referent_offset(unsigned int offset);

extern void gc_finalizer_weakref_metadata_initialize(GC *gc);
extern void gc_finalizer_weakref_metadata_destruct(GC *gc);
extern void gc_finalizer_weakref_metadata_verify(GC *gc, Boolean is_before_gc);
extern void gc_reset_finalizer_weakref_metadata(GC *gc);
extern Vector_Block *finalizer_weakref_get_free_block(void);
extern void gc_finalizer_weakref_metadata_shrink(GC *gc);

extern void mutator_finalizer_add_entry(Mutator *mutator, Partial_Reveal_Object *ref);
extern void gc_finalizable_objects_add_entry(GC *gc, Partial_Reveal_Object *ref);
extern void collector_softref_set_add_entry(Collector *collector, Partial_Reveal_Object *ref);
extern void collector_weakref_set_add_entry(Collector *collector, Partial_Reveal_Object *ref);
extern void collector_phanref_set_add_entry(Collector *collector, Partial_Reveal_Object *ref);
extern void finalizer_weakref_repset_add_entry(GC *gc, Partial_Reveal_Object **ref);

extern Boolean objects_with_finalizer_pool_is_empty(GC *gc);
extern Boolean finalizable_objects_pool_is_empty(GC *gc);
extern Boolean softref_set_pool_is_empty(GC *gc);
extern Boolean weakref_set_pool_is_empty(GC *gc);
extern Boolean phanref_set_pool_is_empty(GC *gc);
extern Boolean finalizer_weakref_repset_pool_is_empty(GC *gc);

extern void gc_clear_special_reference_pools(GC *gc);


/* called before loop of recording finalizable objects */
inline void gc_reset_finalizable_objects(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  assert(!metadata->finalizable_objects);
  metadata->finalizable_objects = pool_get_entry(metadata->free_pool);
}
/* called after loop of recording finalizable objects */
inline void gc_put_finalizable_objects(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  pool_put_entry(metadata->finalizable_objects_pool, metadata->finalizable_objects);
  metadata->finalizable_objects = NULL;
}

/* called before loop of recording repointed reference */
inline void finalizer_weakref_reset_repset(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  assert(!metadata->repset);
  metadata->repset = pool_get_entry(metadata->free_pool);
}
/* called after loop of recording repointed reference */
inline void finalizer_weakref_put_repset(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  pool_put_entry(metadata->repset_pool, metadata->repset);
  metadata->repset = NULL;
}

#endif // _FINALIZER_WEAKREF_METADATA_H_
