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
float mspace_get_expected_threshold_ratio(Mspace* mspace);
POINTER_SIZE_INT lspace_get_failure_size(Lspace* lspace);
    
void gc_decide_space_tune(GC* gc, unsigned int cause)
{
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);  
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);  
  Space_Tuner* tuner = gc->tuner;
  //debug_adjust
  assert(fspace->free_block_idx >= fspace->first_block_idx);
  unsigned int nos_alloc_size = (fspace->free_block_idx - fspace->first_block_idx) * GC_BLOCK_SIZE_BYTES;
  fspace->alloced_size = nos_alloc_size;
  /*Fixme: LOS_Adaptive: There should be a condition here, that fspace->collection_num != 0*/
  mspace->alloced_size += (unsigned int)((float)nos_alloc_size * fspace->survive_ratio);
  /*For_statistic alloc speed: Speed could be represented by sum of alloced size.
   *The right of this time los/mos alloc speed is the biggest.
   */
  tuner->speed_los = lspace->alloced_size;
  tuner->speed_los = (tuner->speed_los + tuner->old_speed_los) >> 1;
  tuner->speed_mos = mspace->alloced_size;
  tuner->speed_mos = (tuner->speed_mos + tuner->old_speed_mos) >> 1;
  
  /*For_statistic wasted memory*/
  POINTER_SIZE_INT curr_used_los = lspace->surviving_size + lspace->alloced_size;
  POINTER_SIZE_INT curr_wast_los = 0;
  if(gc->cause != GC_CAUSE_LOS_IS_FULL) curr_wast_los =  lspace->committed_heap_size - curr_used_los;
  tuner->wast_los += curr_wast_los;
  
  POINTER_SIZE_INT curr_used_mos = mspace->surviving_size + mspace->alloced_size;
  float expected_mos_ratio = mspace_get_expected_threshold_ratio((Mspace*)mspace);
  POINTER_SIZE_INT expected_mos = (POINTER_SIZE_INT)((mspace->committed_heap_size + fspace->committed_heap_size) * expected_mos_ratio);
  POINTER_SIZE_INT curr_wast_mos = 0;
  if(expected_mos > curr_used_mos) curr_wast_mos = expected_mos - curr_used_mos;
  tuner->wast_mos += curr_wast_mos;
  tuner->current_dw = ABS_DIFF(tuner->wast_mos, tuner->wast_los);

  /*For_statistic ds in heuristic*/
  tuner->current_ds = (unsigned int)((float)fspace->committed_heap_size * fspace->survive_ratio);
  /*Fixme: Threshold should be computed by heuristic. tslow, total recycled heap size shold be statistic.*/
  tuner->threshold_waste = tuner->current_ds;
  if(tuner->threshold_waste > 8 * MB) tuner->threshold_waste = 8 * MB;
  tuner->min_tuning_size = tuner->current_ds;
  if(tuner->min_tuning_size > 4 * MB) tuner->min_tuning_size = 4 * MB;  

  if(tuner->speed_los == 0) tuner->speed_los = 16;
  if(tuner->speed_mos == 0) tuner->speed_mos = 16;

  /*Needn't tune if dw does not reach threshold.*/  
  if(tuner->current_dw > tuner->threshold_waste)  tuner->need_tune = 1;
  /*If LOS is full, we should tune at lease "tuner->least_tuning_size" size*/
  if(gc->cause == GC_CAUSE_LOS_IS_FULL) tuner->force_tune = 1;

  return;
}

extern POINTER_SIZE_INT min_los_size_bytes;
extern POINTER_SIZE_INT min_none_los_size_bytes;

