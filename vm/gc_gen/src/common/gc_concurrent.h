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

#ifndef _GC_CONCURRENT_H_
#define _GC_CONCURRENT_H_
#include "gc_common.h"

enum GC_CONCURRENT_STATUS{
  GC_CON_STATUS_NIL = 0x00,
  GC_CON_MARK_PHASE = 0x01,  
  GC_MOSTLY_CON_FINAL_MARK_PHASE = 0x11, // for mostly concurrent only.
  GC_CON_SWEEP_PHASE = 0x02
};

enum HANDSHAKE_SINGAL{
  HSIG_MUTATOR_SAFE = 0x0,

  HSIG_DISABLE_SWEEP_LOCAL_CHUNKS  = 0x01,
  HSIG_DISABLE_SWEEP_GLOBAL_CHUNKS = 0x02,
  HSIG_MUTATOR_ENTER_ALLOC_MARK    = 0x03,
};

inline void gc_set_con_gc(unsigned int con_phase)
{ GC_PROP |= con_phase;  }

inline void gc_specify_con_enum()
{ gc_set_con_gc(ALGO_CON_ENUM); }

inline void gc_specify_con_mark()
{ gc_set_con_gc(ALGO_CON_MARK);  }

inline void gc_specify_con_sweep()
{ gc_set_con_gc(ALGO_CON_SWEEP); }

inline BOOLEAN gc_is_specify_con_gc()
{ return (GC_PROP & ALGO_CON) != 0; }

inline BOOLEAN gc_is_specify_con_enum()
{ return (GC_PROP & ALGO_CON_ENUM) == ALGO_CON_ENUM;  }

inline BOOLEAN gc_is_specify_con_mark()
{ return (GC_PROP & ALGO_CON_MARK) == ALGO_CON_MARK;  }

inline BOOLEAN gc_is_specify_con_sweep()
{ return (GC_PROP & ALGO_CON_SWEEP) == ALGO_CON_SWEEP; }

extern volatile BOOLEAN concurrent_in_marking;
extern volatile BOOLEAN concurrent_in_sweeping;
extern volatile BOOLEAN mark_is_concurrent;
extern volatile BOOLEAN sweep_is_concurrent;

inline BOOLEAN gc_mark_is_concurrent()
{
  return mark_is_concurrent;
}

inline void gc_mark_set_concurrent()
{
  if(gc_is_kind(ALGO_CON_OTF_OBJ) || gc_is_kind(ALGO_CON_OTF_REF)) 
    gc_enable_alloc_obj_live();
  mark_is_concurrent = TRUE;
}

inline void gc_mark_unset_concurrent()
{
  gc_disable_alloc_obj_live();
  mark_is_concurrent = FALSE;
}

inline BOOLEAN gc_con_is_in_marking()
{
  return concurrent_in_marking;
}

inline BOOLEAN gc_con_is_in_marking(GC* gc)
{
  return gc->gc_concurrent_status == GC_CON_MARK_PHASE;
}

inline BOOLEAN gc_sweep_is_concurrent()
{
  return sweep_is_concurrent;
}

inline void gc_sweep_set_concurrent()
{
  sweep_is_concurrent = TRUE;
}

inline void gc_sweep_unset_concurrent()
{
  sweep_is_concurrent = FALSE;
}

inline BOOLEAN gc_con_is_in_sweeping()
{
  return concurrent_in_sweeping;
}

inline BOOLEAN gc_con_is_in_sweeping(GC* gc)
{
  return gc->gc_concurrent_status == GC_CON_SWEEP_PHASE;
}

inline void gc_set_concurrent_status(GC*gc, unsigned int status)
{
  /*Reset status*/
  concurrent_in_marking = FALSE;
  concurrent_in_sweeping = FALSE;

  gc->gc_concurrent_status = status;
  switch(status){
    case GC_CON_MARK_PHASE: 
      gc_mark_set_concurrent();
      concurrent_in_marking = TRUE;
      break;
    case GC_CON_SWEEP_PHASE:
      gc_sweep_set_concurrent();
      concurrent_in_sweeping = TRUE;
      break;
    default:
      assert(!concurrent_in_marking && !concurrent_in_sweeping);
  }
  
  return;
}

void gc_reset_con_mark(GC* gc);
void gc_start_con_mark(GC* gc);
void gc_finish_con_mark(GC* gc, BOOLEAN need_STW);
int64 gc_get_con_mark_time(GC* gc);

void gc_start_con_sweep(GC* gc);
void gc_finish_con_sweep(GC * gc);

void gc_reset_after_con_collect(GC* gc);
void gc_try_finish_con_phase(GC * gc);

void gc_decide_con_algo(char* concurrent_algo);
void gc_set_default_con_algo();

void gc_reset_con_sweep(GC* gc);

void gc_finish_con_GC(GC* gc, int64 time_mutator);

extern volatile BOOLEAN gc_sweep_global_normal_chunk;

inline BOOLEAN gc_is_sweep_global_normal_chunk()
{ return gc_sweep_global_normal_chunk; }

inline void gc_set_sweep_global_normal_chunk()
{ gc_sweep_global_normal_chunk = TRUE; }

inline void gc_unset_sweep_global_normal_chunk()
{ gc_sweep_global_normal_chunk = FALSE; }
#endif
