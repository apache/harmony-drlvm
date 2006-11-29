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
 * @version $Revision: 1.1.2.2.4.3 $
 */  


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>

// VM interface header files
#include "platform_lowlevel.h"
#include "open/vm_gc.h"

// GC header files
#include "gc_cout.h"
#include "gc_header.h"
#include "gc_v4.h"
#include "remembered_set.h"
#include "block_store.h"
#include "object_list.h"
#include "work_packet_manager.h"
#include "garbage_collector.h"
#include "gc_globals.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool stats_gc = false;

// if true then we are doing a characterization of the heap, 
// set with -Dcharacterize_heap=on on the command line.

bool characterize_heap = false; 

// default values are configured in gc_init()
POINTER_SIZE_INT initial_heap_size_bytes = 0;
POINTER_SIZE_INT final_heap_size_bytes = 0;

//
// Global to specify the size differentiating
//
unsigned los_threshold_bytes = 0;


bool garbage_collector_is_initialized = false;


//
// This flag is initially false, and is set to true when the
// VM is fully initialized. This signals that any stop-the-world
// collections can occur.
//
bool vm_initialized = false;


// 
// This is the table holding all the interior pointers used in any given execution 
// of a GC. It is reused each time a GC is done.

slot_offset_list *interior_pointer_table = NULL;
slot_offset_list *compressed_pointer_table = NULL;



unsigned long enumeration_time;

// end file gc\gc_globals.cpp
