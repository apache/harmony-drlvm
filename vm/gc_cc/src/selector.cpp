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

#include "gc_types.h"
#include "collect.h"
#include <math.h>

void reserve_old_object_space(POINTER_SIZE_SINT size) {
    size &= ~(GC_OBJECT_ALIGNMENT - 1);

    if (size < 0) {
        TRACE2("gc.select", "Reserve old object space: can't shrink old object space");
        return;
    }

    assert(heap.old_objects.end == heap.pos);
    if (heap.old_objects.end + size > heap.allocation_region_end()) {
        size = heap.allocation_region_end() - heap.old_objects.end;
    }

    heap.old_objects.end += size;
    TRACE2("gc.select", "Reserved space = " << mb(heap.old_objects.end - heap.old_objects.pos));

    // balancing free areas.
    pinned_areas.push_back(heap.allocation_region_end());

    // update heap.old_objects.pos_limit
    if (heap.old_objects.pos_limit == heap.pos) {
        assert(old_pinned_areas_pos > old_pinned_areas.size());
        if (heap.pos_limit < heap.old_objects.end) {
            // area entirely in old objects
            heap.old_objects.pos_limit = heap.pos_limit;
            heap.pos = pinned_areas[pinned_areas_pos];
            old_pinned_areas.push_back(heap.pos_limit);
            old_pinned_areas.push_back(heap.pos);
            heap.pos_limit = pinned_areas[pinned_areas_pos + 1];
            pinned_areas_pos += 2;
        } else {
            heap.old_objects.pos_limit = heap.old_objects.end;
            heap.pos = heap.old_objects.end;
        }
    }

    while (heap.pos_limit < heap.old_objects.end) {
        // area entirely in old objects
        heap.pos = pinned_areas[pinned_areas_pos];
        old_pinned_areas.push_back(heap.pos_limit);
        old_pinned_areas.push_back(heap.pos);
        heap.pos_limit = pinned_areas[pinned_areas_pos + 1];
        pinned_areas_pos += 2;
    }

    if (heap.pos < heap.old_objects.end) {
        heap.pos = heap.old_objects.end;
    }
    heap.old_objects.end = heap.pos;

    // restore pinned areas.
    pinned_areas.pop_back(); // heap.allocation_region_end()
}

unsigned char *select_gc(int size) {
    // FIXME: missing information of finalizible objects added to evacuation area during allocation
    heap.old_objects.prev_pos = heap.old_objects.pos;

    unsigned char *res;
    int alg = gc_algorithm % 10;

    switch (alg) {
        case 0: break;
        case 1: return full_gc(size);
        case 2: return slide_gc(size);
        default: abort();
    }

    GC_TYPE gc = heap.next_gc;
    TRACE2("gc.select", "starting gc = " << gc_name(gc));
    switch(gc) {
        case GC_COPY: res = copy_gc(size); break;
        case GC_FULL: res = full_gc(size); break;
        case GC_SLIDE_COMPACT: res = slide_gc(size); break;
        default: abort();
    }

    GC_TYPE gc_out = gc_type;

    if (gc_out != gc) {
        // too small reserved space or behaviour changed
    }

    if (!res) {
        TRACE2("gc.mem", "Not enough free memory after collection to allocate " << size << " bytes");
    }
    
    TRACE2("gc.mem", "select_gc = " << res);
    if ((!res) && gc != GC_FULL) {
        TRACE2("gc.select", "no free mem after gc, trying full gc");
        heap.next_gc = GC_FULL;
        res = full_gc(size);
    }

    TRACE2("gc.mem", "select_gc2 = " << res);

    if (res == 0 && heap.size != heap.max_size) {
        assert(heap.pos_limit == heap.allocation_region_end());
        heap_extend(round_up(heap.size + size, 65536));
        if (heap.pos + size <= heap.pos_limit) {
            res = heap.pos;
            heap.pos += size;
        }
    }

    return res;
}

