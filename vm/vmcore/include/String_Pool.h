/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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

#ifndef _STRING_POOL_H_
#define _STRING_POOL_H_

#include <string.h>
#include "tl/memory_pool.h"
#include "object_layout.h"

extern "C" {
    typedef struct String {
#ifdef _DEBUG
        unsigned id;                    // id for debugging
#endif
        // 20030507 Ref to the interned string used by java.lang.String.intern(). It is compressed when compressing refs.
        union {                         
            ManagedObject*         raw_ref;          // raw reference to interned string if not compressing references
            uint32                 compressed_ref;   // equivalent compressed reference.
        } intern;
        unsigned len;
        char bytes[4];
    } String;
}



class String_Pool {
public:
    String_Pool();

    //
    // lookup string in string table & insert if not found
    //
    VMEXPORT String *lookup(const char *str);
    String *lookup(const char *str, unsigned len);

    // Iterators for GC
    String *get_first_string();
    String *get_first_string_intern(unsigned *);

    String *get_next_string(String *);
    String *get_next_string_intern(String *, unsigned *);

    // The GC enumeration code needs to lock and unlock the 
    // pool in order to enumerate it in a thread safe manner.
    void lock_pool();
    void unlock_pool();

private:
     bool valid_string_pool();

    //
    // Entry in string table
    //
    class _Entry {
    public:
        _Entry *const next;
        String str;
        _Entry(const char *s, unsigned len, _Entry *n);
        /**
         * Memory is already allocated for this object.
         */
                void *operator new(size_t UNREF sz, void * mem) { return mem; }
                void operator delete(void * UNREF mem, void * UNREF mem1) { 
                        // define empty operator delete just to get rid of warning
                }
    };

    //
    // string table size
    //
#define STRING_TABLE_SIZE  44449
    //
    // table of string entries
    //
    _Entry  *_table[STRING_TABLE_SIZE];
    //
    // memory pool
    //
    tl::MemoryPool  memory_pool;

    // sufficient to compile empty class and its prerequisites
    //#define INITIAL_STRING_ARENA_SIZE 94000
};

#endif // _STRING_POOL_H_
