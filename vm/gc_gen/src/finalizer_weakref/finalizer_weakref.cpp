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


static inline Boolean obj_is_dead_in_gen_minor_gc(Partial_Reveal_Object *p_obj)
{
  /*
   * The first condition is for supporting switch between nongen and gen minor collection
   * With this kind of switch dead objects in MOS & LOS may be set the mark or fw bit in oi
   */
  return obj_belongs_to_nos(p_obj) && !obj_is_marked_or_fw_in_oi(p_obj);
}

static inline Boolean obj_is_dead_in_nongen_minor_gc(Partial_Reveal_Object *p_obj)
{
  return (obj_belongs_to_nos(p_obj) && !obj_is_fw_in_oi(p_obj))
          || (!obj_belongs_to_nos(p_obj) && !obj_is_marked_in_oi(p_obj));
}

static inline Boolean obj_is_dead_in_major_gc(Partial_Reveal_Object *p_obj)
{
  return !obj_is_marked_in_vt(p_obj);
}
// clear the two least significant bits of p_obj first
static inline Boolean gc_obj_is_dead(GC *gc, Partial_Reveal_Object *p_obj)
{
  unsigned int collect_kind = gc->collect_kind;
  
  assert(p_obj);
  if(collect_kind == MINOR_COLLECTION){
    if( gc_is_gen_mode())
      return obj_is_dead_in_gen_minor_gc(p_obj);
    else
      return obj_is_dead_in_nongen_minor_gc(p_obj);
  } else {
    return obj_is_dead_in_major_gc(p_obj);
  }
}

static inline Boolean fspace_obj_to_be_forwarded(Partial_Reveal_Object *p_obj)
{
  if(!obj_belongs_to_nos(p_obj)) return FALSE;
  return forward_first_half? (p_obj < object_forwarding_boundary):(p_obj>=object_forwarding_boundary);
}
static inline Boolean obj_need_move(GC *gc, Partial_Reveal_Object *p_obj)
{
  assert(!gc_obj_is_dead(gc, p_obj));
  
  if(gc_is_gen_mode() && gc->collect_kind == MINOR_COLLECTION)
    return fspace_obj_to_be_forwarded(p_obj);
  
  Space *space = space_of_addr(gc, p_obj);
  return space->move_object;
}

static void finref_add_repset_from_pool(GC *gc, Pool *pool)
{
  finref_reset_repset(gc);
  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      if(*p_ref && obj_need_move(gc, *p_ref))
        finref_repset_add_entry(gc, p_ref);
    }
  }
  finref_put_repset(gc);
}

static inline void fallback_update_fw_ref(Partial_Reveal_Object **p_ref)
{
  if(!IS_FALLBACK_COMPACTION)
    return;
  
  Partial_Reveal_Object *p_obj = *p_ref;
  if(obj_belongs_to_nos(p_obj) && obj_is_fw_in_oi(p_obj)){
    assert(!obj_is_marked_in_vt(p_obj));
    assert(obj_get_vt(p_obj) == obj_get_vt(obj_get_fw_in_oi(p_obj)));
    p_obj = obj_get_fw_in_oi(p_obj);
    assert(p_obj);
    *p_ref = p_obj;
  }
}