float Smin(float Smax, float Tslow, float Tfast, float dS) {
    /* The function finds maximum for performance function below:
     * Smax - maximum free size = heap.size - working set size
     * Tslow - time of full compaction
     * Tfast - time of copying GC
     * dS - space consumed after each coping GC
     * Smin - minimum free space after which compaction is better then copying
     *
    float perf(float Smax, float Tslow, float Tfast, float dS, float Smin) {
        float avg_free = (Smax + Smin) / 2;
        float n_iter = (Smax - Smin) / dS;
        float total_time = Tslow + Tfast * n_iter;
        float total_free = avg_free * (n_iter + 1);
        return total_free / total_time;
    }*/

    // TODO: simplify expression
    float k = Tslow / Tfast;
    float m = dS / Smax;
    float a = 1;
    float b = - (2 + 2 * k * m);
    float c = k * m * m + 2 * m + 1;
    float D = b * b - 4 * a * c;
    if (D <= 0) {
        return Smax;
    }
    float pm = sqrt (D) / 2 / a;
    float base = - b / 2 / a;
    float res = base - pm;
    if (res > 1.f) res = 1.f;
    return res * Smax;
}

bool need_compaction_next_gc() {
    if (heap.working_set_size == 0 || !gc_adaptive) {
        TRACE2("gc.adaptive", "static Smin analysis");
        return heap.allocation_region_end() - heap.pos < heap.size * 0.7f;
    } else {
        float smin = Smin(heap.roots_start - heap.base - RESERVED_FOR_HEAP_NULL - heap.working_set_size,
                heap.Tcompact, heap.Tcopy, heap.dS_copy);
        float free = (float) (heap.allocation_region_end() - heap.old_objects.pos);
        INFO2("gc.smin", "smin = " << mb((size_t)smin)
                << " (working set " << mb((size_t)heap.working_set_size)
                << " Tfast " << (int)(heap.Tcopy / 1000.)
                << " Tslow " << (int)(heap.Tcompact / 1000.)
                << " dS " << mb((size_t)heap.dS_copy)
                << "), free = " << mb((int)free));
        //INFO2("gc.logic", "Smin = " << (int) mb((int)smin) << "mb, free = " << mb((int)free) << " mb");
        return free < smin;
    }
}

static void check_heap_extend() {
    size_t free_space = heap.allocation_region_end() - heap.allocation_region_start();
    size_t used_space = heap.size - free_space;

    if (free_space < used_space) {
        size_t new_heap_size = used_space * 8;
        if (new_heap_size / 8 != used_space) {
            // overflow!
            new_heap_size = heap.max_size;
        } else if (new_heap_size > heap.max_size) {
            new_heap_size = heap.max_size;
        }

        if (new_heap_size != heap.size) {
            heap_extend(new_heap_size);
        }
    }
}

size_t correction;
Ptr prev_alloc_start;

