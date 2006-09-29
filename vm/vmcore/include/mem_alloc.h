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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#ifndef _MEM_ALLOC_H_
#define _MEM_ALLOC_H_

#include "port_vmem.h"

typedef struct Pool_Descriptor {
    Byte    *start;     // (misnamed) points to the next free byte in the pool
    Byte    *end;       // end of the pool's memory region
    size_t   default_size;
    bool     is_code;
    bool     optimize_for_trace_cache;
#ifdef VM_STATS
    uint64   num_allocations;
    uint64   num_pool_allocations;
    size_t   total_pool_size;
    size_t   total_size_allocated;
    uint64   num_resizes;
    size_t   current_alloc_size;
#endif //VM_STATS
    port_vmem_t *descriptor;
} Pool_Descriptor;


extern Pool_Descriptor* jit_code_pool;
extern Pool_Descriptor* vtable_data_pool;

extern unsigned system_page_size;
extern unsigned page_size_for_allocation;
extern size_t   initial_code_pool_size;

#endif //_MEM_ALLOC_H_

