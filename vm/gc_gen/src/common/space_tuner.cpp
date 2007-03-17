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
 * @author Ji Qi, 2006/10/05
 */

#include "space_tuner.h"

#include <math.h>

struct GC_Gen;
struct Mspace;
struct Lspace;
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);
unsigned int mspace_get_expected_threshold(Mspace* mspace);
unsigned int lspace_get_failure_size(Lspace* lspace);
    
/*Now just prepare the alloc_size field of mspace, used to compute new los size.*/
void gc_space_tune_prepare(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION))
  	return;
  
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);  
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);  
  Space_Tuner* tuner = gc->tuner;

  assert(fspace->free_block_idx > fspace->first_block_idx);
  unsigned int nos_alloc_size = (fspace->free_block_idx - fspace->first_block_idx) * GC_BLOCK_SIZE_BYTES;
  fspace->alloced_size = nos_alloc_size;
  mspace->alloced_size += (unsigned int)((float)nos_alloc_size * fspace->survive_ratio);

  /*For_statistic alloc speed: Speed could be represented by sum of alloced size.*/
  tuner->speed_los += lspace->alloced_size;
  tuner->speed_mos += mspace->alloced_size;

  /*For_statistic wasted memory*/
  unsigned int curr_used_los = lspace->surviving_size + lspace->alloced_size;
  assert(curr_used_los < lspace->committed_heap_size);
  unsigned int curr_wast_los = lspace->committed_heap_size - curr_used_los;
  tuner->wast_los += curr_wast_los;
  unsigned int curr_used_mos = mspace->surviving_size + mspace->alloced_size;
  unsigned int curr_wast_mos = mspace_get_expected_threshold((Mspace*)mspace) - curr_used_mos;
  tuner->wast_mos += curr_wast_mos;
  tuner->current_dw = abs((int)tuner->wast_mos - (int)tuner->wast_los);

  /*For_statistic ds in heuristic*/
  tuner->current_ds = (unsigned int)((float)fspace->committed_heap_size * fspace->survive_ratio);

  /*Fixme: Threshold should be computed by heuristic. tslow, total recycled heap size shold be statistic.*/
  tuner->threshold = tuner->current_ds;
  //For debug
  if(tuner->threshold > 8 * MB) tuner->threshold = 8 * MB;

  tuner->min_tuning_size = tuner->current_ds;
  //For debug
  if(tuner->min_tuning_size > 4 * MB) tuner->min_tuning_size = 4 * MB;  
}

void gc_space_tune_before_gc(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION)) return;

  Space_Tuner* tuner = gc->tuner;

  /*Only tune when LOS need extend*/  
  if( tuner->wast_los > tuner->wast_mos ) return;

  /*Needn't tune if dw does not reach threshold.*/  
  if(tuner->current_dw < tuner->threshold)  return;

  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);

  unsigned int los_expect_survive_sz = (unsigned int)((float)(lspace->surviving_size + lspace->alloced_size) * lspace->survive_ratio);
  unsigned int los_expect_free_sz = lspace->committed_heap_size - los_expect_survive_sz;
  
  unsigned int mos_expect_survive_sz = (unsigned int)((float)(mspace->surviving_size + mspace->alloced_size) * mspace->survive_ratio);
  unsigned int mos_expect_free_sz = mspace_get_expected_threshold((Mspace*)mspace) - mos_expect_survive_sz;
  
  unsigned int total_free = los_expect_free_sz + mos_expect_free_sz;

  float new_los_ratio = (float)tuner->speed_los / (float)(tuner->speed_los  + tuner->speed_mos);
  unsigned int new_free_los_sz = (unsigned int)((float)total_free * new_los_ratio);
  
  if((int)new_free_los_sz - (int)los_expect_free_sz > (int)tuner->min_tuning_size){
    tuner->kind = TRANS_FROM_MOS_TO_LOS;
    tuner->tuning_size = round_up_to_size(new_free_los_sz - los_expect_free_sz, SPACE_ALLOC_UNIT);
    tuner->least_tuning_size = round_up_to_size(lspace_get_failure_size((Lspace*)lspace), SPACE_ALLOC_UNIT);
    tuner->conservative_tuning_size = round_up_to_size(((tuner->tuning_size + tuner->least_tuning_size) >> 1), SPACE_ALLOC_UNIT);
    
     unsigned int none_los_size;
 #ifdef STATIC_NOS_MAPPING
     none_los_size = mspace->committed_heap_size;
 #else
     /*Fixme: There should be a minimal remain size like heap_size >> 3.*/
     none_los_size = mspace->committed_heap_size + fspace->committed_heap_size;
 #endif

     if(tuner->tuning_size < none_los_size) return;

     tuner->tuning_size = tuner->conservative_tuning_size;

     if(tuner->tuning_size < none_los_size) return;
        
     tuner->tuning_size = tuner->least_tuning_size;

     if((tuner->tuning_size + gc->num_active_collectors * GC_BLOCK_SIZE_BYTES) >= none_los_size){
       tuner->tuning_size = 0;
     }

     if(tuner->tuning_size == 0) tuner->kind = TRANS_NOTHING;
  }
}

