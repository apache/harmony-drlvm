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

Boolean IGNORE_FINREF = FALSE;
Boolean DURING_RESURRECTION = FALSE;


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
  assert(p_obj);
  if(gc_match_kind(gc, MINOR_COLLECTION)){
    if(gc_is_gen_mode())
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
  
  if(gc_is_gen_mode() && gc_match_kind(gc, MINOR_COLLECTION))
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
      REF* p_ref = (REF*)iter;
      Partial_Reveal_Object* p_obj = read_slot(p_ref);
      if(*p_ref && obj_need_move(gc, p_obj))
        finref_repset_add_entry(gc, p_ref);
    }
  }
  finref_put_repset(gc);
}

static inline void fallback_update_fw_ref(REF* p_ref)
{
  if(!IS_FALLBACK_COMPACTION)
    return;
  
  Partial_Reveal_Object *p_obj = read_slot(p_ref);
  if(obj_belongs_to_nos(p_obj) && obj_is_fw_in_oi(p_obj)){
    assert(!obj_is_marked_in_vt(p_obj));
    assert(obj_get_vt(p_obj) == obj_get_vt(obj_get_fw_in_oi(p_obj)));
    p_obj = obj_get_fw_in_oi(p_obj);
    assert(p_obj);
    write_slot(p_ref, p_obj);
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
      REF* p_ref = (REF *)iter;
      if(IS_FALLBACK_COMPACTION)
        fallback_update_fw_ref(p_ref);  // in case that this collection is FALLBACK_COLLECTION
      Partial_Reveal_Object *p_obj = read_slot(p_ref);
      if(!p_obj)
        continue;
      if(gc_obj_is_dead(gc, p_obj)){
        gc_add_finalizable_obj(gc, p_obj);
        *p_ref = (REF)NULL;
      } else {
        if(gc_match_kind(gc, MINOR_COLLECTION) && obj_need_move(gc, p_obj)){
          assert(obj_is_fw_in_oi(p_obj));
          write_slot(p_ref, obj_get_fw_in_oi(p_obj));
        }
        ++block_has_ref;
      }
    }
    if(!block_has_ref)
      vector_block_clear(block);
  }
  gc_put_finalizable_objects(gc);
  
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_add_repset_from_pool(gc, obj_with_fin_pool);
}

extern void trace_obj_in_gen_fw(Collector *collector, void *p_ref);
extern void trace_obj_in_nongen_fw(Collector *collector, void *p_ref);
extern void trace_obj_in_marking(Collector *collector, void *p_obj);
extern void trace_obj_in_fallback_marking(Collector *collector, void *p_ref);

