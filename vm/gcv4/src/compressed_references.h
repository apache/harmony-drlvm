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


#ifndef _compressed_references_h
#define _compressed_references_h

#include "gc_v4.h"
#include "open/gc.h" // for the declaration of gc_heap_base_address()

// Set the DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES define to force a build that supports either
// compressed references or raw references, but not both.
// Also set the gc_references_are_compressed define to either true or false.
//#define DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
// 
// See also Makefile or ant build files for platform-specific definitions

#ifdef DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
#define gc_references_are_compressed false
#else // !DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
extern bool gc_references_are_compressed;
#endif // !DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES


//
// The Slot data structure represents a pointer to a heap location that contains
// a reference field.  It is packaged this way because the heap location may
// contain either a raw pointer or a compressed pointer, depending on command line
// options.
//
// The "check_within_heap" parameter to the constructor and the set() routine
// should be set to false when doing the full heap verification, when creating
// a slot that points to an object that is a copy of something that was originally
// in the heap.
//
// Code originally of the form:
//     Partial_Reveal_Object **p_slot = foo ;
//     ... *p_slot ...
// should be changed to this:
//     Slot p_slot(foo);
//     ... p_slot.dereference() ...
//
// The dereference() method still returns a raw Partial_Reveal_Object* pointer,
// which can have vt() and other operations performed on it as before.
//
typedef class Slot {
private:
    union {
        Partial_Reveal_Object **raw;
        uint32 *cmpr;
        void *value;
    } content;
    static void *cached_heap_base;
    static void *cached_heap_ceiling;

public:
    Slot(void *v, bool check_within_heap=true) {
        set(v, check_within_heap);
    }


    // Sets the raw value of the slot.
    void *set(void *v, bool check_within_heap=true) {
        content.value = v;
        if (v != NULL) {
            // Make sure the value points somewhere within the heap.
            if (check_within_heap) {
                ASSERT(v >= cached_heap_base, "v = " << v 
                    << ", cached_heap_base = " << cached_heap_base);
                ASSERT(v < cached_heap_ceiling, "v = " << v
                    << ", cached_heap_ceiling = " << cached_heap_ceiling);
                // Make sure the value pointed to points somewhere else within the heap.
                assert(dereference() == managed_null() ||
                    (dereference() >= cached_heap_base && dereference() < cached_heap_ceiling));
            }
        }
        return v;
    }


    // Dereferences the slot and converts it to a raw object pointer.
    Partial_Reveal_Object *dereference() {
        if (gc_references_are_compressed) {
            assert(content.cmpr != NULL);
            return (Partial_Reveal_Object *) (*content.cmpr + (Byte*)cached_heap_base);
        } else {
            assert(content.raw != NULL);
            return *content.raw;
        }
    }


    // Returns the raw pointer value.
    void *get_address() { return content.value; }


    // Writes a new object reference into the slot.
    void update(Partial_Reveal_Object *obj) {
        assert(NULL == obj || obj >= cached_heap_base);
        assert(obj < cached_heap_ceiling);
        if (gc_references_are_compressed) {
            *content.cmpr = (uint32) ((Byte *)obj - (Byte*)cached_heap_base);
        } else {
            *content.raw = obj;
        }
    }


    // Returns true if the slot points to a null reference.
    bool is_null() {
        if (gc_references_are_compressed) {
            assert(content.cmpr != NULL);
            return (*content.cmpr == 0);
        } else {
            assert(content.raw != NULL);
            return (*content.raw == NULL);
        }
    }


    // Returns the raw value of a managed null, which may be different
    // depending on whether compressed references are used.
    static void *managed_null() {
        return (gc_references_are_compressed ? cached_heap_base : NULL);
    }


    // Returns the maximum heap size supported, which depends on the
    // configuration of compressed references.
    // If not using compressed references, the heap size is basically unlimited.
    // If using compressed references, normally the heap size is limited
    // to 4GB, unless other tricks are used to extend it to 8/16/32GB.
    static uint64 max_supported_heap_size() {
        if (gc_references_are_compressed) {
            return ((uint64)1)<<32; // 4GB
        } else {
            return ~((uint64)0); // unlimited
        }
    }


    // Returns the additional amount of memory that should be requested from
    // VirtualAlloc so that the heap can be aligned at a 4GB boundary.
    // The advantage of such alignment is tbat tbe JIT-generated code doesn't
    // need to subtract the heap base when compressing a reference; instead,
    // it can just store the lower 32 bits.
    static uint64 additional_padding_for_heap_alignment()
    {
        if (gc_references_are_compressed) {
            return ((uint64)1)<<32; // 4GB
        } else {
            return 0;
        }
    }


    // Initializes the cached heap base and ceiling to the specified values.
    // Other required initialization may be done here as well.
    static void init(void *base, void *ceiling) {
        cached_heap_base = base;
        cached_heap_ceiling = ceiling;
    }
} Slot;


#endif // _compressed_references_h
