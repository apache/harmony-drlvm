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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// VM interface header files
#include "open/vm_gc.h"
#include "pair_table.h"
#include "string.h"
#include "port_malloc.h"

// GC includes
#include "gc_cout.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
Pair_Table::Pair_Table()
{
    TRACE("Making new hash table");
    ssb_chain = (ssb *)STD_MALLOC(sizeof(ssb));
    assert(ssb_chain);
    memset (ssb_chain, 0, sizeof(ssb));
    return;
}

Pair_Table::~Pair_Table()
/* Discard this hash table. */
{
    ssb *this_ssb = ssb_chain;
    while (this_ssb) {
        ssb_chain = ssb_chain->next;
        STD_FREE(this_ssb);
        this_ssb = ssb_chain;
    }
    assert (ssb_chain == NULL);
}

void Pair_Table::add(Partial_Reveal_Object *key, POINTER_SIZE_INT val, int32 val2)
{
    ssb *this_ssb = ssb_chain;
    // Grab the last ssb.
    while (this_ssb->next) {
        this_ssb = this_ssb->next;
    }
    if (this_ssb->free_index == SSB_SIZE) {
        this_ssb->next = (ssb *)STD_MALLOC(sizeof(ssb));
        if (this_ssb->next == NULL) {
            DIE("(Internal - VM out of m_malloc space generating a new Pair_Table");
        }
        this_ssb = this_ssb->next;
        memset (this_ssb, 0, sizeof(ssb));
    }
    this_ssb->buffer[this_ssb->free_index].key = key;
    this_ssb->buffer[this_ssb->free_index].val = val;
    this_ssb->buffer[this_ssb->free_index].val2 = val2;
    
    TRACE("latency in is " << val2);
    this_ssb->free_index++;
}

bool Pair_Table::next(Partial_Reveal_Object **key, POINTER_SIZE_INT *val, int32 *val2)
{
    assert (ssb_chain);
    ssb *this_ssb = ssb_chain;
    while ((this_ssb->scan_index == this_ssb->free_index) && this_ssb->next) {
        this_ssb = this_ssb->next;
    }
    if (this_ssb->scan_index < this_ssb->free_index) {
        *key = this_ssb->buffer[this_ssb->scan_index].key;
        *val = this_ssb->buffer[this_ssb->scan_index].val;
        *val2 = this_ssb->buffer[this_ssb->scan_index].val2;
        TRACE("latency out is " << *val2);
        this_ssb->scan_index++;
        return true;
    }
    *key = NULL;
    *val = 0;
    *val2 = 0;
    return false;
}

// Reset all the scan indexes in the ssb chain.
void Pair_Table::rewind()
{
    ssb *this_ssb = ssb_chain;
    while (this_ssb) {
        this_ssb->scan_index = 0;
        this_ssb = this_ssb->next;
    }
}


/****************************** Triple_Table code *************************/
// This table has a key and three values.
  
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
Triple_Table::Triple_Table()
{
    TRACE("Making new hash table");
    ssb_chain = (ssb_triple *)STD_MALLOC(sizeof(ssb_triple));
    assert(ssb_chain);
    memset (ssb_chain, 0, sizeof(ssb_triple));
    return;
}

Triple_Table::~Triple_Table()
/* Discard this hash table. */
{
    ssb_triple *this_ssb = ssb_chain;
    while (this_ssb) {
        ssb_chain = ssb_chain->next;
        STD_FREE(this_ssb);
        this_ssb = ssb_chain;
    }
    assert (ssb_chain == NULL);
}

