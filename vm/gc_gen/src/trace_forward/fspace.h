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

#include "../thread/thread_alloc.h"

/*
 * In our Gen GC, not all live objects are copied to tspace space, the newer baby will
 * still be preserved in  fspace, that means give them time to die.
 */
#define FORWARD_ALL 1.0
#define FORWARD_HALF 0.5

extern float NURSERY_OBJECT_FORWARDING_RATIO;
extern Boolean forward_first_half;
extern void* object_forwarding_boundary; //objects allocated before this boundary remain in fspace

typedef struct Fspace {
  /* <-- first couple of fields are overloadded as Space */
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  GC* gc;
  Boolean move_object;
  Boolean (*mark_object_func)(Fspace* space, Partial_Reveal_Object* p_obj);
  void (*save_reloc_func)(Fspace* space, Partial_Reveal_Object** p_ref);
  void (*update_reloc_func)(Fspace* space);
  /* END of Space --> */

  Block* blocks; /* short-cut for mpsace blockheader access, not mandatory */
  
  /* FIXME:: the block indices should be replaced with block header addresses */
  unsigned int first_block_idx;
  unsigned int ceiling_block_idx;
  volatile unsigned int free_block_idx;
  
  unsigned int num_used_blocks;
  unsigned int num_managed_blocks;
  unsigned int num_total_blocks;

  /* during compaction, save non-zero obj_info who's overwritten by forwarding pointer */
  ObjectMap*  obj_info_map; 
  /* END of Blocked_Space --> */
    
  /* saved remsets of collectors */
  /* saved remsets of exited mutators */
  std::vector<RemslotSet *> *remslot_sets;
  SpinLock rem_sets_lock;
  
} Fspace;

void fspace_initialize(GC* gc, void* start, unsigned int fspace_size);
void fspace_destruct(Fspace *fspace);

inline Boolean fspace_has_free_block(Fspace* fspace){ return fspace->free_block_idx <= fspace->ceiling_block_idx; }
inline unsigned int fspace_free_memory_size(Fspace* fspace){ return GC_BLOCK_SIZE_BYTES * (fspace->ceiling_block_idx - fspace->free_block_idx + 1);  }
inline Boolean fspace_used_memory_size(Fspace* fspace){ return GC_BLOCK_SIZE_BYTES * fspace->num_used_blocks; }


void* fspace_alloc(unsigned size, Allocator *allocator);

Boolean fspace_mark_object(Fspace* fspace, Partial_Reveal_Object *p_obj);
void fspace_save_reloc(Fspace* fspace, Partial_Reveal_Object** p_ref);
void fspace_update_reloc(Fspace* fspace);
void reset_fspace_for_allocation(Fspace* fspace);

inline Block_Header* fspace_get_first_copy_block(Fspace* fspace)
{  return (Block_Header*)fspace->blocks; }

inline Block_Header* fspace_get_next_copy_block(Fspace* fspace, Block_Header* block)
{  return block->next; }


Boolean fspace_compute_object_target(Collector* collector, Fspace* fspace);
void fspace_copy_collect(Collector* collector, Fspace* fspace); 

void trace_forward_fspace(Collector* collector); 
void mark_copy_fspace(Collector* collector); 

void fspace_collection(Fspace* fspace);
  
#endif // _FROM_SPACE_H_
