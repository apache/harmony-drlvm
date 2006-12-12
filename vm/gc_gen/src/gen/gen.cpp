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
 * @author Xiao-Feng Li, 2006/10/05
 */

#include "port_sysinfo.h"

#include "gen.h"

/* fspace size limit is not interesting. only for manual tuning purpose */
unsigned int min_nos_size_bytes = 2 * MB;
unsigned int max_nos_size_bytes = 64 * MB;
unsigned int NOS_SIZE = 0;

static void gc_gen_get_system_info(GC_Gen *gc_gen) 
{
  gc_gen->_machine_page_size_bytes = port_vmem_page_sizes()[0];
  gc_gen->_num_processors = port_CPUs_number();
}

void gc_gen_initialize(GC_Gen *gc_gen, unsigned int min_heap_size, unsigned int max_heap_size) 
{
  assert(gc_gen); 
  assert(max_heap_size <= max_heap_size_bytes);

  min_heap_size = round_up_to_size(min_heap_size, GC_BLOCK_SIZE_BYTES);
  max_heap_size = round_up_to_size(max_heap_size, GC_BLOCK_SIZE_BYTES);

  gc_gen_get_system_info(gc_gen); 

  void *reserved_base = NULL;

  /* allocate memory for gc_gen */
  gc_gen->allocated_memory = NULL;
  pool_create(&gc_gen->aux_pool, 0);
  
  apr_status_t status = port_vmem_reserve(&gc_gen->allocated_memory, 
                  &reserved_base, max_heap_size, 
                  PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE, 
                  gc_gen->_machine_page_size_bytes, gc_gen->aux_pool);
  
  while(APR_SUCCESS != status){
    max_heap_size -= gc_gen->_machine_page_size_bytes;
    status = port_vmem_reserve(&gc_gen->allocated_memory, 
                  &reserved_base, max_heap_size, 
                  PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE, 
                  gc_gen->_machine_page_size_bytes, gc_gen->aux_pool);  
  }
  assert(max_heap_size > min_heap_size_bytes);
  gc_gen->reserved_heap_size = max_heap_size;
  gc_gen->heap_start = reserved_base;
  gc_gen->heap_end = (void*)((unsigned int)reserved_base + max_heap_size);
  gc_gen->blocks = (Block*)reserved_base;
  gc_gen->num_collections = 0;

  /* heuristic nos + mos + LOS */
  unsigned int los_size = max_heap_size >> 2;
  gc_los_initialize(gc_gen, reserved_base, los_size);

  unsigned int mos_size = max_heap_size >> 1;
  reserved_base = (void*)((unsigned int)reserved_base + los_size);
  gc_mos_initialize(gc_gen, reserved_base, mos_size);
  
  unsigned int nos_size; 
  if(NOS_SIZE){
    assert( NOS_SIZE>=min_nos_size_bytes && NOS_SIZE<=max_nos_size_bytes);
    nos_size = NOS_SIZE;  
  }else
    nos_size =  max_heap_size >> 4;
  
  if(nos_size < min_nos_size_bytes ) nos_size = min_nos_size_bytes;  
  if(nos_size > max_nos_size_bytes ) nos_size = max_nos_size_bytes;  
  
  reserved_base = (void*)((unsigned int)reserved_base + mos_size);
  gc_nos_initialize(gc_gen, reserved_base, nos_size); 

  /* connect mos and nos, so that they can be compacted as one space */
  Blocked_Space* mos = (Blocked_Space*)gc_get_mos(gc_gen);
  Blocked_Space* nos = (Blocked_Space*)gc_get_nos(gc_gen);
  Block_Header* mos_last_block = (Block_Header*)&mos->blocks[mos->num_managed_blocks-1];
  Block_Header* nos_first_block = (Block_Header*)&nos->blocks[0];
  mos_last_block->next = nos_first_block;
  assert(space_heap_end((Space*)mos) == space_heap_start((Space*)nos));
    
  gc_gen->committed_heap_size = space_committed_size((Space*)gc_gen->nos) +
                                space_committed_size((Space*)gc_gen->mos) +
                                space_committed_size((Space*)gc_gen->los);
  
  set_native_finalizer_thread_flag(TRUE);
  set_native_ref_enqueue_thread_flag(TRUE);
  
  return;
}

void gc_gen_destruct(GC_Gen *gc_gen) 
{
  gc_nos_destruct(gc_gen);
  gc_gen->nos = NULL;
  
  gc_mos_destruct(gc_gen);  
  gc_gen->mos = NULL;

  gc_los_destruct(gc_gen);  
  gc_gen->los = NULL;

  return;  
}

void* mos_alloc(unsigned size, Allocator *allocator){return mspace_alloc(size, allocator);}
void* nos_alloc(unsigned size, Allocator *allocator){return fspace_alloc(size, allocator);}
void* los_alloc(unsigned size, Allocator *allocator){return lspace_alloc(size, allocator);}
Space* gc_get_nos(GC_Gen* gc){ return (Space*)gc->nos;}
Space* gc_get_mos(GC_Gen* gc){ return (Space*)gc->mos;}
Space* gc_get_los(GC_Gen* gc){ return (Space*)gc->los;}
void gc_set_nos(GC_Gen* gc, Space* nos){ gc->nos = (Fspace*)nos;}
void gc_set_mos(GC_Gen* gc, Space* mos){ gc->mos = (Mspace*)mos;}
void gc_set_los(GC_Gen* gc, Space* los){ gc->los = (Lspace*)los;}
unsigned int gc_get_processor_num(GC_Gen* gc){ return gc->_num_processors;}

static Boolean major_collection_needed(GC_Gen* gc)
{
  return mspace_free_memory_size(gc->mos) < fspace_used_memory_size(gc->nos);  
}

unsigned int gc_decide_collection_kind(GC_Gen* gc, unsigned int cause)
{
  if(major_collection_needed(gc) || cause== GC_CAUSE_LOS_IS_FULL)
    return  MAJOR_COLLECTION;
    
  return MINOR_COLLECTION;     
}

void gc_gen_reclaim_heap(GC_Gen* gc)
{  
  if(gc->collect_kind == MINOR_COLLECTION){
    if( gc_requires_barriers()) /* normal gen gc nos collection */
      fspace_collection(gc->nos);
    else{ /* copy nos to mos for non-gen gc */
      /* we don't move mos objects in MINOR_COLLECTION. This is true for both 
        gen or non-gen collections, but only meaningful for non-gen GC, because
        non-gen GC need mark the heap in order to find the refs from mos/los to nos.
        This can save lots of reloc table space for slots having ref pointing to mos.
        For gen GC, MINOR_COLLECTION doesn't really mark the heap. It has remsets that
        have all the refs from mos/los to nos, which are actually the same thing as reloc table */
      gc->mos->move_object = FALSE;
      fspace_collection(gc->nos);
      gc->mos->move_object = TRUE;
      
      /* these are only needed for non-gen MINOR_COLLECTION, because 
        both mos and los will be collected (and reset) in MAJOR_COLLECTION */
      reset_mspace_after_copy_nursery(gc->mos);
      reset_lspace_after_copy_nursery(gc->los);
    }
  }else{
    /* process mos and nos together in one compaction */
    mspace_collection(gc->mos); /* fspace collection is included */
    lspace_collection(gc->los);
  }
  
  return;
}
