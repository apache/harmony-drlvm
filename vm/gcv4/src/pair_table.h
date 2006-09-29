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


#ifndef _pair_table_H_
#define _pair_table_H_

// VM includes
#include "open/vm.h"

// GC includes
#include "gc_header.h"
#include "gc_cout.h"

struct key_val {
    Partial_Reveal_Object *key;
    POINTER_SIZE_INT val;
    int32 val2;
};

struct key_val_val {
    Partial_Reveal_Object *key;
    POINTER_SIZE_INT val;
    int32 val2;
    int32 val3;
};

class Pair_Table {
public:
    // This is a simple SSB style implementation that holds pairs of object.
    // It is currently *not* thread safe.
#define SSB_SIZE 4096
    
    struct ssb {
        ssb *next;
        unsigned int free_index;
        unsigned int scan_index;
        key_val buffer[SSB_SIZE];
    };
    
    
    Pair_Table ();
    virtual ~Pair_Table();
    
    void add (Partial_Reveal_Object *key, POINTER_SIZE_INT val, int32 val2);
    
    void rewind();
    
    // Return: true if key and val hold valid data
    // Otherwise returns false.
    bool next(Partial_Reveal_Object **key, POINTER_SIZE_INT *val, int32 *val2);

private:    
    ssb *ssb_chain;
    
};

class Triple_Table {
public:
    // This is a simple SSB style implementation that holds pairs of object.
    // It is currently *not* thread safe.
#define SSB_SIZE 4096
    
    struct ssb_triple {
        ssb_triple *next;
        unsigned int free_index;
        unsigned int scan_index;
        key_val_val buffer[SSB_SIZE];
    };
    
    
    Triple_Table ();
    virtual ~Triple_Table();
    
    void add (Partial_Reveal_Object *key, POINTER_SIZE_INT val, int32 val2, int32 val3);
    
    void rewind();
    
    // Return: true if key and val hold valid data
    // Otherwise returns false.
    bool next(Partial_Reveal_Object **key, POINTER_SIZE_INT *val, int32 *val2, int32 *val3);

private:    
    ssb_triple *ssb_chain;    
};

class Sorted_Table {
public: 
    Sorted_Table();
    Sorted_Table(unsigned int size);
    ~Sorted_Table();

    void init_sorted_table(unsigned int size);  
    //
    // Add val to the table. If the table is full then double the size.
    //

    void add(void *val);
    

    // Checks to see if the table contains val. 
    // Returns true if it is a member
    //         false otherwise
    //
   
inline 
bool member (POINTER_SIZE_INT val)
{
    if (!table_sorted) {
        ABORT("We can't sort now unless we make this code thread safe");
    }
    // Do binary search to find val
    assert(val);
    // Binary search.
    int low = 0, high = sorted_table_size - 1;
    while (low <= high) {
        TRACE("high sorted_table[" << high << "]= "
            << (void *)sorted_table[high] << ", low sorted_table[" 
            << low << "]= " << (void *)sorted_table[low]);
        unsigned int mid = (high + low) / 2;
        if (sorted_table[mid] == val) {
            // This object was moved during GC
            members_found++;
            TRACE("Found val = " << val << ", mid = " << mid);
            return true;
        } else if ((POINTER_SIZE_INT) sorted_table[mid] < (POINTER_SIZE_INT) val) { 
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    } 
    return false;
}
    // Sort the table; This assumes that race condition are dealt with above.
    void sort();

private:
    
    unsigned int sorted_table_size;
    unsigned int free_index;
    unsigned int iter;
    POINTER_SIZE_INT *sorted_table;
    Boolean table_sorted;
    unsigned int members_found;
};

#endif // _pair_table_H_
