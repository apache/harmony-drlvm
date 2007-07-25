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
#include "interior_pointer.h"
#include "../finalizer_weakref/finalizer_weakref.h"
#include "gc_block.h"
#include "compressed_ref.h"
#include "../utils/sync_stack.h"

#define GC_METADATA_SIZE_BYTES (1*MB)
#define GC_METADATA_EXTEND_SIZE_BYTES (1*MB)

#define METADATA_BLOCK_SIZE_BYTES VECTOR_BLOCK_DATA_SIZE_BYTES

GC_Metadata gc_metadata;

void gc_metadata_initialize(GC* gc)
{
  /* FIXME:: since we use a list to arrange the root sets and tasks, we can
     dynamically alloc space for metadata. 
     We just don't have this dynamic support at the moment. */

  unsigned int seg_size = GC_METADATA_SIZE_BYTES + METADATA_BLOCK_SIZE_BYTES;
  void* metadata = STD_MALLOC(seg_size);
  memset(metadata, 0, seg_size);
  gc_metadata.segments[0] = metadata;
  metadata = (void*)round_up_to_size((POINTER_SIZE_INT)metadata, METADATA_BLOCK_SIZE_BYTES);
  gc_metadata.num_alloc_segs = 1;

  unsigned int i=0;       
  unsigned int num_blocks =  GC_METADATA_SIZE_BYTES/METADATA_BLOCK_SIZE_BYTES;
  for(i=0; i<num_blocks; i++){
    Vector_Block* block = (Vector_Block*)((POINTER_SIZE_INT)metadata + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
  }
  
  /* part of the metadata space is used for trace_stack */
  unsigned num_tasks = num_blocks >> 1;
  gc_metadata.free_task_pool = sync_pool_create();
  for(i=0; i<num_tasks; i++){
    Vector_Block *block = (Vector_Block*)((POINTER_SIZE_INT)metadata + i*METADATA_BLOCK_SIZE_BYTES);
    vector_stack_init((Vector_Block*)block);
    pool_put_entry(gc_metadata.free_task_pool, (void*)block); 
  }
  gc_metadata.mark_task_pool = sync_pool_create();

  /* the other part is used for root sets (including rem sets) */
  gc_metadata.free_set_pool = sync_pool_create();
  /* initialize free rootset pool so that mutators can use them */  
  for(; i<num_blocks; i++){
    POINTER_SIZE_INT block = (POINTER_SIZE_INT)metadata + i*METADATA_BLOCK_SIZE_BYTES;
    pool_put_entry(gc_metadata.free_set_pool, (void*)block); 
  }

  gc_metadata.gc_rootset_pool = sync_pool_create();
  gc_metadata.gc_uncompressed_rootset_pool = sync_pool_create();
  gc_metadata.mutator_remset_pool = sync_pool_create();
  gc_metadata.collector_remset_pool = sync_pool_create();
  gc_metadata.collector_repset_pool = sync_pool_create();
#ifdef USE_32BITS_HASHCODE  
  gc_metadata.collector_hashcode_pool = sync_pool_create();
#endif
 
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
  sync_pool_destruct(metadata->gc_uncompressed_rootset_pool);
  sync_pool_destruct(metadata->mutator_remset_pool);
  sync_pool_destruct(metadata->collector_remset_pool);
  sync_pool_destruct(metadata->collector_repset_pool);
#ifdef USE_32BITS_HASHCODE  
  sync_pool_destruct(metadata->collector_hashcode_pool);
#endif

  for(unsigned int i=0; i<metadata->num_alloc_segs; i++){
    assert(metadata->segments[i]);
    STD_FREE(metadata->segments[i]);
  }
  
  gc->metadata = NULL;  
}

Vector_Block* gc_metadata_extend(Pool* pool)
{  
  GC_Metadata *metadata = &gc_metadata;
  lock(metadata->alloc_lock);
  Vector_Block* block = pool_get_entry(pool);
  if( block ){
    unlock(metadata->alloc_lock);
    return block;
  }
 
  unsigned int num_alloced = metadata->num_alloc_segs;
  if(num_alloced == GC_METADATA_SEGMENT_NUM){
    printf("Run out GC metadata, please give it more segments!\n");
    exit(0);
  }

  unsigned int seg_size =  GC_METADATA_EXTEND_SIZE_BYTES + METADATA_BLOCK_SIZE_BYTES;
  void *new_segment = STD_MALLOC(seg_size);
  memset(new_segment, 0, seg_size);
  metadata->segments[num_alloced] = new_segment;
  new_segment = (void*)round_up_to_size((POINTER_SIZE_INT)new_segment, METADATA_BLOCK_SIZE_BYTES);
  metadata->num_alloc_segs = num_alloced + 1;
  
  unsigned int num_blocks =  GC_METADATA_EXTEND_SIZE_BYTES/METADATA_BLOCK_SIZE_BYTES;

  unsigned int i=0;
  for(i=0; i<num_blocks; i++){
    Vector_Block* block = (Vector_Block*)((POINTER_SIZE_INT)new_segment + i*METADATA_BLOCK_SIZE_BYTES);
    vector_block_init(block, METADATA_BLOCK_SIZE_BYTES);
    assert(vector_block_is_empty(block));
  }

  if( pool == gc_metadata.free_task_pool){  
    for(i=0; i<num_blocks; i++){
      Vector_Block *block = (Vector_Block *)((POINTER_SIZE_INT)new_segment + i*METADATA_BLOCK_SIZE_BYTES);
      vector_stack_init(block);
      pool_put_entry(gc_metadata.free_task_pool, (void*)block);
    }
  
  }else{ 
    assert( pool == gc_metadata.free_set_pool );
    for(i=0; i<num_blocks; i++){
      POINTER_SIZE_INT block = (POINTER_SIZE_INT)new_segment + i*METADATA_BLOCK_SIZE_BYTES;    
      pool_put_entry(gc_metadata.free_set_pool, (void*)block); 
    }
  }
  
  block = pool_get_entry(pool);
  unlock(metadata->alloc_lock);

  return block;
}

extern Boolean IS_MOVE_COMPACT;

static void gc_update_repointed_sets(GC* gc, Pool* pool)
{
  GC_Metadata* metadata = gc->metadata;
  
  /* NOTE:: this is destructive to the root sets. */
  pool_iterator_init(pool);
  Vector_Block* root_set = pool_iterator_next(pool);

  while(root_set){
    POINTER_SIZE_INT* iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      REF* p_ref = (REF* )*iter;
      iter = vector_block_iterator_advance(root_set,iter);

      Partial_Reveal_Object* p_obj = read_slot(p_ref);
        if(IS_MOVE_COMPACT){
        /*This condition is removed because we do los sliding compaction at every major compaction after add los minor sweep.*/
        //if(obj_is_moved(p_obj)) 
          /*Fixme: los_boundery ruined the modularity of gc_common.h*/
          if(p_obj < los_boundary){
            write_slot(p_ref, obj_get_fw_in_oi(p_obj));
          }else{
            *p_ref = obj_get_fw_in_table(p_obj);
          }
        }else{
          if(obj_is_fw_in_oi(p_obj)){
            /* Condition obj_is_moved(p_obj) is for preventing mistaking previous mark bit of large obj as fw bit when fallback happens.
             * Because until fallback happens, perhaps the large obj hasn't been marked. So its mark bit remains as the last time.
             * This condition is removed because we do los sliding compaction at every major compaction after add los minor sweep.
             * In major collection condition obj_is_fw_in_oi(p_obj) can be omitted,
             * since those which can be scanned in MOS & NOS must have been set fw bit in oi.  */
            assert((POINTER_SIZE_INT)obj_get_fw_in_oi(p_obj) > DUAL_MARKBITS);
            write_slot(p_ref, obj_get_fw_in_oi(p_obj));
          }
        }
    }
    root_set = pool_iterator_next(pool);
  } 
  
  return;
}

