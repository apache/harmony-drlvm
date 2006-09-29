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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>

// VM interface header files
#include "platform_lowlevel.h"
#include "open/vm_gc.h"
#include "open/gc.h"

// GC header files
#include "gc_cout.h"
#include "gc_header.h"
#include "gc_v4.h"
#include "remembered_set.h"
#include "block_store.h"
#include "object_list.h"
#include "work_packet_manager.h"
#include "garbage_collector.h"
#include "descendents.h"

#include "compressed_references.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int object_lock_count = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////


//
// Verify slot
//
static void verify_slot(Slot p_slot)
{
    if (p_slot.is_null()) {
        return;
    }
    Partial_Reveal_Object * UNUSED the_obj = p_slot.dereference();
    
    // verify the block is in the heap and not on the free list and 
    // that the target object appears valid.
    assert (the_obj->vt());
}


void verify_object (Partial_Reveal_Object *p_object, POINTER_SIZE_INT UNREF size)
{
    assert(p_object);
    assert(p_object->vt()); 
    gc_trace(p_object, "Verifying this object.");
    ASSERT_OBJECT(p_object);

    int *offset_scanner; 
    Slot pp_target_object(NULL);
    // Loop through slots in array or objects verify what is in the slot
    if (is_array(p_object)) {
        if (is_array_of_primitives(p_object)) {
            return;
        }
        int32 array_length = vector_get_length((Vector_Handle)p_object);
        for (int32 i=array_length-1; i>=0; i--)
        {
            Slot p_element(vector_get_element_address_ref((Vector_Handle)p_object, i));
            verify_slot(p_element);
        }
        return;
    } // end while for arrays
    // It isn't an array, it is an object.
    offset_scanner = init_strong_object_scanner (p_object);
    assert(offset_scanner);
    if (*offset_scanner == 0) { // We are at the end of the offsets before we start.
        assert(!p_object->has_slots());
    } else {
        assert(p_object->has_slots());
    }
    while (pp_target_object.set(p_get_ref(offset_scanner, p_object)) != NULL) {
        // Move the scanner to the next reference.
        offset_scanner = p_next_ref (offset_scanner);
        verify_slot (pp_target_object);
    } // end while for objects

    // Make sure the object info field has a valid entry, if it holds a lock update the counter.
    if (verify_object_header((void *)p_object)) {
        object_lock_count++;
    }
    // We are done.
}

// This verifies the C code for get_next_set_bit() defined in gc_utils.cpp

void 
verify_get_next_set_bit_code(set_bit_search_info * UNREF info)
{ 
#ifndef _IPF_
#ifndef PLATFORM_POSIX
    // Verification code only for IA32 windows....

    uint8 *p_byte = info->p_start_byte;
    unsigned int start_bit_index = info->start_bit_index;
    uint8 *p_ceil = info->p_ceil_byte;

    uint8 *p_non_zero_byte = NULL;
    unsigned int set_bit_index = 0;

    __asm { 

                mov eax, [p_byte]           // eax holds byte ptr
                mov ecx, start_bit_index    // ecx holds bit index into byte
                mov edx, [p_ceil]           // limit ptr 

_LOOP_:         clc             
                bt [eax], ecx               // bit test
                jc _FOUND_SET_BIT_          // carry bit contains the bit value at eax[ecx]
                add ecx, 1                  // '0' ...,move to next bit
                cmp ecx, GC_NUM_BITS_PER_BYTE                   
                jge _INCR_BYTE_PTR_
                jmp _LOOP_

_INCR_BYTE_PTR_:add eax, 1                  // into next byte..increment byte ptr...
                mov ecx, 0                  // start search at bit index 0
                cmp eax, edx                // have we reached p_ceil ?
                jge _HIT_THE_CEILING_   
                jmp _LOOP_

_FOUND_SET_BIT_:mov [p_non_zero_byte], eax  // found a '1' within range...
                mov set_bit_index, ecx
                jmp _ALL_DONE_
                
_HIT_THE_CEILING_:
                mov [p_non_zero_byte], 0    // hit p_ceil
                mov set_bit_index, 0
                
_ALL_DONE_:     
    }

    // CHECKS
    assert(p_non_zero_byte == info->p_non_zero_byte);
    assert(set_bit_index == info->bit_set_index);

    if ((p_non_zero_byte != info->p_non_zero_byte) || (set_bit_index != info->bit_set_index)) {
        DIE("verify_get_next_set_bit_code() failed.....");
    }

    return;

#endif // PLATFORM_POSIX
#endif // _IPF_
}



void 
Block_Store::verify_no_duplicate_slots_into_compaction_areas()
{
    for (unsigned int i = 0; i < _number_of_blocks_in_heap; i++) {

        if (_blocks_in_block_store[i].is_compaction_block == true) {

            assert(_blocks_in_block_store[i].block->in_nursery_p);
            assert(_blocks_in_block_store[i].block->is_compaction_block);

            Remembered_Set *test_rem_set = new Remembered_Set();
            // verify that there are no duplicate slots into this block
            for (unsigned int j = 0; j < get_num_worker_threads(); j++) {
                Remembered_Set *some_slots = _blocks_in_block_store[i].per_thread_slots_into_compaction_block[j];
                if (some_slots) {
                    some_slots->rewind();
                    Slot one_slot(NULL);
                    while ((one_slot.set(some_slots->next().get_address()))) {
                        if (test_rem_set->is_present(one_slot)) {
                            // this is BAD!!!
                            DIE("verify_no_duplicate_slots_into_compaction_areas(): found duplicate slot -- " 
                                        << one_slot.get_address() << " into compaction block : " << _blocks_in_block_store[i].block);
                        } else {
                            // keep accumulating unique slots...
                            test_rem_set->add_entry(one_slot);
                        }
                    } // while for one rem set
                    some_slots->rewind();
                } // if there exist some slots
            } // for (num_cpus)
            // done with the use of this
            delete test_rem_set;
        }
    } // for blocks inside compaction limits
    // DONE!!!!
} // verify_no_duplicate_slots_into_compaction_areas()



// ***SOB LOOKUP*** Do the lookup in the _blocks_in_block_store. 

bool 
Garbage_Collector::obj_belongs_in_single_object_blocks(Partial_Reveal_Object *p_obj)
{
    unsigned int block_store_block_number = (unsigned int) (((POINTER_SIZE_INT) p_obj - (POINTER_SIZE_INT) get_gc_heap_base_address()) >> GC_BLOCK_SHIFT_COUNT);
    bool new_result = _p_block_store->is_single_object_block(block_store_block_number);

    return new_result;
}
