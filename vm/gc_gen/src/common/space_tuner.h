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
#define GC_FIXED_SIZE_TUNER

//For_LOS_extend
enum Transform_Kind {
  TRANS_NOTHING = 0,
  TRANS_FROM_LOS_TO_MOS = 0x1,
  TRANS_FROM_MOS_TO_LOS = 0x2,
};

typedef struct Space_Tuner{
    Transform_Kind kind;

    unsigned int tuning_size;
    unsigned int conservative_tuning_size;
    unsigned int least_tuning_size;
    unsigned int force_tune;
    
    /*LOS alloc speed sciecne last los variation*/    
    unsigned int speed_los;
    /*MOS alloc speed sciecne last los variation*/    
    unsigned int speed_mos;

    /*Total wasted memory of los science last los variation*/
    unsigned int wast_los;
    /*Total wasted memory of mos science last los variation*/
    unsigned int wast_mos;

    unsigned int current_dw;
    /*NOS survive size of last minor, this could be the least meaningful space unit when talking about tuning.*/
    unsigned int current_ds;

    /*Threshold for deta wast*/
    unsigned int threshold;
    /*Minimun tuning size for los variation*/
    unsigned int min_tuning_size;

    /*Cost of normal major compaction*/
    unsigned int fast_cost;
    /*Cost of major compaction when changing LOS size*/    
    unsigned int slow_cost;    
}Space_Tuner;

void gc_space_tune_prepare(GC* gc, unsigned int cause);
void gc_space_tune_before_gc(GC* gc, unsigned int cause);
void gc_space_tune_before_gc_fixed_size(GC* gc, unsigned int cause);
void gc_space_tuner_reset(GC* gc);
void gc_space_tuner_initialize(GC* gc);

#endif /* _SPACE_TUNER_H_ */
