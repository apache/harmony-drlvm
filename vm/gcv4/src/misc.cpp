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
 * @version $Revision: 1.1.2.3.4.3 $
 */  

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>
using namespace std;

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
#include "work_packet_manager.h"
#include "gc_thread.h"
#include "object_list.h"
#include "garbage_collector.h"
#include "descendents.h"
#include "mark.h"
#include "gc_globals.h"

//////////////////////////////////  V E R I F Y ////////////////////////////////


int num_marked = 0;

static inline bool 
mark_object_header(Partial_Reveal_Object *p_obj)
{
    volatile POINTER_SIZE_INT val = p_obj->get_obj_info();
    
    if (val & GC_OBJECT_MARK_BIT_MASK) {
        // Somebody has already marked it, or I might have previously
        return false;
    }
    p_obj->set_obj_info(p_obj->get_obj_info() | GC_OBJECT_MARK_BIT_MASK);
    return true;
}

void 
Garbage_Collector::trace_verify_sub_heap(Partial_Reveal_Object *p_obj, bool before_gc, MarkStack& stack)
{
    assert(p_obj >= get_gc_heap_base_address()
        && p_obj < get_gc_heap_ceiling_address());
    if (mark_object_header(p_obj)) {

        live_size += get_object_size_bytes(p_obj);
        
        // verify the newly found object
        verify_object(p_obj, get_object_size_bytes(p_obj));
        
        if (before_gc) {
            _num_live_objects_found_by_first_trace_heap++;
            assert(_live_objects_found_by_first_trace_heap);
            _live_objects_found_by_first_trace_heap->add_entry(p_obj);
        } else {
            _num_live_objects_found_by_second_trace_heap++;
            assert(_live_objects_found_by_second_trace_heap);
            _live_objects_found_by_second_trace_heap->add_entry(p_obj);
        }
    } else {
        return;
    }
    
    if (is_array(p_obj)) {
        if (is_array_of_primitives(p_obj)) {
            return;
        }
        int32 array_length = vector_get_length((Vector_Handle)p_obj);
        for (int32 i=array_length-1; i>=0; i--)
        {
            Slot p_element(vector_get_element_address_ref((Vector_Handle)p_obj, i));
            if (!p_element.is_null()) {
                stack.push(p_element.dereference());
            }
        }
        
    } else {
        int *offset_scanner = init_strong_object_scanner (p_obj);
        Slot pp_target_object(NULL);
        
        while (pp_target_object.set(p_get_ref(offset_scanner, p_obj)) != NULL) {
            // Move the scanner to the next reference.
            offset_scanner = p_next_ref (offset_scanner);
            if (!pp_target_object.is_null()) {
                stack.push(pp_target_object.dereference());
            }
        }
    }
}

