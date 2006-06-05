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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"

#include "platform_lowlevel.h"

//MVM
#include <iostream>
using namespace std;

#include <assert.h>
#include "String_Pool.h"
#include <apr-1/apr_atomic.h>
#include "environment.h"
#include "open/vm_util.h"
#include "atomics.h"

// In order to make this code thread safe a light weight lock is used.
// The protocol is simple and the lock is local to this string pool.
volatile POINTER_SIZE_INT string_pool_lock; // 1 is locked, 0 is unlocked

// apr_atomic_casptr should result in an .acq on IPF.
void String_Pool::lock_pool ()
{
    // Spin until lock is m_free.
    while (apr_atomic_casptr(
        (volatile void **)&string_pool_lock, (void *)1, (void *)0) != 0) {
        Sleep (0);
    }
}

// Release lock. string_pool_lock is volatile which results in a st.rel on IPF
void String_Pool::unlock_pool ()
{
    assert (string_pool_lock != 0);
    string_pool_lock = 0;
} //String_Pool::unlock_pool


String_Pool::_Entry::_Entry(const char *s, unsigned len, _Entry *n) : next(n)
{
    str.len = len;
    strncpy(str.bytes, s, len);
    str.bytes[len] = '\0';
    // This constructor can be run very early on during VM execution--even before main is entered.
    // This is before we have had a chance to process any command line arguments. So, initialize the 
    // interned Java_lang_String reference to NULL in a way that will work whether references are compressed or not.
    str.intern.raw_ref = NULL;
    str.intern.raw_ref = NULL;
} //String_Pool::_Entry::_Entry


String_Pool::String_Pool()
{
    // lock_pool(); - IvanV: no need, who will use string pool before
    // constructor?
    //
    // allocate the string table and zero it out
    //
    unsigned table_size = sizeof(_Entry*) * STRING_TABLE_SIZE;
    memset(_table,0,table_size);
    // unlock_pool(); - IvanV: no need, who will use string pool before
    // constructor?
} //String_Pool::String_Pool



#ifdef _DEBUG_STRING_POOL

// Should be wrapped in assert.
static int valid_count = 0;

bool String_Pool::valid_string_pool()
{
    _Entry *e;
    Java_java_lang_String *jstr;
    unsigned short hash;
    for (hash = 0;  hash < STRING_TABLE_SIZE;  hash++) {
        for (e = _table[hash];  e != NULL;  e = e->next) {
            if (VM_Global_State::loader_env->compress_references) {
                jstr = (Java_java_lang_String *)uncompress_compressed_reference(e->intern.compressed_ref);
            } else {
                jstr = e->intern.raw_ref;
            }
            if (jstr != NULL) {
                assert(jstr->vt); // Make sure it is a valid vtable.
            }
        }
    }
    valid_count++;
    return true;
} //String_Pool::valid_string_pool

#else  // !_DEBUG_STRING_POOL

inline bool String_Pool::valid_string_pool()
{
    return true;
} //String_Pool::valid_string_pool

#endif // !_DEBUG_STRING_POOL


String *String_Pool::lookup(const char *s)
{

    //
    // compute hash
    //
    int hash = 0;
    unsigned len = 0;
    const char *t = s;
    int c;
    int h1 = 0, h2 = 0;
    while ((c = *t++) != '\0') {
        h1 = h1 + c;
        if((c = *t++) == 0) {
            break;
        }
        h2 = h2 + c;
    }
    hash = (h1 + (h2 << 8)) & 0x7fffffff;
    len = (unsigned) ((t - s) - 1);
    hash = hash % STRING_TABLE_SIZE;

    assert (valid_string_pool());

    //
    // search bucket for string, no lock
    //
    _Entry *e;
    for (e = _table[hash]; e != NULL; e = e->next) {
        if (e->str.len == len && memcmp(s, e->str.bytes, len) == 0) {
            //
            // found string in table
            //
            return  &e->str;
        }
    }

    lock_pool();

    //
    // search bucket for string, strict variant with locking to avoid
    // duplication
    //
    for (e = _table[hash]; e != NULL; e = e->next) {
        if (e->str.len == len && memcmp(s, e->str.bytes, len) == 0) {
            //
            // found string in table
            //
            unlock_pool();
            return  &e->str;
        }
    }
    //
    // string not in table; insert a new string entry into string pool
    //
    // compute size of _Entry record
    // add one to str_len for '\0'
    // subtract 2 for bytes[2] already in _Entry
    //
    unsigned entry_size = sizeof(_Entry) + len + 1 - 2;

    /* Synchronized via String_Pool lock */
    void * mem = memory_pool.alloc(entry_size);

    // We need ordering of writes here as we use the collection without lock.
    // Entry's next pointer should be updated before we update head reference.
    e = new(mem) _Entry(s,len,_table[hash]);
    MemoryWriteBarrier();
    _table[hash] = e;

    assert (valid_string_pool());

    unlock_pool();
    return &e->str;
} //String_Pool::lookup


