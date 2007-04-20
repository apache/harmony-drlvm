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
#include <math.h>

struct GC_Gen;
struct Mspace;
struct Lspace;
Space* gc_get_mos(GC_Gen* gc);
Space* gc_get_nos(GC_Gen* gc);
Space* gc_get_los(GC_Gen* gc);
POINTER_SIZE_INT mspace_get_expected_threshold(Mspace* mspace);
POINTER_SIZE_INT lspace_get_failure_size(Lspace* lspace);
    
/*Prepare the paramenters which are to be used to compute new los size.*/
void gc_space_tune_prepare(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION))
  	return;
  
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);  
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);  
  Space_Tuner* tuner = gc->tuner;

  assert(fspace->free_block_idx >= fspace->first_block_idx);
  unsigned int nos_alloc_size = (fspace->free_block_idx - fspace->first_block_idx) * GC_BLOCK_SIZE_BYTES;
  fspace->alloced_size = nos_alloc_size;
  /*Fixme: LOS_Adaptive: There should be a condition here, that fspace->collection_num != 0*/
  mspace->alloced_size += (unsigned int)((float)nos_alloc_size * fspace->survive_ratio);
  /*For_statistic alloc speed: Speed could be represented by sum of alloced size.*/
  tuner->speed_los += lspace->alloced_size;
  tuner->speed_los = (tuner->speed_los + tuner->old_speed_los) >> 1;
  tuner->speed_mos += mspace->alloced_size;
  tuner->speed_mos = (tuner->speed_mos + tuner->old_speed_mos) >> 1;
  
  /*For_statistic wasted memory*/
  POINTER_SIZE_INT curr_used_los = lspace->surviving_size + lspace->alloced_size;
  assert(curr_used_los <= lspace->committed_heap_size);
  POINTER_SIZE_INT curr_wast_los = lspace->committed_heap_size - curr_used_los;
  tuner->wast_los += curr_wast_los;
  POINTER_SIZE_INT curr_used_mos = mspace->surviving_size + mspace->alloced_size;
  POINTER_SIZE_INT expected_mos = mspace_get_expected_threshold((Mspace*)mspace);
  POINTER_SIZE_INT curr_wast_mos = 0;
  if(expected_mos > curr_used_mos)
    curr_wast_mos = expected_mos - curr_used_mos;
  tuner->wast_mos += curr_wast_mos;
  tuner->current_dw = ABS_DIFF(tuner->wast_mos, tuner->wast_los);

  /*For_statistic ds in heuristic*/
  tuner->current_ds = (unsigned int)((float)fspace->committed_heap_size * fspace->survive_ratio);
  /*Fixme: Threshold should be computed by heuristic. tslow, total recycled heap size shold be statistic.*/
  tuner->threshold = tuner->current_ds;
  if(tuner->threshold > 8 * MB) tuner->threshold = 8 * MB;
  tuner->min_tuning_size = tuner->current_ds;
  if(tuner->min_tuning_size > 4 * MB) tuner->min_tuning_size = 4 * MB;  
  return;
}

/*Check the tuning size, if too small, cancle the tuning.*/
void check_space_tuner(GC* gc)
{
	POINTER_SIZE_INT los_fail_sz_uped = 0;
	
  Space_Tuner* tuner = gc->tuner;
  if((!tuner->need_tune) && (!tuner->force_tune)){
    assert(tuner->kind == TRANS_NOTHING);
    assert(tuner->tuning_size == 0);
    return;
  }
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);
  if((!tuner->force_tune) && (tuner->tuning_size < tuner->min_tuning_size)){
    tuner->tuning_size = 0;
    goto check_size;
  }
  if((tuner->need_tune) && (!tuner->force_tune)) goto check_size;
  /*tuner->force_tune must be true here!*/
  los_fail_sz_uped = lspace_get_failure_size((Lspace*)lspace);
  assert(!(los_fail_sz_uped % KB));

  if(tuner->kind == TRANS_FROM_LOS_TO_MOS){
    tuner->kind = TRANS_FROM_MOS_TO_LOS;
    tuner->tuning_size = 0;
    lspace->move_object = 0;
  }
  if(tuner->tuning_size < los_fail_sz_uped){
    tuner->tuning_size = los_fail_sz_uped;
  }
  
check_size:
  tuner->tuning_size = round_down_to_size(tuner->tuning_size, GC_BLOCK_SIZE_BYTES);
  if(tuner->tuning_size == 0){
    tuner->kind = TRANS_NOTHING;
    lspace->move_object = 0;
  }
  
  return;
}


