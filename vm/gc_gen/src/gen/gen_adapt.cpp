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

/**
 * @author Xiao-Feng Li, 2006/10/05
 */

#include "gen.h"
#include "../common/space_tuner.h"
#include <math.h>

#define NOS_COPY_RESERVE_DELTA (GC_BLOCK_SIZE_BYTES<<1)
/*Tune this number in case that MOS could be too small, so as to avoid or put off fall back.*/
#define GC_MOS_MIN_EXTRA_REMAIN_SIZE (36*MB)

struct Mspace;
void mspace_set_expected_threshold_ratio(Mspace* mspace, float threshold_ratio);

static float Tslow = 0.0f;
static POINTER_SIZE_INT SMax = 0;
static POINTER_SIZE_INT last_total_free_size = 0;

typedef struct Gen_Mode_Adaptor{
  float gen_minor_throughput;
  float nongen_minor_throughput;

  /*for obtaining the gen minor collection throughput.*/
  int gen_mode_trial_count;

  float major_survive_ratio_threshold;
  unsigned int major_repeat_count;

  POINTER_SIZE_INT adapt_nos_size;
}Gen_Mode_Adaptor;

void gc_gen_mode_adapt_init(GC_Gen *gc)
{
  gc->gen_mode_adaptor = (Gen_Mode_Adaptor*)STD_MALLOC( sizeof(Gen_Mode_Adaptor));
  Gen_Mode_Adaptor* gen_mode_adaptor = gc->gen_mode_adaptor;
  
  gen_mode_adaptor->gen_minor_throughput = 0.0f;
  /*reset the nongen_minor_throughput: the first default nongen minor (maybe testgc)may caused the result
  calculated to be zero. so we initial the value to 1.0f here. */
  gen_mode_adaptor->nongen_minor_throughput = 1.0f;
  gen_mode_adaptor->gen_mode_trial_count = 0;

  gen_mode_adaptor->major_survive_ratio_threshold = 1.0f;
  gen_mode_adaptor->major_repeat_count  = 1;

  gen_mode_adaptor->adapt_nos_size = min_nos_size_bytes;
}

static float mini_free_ratio(float k, float m)
{
  /*fixme: the check should be proved!*/
  if(m < 0.005f) m = 0.005f;
  if(k > 100.f) k = 100.f;
  
  float b = - (2 + 2 * k * m);
  float c = k * m * m + 2 * m + 1;
  float D = b * b - 4 * c;
  if (D <= 0) {
    //printf("output 0.8f from k: %5.3f, m: %5.3f\n", k, m);
    return 0.8f;
  }
  float pm = sqrt (D) / 2 ;
  float base = - b / 2 ;
  float res = base - pm;
  if (res > 1.f) res = 0.8f;

  /*fixme: the check should be proved!*/
  if (res < 0.0f) res = 0.8f;

  //printf("output %5.3f from k: %5.3f, m: %5.3f\n", res, k, m);
  return res;
}

#define MAX_MAJOR_REPEAT_COUNT 3
#define MAX_MINOR_TRIAL_COUNT 2
#define MAX_INT32 0x7fffffff