typedef void (* Trace_Object_Func)(Collector *collector, void *p_ref_or_obj);
// clear the two least significant bits of p_obj first
// add p_ref to repset
static inline void resurrect_obj_tree(Collector *collector, REF* p_ref)
{
  GC *gc = collector->gc;
  GC_Metadata* metadata = gc->metadata;
  Partial_Reveal_Object *p_obj = read_slot(p_ref);
  assert(p_obj && gc_obj_is_dead(gc, p_obj));
  
  void *p_ref_or_obj = p_ref;
  Trace_Object_Func trace_object;
  
  /* set trace_object() function */
  if(gc_match_kind(gc, MINOR_COLLECTION)){
    if(gc_is_gen_mode())
      trace_object = trace_obj_in_gen_fw;
    else
      trace_object = trace_obj_in_nongen_fw;
  } else if(gc_match_kind(gc, MAJOR_COLLECTION)){
    p_ref_or_obj = p_obj;
    trace_object = trace_obj_in_marking;
    obj_mark_in_vt(p_obj);
  } else {
    assert(gc_match_kind(gc, FALLBACK_COLLECTION));
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
      assert((gc_match_kind(gc, MINOR_COLLECTION | FALLBACK_COLLECTION) && *(Partial_Reveal_Object **)p_ref_or_obj)
              || (gc_match_kind(gc, MAJOR_COLLECTION) && p_ref_or_obj));
      trace_object(collector, p_ref_or_obj);
      if(collector->result == FALSE)  break; /* force return */
      
      iter = vector_block_iterator_advance(task_block, iter);
    }
    vector_stack_clear(task_block);
    pool_put_entry(metadata->free_task_pool, task_block);
    
    if(collector->result == FALSE){
      gc_task_pool_clear(metadata->mark_task_pool);
      break; /* force return */
    }
    
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
  Pool *finalizable_obj_pool = metadata->finalizable_obj_pool;
  
  if(finalizable_obj_pool_is_empty(gc))
    return;
  
  DURING_RESURRECTION = TRUE;
  
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_reset_repset(gc);
  pool_iterator_init(finalizable_obj_pool);
  while(Vector_Block *block = pool_iterator_next(finalizable_obj_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      REF* p_ref = (REF *)iter;
      Partial_Reveal_Object *p_obj = read_slot(p_ref);
      assert(p_obj);
      
      /* In major & fallback collection we need record p_ref of the root dead obj to update it later.
       * Because it is outside heap, we can't update in ref fixing.
       * In minor collection p_ref of the root dead obj is automatically updated while tracing.
       */
      if(!gc_match_kind(gc, MINOR_COLLECTION))
        finref_repset_add_entry(gc, p_ref);
      
      /* Perhaps obj has been resurrected by previous resurrections */
      if(!gc_obj_is_dead(gc, p_obj)){
        if(gc_match_kind(gc, MINOR_COLLECTION) && obj_need_move(gc, p_obj))
          write_slot(p_ref, obj_get_fw_in_oi(p_obj));
        continue;
      }
      
      resurrect_obj_tree(collector, p_ref);
      if(collector->result == FALSE){
        /* Resurrection fallback happens */
        assert(gc_match_kind(gc, MINOR_COLLECTION));
        return; /* force return */
      }
    }
  }
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_put_repset(gc);
  metadata->pending_finalizers = TRUE;
  
  DURING_RESURRECTION = FALSE;
  
  /* fianlizable objs have been added to finref repset pool or updated by tracing */
}

static void identify_dead_refs(GC *gc, Pool *pool)
{
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_reset_repset(gc);
  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      REF* p_ref = (REF*)iter;
      Partial_Reveal_Object *p_obj = read_slot(p_ref);
      assert(p_obj);
      REF* p_referent_field = obj_get_referent_field(p_obj);
      if(IS_FALLBACK_COMPACTION)
        fallback_update_fw_ref(p_referent_field);
      Partial_Reveal_Object *p_referent = read_slot(p_referent_field);
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = (REF)NULL;
        continue;
      }
      if(!gc_obj_is_dead(gc, p_referent)){  // referent is alive
        if(obj_need_move(gc, p_referent))
          if(gc_match_kind(gc, MINOR_COLLECTION)){
            assert(obj_is_fw_in_oi(p_referent));
            write_slot(p_referent_field, (obj_get_fw_in_oi(p_referent)));
          } else {
            finref_repset_add_entry(gc, p_referent_field);
          }
        *p_ref = (REF)NULL;
        continue;
      }
      *p_referent_field = (REF)NULL; /* referent is weakly reachable: clear the referent field */
    }
  }
  if(!gc_match_kind(gc, MINOR_COLLECTION)){
    finref_put_repset(gc);
    finref_add_repset_from_pool(gc, pool);
  }
}

