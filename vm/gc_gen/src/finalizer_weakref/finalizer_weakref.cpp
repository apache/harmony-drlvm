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

/* reset objects_with_finalizer vector block of each mutator */
void mutator_reset_objects_with_finalizer(Mutator *mutator)
{
  mutator->objects_with_finalizer = finalizer_weakref_get_free_block();
}

void gc_set_objects_with_finalizer(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  Pool *objects_with_finalizer_pool = metadata->objects_with_finalizer_pool;

  /* put back last objects_with_finalizer block of each mutator */
  Mutator *mutator = gc->mutator_list;
  while(mutator){
    pool_put_entry(objects_with_finalizer_pool, mutator->objects_with_finalizer);
    mutator->objects_with_finalizer = NULL;
    mutator = mutator->next;
  }
  return;
}

/* reset weak references vetctor block of each collector */
void collector_reset_weakref_sets(Collector *collector)
{
  collector->softref_set = finalizer_weakref_get_free_block();
  collector->weakref_set = finalizer_weakref_get_free_block();
  collector->phanref_set= finalizer_weakref_get_free_block();
}

static void gc_set_weakref_sets(GC *gc)
{
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  
  /* put back last weak references block of each collector */
  unsigned int num_active_collectors = gc->num_active_collectors;
  for(unsigned int i = 0; i < num_active_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    pool_put_entry(metadata->softref_set_pool, collector->softref_set);
    pool_put_entry(metadata->weakref_set_pool, collector->weakref_set);
    pool_put_entry(metadata->phanref_set_pool, collector->phanref_set);
    collector->softref_set = NULL;
    collector->weakref_set= NULL;
    collector->phanref_set= NULL;
  }
  return;
}


extern Boolean obj_is_dead_in_minor_forward_collection(Collector *collector, Partial_Reveal_Object *p_obj);
static inline Boolean obj_is_dead_in_minor_copy_collection(Collector *collector, Partial_Reveal_Object *p_obj)
{
  GC *gc = collector->gc;
  Lspace *los = ((GC_Gen *)gc)->los;
  
  if(space_of_addr(gc, p_obj) != (Space *)los)
    return !obj_is_marked_in_vt(p_obj);
  else
    return !lspace_object_is_marked(los, p_obj);
}
static inline Boolean obj_is_dead_in_major_collection(Collector *collector, Partial_Reveal_Object *p_obj)
{
  GC *gc = collector->gc;
  Lspace *los = ((GC_Gen *)gc)->los;
  
  if(space_of_addr(gc, p_obj) != (Space *)los)
    return !obj_is_marked_in_vt(p_obj);
  else
    return !lspace_object_is_marked(los, p_obj);
}
// clear the two least significant bits of p_obj first
static inline Boolean obj_is_dead(Collector *collector, Partial_Reveal_Object *p_obj)
{
  GC *gc = collector->gc;
  
  assert(p_obj);
  if(gc->collect_kind == MINOR_COLLECTION){
    if( gc_requires_barriers())
      return obj_is_dead_in_minor_forward_collection(collector, p_obj);
    else
      return obj_is_dead_in_minor_copy_collection(collector, p_obj);
  } else {
    return obj_is_dead_in_major_collection(collector, p_obj);
  }
}


static inline Boolean fspace_object_to_be_forwarded(Partial_Reveal_Object *p_obj, Space *space)
{
  if(!obj_belongs_to_space(p_obj, (Space*)space)) return FALSE;
  return forward_first_half? (p_obj < object_forwarding_boundary):(p_obj>=object_forwarding_boundary);
}
static inline Boolean obj_need_move(Collector *collector, Partial_Reveal_Object *p_obj)
{
  assert(!obj_is_dead(collector, p_obj));
  GC *gc = collector->gc;
  
  if(gc_requires_barriers() && gc->collect_kind == MINOR_COLLECTION)
    return fspace_object_to_be_forwarded(p_obj, collector->collect_space);
  
  Space *space = space_of_addr(gc, p_obj);
  return space->move_object;
}


