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

#ifndef _SPACE_TUNER_H_
#define _SPACE_TUNER_H_

#include "gc_common.h"
#include "gc_space.h"

#define GC_LOS_MIN_VARY_SIZE ( 2 * MB )
//#define GC_FIXED_SIZE_TUNER

//For_LOS_extend
enum Transform_Kind {
  TRANS_NOTHING = 0,
  TRANS_FROM_LOS_TO_MOS = 0x1,
  TRANS_FROM_MOS_TO_LOS = 0x2,
};

typedef struct Space_Tuner{
    Transform_Kind kind;

    POINTER_SIZE_INT tuning_size;
    POINTER_SIZE_INT conservative_tuning_size;
    POINTER_SIZE_INT least_tuning_size;
    /*Used for LOS_Shrink*/
    Block_Header* interim_blocks;
    Boolean need_tune;
    Boolean force_tune;
    
    /*LOS alloc speed sciecne last los variation*/    
    POINTER_SIZE_INT speed_los;
    POINTER_SIZE_INT old_speed_los;
    /*MOS alloc speed sciecne last los variation*/    
    POINTER_SIZE_INT speed_mos;
    POINTER_SIZE_INT old_speed_mos;
    
    /*Total wasted memory of los science last los variation*/
    POINTER_SIZE_INT wast_los;
    /*Total wasted memory of mos science last los variation*/
    POINTER_SIZE_INT wast_mos;

    POINTER_SIZE_INT current_dw;
    /*NOS survive size of last minor, this could be the least meaningful space unit when talking about tuning.*/
    POINTER_SIZE_INT current_ds;

    /*Threshold for deta wast*/
    POINTER_SIZE_INT threshold;
    /*Minimun tuning size for los variation*/
    POINTER_SIZE_INT min_tuning_size;

    /*Cost of normal major compaction*/
    unsigned int fast_cost;
    /*Cost of major compaction when changing LOS size*/    
    unsigned int slow_cost;    
}Space_Tuner;

void gc_space_tune_prepare(GC* gc, unsigned int cause);
void gc_space_tune_before_gc(GC* gc, unsigned int cause);
void gc_space_tune_before_gc_fixed_size(GC* gc, unsigned int cause);
Boolean gc_space_retune(GC *gc);
void gc_space_tuner_reset(GC* gc);
void gc_space_tuner_initialize(GC* gc);

#endif /* _SPACE_TUNER_H_ */