static void identify_finalizable_objects(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *obj_with_fin_pool = metadata->obj_with_fin_pool;
  
  gc_reset_finalizable_objects(gc);
  pool_iterator_init(obj_with_fin_pool);
  while(Vector_Block *block = pool_iterator_next(obj_with_fin_pool)){
    unsigned int block_has_ref = 0;
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      fallback_update_fw_ref(p_ref);
      Partial_Reveal_Object *p_obj = *p_ref;
      if(!p_obj)
        continue;
      if(gc_obj_is_dead(gc, p_obj)){
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
}

extern void trace_obj_in_gen_fw(Collector *collector, void *p_ref);
extern void trace_obj_in_nongen_fw(Collector *collector, void *p_ref);
extern void trace_obj_in_marking(Collector *collector, void *p_obj);
extern void trace_obj_in_fallback_marking(Collector *collector, void *p_ref);

typedef void (* Trace_Object_Func)(Collector *collector, void *p_ref_or_obj);
// clear the two least significant bits of p_obj first
// add p_ref to repset
static inline void resurrect_obj_tree(Collector *collector, Partial_Reveal_Object **p_ref)
{
  GC *gc = collector->gc;
  GC_Metadata* metadata = gc->metadata;
  unsigned int collect_kind = gc->collect_kind;
  Partial_Reveal_Object *p_obj = *p_ref;
  assert(p_obj && gc_obj_is_dead(gc, p_obj));
  
  void *p_ref_or_obj = p_ref;
  Trace_Object_Func trace_object;
  
  /* set trace_object() function */
  if(collect_kind == MINOR_COLLECTION){
    if(gc_is_gen_mode())
      trace_object = trace_obj_in_gen_fw;
    else
      trace_object = trace_obj_in_nongen_fw;
  } else if(collect_kind == MAJOR_COLLECTION){
    p_ref_or_obj = p_obj;
    trace_object = trace_obj_in_marking;
    obj_mark_in_vt(p_obj);
  } else {
    assert(collect_kind == FALLBACK_COLLECTION);
    trace_object = trace_obj_in_fallback_marking;
  }
  
  collector->trace_stack = free_task_pool_get_entry(metadata);
  collector_tracestack_push(collector, p_ref_or_obj);
  pool_put_entry(metadata->mark_task_pool, collector->trace_stack);
  
  collector->trace_stack = free_task_pool_get_entry(metadata);
  Vector_Block *task_block = pool_get_entry(metadata->mark_task_pool);
  while(task_block){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(task_block);
    while(!vector_block_iterator_end(task_block, iter)){
      void* p_ref_or_obj = (void *)*iter;
      assert((collect_kind!=MAJOR_COLLECTION && *(Partial_Reveal_Object **)p_ref_or_obj)
              || (collect_kind==MAJOR_COLLECTION && p_ref_or_obj));
      trace_object(collector, p_ref_or_obj);
      iter = vector_block_iterator_advance(task_block, iter);
    }
    vector_stack_clear(task_block);
    pool_put_entry(metadata->free_task_pool, task_block);
    task_block = pool_get_entry(metadata->mark_task_pool);      
  }
  
  task_block = (Vector_Block*)collector->trace_stack;
  vector_stack_clear(task_block);
  pool_put_entry(metadata->free_task_pool, task_block);
  collector->trace_stack = NULL;
}

static void resurrect_finalizable_objects(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *obj_with_fin_pool = metadata->obj_with_fin_pool;
  Pool *finalizable_obj_pool = metadata->finalizable_obj_pool;
  unsigned int collect_kind = gc->collect_kind;
  
  if(!finalizable_obj_pool_is_empty(gc)){
    finref_reset_repset(gc);
    pool_iterator_init(finalizable_obj_pool);
    while(Vector_Block *block = pool_iterator_next(finalizable_obj_pool)){
      POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
      for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
        Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
        Partial_Reveal_Object *p_obj = *p_ref;
        assert(p_obj);
        
        /*
         * In major & fallback collection we need record p_ref of the root dead obj to update it later.
         * Because it is outside heap, we can't update in ref fixing.
         * In minor collection p_ref of the root dead obj is automatically updated while tracing.
         */
        if(collect_kind != MINOR_COLLECTION)
          finref_repset_add_entry(gc, p_ref);
        
        /* Perhaps obj has been resurrected by previous resurrections */
        if(!gc_obj_is_dead(gc, p_obj)){
          if(gc->collect_kind == MINOR_COLLECTION && obj_need_move(gc, p_obj))
            *p_ref = obj_get_fw_in_oi(p_obj);
          continue;
        }
        
        resurrect_obj_tree(collector, p_ref);
      }
    }
    metadata->pending_finalizers = TRUE;
    finref_put_repset(gc);
  }
  
  finref_add_repset_from_pool(gc, obj_with_fin_pool);
  /* fianlizable objects have been added to collector repset pool */
  //finref_add_repset_from_pool(collector, finalizable_obj_pool);
}

static void identify_dead_refs(GC *gc, Pool *pool)
{
  finref_reset_repset(gc);
  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      fallback_update_fw_ref(p_referent_field);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!gc_obj_is_dead(gc, p_referent)){  // referent is alive
        if(obj_need_move(gc, p_referent))
          finref_repset_add_entry(gc, p_referent_field);
        *p_ref = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is weakly reachable: clear the referent field */
    }
  }
  finref_put_repset(gc);
  
  finref_add_repset_from_pool(gc, pool);
}

static void identify_dead_softrefs(Collector *collector)
{
  GC *gc = collector->gc;
  if(gc->collect_kind == MINOR_COLLECTION){
    assert(softref_pool_is_empty(gc));
    return;
  }
  
  Pool *softref_pool = gc->finref_metadata->softref_pool;
  identify_dead_refs(gc, softref_pool);
}

static void identify_dead_weakrefs(Collector *collector)
{
  GC *gc = collector->gc;
  Pool *weakref_pool = gc->finref_metadata->weakref_pool;
  
  identify_dead_refs(gc, weakref_pool);
}

/*
 * The reason why we don't use identify_dead_refs() to implement this function is
 * that we will differentiate phanref from softref & weakref in the future.
 */