static void identify_dead_softrefs(Collector *collector)
{
  GC *gc = collector->gc;
  if(gc_match_kind(gc, MINOR_COLLECTION)){
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
  
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_reset_repset(gc);
//  collector_reset_repset(collector);
  pool_iterator_init(phanref_pool);
  while(Vector_Block *block = pool_iterator_next(phanref_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object **p_ref = (Partial_Reveal_Object **)iter;
      Partial_Reveal_Object *p_obj = read_slot((REF*)p_ref);
      assert(p_obj);
      REF* p_referent_field = obj_get_referent_field(p_obj);
      if(IS_FALLBACK_COMPACTION)
      fallback_update_fw_ref(p_referent_field);
      Partial_Reveal_Object *p_referent = read_slot(p_referent_field);
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = NULL;
        continue;
      }
      if(!gc_obj_is_dead(gc, p_referent)){  // referent is alive
        if(obj_need_move(gc, p_referent))
           if(gc_match_kind(gc, MINOR_COLLECTION)){
            assert(obj_is_fw_in_oi(p_referent));
            write_slot(p_referent_field, (obj_get_fw_in_oi(p_referent)));
          } else {
            finref_repset_add_entry(gc, p_referent_field);
          }
        *p_ref = (REF)NULL;
        continue;
      }
      *p_referent_field = (REF)NULL;
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
  if(!gc_match_kind(gc, MINOR_COLLECTION)){
    finref_put_repset(gc);
    finref_add_repset_from_pool(gc, phanref_pool);
  }
}

static void put_finalizable_obj_to_vm(GC *gc)
{
  Pool *finalizable_obj_pool = gc->finref_metadata->finalizable_obj_pool;
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(finalizable_obj_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      assert(*iter);
      Managed_Object_Handle p_obj = (Managed_Object_Handle)read_slot((REF*)iter);
      vm_finalize_object(p_obj);
      iter = vector_block_iterator_advance(block, iter);
    }
    vector_block_clear(block);
    pool_put_entry(free_pool, block);
  }
}

static inline void put_dead_weak_refs_to_vm(GC *gc, Pool *ref_pool)
{
  Pool *free_pool = gc->finref_metadata->free_pool;
  
  while(Vector_Block *block = pool_get_entry(ref_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      Managed_Object_Handle p_obj = (Managed_Object_Handle)read_slot((REF*)iter);
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
  Finref_Metadata *metadata = gc->finref_metadata;
  
  if(softref_pool_is_empty(gc)
      && weakref_pool_is_empty(gc)
      && phanref_pool_is_empty(gc)
      && pool_is_empty(metadata->fallback_ref_pool)){
    gc_clear_weakref_pools(gc);
    return;
  }
  
  put_dead_weak_refs_to_vm(gc, metadata->softref_pool);
  put_dead_weak_refs_to_vm(gc, metadata->weakref_pool);
  put_dead_weak_refs_to_vm(gc, metadata->phanref_pool);
  
  if(/*IS_FALLBACK_COMPACTION && */!pool_is_empty(metadata->fallback_ref_pool))
    put_dead_weak_refs_to_vm(gc, metadata->fallback_ref_pool);
  metadata->pending_weakrefs = TRUE;
}

/* Finalizable objs falls back to objs with fin when resurrection fallback happens */
static void finalizable_objs_fallback(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *finalizable_obj_pool = metadata->finalizable_obj_pool;
  Pool *obj_with_fin_pool = metadata->obj_with_fin_pool;
  Vector_Block *obj_with_fin_block = pool_get_entry(obj_with_fin_pool);
  assert(obj_with_fin_block);
  
  Boolean pending_finalizers = FALSE;
  
  while(Vector_Block *block = pool_get_entry(finalizable_obj_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      REF* p_ref = (REF*)iter;
      Partial_Reveal_Object *p_obj = read_slot(p_ref);
      assert(p_obj);
      /* Perhaps obj has been resurrected by previous resurrections */
      if(!gc_obj_is_dead(gc, p_obj) && obj_belongs_to_nos(p_obj)){
        if(!gc_is_gen_mode() || fspace_obj_to_be_forwarded(p_obj)){
          write_slot(p_ref , obj_get_fw_in_oi(p_obj));
          p_obj = read_slot(p_ref);
        }
      }
      gc_add_finalizer(gc, obj_with_fin_block, p_obj);  // Perhaps p_obj has been forwarded, so we use *p_ref rather than p_obj
    }
  }
  
  pool_put_entry(obj_with_fin_pool, obj_with_fin_block);
  metadata->pending_finalizers = pending_finalizers;
}

static void dead_weak_refs_fallback(GC *gc, Pool *ref_pool)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  Pool *free_pool = metadata->free_pool;
  Pool *fallback_ref_pool = metadata->fallback_ref_pool;
  
  Vector_Block *fallback_ref_block = finref_get_free_block(gc);
  while(Vector_Block *block = pool_get_entry(ref_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    while(!vector_block_iterator_end(block, iter)){
      Partial_Reveal_Object *p_obj = read_slot((REF*)iter);
      if(p_obj)
        finref_add_fallback_ref(gc, fallback_ref_block, p_obj);
      iter = vector_block_iterator_advance(block, iter);
    }
    vector_block_clear(block);
    pool_put_entry(free_pool, block);
  }
  
  pool_put_entry(fallback_ref_pool, fallback_ref_block);
}

/* Record softrefs and weakrefs whose referents are dead.
 * In fallback collection these refs will not be considered for enqueueing again,
 * since their referent fields have been cleared by identify_dead_refs().
 */
static void dead_refs_fallback(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  if(!softref_pool_is_empty(gc) || !weakref_pool_is_empty(gc))
    metadata->pending_weakrefs = TRUE;
  
  dead_weak_refs_fallback(gc, metadata->softref_pool);
  dead_weak_refs_fallback(gc, metadata->weakref_pool);
  
  gc_clear_weakref_pools(gc);
}

static void resurrection_fallback_handler(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  /* Repset pool should be empty, because we don't add anthing to this pool in Minor Collection. */
  assert(pool_is_empty(metadata->repset_pool));
  
  finalizable_objs_fallback(gc);
  dead_refs_fallback(gc);
  
  assert(pool_is_empty(metadata->finalizable_obj_pool));
  assert(pool_is_empty(metadata->softref_pool));
  assert(pool_is_empty(metadata->weakref_pool));
  assert(pool_is_empty(metadata->phanref_pool));
  
  assert(metadata->finalizable_obj_set == NULL);
  assert(metadata->repset == NULL);
}

void collector_identify_finref(Collector *collector)
{
  GC *gc = collector->gc;
  
  gc_set_weakref_sets(gc);
  identify_dead_softrefs(collector);
  identify_dead_weakrefs(collector);
  identify_finalizable_objects(collector);
  resurrect_finalizable_objects(collector);
  gc->collect_result = gc_collection_result(gc);
  if(!gc->collect_result){
    assert(gc_match_kind(gc, MINOR_COLLECTION));
    resurrection_fallback_handler(gc);
    return;
  }
  identify_dead_phanrefs(collector);
}

void fallback_finref_cleanup(GC *gc)
{
  gc_set_weakref_sets(gc);
  gc_clear_weakref_pools(gc);
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
      Managed_Object_Handle p_obj = (Managed_Object_Handle)read_slot((REF*)iter);
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
      REF* p_ref = (REF*)iter;
      Partial_Reveal_Object *p_obj = read_slot(p_ref);
      assert(p_obj);
      REF* p_referent_field = obj_get_referent_field(p_obj);
      if(IS_FALLBACK_COMPACTION)
        fallback_update_fw_ref(p_referent_field);
      Partial_Reveal_Object* p_referent = read_slot(p_referent_field);
      
      if(!p_referent){  // referent field has been cleared
        *p_ref = (REF)NULL;
        continue;
      }
      if(!gc_obj_is_dead(gc, p_referent)){  // referent is alive
        if(obj_need_move(gc, p_referent))
          if(gc_match_kind(gc, MINOR_COLLECTION)){
            assert(obj_is_fw_in_oi(p_referent));
            write_slot(p_referent_field , obj_get_fw_in_oi(p_referent));
          } else {
            finref_repset_add_entry(gc, p_referent_field);
          }
        *p_ref = (REF)NULL;
        continue;
      }
      *p_referent_field = (REF)NULL; /* referent is weakly reachable: clear the referent field */
    }
  }
}

void gc_update_weakref_ignore_finref(GC *gc)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_reset_repset(gc);
  update_referent_field_ignore_finref(gc, metadata->softref_pool);
  update_referent_field_ignore_finref(gc, metadata->weakref_pool);
  update_referent_field_ignore_finref(gc, metadata->phanref_pool);
  if(!gc_match_kind(gc, MINOR_COLLECTION))
    finref_put_repset(gc);
}