extern POINTER_SIZE_INT min_los_size_bytes;
extern POINTER_SIZE_INT min_none_los_size_bytes;
/*Give the tuning kind, and tuning size hint*/
void gc_space_tune_before_gc(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION)) return;
  Space_Tuner* tuner = gc->tuner;

  /*Needn't tune if dw does not reach threshold.*/  
  if(tuner->current_dw > tuner->threshold)  tuner->need_tune = 1;
  /*If LOS is full, we should tune at lease "tuner->least_tuning_size" size*/
  if(gc->cause == GC_CAUSE_LOS_IS_FULL) tuner->force_tune = 1;
  if((!tuner->need_tune) && (!tuner->force_tune)) return;

  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);

  POINTER_SIZE_INT los_expect_survive_sz = (POINTER_SIZE_INT)((float)(lspace->surviving_size + lspace->alloced_size) * lspace->survive_ratio);
  POINTER_SIZE_INT los_expect_free_sz = lspace->committed_heap_size - los_expect_survive_sz;
  POINTER_SIZE_INT mos_expect_survive_sz = (POINTER_SIZE_INT)((float)(mspace->surviving_size + mspace->alloced_size) * mspace->survive_ratio);
  POINTER_SIZE_INT mos_expect_free_sz = mspace_get_expected_threshold((Mspace*)mspace) - mos_expect_survive_sz;
  POINTER_SIZE_INT total_free = los_expect_free_sz + mos_expect_free_sz;
  float new_los_ratio = (float)tuner->speed_los / (float)(tuner->speed_los  + tuner->speed_mos);
  POINTER_SIZE_INT new_free_los_sz = (POINTER_SIZE_INT)((float)total_free * new_los_ratio);
  POINTER_SIZE_INT max_tuning_size = 0;
  /*LOS_Extend:*/
  if((new_free_los_sz > los_expect_free_sz) )
  { 
    if ( (!tuner->force_tune) && (new_free_los_sz - los_expect_free_sz < tuner->min_tuning_size) ){
      tuner->kind = TRANS_NOTHING;
      tuner->tuning_size = 0;
      return;
    }
    tuner->kind = TRANS_FROM_MOS_TO_LOS;
    tuner->tuning_size = round_down_to_size(new_free_los_sz - los_expect_free_sz, GC_BLOCK_SIZE_BYTES);
    POINTER_SIZE_INT non_los_sz = mspace->committed_heap_size + fspace->committed_heap_size;
    if(non_los_sz > min_none_los_size_bytes)
      max_tuning_size = non_los_sz - min_none_los_size_bytes;
    if(tuner->tuning_size > max_tuning_size) tuner->tuning_size = max_tuning_size;    
  }
  /*LOS_Shrink:*/
  if((new_free_los_sz < los_expect_free_sz))
  {
    if ( (!tuner->force_tune) && (los_expect_free_sz - new_free_los_sz < tuner->min_tuning_size) ){
      tuner->kind = TRANS_NOTHING;
      tuner->tuning_size = 0;
      return;
    }
    tuner->kind = TRANS_FROM_LOS_TO_MOS;
    lspace->move_object = 1;
    assert(lspace->committed_heap_size >= min_los_size_bytes);
    max_tuning_size = lspace->committed_heap_size - min_los_size_bytes;
    POINTER_SIZE_INT tuning_size = los_expect_free_sz - new_free_los_sz;
    if(tuning_size > max_tuning_size) tuning_size = max_tuning_size;
    tuner->tuning_size = round_down_to_size(tuning_size, GC_BLOCK_SIZE_BYTES);
  }
  if( (tuner->tuning_size == 0) && (!tuner->force_tune) ){
    tuner->kind = TRANS_NOTHING;
    lspace->move_object = 0;
    return;
  }
  check_space_tuner(gc);
  return;
}

void gc_space_tune_before_gc_fixed_size(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION)) return;
  Space_Tuner* tuner = gc->tuner;
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);

  if(cause == GC_CAUSE_LOS_IS_FULL){
    tuner->kind = TRANS_FROM_MOS_TO_LOS;
    POINTER_SIZE_INT los_fail_sz = lspace_get_failure_size((Lspace*)lspace);
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
    POINTER_SIZE_INT none_los_size;
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
  else
  /*LOS_Shrink: Fixme: Very simple strategy now. */
  {
    return;
    tuner->kind = TRANS_FROM_LOS_TO_MOS;
    lspace->move_object = TRUE;
    tuner->tuning_size = GC_LOS_MIN_VARY_SIZE >> 1;
  }
  
  /*Fixme: Should MOS heap_start must be 64k aligned?*/
  tuner->tuning_size = round_up_to_size(tuner->tuning_size, GC_BLOCK_SIZE_BYTES);
  if(tuner->tuning_size == 0){
    tuner->kind = TRANS_NOTHING;
    lspace->move_object = 0;
  }

  return;  
}