extern void resurrect_obj_tree_after_trace(Collector *collector, Partial_Reveal_Object **p_ref);
extern void resurrect_obj_tree_after_mark(Collector *collector, Partial_Reveal_Object *p_obj);
static inline void resurrect_obj_tree_in_minor_copy_collection(Collector *collector, Partial_Reveal_Object *p_obj)
{
  resurrect_obj_tree_after_mark(collector, p_obj);
}
static inline void resurrect_obj_tree_in_major_collection(Collector *collector, Partial_Reveal_Object *p_obj)
{
  resurrect_obj_tree_after_mark(collector, p_obj);
}
// clear the two least significant bits of p_obj first
// add p_ref to repset
static inline void resurrect_obj_tree(Collector *collector, Partial_Reveal_Object **p_ref)
{
  GC *gc = collector->gc;
  
  if(!gc_requires_barriers() || !(gc->collect_kind == MINOR_COLLECTION))
    collector_repset_add_entry(collector, p_ref);
  if(!obj_is_dead(collector, *p_ref)){
    if(gc_requires_barriers() && gc->collect_kind == MINOR_COLLECTION && obj_need_move(collector, *p_ref))
      *p_ref = obj_get_forwarding_pointer_in_vt(*p_ref);
    return;
  }
  Partial_Reveal_Object* p_obj = *p_ref;
  assert(p_obj);
  
  if(gc->collect_kind == MINOR_COLLECTION){
    if( gc_requires_barriers())
      resurrect_obj_tree_after_trace(collector, p_ref);
    else
      resurrect_obj_tree_in_minor_copy_collection(collector, p_obj);
  } else {
    resurrect_obj_tree_in_major_collection(collector, p_obj);
  }
}


/* called before loop of resurrect_obj_tree() */
static inline void collector_reset_repset(Collector *collector)
{
  GC *gc = collector->gc;
  
  assert(!collector->rep_set);
  if(gc_requires_barriers() && gc->collect_kind == MINOR_COLLECTION)
    return;
  collector->rep_set = pool_get_entry(gc->metadata->free_set_pool);
}
/* called after loop of resurrect_obj_tree() */
static inline void collector_put_repset(Collector *collector)
{
  GC *gc = collector->gc;
  
  if(gc_requires_barriers() && gc->collect_kind == MINOR_COLLECTION)
    return;
  pool_put_entry(gc->metadata->collector_repset_pool, collector->rep_set);
  collector->rep_set = NULL;
}


void finalizer_weakref_repset_add_entry_from_pool(Collector *collector, Pool *pool)
{
  GC *gc = collector->gc;
  
  finalizer_weakref_reset_repset(gc);

  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    
    while(!vector_block_iterator_end(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      iter = vector_block_iterator_advance(block, iter);
	  
      if(*p_ref && obj_need_move(collector, *p_ref))
        finalizer_weakref_repset_add_entry(gc, p_ref);
    }
  }
  finalizer_weakref_put_repset(gc);
}


