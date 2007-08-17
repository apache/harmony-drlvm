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
#include "open/vm_util.h"

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

#ifndef PLATFORM_POSIX
#include "process.h"
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void insert_scanned_compaction_objects_into_compaction_blocks(GC_Thread *);
extern void allocate_forwarding_pointers_for_compaction_live_objects(GC_Thread *);
// extern void allocate_forwarding_pointers_for_colocated_live_objects(GC_Thread *);

extern void fix_slots_to_compaction_live_objects(GC_Thread *);
extern void slide_cross_compact_live_objects_in_compaction_blocks(GC_Thread *);

// extern void slide_compact_live_objects_in_same_compaction_blocks(GC_Thread *);
extern void restore_hijacked_object_headers(GC_Thread *);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool mark_scan_load_balanced;
extern bool cross_block_compaction;
extern bool sweeps_during_gc;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int __cdecl gc_thread_func (void *arg);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

GC_Mark_Activity::GC_Mark_Activity(Garbage_Collector *p_gc) {
    assert(p_gc);
    _p_gc = p_gc;

    // Create a mark stack per thread.
    _mark_stack = new Mark_Stack(16, 65536);

    // FIXME: should be private
    _compaction_turned_on_during_this_gc = false;

    _num_slots_collected = 0;
    _output_packet = NULL;
    _input_packet = NULL;
    _id = 1; // FIXME ivan 20051026: concurent mark scan uses thread id = 1 for all mutator threads
}

GC_Mark_Activity::~GC_Mark_Activity() {
    delete _mark_stack;
    assert(_output_packet == NULL);
    assert(_input_packet == NULL);
}


hythread_group_t gc_thread_group = NULL;
hythread_group_t get_thread_group () {
    if (!gc_thread_group) {
        IDATA UNUSED stat = hythread_group_create(&gc_thread_group);
        assert(stat == TM_ERROR_NONE);
    }
    return gc_thread_group;
}

GC_Thread::GC_Thread(Garbage_Collector *p_gc, int gc_thread_id)
    : GC_Mark_Activity(p_gc)
{

    _id = gc_thread_id;


    stash = NULL;

    if (sweeps_during_gc) {
        _sweep_start_index = -1;
        _num_chunks_to_sweep = -1;
#ifdef _DEBUG
        zero_out_sweep_stats((chunk_sweep_stats *) &(_sweep_stats));
#endif
    }
        
    //////////////////
    IDATA stat = hysem_create(&_gc_thread_start_work_event, 0, 1); 
    assert(stat == TM_ERROR_NONE);

    //////////////////
    stat = hysem_create(&_gc_thread_work_done_event, 0, 1); 
    assert(stat == TM_ERROR_NONE);
    _thread_handle=NULL;
    stat = hythread_create(&_thread_handle, 0, 0, 0, gc_thread_func, this);
    if (stat != TM_ERROR_NONE) { 
        DIE("GC_Thread::GC_Thread(..): CreateThread() failed...exiting...");
    }

    // Reset every GC
    _num_bytes_recovered_by_sweep = 0;

/////////////////////////////////////////////////// 
    _object_headers = NULL;
    placement_chunk = NULL;
    placement_block = NULL;
}

GC_Thread::~GC_Thread()
{
    hysem_destroy(_gc_thread_start_work_event);
    hysem_destroy(_gc_thread_work_done_event);
}


void 
GC_Thread::reset(bool compact_this_gc)
{
    // There is no need to zero the mark stack between GCs since it is cleared each time a POP is done
    assert(_mark_stack->empty());

    if (sweeps_during_gc) {
        _sweep_start_index = -1;
        _num_chunks_to_sweep = -1;
#ifdef _DEBUG
        zero_out_sweep_stats((chunk_sweep_stats *) &(_sweep_stats));
#endif
    }

    IDATA rstat = hysem_set(_gc_thread_start_work_event,0);
    assert(rstat == TM_ERROR_NONE);
    rstat = hysem_set(_gc_thread_work_done_event,0);
    assert(rstat == TM_ERROR_NONE);
    _num_bytes_recovered_by_sweep = 0;

////////////////////////////////
    set_compaction_during_this_gc(compact_this_gc);
    assert(_object_headers == NULL);
    _num_slots_collected = 0;
}



