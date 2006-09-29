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


#ifndef _Object_List_H_
#define _Object_List_H_

// GC header files
#include "gc_cout.h"
#include "gc_header.h"

#include "port_malloc.h"
//
// The default size of object lists. Keep modest for now.
//
#define DEFAULT_OBJECT_SIZE_IN_ENTRIES 4096*16

//
// An Object List is used to keep track of lists of objects when
// a hash table-style remembered set or object set is overkill.
//

template<class T>class Template_List{
public:
    Template_List() {
        _size_in_entries = DEFAULT_OBJECT_SIZE_IN_ENTRIES;
        _store = (T *)STD_MALLOC(_size_in_entries * sizeof(T));
        if (_store==NULL) {
            DIE("Error: STD_MALLOC failed while creating list.");
        }
        reset();
    }

    virtual ~Template_List() {
        STD_FREE(_store);
    }

    unsigned add_entry(T p_obj) {
        if (_resident_count >= (_size_in_entries - 1)) {
            _extend();
        }
        _store[_resident_count++] = p_obj;
        return (_resident_count - 1);
    }
    
    bool exists(T p_obj) {
        for (unsigned int i = 0; i < _resident_count; i++) {
            if (_store[i] == p_obj) {
                return true;
            } 
        } 
        return false;
    }
    
    T next(void) {
        if (_current_pointer >= _resident_count) {
            return NULL;
        }
        return _store[_current_pointer++];
    }

    void rewind() {
        _current_pointer = 0;
    }

    void reset() {
        rewind();
        _resident_count = 0;
    }
    
    unsigned size() {
        return _resident_count;
    }
    
    void debug_dump_list() {
        INFO("Dump of object list:\n");
        for (unsigned idx = 0; idx < _resident_count; idx++) {
            INFO("entry[" << idx << "] = " << _store[idx]);
        }
    }

private:

    unsigned _current_pointer;

    void _extend() {
        T *old_store = _store;
        
        // Present policy: double it.
        _size_in_entries = _size_in_entries * 2;

        _store = (T *)STD_MALLOC(_size_in_entries * sizeof (T));

        if (_store==NULL) {
            DIE("Error: STD_MALLOC failed while creating object list.");
        }

        memcpy((void *)_store, (void *)old_store, _resident_count * sizeof(T));

        STD_FREE(old_store);
    }
    
    unsigned _resident_count;

    unsigned _size_in_entries;

    T *_store;
};

typedef Template_List<Partial_Reveal_Object *> Object_List;
typedef Template_List<Partial_Reveal_VTable *> VTable_List;


#endif // _Object_List_H_