static void process_objects_with_finalizer(Collector *collector)
{
  GC *gc = collector->gc;
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  Pool *objects_with_finalizer_pool = metadata->objects_with_finalizer_pool;
  Pool *finalizable_objects_pool = metadata->finalizable_objects_pool;
  
  gc_reset_finalizable_objects(gc);
  pool_iterator_init(objects_with_finalizer_pool);
  while(Vector_Block *block = pool_iterator_next(objects_with_finalizer_pool)){
    unsigned int block_has_ref = 0;
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      if(!p_obj)
        continue;
      if(obj_is_dead(collector, p_obj)){
        gc_finalizable_objects_add_entry(gc, p_obj);
        *iter = NULL;
      } else {
        ++block_has_ref;
      }
    }
    if(!block_has_ref)
      vector_block_clear(block);
  }
  gc_put_finalizable_objects(gc);
  
  collector_reset_repset(collector);
  if(!finalizable_objects_pool_is_empty(gc)){
    pool_iterator_init(finalizable_objects_pool);
    while(Vector_Block *block = pool_iterator_next(finalizable_objects_pool)){
      unsigned int *iter = vector_block_iterator_init(block);
      while(!vector_block_iterator_end(block, iter)){
        assert(*iter);
        resurrect_obj_tree(collector, (Partial_Reveal_Object **)iter);
        iter = vector_block_iterator_advance(block, iter);
      }
    }
    metadata->pending_finalizers = TRUE;
  }
  collector_put_repset(collector);
  
  finalizer_weakref_repset_add_entry_from_pool(collector, objects_with_finalizer_pool);
  /* fianlizable objects have been added to collector repset pool */
  //finalizer_weakref_repset_add_entry_from_pool(collector, finalizable_objects_pool);
}

static void post_process_finalizable_objects(GC *gc)
{
  Pool *finalizable_objects_pool = gc->finalizer_weakref_metadata->finalizable_objects_pool;
  Pool *free_pool = gc->finalizer_weakref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(finalizable_objects_pool)){
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

static void process_soft_references(Collector *collector)
{
  GC *gc = collector->gc;
  if(gc->collect_kind == MINOR_COLLECTION){
    assert(softref_set_pool_is_empty(gc));
    return;
  }
  
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  Pool *softref_set_pool = metadata->softref_set_pool;
  
  finalizer_weakref_reset_repset(gc);
  pool_iterator_init(softref_set_pool);
  while(Vector_Block *block = pool_iterator_next(softref_set_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *iter = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finalizer_weakref_repset_add_entry(gc, p_referent_field);
        *iter = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is softly reachable: clear the referent field */
    }
  }
  finalizer_weakref_put_repset(gc);
  
  finalizer_weakref_repset_add_entry_from_pool(collector, softref_set_pool);
  return;
}

static void process_weak_references(Collector *collector)
{
  GC *gc = collector->gc;
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  Pool *weakref_set_pool = metadata->weakref_set_pool;
  
  finalizer_weakref_reset_repset(gc);
  pool_iterator_init(weakref_set_pool);
  while(Vector_Block *block = pool_iterator_next(weakref_set_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *iter = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finalizer_weakref_repset_add_entry(gc, p_referent_field);
        *iter = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is weakly reachable: clear the referent field */
    }
  }
  finalizer_weakref_put_repset(gc);
  
  finalizer_weakref_repset_add_entry_from_pool(collector, weakref_set_pool);
  return;
}

static void process_phantom_references(Collector *collector)
{
  GC *gc = collector->gc;
  Finalizer_Weakref_Metadata *metadata = gc->finalizer_weakref_metadata;
  Pool *phanref_set_pool = metadata->phanref_set_pool;
  
  finalizer_weakref_reset_repset(gc);
//  collector_reset_repset(collector);
  pool_iterator_init(phanref_set_pool);
  while(Vector_Block *block = pool_iterator_next(phanref_set_pool)){
    unsigned int *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)*iter;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *iter = NULL;
        continue;
      }
      if(!obj_is_dead(collector, p_referent)){  // referent is alive
        if(obj_need_move(collector, p_referent))
          finalizer_weakref_repset_add_entry(gc, p_referent_field);
        *iter = NULL;
        continue;
      }
      *p_referent_field = NULL;
      /* Phantom status: for future use
       * if((unsigned int)p_referent & PHANTOM_REF_ENQUEUE_STATUS_MASK){
       *   // enqueued but not explicitly cleared OR pending for enqueueing
       *   *iter = NULL;
       * }
       * resurrect_obj_tree(collector, p_referent_field);
       */
    }
  }