void 
GC_Thread::wait_for_work()
{
    IDATA UNUSED wstat = hysem_wait(_gc_thread_start_work_event);        
    assert(wstat == TM_ERROR_NONE);
}



void 
GC_Thread::signal_work_is_done()
{
    // I am done with my job. SIGNAL MASTER THREAD
    IDATA UNUSED sstat = hysem_post(_gc_thread_work_done_event);
    assert(sstat == TM_ERROR_NONE);
}


volatile POINTER_SIZE_INT dummy_for_good_cache_performance = 0;             


int __cdecl gc_thread_func (void *arg)
{
    GC_Thread *p_gc_thread = (GC_Thread *) arg; 
    assert(p_gc_thread);
    apr_time_t task_start_time, task_end_time;

    while(true) {

        //I will go to sleep forever or until the next time I am woken up with work
        p_gc_thread->wait_for_work();
        // When thread is woken up, it has work to do.
        assert(p_gc_thread->get_task_to_do() != GC_BOGUS_TASK);


        if (p_gc_thread->get_task_to_do() == GC_MARK_SCAN_TASK) {
            gc_time_start_hook(&task_start_time);

            if (mark_scan_load_balanced) {
                LOG2("gc", "Staring marking pools");
                p_gc_thread->_p_gc->mark_scan_pools(p_gc_thread);
            } else {
                LOG2("gc", "Staring marking heap (no pools)");
                p_gc_thread->_p_gc->mark_scan_heap(p_gc_thread);
            }

            gc_time_end_hook("GC_MARK_SCAN_TASK", &task_start_time, &task_end_time);

        } else if (p_gc_thread->get_task_to_do() == GC_SWEEP_TASK) {

            if (sweeps_during_gc) {
                // Sweep part of the heap.
                assert(p_gc_thread->get_num_bytes_recovered_by_sweep() == 0);
                p_gc_thread->set_num_bytes_recovered_by_sweep(0);
                gc_time_start_hook(&task_start_time);
                unsigned int bytes_recovered = p_gc_thread->_p_gc->sweep_heap(p_gc_thread);
                p_gc_thread->set_num_bytes_recovered_by_sweep(bytes_recovered);
                INFOW(p_gc_thread, "recovered " << bytes_recovered << "bytes");
                gc_time_end_hook(": GC_SWEEP_TASK ", &task_start_time, &task_end_time);
            } else {
                DIE("SWEEP_TASK not expected during GC in the configuration in which you have built GC code");
            }

        }  else if (p_gc_thread->get_task_to_do() == GC_OBJECT_HEADERS_CLEAR_TASK) {

            DIE("BAD GC configuration");
            
        } else if (p_gc_thread->get_task_to_do() == GC_INSERT_COMPACTION_LIVE_OBJECTS_INTO_COMPACTION_BLOCKS_TASK) {


            DIE("BAD GC configuration");

        } else if (p_gc_thread->get_task_to_do() == GC_ALLOCATE_FORWARDING_POINTERS_FOR_COMPACTION_LIVE_OBJECTS_TASK) {
            
            allocate_forwarding_pointers_for_compaction_live_objects(p_gc_thread);

        } else if (p_gc_thread->get_task_to_do() == GC_FIX_SLOTS_TO_COMPACTION_LIVE_OBJECTS_TASK) {

            fix_slots_to_compaction_live_objects(p_gc_thread);

        } else if (p_gc_thread->get_task_to_do() == GC_SLIDE_COMPACT_LIVE_OBJECTS_IN_COMPACTION_BLOCKS) {

            if (cross_block_compaction) {
                slide_cross_compact_live_objects_in_compaction_blocks(p_gc_thread);
            } else {
                ABORT("Not supported");
            }

        } else if (p_gc_thread->get_task_to_do() == GC_RESTORE_HIJACKED_HEADERS) {

            restore_hijacked_object_headers(p_gc_thread);

        } else {
            ABORT("Unexpected GC thread action");
        }
    
        // Work is done!!!
        p_gc_thread->signal_work_is_done();

    }

    // get rid of remark -statement is unreachable
    return 0;
}


void 
GC_Thread::insert_object_header_info_during_sliding_compaction(object_lock_save_info *obj_header_info) 
{
    assert(obj_header_info);
    obj_header_info->p_next = _object_headers;
    _object_headers = obj_header_info;
}
