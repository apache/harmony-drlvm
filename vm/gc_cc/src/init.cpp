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
 * @author Ivan Volosyuk
 */

// System header files
#include <iostream>

// VM interface header files
#include "port_malloc.h"
#include <apr_general.h>
#include "platform_lowlevel.h"
#include "open/vm_gc.h"
#include "open/gc.h"
#include "jit_intf.h"
#include <assert.h>
#include "gc_types.h"
#include "cxxlog.h"
#include "timer.h"
#include "apr_time.h"
#ifndef _WIN32
#include <sys/mman.h>
#endif

// Variables partially sorted by usage pattern. Should optimize cache lines

unsigned int heap_mark_phase;

HeapSegment heap;
uint32 chunk_size;

int pending_finalizers = false;

#define RESERVED_FOR_LAST_HASH 4

#define MB * (1024 * 1024)
size_t HEAP_SIZE_DEFAULT = 256 MB;

unsigned int prev_mark_phase;
bool cleaning_needed = false;
int gc_algorithm = 0;
int gc_adaptive = true;
int64 timer_start;
int64 timer_dt;
Ptr heap_base;
size_t max_heap_size;
size_t min_heap_size;
bool ignore_finalizers = false;
bool remember_root_set = false;
const char *lp_hint = NULL;
bool jvmti_heap_iteration = false;

static size_t get_size_property(const char* name) 
{
    char* size_string = get_property(name, VM_PROPERTIES);
    size_t size = atol(size_string);
    int sizeModifier = tolower(size_string[strlen(size_string) - 1]);
    destroy_property_value(size_string);

    size_t unit = 1;
    switch (sizeModifier) {
    case 'k': unit = 1024; break;
    case 'm': unit = 1024 * 1024; break;
    case 'g': unit = 1024 * 1024 * 1024;break;
    }

    size_t res = size * unit;
    if (res / unit != size) {
        /* overflow happened */
        return 0;
    }
    return res;
}

static void parse_configuration_properties() {
    max_heap_size = HEAP_SIZE_DEFAULT;
    min_heap_size = 16 MB;
    if (is_property_set("gc.mx", VM_PROPERTIES) == 1) {
        max_heap_size = get_size_property("gc.mx");

        if (max_heap_size < 16 MB) {
            INFO("max heap size is too small: " << max_heap_size);
            max_heap_size = 16 MB;
        }
        if (0 == max_heap_size) {
            INFO("wrong max heap size");
            max_heap_size = HEAP_SIZE_DEFAULT;
        }

        min_heap_size = max_heap_size / 10;
        if (min_heap_size < 16 MB) min_heap_size = 16 MB;
    }

    if (is_property_set("gc.ms", VM_PROPERTIES) == 1) {
        min_heap_size = get_size_property("gc.ms");

        if (min_heap_size < 16 MB) {
            INFO("min heap size is too small: " << min_heap_size);
            min_heap_size = 16 MB;
        }

        if (0 == min_heap_size)
            INFO("wrong min heap size");
    }

    if (min_heap_size > max_heap_size) {
        INFO("min heap size is larger then max");
        max_heap_size = min_heap_size;
    }

#ifdef POINTER64
        size_t max_compressed = (4096 * (size_t) 1024 * 1024);
        if (max_heap_size > max_compressed) {
            INFO("maximum heap size is limited"
                    " to 4 Gb due to pointer compression");
            max_heap_size = max_compressed;
            if (min_heap_size > max_heap_size)
                min_heap_size = max_heap_size;
        }
#endif


    if (is_property_set("gc.lp", VM_PROPERTIES) == 1) {
        char* value = get_property("gc.lp", VM_PROPERTIES);
        lp_hint = strdup(value);
        destroy_property_value(value);
    }
    
    gc_algorithm = get_int_property("gc.type", 0, VM_PROPERTIES);

    // version
    INFO(gc_version_string());
    INFO("GC type = " << gc_algorithm);

    if (get_boolean_property("gc.ignore_finalizers", false, VM_PROPERTIES)) {
        ignore_finalizers = true;
        INFO("GC will ignore finalizers");
    }

    if (get_boolean_property("gc.adaptive", true, VM_PROPERTIES)) {
        INFO("GC will use adaptive algorithm selection");
    } else {
        INFO("GC will NOT use adaptive algorithm selection");
        gc_adaptive = false;
    }

    if (get_boolean_property("gc.remember_root_set", false, VM_PROPERTIES)) {
        remember_root_set = true;
        INFO("GC will retrieve root set before any modification in heap");
    }

    if (get_boolean_property("gc.heap_iteration", false, VM_PROPERTIES)) {
        jvmti_heap_iteration = true;
        INFO("GC jvmti heap iteration enabled");
    }
}

