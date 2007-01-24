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

#include "open/types.h"
#include "open/vm_gc.h"
#include "finalizer_weakref.h"
#include "../thread/mutator.h"
#include "../common/gc_metadata.h"
#include "../trace_forward/fspace.h"
#include "../mark_sweep/lspace.h"
#include "../gen/gen.h"

Boolean IGNORE_FINREF = TRUE;

/* reset obj_with_fin vector block of each mutator */
void mutator_reset_obj_with_fin(Mutator *mutator)
{
  mutator->obj_with_fin = finref_get_free_block();
}

void gc_set_obj_with_fin(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *obj_with_fin_pool = metadata->obj_with_fin_pool;

  /* put back last obj_with_fin block of each mutator */
  Mutator *mutator = gc->mutator_list;
  while(mutator){
    pool_put_entry(obj_with_fin_pool, mutator->obj_with_fin);
    mutator->obj_with_fin = NULL;
    mutator = mutator->next;
  }
  return;
}

/* reset weak references vetctor block of each collector */
void collector_reset_weakref_sets(Collector *collector)
{
  collector->softref_set = finref_get_free_block();
  collector->weakref_set = finref_get_free_block();
  collector->phanref_set= finref_get_free_block();
}

void gc_set_weakref_sets(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  /* put back last weak references block of each collector */
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


extern Boolean obj_is_dead_in_minor_forward_gc(Collector *collector, Partial_Reveal_Object *p_obj);
static inline Boolean obj_is_dead_in_minor_copy_gc(Collector *collector, Partial_Reveal_Object *p_obj)
{
  return !obj_is_marked_in_vt(p_obj);
}
static inline Boolean obj_is_dead_in_major_gc(Collector *collector, Partial_Reveal_Object *p_obj)
{
  return !obj_is_marked_in_vt(p_obj);
}
// clear the two least significant bits of p_obj first
static inline Boolean obj_is_dead(Collector *collector, Partial_Reveal_Object *p_obj)
{
  GC *gc = collector->gc;
  
  assert(p_obj);
  if(gc->collect_kind == MINOR_COLLECTION){
    if( gc_is_gen_mode())
      return obj_is_dead_in_minor_forward_gc(collector, p_obj);
    else
      return obj_is_dead_in_minor_copy_gc(collector, p_obj);
  } else {
    return obj_is_dead_in_major_gc(collector, p_obj);
  }
}


static inline Boolean fspace_obj_to_be_forwarded(Partial_Reveal_Object *p_obj, Space *space)
{
  if(!obj_belongs_to_space(p_obj, (Space*)space)) return FALSE;
  return forward_first_half? (p_obj < object_forwarding_boundary):(p_obj>=object_forwarding_boundary);
}
static inline Boolean obj_need_move(Collector *collector, Partial_Reveal_Object *p_obj)
{
  assert(!obj_is_dead(collector, p_obj));
  GC *gc = collector->gc;
  
  if(gc_is_gen_mode() && gc->collect_kind == MINOR_COLLECTION)
    return fspace_obj_to_be_forwarded(p_obj, collector->collect_space);
  
  Space *space = space_of_addr(gc, p_obj);
  return space->move_object;
}


extern void resurrect_obj_tree_after_trace(Collector *collector, Partial_Reveal_Object **p_ref);
extern void resurrect_obj_tree_after_mark(Collector *collector, Partial_Reveal_Object *p_obj);
static inline void resurrect_obj_tree_in_minor_copy_gc(Collector *collector, Partial_Reveal_Object *p_obj)
{
  resurrect_obj_tree_after_mark(collector, p_obj);
}
static inline void resurrect_obj_tree_in_major_gc(Collector *collector, Partial_Reveal_Object *p_obj)
{
  resurrect_obj_tree_after_mark(collector, p_obj);
}
// clear the two least significant bits of p_obj first
// add p_ref to repset
static inline void resurrect_obj_tree(Collector *collector, Partial_Reveal_Object **p_ref)
{
  GC *gc = collector->gc;
  
  if(!gc_is_gen_mode() || !(gc->collect_kind == MINOR_COLLECTION))
    collector_repset_add_entry(collector, p_ref);
  if(!obj_is_dead(collector, *p_ref)){
    if(gc_is_gen_mode() && gc->collect_kind == MINOR_COLLECTION && obj_need_move(collector, *p_ref))
      *p_ref = obj_get_fw_in_oi(*p_ref);
    return;
  }
  Partial_Reveal_Object* p_obj = *p_ref;
  assert(p_obj);
  
  if(gc->collect_kind == MINOR_COLLECTION){
    if( gc_is_gen_mode())
      resurrect_obj_tree_after_trace(collector, p_ref);
    else
      resurrect_obj_tree_in_minor_copy_gc(collector, p_obj);
  } else {
    resurrect_obj_tree_in_major_gc(collector, p_obj);
  }
}


/* called before loop of resurrect_obj_tree() */
static inline void collector_reset_repset(Collector *collector)
{
  GC *gc = collector->gc;
  
  assert(!collector->rep_set);
  if(gc_is_gen_mode() && gc->collect_kind == MINOR_COLLECTION)
    return;
  collector->rep_set = free_set_pool_get_entry(gc->metadata);
}
/* called after loop of resurrect_obj_tree() */
static inline void collector_put_repset(Collector *collector)
{
  GC *gc = collector->gc;
  
  if(gc_is_gen_mode() && gc->collect_kind == MINOR_COLLECTION)
    return;
  pool_put_entry(gc->metadata->collector_repset_pool, collector->rep_set);
  collector->rep_set = NULL;
}


static void finref_add_repset_from_pool(Collector *collector, Pool *pool)
{
  GC *gc = collector->gc;
  
  finref_reset_repset(gc);

  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    
    while(!vector_block_iterator_end(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      iter = vector_block_iterator_advance(block, iter);
	  
      if(*p_ref && obj_need_move(collector, *p_ref))
        finref_repset_add_entry(gc, p_ref);
    }
  }
  finref_put_repset(gc);
}


static void identify_finalizable_objects(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *obj_with_fin_pool = metadata->obj_with_fin_pool;
  Pool *finalizable_obj_pool = metadata->finalizable_obj_pool;
  
  gc_reset_finalizable_objects(gc);
  pool_iterator_init(obj_with_fin_pool);
  while(Vector_Block *block = pool_iterator_next(obj_with_fin_pool)){
    unsigned int block_has_ref = 0;
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      if(!p_obj)
        continue;
      if(obj_is_dead(collector, p_obj)){
        gc_add_finalizable_obj(gc, p_obj);
        *p_ref = NULL;
      } else {
        ++block_has_ref;
      }
    }
    if(!block_has_ref)
      vector_block_clear(block);
  }
  gc_put_finalizable_objects(gc);
  
  if(!finalizable_obj_pool_is_empty(gc)){
    collector_reset_repset(collector);
    pool_iterator_init(finalizable_obj_pool);
    while(Vector_Block *block = pool_iterator_next(finalizable_obj_pool)){
      unsigned int *iter = vector_block_iterator_init(block);
      while(!vector_block_iterator_end(block, iter)){
        assert(*iter);
        resurrect_obj_tree(collector, (Partial_Reveal_Object **)iter);
        iter = vector_block_iterator_advance(block, iter);
      }
    }
    metadata->pending_finalizers = TRUE;
    collector_put_repset(collector);
  }
  
  finref_add_repset_from_pool(collector, obj_with_fin_pool);
  /* fianlizable objects have been added to collector repset pool */
  //finref_add_repset_from_pool(collector, finalizable_obj_pool);
}