void gc_fix_rootset(Collector* collector)
{  
  GC* gc = collector->gc;  
  GC_Metadata* metadata = gc->metadata;

  /* MINOR_COLLECTION doesn't need rootset update, but need reset */
  if( !gc_match_kind(gc, MINOR_COLLECTION)){
    gc_update_repointed_sets(gc, metadata->gc_rootset_pool);
#ifndef BUILD_IN_REFERENT
    gc_update_finref_repointed_refs(gc);
#endif
  } else {
    gc_set_pool_clear(metadata->gc_rootset_pool);
  }

#ifdef COMPRESS_REFERENCE
  gc_fix_uncompressed_rootset(gc);
#endif


  update_rootset_interior_pointer();
  /* it was pointing to the last root_set entry in gc_rootset_pool (before rem_sets). */
  gc->root_set = NULL;
  
  return;
}

void gc_set_rootset(GC* gc)
{
  GC_Metadata* metadata = gc->metadata;
  Pool* gc_rootset_pool = metadata->gc_rootset_pool;
  Pool* mutator_remset_pool = metadata->mutator_remset_pool;
  Pool* collector_remset_pool = metadata->collector_remset_pool;
  Pool* free_set_pool = metadata->free_set_pool;

  Vector_Block* root_set = NULL;
#ifdef COMPRESS_REFERENCE  
  gc_set_uncompressed_rootset(gc);
#endif
  /* put back last rootset block */
  pool_put_entry(gc_rootset_pool, gc->root_set);
  
  /* we only reset gc->root_set here for non gen mode, because we need it to remember the border
     between root_set and rem_set in gc_rootset_pool for gen mode. This is useful when a minor
     gen collection falls back to compaction, we can clear all the blocks in 
     gc_rootset_pool after the entry pointed by gc->root_set. So we clear this value
     only after we know we are not going to fallback. */
    // gc->root_set = NULL;
  
  if(!gc_is_gen_mode()) return;

  /* put back last remset block of each mutator */
  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    pool_put_entry(mutator_remset_pool, mutator->rem_set);
    mutator->rem_set = NULL;
    mutator = mutator->next;
  }

  /* put back last remset block of each collector (saved in last collection) */  
  unsigned int num_active_collectors = gc->num_active_collectors;
  for(unsigned int i=0; i<num_active_collectors; i++)
  {
    Collector* collector = gc->collectors[i];
    /* 1. in the first time GC, rem_set is NULL. 2. it should be NULL when NOS is forwarding_all */
    if(collector->rem_set == NULL) continue;
    pool_put_entry(metadata->collector_remset_pool, collector->rem_set);
    collector->rem_set = NULL;
  }

  if( !gc_match_kind(gc, MINOR_COLLECTION )){
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

  }else{ /* generational MINOR_COLLECTION */
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

void gc_reset_rootset(GC* gc)
{
  assert(pool_is_empty(gc_metadata.gc_rootset_pool));
  assert(gc->root_set == NULL);
  gc->root_set = free_set_pool_get_entry(&gc_metadata);
  assert(vector_block_is_empty(gc->root_set));

#ifdef COMPRESS_REFERENCE
  assert(pool_is_empty(gc_metadata.gc_uncompressed_rootset_pool));
  assert(gc->uncompressed_root_set == NULL);
  gc->uncompressed_root_set = free_set_pool_get_entry(&gc_metadata);
  assert(vector_block_is_empty(gc->uncompressed_root_set));
#endif

  return;
}

void gc_clear_remset(GC* gc)
{
  assert(gc->root_set != NULL);

  Pool* pool = gc_metadata.gc_rootset_pool;    
  Vector_Block* rem_set = pool_get_entry(pool);
  while(rem_set != gc->root_set){
    vector_block_clear(rem_set);
    pool_put_entry(gc_metadata.free_set_pool, rem_set);
    rem_set = pool_get_entry(pool);
  }
 
  assert(rem_set == gc->root_set);
  /* put back root set */
  pool_put_entry(pool, rem_set);
    
  return;
} 

extern Boolean verify_live_heap;
void gc_metadata_verify(GC* gc, Boolean is_before_gc)
{
  GC_Metadata* metadata = gc->metadata;
  assert(pool_is_empty(metadata->gc_rootset_pool));
  assert(pool_is_empty(metadata->collector_repset_pool));
  assert(pool_is_empty(metadata->mark_task_pool));
  
  if(!is_before_gc || !gc_is_gen_mode())
    assert(pool_is_empty(metadata->mutator_remset_pool));
  
  if(!gc_is_gen_mode()){
    /* FIXME:: even for gen gc, it should be empty if NOS is forwarding_all */  
    assert(pool_is_empty(metadata->collector_remset_pool));
  }

  if(verify_live_heap ){
    unsigned int free_pool_size = pool_size(metadata->free_set_pool);
  }
  
  return;  
}