void gc_vm_initialized() {
    TRACE2("gc.init", "gc_vm_initialized");

    if (get_boolean_property("gc.heap_iteration", false, VM_PROPERTIES)) {
        jvmti_heap_iteration = true;
        INFO("GC jvmti heap iteration enabled");
    }
}

#ifdef _WIN32
static inline void *reserve_mem(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
}
static const void* RESERVE_FAILURE = 0;
#else
static inline void *reserve_mem(size_t size) {
#ifdef POINTER64
    /* We have plenty of address space, let's protect unaccessible part of heap
     * to find some of bad pointers. */
    size_t four_gig = 4 * 1024 * (size_t) 1024 * 1024;
    size_t padding = 4 * 1024 * (size_t) 1024 * 1024;
    four_gig = 0;
    padding = 0;
    void *addr = mmap(0, padding + max_heap_size + four_gig, PROT_READ | PROT_WRITE,
            MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(addr != MAP_FAILED);
    UNUSED int err = mprotect((Ptr)addr, padding, PROT_NONE);
    assert(!err);
    err = mprotect((Ptr)addr + padding + max_heap_size,
                    four_gig, PROT_NONE);
    assert(!err);
    return (Ptr)addr + padding;
#else
    return mmap(0, max_heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}
static const void* RESERVE_FAILURE = MAP_FAILED;
#endif




void init_mem() {
    parse_configuration_properties();
    max_heap_size = round_down(max_heap_size, 65536);
    min_heap_size = round_down(min_heap_size, 65536);
    INFO("min heap size " << mb(min_heap_size) << " mb");
    INFO("max heap size " << mb(max_heap_size) << " mb");

    heap_base = 0;


    heap_base = NULL;
    if (lp_hint) {
        heap_base = (unsigned char*) alloc_large_pages(max_heap_size, lp_hint);
        if (heap_base == NULL) lp_hint = NULL;
        else min_heap_size = max_heap_size;
    }

    if (heap_base == NULL) {
        INFO("GC use small pages\n");
    } else {
        INFO("GC use large pages\n");
    }

    if (heap_base == NULL) {
        heap_base = (unsigned char*) reserve_mem(max_heap_size);
        if (heap_base == RESERVE_FAILURE) {
            size_t dec = 100 * 1024 * 1024;
            max_heap_size = max_heap_size / dec * dec;

            while(true) {
                heap_base = (unsigned char*) reserve_mem(max_heap_size);
                if (heap_base != RESERVE_FAILURE) break;
                max_heap_size -= dec;
                assert(max_heap_size > 0);
            }
            ECHO("WARNING: max heap size is too large, reduced to " << mb(max_heap_size) << " Mb");
        }
    }

    if (min_heap_size > max_heap_size) {
        min_heap_size = max_heap_size;
        ECHO("WARNING: min heap size reduced to " << mb(min_heap_size) << " Mb");
    }

    heap.ceiling = heap_base + min_heap_size - RESERVED_FOR_LAST_HASH;

    heap.base = heap_base;
    heap.size = min_heap_size;
    heap.max_size = max_heap_size;
    heap.roots_start = heap.roots_pos = heap.roots_end =
        heap.base + heap.max_size - RESERVED_FOR_LAST_HASH;

#ifdef _WIN32
    void *res;
    if (heap_base && !lp_hint) {
        res = VirtualAlloc(heap.base, heap.size, MEM_COMMIT, PAGE_READWRITE);
        if (!res) DIE("Can't create heap_L");
    }
#endif
    chunk_size = round_down(heap.size / 10, 65536);
    init_gcvt();
    gc_reserve_mark_bits();
}

void gc_init() {
    INFO2("gc.init", "GC init called\n");
    init_mem();
    init_slots();
    init_select_gc();
    gc_end = apr_time_now();
    timer_init();
}

void
gc_wrapup() {
    gc_start = apr_time_now();
    total_user_time += gc_start - gc_end;
    INFO("\nGC: "
        << gc_num << " time(s), "
        << "avg " << (gc_num ? (total_gc_time/gc_num/1000) : 0) << " ms, "
        << "max " << (max_gc_time/1000) << " ms, "
        << "total " << total_gc_time/1000 << " ms, "
        << "gc/user " << (int)(total_gc_time*100.f/total_user_time) << " %"
    );
    INFO2("gc.init", "gc_wrapup called");
    gc_unreserve_mark_bits();
    deinit_gcvt();
#ifdef _WIN32
    bool UNUSED res = VirtualFree(heap_base, max_heap_size, MEM_DECOMMIT);
    assert (res);
#else
    int UNUSED res = munmap(heap_base, max_heap_size);
    assert (res != -1);
#endif
    INFO2("gc.init", "gc_wrapup done");
}

void gc_reserve_mark_bits() {
    mark_bits_size = max_heap_size / sizeof(void*) / 8;
#ifdef _WIN32
    mark_bits = (unsigned char*) VirtualAlloc(NULL, mark_bits_size, MEM_RESERVE, PAGE_READWRITE);
    assert(mark_bits);
#else
    mark_bits = (unsigned char*) mmap(0, mark_bits_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mark_bits != MAP_FAILED);
#endif
}

void gc_unreserve_mark_bits() {
#ifdef _WIN32
    bool UNUSED res = VirtualFree(mark_bits, 0, MEM_RELEASE);
    assert(res);
#else
    int UNUSED res = munmap(mark_bits, mark_bits_size);
    assert(res != -1);
#endif
}

static unsigned char *mark_bits_allocated_start;
static unsigned char *mark_bits_allocated_end;

void gc_allocate_mark_bits() {
    //memset(heap.compaction_region_start(), 0, heap.compaction_region_end() - heap.compaction_region_start());
    unsigned char *start = mark_bits + (heap.compaction_region_start() - heap_base) / sizeof(void*) / 8;
    unsigned char *end = mark_bits + (heap.compaction_region_end() - heap_base + sizeof(void*) * 8 - 1) / sizeof(void*) / 8;
    int page = 4096; // FIXME
    mark_bits_allocated_start = (unsigned char*)((POINTER_SIZE_INT)start & ~(page - 1));
    mark_bits_allocated_end = (unsigned char*)(((POINTER_SIZE_INT)end + page - 1) & ~(page - 1));
#ifdef _WIN32
    unsigned char *res = (unsigned char*) VirtualAlloc(mark_bits_allocated_start,
            mark_bits_allocated_end - mark_bits_allocated_start, MEM_COMMIT, PAGE_READWRITE);
    assert(res);
#endif
}

void gc_deallocate_mark_bits() {
#ifdef _WIN32
    bool UNUSED res = VirtualFree(mark_bits_allocated_start,
            mark_bits_allocated_end - mark_bits_allocated_start, MEM_DECOMMIT);
    assert(res);
#else
    void UNUSED *res = mmap(mark_bits, mark_bits_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(res == (void*)mark_bits);
    assert(mark_bits[0] == 0);
#endif
}

void heap_extend(size_t size) {
    size = (size + 65535) & ~65535;
    size_t max_size = heap.max_size - (heap.roots_end - heap.roots_start);
    if (size > max_size) size = max_size;
    if (size <= heap.size) return;

#ifdef _WIN32
    void* UNUSED res = VirtualAlloc(heap.base + heap.size, size - heap.size, MEM_COMMIT, PAGE_READWRITE);
    assert(res);
#endif
    heap.size = size;
    unsigned char *old_ceiling = heap.ceiling;
    heap.ceiling = heap.base + heap.size - RESERVED_FOR_LAST_HASH;

    if (heap.pos_limit == old_ceiling) {
        heap.pos_limit = heap.ceiling;
    }
    chunk_size = round_down(heap.size / (10 * num_threads),128);
    INFO("heap extended to  " << mb(heap.size) << " mb");
}

// disabled now
void heap_shrink(size_t size) {
    size = (size + 65535) & ~65535;
    if (size < min_heap_size) size = min_heap_size;
    if (!pinned_areas.empty()) {
        size_t pin_limit = pinned_areas[pinned_areas.size() - 1] - heap.base;
        pin_limit = (pin_limit + 65535) & ~65535;
        if (size < pin_limit) size = pin_limit;
    }
    if (size >= heap.size) return;

#ifdef _WIN32
    bool UNUSED res = VirtualFree(heap.base + size, heap.size - size, MEM_DECOMMIT);
    assert(res);
#else
    void UNUSED *res = mmap(heap.base + size, heap.size - size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(res == (void*)(heap.base + size));
#endif

    heap.size = size;
    heap.ceiling = heap.base + heap.size - RESERVED_FOR_LAST_HASH;

    if (heap.ceiling > heap.pos_limit) {
        heap.pos_limit = heap.ceiling;
    }
    chunk_size = round_down(heap.size / (10 * num_threads),128);
    INFO("heap shrank to  " << mb(heap.size) << " mb");
}