String *String_Pool::lookup(const char *s, unsigned len)
{
    //
    // compute hash
    //
    int hash = 0;
    const char *t = s;
    const char *eos = s + len;
    int c;
    int h1 = 0, h2 = 0;
    while (t != eos) {
        c = *t++;
        h1 = h1 + c;
        if(t == eos) {
            break;
        }
        c = *t++;
        h2 = h2 + c;
    }
    hash = (h1 + (h2 << 8)) & 0x7fffffff;
    hash = hash % STRING_TABLE_SIZE;

    lock_pool();

    assert (valid_string_pool());

    //
    // search bucket for string
    //
    _Entry *e;
    for (e = _table[hash]; e != NULL; e = e->next) {
        if (e->str.len == len && strncmp(s, e->str.bytes, len) == 0) {
            //
            // found string in table
            //
            unlock_pool();
            return  &e->str;
        }
    }
    //
    // string not in table; insert a new string entry into string pool
    //
    // compute size of _Entry record
    // add one to str_len for '\0'
    // subtract 2 for bytes[2] already in _Entry
    //
    unsigned entry_size = sizeof(_Entry) + len + 1 - 2;
    //
    // round up to word aligned size
    //
    entry_size = (entry_size + 3) & 0xFFFFFFFC;

    /* Synchronized via String_Pool lock */
    void * mem = memory_pool.alloc(entry_size);

    _table[hash] = e = new(mem) _Entry(s,len,_table[hash]);

    assert (valid_string_pool());
    unlock_pool();
    return &e->str;
} //String_Pool::lookup



// A simple iterator.
// It is used for GC at the moment, but it should be replaced with
// something that would allow GC of classes.
String *String_Pool::get_first_string()
{
    for(unsigned i = 0; i < STRING_TABLE_SIZE; i++) {
        if(_table[i] != NULL)
            return &(_table[i]->str);
    }
    return NULL;
} //String_Pool::get_first_string



String *String_Pool::get_next_string(String *prev)
{
    assert(prev);
    // hash on string name address
    int hash = 0;
    const char *s = prev->bytes;
    const char *t = s;
    int c;
    int h1 = 0, h2 = 0;
    while ((c = *t++) != '\0') {
        h1 = h1 + c;
        if((c = *t++) == 0) {
            break;
        }
        h2 = h2 + c;
    }
    hash = (h1 + (h2 << 8)) & 0x7fffffff;
    hash = hash % STRING_TABLE_SIZE;

    for(_Entry *e = _table[hash]; e != NULL; e = e->next) {
        if((&(e->str)) == prev) {
            if(e->next) {
                return &(e->next->str);
            } else {
                for(unsigned i = hash + 1; i < STRING_TABLE_SIZE; i++) {
                    if(_table[i] != NULL)
                        return &(_table[i]->str);
                }
                return NULL;
            }
        }
    }
    ABORT("Can't find the string given in the table");
    return NULL;
} //String_Pool::get_next_string


String *String_Pool::get_first_string_intern(unsigned *cookie)
{
    if (VM_Global_State::loader_env->compress_references) {
        // Examine the "compressed_ref" instead of the "raw_ref" field.
        for (unsigned i = 0; i < STRING_TABLE_SIZE; i++) {
            for (_Entry *e = _table[i]; e != NULL; e = e->next) {
                if (e->str.intern.compressed_ref != 0) {
                    *cookie = i;
                    return &(e->str);
                }
            }
        }
    } else {
        for (unsigned i = 0; i < STRING_TABLE_SIZE; i++) {
            for (_Entry *e = _table[i]; e != NULL; e = e->next) {
                if (e->str.intern.raw_ref != NULL) {
                    *cookie = i;
                    return &(e->str);
                }
            }
        }
    }
    *cookie = 0;
    return NULL;
} //String_Pool::get_first_string_intern


String *String_Pool::get_next_string_intern(String *prev, unsigned *cookie)
{
    assert(prev);
    unsigned short hash = (short)*cookie;
    if (VM_Global_State::loader_env->compress_references) {
        // Examine the "compressed_ref" instead of the "raw_ref" field.
        for (_Entry *e = _table[hash]; e != NULL; e = e->next) {
            if ((&(e->str)) == prev) {
                for (e = e->next; e != NULL; e = e->next) {
                    if (e->str.intern.compressed_ref) {
                        // same cookie
                        return &(e->str);
                    }
                }
                break;
            }
        }
        for (unsigned i = hash +1; i < STRING_TABLE_SIZE; i++ ) {
            for (_Entry *e = _table[i]; e!= NULL; e= e->next) {
                if (e->str.intern.compressed_ref) {
                    *cookie = i;
                    return &(e->str);
                }
            }
        }
    } else {
        for (_Entry *e = _table[hash]; e != NULL; e = e->next) {
            if ((&(e->str)) == prev) {
                for (e = e->next; e != NULL; e = e->next) {
                    if (e->str.intern.raw_ref) {
                        // same cookie
                        return &(e->str);
                    }
                }
                break;
            }
        }
        for (unsigned i = hash +1; i < STRING_TABLE_SIZE; i++ ) {
            for (_Entry *e = _table[i]; e!= NULL; e= e->next) {
                if (e->str.intern.raw_ref) {
                    *cookie = i;
                    return &(e->str);
                }
            }
        }
    }
    return NULL;
} //String_Pool::get_next_string_intern
