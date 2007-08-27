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

#include "gc_common.h"
#include "gc_metadata.h"
#include "object_status.h"

void identify_dead_weak_roots(GC *gc, Pool *pool)
{
  pool_iterator_init(pool);
  while(Vector_Block *block = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(block);
    for(; !vector_block_iterator_end(block, iter); iter = vector_block_iterator_advance(block, iter)){
      Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)*iter;
      Partial_Reveal_Object *p_obj = *p_ref;
      if(!p_obj){  // reference has been cleared
        continue;
      }
      if(IS_FALLBACK_COMPACTION) {
          if(obj_belongs_to_nos(p_obj) && obj_is_fw_in_oi(p_obj)){
             //this is unreachable for VTable->jlc(p_obj), but needed by general weak roots
             assert(!obj_is_marked_in_vt(p_obj));
             assert(obj_get_vt(p_obj) == obj_get_vt(obj_get_fw_in_oi(p_obj)));
             p_obj = obj_get_fw_in_oi(p_obj);
             assert(p_obj);
             *p_ref = p_obj;
          }
      }
      if(gc_obj_is_dead(gc, p_obj))
        *p_ref = 0; 
    }
  }
}

extern Boolean IS_MOVE_COMPACT;

/* parameter pointer_addr_in_pool means it is p_ref or p_obj in pool */
void gc_update_weak_roots_pool(GC *gc)
{
  GC_Metadata* metadata = gc->metadata;
  Pool *pool = metadata->weak_roots_pool;
  Partial_Reveal_Object** p_ref;
  Partial_Reveal_Object *p_obj;
  
  pool_iterator_init(pool);
  while(Vector_Block *repset = pool_iterator_next(pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(repset);
    for(; !vector_block_iterator_end(repset,iter); iter = vector_block_iterator_advance(repset,iter)){
      p_ref = (Partial_Reveal_Object**)*iter;
      p_obj = *p_ref;
      if(!p_obj){  // reference has been cleared
        continue;
      }

      if(obj_need_move(gc, p_obj))  {
        if(!IS_MOVE_COMPACT){
          assert((POINTER_SIZE_INT)obj_get_fw_in_oi(p_obj) > DUAL_MARKBITS);
          *p_ref = obj_get_fw_in_oi(p_obj);
        } else {
          assert(space_of_addr(gc, (void*)p_obj)->move_object);
          *p_ref = ref_to_obj_ptr(obj_get_fw_in_table(p_obj));
        }
      }
    }
  }
}
