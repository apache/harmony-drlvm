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
#include "slot_offset_list.h"
#include "port_malloc.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////




// This code is used to support interior pointers.
//

slot_offset_list::slot_offset_list() 
{
    _size_in_entries = DEFAULT_OBJECT_SIZE_IN_ENTRIES;

    _store = (slot_offset_entry *)STD_MALLOC(_size_in_entries * 
                                          sizeof(slot_offset_entry));

    if (_store==NULL) {
        DIE("Error: m_malloc failed while creating slot offset list.");
    }

    _resident_count  = 0;
}

slot_offset_list::~slot_offset_list()
{
    STD_FREE(_store);
}

void
slot_offset_list::add_entry(void **slot, Partial_Reveal_Object *base, POINTER_SIZE_INT offset)
{
    if (_resident_count >= (_size_in_entries - 1)) {
        _extend();
    }

    _store[_resident_count].slot = slot;
    _store[_resident_count].base = base;
    _store[_resident_count].offset = offset;
    _resident_count++;

    return;
}

void
slot_offset_list::_extend()
{
    slot_offset_list::slot_offset_entry *old_store = _store;

    //
    // Present policy: double it.
    //
    _size_in_entries = _size_in_entries * 2;

    _store = (slot_offset_list::slot_offset_entry *)STD_MALLOC(_size_in_entries * 
                                          sizeof (slot_offset_entry));

    if (_store==NULL) {
        DIE("Error: m_malloc failed while creating slot offset list.");
    }

    memcpy((void *)_store, 
           (void *)old_store, 
           _resident_count * sizeof(Partial_Reveal_Object *));

    STD_FREE(old_store);
}

void
slot_offset_list::next()
{
    _current_pointer++;
    // to avoid infinite loops of nexts make sure it is called only
    // once when the table is empty.
    assert (_current_pointer < (_resident_count + 1));
    return;
}

void **slot_offset_list::get_slot ()
{
    return _store[_current_pointer].slot;
}

Partial_Reveal_Object *slot_offset_list::get_base ()
{
    return _store[_current_pointer].base;
}

POINTER_SIZE_INT slot_offset_list::get_offset ()
{
    return _store[_current_pointer].offset;
}

Partial_Reveal_Object **slot_offset_list::get_last_base_slot ()
{   
    return &_store[_resident_count - 1].base;
}

void 
slot_offset_list::debug_dump_list()
{
    INFO("Dump of slot_offset_list:");
    for (unsigned idx = 0; idx < _resident_count; idx++) {
        INFO("entry[" << idx << "].base = " << _store[idx].base);
        INFO("entry[" << idx << "].slot = " << _store[idx].slot);
        INFO("entry[" << idx << "].offset = " << _store[idx].offset);
    }
}

void
slot_offset_list::rewind()
{
    _current_pointer = 0;
}

bool
slot_offset_list::available()
{
    return (_current_pointer < _resident_count);
}

// Clear the table.
void
slot_offset_list::reset()
{
    rewind();
    _resident_count = 0;
}

unsigned
slot_offset_list::size()
{
    return _resident_count;
}

// end file gc\slot_base_offset_list.cpp
