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

#define FINREF_METADATA_SEG_SIZE_BIT_SHIFT          20
#define FINREF_METADATA_SEG_SIZE_BYTES                (1 << FINREF_METADATA_SEG_SIZE_BIT_SHIFT)

//#define FINREF_METADATA_BLOCK_SIZE_BYTES    must be equal to   VECTOR_BLOCK_DATA_SIZE_BYTES
#define FINREF_METADATA_BLOCK_SIZE_BIT_SHIFT  11
#define FINREF_METADATA_BLOCK_SIZE_BYTES        (1 << FINREF_METADATA_BLOCK_SIZE_BIT_SHIFT)

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
  unsigned int seg_size =  FINREF_METADATA_SEG_SIZE_BYTES + FINREF_METADATA_BLOCK_SIZE_BYTES;
  void *first_segment = STD_MALLOC(seg_size);
  memset(first_segment, 0, seg_size);
  finref_metadata.segments[0] = first_segment;
  first_segment = (void*)round_up_to_size((POINTER_SIZE_INT)first_segment, FINREF_METADATA_BLOCK_SIZE_BYTES);
  finref_metadata.num_alloc_segs = 1;
  finref_metadata.alloc_lock = FREE_LOCK;
  
  finref_metadata.free_pool = sync_pool_create();
  finref_metadata.obj_with_fin_pool = sync_pool_create();
  finref_metadata.finalizable_obj_pool = sync_pool_create();
  finref_metadata.softref_pool = sync_pool_create();
  finref_metadata.weakref_pool = sync_pool_create();
  finref_metadata.phanref_pool = sync_pool_create();
  finref_metadata.repset_pool = sync_pool_create();
  
  finref_metadata.finalizable_obj_set= NULL;
  finref_metadata.repset = NULL;
  
  unsigned int num_blocks =  FINREF_METADATA_SEG_SIZE_BYTES >> FINREF_METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(unsigned int i=0; i<num_blocks; i++){
    Vector_Block *block = (Vector_Block *)((POINTER_SIZE_INT)first_segment + i*FINREF_METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, FINREF_METADATA_BLOCK_SIZE_BYTES);
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
    assert(metadata->segments[i]);
    STD_FREE(metadata->segments[i]);
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


/* called when there is no Vector_Block in finref_metadata->free_pool
 * extend the pool by a segment
 */
Vector_Block *finref_metadata_extend(void)
{
  Finref_Metadata *metadata = &finref_metadata;
  lock(metadata->alloc_lock);
  Vector_Block* block = pool_get_entry(metadata->free_pool);
  if( block ){
    unlock(metadata->alloc_lock);
    return block;
  }
  
  unsigned int num_alloced = metadata->num_alloc_segs;
  if(num_alloced == FINREF_METADATA_SEGMENT_NUM){
    printf("Run out Finref metadata, please give it more segments!\n");
    exit(0);
  }
  
  unsigned int seg_size =  FINREF_METADATA_SEG_SIZE_BYTES + FINREF_METADATA_BLOCK_SIZE_BYTES;
  void *new_segment = STD_MALLOC(seg_size);
  memset(new_segment, 0, seg_size);
  metadata->segments[num_alloced] = new_segment;
  new_segment = (void*)round_up_to_size((POINTER_SIZE_INT)new_segment, FINREF_METADATA_BLOCK_SIZE_BYTES);
  metadata->num_alloc_segs++;
  
  unsigned int num_blocks =  FINREF_METADATA_SEG_SIZE_BYTES >> FINREF_METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(unsigned int i=0; i<num_blocks; i++){
    Vector_Block *block = (Vector_Block *)((POINTER_SIZE_INT)new_segment + i*FINREF_METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, FINREF_METADATA_BLOCK_SIZE_BYTES);
    assert(vector_block_is_empty((Vector_Block *)block));
    pool_put_entry(metadata->free_pool, (void *)block);
  }
  
  block = pool_get_entry(metadata->free_pool);
  unlock(metadata->alloc_lock);
  return block;
}

/* called when GC completes and there is no Vector_Block in the last five pools of gc->finref_metadata
 * shrink the free pool by half
 */
static void finref_metadata_shrink(GC *gc)
{
}


/* reset obj_with_fin vector block of each mutator */
static void gc_reset_obj_with_fin(GC *gc)
{
  Mutator *mutator = gc->mutator_list;
  while(mutator){
    assert(!mutator->obj_with_fin);
    mutator->obj_with_fin = finref_get_free_block(gc);
    mutator = mutator->next;
  }
}

/* put back last obj_with_fin block of each mutator */
void gc_set_obj_with_fin(GC *gc)
{
  Pool *obj_with_fin_pool = gc->finref_metadata->obj_with_fin_pool;

  Mutator *mutator = gc->mutator_list;
  while(mutator){
    pool_put_entry(obj_with_fin_pool, mutator->obj_with_fin);
    mutator->obj_with_fin = NULL;
    mutator = mutator->next;
  }
}

/* reset weak references vetctor block of each collector */
void collector_reset_weakref_sets(Collector *collector)
{
  GC *gc = collector->gc;
  
  collector->softref_set = finref_get_free_block(gc);
  collector->weakref_set = finref_get_free_block(gc);
  collector->phanref_set= finref_get_free_block(gc);
}

/* put back last weak references block of each collector */
void gc_set_weakref_sets(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  unsigned int num_active_collectors = gc->num_active_collectors;
  for(unsigned int i = 0; i < num_active_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    pool_put_entry(metadata->softref_pool, collector->softref_set);
    pool_put_entry(metadata->weakref_pool, collector->weakref_set);
    pool_put_entry(metadata->phanref_pool, collector->phanref_set);
    collector->softref_set = NULL;
    collector->weakref_set= NULL;
    collector->phanref_set= NULL;
  }
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
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
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
  
  gc_reset_obj_with_fin(gc);
}


static inline void finref_metadata_add_entry(GC *gc, Vector_Block* &vector_block_in_use, Pool *pool, Partial_Reveal_Object *ref)
{
  assert(vector_block_in_use);
  assert(ref);

  Vector_Block* block = vector_block_in_use;
  vector_block_add_entry(block, (POINTER_SIZE_INT)ref);
  
  if(!vector_block_is_full(block)) return;
  
  pool_put_entry(pool, block);
  vector_block_in_use = finref_get_free_block(gc);
}

void mutator_add_finalizer(Mutator *mutator, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(mutator->gc, mutator->obj_with_fin, finref_metadata.obj_with_fin_pool, ref);
}

void gc_add_finalizable_obj(GC *gc, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(gc, finref_metadata.finalizable_obj_set, finref_metadata.finalizable_obj_pool, ref);
}

void collector_add_softref(Collector *collector, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(collector->gc, collector->softref_set, finref_metadata.softref_pool, ref);
}

void collector_add_weakref(Collector *collector, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(collector->gc, collector->weakref_set, finref_metadata.weakref_pool, ref);
}

void collector_add_phanref(Collector *collector, Partial_Reveal_Object *ref)
{
  finref_metadata_add_entry(collector->gc, collector->phanref_set, finref_metadata.phanref_pool, ref);
}

void finref_repset_add_entry(GC *gc, Partial_Reveal_Object **p_ref)
{
  assert(*p_ref);
  finref_metadata_add_entry(gc, finref_metadata.repset, finref_metadata.repset_pool, (Partial_Reveal_Object *)p_ref);
}

static inline Boolean pool_has_no_ref(Pool *pool)
{
  if(pool_is_empty(pool))
    return TRUE;
  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
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
