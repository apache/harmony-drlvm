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

#include "gen.h"
#include "../common/space_tuner.h"
#include <math.h>

#define NOS_COPY_RESERVE_DELTA (GC_BLOCK_SIZE_BYTES<<5)
/*Tune this number in case that MOS could be too small, so as to avoid or put off fall back.*/
#define GC_MOS_MIN_EXTRA_REMAIN_SIZE (36*MB)
/*Switch on this MACRO when we want lspace->survive_ratio to be sensitive.*/
//#define NOS_SURVIVE_RATIO_SENSITIVE

struct Mspace;
void mspace_set_expected_threshold(Mspace* mspace, POINTER_SIZE_INT threshold);

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

  POINTER_SIZE_INT mos_free_size = space_free_memory_size(mspace);
  POINTER_SIZE_INT nos_free_size = space_free_memory_size(fspace);
  POINTER_SIZE_INT total_free_size = mos_free_size  + nos_free_size;
  
  if(gc->collect_kind != MINOR_COLLECTION) {
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

void mspace_set_expected_threshold(Mspace* mspace, POINTER_SIZE_INT threshold);

static void gc_decide_next_collect(GC_Gen* gc, int64 pause_time)
{
  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;

  float survive_ratio = 0;

  POINTER_SIZE_INT mos_free_size = space_free_memory_size(mspace);
  POINTER_SIZE_INT nos_free_size = space_free_memory_size(fspace);
  POINTER_SIZE_INT total_free_size = mos_free_size  + nos_free_size;
  if(gc->collect_kind != MINOR_COLLECTION) gc->force_gen_mode = FALSE;
  if(!gc->force_gen_mode){  
    if(gc->collect_kind != MINOR_COLLECTION){
      mspace->time_collections += pause_time;
  
      Tslow = (float)pause_time;
      SMax = total_free_size;
      gc->force_major_collect = FALSE;
      
      POINTER_SIZE_INT major_survive_size = space_committed_size((Space*)mspace) - mos_free_size;
      /*If major is caused by LOS, or collection kind is EXTEND_COLLECTION, all survive ratio is not updated.*/
      if((gc->cause != GC_CAUSE_LOS_IS_FULL) && (gc->collect_kind != EXTEND_COLLECTION)){
        survive_ratio = (float)major_survive_size/(float)space_committed_size((Space*)mspace);
        mspace->survive_ratio = survive_ratio;
      }
      if(gc->tuner->kind == TRANS_FROM_MOS_TO_LOS){
        POINTER_SIZE_INT mspace_size_threshold = (space_committed_size((Space*)mspace) + space_committed_size((Space*)fspace)) >> 1;
        mspace_set_expected_threshold((Mspace *)mspace, mspace_size_threshold );
      }
  #ifdef NOS_SURVIVE_RATIO_SENSITIVE
      /*If this major is caused by fall back compaction, 
         we must give fspace->survive_ratio a conservative and reasonable number to avoid next fall back.*/
      fspace->survive_ratio = mspace->survive_ratio;
  #endif
    }else{
      /*Give a hint to mini_free_ratio. */
      if(fspace->num_collections == 1){
        /*fixme: This is only set for tuning the first warehouse!*/
        Tslow = pause_time / gc->survive_ratio;
        SMax = (POINTER_SIZE_INT)((float)gc->committed_heap_size * ( 1 - gc->survive_ratio ));
        last_total_free_size = gc->committed_heap_size - gc->los->committed_heap_size;
      }
  
      fspace->time_collections += pause_time;  
      POINTER_SIZE_INT free_size_threshold;
        
      POINTER_SIZE_INT minor_survive_size = last_total_free_size - total_free_size;
  
      float k = Tslow * fspace->num_collections/fspace->time_collections;
      float m = ((float)minor_survive_size)*1.0f/((float)(SMax - GC_MOS_MIN_EXTRA_REMAIN_SIZE ));
      float free_ratio_threshold = mini_free_ratio(k, m);
      free_size_threshold = (POINTER_SIZE_INT)(free_ratio_threshold * (SMax - GC_MOS_MIN_EXTRA_REMAIN_SIZE ) + GC_MOS_MIN_EXTRA_REMAIN_SIZE );
  
      if ((mos_free_size + nos_free_size)< free_size_threshold)  {
        gc->force_major_collect = TRUE;
      }
  
      survive_ratio = (float)minor_survive_size/(float)space_committed_size((Space*)fspace);
      fspace->survive_ratio = survive_ratio;
      /*For_LOS adaptive*/
      POINTER_SIZE_INT mspace_size_threshold = space_committed_size((Space*)mspace) + space_committed_size((Space*)fspace) - free_size_threshold;
      mspace_set_expected_threshold((Mspace *)mspace, mspace_size_threshold );
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
  POINTER_SIZE_INT used_mos_size = space_used_memory_size(mspace);
  POINTER_SIZE_INT free_mos_size = space_committed_size((Space*)mspace) - used_mos_size;

  POINTER_SIZE_INT total_size;

#ifdef STATIC_NOS_MAPPING
    total_size = max_heap_size_bytes - lspace->committed_heap_size;
#else
    total_size = (POINTER_SIZE_INT)gc->heap_end - (POINTER_SIZE_INT)mspace->heap_start;
#endif

  /* check if curr nos size is too small to shrink */
  /*
  if(curr_nos_size <= min_nos_size_bytes){
    //after major, should not allow this size 
    assert(gc->collect_kind == MINOR_COLLECTION);
    return FALSE;
  }
  */
  
  POINTER_SIZE_INT total_free = total_size - used_mos_size;
  /* predict NOS + NOS*ratio = total_free_size */
  POINTER_SIZE_INT nos_reserve_size;
  nos_reserve_size = (POINTER_SIZE_INT)(((float)total_free)/(1.0f + fspace->survive_ratio));
  new_nos_size = round_down_to_size((POINTER_SIZE_INT)nos_reserve_size, SPACE_ALLOC_UNIT);
#ifdef STATIC_NOS_MAPPING
  if(new_nos_size > fspace->reserved_heap_size) new_nos_size = fspace->reserved_heap_size;
#endif  
  if(new_nos_size > GC_MOS_MIN_EXTRA_REMAIN_SIZE) new_nos_size -= GC_MOS_MIN_EXTRA_REMAIN_SIZE ;

  if(gc->force_gen_mode){
    new_nos_size = min_nos_size_bytes;//round_down_to_size((unsigned int)(gc->gen_minor_adaptor->adapt_nos_size), SPACE_ALLOC_UNIT);
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

// this function is added to disambiguate on windows/em64t calls to asm() below 
POINTER_SIZE_SINT abs(POINTER_SIZE_SINT x) 
{
    return x<0?-x:x;    
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

  if( abs((POINTER_SIZE_SINT)new_nos_size - (POINTER_SIZE_SINT)curr_nos_size) < NOS_COPY_RESERVE_DELTA )
    return;
  
  /* below are ajustment */  

  nos_boundary = (void*)((POINTER_SIZE_INT)gc->heap_end - new_nos_size);

  fspace->heap_start = nos_boundary;
  fspace->blocks = (Block*)nos_boundary;
  fspace->committed_heap_size = new_nos_size;
  fspace->num_managed_blocks = new_nos_size >> GC_BLOCK_SHIFT_COUNT;
  fspace->num_total_blocks = fspace->num_managed_blocks;
  fspace->first_block_idx = ((Block_Header*)nos_boundary)->block_idx;
  fspace->free_block_idx = fspace->first_block_idx;

  mspace->heap_end = nos_boundary;
  mspace->committed_heap_size = new_mos_size;
  mspace->num_managed_blocks = new_mos_size >> GC_BLOCK_SHIFT_COUNT;
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

#else /* ifndef STATIC_NOS_MAPPING */

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

  if( abs((POINTER_SIZE_SINT)new_nos_size - (POINTER_SIZE_SINT)curr_nos_size) < NOS_COPY_RESERVE_DELTA )
    return;
      
  POINTER_SIZE_INT used_mos_size = space_used_memory_size((Blocked_Space*)mspace);  
  POINTER_SIZE_INT free_mos_size = space_free_memory_size((Blocked_Space*)mspace);  

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
