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
 * @author Xiao-Feng Li, 2006/10/25
 */

#include "gc_metadata.h"
#include "../thread/mutator.h"
#include "../thread/collector.h"

#define GC_METADATA_SIZE_BYTES 32*MB

#define METADATA_BLOCK_SIZE_BIT_SHIFT 12
#define METADATA_BLOCK_SIZE_BYTES (1<<METADATA_BLOCK_SIZE_BIT_SHIFT)

static GC_Metadata gc_metadata;

void gc_metadata_initialize(GC* gc)
{
  /* FIXME:: since we use a list to arrange the root sets and tasks, we can
     dynamically alloc space for metadata. 
     We just don't have this dynamic support at the moment. */

  void* metadata = STD_MALLOC(GC_METADATA_SIZE_BYTES);
  memset(metadata, 0, GC_METADATA_SIZE_BYTES);
  gc_metadata.heap_start = metadata;
  gc_metadata.heap_end = (void*)((unsigned int)metadata + GC_METADATA_SIZE_BYTES);

  unsigned int i=0;       
  unsigned int num_blocks =  GC_METADATA_SIZE_BYTES >> METADATA_BLOCK_SIZE_BIT_SHIFT;
  for(i=0; i<num_blocks; i++){
    Vector_Block* block = (Vector_Block*)((unsigned int)metadata + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
  }
  
  /* half of the metadata space is used for mark_stack */
  unsigned num_tasks = num_blocks >> 1;
  gc_metadata.free_task_pool = sync_pool_create(num_tasks);
  for(i=0; i<num_tasks; i++){
    unsigned int block = (unsigned int)metadata + i*METADATA_BLOCK_SIZE_BYTES;    
    pool_put_entry(gc_metadata.free_task_pool, (void*)block); 
  }
  gc_metadata.mark_task_pool = sync_pool_create(num_tasks);

  /* the other half is used for root sets (including rem sets) */
  unsigned num_sets = num_blocks >> 1;
  gc_metadata.free_set_pool = sync_pool_create(num_sets);
  /* initialize free rootset pool so that mutators can use them */  
  for(; i<num_sets+num_tasks; i++){
    unsigned int block = (unsigned int)metadata + i*METADATA_BLOCK_SIZE_BYTES;    
    pool_put_entry(gc_metadata.free_set_pool, (void*)block); 
  }

  gc_metadata.gc_rootset_pool = sync_pool_create(num_sets);
  gc_metadata.mutator_remset_pool = sync_pool_create(num_sets);
  gc_metadata.collector_remset_pool = sync_pool_create(num_sets);
  gc_metadata.collector_repset_pool = sync_pool_create(num_sets);
 
  gc->metadata = &gc_metadata; 
  return;  
}

void gc_metadata_destruct(GC* gc)
{
  GC_Metadata* metadata = gc->metadata;
  sync_pool_destruct(metadata->free_task_pool);
  sync_pool_destruct(metadata->mark_task_pool);
  
  sync_pool_destruct(metadata->free_set_pool);
  sync_pool_destruct(metadata->gc_rootset_pool); 
  sync_pool_destruct(metadata->mutator_remset_pool);  
  sync_pool_destruct(metadata->collector_remset_pool);
  sync_pool_destruct(metadata->collector_repset_pool);

  STD_FREE(metadata->heap_start);
  gc->metadata = NULL;  
}

void gc_metadata_reset(GC* gc)
{
  GC_Metadata* metadata = gc->metadata;
  Pool* gc_rootset_pool = metadata->gc_rootset_pool;
  Pool* mutator_remset_pool = metadata->mutator_remset_pool;
  Pool* collector_remset_pool = metadata->collector_remset_pool;
  Pool* free_set_pool = metadata->free_set_pool;

  Vector_Block* root_set = NULL;
  
  /* put back last rootset block */
  pool_put_entry(gc_rootset_pool, gc->root_set);
  gc->root_set = NULL;
  
  if(!gc_requires_barriers()) return;

  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    pool_put_entry(mutator_remset_pool, mutator->rem_set);
    mutator->rem_set = NULL;
  }  
  
  for(unsigned int i=0; i<gc->num_collectors; i++){
    Collector* collector = gc->collectors[i];
    pool_put_entry(collector_remset_pool, collector->rem_set);
    collector->rem_set = NULL;
  }

  if( gc->collect_kind == MAJOR_COLLECTION ){
    /* all the remsets are useless now */
    /* clean and put back mutator remsets */  
    root_set = pool_get_entry( mutator_remset_pool );
    while(root_set){
        vector_block_clear(root_set);
        pool_put_entry(free_set_pool, root_set);
        root_set = pool_get_entry( mutator_remset_pool );
    }
  
    /* clean and put back collector remsets */  
    root_set = pool_get_entry( collector_remset_pool );
    while(root_set){
        vector_block_clear(root_set);
        pool_put_entry(free_set_pool, root_set);
        root_set = pool_get_entry( collector_remset_pool );
    }

  }else{ /* MINOR_COLLECTION */
    /* all the remsets are put into the shared pool */
    root_set = pool_get_entry( mutator_remset_pool );
    while(root_set){
        pool_put_entry(gc_rootset_pool, root_set);
        root_set = pool_get_entry( mutator_remset_pool );
    }
  
    /* put back collector remsets */  
    root_set = pool_get_entry( collector_remset_pool );
    while(root_set){
        pool_put_entry(gc_rootset_pool, root_set);
        root_set = pool_get_entry( collector_remset_pool );
    }
  }
  
  return;

}