void gc_compute_space_tune_size_before_marking(GC* gc, unsigned int cause)
{
  if(gc_match_kind(gc, MINOR_COLLECTION))  return;
  
  gc_decide_space_tune(gc, cause);
  
  Space_Tuner* tuner = gc->tuner;
  if((tuner->speed_los == 0) && ( tuner->speed_mos == 0)) return;
  if((!tuner->need_tune) && (!tuner->force_tune)) return;
  
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);
  Space* lspace = (Space*)gc_get_los((GC_Gen*)gc);

  POINTER_SIZE_INT los_expect_surviving_sz = (POINTER_SIZE_INT)((float)(lspace->surviving_size + lspace->alloced_size) * lspace->survive_ratio);
  POINTER_SIZE_INT los_expect_free_sz = ((lspace->committed_heap_size > los_expect_surviving_sz) ? 
                                                            (lspace->committed_heap_size - los_expect_surviving_sz) : 0);
  POINTER_SIZE_INT mos_expect_survive_sz = (POINTER_SIZE_INT)((float)(mspace->surviving_size + mspace->alloced_size) * mspace->survive_ratio);
  float mos_expect_threshold_ratio = mspace_get_expected_threshold_ratio((Mspace*)mspace);
  POINTER_SIZE_INT mos_expect_threshold = (POINTER_SIZE_INT)((mspace->committed_heap_size + fspace->committed_heap_size) * mos_expect_threshold_ratio);
  POINTER_SIZE_INT mos_expect_free_sz = ((mos_expect_threshold > mos_expect_survive_sz)?
                                                            (mos_expect_threshold - mos_expect_survive_sz) : 0);
  POINTER_SIZE_INT total_expect_free_sz = los_expect_free_sz + mos_expect_free_sz;

  float new_los_ratio = (float)tuner->speed_los / (float)(tuner->speed_los  + tuner->speed_mos);
  POINTER_SIZE_INT new_free_los_sz = (POINTER_SIZE_INT)((float)total_expect_free_sz * new_los_ratio);

  /*LOS_Extend:*/
  if((new_free_los_sz > los_expect_free_sz) )
  { 
    tuner->kind = TRANS_FROM_MOS_TO_LOS;
    tuner->tuning_size = new_free_los_sz - los_expect_free_sz;
    lspace->move_object = 0;
  }
  /*LOS_Shrink:*/
  else if(new_free_los_sz < los_expect_free_sz)
  {
    tuner->kind = TRANS_FROM_LOS_TO_MOS;
    tuner->tuning_size = los_expect_free_sz - new_free_los_sz;
    lspace->move_object = 1;
  }
  /*Nothing*/
  else
  {    
    tuner->tuning_size = 0;
  }

  /*If not force tune, and the tuning size is too small, tuner will not take effect.*/
  if( (!tuner->force_tune) && (tuner->tuning_size < tuner->min_tuning_size) ){
    tuner->kind = TRANS_NOTHING;
    tuner->tuning_size = 0;
    lspace->move_object = 0;
  }

  /*If los or non-los is already the smallest size, there is no need to tune anymore.
   *But we give "force tune" a chance to extend the whole heap size down there.
   */
  if(((lspace->committed_heap_size <= min_los_size_bytes) && (tuner->kind == TRANS_FROM_LOS_TO_MOS)) ||
      ((fspace->committed_heap_size + mspace->committed_heap_size <= min_none_los_size_bytes) && (tuner->kind == TRANS_FROM_MOS_TO_LOS))){
    assert((lspace->committed_heap_size == min_los_size_bytes) || (fspace->committed_heap_size + mspace->committed_heap_size == min_none_los_size_bytes));
    tuner->kind = TRANS_NOTHING;
    tuner->tuning_size = 0;
    lspace->move_object = 0;
  }
  
  if(tuner->force_tune){
    if(tuner->kind != TRANS_FROM_MOS_TO_LOS){
      tuner->kind = TRANS_FROM_MOS_TO_LOS;
      tuner->tuning_size = 0;
      tuner->reverse_1 = 1;
    }
  }

  return;
}

#include "../thread/collector.h"
#include "../los/lspace.h"

