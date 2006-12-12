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

#include "finalizer_weakref_metadata.h"
#include "../thread/mutator.h"
#include "../thread/collector.h"

#define POOL_SEGMENT_SIZE_BIT_SHIFT 20
#define POOL_SEGMENT_SIZE_BYTES (1 << POOL_SEGMENT_SIZE_BIT_SHIFT)

#define METADATA_BLOCK_SIZE_BIT_SHIFT 10
#define METADATA_BLOCK_SIZE_BYTES (1<<METADATA_BLOCK_SIZE_BIT_SHIFT)

static Finalizer_Weakref_Metadata finalizer_weakref_metadata;

unsigned int get_gc_referent_offset(void)
{
  return finalizer_weakref_metadata.gc_referent_offset;
}
void set_gc_referent_offset(unsigned int offset)
{
  finalizer_weakref_metadata.gc_referent_offset = offset;
}

void gc_finalizer_weakref_metadata_initialize(GC *gc)
{
  void *pool_segment = STD_MALLOC(POOL_SEGMENT_SIZE_BYTES);
  memset(pool_segment, 0, POOL_SEGMENT_SIZE_BYTES);
  finalizer_weakref_metadata.next_segment_pos = 0;
  finalizer_weakref_metadata.pool_segments[finalizer_weakref_metadata.next_segment_pos] = pool_segment;
  ++finalizer_weakref_metadata.next_segment_pos;
  
  finalizer_weakref_metadata.free_pool = sync_pool_create();
  finalizer_weakref_metadata.objects_with_finalizer_pool = sync_pool_create();
  finalizer_weakref_metadata.finalizable_objects_pool = sync_pool_create();
  finalizer_weakref_metadata.softref_set_pool = sync_pool_create();
  finalizer_weakref_metadata.weakref_set_pool = sync_pool_create();
  finalizer_weakref_metadata.phanref_set_pool = sync_pool_create();
  finalizer_weakref_metadata.repset_pool = sync_pool_create();
  
  finalizer_weakref_metadata.finalizable_objects = NULL;
  finalizer_weakref_metadata.repset = NULL;
  
  unsigned int num_blocks =  POOL_SEGMENT_SIZE_BYTES >> METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(unsigned int i=0; i<num_blocks; i++){
    Vector_Block *block = (Vector_Block *)((unsigned int)pool_segment + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
    assert(vector_block_is_empty((Vector_Block *)block));
    pool_put_entry(finalizer_weakref_metadata.free_pool, (void *)block);
  }
  
  finalizer_weakref_metadata.pending_finalizers = FALSE;
  finalizer_weakref_metadata.pending_weak_references = FALSE;
  finalizer_weakref_metadata.gc_referent_offset = 0;
  
  gc->finalizer_weakref_metadata = &finalizer_weakref_metadata;
  return;
}

void gc_finalizer_weakref_metadata_destruct(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  sync_pool_destruct(metadata->free_pool);
  sync_pool_destruct(metadata->objects_with_finalizer_pool);
  sync_pool_destruct(metadata->finalizable_objects_pool);
  sync_pool_destruct(metadata->softref_set_pool);
  sync_pool_destruct(metadata->weakref_set_pool);
  sync_pool_destruct(metadata->phanref_set_pool);
  sync_pool_destruct(metadata->repset_pool);
  
  metadata->finalizable_objects = NULL;
  metadata->repset = NULL;
  
  for(unsigned int i=0; i<metadata->next_segment_pos; i++){
    assert(metadata->pool_segments[i]);
    STD_FREE(metadata->pool_segments[i]);
  }
  
  gc->finalizer_weakref_metadata = NULL;
}

void gc_finalizer_weakref_metadata_verify(GC *gc, Boolean is_before_gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  assert(pool_is_empty(metadata->finalizable_objects_pool));
  assert(pool_is_empty(metadata->softref_set_pool));
  assert(pool_is_empty(metadata->weakref_set_pool));
  assert(pool_is_empty(metadata->phanref_set_pool));
  assert(pool_is_empty(metadata->repset_pool));
  assert(metadata->finalizable_objects == NULL);
  assert(metadata->repset == NULL);
  
  return;
}

void gc_reset_finalizer_weakref_metadata(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  Pool *objects_with_finalizer_pool = metadata->objects_with_finalizer_pool;
  Pool *finalizable_objects_pool = metadata->finalizable_objects_pool;
  
  assert(pool_is_empty(finalizable_objects_pool));
  assert(pool_is_empty(metadata->softref_set_pool));
  assert(pool_is_empty(metadata->weakref_set_pool));
  assert(pool_is_empty(metadata->phanref_set_pool));
  assert(pool_is_empty(metadata->repset_pool));
  assert(metadata->finalizable_objects == NULL);
  assert(metadata->repset == NULL);
  
  while(Vector_Block *block = pool_get_entry(objects_with_finalizer_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    if(vector_block_iterator_end(block, iter)){
      vector_block_clear(block);
      pool_put_entry(metadata->free_pool, block);
    } else {
      pool_put_entry(finalizable_objects_pool, block);
    }
  }
  assert(pool_is_empty(objects_with_finalizer_pool));
  metadata->objects_with_finalizer_pool = finalizable_objects_pool;
  metadata->finalizable_objects_pool = objects_with_finalizer_pool;
}

/* called when there is no Vector_Block in finalizer_weakref_metadata->free_pool
 * extend the pool by a pool segment
 */
static void gc_finalizer_weakref_metadata_extend(void)
{
  Finalizer_Weakref_Metadata metadata = finalizer_weakref_metadata;
  
  unsigned int segment_pos = metadata.next_segment_pos;
  while(segment_pos < POOL_SEGMENT_NUM){
    unsigned int next_segment_pos = segment_pos + 1;
    unsigned int temp = (unsigned int)atomic_cas32((volatile unsigned int *)&metadata.next_segment_pos, next_segment_pos, segment_pos);
    if(temp == segment_pos)
      break;
    segment_pos = metadata.next_segment_pos;
  }
  if(segment_pos > POOL_SEGMENT_NUM)
    return;
  
  void *pool_segment = STD_MALLOC(POOL_SEGMENT_SIZE_BYTES);
  memset(pool_segment, 0, POOL_SEGMENT_SIZE_BYTES);
  metadata.pool_segments[segment_pos] = pool_segment;
  
  unsigned int num_blocks =  POOL_SEGMENT_SIZE_BYTES >> METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(unsigned int i=0; i<num_blocks; i++){
    Vector_Block *block = (Vector_Block *)((unsigned int)pool_segment + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
    assert(vector_block_is_empty((Vector_Block *)block));
    pool_put_entry(metadata.free_pool, (void *)block);
  }
  
  return;
}

Vector_Block *finalizer_weakref_get_free_block(void)
{
  Vector_Block *block;
  
  while(!(block = pool_get_entry(finalizer_weakref_metadata.free_pool)))
    gc_finalizer_weakref_metadata_extend();
  return block;
}

/* called when GC completes and there is no Vector_Block in the last five pools of gc->finalizer_weakref_metadata
 * shrink the free pool by half
 */
void gc_finalizer_weakref_metadata_shrink(GC *gc)
{
}

static inline void finalizer_weakref_metadata_general_add_entry(Vector_Block* &vector_block_in_use, Pool *pool, Partial_Reveal_Object *ref)
{
  assert(vector_block_in_use);
  assert(ref);

  Vector_Block* block = vector_block_in_use;
  vector_block_add_entry(block, (unsigned int)ref);
  
  if(!vector_block_is_full(block)) return;
  
  pool_put_entry(pool, block);
  vector_block_in_use = finalizer_weakref_get_free_block();
}

void mutator_finalizer_add_entry(Mutator *mutator, Partial_Reveal_Object *ref)
{
  finalizer_weakref_metadata_general_add_entry(mutator->objects_with_finalizer, finalizer_weakref_metadata.objects_with_finalizer_pool, ref);
}

void gc_finalizable_objects_add_entry(GC *gc, Partial_Reveal_Object *ref)
{
  finalizer_weakref_metadata_general_add_entry(finalizer_weakref_metadata.finalizable_objects, finalizer_weakref_metadata.finalizable_objects_pool, ref);
}

void collector_softref_set_add_entry(Collector *collector, Partial_Reveal_Object *ref)
{
  finalizer_weakref_metadata_general_add_entry(collector->softref_set, finalizer_weakref_metadata.softref_set_pool, ref);
}

void collector_weakref_set_add_entry(Collector *collector, Partial_Reveal_Object *ref)
{
  finalizer_weakref_metadata_general_add_entry(collector->weakref_set, finalizer_weakref_metadata.weakref_set_pool, ref);
}

void collector_phanref_set_add_entry(Collector *collector, Partial_Reveal_Object *ref)
{
  finalizer_weakref_metadata_general_add_entry(collector->phanref_set, finalizer_weakref_metadata.phanref_set_pool, ref);
}

void finalizer_weakref_repset_add_entry(GC *gc, Partial_Reveal_Object **p_ref)
{
  assert(*p_ref);
  finalizer_weakref_metadata_general_add_entry(finalizer_weakref_metadata.repset, finalizer_weakref_metadata.repset_pool, (Partial_Reveal_Object *)p_ref);
}

static inline Boolean pool_has_no_reference(Pool *pool)
{
  if(pool_is_empty(pool))
    return TRUE;
  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      if(*iter)
        return FALSE;
      iter = vector_block_iterator_advance(block, iter);
    }
  }
  return TRUE;
}

Boolean objects_with_finalizer_pool_is_empty(GC *gc)
{
  return pool_has_no_reference(gc->finalizer_weakref_metadata->objects_with_finalizer_pool);
}

Boolean finalizable_objects_pool_is_empty(GC *gc)
{
  return pool_has_no_reference(gc->finalizer_weakref_metadata->finalizable_objects_pool);
}

Boolean softref_set_pool_is_empty(GC *gc)
{
  return pool_has_no_reference(gc->finalizer_weakref_metadata->softref_set_pool);
}

Boolean weakref_set_pool_is_empty(GC *gc)
{
  return pool_has_no_reference(gc->finalizer_weakref_metadata->weakref_set_pool);
}

Boolean phanref_set_pool_is_empty(GC *gc)
{
  return pool_has_no_reference(gc->finalizer_weakref_metadata->phanref_set_pool);
}

Boolean finalizer_weakref_repset_pool_is_empty(GC *gc)
{
  return pool_has_no_reference(gc->finalizer_weakref_metadata->repset_pool);
}

static inline void finalizer_weakref_metadata_clear_pool(Pool *pool)
{
  while(Vector_Block* block = pool_get_entry(pool))
  {
    vector_block_clear(block);
    pool_put_entry(finalizer_weakref_metadata.free_pool, block);
  }
}

void gc_clear_special_reference_pools(GC *gc)
{
  finalizer_weakref_metadata_clear_pool(gc->finalizer_weakref_metadata->softref_set_pool);
  finalizer_weakref_metadata_clear_pool(gc->finalizer_weakref_metadata->weakref_set_pool);
  finalizer_weakref_metadata_clear_pool(gc->finalizer_weakref_metadata->phanref_set_pool);
}
