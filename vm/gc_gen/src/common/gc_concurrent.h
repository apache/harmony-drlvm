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
  GC_CONCURRENT_STATUS_NIL = 0x00,
  GC_CONCURRENT_MARK_PHASE = 0x01,  
  GC_CONCURRENT_MARK_FINAL_PAUSE_PHASE = 0x11, // for mostly concurrent only.
  GC_CONCURRENT_SWEEP_PHASE = 0x02
};

enum HANDSHAKE_SINGAL{
  HANDSHAKE_NIL = 0x00,
    
  /*mutator to collector*/
  ENABLE_COLLECTOR_SWEEP_LOCAL_CHUNKS = 0x01,
  DISABLE_COLLECTOR_SWEEP_LOCAL_CHUNKS = 0x02,

  
  ENABLE_COLLECTOR_SWEEP_GLOBAL_CHUNKS = 0x03,
  DISABLE_COLLECTOR_SWEEP_GLOBAL_CHUNKS = 0x04
//  /*collector to mutator*/
//  ENABLE_MUTATOR_ALLOC_BARRIER = 0x03,
//  DISABLE_MUTATOR_ALLOC_BARRIER = 0x04
};

extern Boolean USE_CONCURRENT_GC;
extern Boolean USE_CONCURRENT_ENUMERATION;
extern Boolean USE_CONCURRENT_MARK;
extern Boolean USE_CONCURRENT_SWEEP;

extern volatile Boolean concurrent_mark_phase;
extern volatile Boolean mark_is_concurrent;
extern volatile Boolean concurrent_sweep_phase;
extern volatile Boolean sweep_is_concurrent;
extern unsigned int CONCURRENT_ALGO; 

enum CONCURRENT_MARK_ALGORITHM{
  OTF_REM_OBJ_SNAPSHOT_ALGO = 0x01,
  OTF_REM_NEW_TARGET_ALGO = 0x02,
  MOSTLY_CONCURRENT_ALGO = 0x03
};

inline Boolean gc_concurrent_match_algorithm(unsigned int concurrent_algo)
{
  return CONCURRENT_ALGO == concurrent_algo;
}

inline Boolean gc_mark_is_concurrent()
{
  return mark_is_concurrent;
}

inline void gc_mark_set_concurrent()
{
  if(gc_concurrent_match_algorithm(OTF_REM_OBJ_SNAPSHOT_ALGO)
      ||gc_concurrent_match_algorithm(OTF_REM_NEW_TARGET_ALGO)) 
    gc_enable_alloc_obj_live();
  mark_is_concurrent = TRUE;
}

inline void gc_mark_unset_concurrent()
{
  gc_disenable_alloc_obj_live();
  mark_is_concurrent = FALSE;
}

inline Boolean gc_is_concurrent_mark_phase()
{
  return concurrent_mark_phase;
}

inline Boolean gc_is_concurrent_mark_phase(GC* gc)
{
  return gc->gc_concurrent_status == GC_CONCURRENT_MARK_PHASE;
}

inline Boolean gc_sweep_is_concurrent()
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

inline Boolean gc_is_concurrent_sweep_phase()
{
  return concurrent_sweep_phase;
}

inline Boolean gc_is_concurrent_sweep_phase(GC* gc)
{
  return gc->gc_concurrent_status == GC_CONCURRENT_SWEEP_PHASE;
}

inline void gc_set_concurrent_status(GC*gc, unsigned int status)
{
  /*Reset status*/
  concurrent_mark_phase = FALSE;
  concurrent_sweep_phase = FALSE;

  gc->gc_concurrent_status = status;
  switch(status){
    case GC_CONCURRENT_MARK_PHASE:  
      concurrent_mark_phase = TRUE;
      gc_mark_set_concurrent();
      break;
    case GC_CONCURRENT_SWEEP_PHASE:
      concurrent_sweep_phase = TRUE;
      gc_sweep_set_concurrent();
      break;
    default:
      assert(!concurrent_mark_phase && !concurrent_sweep_phase);
  }
  
  return;
}

void gc_reset_concurrent_mark(GC* gc);
void gc_start_concurrent_mark(GC* gc);
void gc_finish_concurrent_mark(GC* gc, Boolean is_STW);
int64 gc_get_concurrent_mark_time(GC* gc);

void gc_start_concurrent_sweep(GC* gc);
void gc_finish_concurrent_sweep(GC * gc);

void gc_reset_after_concurrent_collection(GC* gc);
void gc_check_concurrent_phase(GC * gc);

void gc_decide_concurrent_algorithm(GC* gc, char* concurrent_algo);

void gc_reset_concurrent_sweep(GC* gc);

extern volatile Boolean gc_sweeping_global_normal_chunk;

inline Boolean gc_is_sweeping_global_normal_chunk()
{ return gc_sweeping_global_normal_chunk; }

inline void gc_set_sweeping_global_normal_chunk()
{ gc_sweeping_global_normal_chunk = TRUE; }

inline void gc_unset_sweeping_global_normal_chunk()
{ gc_sweeping_global_normal_chunk = FALSE; }
#endif
