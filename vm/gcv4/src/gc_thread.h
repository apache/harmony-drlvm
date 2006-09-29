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


#ifndef _gc_thread_H_
#define _gc_thread_H_
#include "stash_block.h"
#include "gc_cout.h"
#include "faststack.h"
#include "work_packet_manager.h"
#include "open/hythread_ext.h"

class Garbage_Collector;

#define Mark_Stack FastStack<Partial_Reveal_Object * > 

class GC_Mark_Activity {
    public:
    GC_Mark_Activity(Garbage_Collector*);
    virtual ~GC_Mark_Activity();

    inline Mark_Stack *get_mark_stack() {
        return _mark_stack;
    }

    inline bool is_compaction_turned_on_during_this_gc() {
        return _compaction_turned_on_during_this_gc;
    }

    inline void set_compaction_during_this_gc(bool on) {
        _compaction_turned_on_during_this_gc = on;
    }

    inline void increment_num_slots_collected() {
        _num_slots_collected++;
    }

    inline unsigned int get_num_slots_collected() {
        return _num_slots_collected;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////

    inline Work_Packet *get_input_packet() {
        return _input_packet;
    }

    inline void set_input_packet(Work_Packet *wp) {
        _input_packet = wp;
    }


    inline Work_Packet *get_output_packet() {
        return _output_packet;
    }

    inline void set_output_packet(Work_Packet *wp) {
        _output_packet = wp;
    }

    // FIXME ivan 20051026: should not be there, but rather in GC_Thread
    // change to add_slot_to_compaction_block required!
    inline unsigned int get_id() {
        return _id;
    }


///////////////////////////////////////////////////////////////////////////////////////////////////////////

    // This doesnt need to be a public -- expose using an accessor
    Garbage_Collector *_p_gc;

    protected:
    // FIXME ivan 20051026: uncontrollable changes to the vars
    // should be made private
    bool _compaction_turned_on_during_this_gc;
    unsigned int _num_slots_collected;
    Mark_Stack *_mark_stack;

    // FIXME ivan 20051026: should not be there, but rather in GC_Thread
    // change to add_slot_to_compaction_block required!
    unsigned int _id;

    private:
    /////////////////////////////////////////////////////////////////////////////
    Work_Packet *_input_packet;
    Work_Packet *_output_packet;
};

class GC_Thread : public GC_Mark_Activity {

public:

    GC_Thread(Garbage_Collector *, int);

    virtual ~GC_Thread();

    void reset(bool);

    void wait_for_work();

    void signal_work_is_done();

    inline gc_thread_action get_task_to_do() {
        return _task_to_do;
    }

    inline void set_task_to_do(gc_thread_action task) {
        _task_to_do = task;
    }

    hythread_t  get_thread_handle() {
        return _thread_handle;
    }

    inline hysem_t  get_gc_thread_work_done_event_handle() {
        return _gc_thread_work_done_event;
    }

    inline hysem_t  get_gc_thread_start_work_event_handle() {
        return _gc_thread_start_work_event;
    }

    inline unsigned int get_num_bytes_recovered_by_sweep() {
        return _num_bytes_recovered_by_sweep;
    }

    inline void set_num_bytes_recovered_by_sweep(unsigned int bytes) {
        _num_bytes_recovered_by_sweep = bytes;
    }

    inline int get_sweep_start_index() {
        return _sweep_start_index;
    }

    inline void set_sweep_start_index(int index) {
        _sweep_start_index = index;
    }

    inline int get_num_chunks_to_sweep() {
        return _num_chunks_to_sweep;
    }

    inline void set_num_chunks_to_sweep(int num) {
        _num_chunks_to_sweep = num;
    }
    
    inline void add_to_num_chunks_to_sweep(int num) {
        _num_chunks_to_sweep += num;
    }
#ifdef _DEBUG
    inline void set_chunk_average_number_of_free_areas(unsigned int chunk_index, unsigned int num) {
        _sweep_stats[chunk_index].average_number_of_free_areas = num;
    }

    inline void set_chunk_average_size_per_free_area(unsigned int chunk_index, unsigned int sz) {
        _sweep_stats[chunk_index].average_size_per_free_area = sz;
    }
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////


    void insert_object_header_info_during_sliding_compaction(object_lock_save_info *);

    inline object_lock_save_info *object_headers_to_restore_at_end_of_sliding_compaction() {
        object_lock_save_info *ret = _object_headers;
        // these nodes are freed by slide_compact_live_objects_in_compaction_blocks()
        _object_headers = NULL;
        return ret;
    }

    inline Stash_Block *get_stash_block () {
        assert(stash);
        return stash;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////

private:

    int _sweep_start_index;
    
    int _num_chunks_to_sweep;
#ifdef _DEBUG   
    chunk_sweep_stats _sweep_stats[GC_MAX_CHUNKS];
#endif
    hythread_t _thread_handle;

    hysem_t  _gc_thread_start_work_event;

    hysem_t  _gc_thread_work_done_event;

    gc_thread_action _task_to_do;

    unsigned int _clear_mark_start_index ;

    unsigned int _num_marks_to_clear;

    unsigned int _num_bytes_recovered_by_sweep;

    /////////////////////////////////////////////////////////////////////////////

    object_lock_save_info *_object_headers;

    // This is the chunk that is to be used by the object placement code to colocate objects
    block_info *placement_chunk;
    // This is the block in the chunk that is used by object placement code to colocate objects
    block_info *placement_block;

    Stash_Block *stash;
};

#endif // _gc_thread_H_