void gc_gen_mode_adapt(GC_Gen* gc, int64 pause_time)
{
  if(GEN_NONGEN_SWITCH == FALSE) return;
  
  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  Gen_Mode_Adaptor* gen_mode_adaptor = gc->gen_mode_adaptor;

  POINTER_SIZE_INT mos_free_size = blocked_space_free_mem_size(mspace);
  POINTER_SIZE_INT nos_free_size = blocked_space_free_mem_size(fspace);
  POINTER_SIZE_INT total_free_size = mos_free_size  + nos_free_size;
  
  if(gc_match_kind((GC*)gc, MAJOR_COLLECTION)) {
    assert(!gc_is_gen_mode());
    
    if(gen_mode_adaptor->major_survive_ratio_threshold != 0 && mspace->survive_ratio > gen_mode_adaptor->major_survive_ratio_threshold){    
      if(gen_mode_adaptor->major_repeat_count > MAX_MAJOR_REPEAT_COUNT ){
        gc->force_gen_mode = TRUE;
        gc_enable_gen_mode();
        gc->force_major_collect = FALSE;
        return;
      }else{
        gen_mode_adaptor->major_repeat_count++;
      }
    }else{
      gen_mode_adaptor->major_repeat_count = 1;
    }
    
  }else{
    /*compute throughput*/
    if(gc->last_collect_kind != MINOR_COLLECTION){
      gen_mode_adaptor->nongen_minor_throughput = 1.0f;
    }
    if(gc->force_gen_mode){
      if(pause_time!=0){
        if(gen_mode_adaptor->gen_minor_throughput != 0)
          gen_mode_adaptor->gen_minor_throughput = (gen_mode_adaptor->gen_minor_throughput + (float) nos_free_size/(float)pause_time)/2.0f;
        else
          gen_mode_adaptor->gen_minor_throughput =(float) nos_free_size/(float)pause_time;
      }
    }else{
      if(pause_time!=0){
        if(gen_mode_adaptor->gen_minor_throughput != 1.0f)
          gen_mode_adaptor->nongen_minor_throughput = (gen_mode_adaptor->nongen_minor_throughput + (float) nos_free_size/(float)pause_time)/2.0f;      
        else
          gen_mode_adaptor->nongen_minor_throughput = (float) nos_free_size/(float)pause_time;
      }
   }

    if(gen_mode_adaptor->nongen_minor_throughput <=  gen_mode_adaptor->gen_minor_throughput ){
      if( gc->last_collect_kind != MINOR_COLLECTION ){
        gen_mode_adaptor->major_survive_ratio_threshold = mspace->survive_ratio;
      }else if( !gc->force_gen_mode ){
        gc->force_gen_mode = TRUE;
        gen_mode_adaptor->gen_mode_trial_count = MAX_INT32;        
      } 
    }

    if(gc->force_major_collect && !gc->force_gen_mode){
        gc->force_major_collect = FALSE;
        gc->force_gen_mode = TRUE;
        gen_mode_adaptor->gen_mode_trial_count = 2;
    }else if(gc->last_collect_kind != MINOR_COLLECTION && gc->force_gen_mode){
       gen_mode_adaptor->gen_mode_trial_count = MAX_INT32;
    }

    if(gc->force_gen_mode && (total_free_size <= ((float)min_nos_size_bytes) * 1.3 )){
        gc->force_gen_mode = FALSE;
        gc_disable_gen_mode();
        gc->force_major_collect = TRUE;
        gen_mode_adaptor->gen_mode_trial_count = 0;
        return;
    }
    
    if( gc->force_gen_mode ){
      assert( gen_mode_adaptor->gen_mode_trial_count >= 0);

      gen_mode_adaptor->gen_mode_trial_count --;
      if( gen_mode_adaptor->gen_mode_trial_count >= 0){
        gc_enable_gen_mode();
        return;
      }
          
      gc->force_gen_mode = FALSE;
      gc->force_major_collect = TRUE;    
      gen_mode_adaptor->gen_mode_trial_count = 0;
    }
  }
  
  gc_disable_gen_mode();
  return;
}

void mspace_set_expected_threshold_ratio(Mspace* mspace, float threshold_ratio);