void mutator_remset_add_entry(Mutator* mutator, Partial_Reveal_Object** p_ref)
{
  Vector_Block* root_set = mutator->rem_set;  
  vector_block_add_entry(root_set, (unsigned int)p_ref);
  
  if( !vector_block_is_full(root_set)) return;
    
  pool_put_entry(gc_metadata.mutator_remset_pool, root_set);
  mutator->rem_set = pool_get_entry(gc_metadata.free_set_pool);  
}

void collector_repset_add_entry(Collector* collector, Partial_Reveal_Object** p_ref)
{
  assert( p_ref >= gc_heap_base_address() && p_ref < gc_heap_ceiling_address()); 

  Vector_Block* root_set = collector->rep_set;  
  vector_block_add_entry(root_set, (unsigned int)p_ref);
  
  if( !vector_block_is_full(root_set)) return;
    
  pool_put_entry(gc_metadata.collector_repset_pool, root_set);
  collector->rep_set = pool_get_entry(gc_metadata.free_set_pool);  
}

void collector_remset_add_entry(Collector* collector, Partial_Reveal_Object** p_ref)
{
  Vector_Block* root_set = collector->rem_set;  
  vector_block_add_entry(root_set, (unsigned int)p_ref);
  
  if( !vector_block_is_full(root_set)) return;
    
  pool_put_entry(gc_metadata.collector_remset_pool, root_set);
  collector->rem_set = pool_get_entry(gc_metadata.free_set_pool);  
}

void collector_marktask_add_entry(Collector* collector, Partial_Reveal_Object* p_obj)
{
  assert( p_obj>= gc_heap_base_address() && p_obj < gc_heap_ceiling_address()); 

  Vector_Block* mark_task = (Vector_Block*)collector->mark_stack;
  vector_block_add_entry(mark_task, (unsigned int)p_obj);

  if( !vector_block_is_full(mark_task)) return;

  pool_put_entry(gc_metadata.mark_task_pool, mark_task);
  collector->mark_stack = (MarkStack*)pool_get_entry(gc_metadata.free_task_pool);
}

void gc_rootset_add_entry(GC* gc, Partial_Reveal_Object** p_ref)
{
  assert( p_ref < gc_heap_base_address() || p_ref >= gc_heap_ceiling_address()); 
  
  Vector_Block* root_set = gc->root_set;  
  vector_block_add_entry(root_set, (unsigned int)p_ref);
  
  if( !vector_block_is_full(root_set)) return;
    
  pool_put_entry(gc_metadata.gc_rootset_pool, root_set);
  gc->root_set = pool_get_entry(gc_metadata.free_set_pool);  
}


static void gc_update_repointed_sets(GC* gc, Pool* pool)
{
  GC_Metadata* metadata = gc->metadata;
  
  /* NOTE:: this is destructive to the root sets. */
  Vector_Block* root_set = pool_get_entry(pool);

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
      if( pool != metadata->gc_rootset_pool)
        assert(obj_is_forwarded_in_obj_info(p_obj));
      else
#endif
      if(!obj_is_forwarded_in_obj_info(p_obj)) continue;
      Partial_Reveal_Object* p_target_obj = get_forwarding_pointer_in_obj_info(p_obj);
      *p_ref = p_target_obj; 
    }
    vector_block_clear(root_set);
    pool_put_entry(metadata->free_set_pool, root_set);
    root_set = pool_get_entry(pool);
  } 
  
  return;
}

void update_rootset_interior_pointer();

void gc_update_repointed_refs(Collector* collector)
{  
  GC* gc = collector->gc;
  GC_Metadata* metadata = gc->metadata;
  gc_update_repointed_sets(gc, metadata->gc_rootset_pool);
  gc_update_repointed_sets(gc, metadata->collector_repset_pool);   
  update_rootset_interior_pointer();
  
  return;
}

void gc_reset_rootset(GC* gc)
{
  gc->root_set = pool_get_entry(gc_metadata.free_set_pool);  
  return;
}  


