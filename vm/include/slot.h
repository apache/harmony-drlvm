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
 * @version $Revision$
 */  
#ifndef _SLOT_H_
#define _SLOT_H_

/**
 * @file
 * Define the Slot structure to read the object fields of reference type.
 *
 * This data layout is statically shared between all VM components.
 */

#include "open/gc.h"        // for the declaration of gc_heap_base_address()
#include "open/vm_util.h"   // VM_Global_State
#include "environment.h"    // Global_Env

// (this file is based on gc_v4/src/compressed_references.h)
//
// The Slot data structure represents a pointer to a heap location that contains
// a reference field.  It is packaged this way because the heap location may
// contain either a raw pointer or a compressed pointer, depending on command line
// options.
//
// Code originally of the form:
//     ManagedObject **p_slot = foo ;
//     ... *p_slot ...
// can be expressed as:
//     Slot p_slot(foo);
//     ... p_slot.dereference() ...

class Slot {
private:
    union {
        void **raw;
        uint32 *compressed;
        void *value;
    } content;

    static void* heap_base;
    static void* heap_ceiling;

public:
    Slot(void *v) {
        set_address(v);
    }

    static void init(void* base, void* ceiling)
    {
        heap_base = base;
        heap_ceiling = ceiling;
    }

    // Sets the raw value of the slot.
    void *set_address(void *v) {
        content.value = v;
        return v;
    }

    // Returns the raw pointer value.
    void *get_address() { return content.value; }

    // Dereferences the slot and converts it to a raw object pointer.
    void *dereference() {
        if (VM_Global_State::loader_env->compress_references) {
            assert(content.compressed != NULL);
            return (void*)((UDATA)*content.compressed + (UDATA)heap_base);
        } else {
            assert(content.raw != NULL);
            return *content.raw;
        }
    }

    // Writes a new object reference into the slot.
    void write(void *obj) {
        if (VM_Global_State::loader_env->compress_references) {
            if (obj != NULL) {
                *content.compressed = (uint32) ((UDATA)obj - (UDATA)heap_base);
            } else {
                *content.compressed = 0;
            }
        } else {
            *content.raw = obj;
        }
    }

    // Returns true if the slot points to a null reference.
    bool is_null() {
        if (VM_Global_State::loader_env->compress_references) {
            assert(content.compressed != NULL);
            return (*content.compressed == 0);
        } else {
            assert(content.raw != NULL);
            return (*content.raw == NULL);
        }
    }

    // Returns the raw value of a managed null, which may be different
    // depending on whether compressed references are used.
    static void *managed_null() {
        return (VM_Global_State::loader_env->compress_references ? heap_base : NULL);
    }

};

#endif // _SLOT_H_
