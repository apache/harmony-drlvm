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
#include <memory.h>
#include <assert.h>
#include <time.h>
#include <open/hythread_ext.h>


// VM interface header files
#include "platform_lowlevel.h"
#include "port_atomic.h"
#include "open/vm_gc.h"
#include "open/gc.h"

// GC header files
#include "gc_cout.h"
#include "gc_header.h"
#include "gc_v4.h"
#include "work_packet_manager.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool verify_gc;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



Work_Packet_Manager::Work_Packet_Manager()
{
    _num_total_work_packets = 0;
    _full_work_packets = NULL;
    _num_full_work_packets = 0;;
    _empty_work_packets = NULL;
    _num_empty_work_packets = 0;
    _almost_full_work_packets = NULL;
    _num_almost_full_work_packets = 0;
    _almost_empty_work_packets = NULL;
    _num_almost_empty_work_packets = 0;
}



Work_Packet *
Work_Packet_Manager::get_full_work_packet()
{
    while (_full_work_packets != NULL) {
        
        void * old_val = (void *)_full_work_packets;
        if (old_val) {
            void * wp_next = ((Work_Packet *)old_val)->get_next();
            void * val = apr_atomic_casptr(
                (volatile void **)&_full_work_packets,
                wp_next,
                old_val
                );
            if (val && (old_val == val)) {
                ((Work_Packet *)val)->set_next(NULL);
                assert(sizeof(int) == 4);
                apr_atomic_dec32((volatile uint32 *)&_num_full_work_packets);
                return (Work_Packet *)val;
            }
        }
    }
    return NULL;
}


Work_Packet *
Work_Packet_Manager::get_almost_full_work_packet()
{
    while (_almost_full_work_packets != NULL) {
        
        void * old_val = (void *)_almost_full_work_packets;
        if (old_val) {
            void * wp_next = ((Work_Packet *)old_val)->get_next();
            void * val = apr_atomic_casptr(
                (volatile void **)&_almost_full_work_packets,
                wp_next,
                old_val
                );
            if (val && (old_val == val)) {
                ((Work_Packet *)val)->set_next(NULL);
                assert(sizeof(int) == 4);
                apr_atomic_dec32((volatile uint32 *)&_num_almost_full_work_packets);
                return (Work_Packet *)val;
            }
        }
    }
    return NULL;
}


Work_Packet *
Work_Packet_Manager::get_empty_work_packet(bool dont_fail_me)
{
    while (_empty_work_packets != NULL) {
        
        void * old_val = (void *)_empty_work_packets;
        if (old_val) {
            void * wp_next = ((Work_Packet *)old_val)->get_next();
            void * val = apr_atomic_casptr(
                (volatile void **)&_empty_work_packets,
                wp_next,
                old_val
                );
            if (val && (old_val == val)) {
                ((Work_Packet *)val)->set_next(NULL);
                assert(sizeof(int) == 4);
                apr_atomic_dec32((volatile uint32 *)&_num_empty_work_packets);
                return (Work_Packet *)val;
            }
        }
    }
    
    if (dont_fail_me) {
        // Create and return a new packet....
        apr_atomic_inc32((volatile uint32 *)&_num_total_work_packets);
        return new Work_Packet();
    } else {
        return NULL;
    }
}


Work_Packet *
Work_Packet_Manager::get_almost_empty_work_packet()
{
    while (_almost_empty_work_packets != NULL) {
        
        void * old_val = (void *)_almost_empty_work_packets;
        if (old_val) {
            void * wp_next = ((Work_Packet *)old_val)->get_next();
            void * val = apr_atomic_casptr(
                (volatile void **)&_almost_empty_work_packets,
                wp_next,
                old_val
                );
            if (val && (old_val == val)) {
                ((Work_Packet *)val)->set_next(NULL);
                assert(sizeof(int) == 4);
                apr_atomic_dec32((volatile uint32 *)&_num_almost_empty_work_packets);
                return (Work_Packet *)val;
            }
        }
    }
    return NULL;
}


void 
Work_Packet_Manager::return_work_packet(Work_Packet *wp)
{
    // this packet is part of no list (no context)
    if (wp->get_next() != NULL) {
        DIE("Work packet returned was still connected to some list...");
    }
    
    volatile Work_Packet **ptr = NULL;

    assert(sizeof(int) == 4);
    if (wp->fullness() == packet_full) {
        ptr = &_full_work_packets;
        apr_atomic_inc32((volatile uint32 *)&_num_full_work_packets);
    } else if (wp->fullness() == packet_empty) {
        ptr = &_empty_work_packets;
        apr_atomic_inc32((volatile uint32 *)&_num_empty_work_packets);
    } else if (wp->fullness() == packet_almost_full) {
        ptr = &_almost_full_work_packets;
        apr_atomic_inc32((volatile uint32 *)&_num_almost_full_work_packets);
    } else {
        assert(wp->fullness() == packet_almost_empty);
        ptr = &_almost_empty_work_packets;
        apr_atomic_inc32((volatile uint32 *)&_num_almost_empty_work_packets);
    }
    
    assert(ptr);
    
    while (true) {
        Work_Packet *old_val = (Work_Packet *) *ptr;
        wp->set_next((Work_Packet *)old_val);

        void * val = apr_atomic_casptr(
            (volatile void **)ptr,
            wp,
            old_val
            );
        if (val == old_val) {
            return ;
        }
    }
}



