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

#include "space_tuner.h"

#define GC_LOS_MIN_VARY_SIZE ( 2 * 1024 * 1024 ) 


struct GC_Gen;
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_nos(GC_Gen* gc);

void gc_space_tune(GC* gc, unsigned int cause)
{
  if((gc->collect_kind == MINOR_COLLECTION) || (cause != GC_CAUSE_LOS_IS_FULL) )
  	return;
  	
  Space_Tuner* tuner = gc->tuner;
  tuner->kind = TRANS_FROM_MOS_TO_LOS;

  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);

  unsigned int mos_free_block_nr = (mspace->ceiling_block_idx - mspace->free_block_idx + 1);
  unsigned int nos_used_block_nr = fspace->free_block_idx - fspace->first_block_idx;
  unsigned int mos_wast_block_nr = mos_free_block_nr - nos_used_block_nr; 
  unsigned int min_vary_block_nr =  (GC_LOS_MIN_VARY_SIZE >> GC_BLOCK_SHIFT_COUNT);
  if( mos_wast_block_nr > min_vary_block_nr ){
    tuner->tuning_size = min_vary_block_nr << GC_BLOCK_SHIFT_COUNT;
  }else{
    tuner->tuning_size = mos_wast_block_nr << GC_BLOCK_SHIFT_COUNT;
  }

  if(tuner->tuning_size == 0) tuner->kind = TRANS_NOTHING;

	return;  
}

void  gc_space_tuner_reset(GC* gc)
{
  if(gc->collect_kind != MINOR_COLLECTION){
    Space_Tuner* tuner = gc->tuner;
    memset(tuner, 0, sizeof(Space_Tuner));
  }
}

void gc_space_tuner_initialize(GC* gc)
{
    Space_Tuner* tuner = (Space_Tuner*)STD_MALLOC(sizeof(Space_Tuner));
    assert(tuner);
    memset(tuner, 0, sizeof(Space_Tuner));
    tuner->kind = TRANS_NOTHING;
    tuner->tuning_size = 0;
    gc->tuner = tuner;
}
