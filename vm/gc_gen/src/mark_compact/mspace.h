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

#include "../common/gc_common.h"
#include "../thread/thread_alloc.h"

typedef struct Block_Header {
  void* base;                       
  void* free;                       
  void* ceiling;                    
  unsigned int block_idx;           
  unsigned int mark_table[1];  /* entry num == MARKBIT_TABLE_SIZE_WORDS */
}Block_Header;

enum Block_Status {
  BLOCK_NIL,
  BLOCK_FREE,
  BLOCK_IN_USE,
  BLOCK_USED
};

typedef struct Block_Info{
  Block_Header* block;
  Boolean status;

  SlotVector* reloc_table;
    
}Block_Info;

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
    
  Block_Info* block_info; /* data structure for all the blocks */
  
  volatile unsigned int num_used_blocks;
  volatile unsigned int free_block_idx;
  unsigned int compact_block_low_idx;
  unsigned int compact_block_high_idx;

  unsigned int block_size_bytes;
  unsigned int num_current_blocks;
  unsigned int num_total_blocks;

  /* during compaction, save non-zero obj_info who's overwritten by forwarding pointer */
  ObjectMap*  obj_info_map; 
    
}Mspace;

void mspace_initialize(GC* gc, void* reserved_base, unsigned int mspace_size);
void mspace_destruct(Mspace* mspace);

inline unsigned int mspace_free_memory_size(Mspace* mspace){ return GC_BLOCK_SIZE_BYTES * (mspace->num_current_blocks - mspace->num_used_blocks);  }
inline Boolean mspace_has_free_block(Mspace* mspace){ return mspace->free_block_idx < mspace->num_current_blocks; }

void* mspace_alloc(unsigned size, Alloc_Context *alloc_ctx);
void mspace_collection(Mspace* mspace);

#define GC_BLOCK_HEADER_VARS_SIZE_BYTES (unsigned int)&(((Block_Header*)0)->mark_table)

/* BlockSize - MarkbitTable*32 = HeaderVars + MarkbitTable
   => MarkbitTable = (BlockSize - HeaderVars)/33 */
#define MARKBIT_TABLE_COMPUTE_DIVISOR 33
/* +1 to round up*/
#define MARKBIT_TABLE_COMPUTED_SIZE_BYTE ((GC_BLOCK_SIZE_BYTES-GC_BLOCK_HEADER_VARS_SIZE_BYTES)/MARKBIT_TABLE_COMPUTE_DIVISOR + 1)
#define MARKBIT_TABLE_SIZE_WORDS ((MARKBIT_TABLE_COMPUTED_SIZE_BYTE + MASK_OF_BYTES_PER_WORD)&~MASK_OF_BYTES_PER_WORD)
#define MARKBIT_TABLE_SIZE_BYTES (MARKBIT_TABLE_SIZE_WORDS * BYTES_PER_WORD)

#define GC_BLOCK_HEADER_SIZE_BYTES (MARKBIT_TABLE_SIZE_BYTES + GC_BLOCK_HEADER_VARS_SIZE_BYTES)
#define GC_BLOCK_BODY_SIZE_BYTES (GC_BLOCK_SIZE_BYTES - GC_BLOCK_HEADER_SIZE_BYTES)
#define GC_BLOCK_BODY(block) ((void*)((unsigned int)block + GC_BLOCK_HEADER_SIZE_BYTES))
#define GC_BLOCK_END(block) ((void*)((unsigned int)block + GC_BLOCK_SIZE_BYTES))

#define GC_BLOCK_LOW_MASK ((unsigned int)(GC_BLOCK_SIZE_BYTES - 1))
#define GC_BLOCK_HIGH_MASK (~GC_BLOCK_LOW_MASK)
#define GC_BLOCK_HEADER(addr) ((Block_Header *)((unsigned int)addr & GC_BLOCK_HIGH_MASK))
#define GC_BLOCK_INDEX(addr) ((unsigned int)(GC_BLOCK_HEADER(addr)->block_idx))
#define GC_BLOCK_INFO_ADDRESS(mspace, addr) ((Block_Info *)&(mspace->block_info[GC_BLOCK_INDEX(addr)]))
#define GC_BLOCK_INFO_BLOCK(mspace, block) ((Block_Info *)&(mspace->block_info[block->block_idx]))

#define ADDRESS_OFFSET_TO_BLOCK_HEADER(addr) ((unsigned int)((unsigned int)addr&GC_BLOCK_LOW_MASK))
#define ADDRESS_OFFSET_IN_BLOCK_BODY(addr) ((unsigned int)(ADDRESS_OFFSET_TO_BLOCK_HEADER(addr)- GC_BLOCK_HEADER_SIZE_BYTES))

#define OBJECT_BIT_INDEX_TO_MARKBIT_TABLE(p_obj)    (ADDRESS_OFFSET_IN_BLOCK_BODY(p_obj) >> 2)
#define OBJECT_WORD_INDEX_TO_MARKBIT_TABLE(p_obj)   (OBJECT_BIT_INDEX_TO_MARKBIT_TABLE(p_obj) >> BIT_SHIFT_TO_BITS_PER_WORD)
#define OBJECT_WORD_OFFSET_IN_MARKBIT_TABLE(p_obj)  (OBJECT_BIT_INDEX_TO_MARKBIT_TABLE(p_obj) & BIT_MASK_TO_BITS_PER_WORD)

#define GC_BLOCK_OBJECT_AT_INDEX(block, idx) (block->mark_table[idx])

Boolean mspace_mark_object(Mspace* mspace, Partial_Reveal_Object *p_obj);
void mspace_save_reloc(Mspace* mspace, Partial_Reveal_Object** p_ref);
void mspace_update_reloc(Mspace* mspace);

#endif //#ifdef _MSC_SPACE_H_