static void gc_decide_next_collect(GC_Gen* gc, int64 pause_time)
{
  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;

  float survive_ratio = 0.2f;

  POINTER_SIZE_INT mos_free_size = blocked_space_free_mem_size(mspace);
  POINTER_SIZE_INT nos_free_size = blocked_space_free_mem_size(fspace);
  assert(nos_free_size == space_committed_size((Space*)fspace));
  POINTER_SIZE_INT total_free_size = mos_free_size  + nos_free_size;
  if(gc_match_kind((GC*)gc, MAJOR_COLLECTION)) gc->force_gen_mode = FALSE;
  if(!gc->force_gen_mode){
    /*Major collection:*/
    if(gc_match_kind((GC*)gc, MAJOR_COLLECTION)){
      mspace->time_collections += pause_time;
  
      Tslow = (float)pause_time;
      SMax = total_free_size;
      /*If fall back happens, and nos_boundary is up to heap_ceiling, then we force major.*/
      if(((Fspace*)gc->nos)->num_managed_blocks == 0)
        gc->force_major_collect = TRUE;
      else gc->force_major_collect = FALSE;
      
      /*If major is caused by LOS, or collection kind is EXTEND_COLLECTION, all survive ratio is not updated.*/
      if((gc->cause != GC_CAUSE_LOS_IS_FULL) && (!gc_match_kind((GC*)gc, EXTEND_COLLECTION))){
        survive_ratio = (float)mspace->period_surviving_size/(float)mspace->committed_heap_size;
        mspace->survive_ratio = survive_ratio;
      }
      /*If there is no minor collection at all, we must give mspace expected threshold a reasonable value.*/
      if((gc->tuner->kind != TRANS_NOTHING) && (fspace->num_collections == 0))
        mspace_set_expected_threshold_ratio((Mspace *)mspace, 0.5f);
      /*If this major is caused by fall back compaction, we must give fspace->survive_ratio 
        *a conservative and reasonable number to avoid next fall back.
        *In fallback compaction, the survive_ratio of mspace must be 1.*/
      if(gc_match_kind((GC*)gc, FALLBACK_COLLECTION)) fspace->survive_ratio = 1;

    }
    /*Minor collection:*/    
    else
    {
      /*Give a hint to mini_free_ratio. */
      if(fspace->num_collections == 1){
        /*Fixme: This is only set for tuning the first warehouse!*/
        Tslow = pause_time / gc->survive_ratio;
        SMax = (POINTER_SIZE_INT)((float)(gc->committed_heap_size - gc->los->committed_heap_size) * ( 1 - gc->survive_ratio ));
        last_total_free_size = gc->committed_heap_size - gc->los->committed_heap_size;
      }
  
      fspace->time_collections += pause_time;  
      POINTER_SIZE_INT free_size_threshold;

      POINTER_SIZE_INT minor_surviving_size = last_total_free_size - total_free_size;
      /*If the first GC is caused by LOS, mspace->last_alloced_size should be smaller than this minor_surviving_size
        *Because the last_total_free_size is not accurate.*/
      if(fspace->num_collections != 1) assert(minor_surviving_size == mspace->last_alloced_size);
  
      float k = Tslow * fspace->num_collections/fspace->time_collections;
      float m = ((float)minor_surviving_size)*1.0f/((float)(SMax - GC_MOS_MIN_EXTRA_REMAIN_SIZE ));
      float free_ratio_threshold = mini_free_ratio(k, m);

      if(SMax > GC_MOS_MIN_EXTRA_REMAIN_SIZE)
        free_size_threshold = (POINTER_SIZE_INT)(free_ratio_threshold * (SMax - GC_MOS_MIN_EXTRA_REMAIN_SIZE ) + GC_MOS_MIN_EXTRA_REMAIN_SIZE );
      else
        free_size_threshold = (POINTER_SIZE_INT)(free_ratio_threshold * SMax);

/*Fixme: if the total free size is lesser than threshold, the time point might be too late!
 *Have a try to test whether the backup solution is better for specjbb.
 */
//   if ((mos_free_size + nos_free_size + minor_surviving_size) < free_size_threshold) gc->force_major_collect = TRUE;  
      if ((mos_free_size + nos_free_size)< free_size_threshold) gc->force_major_collect = TRUE;
  
      survive_ratio = (float)minor_surviving_size/(float)space_committed_size((Space*)fspace);
      fspace->survive_ratio = survive_ratio;
      /*For LOS_Adaptive*/
      POINTER_SIZE_INT mspace_committed_size = space_committed_size((Space*)mspace);
      POINTER_SIZE_INT fspace_committed_size = space_committed_size((Space*)fspace);
      if(mspace_committed_size  + fspace_committed_size > free_size_threshold){
        POINTER_SIZE_INT mspace_size_threshold;
        mspace_size_threshold = mspace_committed_size  + fspace_committed_size - free_size_threshold;
        float mspace_size_threshold_ratio = (float)mspace_size_threshold / (mspace_committed_size  + fspace_committed_size);
        mspace_set_expected_threshold_ratio((Mspace *)mspace, mspace_size_threshold_ratio);
      }
    }
  
    gc->survive_ratio =  (gc->survive_ratio + survive_ratio)/2.0f;
    last_total_free_size = total_free_size;
  }

  gc_gen_mode_adapt(gc,pause_time);

  return;
}


