/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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

void gc_identify_dead_weak_roots(GC *gc)
{
  Pool *weakroot_pool = gc->metadata->weakroot_pool;
  
  pool_iterator_init(weakroot_pool);
  while(Vector_Block *block = pool_iterator_next(weakroot_pool)){
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
void gc_update_weak_roots(GC *gc, Boolean double_fix)
{
  GC_Metadata* metadata = gc->metadata;
  Pool *weakroot_pool = metadata->weakroot_pool;
  Partial_Reveal_Object** p_ref;
  Partial_Reveal_Object *p_obj;
  
  pool_iterator_init(weakroot_pool);
  while(Vector_Block *repset = pool_iterator_next(weakroot_pool)){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(repset);
    for(; !vector_block_iterator_end(repset,iter); iter = vector_block_iterator_advance(repset,iter)){
      p_ref = (Partial_Reveal_Object**)*iter;
      p_obj = *p_ref;
      if(!p_obj || !obj_need_move(gc, p_obj)){  // reference has been cleared or not moved
        continue;
      }

      if(IS_MOVE_COMPACT){
        assert(space_of_addr(gc, p_obj)->move_object);
        *p_ref = obj_get_fw_in_table(p_obj);
      } else if(gc_match_kind(gc, MC_COLLECTION)){
        *p_ref = obj_get_fw_in_table(p_obj);
      } else if(gc_match_kind(gc, MS_COMPACT_COLLECTION) || gc_get_mos((GC_Gen*)gc)->collect_algorithm==MAJOR_MARK_SWEEP){
        if(obj_is_fw_in_oi(p_obj)){
          p_obj = obj_get_fw_in_oi(p_obj);
          /* Only major collection in MS Gen GC might need double_fix.
           * Double fixing happens when both forwarding and compaction happen.
           */
          if(double_fix && obj_is_fw_in_oi(p_obj)){
            assert(gc_get_mos((GC_Gen*)gc)->collect_algorithm == MAJOR_MARK_SWEEP);
            p_obj = obj_get_fw_in_oi(p_obj);
            assert(address_belongs_to_gc_heap(p_obj, gc));
          }
          *p_ref = p_obj;
        }
      } else {
        assert(obj_is_fw_in_oi(p_obj));
        *p_ref = obj_get_fw_in_oi(p_obj);
      }
    }
  }
}
