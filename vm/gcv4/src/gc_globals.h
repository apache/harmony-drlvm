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
 * @author Intel, Salikh Zakirov
 * @version $Revision: 1.1.2.1.4.3 $
 */  



// Globals in the GC - need to eliminate.
//

#ifndef _gc_globals_h_
#define _gc_globals_h_

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "pair_table.h"
#include "slot_offset_list.h"
#include "list"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool stats_gc;
extern bool verify_gc;
extern bool characterize_heap;

extern POINTER_SIZE_INT initial_heap_size_bytes;
extern POINTER_SIZE_INT final_heap_size_bytes;

// Constants used to size the number of objects that can be colocated.
#define MAX_FUSABLE_OBJECT_SCAN_STACK   64
#define MAX_FUSED_OBJECT_COUNT          64

//
// The size to determine if object is large or small.
//

extern unsigned los_threshold_bytes;


//
// This flag is initially false during VM startup. It becomes true
// when the VM is fully initialized. This signals the GC to feel
// free to do a stop-the-world at any time.
//
extern bool vm_initialized;

extern slot_offset_list *interior_pointer_table;
extern slot_offset_list *compressed_pointer_table;

#endif // _gc_globals_h_
