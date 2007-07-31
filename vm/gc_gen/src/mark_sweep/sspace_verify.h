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

#ifndef _SSPACE_VERIFY_H_
#define _SSPACE_VERIFY_H_

#include "../common/gc_common.h"

//#define SSPACE_VERIFY
//#define SSPACE_CHUNK_INFO
//#define SSPACE_ALLOC_INFO
//#define SSPACE_TIME

struct Sspace;

void sspace_verify_init(GC *gc);
void sspace_verify_alloc(void *addr, unsigned int size);
void sspace_verify_vtable_mark(GC *gc);
void sspace_verify_mark(void *addr, unsigned int size);
void sspace_verify_free_area(POINTER_SIZE_INT *start, POINTER_SIZE_INT size);
void sspace_verify_after_collection(GC *gc);

void sspace_chunks_info(Sspace *sspace, Boolean beore_gc);
void sspace_alloc_info(unsigned int size);
void sspace_alloc_info_summary(void);

void sspace_gc_time(GC *gc, Boolean before_gc);
void sspace_mark_time(Boolean before_mark);
void sspace_sweep_time(Boolean before_sweep);
void sspace_merge_time(Boolean before_merge);

#endif // _SSPACE_VERIFY_H_
