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

#ifndef _FROM_SPACE_H_
#define _FROM_SPACE_H_

#include "../thread/gc_thread.h"
#include "../thread/collector_alloc.h"

/*
 * In our Gen GC, not all live objects are copied to tspace space, the newer baby will
 * still be preserved in  fspace, that means to give them time to die. 
 */

extern Boolean forward_first_half;
/* boundary splitting fspace into forwarding part and remaining part */
extern void* object_forwarding_boundary; 

typedef struct Fspace {
  /* <-- first couple of fields are overloaded as Space */
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  int64 time_collections;
  float survive_ratio;
  unsigned int collect_algorithm;
  GC* gc;
  Boolean move_object;
  /* END of Space --> */

  Block* blocks; /* short-cut for mpsace blockheader access, not mandatory */
  
  /* FIXME:: the block indices should be replaced with block header addresses */
  unsigned int first_block_idx;
  unsigned int ceiling_block_idx;
  volatile unsigned int free_block_idx;
  
  unsigned int num_used_blocks;
  unsigned int num_managed_blocks;
  unsigned int num_total_blocks;
  /* END of Blocked_Space --> */
      
} Fspace;

void fspace_initialize(GC* gc, void* start, unsigned int fspace_size, unsigned int commit_size);
void fspace_destruct(Fspace *fspace);

inline Boolean obj_is_dead_in_minor_forward_gc(Collector *collector, Partial_Reveal_Object *p_obj)
{
  return (!obj_is_marked_or_fw_in_oi(p_obj)) ;
}

void* fspace_alloc(unsigned size, Allocator *allocator);

void fspace_reset_for_allocation(Fspace* fspace);

/* gen mode */
void gen_forward_pool(Collector* collector); 
void gen_forward_steal(Collector* collector);
/* nongen mode */
void nongen_slide_copy(Collector* collector); 

#ifdef MARK_BIT_FLIPPING

void nongen_forward_steal(Collector* collector); 
void nongen_forward_pool(Collector* collector); 

#endif /* MARK_BIT_FLIPPING */


void fspace_collection(Fspace* fspace);
  
#endif // _FROM_SPACE_H_
