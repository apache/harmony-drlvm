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
#include "gc_globals.h"
#include "gc_thread.h"
#include "mark.h"
#include "descendents.h"
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//#ifdef GC_MARK_SCAN_POOLS


// Convert all roots into packets and drop them into the mark scan pool....
void 
Garbage_Collector::setup_mark_scan_pools()
{
    DIE("GC::setup_mark_scan_pools: The code is not tested");
    assert(_mark_scan_pool);

    unsigned int num_root_objects_per_packet = root_set.size() / get_num_worker_threads();

    Work_Packet *wp = _mark_scan_pool->get_output_work_packet();
    assert(wp);

    unsigned int roots_in_this_packet = 0;

    RootSet::iterator i;
    for (i = root_set.begin(); i != root_set.end(); ++i) {
        Partial_Reveal_Object **slot = *i;
        if (*slot == NULL) {
            continue;
        }
        // Mark the object. 
        if (mark_object(*slot)) {
            // Check if packet is empty
            if ((wp->is_full() == true) || (num_root_objects_per_packet == roots_in_this_packet)) {
                // return this packet and get a new output packet
                _mark_scan_pool->return_work_packet(wp);
                wp = _mark_scan_pool->get_output_work_packet();
                roots_in_this_packet = 0;
            }
            assert(wp && !wp->is_full());
            wp->add_unit_of_work(*slot);        
            roots_in_this_packet++;
        }   
        // the object may have a already been marked if an earlier root pointed to it.
        // In this case we dont duplicate this object reference in the mark scan pool
    }
    // return the retained work packet to the pool
    assert(wp);
    _mark_scan_pool->return_work_packet(wp);
}

void 
Garbage_Collector::scan_slot(Slot p_slot, GC_Mark_Activity *gc_thread)
{
    DIE("GC::scan_slot: The code is not tested");
    assert(p_slot.get_address());

    if (p_slot.is_null()) {
        return;
    }
    Partial_Reveal_Object *p_obj = p_slot.dereference();

    gc_trace(p_obj, " scan_slot(): some slot pointed to this object...");

    Work_Packet *o_wp = gc_thread->get_output_packet();
    assert(o_wp);

    // I will try to be the first to mark it. If success..I will add it to my output packet
    if (mark_object(p_obj)) {
        if (o_wp->is_full() == true) {
            // output packet is full....return to pool and a get a new output packet
            _mark_scan_pool->return_work_packet(o_wp);
            gc_thread->set_output_packet(NULL);
            o_wp = _mark_scan_pool->get_output_work_packet();
            gc_thread->set_output_packet(o_wp);
        }
        assert(o_wp && !o_wp->is_full());
        o_wp->add_unit_of_work(p_obj);
    }

    if (gc_thread->is_compaction_turned_on_during_this_gc()) {  // Collect slots as we go
        block_info *p_obj_block_info = GC_BLOCK_INFO(p_obj);
        if (gc_thread->_p_gc->is_compaction_block(p_obj_block_info)) {
            gc_thread->_p_gc->_p_block_store->
                add_slot_to_compaction_block(p_slot, p_obj_block_info, gc_thread->get_id());

            if (stats_gc) {
                gc_thread->increment_num_slots_collected(); 
            }
        }
    }
}