static void update_evacuation_area() {
    POINTER_SIZE_SINT free = heap.allocation_region_end() - heap.allocation_region_start();
    POINTER_SIZE_SINT incr = heap.allocation_region_start() - heap.old_objects.prev_pos;
    //INFO2("gc.logic", "free = " << free / 1024 / 1024 << " incr = " << incr / 1024 / 1024);

    if (incr > 0 && incr > free) {
        //INFO2("gc.logic", "increment too large, switching to compaction");
        heap.next_gc = GC_FULL;
        return;
    }

    POINTER_SIZE_SINT dS = heap.old_objects.pos - prev_alloc_start;
    if (prev_alloc_start != 0) {
        heap.dS_copy = (float)dS;
    }
    prev_alloc_start = heap.old_objects.pos;

    if (need_compaction_next_gc()) {
        //INFO2("gc.logic", "compaction triggered by Smin");
        heap.next_gc = GC_FULL;
        prev_alloc_start = 0;
        return;
    }

    // original gc type
    GC_TYPE gc = heap.next_gc;
    POINTER_SIZE_SINT overflow = heap.old_objects.pos - heap.predicted_pos;

    // heuristics down here
    
    if (gc != GC_COPY) {
        heap.next_gc = GC_COPY;
        float reserve = (heap.incr_abs + heap.incr_rel * free);
        heap.predicted_pos = heap.old_objects.pos + (POINTER_SIZE_SINT) reserve;
        //INFO2("gc.logic", "1.incr_abs = " << heap.incr_abs / 1024 / 1024 << " mb incr_rel = " << (double) heap.incr_rel);
        reserve_old_object_space(heap.predicted_pos - heap.old_objects.end);
        return;
    }
    assert(incr >= 0);

    /*INFO2("gc.logic", 
            "mb overflow = " << overflow / 1024 / 1024
            << "mb rest = " << mb(heap.old_object_region_end - heap.old_object_region_pos) << " mb");*/

    // correct heap.incr_abs, heap.incr_rel
    if (correction == 0) {
        correction = heap.size / 30;
    }
    overflow += correction;
    float fullness = (float) (free + incr) / heap.size;
    float overflow_rel = fullness * overflow;
    float overflow_abs = (1.f - fullness) * overflow;
    heap.incr_abs += (size_t) overflow_abs;
    if (heap.incr_abs < 0) {
        heap.incr_rel += (overflow_rel + heap.incr_abs) / (free + incr);
        heap.incr_abs = 0;
    } else {
        heap.incr_rel += overflow_rel / (free + incr);
    }


    float reserve = (heap.incr_abs + heap.incr_rel * free);
    heap.predicted_pos = heap.old_objects.pos + (POINTER_SIZE_SINT) reserve;

    //INFO2("gc.logic", "2.incr_abs = " << heap.incr_abs / 1024 / 1024 << " mb incr_rel = " << heap.incr_rel);
    reserve_old_object_space(heap.predicted_pos - heap.old_objects.end);
    heap.next_gc = GC_COPY;
}


void after_copy_gc() {
    update_evacuation_area();
}

void after_slide_gc() {

    check_heap_extend();

    /* FIXME: shrink disabled for safety
        else if (free_space / 9 > used_space) {
        heap_shrink(free_space * 10);
    */

    if (gc_algorithm % 10 != 0) return;

    update_evacuation_area();
}

void select_force_gc() {
    vm_gc_lock_enum();
    if (gc_algorithm < 10) {
        force_gc();
    } else if ((gc_algorithm / 10) == 2) {
        full_gc(0);
    } else if ((gc_algorithm / 10) == 3) {
        heap.old_objects.prev_pos = heap.old_objects.pos;
        copy_gc(0);
    }
    vm_gc_unlock_enum();
    vm_hint_finalize();
}

void init_select_gc() {
    heap.old_objects.end = heap.old_objects.pos = heap.old_objects.pos_limit
        = heap.base + RESERVED_FOR_HEAP_NULL;

    heap.pos = heap.base + RESERVED_FOR_HEAP_NULL;
    heap.pos_limit = heap.allocation_region_end();

    heap.incr_abs = 0;
    heap.incr_rel = 0.2f;

    old_pinned_areas_pos = 1;
    heap_mark_phase = 1;
    pinned_areas_pos = 1;

    if (gc_algorithm % 10 == 0) {
        size_t reserve = heap.size / 5;
        reserve_old_object_space(reserve);
        heap.predicted_pos = heap.base + reserve + RESERVED_FOR_HEAP_NULL;
    }
    if (gc_algorithm % 10 == 3) {
        size_t reserve = heap.size / 3;
        reserve_old_object_space(reserve);
        heap.predicted_pos = heap.base + reserve + RESERVED_FOR_HEAP_NULL;
    }
    heap.next_gc = GC_COPY;
}
