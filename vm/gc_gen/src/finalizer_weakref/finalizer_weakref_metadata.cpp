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

static Finref_Metadata finref_metadata;

unsigned int get_gc_referent_offset(void)
{
  return finref_metadata.gc_referent_offset;
}
void set_gc_referent_offset(unsigned int offset)
{
  finref_metadata.gc_referent_offset = offset;
}

void gc_finref_metadata_initialize(GC *gc)
{
  void *pool_segment = STD_MALLOC(POOL_SEGMENT_SIZE_BYTES);
  memset(pool_segment, 0, POOL_SEGMENT_SIZE_BYTES);
  finref_metadata.num_alloc_segs = 0;
  finref_metadata.pool_segments[finref_metadata.num_alloc_segs] = pool_segment;
  ++finref_metadata.num_alloc_segs;
  
  finref_metadata.free_pool = sync_pool_create();
  finref_metadata.obj_with_fin_pool = sync_pool_create();
  finref_metadata.finalizable_obj_pool = sync_pool_create();
  finref_metadata.softref_pool = sync_pool_create();
  finref_metadata.weakref_pool = sync_pool_create();
  finref_metadata.phanref_pool = sync_pool_create();
  finref_metadata.repset_pool = sync_pool_create();
  
  finref_metadata.finalizable_obj_set= NULL;
  finref_metadata.repset = NULL;
  
  unsigned int num_blocks =  POOL_SEGMENT_SIZE_BYTES >> METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(unsigned int i=0; i<num_blocks; i++){
    Vector_Block *block = (Vector_Block *)((unsigned int)pool_segment + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
    assert(vector_block_is_empty((Vector_Block *)block));
    pool_put_entry(finref_metadata.free_pool, (void *)block);
  }
  
  finref_metadata.pending_finalizers = FALSE;
  finref_metadata.pending_weakrefs = FALSE;
  finref_metadata.gc_referent_offset = 0;
  
  gc->finref_metadata = &finref_metadata;
  return;
}

void gc_finref_metadata_destruct(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  sync_pool_destruct(metadata->free_pool);
  sync_pool_destruct(metadata->obj_with_fin_pool);
  sync_pool_destruct(metadata->finalizable_obj_pool);
  sync_pool_destruct(metadata->softref_pool);
  sync_pool_destruct(metadata->weakref_pool);
  sync_pool_destruct(metadata->phanref_pool);
  sync_pool_destruct(metadata->repset_pool);
  
  metadata->finalizable_obj_set = NULL;
  metadata->repset = NULL;
  
  for(unsigned int i=0; i<metadata->num_alloc_segs; i++){
    assert(metadata->pool_segments[i]);
    STD_FREE(metadata->pool_segments[i]);
  }
  
  gc->finref_metadata = NULL;
}

void gc_finref_metadata_verify(GC *gc, Boolean is_before_gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  assert(pool_is_empty(metadata->finalizable_obj_pool));
  assert(pool_is_empty(metadata->softref_pool));
  assert(pool_is_empty(metadata->weakref_pool));
  assert(pool_is_empty(metadata->phanref_pool));
  assert(pool_is_empty(metadata->repset_pool));
  assert(metadata->finalizable_obj_set == NULL);
  assert(metadata->repset == NULL);
  
  return;
}

void gc_reset_finref_metadata(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *obj_with_fin_pool = metadata->obj_with_fin_pool;
  Pool *finalizable_obj_pool = metadata->finalizable_obj_pool;
  
  assert(pool_is_empty(finalizable_obj_pool));
  assert(pool_is_empty(metadata->softref_pool));
  assert(pool_is_empty(metadata->weakref_pool));
  assert(pool_is_empty(metadata->phanref_pool));
  assert(pool_is_empty(metadata->repset_pool));
  assert(metadata->finalizable_obj_set == NULL);
  assert(metadata->repset == NULL);
  
  while(Vector_Block *block = pool_get_entry(obj_with_fin_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    if(vector_block_iterator_end(block, iter)){
      vector_block_clear(block);
      pool_put_entry(metadata->free_pool, block);
    } else {
      pool_put_entry(finalizable_obj_pool, block);
    }
  }
  assert(pool_is_empty(obj_with_fin_pool));
  metadata->obj_with_fin_pool = finalizable_obj_pool;
  metadata->finalizable_obj_pool = obj_with_fin_pool;
}

/* called when there is no Vector_Block in finref_metadata->free_pool
 * extend the pool by a pool segment
 */
static void finref_metadata_extend(void)
{
  Finref_Metadata *metadata = &finref_metadata;
  
  unsigned int pos = metadata->num_alloc_segs;
  while(pos < POOL_SEGMENT_NUM){
    unsigned int next_pos = pos + 1;
    unsigned int temp = (unsigned int)atomic_cas32((volatile unsigned int *)&metadata->num_alloc_segs, next_pos, pos);
    if(temp == pos)
      break;
    pos = metadata->num_alloc_segs;
  }
  if(pos > POOL_SEGMENT_NUM)
    return;
  
  void *pool_segment = STD_MALLOC(POOL_SEGMENT_SIZE_BYTES);
  memset(pool_segment, 0, POOL_SEGMENT_SIZE_BYTES);
  metadata->pool_segments[pos] = pool_segment;
  
  unsigned int num_blocks =  POOL_SEGMENT_SIZE_BYTES >> METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(unsigned int i=0; i<num_blocks; i++){
    Vector_Block *block = (Vector_Block *)((unsigned int)pool_segment + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
    assert(vector_block_is_empty((Vector_Block *)block));
    pool_put_entry(metadata->free_pool, (void *)block);
  }
  
  return;
}

Vector_Block *finref_get_free_block(void)
{
  Vector_Block *block;
  
  while(!(block = pool_get_entry(finref_metadata.free_pool)))
    finref_metadata_extend();
  return block;
}

/* called when GC completes and there is no Vector_Block in the last five pools of gc->finref_metadata
 * shrink the free pool by half
 */
void finref_metadata_shrink(GC *gc)
{
}

static inline void finref_metadata_add_entry(Vector_Block* &vector_block_in_use, Pool *pool, Partial_Reveal_Object *ref)
{
  assert(vector_block_in_use);
  assert(ref);

  Vector_Block* block = vector_block_in_use;
  vector_block_add_entry(block, (unsigned int)ref);
  
  if(!vector_block_is_full(block)) return;
  
  pool_put_entry(pool, block);
  vector_block_in_use = finref_get_free_block();
}

void mutator_add_finalizer(Mutator *mutator, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(mutator->obj_with_fin, finref_metadata.obj_with_fin_pool, ref);
}

void gc_add_finalizable_obj(GC *gc, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(finref_metadata.finalizable_obj_set, finref_metadata.finalizable_obj_pool, ref);
}

void collector_add_softref(Collector *collector, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(collector->softref_set, finref_metadata.softref_pool, ref);
}

void collector_add_weakref(Collector *collector, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(collector->weakref_set, finref_metadata.weakref_pool, ref);
}

void collector_add_phanref(Collector *collector, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(collector->phanref_set, finref_metadata.phanref_pool, ref);
}

void finref_repset_add_entry(GC *gc, Partial_Reveal_Object **p_ref)
{
  assert(*p_ref);
  finref_metadata_add_entry(finref_metadata.repset, finref_metadata.repset_pool, (Partial_Reveal_Object *)p_ref);
}

static inline Boolean pool_has_no_ref(Pool *pool)
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

Boolean obj_with_fin_pool_is_empty(GC *gc)
{
  return pool_has_no_ref(gc->finref_metadata->obj_with_fin_pool);
}

Boolean finalizable_obj_pool_is_empty(GC *gc)
{
  return pool_has_no_ref(gc->finref_metadata->finalizable_obj_pool);
}

Boolean softref_pool_is_empty(GC *gc)
{
  return pool_has_no_ref(gc->finref_metadata->softref_pool);
}

Boolean weakref_pool_is_empty(GC *gc)
{
  return pool_has_no_ref(gc->finref_metadata->weakref_pool);
}

Boolean phanref_pool_is_empty(GC *gc)
{
  return pool_has_no_ref(gc->finref_metadata->phanref_pool);
}

Boolean finref_repset_pool_is_empty(GC *gc)
{
  return pool_has_no_ref(gc->finref_metadata->repset_pool);
}

static inline void finref_metadata_clear_pool(Pool *pool)
{
  while(Vector_Block* block = pool_get_entry(pool))
  {
    vector_block_clear(block);
    pool_put_entry(finref_metadata.free_pool, block);
  }
}

void gc_clear_weakref_pools(GC *gc)
{
  finref_metadata_clear_pool(gc->finref_metadata->softref_pool);
  finref_metadata_clear_pool(gc->finref_metadata->weakref_pool);
  finref_metadata_clear_pool(gc->finref_metadata->phanref_pool);
}