Work_Packet *
Work_Packet_Manager::get_output_work_packet() {

    Work_Packet *wp = get_empty_work_packet(false);
    if (wp == NULL) {
        wp = get_almost_empty_work_packet();
    }
    if (wp == NULL) {
        // dont fail me this time
        wp = get_empty_work_packet(true);
    }
    assert(wp);

    if (wp) {
        // this packet loses its context
        wp->set_next(NULL);
    }
    return wp;
}


Work_Packet *
Work_Packet_Manager::get_input_work_packet() {

    Work_Packet *wp = get_full_work_packet();
    if (wp == NULL) {
        wp = get_almost_full_work_packet();
    }
    if (wp == NULL) {
        wp = get_almost_empty_work_packet();
    } 
    if (wp) {
        // this packet loses its context
        wp->set_next(NULL);
    }
    return wp;
}



void 
Work_Packet_Manager::_dump_state()
{
    INFO("==========================================================================\n"
        << "_num_total_work_packets = " << _num_total_work_packets << "\n"
        << "_almost_empty_work_packets = " << _almost_empty_work_packets << "\n"
        << "_num_almost_empty_work_packets = " << _num_almost_empty_work_packets << "\n"
        << "_empty_work_packets = " << _empty_work_packets << "\n"
        << "_num_empty_work_packets = " << _num_empty_work_packets << "\n"
        << "_full_work_packets = " << _full_work_packets << "\n"
        << "_num_full_work_packets = " << _num_full_work_packets << "\n"
        << "_almost_full_work_packets = " << _almost_full_work_packets << "\n"
        << "_num_almost_full_work_packets = " << _num_almost_full_work_packets);
}


bool
Work_Packet_Manager::wait_till_there_is_work_or_no_work()
{
    if (    (_full_work_packets != NULL) || 
            (_almost_empty_work_packets != NULL) ||
            (_almost_full_work_packets != NULL)) {
        // there is some work to be done....
        return true;
    }

    clock_t start, finish;
    start = clock();

    while (true) {

        if ((_full_work_packets == NULL) && 
            (_almost_empty_work_packets == NULL) &&
            (_almost_full_work_packets == NULL)) {

            if ((_num_full_work_packets == 0) &&
                (_num_almost_full_work_packets == 0) && 
                (_num_almost_empty_work_packets == 0)) {
                    
                    return false;
            }
        }

        hythread_sleep(0);

#ifndef _IPF_
#ifndef PLATFORM_POSIX
        __asm {
            pause
        }
#endif // PLATFORM_POSIX
#endif // _IPF_

  
        finish = clock();
        if (((finish - start) / CLOCKS_PER_SEC) > 5) {
            // if I have waited for over 5 seconds....terminate.
            WARN("WAITED TOO LONG FOR WORK THAT APPARENTLY IS AVAILABLE!!, more info follows");
            _dump_state();
            DIE("WAITED TOO LONG FOR WORK THAT APPARENTLY IS AVAILABLE!!, terminating");
        }

    }
}





void 
Work_Packet_Manager::verify_after_gc()
{
    if (_num_almost_empty_work_packets || _num_full_work_packets || _num_almost_full_work_packets || 
        _almost_empty_work_packets || _full_work_packets || _almost_full_work_packets || 
        (_num_almost_empty_work_packets + _num_full_work_packets + _num_almost_full_work_packets + _num_empty_work_packets != _num_total_work_packets)
        ) {
        // Bad termination of mark/scan phase...
        WARN("BAD values in _mark_scan_pool...unfinished phase, more info below");
        _dump_state();
        DIE("BAD values in _mark_scan_pool...unfinished phase, terminating");
    } 
    assert(_empty_work_packets);
    assert(_num_empty_work_packets > 0);


#ifndef _DEBUG
    if (verify_gc) {
#endif // _DEBUG
        int num_actual_packets = 0;
        Work_Packet *wp = (Work_Packet *) _empty_work_packets;
        while (wp) {
            num_actual_packets++;
            assert(wp->is_empty());
            wp = wp->get_next();
        }
        assert(num_actual_packets == _num_empty_work_packets);
        if (num_actual_packets != _num_empty_work_packets) {
            DIE("Mismatch in empty work packets accounting at the end of GC..."
                << num_actual_packets << " supposed to be there...only " 
                << _num_empty_work_packets << " found");
        }
#ifndef _DEBUG
    }
#endif // _DEBUG
}