void Triple_Table::add(Partial_Reveal_Object *key, POINTER_SIZE_INT val, int32 val2, int32 val3)
{
    ssb_triple *this_ssb = ssb_chain;
    // Grab the last ssb.
    while (this_ssb->next) {
        this_ssb = this_ssb->next;
    }
    if (this_ssb->free_index == SSB_SIZE) {
        this_ssb->next = (ssb_triple *)STD_MALLOC(sizeof(ssb_triple));
        if (this_ssb->next == NULL) {
            DIE("(Internal - VM out of m_malloc space generating a new Pair_Table");
        }
        this_ssb = this_ssb->next;
        memset (this_ssb, 0, sizeof(ssb_triple));
    }
    this_ssb->buffer[this_ssb->free_index].key = key;
    this_ssb->buffer[this_ssb->free_index].val = val;
    this_ssb->buffer[this_ssb->free_index].val2 = val2;
    this_ssb->buffer[this_ssb->free_index].val3 = val3;
    
    TRACE("latency in is " << val2);
    this_ssb->free_index++;
}
bool Triple_Table::next(Partial_Reveal_Object **key, POINTER_SIZE_INT *val, int32 *val2, int32 *val3)
{
    assert (ssb_chain);
    ssb_triple *this_ssb = ssb_chain;
    while ((this_ssb->scan_index == this_ssb->free_index) && this_ssb->next) {
        this_ssb = this_ssb->next;
    }
    if (this_ssb->scan_index < this_ssb->free_index) {
        *key = this_ssb->buffer[this_ssb->scan_index].key;
        *val = this_ssb->buffer[this_ssb->scan_index].val;
        *val2 = this_ssb->buffer[this_ssb->scan_index].val2;
        *val3 = this_ssb->buffer[this_ssb->scan_index].val3;
        this_ssb->scan_index++;
        return true;
    }
    *key = NULL;
    *val = 0;
    *val2 = 0;
    *val3 = 0;
    return false;
}

// Reset all the scan indexes in the ssb chain.
void Triple_Table::rewind()
{
    ssb_triple *this_ssb = ssb_chain;
    while (this_ssb) {
        this_ssb->scan_index = 0;
        this_ssb = this_ssb->next;
    }
}


/****************************** Sorted_Table code *************************/
//
// This table is populated, sorted and then probed to membership. If an object is added
// to the table after it has been sorted then an error is reported. Currently the 
// probe is log N where N is the number of entries.
//

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SORTED_TABLE_SIZE 256

static int 
pair_table_intcompare( const void *arg1, const void *arg2 )
{
    POINTER_SIZE_INT a = *(POINTER_SIZE_INT *)arg1;
    POINTER_SIZE_INT b = *(POINTER_SIZE_INT *)arg2;
    if (a < b) {
        return -1;
    }
    if (a == b) {
        return 0;
    }
    return 1;
}

Sorted_Table::Sorted_Table()
{
    init_sorted_table(SORTED_TABLE_SIZE);
    free_index = 0;
    iter = 0;
}

Sorted_Table::Sorted_Table(unsigned int size)
{
    init_sorted_table(size);
    free_index = 0;
    iter = 0;
}

Sorted_Table::~Sorted_Table()
/* Discard this hash table. */
{
    STD_FREE(sorted_table);
}

void Sorted_Table::init_sorted_table(unsigned int size)
{
    unsigned int size_in_bytes = sizeof(void *) * size;
    sorted_table = (POINTER_SIZE_INT *)STD_MALLOC(size_in_bytes);
    memset (sorted_table, 0, size_in_bytes);
    sorted_table_size = size;
    table_sorted = false;
    members_found = 0;
}

//
// Add val to the table. If the table is full then double the size.
//

void Sorted_Table::add(void *val)
{
    assert (free_index < sorted_table_size);
    sorted_table[free_index] = (POINTER_SIZE_INT)val;
    free_index++;
    if (free_index == sorted_table_size) {
        void *old_sorted_table = sorted_table;
        init_sorted_table (sorted_table_size * 2);
        memmove(sorted_table, old_sorted_table, free_index * sizeof (void *)); 
        STD_FREE(old_sorted_table);
    }
    table_sorted = false;
}

// Sort the table;
void Sorted_Table::sort()
{
    if (table_sorted) {
        return;
    }
    sorted_table_size = free_index;
    qsort(sorted_table, sorted_table_size, sizeof(void *), pair_table_intcompare);
    table_sorted = true;
#ifdef _DEBUG
    unsigned int i = 0;
    for (i = 0; i < sorted_table_size; i++) {
        TRACE("sorted_table[" << i << "] = " << sorted_table[i]);
        if (i > 0) {
            assert (sorted_table[i] >= sorted_table[i-1]);
        }
    } 
#endif
}
