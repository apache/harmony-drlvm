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
    
  /* places to start and stop alloc */
  volatile void* alloc_free;
  void* alloc_ceiling;
    
  unsigned int block_size_bytes;
  
  unsigned int* mark_table; //table to keep mark_bits of fspace
  unsigned int mark_table_size;
  unsigned int* mark_table_half_boundary;
  
  /* saved remsets of collectors */
  /* saved remsets of exited mutators */
  std::vector<RemslotSet *> *remslot_sets;
  std::vector<RemobjSet *> *remobj_sets;
  SpinLock rem_sets_lock;

  /* support other space moving collection */
  SlotVector* reloc_table;
  ObjectMap*  obj_info_map; 
  
} Fspace;

void fspace_initialize(GC* gc, void* start, unsigned int fspace_size);
void fspace_destruct(Fspace *fspace);

inline unsigned int fspace_free_memory_size(Fspace* fspace)
{ return (unsigned int)fspace->alloc_ceiling - (unsigned int)fspace->alloc_free; }

inline unsigned int fspace_used_memory_size(Fspace* fspace)
{ return (unsigned int)fspace->alloc_free - (unsigned int)fspace->heap_start; }

void* fspace_alloc(unsigned size, Alloc_Context *alloc_ctx);

Boolean fspace_mark_object(Fspace* fspace, Partial_Reveal_Object *p_obj);
void fspace_clear_mark_bits(Fspace* fspace);

void fspace_save_reloc(Fspace* fspace, Partial_Reveal_Object** p_ref);
void fspace_update_reloc(Fspace* fspace);
void fspace_restore_obj_info(Fspace* fspace);
void fspace_copy_collect(Collector* collector, Fspace* fspace); 
void space_copy_remset_slots(Fspace* space, RootSet* copy_rootset);
void space_update_remsets(Fspace* space);

void fspace_collection(Fspace* fspace);
void reset_fspace_for_allocation(Fspace* fspace);

inline unsigned int fspace_allocated_size(Fspace* fspace)
{ 
  return (unsigned int)fspace->alloc_ceiling - (unsigned int)fspace->alloc_free;
}
  
inline Boolean fspace_has_free_block(Fspace* fspace)
{
  return ((POINTER_SIZE_INT)fspace->alloc_free + fspace->block_size_bytes) <= (POINTER_SIZE_INT)fspace->alloc_ceiling;  
}
#endif // _FROM_SPACE_H_
