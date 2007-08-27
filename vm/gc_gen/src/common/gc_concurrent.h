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
};

extern volatile Boolean concurrent_mark_phase;
extern volatile Boolean mark_is_concurrent;
extern Boolean USE_CONCURRENT_GC;

inline Boolean gc_mark_is_concurrent()
{
  return mark_is_concurrent;
}

inline void gc_mark_set_concurrent()
{
  mark_is_concurrent = TRUE;
}

inline void gc_mark_unset_concurrent()
{
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

inline void gc_set_concurrent_status(GC*gc, unsigned int status)
{
  gc->gc_concurrent_status = status;
  if(status  == GC_CONCURRENT_MARK_PHASE){
    concurrent_mark_phase = TRUE;
    gc_mark_set_concurrent();
  }else{
    concurrent_mark_phase = FALSE;
  }
}

void gc_reset_concurrent_mark(GC* gc);
void gc_start_concurrent_mark(GC* gc);
void gc_finish_concurrent_mark(GC* gc);
int64 gc_get_concurrent_mark_time(GC* gc);


#endif