#include "../thread/collector.h"
#include "../mark_sweep/lspace.h"
Boolean gc_space_retune(GC *gc)
{
  Lspace *los = (Lspace*)gc_get_los((GC_Gen*)gc);
  Space_Tuner* tuner = gc->tuner;
  /*LOS_Extend:*/
  if(tuner->kind == TRANS_FROM_MOS_TO_LOS){
    POINTER_SIZE_INT non_los_live_obj_size = 0;
    unsigned int collector_num = gc->num_active_collectors;
    for(unsigned int i = collector_num; i--;){
      Collector *collector = gc->collectors[i];
      non_los_live_obj_size += collector->non_los_live_obj_size;
    }
    non_los_live_obj_size += GC_BLOCK_SIZE_BYTES * collector_num * 4;
    non_los_live_obj_size = round_up_to_size(non_los_live_obj_size, GC_BLOCK_SIZE_BYTES);
    POINTER_SIZE_INT max_free_for_tuning = 0;
    if (gc->committed_heap_size > los->committed_heap_size + non_los_live_obj_size)
      max_free_for_tuning = gc->committed_heap_size - los->committed_heap_size - non_los_live_obj_size;

    if(!tuner->force_tune){
    /*This should not happen! If GC is not issued by los, then it's not necessary to extend it*/
      if(max_free_for_tuning < tuner->tuning_size)
        tuner->tuning_size = max_free_for_tuning;
      if(tuner->tuning_size == 0){
        tuner->kind = TRANS_NOTHING;
        los->move_object = 0;
      }
      return TRUE;
    }
    /*force tune here!*/
    POINTER_SIZE_INT min_tuning_uped = round_up_to_size(los->failure_size, GC_BLOCK_SIZE_BYTES);
    if(min_tuning_uped > max_free_for_tuning){
      tuner->tuning_size = 0;
      tuner->kind = TRANS_NOTHING;
      los->move_object = 0;
      return FALSE;
    }
    if(tuner->tuning_size < min_tuning_uped){
      assert(tuner->tuning_size < max_free_for_tuning);
      tuner->tuning_size = min_tuning_uped;
      return TRUE;
    }else/*tuner->tuning_size >= min_tuning_uped*/{
      if(tuner->tuning_size > max_free_for_tuning)
        tuner->tuning_size = max_free_for_tuning;
      return TRUE;
    }
  }
  else// if(gc->tuner->kind == TRANS_FROM_LOS_TO_MOS)
  {
    POINTER_SIZE_INT los_live_obj_size = 0;
    unsigned int collector_num = gc->num_active_collectors;
    for(unsigned int i = collector_num; i--;){
      Collector *collector = gc->collectors[i];
      los_live_obj_size += collector->los_live_obj_size;
    }
    los_live_obj_size = round_up_to_size(los_live_obj_size, GC_BLOCK_SIZE_BYTES);
    los_live_obj_size += (collector_num << 2 << GC_BLOCK_SHIFT_COUNT);
    
    Lspace *los = (Lspace*)gc_get_los((GC_Gen*)gc);
    Space_Tuner *tuner = gc->tuner;
    POINTER_SIZE_INT los_max_shrink_size = 0;
    if(los->committed_heap_size > los_live_obj_size)
      los_max_shrink_size = los->committed_heap_size - los_live_obj_size;
    if(tuner->tuning_size > los_max_shrink_size) 
      tuner->tuning_size = los_max_shrink_size;
    assert(!(tuner->tuning_size % GC_BLOCK_SIZE_BYTES));
    if(tuner->tuning_size == 0){
      tuner->kind = TRANS_NOTHING;
      los->move_object = 0;
      return TRUE;
    }else 
      return TRUE;
  }
}

void  gc_space_tuner_reset(GC* gc)
{
  if( !gc_match_kind(gc, MINOR_COLLECTION)){
    Space_Tuner* tuner = gc->tuner;
    POINTER_SIZE_INT old_slos = tuner->speed_los;
    POINTER_SIZE_INT old_smos = tuner->speed_mos;
    memset(tuner, 0, sizeof(Space_Tuner));
    tuner->old_speed_los = old_slos;
    tuner->old_speed_mos = old_smos;
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