static void put_finalizable_obj_to_vm(GC *gc)
{
  Pool *finalizable_obj_pool = gc->finref_metadata->finalizable_obj_pool;
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(finalizable_obj_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      assert(*iter);
      Managed_Object_Handle p_obj = (Managed_Object_Handle)*iter;
      vm_finalize_object(p_obj);
      iter = vector_block_iterator_advance(block, iter);
    }
    vector_block_clear(block);
    pool_put_entry(free_pool, block);
  }
}

static void update_referent_ignore_finref(Collector *collector, Pool *pool)
{
  GC *gc = collector->gc;
  
  while(Vector_Block *block = pool_get_entry(pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finref_repset_add_entry(gc, p_referent_field);
        *p_ref = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is softly reachable: clear the referent field */
    }
  }
}

void update_ref_ignore_finref(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  
  finref_reset_repset(gc);
  update_referent_ignore_finref(collector, metadata->softref_pool);
  update_referent_ignore_finref(collector, metadata->weakref_pool);
  update_referent_ignore_finref(collector, metadata->phanref_pool);
  finref_put_repset(gc);
}

static void identify_dead_softrefs(Collector *collector)
{
  GC *gc = collector->gc;
  if(gc->collect_kind == MINOR_COLLECTION){
    assert(softref_pool_is_empty(gc));
    return;
  }
  
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *softref_pool = metadata->softref_pool;
  
  finref_reset_repset(gc);
  pool_iterator_init(softref_pool);
  while(Vector_Block *block = pool_iterator_next(softref_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finref_repset_add_entry(gc, p_referent_field);
        *p_ref = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is softly reachable: clear the referent field */
    }
  }
  finref_put_repset(gc);
  
  finref_add_repset_from_pool(collector, softref_pool);
  return;
}

static void identify_dead_weakrefs(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *weakref_pool = metadata->weakref_pool;
  
  finref_reset_repset(gc);
  pool_iterator_init(weakref_pool);
  while(Vector_Block *block = pool_iterator_next(weakref_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finref_repset_add_entry(gc, p_referent_field);
        *p_ref = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is weakly reachable: clear the referent field */
    }
  }
  finref_put_repset(gc);
  
  finref_add_repset_from_pool(collector, weakref_pool);
  return;
}

