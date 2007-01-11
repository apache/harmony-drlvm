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

//For_LOS_extend
enum Transform_Kind {
  TRANS_NOTHING = 0,
  TRANS_FROM_LOS_TO_MOS = 0x1,
  TRANS_FROM_MOS_TO_LOS = 0x2,
};

typedef struct Space_Tuner{
    /*fixme: Now we use static value of GC_LOS_MIN_VARY_SIZE. */
    unsigned int tuning_threshold;
    Transform_Kind kind;
    unsigned int tuning_size;
}Space_Tuner;

void gc_space_tune(GC* gc, unsigned int cause);
void gc_space_tuner_reset(GC* gc);
void gc_space_tuner_initialize(GC* gc);

#endif /* _SPACE_TUNER_H_ */