void gc_space_tune_before_gc_fixed_size(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION) || (cause != GC_CAUSE_LOS_IS_FULL) )
  	 return;

  Space_Tuner* tuner = gc->tuner;
  tuner->kind = TRANS_FROM_MOS_TO_LOS;

  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);

  /*Fixme: this branch should be modified after the policy of gen major is decieded!*/
  if(false){
    unsigned int mos_free_sz = ((mspace->ceiling_block_idx - mspace->free_block_idx + 1) << GC_BLOCK_SHIFT_COUNT);
    unsigned int nos_survive_sz = 
                      (unsigned int)((float)((fspace->free_block_idx - fspace->first_block_idx) << GC_BLOCK_SHIFT_COUNT) * fspace->survive_ratio);
    int mos_wast_sz = mos_free_sz - nos_survive_sz; 
  
    if( mos_wast_sz > GC_LOS_MIN_VARY_SIZE){
      tuner->tuning_size = GC_LOS_MIN_VARY_SIZE;
    }else if(mos_wast_sz > 0){
      tuner->tuning_size = mos_wast_sz;
    }else 
      tuner->tuning_size = 0;
    
  }
  /*For non gen virable sized NOS*/
  else
  {
    unsigned int los_fail_sz = lspace_get_failure_size((Lspace*)lspace);
    
    if(los_fail_sz > GC_LOS_MIN_VARY_SIZE){
      /*Fixme: we should set the least_tuning_size after finding out the biggest free area in LOS, this number could be zero*/
      tuner->tuning_size = los_fail_sz;
      tuner->least_tuning_size = los_fail_sz;
      tuner->conservative_tuning_size = los_fail_sz;
    }else{
      tuner->tuning_size = GC_LOS_MIN_VARY_SIZE;
      tuner->least_tuning_size = los_fail_sz;         
      tuner->conservative_tuning_size = ((tuner->tuning_size + tuner->min_tuning_size) >> 1);
    }
    
    unsigned int none_los_size;
#ifdef STATIC_NOS_MAPPING
    none_los_size = mspace->committed_heap_size;
#else
    none_los_size = mspace->committed_heap_size + fspace->committed_heap_size;
#endif

    if(tuner->tuning_size > none_los_size){
      tuner->tuning_size = tuner->conservative_tuning_size;
    }
    if(tuner->tuning_size > none_los_size){
      tuner->tuning_size = tuner->least_tuning_size;
    }
    if((tuner->tuning_size + gc->num_active_collectors * GC_BLOCK_SIZE_BYTES) >= none_los_size){
      tuner->tuning_size = 0;
    }
  }
  
  /*Fixme: Should MOS heap_start must be 64k aligned?*/
  tuner->tuning_size = round_up_to_size(tuner->tuning_size, SPACE_ALLOC_UNIT);
  if(tuner->tuning_size == 0) tuner->kind = TRANS_NOTHING;

  return;  
}

void  gc_space_tuner_reset(GC* gc)
{
  if( !gc_match_kind(gc, MINOR_COLLECTION) && (gc->tuner->kind != TRANS_NOTHING)){
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