Boolean gc_compute_new_space_size(GC_Gen* gc, POINTER_SIZE_INT* mos_size, POINTER_SIZE_INT* nos_size)
{
  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  Blocked_Space* lspace = (Blocked_Space*)gc->los;  
  
  POINTER_SIZE_INT new_nos_size;
  POINTER_SIZE_INT new_mos_size;

  POINTER_SIZE_INT curr_nos_size = space_committed_size((Space*)fspace);
  POINTER_SIZE_INT used_mos_size = blocked_space_used_mem_size(mspace);

  POINTER_SIZE_INT total_size;

#ifdef STATIC_NOS_MAPPING
    total_size = max_heap_size_bytes - lspace->committed_heap_size;
#else
    POINTER_SIZE_INT curr_heap_commit_end = 
                              (POINTER_SIZE_INT)gc->heap_start + LOS_HEAD_RESERVE_FOR_HEAP_NULL + gc->committed_heap_size;
    assert(curr_heap_commit_end > (POINTER_SIZE_INT)mspace->heap_start);
    total_size = curr_heap_commit_end - (POINTER_SIZE_INT)mspace->heap_start;
#endif
  assert(total_size >= used_mos_size);
  POINTER_SIZE_INT total_free = total_size - used_mos_size;
  /*If total free is smaller than one block, there is no room for us to adjust*/
  if(total_free < GC_BLOCK_SIZE_BYTES)  return FALSE;

  /* predict NOS + NOS*ratio = total_free_size */
  POINTER_SIZE_INT nos_reserve_size;
  nos_reserve_size = (POINTER_SIZE_INT)(((float)total_free)/(1.0f + fspace->survive_ratio));
  /*NOS should not be zero, if there is only one block in non-los, i.e. in the former if sentence,
    *if total_free = GC_BLOCK_SIZE_BYTES, then the computed nos_reserve_size is between zero
    *and GC_BLOCK_SIZE_BYTES. In this case, we assign this block to NOS*/
  if(nos_reserve_size <= GC_BLOCK_SIZE_BYTES)  nos_reserve_size = GC_BLOCK_SIZE_BYTES;

#ifdef STATIC_NOS_MAPPING
  if(nos_reserve_size > fspace->reserved_heap_size) nos_reserve_size = fspace->reserved_heap_size;
#endif  
  /*To reserve some MOS space to avoid fallback situation. 
   *But we need ensure nos has at least one block */
  POINTER_SIZE_INT reserve_in_mos = GC_MOS_MIN_EXTRA_REMAIN_SIZE;
  while (reserve_in_mos >= GC_BLOCK_SIZE_BYTES){
    if(nos_reserve_size >= reserve_in_mos + GC_BLOCK_SIZE_BYTES){
      nos_reserve_size -= reserve_in_mos;    
      break;
    }
    reserve_in_mos >>= 1;
  }

  new_nos_size = round_down_to_size((POINTER_SIZE_INT)nos_reserve_size, GC_BLOCK_SIZE_BYTES); 

  if(gc->force_gen_mode){
    new_nos_size = min_nos_size_bytes;
  }
  
  new_mos_size = total_size - new_nos_size;
#ifdef STATIC_NOS_MAPPING
  if(new_mos_size > mspace->reserved_heap_size) new_mos_size = mspace->reserved_heap_size;
#endif
  assert(new_nos_size + new_mos_size == total_size);
  *nos_size = new_nos_size;
  *mos_size = new_mos_size;
  return TRUE;;
}

#ifndef STATIC_NOS_MAPPING
void gc_gen_adapt(GC_Gen* gc, int64 pause_time)
{
  gc_decide_next_collect(gc, pause_time);

  if(NOS_SIZE) return;

  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  
  POINTER_SIZE_INT new_nos_size;
  POINTER_SIZE_INT new_mos_size;

  Boolean result = gc_compute_new_space_size(gc, &new_mos_size, &new_nos_size);

  if(!result) return;

  POINTER_SIZE_INT curr_nos_size = space_committed_size((Space*)fspace);

  //if( ABS_DIFF(new_nos_size, curr_nos_size) < NOS_COPY_RESERVE_DELTA )
  if( new_nos_size == curr_nos_size ){
    return;
  }else if ( new_nos_size >= curr_nos_size ){
    INFO2("gc.process", "GC: gc_gen space adjustment after GC["<<gc->num_collections<<"] ...");
    POINTER_SIZE_INT adapt_size = new_nos_size - curr_nos_size;
    INFO2("gc.space", "GC: Space Adapt:  nos  --->  mos  ("
      <<verbose_print_size(adapt_size)
      <<" size was transfered from nos to mos)\n"); 
  } else {
    INFO2("gc.process", "GC: gc_gen space adjustment after GC["<<gc->num_collections<<"] ...");
    POINTER_SIZE_INT  adapt_size = curr_nos_size - new_nos_size;
    INFO2("gc.space", "GC: Space Adapt:  mos  --->  nos  ("
      <<verbose_print_size(adapt_size)
      <<" size was transfered from mos to nos)\n"); 
  }

  /* below are ajustment */  
  POINTER_SIZE_INT curr_heap_commit_end = 
                             (POINTER_SIZE_INT)gc->heap_start + LOS_HEAD_RESERVE_FOR_HEAP_NULL + gc->committed_heap_size;
  nos_boundary = (void*)(curr_heap_commit_end - new_nos_size);

  fspace->heap_start = nos_boundary;
  fspace->blocks = (Block*)nos_boundary;
  fspace->committed_heap_size = new_nos_size;
  fspace->num_managed_blocks = (unsigned int)(new_nos_size >> GC_BLOCK_SHIFT_COUNT);
  fspace->num_total_blocks = fspace->num_managed_blocks;
  fspace->first_block_idx = ((Block_Header*)nos_boundary)->block_idx;
  fspace->free_block_idx = fspace->first_block_idx;
  if( NOS_PARTIAL_FORWARD )
    object_forwarding_boundary = (void*)&fspace->blocks[fspace->num_managed_blocks >>1];
  else
    object_forwarding_boundary = (void*)&fspace->blocks[fspace->num_managed_blocks];

  mspace->heap_end = nos_boundary;
  mspace->committed_heap_size = new_mos_size;
  mspace->num_managed_blocks = (unsigned int)(new_mos_size >> GC_BLOCK_SHIFT_COUNT);
  mspace->num_total_blocks = mspace->num_managed_blocks;
  mspace->ceiling_block_idx = ((Block_Header*)nos_boundary)->block_idx - 1;

  Block_Header* mos_last_block = (Block_Header*)&mspace->blocks[mspace->num_managed_blocks-1];
  assert(mspace->ceiling_block_idx == mos_last_block->block_idx);
  Block_Header* nos_first_block = (Block_Header*)&fspace->blocks[0];
  /* this is redundant: mos_last_block->next = nos_first_block; */

  if( gc_is_gen_mode())
    HelperClass_set_NosBoundary(nos_boundary);
  
  return;
}