//  collector_put_repset(collector);
  finalizer_weakref_put_repset(gc);
  
  finalizer_weakref_repset_add_entry_from_pool(collector, phanref_set_pool);
  return;
}

static inline void post_process_special_reference_pool(GC *gc, Pool *reference_pool)
{
  Pool *free_pool = gc->finalizer_weakref_metadata->free_pool;
  
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

static void post_process_special_references(GC *gc)
{
  if(softref_set_pool_is_empty(gc)
      && weakref_set_pool_is_empty(gc)
      && phanref_set_pool_is_empty(gc)){
    gc_clear_special_reference_pools(gc);
    return;
  }
  
  gc->finalizer_weakref_metadata->pending_weak_references = TRUE;
  
  Pool *softref_set_pool = gc->finalizer_weakref_metadata->softref_set_pool;
  Pool *weakref_set_pool = gc->finalizer_weakref_metadata->weakref_set_pool;
  Pool *phanref_set_pool = gc->finalizer_weakref_metadata->phanref_set_pool;
  Pool *free_pool = gc->finalizer_weakref_metadata->free_pool;
  
  post_process_special_reference_pool(gc, softref_set_pool);
  post_process_special_reference_pool(gc, weakref_set_pool);
  post_process_special_reference_pool(gc, phanref_set_pool);
}

void collector_process_finalizer_weakref(Collector *collector)
{
  GC *gc = collector->gc;
  
  gc_set_weakref_sets(gc);
  process_soft_references(collector);
  process_weak_references(collector);
  process_objects_with_finalizer(collector);
  process_phantom_references(collector);
}

void gc_post_process_finalizer_weakref(GC *gc)
{
  post_process_special_references(gc);
  post_process_finalizable_objects(gc);
}

void process_objects_with_finalizer_on_exit(GC *gc)
{
  Pool *objects_with_finalizer_pool = gc->finalizer_weakref_metadata->objects_with_finalizer_pool;
  Pool *free_pool = gc->finalizer_weakref_metadata->free_pool;
  
  vm_gc_lock_enum();
  /* FIXME: holding gc lock is not enough, perhaps there are mutators that are allocating objects with finalizer
   * could be fixed as this:
   * in fspace_alloc() and lspace_alloc() hold gc lock through
   * allocating mem and adding the objects with finalizer to the pool
   */
  lock(gc->mutator_list_lock);
  gc_set_objects_with_finalizer(gc);
  unlock(gc->mutator_list_lock);
  while(Vector_Block *block = pool_get_entry(objects_with_finalizer_pool)){
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

void gc_update_finalizer_weakref_repointed_refs(GC* gc)
{
  Finalizer_Weakref_Metadata* metadata = gc->finalizer_weakref_metadata;
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
#ifdef _DEBUG
      if( !gc_requires_barriers() || gc->collect_kind == MAJOR_COLLECTION ){
        assert(obj_is_forwarded_in_obj_info(p_obj));
      } else
        assert(obj_is_forwarded_in_vt(p_obj));
#endif
      Partial_Reveal_Object* p_target_obj;
      if( !gc_requires_barriers() || gc->collect_kind == MAJOR_COLLECTION )
        p_target_obj = get_forwarding_pointer_in_obj_info(p_obj);
      else
        p_target_obj = obj_get_forwarding_pointer_in_vt(p_obj);
      *p_ref = p_target_obj;
    }
    vector_block_clear(root_set);
    pool_put_entry(metadata->free_pool, root_set);
    root_set = pool_get_entry(repset_pool);
  } 
  
  return;
}

void gc_activate_finalizer_weakref_threads(GC *gc)
{
  Finalizer_Weakref_Metadata* metadata = gc->finalizer_weakref_metadata;
  
  if(metadata->pending_finalizers || metadata->pending_weak_references){
	  metadata->pending_finalizers = FALSE;
    metadata->pending_weak_references = FALSE;
    vm_hint_finalize();
  }
}