unsigned int 
Garbage_Collector::trace_verify_heap(bool before_gc)
{
    // reset the live size
    live_size = 0;

    Partial_Reveal_Object *p_obj = NULL;
    // Reset instead of deleting and reallocating.
    if (before_gc) {
        _num_live_objects_found_by_first_trace_heap = 0;
        if (_live_objects_found_by_first_trace_heap == NULL) {
            _live_objects_found_by_first_trace_heap = new Object_List();
        }
        _live_objects_found_by_first_trace_heap->reset();
        assert(_live_objects_found_by_first_trace_heap);
    } else {
        _num_live_objects_found_by_second_trace_heap = 0;
        if (_live_objects_found_by_second_trace_heap == NULL) {
            _live_objects_found_by_second_trace_heap = new Object_List();
        }
        _live_objects_found_by_second_trace_heap->reset();
        assert(_live_objects_found_by_second_trace_heap);
    }
    
    MarkStack stack;
    std::vector<Partial_Reveal_Object**>::iterator i;
    for (i = verify_root_set.begin(); i != verify_root_set.end(); ++i) {
        p_obj = *(*i);
        if (p_obj) {
            assert(p_obj >= get_gc_heap_base_address() 
                && p_obj < get_gc_heap_ceiling_address());
            assert(GC_BLOCK_INFO(p_obj)->in_nursery_p 
                || GC_BLOCK_INFO(p_obj)->in_los_p 
                || GC_BLOCK_INFO(p_obj)->is_single_object_block);
            stack.push(p_obj);
        }
    }

    std::list<Partial_Reveal_Object*>::iterator j;
    for (j = m_listFinalize->begin(); j != m_listFinalize->end(); j++) {
        p_obj = *j;
        assert(p_obj);
        assert(p_obj >= get_gc_heap_base_address() 
            && p_obj < get_gc_heap_ceiling_address());
        assert(GC_BLOCK_INFO(p_obj)->in_nursery_p 
            || GC_BLOCK_INFO(p_obj)->in_los_p 
            || GC_BLOCK_INFO(p_obj)->is_single_object_block);
        stack.push(p_obj);
    }

    while (!stack.empty()) {
        p_obj = stack.top();
        stack.pop();
        trace_verify_sub_heap(p_obj, before_gc, stack);
    }
    
    p_obj = NULL;
    
    unsigned int result = 0;
    if (before_gc) {
        _live_objects_found_by_first_trace_heap->rewind();
        while ((p_obj = _live_objects_found_by_first_trace_heap->next())) {
            assert((p_obj->get_obj_info()) & GC_OBJECT_MARK_BIT_MASK);
            p_obj->set_obj_info((p_obj->get_obj_info()) & ~GC_OBJECT_MARK_BIT_MASK);
        }
        result = _num_live_objects_found_by_first_trace_heap;
    } else {
        _live_objects_found_by_second_trace_heap->rewind();
        while ((p_obj = _live_objects_found_by_second_trace_heap->next())) {
            assert((p_obj->get_obj_info()) & GC_OBJECT_MARK_BIT_MASK);
            p_obj->set_obj_info((p_obj->get_obj_info()) & ~GC_OBJECT_MARK_BIT_MASK);
        }
        result = _num_live_objects_found_by_second_trace_heap;
    }
    
    return result;
}


void
verify_marks_for_live_object(Partial_Reveal_Object *p_obj) 
{
    unsigned int object_index_byte = 0;
    unsigned int bit_index_into_byte = 0;
    get_mark_byte_and_mark_bit_index_for_object(p_obj, &object_index_byte, &bit_index_into_byte);
    uint8 *p_byte = &(GC_BLOCK_INFO(p_obj)->mark_bit_vector[object_index_byte]);
    
    // the base of the object is marked...
    if ((*p_byte & (1 << bit_index_into_byte)) == 0) {
        // We have a problem, lets try to focus in on it first - is this a legal object?
        // This is never called while collecting characterizations.
        verify_object(p_obj, get_object_size_bytes(p_obj));
        WARN("verify_marks_for_live_object() failed for base of object " << p_obj << "\n"
            << "\tIn block " << GC_BLOCK_INFO(p_obj));
        int x = *p_byte;
        WARN("The byte is " << x << " Now verify object and draw line.");
        verify_object(p_obj, get_object_size_bytes(p_obj));
        WARN("-----------------------------");
    }
    return;
}



void 
Garbage_Collector::verify_marks_for_all_lives()
{
    if (verify_gc) {
        // If the verifier is turned on, use the trace heap utility.
        if (_live_objects_found_by_first_trace_heap) {
            _live_objects_found_by_first_trace_heap->rewind();
            Partial_Reveal_Object *p_obj = NULL;
            while ((p_obj = _live_objects_found_by_first_trace_heap->next())) {
                verify_marks_for_live_object(p_obj);            
            } // while
        } // if
    } // if
}


//////////////////////////////////  D U M P   R O U T I N E S //////////////////////////////////


#define NUM_OBJECTS_PER_LINE 4

static FILE *fp = NULL;

void
close_dump_file()
{
    assert(fp);
    fclose(fp);
    fp = NULL;
}

// dumps to the screen all lives in a compacted block after compaction is complete..
void 
dump_object_layouts_in_compacted_block(block_info * UNREF block, unsigned int UNREF num_lives)
{
    // This is not used and is broken since pair tables are now SSB structures that
    // don't remove duplicates
    ABORT("Not supported"); 
}


//////////////////////////////////  T R A C E   R O U T I N E S ////////////////////////////////

// gc_v4.h references these routines for debug, for non-debug they are inlined into empty routines
#ifdef _DEBUG
// If GC_DEBUG is 0 then this is an inline empty latency free routine..

// We have a selection of 4 objects that are normally NULL.
// If we want to trace an object we set the address of one
// of the objectn to the object we want to trace.