static POINTER_SIZE_INT non_los_live_obj_size;
static  POINTER_SIZE_INT los_live_obj_size;
static void gc_compute_live_object_size_after_marking(GC* gc, POINTER_SIZE_INT non_los_size)
{
  non_los_live_obj_size = 0;
  los_live_obj_size = 0;
  
  POINTER_SIZE_INT segment_live_size[NORMAL_SIZE_SEGMENT_NUM];
  memset(segment_live_size, 0, sizeof(POINTER_SIZE_INT) * NORMAL_SIZE_SEGMENT_NUM);

  unsigned int collector_num = gc->num_active_collectors;
  for(unsigned int i = collector_num; i--;){
    Collector *collector = gc->collectors[i];
    non_los_live_obj_size += collector->non_los_live_obj_size;
    los_live_obj_size += collector->los_live_obj_size;
    for(unsigned int j = NORMAL_SIZE_SEGMENT_NUM; j--;){
      segment_live_size[j] += collector->segment_live_size[j];
    }
    memset(collector->segment_live_size, 0, sizeof(POINTER_SIZE_INT) * NORMAL_SIZE_SEGMENT_NUM);
  }
  
  //POINTER_SIZE_INT additional_non_los_size = ((collector_num * 2) << GC_BLOCK_SHIFT_COUNT) + (non_los_live_obj_size >> GC_BLOCK_SHIFT_COUNT) * (GC_OBJ_SIZE_THRESHOLD/4);
  double additional_non_los_size = 0;
  for(unsigned int i = NORMAL_SIZE_SEGMENT_NUM; i--;){
    additional_non_los_size += (double)segment_live_size[i] * SEGMENT_INDEX_TO_SIZE(i) / non_los_live_obj_size;
  }
  additional_non_los_size *= 1.2; // in case of some cases worse than average one
  POINTER_SIZE_INT non_los_live_block = non_los_live_obj_size / (GC_BLOCK_BODY_SIZE_BYTES-(POINTER_SIZE_INT)additional_non_los_size);
  additional_non_los_size *= non_los_live_block + 1;
  additional_non_los_size += collector_num << (GC_BLOCK_SHIFT_COUNT + 1);
  non_los_live_obj_size = round_up_to_size(non_los_live_obj_size + (POINTER_SIZE_INT)additional_non_los_size, GC_BLOCK_SIZE_BYTES);
  if(non_los_live_obj_size > non_los_size)
    non_los_live_obj_size = non_los_size;

  los_live_obj_size += ((collector_num << 2) << GC_BLOCK_SHIFT_COUNT);
  los_live_obj_size = round_up_to_size(los_live_obj_size, GC_BLOCK_SIZE_BYTES);

}

