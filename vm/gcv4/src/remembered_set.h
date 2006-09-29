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


#ifndef _Remembered_Set_H_
#define _Remembered_Set_H_

//
// A remembered set is maintained per car and per generation 
// (except the highest numbered car and the oldest generation).
// and is a non-duplicate list of pointers to pointers from
// outside that region. Typically only pointers from older to
// younger generations, and higher to lower trains are recorded.
//

#include "platform_lowlevel.h"
#include "stdio.h"
#include "hash_table.h"
#include "compressed_references.h"
#include "gc_cout.h"

class Train_Generation;

//
// Remembered Sets inherit from Hash Table and enhance it
// by typing the entries of the Hash Table to be pointers
// to Java_lang_Object.
//
class Remembered_Set : public Hash_Table {
public:

    // For now remove the inline so we can check for valid references.

    unsigned add_entry(Slot address) ;

    Remembered_Set *p_clone();

    void dump() {
        Slot pp_obj(NULL);

        TRACE("[[");
        TRACE(_resident_count << " entries[[");
        rewind();

        while (pp_obj.set(next().get_address()) != NULL) {
            TRACE("\t==> [" << pp_obj.get_address()<< "](" << pp_obj.dereference() << ")");
        }
        TRACE("]]");
        return;
    }

    Remembered_Set *merge(Remembered_Set *rs) {
        return (Remembered_Set *)Hash_Table::merge((Hash_Table *)rs);
    }

    inline bool is_present(Slot address) {
        return Hash_Table::is_present(address.get_address());
    }

    inline bool is_not_present(Slot address) {
        return Hash_Table::is_not_present(address.get_address());
    }

    inline Slot next(void) {
        return Slot(Hash_Table::next());
    }

};

#endif // _Remembered_Set_H_