void *object1 = (void *)0x0;                // The bogus object
void *object2 = (void *)0x0;                // the object that should be moved to the bogus object    //  
void *object3 = (void *)0x0;                // The object that obliterate the object to be moved tot eh bogus object. ..
void *object4 = (void *)0x0;                // The bogus slot in company points to the middle of this object
void *object5 = (void *)0x0;
void *object6 = (void *)0x0;
void *object7 = (void *)0x0;

#undef LOG_DOMAIN
#define LOG_DOMAIN "gc.trace"

// Set this object to be traced if we aren't already tracing 4 objects.
void trace_object (void *obj_to_trace)
{
    TRACE("starting tracing of " << obj_to_trace);
    if (!object1) {
        object1 = obj_to_trace;
    } else if (!object2) {
        object2 = obj_to_trace;
    } else if (!object3) {
        object3 = obj_to_trace;
    } else if (!object4) {
        object4 = obj_to_trace;
    } else {
        TRACE("We can trace only 4 objects for now, this one ignored. ");
    }
}

// This is called from a lot of places and if the object passed in
// and non-NULL and equal one of the objects being traced then
// string_x is printed out along with the object. 
bool is_object_traced(void *object) {
    // The NULL object is not interesting
    if (!object) {
        return false;
    }

    return ((object==object1)||(object==object2)||(object==object3)
            ||(object==object4)||(object==object5)||(object==object6)
            ||(object==object7));
}

// Trace all allocations in a specific block. This is valuable when trying to find objects allocated in the same block as a problem
// object or perhaps a bogus address.
block_info *block_alloc_1 = (block_info *)0x0; //0x000006fbf6ff0000; // The interesting compress enumeration bug items

void gc_trace_allocation (void *object, const char *string_x)
{
    if (object) {
        if (GC_BLOCK_INFO(object) == block_alloc_1) {
            TRACE("In gc trace block allocation with object " << object << string_x);
        }
        gc_trace (object, string_x);
    }
}

void **object_slot1 = (void **)0x0;             // 00006fbfdb80118; // These are the interesting compress enumeration bug items
void **object_slot2 = (void **)0x0;
void **object_slot3 = (void **)0x0;
void **object_slot4 = (void **)0x0;
void **object_slot5 = (void **)0x0;

void gc_trace_slot (void **object_slot, void *object, const char *string_x) {
    
    if ((object_slot==object_slot1)||(object_slot==object_slot2)||(object_slot==object_slot3)
        ||(object_slot==object_slot4)||(object_slot==object_slot5)) {
        if (!object) {
            object = (void *) "NULL";
        }
        TRACE(" GC Slot Trace " << object_slot << " with value " << object << " " << string_x);
    }
    return;
}

block_info *block1 = (block_info *)0x0;  // (block_info *)0x000006fbf6ff0000; // These are the interesting compress enumeration bug items
block_info *block2 = (block_info *)0x0;
block_info *block3 = (block_info *)0x0;
block_info *block4 = (block_info *)0x0;

void gc_trace_block (void *the_block, const char *string_x) {
    block_info *b = (block_info *)the_block;
    assert (the_block);
    if ((b == block1) || (b == block2) || (b == block3) || (b == block4)) {
        TRACE((void *)the_block << " " << string_x  << "------ block trace --------");
    }
}

#endif // _DEBUG 



bool is_vtable(Partial_Reveal_VTable *p_vtable)
{
    if (p_vtable == 0) {
        return false;
    }
    
    if ((POINTER_SIZE_INT)p_vtable < 0x0100) {
        // Pick up random low bits.
        return false;
    }
        // Best guess is that it is legal.
        return true;
}

//
// This API can be called at debug time by the VM to determine
// if the specified reference is to a live, valid Java object.
// The answer is approximate, however, since live objects can
// take the places of previous dead objects. (I.e. the response
// may occasionally mislead the VM.)
//

unsigned int
get_object_size_bytes(Partial_Reveal_Object *p_obj)
{
    bool arrayp = is_array (p_obj);
    unsigned int sz;
    if (arrayp) {
        sz = vm_vector_size(p_obj->vt()->get_gcvt()->gc_clss, vector_get_length((Vector_Handle)p_obj));
        return sz; 
    } else {
            return p_obj->vt()->get_gcvt()->gc_allocated_size;
    }
}

// end file misc.cpp