void 
Garbage_Collector::mark_scan_pools(GC_Thread *gc_thread)
{
    DIE("GC::mark_scan_pools: The code is not tested");

    // thread has no output packet yet
    assert(gc_thread->get_output_packet() == NULL);

    unsigned int num_objects_scanned_by_thread = 0;

    // get a thread a new output packet
    Work_Packet *o_wp = _mark_scan_pool->get_output_work_packet();
    assert(o_wp);
    gc_thread->set_output_packet(o_wp);

    while (true) {
        // try to get work-packet from common mark/scan pool
        Work_Packet *i_wp = _mark_scan_pool->get_input_work_packet();
        
        if (i_wp == NULL) {
            //...return output packet if non-empty
            Work_Packet *o_wp = gc_thread->get_output_packet();
            if (o_wp->is_empty() == false) {
                _mark_scan_pool->return_work_packet(o_wp);
                gc_thread->set_output_packet(NULL);
                o_wp = _mark_scan_pool->get_output_work_packet();
                gc_thread->set_output_packet(o_wp);
                continue;   // try to get more work
            } else {
                // NO TILL NO WORK......leave..or else keep working...
                bool there_is_work = wait_till_there_is_work_or_no_work();
                if (there_is_work == false) {
                    // No more work left.....
                    break;
                } else {
                    // there seems to be some work...try again...
                    continue;
                }
            }
        }
        if (i_wp->is_empty() == true) {
            // try to get some real work again..
            continue;
        }

        assert(gc_thread->get_input_packet() == NULL);
        gc_thread->set_input_packet(i_wp);

        // iterate through the work packet
        i_wp->init_work_packet_iterator();
        Partial_Reveal_Object *p_obj = NULL;

        while ((p_obj = (Partial_Reveal_Object *)i_wp->remove_next_unit_of_work())) {

            // Object had better be marked....since it is grey
            assert(is_object_marked(p_obj) == true);

            num_objects_scanned_by_thread++;

            if (p_obj->vt()->get_gcvt()->gc_object_has_slots) {
                if (is_array(p_obj)) {
                    if (is_array_of_primitives(p_obj)) {
                        break;  // no references here..
                    }
                    int32 array_length = vector_get_length((Vector_Handle)p_obj);
                        for (int32 i = array_length - 1; i >= 0; i--)   {
                            Slot p_element(vector_get_element_address_ref((Vector_Handle)p_obj, i));
                            scan_slot(p_element, gc_thread);
                        }
                } else {
                    // not an array
                    int *offset_scanner = init_strong_object_scanner (p_obj);
                    Slot pp_target_object(NULL);
                    while (pp_target_object.set(p_get_ref(offset_scanner, p_obj)) != NULL) {
                        // Move the scanner to the next reference.
                        offset_scanner = p_next_ref (offset_scanner);
                        scan_slot (pp_target_object, gc_thread);
                    }
                }
            } // if

        } // while (unit)

        // we have run through the input work packet...lets return it to the shared pool
        assert(i_wp->is_empty());
        _mark_scan_pool->return_work_packet(i_wp);
        assert(gc_thread->get_input_packet() == i_wp);
        gc_thread->set_input_packet(NULL);

    } // while(true)

    // we need to return any input and output packets that we still hold
    assert(gc_thread->get_input_packet() == NULL);
    assert(gc_thread->get_output_packet() != NULL);

    o_wp = gc_thread->get_output_packet();
    assert(o_wp);
    _mark_scan_pool->return_work_packet(o_wp);
    gc_thread->set_output_packet(NULL);

    INFOW(gc_thread, "scanned "
        << num_objects_scanned_by_thread << " objects...");
}

//#endif //     GC_MARK_SCAN_POOLS

void 
Garbage_Collector::_verify_gc_threads_state()
{
    DIE("GC::_verify_gc_threads_state: The code is not tested");

    for (unsigned int i = 0; i < get_num_worker_threads() ; i++) {
        GC_Thread *gc_thr = _gc_threads[i];
        if (gc_thr->get_input_packet() != NULL) {
            DIE("GC thread " << gc_thr->get_id() 
                << " has remaining possibly unprocessed input packet with "
                << gc_thr->get_input_packet()->get_num_work_units_in_packet() 
                << " units of work...");
        }
        if (gc_thr->get_output_packet() != NULL) {
            DIE("GC thread " << gc_thr->get_id() 
                << " has remaining possibly unprocessed output packet with "
                << gc_thr->get_output_packet()->get_num_work_units_in_packet() 
                << " units of work...");
        }
    }
}