static void identify_dead_phanrefs(Collector *collector)
{
  GC *gc = collector->gc;
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *phanref_pool = metadata->phanref_pool;
  
  finref_reset_repset(gc);
//  collector_reset_repset(collector);
  pool_iterator_init(phanref_pool);
  while(Vector_Block *block = pool_iterator_next(phanref_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      fallback_update_fw_ref(p_referent_field);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!gc_obj_is_dead(gc, p_referent)){  // referent is alive
        if(obj_need_move(gc, p_referent))
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
  
  finref_add_repset_from_pool(gc, phanref_pool);
}

static void put_finalizable_obj_to_vm(GC *gc)
{
  Pool *finalizable_obj_pool = gc->finref_metadata->finalizable_obj_pool;
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(finalizable_obj_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
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

static inline void put_dead_weak_refs_to_vm(GC *gc, Pool *reference_pool)
{
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(reference_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
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

static void put_dead_refs_to_vm(GC *gc)
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
  
  put_dead_weak_refs_to_vm(gc, softref_pool);
  put_dead_weak_refs_to_vm(gc, weakref_pool);
  put_dead_weak_refs_to_vm(gc, phanref_pool);
}

void collector_identify_finref(Collector *collector)
{
  GC *gc = collector->gc;
  
  gc_set_weakref_sets(gc);
  identify_dead_softrefs(collector);
  identify_dead_weakrefs(collector);
  identify_finalizable_objects(collector);
  resurrect_finalizable_objects(collector);
  identify_dead_phanrefs(collector);
}

void gc_put_finref_to_vm(GC *gc)
{
  put_dead_refs_to_vm(gc);
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
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
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

static void update_referent_field_ignore_finref(GC *gc, Pool *pool)
{
  while(Vector_Block *block = pool_get_entry(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      assert(p_obj);
      Partial_Reveal_Object **p_referent_field = obj_get_referent_field(p_obj);
      fallback_update_fw_ref(p_referent_field);
      Partial_Reveal_Object *p_referent = *p_referent_field;
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!gc_obj_is_dead(gc, p_referent)){  // referent is alive
        if(obj_need_move(gc, p_referent))
          finref_repset_add_entry(gc, p_referent_field);
        *p_ref = NULL;
        continue;
      }
      *p_referent_field = NULL; /* referent is weakly reachable: clear the referent field */
    }
  }
}

void gc_update_weakref_ignore_finref(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  finref_reset_repset(gc);
  update_referent_field_ignore_finref(gc, metadata->softref_pool);
  update_referent_field_ignore_finref(gc, metadata->weakref_pool);
  update_referent_field_ignore_finref(gc, metadata->phanref_pool);
  finref_put_repset(gc);
}

static void move_compaction_update_referent_field(GC *gc, Partial_Reveal_Object **p_referent_field)
{
  if(!address_belongs_to_gc_heap((void *)p_referent_field, gc)){
    *p_referent_field = obj_get_fw_in_table(*p_referent_field);
    return;
  }
  
  Space *ref_space = space_of_addr(gc, p_referent_field);
  if(ref_space->move_object){
    unsigned int offset = get_gc_referent_offset();
    Partial_Reveal_Object *p_old_ref = (Partial_Reveal_Object *)((POINTER_SIZE_INT)p_referent_field - offset);
    Partial_Reveal_Object *p_new_ref = obj_get_fw_in_table(p_old_ref);
    p_referent_field = (Partial_Reveal_Object **)((POINTER_SIZE_INT)p_new_ref + offset);
  }
  assert(space_of_addr(gc, *p_referent_field)->move_object);
  *p_referent_field = obj_get_fw_in_table(*p_referent_field);
}

extern Boolean IS_MOVE_COMPACT;

void gc_update_finref_repointed_refs(GC *gc)
{
  unsigned int collect_kind = gc->collect_kind;
  Finref_Metadata* metadata = gc->finref_metadata;
  Pool *repset_pool = metadata->repset_pool;
  
  /* NOTE:: this is destructive to the root sets. */
  Vector_Block* repset = pool_get_entry(repset_pool);

  while(repset){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(repset);
    for(; !vector_block_iterator_end(repset,iter); iter = vector_block_iterator_advance(repset,iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object** )*iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      
      if(!IS_MOVE_COMPACT){
        assert(obj_is_fw_in_oi(p_obj));
        assert(collect_kind == MINOR_COLLECTION || obj_is_marked_in_vt(p_obj));
        *p_ref = obj_get_fw_in_oi(p_obj);
      } else {
        move_compaction_update_referent_field(gc, p_ref);
      }
    }
    vector_block_clear(repset);
    pool_put_entry(metadata->free_pool, repset);
    repset = pool_get_entry(repset_pool);
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
