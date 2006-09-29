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

#ifndef _work_packet_manager_H_
#define _work_packet_manager_H_

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define GC_SIZEOF_WORK_PACKET 200


enum packet_emptiness {
    packet_full,
    packet_empty,
    packet_almost_full,
    packet_almost_empty
};


class Work_Packet {

public:
    Work_Packet() {
        memset(_work_units, 0, sizeof(void *) * GC_SIZEOF_WORK_PACKET);
        _current_work_unit_index = _num_work_units_in_packet = 0;
        _next = NULL;
    }

    inline bool is_empty() {
        return (_num_work_units_in_packet == 0);
    }

    inline bool is_full() {
        return (_num_work_units_in_packet == GC_SIZEOF_WORK_PACKET);
    }

    inline unsigned int get_num_work_units_in_packet() {
        return _num_work_units_in_packet;
    }

    inline packet_emptiness fullness() {
        if (_num_work_units_in_packet == 0) {
            return packet_empty;
        } else if (_num_work_units_in_packet == GC_SIZEOF_WORK_PACKET) {
            return packet_full;
        } else if (_num_work_units_in_packet <= (GC_SIZEOF_WORK_PACKET / 4)) {
            return packet_almost_empty;
        } else {
            return packet_almost_full;
        }
    }

    /////////////////////////////////////////////////////////
    inline Work_Packet *get_next() {
        return _next;
    }
    
    inline void set_next(Work_Packet *wp) {
        _next = wp;
    }
    /////////////////////////////////////////////////////////

    // iterator to remove units of work from the work packet
    inline void init_work_packet_iterator() {
        assert(_num_work_units_in_packet > 0); 
        if (_num_work_units_in_packet > 0) {
            assert(_num_work_units_in_packet <= GC_SIZEOF_WORK_PACKET);     
            _current_work_unit_index = _num_work_units_in_packet - 1;
        } else {
            assert(_num_work_units_in_packet == 0);
            _current_work_unit_index = 0;
        }
    }

    inline void *remove_next_unit_of_work() {
        if (_num_work_units_in_packet == 0) {
            return NULL;
        }
        assert(_num_work_units_in_packet <= GC_SIZEOF_WORK_PACKET);     
        assert(_current_work_unit_index < GC_SIZEOF_WORK_PACKET);

        void *ret = _work_units[_current_work_unit_index];
        _work_units[_current_work_unit_index] = NULL;
        if (_current_work_unit_index > 0) {
            _current_work_unit_index--;
        }
        assert(_num_work_units_in_packet > 0);
        _num_work_units_in_packet--; 
        return ret;
    }

    inline void reset_work_packet() {
        assert(_num_work_units_in_packet == 0);
        memset(_work_units, 0, sizeof(void *) * GC_SIZEOF_WORK_PACKET);
        _current_work_unit_index = _num_work_units_in_packet = 0;
        _next = NULL;
    }

    inline bool work_packet_has_space_to_add() {
        return (_num_work_units_in_packet < GC_SIZEOF_WORK_PACKET);
    }

    // add work unit to work packet
    inline void add_unit_of_work(void *work_unit) {
        assert(_num_work_units_in_packet < GC_SIZEOF_WORK_PACKET);
        assert(_work_units[_num_work_units_in_packet] == NULL);
        _work_units[_num_work_units_in_packet] = work_unit;
        _num_work_units_in_packet++;
    }

private:

    void *_work_units[GC_SIZEOF_WORK_PACKET];
    unsigned int _num_work_units_in_packet;
    unsigned int _current_work_unit_index;
    
    Work_Packet *_next;
};


class Work_Packet_Manager {

public:

    Work_Packet_Manager();

    Work_Packet *get_full_work_packet();

    Work_Packet *get_almost_full_work_packet();

    Work_Packet *get_empty_work_packet(bool);
    
    Work_Packet *get_almost_empty_work_packet();

    void return_work_packet(Work_Packet *);

    //...some more...
    Work_Packet *get_input_work_packet();

    Work_Packet *get_output_work_packet();

    // termination for any thread...
    bool wait_till_there_is_work_or_no_work();

    void verify_after_gc();

private:

    volatile int _num_total_work_packets;

    volatile Work_Packet *_full_work_packets;
    volatile int _num_full_work_packets;

    volatile Work_Packet *_empty_work_packets;
    volatile int _num_empty_work_packets;

    volatile Work_Packet *_almost_full_work_packets;
    volatile int _num_almost_full_work_packets;

    volatile Work_Packet *_almost_empty_work_packets;
    volatile int _num_almost_empty_work_packets;

    void _dump_state();
};


Work_Packet *get_output_work_packet(Work_Packet_Manager *wpm);
Work_Packet *get_input_work_packet(Work_Packet_Manager *wpm);

#endif // _work_packet_manager_H_