static void move_compaction_update_ref(GC *gc, REF* p_ref)
{
  /* If p_ref belongs to heap, it must be a referent field pointer */
  if(address_belongs_to_gc_heap((void *)p_ref, gc) && (space_of_addr(gc, p_ref))->move_object){
    unsigned int offset = get_gc_referent_offset();
    Partial_Reveal_Object *p_old_ref = (Partial_Reveal_Object *)((POINTER_SIZE_INT)p_ref - offset);
    Partial_Reveal_Object *p_new_ref = ref_to_obj_ptr(obj_get_fw_in_table(p_old_ref));
    p_ref = (REF*)((POINTER_SIZE_INT)p_new_ref + offset);
  }
  Partial_Reveal_Object* p_obj = read_slot(p_ref);
  assert(space_of_addr(gc, (void*)p_obj)->move_object);
  *p_ref = obj_get_fw_in_table(p_obj);
}

extern Boolean IS_MOVE_COMPACT;

/* parameter pointer_addr_in_pool means it is p_ref or p_obj in pool */
static void destructively_fix_finref_pool(GC *gc, Pool *pool, Boolean pointer_addr_in_pool)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  REF* p_ref;
  Partial_Reveal_Object *p_obj;
  
  /* NOTE:: this is destructive to the root sets. */
  Vector_Block *repset = pool_get_entry(pool);
  while(repset){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(repset);
    for(; !vector_block_iterator_end(repset,iter); iter = vector_block_iterator_advance(repset,iter)){
      if(pointer_addr_in_pool)
        p_ref = (REF*)*iter;
      else
        p_ref = (REF*)iter;
      p_obj = read_slot(p_ref);
      
      if(!IS_MOVE_COMPACT){
        assert(obj_is_marked_in_vt(p_obj));
        assert(obj_is_fw_in_oi(p_obj));
        write_slot(p_ref , obj_get_fw_in_oi(p_obj));
      } else {
        move_compaction_update_ref(gc, p_ref);
      }
    }
    vector_block_clear(repset);
    pool_put_entry(metadata->free_pool, repset);
    repset = pool_get_entry(pool);
  }
}

