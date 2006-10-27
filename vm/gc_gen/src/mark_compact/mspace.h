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

#include "../common/gc_block.h"
#include "../thread/thread_alloc.h"

/* Mark-compaction space is orgnized into blocks*/
typedef struct Mspace{
  /* <-- first couple of fields are overloadded as Space */
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  GC* gc;
  Boolean move_object;
  Boolean (*mark_object_func)(Mspace* space, Partial_Reveal_Object* p_obj);
  void (*save_reloc_func)(Mspace* space, Partial_Reveal_Object** p_ref);
  void (*update_reloc_func)(Mspace* space);
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
    
}Mspace;

void mspace_initialize(GC* gc, void* reserved_base, unsigned int mspace_size);
void mspace_destruct(Mspace* mspace);

inline Boolean mspace_has_free_block(Mspace* mspace){ return mspace->free_block_idx <= mspace->ceiling_block_idx; }
inline unsigned int mspace_free_memory_size(Mspace* mspace){ return GC_BLOCK_SIZE_BYTES * (mspace->ceiling_block_idx - mspace->free_block_idx + 1);  }
inline Boolean mspace_used_memory_size(Mspace* mspace){ return GC_BLOCK_SIZE_BYTES * mspace->num_used_blocks; }

void* mspace_alloc(unsigned size, Allocator *allocator);
void mspace_collection(Mspace* mspace);

void reset_mspace_after_copy_nursery(Mspace* mspace);


Boolean mspace_mark_object(Mspace* mspace, Partial_Reveal_Object *p_obj);
void mspace_save_reloc(Mspace* mspace, Partial_Reveal_Object** p_ref);
void mspace_update_reloc(Mspace* mspace);

#endif //#ifdef _MSC_SPACE_H_