static void compute_space_tune_size_for_force_tune(GC *gc, POINTER_SIZE_INT max_tune_for_min_non_los)
{
  Space_Tuner* tuner = gc->tuner;
  Lspace *lspace = (Lspace*)gc_get_los((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);  
  POINTER_SIZE_INT max_tuning_size = 0;
  POINTER_SIZE_INT failure_size = lspace->failure_size;
  POINTER_SIZE_INT lspace_free_size = ( (lspace->committed_heap_size > los_live_obj_size) ? (lspace->committed_heap_size - los_live_obj_size) : (0) );
  //debug_adjust
  assert(!(lspace_free_size % KB));
  assert(!(failure_size % KB));

  if(lspace_free_size >= failure_size){
    tuner->tuning_size = 0;
    tuner->kind = TRANS_NOTHING;
    lspace->move_object = 1;
  }else{
    tuner->tuning_size = failure_size -lspace_free_size;
    
    /*We should assure that the tuning size is no more than the free space of non_los area*/
    if( gc->committed_heap_size > lspace->committed_heap_size + non_los_live_obj_size )
      max_tuning_size = gc->committed_heap_size - lspace->committed_heap_size - non_los_live_obj_size;

    if(max_tuning_size > max_tune_for_min_non_los)
      max_tuning_size = max_tune_for_min_non_los;

    /*Round up to satisfy LOS alloc demand.*/
    tuner->tuning_size = round_up_to_size(tuner->tuning_size, GC_BLOCK_SIZE_BYTES);
    max_tuning_size = round_down_to_size(max_tuning_size, GC_BLOCK_SIZE_BYTES);

    /*If the tuning size is too large, we did nothing and wait for the OOM of JVM*/
    /*Fixme: if the heap size is not mx, we can extend the whole heap size*/
    if(tuner->tuning_size > max_tuning_size){
      tuner->tuning_size = round_up_to_size(tuner->tuning_size, SPACE_ALLOC_UNIT);
      max_tuning_size = round_down_to_size(max_tuning_size, SPACE_ALLOC_UNIT);
        //debug_adjust
      assert(max_heap_size_bytes >= gc->committed_heap_size);
      POINTER_SIZE_INT extend_heap_size = 0;
      POINTER_SIZE_INT potential_max_tuning_size = max_tuning_size + max_heap_size_bytes - gc->committed_heap_size;
      potential_max_tuning_size -= LOS_HEAD_RESERVE_FOR_HEAP_NULL;

      //debug_adjust
      assert(!(potential_max_tuning_size % SPACE_ALLOC_UNIT));
      if(tuner->tuning_size > potential_max_tuning_size){
        tuner->tuning_size = 0;
        tuner->kind = TRANS_NOTHING;
        lspace->move_object = 0;      
      }else{
        /*We have tuner->tuning_size > max_tuning_size up there.*/
        extend_heap_size = tuner->tuning_size - max_tuning_size;
        blocked_space_extend(fspace, (unsigned int)extend_heap_size);
        gc->committed_heap_size += extend_heap_size;
        tuner->kind = TRANS_FROM_MOS_TO_LOS;
        lspace->move_object = 1;        
      }
    } else{
      tuner->kind = TRANS_FROM_MOS_TO_LOS;
      lspace->move_object = 1;
    }
  }

  return;
}

static void make_sure_tuning_size(GC* gc)
{
  Space_Tuner* tuner = gc->tuner;
  Lspace *lspace = (Lspace*)gc_get_los((GC_Gen*)gc);
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);

  POINTER_SIZE_INT los_free_sz =  ((lspace->committed_heap_size > los_live_obj_size) ? 
                                                   (lspace->committed_heap_size - los_live_obj_size) : 0);
  float mos_expect_threshold_ratio = mspace_get_expected_threshold_ratio((Mspace*)mspace);
  POINTER_SIZE_INT mos_expect_threshold = (POINTER_SIZE_INT)((mspace->committed_heap_size + fspace->committed_heap_size) * mos_expect_threshold_ratio);
  POINTER_SIZE_INT mos_free_sz = ((mos_expect_threshold > non_los_live_obj_size)?
                                                            (mos_expect_threshold - non_los_live_obj_size) : 0);
  POINTER_SIZE_INT total_free_sz = los_free_sz + mos_free_sz;

  float new_los_ratio = (float)tuner->speed_los / (float)(tuner->speed_los  + tuner->speed_mos);
  POINTER_SIZE_INT new_free_los_sz = (POINTER_SIZE_INT)((float)total_free_sz * new_los_ratio);

  /*LOS_Extend:*/
  if((new_free_los_sz > los_free_sz) )
  { 
    tuner->kind = TRANS_FROM_MOS_TO_LOS;
    tuner->tuning_size = new_free_los_sz - los_free_sz;
    lspace->move_object = 0; //This is necessary, because the flag might be set by gc_compute_space_tune_size_before_marking.
  }
  /*LOS_Shrink:*/
  else if(new_free_los_sz < los_free_sz)
  {
    tuner->kind = TRANS_FROM_LOS_TO_MOS;
    tuner->tuning_size = los_free_sz - new_free_los_sz;
    lspace->move_object = 1;
  }
  /*Nothing*/
  else
  {
    tuner->tuning_size = 0;
    tuner->kind = TRANS_NOTHING;//This is necessary, because the original value of kind might not be NOTHING.
  }

  /*If not force tune, and the tuning size is too small, tuner will not take effect.*/
  if( (!tuner->force_tune) && (tuner->tuning_size < tuner->min_tuning_size) ){
    tuner->kind = TRANS_NOTHING;
    tuner->tuning_size = 0;
    lspace->move_object = 0;
  }

  /*If los or non-los is already the smallest size, there is no need to tune anymore.
   *But we give "force tune" a chance to extend the whole heap size down there.
   */
  if(((lspace->committed_heap_size <= min_los_size_bytes) && (tuner->kind == TRANS_FROM_LOS_TO_MOS)) ||
      ((fspace->committed_heap_size + mspace->committed_heap_size <= min_none_los_size_bytes) && (tuner->kind == TRANS_FROM_MOS_TO_LOS))){
    assert((lspace->committed_heap_size == min_los_size_bytes) || (fspace->committed_heap_size + mspace->committed_heap_size == min_none_los_size_bytes));
    tuner->kind = TRANS_NOTHING;
    tuner->tuning_size = 0;
    lspace->move_object = 0;
  }
  
  if(tuner->force_tune){
    if(tuner->kind != TRANS_FROM_MOS_TO_LOS){
      tuner->kind = TRANS_FROM_MOS_TO_LOS;
      tuner->reverse_2 = 1;
    }
  }

  return;  
}

