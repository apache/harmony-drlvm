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
#include "port_atomic.h"
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
#include "gc_thread.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// Callback from VM/JIT
void 
Garbage_Collector::gc_internal_add_root_set_entry(Partial_Reveal_Object **ref) 
{
    // Is is NULL.  The NULL reference is not interesting.
    if (*ref == Slot::managed_null()) {
        return;
    }
#ifdef _DEBUG
    assert(ref);
    assert(NULL == *ref || (*ref >= get_gc_heap_base_address()
        && *ref < get_gc_heap_ceiling_address()));
    Partial_Reveal_Object *p_obj = *ref;
    
    ASSERT ((p_obj->get_obj_info() & FORWARDING_BIT_MASK) == 0,
        "obj_info = " << (void*)(POINTER_SIZE_INT)p_obj->get_obj_info() << ", "
        << " FORWARDING_BIT_MASK = " << (void*)FORWARDING_BIT_MASK << "\n"
        << p_obj->vt()->get_gcvt()->gc_class_name << " at " << p_obj << "(root at " << ref << ")"
        << (((void*)(POINTER_SIZE_INT)p_obj->get_obj_info() >= p_global_gc->get_gc_heap_base_address()
            && (void*)(POINTER_SIZE_INT)p_obj->get_obj_info() < p_global_gc->get_gc_heap_ceiling_address()) ?
            ("\n-----------------\n"
             "Enumeration bug: The root points to the old copy of an object") : 
             "");
    );
    
    assert (p_obj->vt());
#endif
    num_roots_added++;
    // Here I will just collect them in an array and then let the mark threads compete for it
    
    gc_trace_slot((void **)ref, (void *)*ref, "In gc_internal_add_root_set_entry.");
    gc_trace (*ref, "In gc_internal_add_root_set_entry with this object in a slot.");

    root_set.insert(ref);
}

// Callback from VM/JIT
void 
Garbage_Collector::gc_internal_add_weak_root_set_entry(Partial_Reveal_Object **ref,Boolean is_short_weak) 
{
    // Is it NULL.  The NULL reference is not interesting.
    if (*ref == Slot::managed_null()) {
        return;
    }
#ifdef _DEBUG
    Partial_Reveal_Object *the_obj = *ref;
    assert (the_obj->vt());
#endif

    if(is_short_weak)
    {
        m_short_weak_roots.push_back(ref);
    }
    else
    {
        m_long_weak_roots.push_back(ref);
    }
}


Partial_Reveal_Object **
Garbage_Collector::get_root_atomic(unsigned index) {
    volatile Partial_Reveal_Object **ref = 
        (volatile Partial_Reveal_Object ** ) root_set[index];

    if (!ref) return NULL;

    // Atomically grab a root....multiple GC threads compete for roots
#ifndef POINTER64
    // optimize 32-bit case a little bit by using atomic exchange
    // instead of atomic compare-and-swap
    Partial_Reveal_Object** got = 
        (Partial_Reveal_Object**)
        apr_atomic_xchg32(
                (volatile apr_uint32_t *)&root_set[index],
                (apr_uint32_t) NULL);
    if (got == NULL) return NULL;
    assert(got == ref);
#else
    if (apr_atomic_casptr( 
                (volatile void **)&root_set[index],
                (void *) NULL,
                (void *) ref
                ) == NULL) return NULL;
#endif

    gc_trace_slot((void **)ref, (void *)*ref, "In get_fresh_root_to_trace.");
    return (Partial_Reveal_Object **) ref;
}

void
Garbage_Collector::mark_scan_roots(GC_Thread *gc_thread) {
    int nthreads = get_num_worker_threads();
    int thread_id = gc_thread->get_id();
    unsigned size = root_set.size();

    if (nthreads == 1 || (nthreads == 2  && thread_id == 0)) {
        // ONE THREADED VERSION OF MARK SCAN
        
        for(unsigned i = 0; i < size; i++) {
            Partial_Reveal_Object **ref = root_set[i];
            if (!ref) continue;
            root_set[i] = 0;
            scan_root(*ref, gc_thread);
        }
        return;
    }

    if (nthreads == 2) {
        // SECOND THREAD OF 2-THREADED MARK SCAN
        //
        for(int i = size - 1; i >= 0; --i) {
            Partial_Reveal_Object **ref = root_set[i];
            if (!ref) continue;
            root_set[i] = 0;
            scan_root(*ref, gc_thread);
        }
        return;
    }

    // ARBITRARY NUMBER OF THREADS MARK SCAN
    
    unsigned index = 0;
    unsigned start_index;

    while (true) {
        if (index >= size) index = 0;
        Partial_Reveal_Object **ref = get_root_atomic(index++);

        if (ref) {
            scan_root(*ref, gc_thread);
            continue;
        }

        // found zero root, jumping to random place
        start_index = index = (rand() * size ) / RAND_MAX;;

        // searching for
        while (index < size) {
            ref = get_root_atomic(index++);
            if (!ref) continue;

            scan_root(*ref, gc_thread);
            break;
        }

        if (ref) continue;

        // Not found, search the rest of array
        index = 0;
        while (index < start_index) {
            ref = get_root_atomic(index++);
            if (!ref) continue;
            scan_root(*ref, gc_thread);
            break;
        }
        if (!ref) return; // no more roots
    }
}


Partial_Reveal_Object **
Garbage_Collector::get_fresh_root_to_trace(unsigned &index)
{
    ABORT("Not implemented");
    return NULL;
}


void
Garbage_Collector::clear_roots()
{
    // Zero out array of roots
    root_set.clear();

    // clear out the previous set of weak roots
    m_short_weak_roots.clear();
    m_long_weak_roots.clear();

    // clear reference lists
    soft_references->clear();
    weak_references->clear();
    phantom_references->clear();
    references_to_enqueue->clear();

    // Forget any pinned blocks that may have been identified
    // during a previous GC.
    m_pinned_blocks.clear();
}


void
Garbage_Collector::roots_init()
{
    root_set.clear();
}