static void identify_dead_phanrefs(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *phanref_pool = metadata->phanref_pool;
  
  finref_reset_repset(gc);
//  collector_reset_repset(collector);
  pool_iterator_init(phanref_pool);
  while(Vector_Block *block = pool_iterator_next(phanref_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finref_repset_add_entry(gc, p_referent_field);
        *p_ref = NULL;
        continue;
      }
      *p_referent_field = NULL;
      /* Phantom status: for future use
       * if((unsigned int)p_referent & PHANTOM_REF_ENQUEUE_STATUS_MASK){
       *   // enqueued but not explicitly cleared OR pending for enqueuing
       *   *iter = NULL;
       * }
       * resurrect_obj_tree(collector, p_referent_field);
       */
    }
  }
//  collector_put_repset(collector);
  finref_put_repset(gc);
  
  finref_add_repset_from_pool(collector, phanref_pool);
  return;
}

static inline void put_dead_refs_to_vm(GC *gc, Pool *reference_pool)
{
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(reference_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      Managed_Object_Handle p_obj = (Managed_Object_Handle)*iter;
      if(p_obj)
        vm_enqueue_reference(p_obj);
      iter = vector_block_iterator_advance(block, iter);
    }
    vector_block_clear(block);
    pool_put_entry(free_pool, block);
  }
}

static void put_dead_weak_refs_to_vm(GC *gc)
{
  if(softref_pool_is_empty(gc)
      && weakref_pool_is_empty(gc)
      && phanref_pool_is_empty(gc)){
    gc_clear_weakref_pools(gc);
    return;
  }
  
  gc->finref_metadata->pending_weakrefs = TRUE;
  
  Pool *softref_pool = gc->finref_metadata->softref_pool;
  Pool *weakref_pool = gc->finref_metadata->weakref_pool;
  Pool *phanref_pool = gc->finref_metadata->phanref_pool;
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  put_dead_refs_to_vm(gc, softref_pool);
  put_dead_refs_to_vm(gc, weakref_pool);
  put_dead_refs_to_vm(gc, phanref_pool);
}

void collector_identify_finref(Collector *collector)
{
  GC *gc = collector->gc;
  
  gc_set_weakref_sets(gc);
  identify_dead_softrefs(collector);
  identify_dead_weakrefs(collector);
  identify_finalizable_objects(collector);
  identify_dead_phanrefs(collector);
}

void gc_put_finref_to_vm(GC *gc)
{
  put_dead_weak_refs_to_vm(gc);
  put_finalizable_obj_to_vm(gc);
}

void put_all_fin_on_exit(GC *gc)
{
  Pool *obj_with_fin_pool = gc->finref_metadata->obj_with_fin_pool;
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  vm_gc_lock_enum();
  /* FIXME: holding gc lock is not enough, perhaps there are mutators that are allocating objects with finalizer
   * could be fixed as this:
   * in fspace_alloc() and lspace_alloc() hold gc lock through
   * allocating mem and adding the objects with finalizer to the pool
   */
  lock(gc->mutator_list_lock);
  gc_set_obj_with_fin(gc);
  unlock(gc->mutator_list_lock);
  while(Vector_Block *block = pool_get_entry(obj_with_fin_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      Managed_Object_Handle p_obj = (Managed_Object_Handle)*iter;
      if(p_obj)
        vm_finalize_object(p_obj);
      iter = vector_block_iterator_advance(block, iter);
    }
    vector_block_clear(block);
    pool_put_entry(free_pool, block);
  }
  vm_gc_unlock_enum();
}

void gc_update_finref_repointed_refs(GC* gc)
{
  Finref_Metadata* metadata = gc->finref_metadata;
  Pool *repset_pool = metadata->repset_pool;
  
  /* NOTE:: this is destructive to the root sets. */
  Vector_Block* root_set = pool_get_entry(repset_pool);

  while(root_set){
    unsigned int* iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object** )*iter;
      iter = vector_block_iterator_advance(root_set,iter);

      Partial_Reveal_Object* p_obj = *p_ref;
      /* For repset, this check is unnecessary, since all slots are repointed; otherwise
         they will not be recorded. For root set, it is possible to point to LOS or other
         non-moved space.  */
      Partial_Reveal_Object* p_target_obj;
      assert(obj_is_fw_in_oi(p_obj));
      p_target_obj = obj_get_fw_in_oi(p_obj);

      *p_ref = p_target_obj;
    }
    vector_block_clear(root_set);
    pool_put_entry(metadata->free_pool, root_set);
    root_set = pool_get_entry(repset_pool);
  } 
  
  return;
}

void gc_activate_finref_threads(GC *gc)
{
  Finref_Metadata* metadata = gc->finref_metadata;
  
  if(metadata->pending_finalizers || metadata->pending_weakrefs){
	  metadata->pending_finalizers = FALSE;
    metadata->pending_weakrefs = FALSE;
    vm_hint_finalize();
  }
}
