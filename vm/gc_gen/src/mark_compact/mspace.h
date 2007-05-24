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

#ifndef _MSC_SPACE_H_
#define _MSC_SPACE_H_

#include "../thread/gc_thread.h"

/* Mark-compaction space is orgnized into blocks*/
typedef struct Mspace{
  /* <-- first couple of fields are overloadded as Space */
  void* heap_start;
  void* heap_end;
  POINTER_SIZE_INT reserved_heap_size;
  POINTER_SIZE_INT committed_heap_size;
  unsigned int num_collections;
  int64 time_collections;
  float survive_ratio;
  unsigned int collect_algorithm;
  GC* gc;
  Boolean move_object;
  /*Size allocted after last collection.*/
  POINTER_SIZE_INT alloced_size;
  /*For_statistic: size survived after major*/  
  POINTER_SIZE_INT surviving_size;
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
  
  volatile Block_Header* block_iterator;    
  /*Threshold computed by NOS adaptive*/
  float expected_threshold_ratio;
}Mspace;

void mspace_initialize(GC* gc, void* reserved_base, POINTER_SIZE_INT mspace_size, POINTER_SIZE_INT commit_size);
void mspace_destruct(Mspace* mspace);

void* mspace_alloc(unsigned size, Allocator *allocator);
void mspace_collection(Mspace* mspace);

void mspace_block_iterator_init(Mspace* mspace);
void mspace_block_iterator_init_free(Mspace* mspace);
Block_Header* mspace_block_iterator_next(Mspace* mspace);
Block_Header* mspace_block_iterator_get(Mspace* mspace);

void mspace_fix_after_copy_nursery(Collector* collector, Mspace* mspace);

void mspace_set_expected_threshold_ratio(Mspace* mspace, float threshold_ratio);

#endif //#ifdef _MSC_SPACE_H_
