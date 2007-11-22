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

#ifndef _OBJECT_STATUS_H_
#define _OBJECT_STATUS_H_

#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "../mark_sweep/sspace_mark_sweep.h"


inline Boolean obj_is_dead_in_gen_minor_gc(Partial_Reveal_Object *p_obj)
{
  /*
   * The first condition is for supporting switch between nongen and gen minor collection.
   * With this kind of switch dead objects in MOS & LOS may be set the mark or fw bit in oi.
   * The second condition is for supporting partially forwarding NOS.
   * In partially forwarding situation live objects in the non-forwarding half NOS will only be marked but not forwarded.
   */
  return obj_belongs_to_nos(p_obj) && !obj_is_marked_or_fw_in_oi(p_obj);
}

inline Boolean obj_is_dead_in_nongen_minor_gc(Partial_Reveal_Object *p_obj)
{
  return (obj_belongs_to_nos(p_obj) && !obj_is_fw_in_oi(p_obj))
          || (!obj_belongs_to_nos(p_obj) && !obj_is_marked_in_oi(p_obj));
}

/* The caller must be in places where alloc color and mark color haven't been flipped */
inline Boolean obj_is_dead_in_sweep_major_gc(Partial_Reveal_Object *p_obj)
{
  return (obj_belongs_to_nos(p_obj) && !obj_is_marked_in_vt(p_obj))
          || (!obj_belongs_to_nos(p_obj) && !obj_is_mark_black_in_table(p_obj));
}

inline Boolean obj_is_dead_in_compact_major_gc(Partial_Reveal_Object *p_obj)
{
  return !obj_is_marked_in_vt(p_obj);
}

#ifdef USE_MARK_SWEEP_GC
inline Boolean obj_is_dead_in_mark_sweep_gc(Partial_Reveal_Object *p_obj)
{
  return !obj_is_mark_black_in_table(p_obj);
}
#endif

inline Boolean gc_obj_is_dead(GC *gc, Partial_Reveal_Object *p_obj)
{
  assert(p_obj);

#ifdef USE_MARK_SWEEP_GC
  return obj_is_dead_in_mark_sweep_gc(p_obj);
#endif

  if(gc_match_kind(gc, MINOR_COLLECTION)){
    if(gc_is_gen_mode())
      return obj_is_dead_in_gen_minor_gc(p_obj);
    else
      return obj_is_dead_in_nongen_minor_gc(p_obj);
  } else if(gc_get_mos((GC_Gen*)gc)->collect_algorithm == MAJOR_MARK_SWEEP){
    return obj_is_dead_in_sweep_major_gc(p_obj);
  } else {
    return obj_is_dead_in_compact_major_gc(p_obj);
  }
}

inline Boolean fspace_obj_to_be_forwarded(Partial_Reveal_Object *p_obj)
{
  if(!obj_belongs_to_nos(p_obj)) return FALSE;
  return forward_first_half ? (p_obj < object_forwarding_boundary) : (p_obj>=object_forwarding_boundary);
}
inline Boolean obj_need_move(GC *gc, Partial_Reveal_Object *p_obj)
{
  /* assert(!gc_obj_is_dead(gc, p_obj)); commented out for weakroot */

#ifdef USE_MARK_SWEEP_GC
  Sspace *sspace = gc_ms_get_sspace((GC_MS*)gc);
  return sspace->move_object;
#endif

  if(gc_is_gen_mode() && gc_match_kind(gc, MINOR_COLLECTION))
    return fspace_obj_to_be_forwarded(p_obj);
  
  Space *space = space_of_addr(gc, p_obj);
  return space->move_object;
}


#endif /* _OBJECT_STATUS_H_ */
