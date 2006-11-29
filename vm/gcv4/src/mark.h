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

/**
 * @file 
 * Routines to support object marking.
 */

#ifndef _MARK_H
#define _MARK_H

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.mark"
#include "cxxlog.h"
#include "port_atomic.h"
#include "characterize_heap.h"

// for vtable_get_class
#include "open/vm_gc.h"

// returns address correcsponding bit index in specified block
inline void*
block_address_from_mark_bit_index(block_info *block, unsigned int bit_index) {
    POINTER_SIZE_INT data = (POINTER_SIZE_INT)block + GC_BLOCK_INFO_SIZE_BYTES;
    return (void*) (data + (bit_index * GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES));
}

// clear mark bit vector in block
inline void
clear_block_mark_bit_vector(block_info *block) {
    memset(block->mark_bit_vector, 0, GC_SIZEOF_MARK_BIT_VECTOR_IN_BYTES);
}


static inline bool object_info_is_not_zero(Partial_Reveal_Object * p_obj) {
    return p_obj->get_obj_info() != 0;
}

static inline void
get_mark_byte_and_mark_bit_index_for_object(Partial_Reveal_Object * p_obj,
                                            unsigned int *p_byte_index,
                                            unsigned int *p_bit_index)
{
    ASSERT_OBJECT(p_obj)
    unsigned int object_index_into_block =
        (unsigned int) (GC_LIVE_OBJECT_CARD_INDEX_INTO_GC_BLOCK_NEW(p_obj));
    ASSERT(object_index_into_block < GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK_DATA,
           "p_obj = " << p_obj << "\n" "object_index_into_block = " <<
           object_index_into_block << "\n"
           "GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK_DATA = " <<
           GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK_DATA);

    // Check that the bit conversion STUFF has two-way integrity.....
    ASSERT(p_obj == block_address_from_mark_bit_index(GC_BLOCK_INFO(p_obj), object_index_into_block),
            "p_obj = " << p_obj);

    *p_byte_index = object_index_into_block / GC_NUM_BITS_PER_BYTE;
    *p_bit_index = object_index_into_block & (GC_NUM_BITS_PER_BYTE - 1);

    assert(*p_bit_index == (object_index_into_block % GC_NUM_BITS_PER_BYTE));
    assert(object_index_into_block ==
           ((*p_byte_index * GC_NUM_BITS_PER_BYTE) + *p_bit_index));
}

// returns true if it was us who have marked the object
static inline bool mark_object(Partial_Reveal_Object * p_obj)
{
    unsigned int object_index_byte = 0;
    unsigned int bit_index_into_byte = 0;

    ASSERT((p_obj->get_obj_info() & FORWARDING_BIT_MASK) == 0,
           "obj_info = " << (void *) (POINTER_SIZE_INT) p_obj->
           get_obj_info() << ", " << " FORWARDING_BIT_MASK = " << (void *)
           FORWARDING_BIT_MASK << "\n" <<
           class_get_name(vtable_get_class((VTable *) p_obj->vt())) << " at "
           << p_obj <<
           (((void *) (POINTER_SIZE_INT) p_obj->get_obj_info() >=
             p_global_gc->get_gc_heap_base_address()
             && (void *) (POINTER_SIZE_INT) p_obj->get_obj_info() <
             p_global_gc->
             get_gc_heap_ceiling_address())? ("\n-----------------\n"
                                              "Possible enumeration bug: the obj_info field looks like a forwarding pointer\n"
                                              "in the old copy of an object")
            : "")
        );                      // This might break for Linux if stacks are allocated in the upper 2 gig.
    // If it does break we need to either
    // 1. Adjust the bits in the header so the GC has a bit
    //    that is not used by other parts of the VM or by an address.
    // 2. Adjust the algorithm so that there is an additional
    //    pass of the objects that have non-zero values in the obj_info field so
    //    that these values are saved prior to the colocation work being done.
    // 3. Use "bits are bits" hack.")
    //
    // This also might break in case of enumeration or slot update bugs,
    // if the p_obj points to old copy of a moved object, with forwarding pointer
    // in obj_info field.

    get_mark_byte_and_mark_bit_index_for_object(p_obj, &object_index_byte,
                                                &bit_index_into_byte);

    volatile uint8 *p_byte =
        &(GC_BLOCK_INFO(p_obj)->mark_bit_vector[object_index_byte]);

    uint8 mask = (uint8) (1 << bit_index_into_byte);

    while (true) {
        uint8 old_val = *p_byte;
        uint8 final_val = (uint8) (old_val | mask);

        // object is already marked by someone else
        if (old_val == final_val) return false;

        if (port_atomic_cas8((volatile uint8 *) p_byte, final_val, old_val)
                == old_val) {

            // in debug and windows mode only
            heapTraceObject(p_obj, get_object_size_bytes(p_obj));
            gc_trace(p_obj, "is marked");
            // we marked the object
            return true;
        }
    } // while
}


static inline bool is_object_marked(Partial_Reveal_Object * p_obj)
{
    unsigned int object_index_byte;
    unsigned int bit_index_into_byte;

    get_mark_byte_and_mark_bit_index_for_object(p_obj, &object_index_byte,
                                                &bit_index_into_byte);

    volatile uint8 *p_byte =
        &(GC_BLOCK_INFO(p_obj)->mark_bit_vector[object_index_byte]);

    if ((*p_byte & (1 << bit_index_into_byte)) == (1 << bit_index_into_byte)) {
        // Object is marked
        return true;
    }
    else {
        return false;
    }
}

#endif // _MARK_H
