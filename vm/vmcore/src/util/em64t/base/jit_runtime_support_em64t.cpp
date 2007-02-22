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
 * @author Intel, Evgueni Brevnov
 * @version $Revision$
 */  

#include <assert.h>

#include "open/types.h"
#include "open/gc.h"

#include "port_general.h"
#include "heap.h"
#include "vm_threads.h"

#include "lil.h"
#include "lil_code_generator.h"
#include "jit_runtime_support.h"
#include "m2n.h"
#include "../m2n_em64t_internal.h"

#define LOG_DOMAIN "vm.helpers"
#include "cxxlog.h"

#include "vm_stats.h"
#include "dump.h"
#include "exceptions.h"
#include "jit_runtime_support_common.h"

/**
 * Generic allocation routine.
 */
static LilCodeStub * rth_get_lil_new_generic(LilCodeStub * cs, void * fast_alloc, void * slow_alloc) {

    POINTER_SIZE_INT ts_gc_offset = APR_OFFSETOF(VM_thread, _gc_private_information);    

    return lil_parse_onto_end(cs,
        "locals 1;"
        "l0 = ts;"
        "out platform:g4,pint,pint:ref;"
        "o0 = i0;"
        "o1 = i1;"
        "o2 = l0+%0i:pint;"
        "call %1i;"
        "jc r = 0, alloc_slow;"
        "ret;"
        ":alloc_slow;"
        "push_m2n 0, 0;"
        "out platform:g4,pint,pint:ref;"
        "o0 = i0;"
        "o1 = i1;"
        "o2 = l0+%2i:pint;"
        "call %3i;"
        "pop_m2n;"
        "ret;",
        ts_gc_offset, fast_alloc, ts_gc_offset, slow_alloc);
}

/**
 * Object allocation helper.
 */
NativeCodePtr rth_get_lil_new_resolved_using_vtable_and_size() {
    static NativeCodePtr addr = NULL;

    if (addr != NULL) {
        return addr;
    }

    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:g4,pint:ref;");
    assert(cs);

#ifdef VM_STATS
    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add(
        (void*)VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE, 0, NULL);
    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
    assert(cs);
#endif

    cs = rth_get_lil_new_generic(cs, (void *)gc_alloc_fast, (void *)vm_malloc_with_thread_pointer);
    assert(cs && lil_is_valid(cs));

    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "new_resolved_using_vtable_and_size", lil_cs_get_code_size(cs));
    
    lil_free_code_stub(cs);
    return addr;
}

/**
 * Vector allocation routine.
 */
NativeCodePtr rth_get_lil_new_vector_using_vtable() {
    static NativeCodePtr addr = NULL;

    if (addr != NULL) {
        return addr;
    }

    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:g4,pint:ref;");
    assert(cs);

#ifdef VM_STATS
    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add(
        (void*)VM_RT_NEW_VECTOR_USING_VTABLE, 0, NULL);
    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
    assert(cs);
#endif

    cs = rth_get_lil_new_generic(cs,
        (void *)vm_new_vector_or_null_using_vtable_and_thread_pointer,
        (void *)vm_rt_new_vector_using_vtable_and_thread_pointer);
    assert(cs && lil_is_valid(cs));

    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "new_vector_using_vtable", lil_cs_get_code_size(cs));
    
    lil_free_code_stub(cs);
    return addr;

}

/**
 * Creates multidimensional array. Arguments are expected to lie right above last m2n frame.
 * Doesn't return if exception occurs.
 *
 * Note: this is a helper routine for rth_get_lil_multianewarray (see jit_runtime_support.cpp).
 */
Vector_Handle
vm_rt_multianewarray_recursive(Class    *c,
                             int      *dims_array,
                             unsigned  dims);

Vector_Handle rth_multianewarrayhelper()
{
    ASSERT_THROW_AREA;
    M2nFrame* m2nf = m2n_get_last_frame();
    const unsigned max_dim = 255;
    int lens[max_dim];

#ifdef VM_STATS
    VM_Statistics::get_vm_stats().num_multianewarray++;  
#endif
    // +1(skip rip)
    uint64 * args = (uint64 *)m2n_get_frame_base(m2nf) + 1;
    Class * c = (Class *)args[0];
    unsigned dims = (unsigned)(args[1] & 0xFFFFffff);
    assert(dims <= max_dim);
    // compute the base address of an array
    uint64* lens_base = (uint64*)(args+2);
    for(unsigned i = 0; i < dims; i++) {
        lens[i] = (int)lens_base[dims-i-1];
    }
    return vm_rt_multianewarray_recursive(c, lens, dims);
}


// see jit_lock_rt_support.cpp for the implementation
NativeCodePtr rth_get_lil_monitor_enter_static();
NativeCodePtr rth_get_lil_monitor_enter();
NativeCodePtr rth_get_lil_monitor_enter_non_null();

NativeCodePtr rth_get_lil_monitor_exit_static();
NativeCodePtr rth_get_lil_monitor_exit();
NativeCodePtr rth_get_lil_monitor_exit_non_null();

void * vm_get_rt_support_addr(VM_RT_SUPPORT f) {

#ifdef VM_STATS
    VM_Statistics::get_vm_stats().rt_function_requests.add((void *)f, 1, NULL);
#endif // VM_STATS

    NativeCodePtr res = rth_get_lil_helper(f);
    if (res) return res;
    
    switch(f) {
    // Monitor enter runtime helpers
    case VM_RT_MONITOR_ENTER_STATIC:
        return rth_get_lil_monitor_enter_static();
    case VM_RT_MONITOR_ENTER:
        return rth_get_lil_monitor_enter();
    case VM_RT_MONITOR_ENTER_NON_NULL:
        return rth_get_lil_monitor_enter_non_null();

   // Monitor exit runtime helpers
    case VM_RT_MONITOR_EXIT_STATIC:
        return rth_get_lil_monitor_exit_static();
    case VM_RT_MONITOR_EXIT:
        return rth_get_lil_monitor_exit();
    case VM_RT_MONITOR_EXIT_NON_NULL:
        return rth_get_lil_monitor_exit_non_null();

    // Object creation helper
    case VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE:
        return rth_get_lil_new_resolved_using_vtable_and_size();
    // Array creation helper
    case VM_RT_NEW_VECTOR_USING_VTABLE:
        return rth_get_lil_new_vector_using_vtable();
    default:
        ASSERT(false, "Unexpected helper id" << f);
        return NULL;
    }
}

void * vm_get_rt_support_addr_optimized(VM_RT_SUPPORT f, Class_Handle UNREF c) {
    return vm_get_rt_support_addr(f);
}
