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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Override the add_entry to do check to ensure pointer is valid.

//
// Add an entry into this hash table, if it doesn't already exist.
//

// For now remove the inline so we can check for valid references.
//    inline 
unsigned 
Remembered_Set::add_entry(Slot address)
{

    return Hash_Table::add_entry(address.get_address());
}
//
// Clone myself.
//
Remembered_Set *
Remembered_Set::p_clone()
{
    Remembered_Set *p_cloned_rs = new Remembered_Set();

    rewind();
    
    Slot pp_obj_ref(NULL);
    while (pp_obj_ref.set(next().get_address()) != NULL) {
        p_cloned_rs->add_entry(pp_obj_ref);
    }
    rewind();
    p_cloned_rs->rewind();

    return p_cloned_rs;
}
