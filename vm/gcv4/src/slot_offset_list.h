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


#ifndef _SLOT_OFFSET_LIST_H_
#define _SLOT_OFFSET_LIST_H_

//
// An Object List is used to keep track of lists of objects when
// a hash table-style remembered set or object set is overkill.
//

#include "gc_header.h"
#include "platform_lowlevel.h"
//#include "object_layout.h"

class slot_offset_list {
public:
    slot_offset_list();

    virtual ~slot_offset_list();

    void add_entry(void **slot, Partial_Reveal_Object *p_obj, POINTER_SIZE_INT offset);

    void next();
    
    bool available();

    void rewind();

    void reset();

    unsigned size();

    void debug_check_list_integrity();

    void debug_dump_list();
    
    // Interior pointers can point at non objects, thus the void.
    void **get_slot ();


    Partial_Reveal_Object **get_last_base_slot ();
    
    Partial_Reveal_Object *get_base ();

    POINTER_SIZE_INT get_offset ();

private:

    typedef struct slot_offset_entry {
        void **slot;
        Partial_Reveal_Object *base;
        POINTER_SIZE_INT offset;
    } slot_offset_entry;

    unsigned _current_pointer;

    void _extend();

    unsigned _resident_count;

    unsigned _size_in_entries;

    slot_offset_entry *_store;
};

#endif // _SLOT_OFFSET_LIST_H_