void gc_compute_space_tune_size_after_marking(GC *gc)
{
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Blocked_Space* fspace = (Blocked_Space*)gc_get_nos((GC_Gen*)gc);
  Lspace *lspace = (Lspace*)gc_get_los((GC_Gen*)gc);
  Space_Tuner* tuner = gc->tuner;

  POINTER_SIZE_INT max_tuning_size = 0;  
  POINTER_SIZE_INT non_los_size = mspace->committed_heap_size + fspace->committed_heap_size;

  gc_compute_live_object_size_after_marking(gc, non_los_size);

  make_sure_tuning_size(gc);
  
  /*We should assure that the non_los area is no less than min_none_los_size_bytes*/
  POINTER_SIZE_INT max_tune_for_min_non_los = 0;
  if(non_los_size > min_none_los_size_bytes)
    max_tune_for_min_non_los = non_los_size - min_none_los_size_bytes;
  POINTER_SIZE_INT max_tune_for_min_los = 0;
  //debug_adjust
  assert(lspace->committed_heap_size >= min_los_size_bytes);
  max_tune_for_min_los = lspace->committed_heap_size - min_los_size_bytes;

  /*Not force tune, LOS_Extend:*/
  if(tuner->kind == TRANS_FROM_MOS_TO_LOS)
  {
    if (gc->committed_heap_size > lspace->committed_heap_size + non_los_live_obj_size){
      max_tuning_size = gc->committed_heap_size - lspace->committed_heap_size - non_los_live_obj_size;
      if(max_tuning_size > max_tune_for_min_non_los)
        max_tuning_size = max_tune_for_min_non_los;
      if( tuner->tuning_size > max_tuning_size)
        tuner->tuning_size = max_tuning_size;
      /*Round down so as not to break max_tuning_size*/
      tuner->tuning_size = round_down_to_size(tuner->tuning_size, GC_BLOCK_SIZE_BYTES);
      if(tuner->tuning_size == 0){
        //If tuning size is zero, we should reset kind to NOTHING, in case that gc_init_block_for_collectors relink the block list.
        tuner->kind = TRANS_NOTHING;
        lspace->move_object = 0;
      }
    }else{ 
      tuner->tuning_size = 0;
      tuner->kind = TRANS_NOTHING;
      lspace->move_object = 0;
    }
  }
  /*Not force tune, LOS Shrink*/
  else
  {    
    if(lspace->committed_heap_size > los_live_obj_size){
      max_tuning_size = lspace->committed_heap_size - los_live_obj_size;
      if(max_tuning_size > max_tune_for_min_los)
        max_tuning_size = max_tune_for_min_los;
      if(tuner->tuning_size > max_tuning_size) 
        tuner->tuning_size = max_tuning_size;
      /*Round down so as not to break max_tuning_size*/
      tuner->tuning_size = round_down_to_size(tuner->tuning_size, GC_BLOCK_SIZE_BYTES);
      if(tuner->tuning_size == 0){
        tuner->kind = TRANS_NOTHING;
        lspace->move_object = 0;
      }
    }else{
      /* this is possible because of the reservation in gc_compute_live_object_size_after_marking*/        
      tuner->tuning_size = 0;
      tuner->kind = TRANS_NOTHING;
      lspace->move_object = 0;
    }
  }

  /*If the tuning strategy give a bigger tuning_size than failure size, we just follow the strategy and set noforce.*/
  Boolean doforce = TRUE;
  POINTER_SIZE_INT failure_size = lspace_get_failure_size((Lspace*)lspace);  
  if( (tuner->kind == TRANS_FROM_MOS_TO_LOS) && (!tuner->reverse_2) && (tuner->tuning_size > failure_size) )
    doforce = FALSE;

  /*If force tune*/
  if( (tuner->force_tune) && (doforce) ){
    compute_space_tune_size_for_force_tune(gc, max_tune_for_min_non_los);
  }

  return;
  
}