/* ifdef STATIC_NOS_MAPPING */
#else
void gc_gen_adapt(GC_Gen* gc, int64 pause_time)
{
  gc_decide_next_collect(gc, pause_time);

  if(NOS_SIZE) return;

  POINTER_SIZE_INT new_nos_size;
  POINTER_SIZE_INT new_mos_size;

  Boolean result = gc_compute_new_space_size(gc, &new_mos_size, &new_nos_size);

  if(!result) return;

  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  
  POINTER_SIZE_INT curr_nos_size = space_committed_size((Space*)fspace);

  //if( ABS_DIFF(new_nos_size, curr_nos_size) < NOS_COPY_RESERVE_DELTA )
  if( new_nos_size == curr_nos_size ){
    return;
  }else if ( new_nos_size >= curr_nos_size ){
    INFO2("gc.process", "GC: gc_gen space adjustment after GC["<<gc->num_collections<<"] ...\n");
    POINTER_SIZE_INT adapt_size = new_nos_size - curr_nos_size;
    INFO2("gc.space", "GC: Space Adapt:  mos  --->  nos  ("
      <<verbose_print_size(adapt_size)
      <<" size was transfered from mos to nos)\n"); 
  } else {
    INFO2("gc.process", "GC: gc_gen space adjustment after GC["<<gc->num_collections<<"] ...\n");
    POINTER_SIZE_INT  adapt_size = curr_nos_size - new_nos_size;
    INFO2("gc.space", "GC: Space Adapt:  nos  --->  mos  ("
      <<verbose_print_size(adapt_size)
      <<" size was transfered from nos to mos)\n"); 
  }
  
  POINTER_SIZE_INT used_mos_size = blocked_space_used_mem_size((Blocked_Space*)mspace);
  POINTER_SIZE_INT free_mos_size = blocked_space_free_mem_size((Blocked_Space*)mspace);

  POINTER_SIZE_INT new_free_mos_size = new_mos_size -  used_mos_size;
  
  POINTER_SIZE_INT curr_mos_end = (POINTER_SIZE_INT)&mspace->blocks[mspace->free_block_idx - mspace->first_block_idx];
  POINTER_SIZE_INT mos_border = (POINTER_SIZE_INT)mspace->heap_end;
  if(  curr_mos_end + new_free_mos_size > mos_border){
    /* we can't let mos cross border */
    new_free_mos_size = mos_border - curr_mos_end;    
  }

  if(new_nos_size < curr_nos_size){
  /* lets shrink nos */
    assert(new_free_mos_size > free_mos_size);
    blocked_space_shrink((Blocked_Space*)fspace, curr_nos_size - new_nos_size);
    blocked_space_extend((Blocked_Space*)mspace, new_free_mos_size - free_mos_size);
  }else if(new_nos_size > curr_nos_size){
    /* lets grow nos */
    assert(new_free_mos_size < free_mos_size);
    blocked_space_shrink((Blocked_Space*)mspace, free_mos_size - new_free_mos_size);
    blocked_space_extend((Blocked_Space*)fspace, new_nos_size - curr_nos_size);     
  }

  Block_Header* mos_last_block = (Block_Header*)&mspace->blocks[mspace->num_managed_blocks-1];
  Block_Header* nos_first_block = (Block_Header*)&fspace->blocks[0];
  mos_last_block->next = nos_first_block;
  
  return;
}

#endif /* STATIC_NOS_MAPPING */
