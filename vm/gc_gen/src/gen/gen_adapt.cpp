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

#define NOS_COPY_RESERVE_DELTA (GC_BLOCK_SIZE_BYTES<<5)

#include <math.h>

static float Tslow = 0.0f;
static unsigned int SMax = 0;
static unsigned int last_total_free_size = 0;

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

#define GC_MOS_MIN_EXTRA_REMAIN_SIZE (4*1024*1024)
static void gc_decide_next_collect(GC_Gen* gc, int64 pause_time)
{
  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;

  float survive_ratio = 0;

  unsigned int mos_free_size = space_free_memory_size(mspace);
  unsigned int nos_free_size = space_free_memory_size(fspace);
  unsigned int total_free_size = mos_free_size  + nos_free_size;
  
  if(gc->collect_kind != MINOR_COLLECTION) 
  {
    mspace->time_collections += pause_time;

    Tslow = (float)pause_time;
    SMax = total_free_size;
    gc->force_major_collect = FALSE;
    
    unsigned int major_survive_size = space_committed_size((Space*)mspace) - mos_free_size;
    survive_ratio = (float)major_survive_size/(float)gc_gen_total_memory_size(gc);
    mspace->survive_ratio = survive_ratio;
  
  }else{
    /*Give a hint to mini_free_ratio. */
    if(gc->num_collections == 1){
      /*fixme: This is only set for tuning the first warehouse!*/
      Tslow = pause_time / gc->survive_ratio;
      SMax = (unsigned int)((float)gc->committed_heap_size * ( 1 - gc->survive_ratio ));
      last_total_free_size = gc->committed_heap_size - gc->los->committed_heap_size;
    }

    fspace->time_collections += pause_time;  
    unsigned int free_size_threshold;
      
    unsigned int minor_survive_size = last_total_free_size - total_free_size;

    float k = Tslow * fspace->num_collections/fspace->time_collections;
    float m = ((float)minor_survive_size)*1.0f/((float)(SMax - GC_MOS_MIN_EXTRA_REMAIN_SIZE ));
    float free_ratio_threshold = mini_free_ratio(k, m);
    free_size_threshold = (unsigned int)(free_ratio_threshold * (SMax - GC_MOS_MIN_EXTRA_REMAIN_SIZE ) + GC_MOS_MIN_EXTRA_REMAIN_SIZE );

    if ((mos_free_size + nos_free_size)< free_size_threshold)  {
      gc->force_major_collect = TRUE;
    }

    survive_ratio = (float)minor_survive_size/(float)space_committed_size((Space*)fspace);
    fspace->survive_ratio = survive_ratio;
  }
  
  gc->survive_ratio =  (gc->survive_ratio + survive_ratio)/2.0f;

  last_total_free_size = total_free_size;

  return;
}


Boolean gc_compute_new_space_size(GC_Gen* gc, unsigned int* mos_size, unsigned int* nos_size)
{
  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  Blocked_Space* lspace = (Blocked_Space*)gc->los;  
  
  unsigned int new_nos_size;
  unsigned int new_mos_size;

  unsigned int curr_nos_size = space_committed_size((Space*)fspace);
  unsigned int used_mos_size = space_used_memory_size(mspace);
  unsigned int free_mos_size = space_committed_size((Space*)mspace) - used_mos_size;

  unsigned int total_size;

#ifdef STATIC_NOS_MAPPING
    total_size = max_heap_size_bytes - lspace->committed_heap_size;
#else
    total_size = (unsigned int)gc->heap_end - (unsigned int)mspace->heap_start;
#endif

  /* check if curr nos size is too small to shrink */
  /*
  if(curr_nos_size <= min_nos_size_bytes){
    //after major, should not allow this size 
    assert(gc->collect_kind == MINOR_COLLECTION);
    return FALSE;
  }
  */
  
  unsigned int total_free = total_size - used_mos_size;
  /* predict NOS + NOS*ratio = total_free_size */
  int nos_reserve_size;
  nos_reserve_size = (int)(((float)total_free)/(1.0f + fspace->survive_ratio));
  new_nos_size = round_down_to_size((unsigned int)nos_reserve_size, SPACE_ALLOC_UNIT);
#ifdef STATIC_NOS_MAPPING
  if(new_nos_size > fspace->reserved_heap_size) new_nos_size = fspace->reserved_heap_size;
#endif  
  if(new_nos_size > GC_MOS_MIN_EXTRA_REMAIN_SIZE) new_nos_size -= GC_MOS_MIN_EXTRA_REMAIN_SIZE ;

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
  
  unsigned int new_nos_size;
  unsigned int new_mos_size;

  Boolean result = gc_compute_new_space_size(gc, &new_mos_size, &new_nos_size);

  if(!result) return;

  unsigned int curr_nos_size = space_committed_size((Space*)fspace);

  if( abs((int)new_nos_size - (int)curr_nos_size) < NOS_COPY_RESERVE_DELTA )
    return;
  
  /* below are ajustment */  

  nos_boundary = (void*)((unsigned int)gc->heap_end - new_nos_size);

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

  HelperClass_set_NosBoundary(nos_boundary);
  
  return;
}

#else /* ifndef STATIC_NOS_MAPPING */

void gc_gen_adapt(GC_Gen* gc, int64 pause_time)
{
  gc_decide_next_collect(gc, pause_time);

  if(NOS_SIZE) return;

  unsigned int new_nos_size;
  unsigned int new_mos_size;

  Boolean result = gc_compute_new_space_size(gc, &new_mos_size, &new_nos_size);

  if(!result) return;

  Blocked_Space* fspace = (Blocked_Space*)gc->nos;
  Blocked_Space* mspace = (Blocked_Space*)gc->mos;
  
  unsigned int curr_nos_size = space_committed_size((Space*)fspace);

  if( abs((int)new_nos_size - (int)curr_nos_size) < NOS_COPY_RESERVE_DELTA )
    return;
      
  unsigned int used_mos_size = space_used_memory_size((Blocked_Space*)mspace);  
  unsigned int free_mos_size = space_free_memory_size((Blocked_Space*)mspace);  

  unsigned int new_free_mos_size = new_mos_size -  used_mos_size;
  
  unsigned int curr_mos_end = (unsigned int)&mspace->blocks[mspace->free_block_idx - mspace->first_block_idx];
  unsigned int mos_border = (unsigned int)mspace->heap_end;
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