/* parameter pointer_addr_in_pool means it is p_ref or p_obj in pool */
static void nondestructively_fix_finref_pool(GC *gc, Pool *pool, Boolean pointer_addr_in_pool)
{
  Finref_Metadata *metadata = gc->finref_metadata;
  REF* p_ref;
  Partial_Reveal_Object *p_obj;
  
  /* NOTE:: this is nondestructive to the root sets. */
  pool_iterator_init(pool);
  while(Vector_Block *repset = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(repset);
    for(; !vector_block_iterator_end(repset,iter); iter = vector_block_iterator_advance(repset,iter)){
      if(pointer_addr_in_pool)
        p_ref = (REF*)*iter;
      else
        p_ref = (REF*)iter;
      p_obj = read_slot(p_ref);
      
      if(!IS_MOVE_COMPACT){
        assert(obj_is_marked_in_vt(p_obj));
        assert(obj_is_fw_in_oi(p_obj));
        write_slot(p_ref , obj_get_fw_in_oi(p_obj));
      } else {
        move_compaction_update_ref(gc, p_ref);
      }
    }
  }
}

void gc_update_finref_repointed_refs(GC *gc)
{
  assert(!gc_match_kind(gc, MINOR_COLLECTION));
  
  Finref_Metadata* metadata = gc->finref_metadata;
  Pool *repset_pool = metadata->repset_pool;
  Pool *fallback_ref_pool = metadata->fallback_ref_pool;
  
  destructively_fix_finref_pool(gc, repset_pool, TRUE);
  if(IS_FALLBACK_COMPACTION && !pool_is_empty(fallback_ref_pool))
    nondestructively_fix_finref_pool(gc, fallback_ref_pool, FALSE);
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
