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
#include "jit_runtime_support.h"
#include <assert.h>
#include "gc_types.h"
#include "cxxlog.h"
#include "timer.h"
#include "apr_time.h"
#ifndef _WIN32
#include <sys/mman.h>
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

// Variables partially sorted by usage pattern. Should optimize cache lines

unsigned int heap_mark_phase;

HeapSegment heap;
POINTER_SIZE_INT chunk_size;

int pending_finalizers = false;

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
Ptr vtable_base;
bool ignore_finalizers = false;
bool remember_root_set = false;
const char *lp_hint = NULL;
bool jvmti_heap_iteration = false;

#ifdef _WIN32
 /* WINDOWS */
static const void* MEM_FAILURE = 0;
void* mem_reserve(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
}
void  mem_unreserve(void *ptr, size_t size) {
    bool UNUSED res = VirtualFree(ptr, 0, MEM_RELEASE);
	int err = GetLastError();
    assert(res);
}

void  mem_commit(void *ptr, size_t size) {
    bool UNUSED res = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    assert(res);
}

void  mem_decommit(void *ptr, size_t size) {
    bool UNUSED res = VirtualFree(ptr, size, MEM_DECOMMIT);
    assert (res);
}
#else
 /* LINUX */
static const void* MEM_FAILURE = MAP_FAILED;
void* mem_reserve(size_t size) {
    return mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}
void  mem_unreserve(void *ptr, size_t size) {
    int UNUSED res = munmap(ptr, size);
    assert (res != -1);
}
void* mem_commit(void *ptr, size_t size) {
}
void mem_decommit(void *ptr, size_t size) {
    UNUSED void *res = mmap(ptr, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(res != MAP_FAILED);
}
#endif


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

void enable_heap_region(void *start, size_t size) {
    if (!lp_hint) mem_commit(start, size);
}
void disable_heap_region(void *start, size_t size) {
    if (!lp_hint) mem_decommit(start, size);
}

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
        heap_base = (unsigned char*) mem_reserve(max_heap_size);
        if (heap_base == (Ptr) MEM_FAILURE) {
            size_t dec = 100 * 1024 * 1024;
            max_heap_size = max_heap_size / dec * dec;

            while(true) {
                heap_base = (unsigned char*) mem_reserve(max_heap_size);
                if (heap_base != (Ptr) MEM_FAILURE) break;
                max_heap_size -= dec;
                assert(max_heap_size > 0);
            }
            LECHO(19, "WARNING: max heap size is too large, reduced to {0} Mb" << mb(max_heap_size));
        }
    }

    if (min_heap_size > max_heap_size) {
        min_heap_size = max_heap_size;
        LECHO(20, "WARNING: min heap size reduced to {0} Mb" << mb(min_heap_size));
    }

    heap.ceiling = heap_base + min_heap_size - RESERVED_FOR_LAST_HASH;

    heap.base = heap_base;
    heap.size = min_heap_size;
    heap.max_size = max_heap_size;
    heap.roots_start = heap.roots_pos = heap.roots_end =
        heap.base + heap.max_size;

    enable_heap_region(heap.base, heap.size);

    chunk_size = round_down(heap.size / 10, 65536);
    init_gcvt();

    mark_bits_size = max_heap_size / sizeof(void*) / 8;
    mark_bits = (Ptr) mem_reserve(mark_bits_size);
    assert(mark_bits != MEM_FAILURE);
}

static void init_gc_helpers()
{
    set_property("vm.component.classpath.gc_cc", "gc_cc.jar", VM_PROPERTIES);
    vm_helper_register_magic_helper(VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE, "org/apache/harmony/drlvm/gc_cc/GCHelper", "alloc");
    vm_helper_register_magic_helper(VM_RT_NEW_VECTOR_USING_VTABLE, "org/apache/harmony/drlvm/gc_cc/GCHelper", "allocArray");
}

int gc_init() {
    INFO2("gc.init", "GC init called\n");

    if (!get_boolean_property("vm.assert_dialog", TRUE, VM_PROPERTIES)) {
        disable_assert_dialogs();
    }

#ifdef POINTER64
    if(vm_vtable_pointers_are_compressed()) {
        vtable_base = (Ptr)vm_get_vtable_base();
    }
#endif

    vm_gc_lock_init();
    init_mem();
    init_slots();
    init_select_gc();
    gc_end = apr_time_now();
    timer_init();

    init_gc_helpers();

    return JNI_OK;
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
    mem_unreserve(heap_base, max_heap_size);
    mem_unreserve(mark_bits, mark_bits_size);
    deinit_gcvt();
    INFO2("gc.init", "gc_wrapup done");
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
    mem_commit(mark_bits_allocated_start, mark_bits_allocated_end - mark_bits_allocated_start);
}

void gc_deallocate_mark_bits() {
    mem_decommit(mark_bits, mark_bits_size);
}

void heap_extend(size_t size) {
    size = (size + 65535) & ~65535;
    size_t max_size = heap.max_size - (heap.roots_end - heap.roots_start);
    if (size > max_size) size = max_size;
    if (size <= heap.size) return;

    enable_heap_region(heap.base + heap.size, size - heap.size);

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

    disable_heap_region(heap.base + size, heap.size - size);

    heap.size = size;
    heap.ceiling = heap.base + heap.size - RESERVED_FOR_LAST_HASH;

    if (heap.ceiling > heap.pos_limit) {
        heap.pos_limit = heap.ceiling;
    }
    chunk_size = round_down(heap.size / (10 * num_threads),128);
    INFO("heap shrank to  " << mb(heap.size) << " mb");
}