void  gc_space_tuner_reset(GC* gc)
{
  Space_Tuner* tuner = gc->tuner;
  if( !gc_match_kind(gc, MINOR_COLLECTION)){
    /*Clear the fields every major collection except the wast area statistic.*/
    tuner->tuning_size = 0;
    tuner->interim_blocks = NULL;
    tuner->need_tune = FALSE;
    tuner->force_tune = FALSE;

    tuner->old_speed_los = tuner->speed_los;
    tuner->old_speed_mos = tuner->speed_mos;
    tuner->speed_los = 0;
    tuner->speed_mos = 0;

    tuner->current_dw  = 0;
    tuner->current_ds = 0;

    tuner->threshold_waste = 0;
    tuner->min_tuning_size = 0;
    /*Reset the sum up of wast area size only if los is changed.*/
    if(tuner->kind != TRANS_NOTHING){
      tuner->wast_los = 0;
      tuner->wast_mos = 0;
    }
    tuner->kind = TRANS_NOTHING;    
    tuner->reverse_1 = 0;
    tuner->reverse_2 = 0;
  }
  
  return;  
  
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

/*Malloc and initialize fake blocks for LOS_Shrink*/
void gc_space_tuner_init_fake_blocks_for_los_shrink(GC* gc)
{
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  Space_Tuner* tuner = gc->tuner;
  Block_Header* mos_first_block = (Block_Header*)&mspace->blocks[0];
  unsigned int trans_blocks = (unsigned int)(tuner->tuning_size >> GC_BLOCK_SHIFT_COUNT);
  tuner->interim_blocks = (Block_Header*)STD_MALLOC(trans_blocks * sizeof(Block_Header));
  Block_Header* los_trans_fake_blocks = tuner->interim_blocks;
  memset(los_trans_fake_blocks, 0, trans_blocks * sizeof(Block_Header));
  void* trans_base = (void*)((POINTER_SIZE_INT)mos_first_block - tuner->tuning_size);
  unsigned int start_idx = GC_BLOCK_INDEX_FROM(gc->heap_start, trans_base);
  Block_Header* last_block = los_trans_fake_blocks;

  for(unsigned int i = 0; i < trans_blocks; i ++){
      Block_Header* curr_block = &los_trans_fake_blocks[i];
      curr_block->block_idx = start_idx + i;
      curr_block->base = (void*)((POINTER_SIZE_INT)trans_base + i * GC_BLOCK_SIZE_BYTES + GC_BLOCK_HEADER_SIZE_BYTES);
      curr_block->free = curr_block->base ;
      curr_block->new_free = curr_block->free;
      curr_block->ceiling = (void*)((POINTER_SIZE_INT)curr_block->base + GC_BLOCK_BODY_SIZE_BYTES);
      curr_block->status = BLOCK_COMPACTED;
#ifdef USE_32BITS_HASHCODE
      curr_block->hashcode_buf = hashcode_buf_create();
#endif
      last_block->next = curr_block;
      last_block = curr_block;
  }
  last_block->next = mos_first_block;
}

/*Copy the fake blocks into real blocks, reconnect these new block into main list of mspace.
  *Free the fake blocks. The infomation of mspace is not updated yet.
 */
void gc_space_tuner_release_fake_blocks_for_los_shrink(GC* gc)
{
  Space_Tuner *tuner = gc->tuner;
  Blocked_Space* mspace = (Blocked_Space*)gc_get_mos((GC_Gen*)gc);
  
  POINTER_SIZE_INT tune_size = tuner->tuning_size;
  unsigned int tune_blocks = (unsigned int)(tune_size >> GC_BLOCK_SHIFT_COUNT);

  Block* blocks = (Block*)((POINTER_SIZE_INT)mspace->blocks - tune_size);
  Block_Header* last_real_block = (Block_Header*)blocks;
  unsigned int i;
  for(i=0; i < tune_blocks; i++){
    Block_Header* real_block = (Block_Header*)&(blocks[i]);
    Block_Header* fake_block = &tuner->interim_blocks[i];
    memcpy((void*)real_block, (void*)fake_block, sizeof(Block_Header));
    last_real_block->next = real_block;
    last_real_block = real_block;
  }
  last_real_block->next = (Block_Header*)mspace->blocks;
  STD_FREE(tuner->interim_blocks);
  return;
}


