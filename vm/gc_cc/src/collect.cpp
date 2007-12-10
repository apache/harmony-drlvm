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

#include <vector>
#include <open/vm_gc.h>
#include <jni_types.h>
#include "gc_types.h"
#include "collect.h"
#include "timer.h"
#include <stdio.h>
#include "open/hythread_ext.h"

fast_list<Slot,65536> slots;
reference_vector soft_references;
reference_vector weak_references;
reference_vector phantom_references;
reference_vector to_finalize;

slots_vector weak_roots;
slots_vector phantom_roots;

std::vector<unsigned char*> pinned_areas;
fast_list<unsigned char*, 1024> pinned_areas_unsorted;
unsigned pinned_areas_pos = 0;

std::vector<unsigned char*> old_pinned_areas;
unsigned old_pinned_areas_pos = 0;

int gc_num = 0;
apr_time_t total_gc_time = 0;
apr_time_t total_user_time = 0;
apr_time_t last_gc_time = 0;
apr_time_t last_user_time = 0;
apr_time_t max_gc_time = 0;
int object_count = 0;
int soft_refs = 0;
int weak_refs = 0;
int phantom_refs = 0;
apr_time_t gc_start, gc_end;

enum GC_TYPE gc_type;

void
gc_time_start_hook(apr_time_t *start_time) {	
	*start_time = apr_time_now();
}


apr_time_t
gc_time_end_hook(const char *event, apr_time_t *start_time, apr_time_t *end_time)
{	
    *end_time = apr_time_now();
    apr_time_t time =  *end_time - *start_time;
    INFO2("gc.time", time/1000 << " ms, " << event);
    //INFO2("gc.objs", "Live objects = " << object_count);
    return time;
}

void notify_gc_start()
{
    gc_num++;
    last_gc_time = 0;

    gc_time_start_hook(&gc_start);
    last_user_time = gc_start - gc_end;
    if (gc_num == 1) {
        timer_calibrate(last_user_time);
    }
}

const char *gc_name(GC_TYPE gc_type) {
    switch (gc_type) {
        case GC_COPY: return "COPY";
        case GC_FORCED: return "FORCED";
        case GC_FULL: return "FULL";
        case GC_SLIDE_COMPACT: return "COMPACT";
        default: return "UNKNOWN";
    };
}

void notify_gc_end() {
    last_gc_time = gc_time_end_hook("GC", &gc_start, &gc_end);

    if (last_gc_time > max_gc_time) max_gc_time = last_gc_time;
    total_gc_time += last_gc_time;
    total_user_time += last_user_time;

    const char *gc_type_str = gc_name(gc_type);
    
    INFO2("gc.verbose", "GC " << gc_type_str << " ["
            << (gc_num - 1) << "]: " << last_gc_time/1000 << " ms, "
        "User " << last_user_time/1000 << " ms, "
        "Total GC " << total_gc_time/1000 << " ms, "
        "Total User " << total_user_time/1000 << " ms, "
        "Used " << ((int64)heap.size - gc_free_memory()) / 1024 / 1024 << " mb"
    );
}

void clear_thread_local_buffers() {
    int disable_count = hythread_reset_suspend_disable();
    hythread_iterator_t it = hythread_iterator_create(0);
    int count = (int)hythread_iterator_size (it);
    for (int i = 0; i < count; i++){
        unsigned char* tls_base = (unsigned char*)hythread_iterator_next(&it);
        GC_Thread_Info  info(tls_base);
        info.set_tls_current_free(0);
        info.set_tls_current_cleaned(0);
        info.set_tls_current_ceiling(0);
    }
    hythread_iterator_release(&it);
    hythread_set_suspend_disable(disable_count);
}

static void enumerate_universe() {
    if (remember_root_set) {
        // remember root set before doing modifications in heap
        gc_cache_retrieve_root_set();
        gc_cache_emit_root_set();
    } else {
        // default behaviour
        vm_enumerate_root_set_all_threads();
    }
}

void process_finalizible_objects_on_exit() {
    vm_gc_lock_enum();
    // FIXME: leak of processed objects.
    for(reference_vector::iterator i = finalizible_objects.begin();
            i != finalizible_objects.end(); ++i) {

        Partial_Reveal_Object *obj = *i;
        if (!obj) continue;
        vm_finalize_object((Managed_Object_Handle)obj);
    }
    finalizible_objects.clear();
    vm_gc_unlock_enum();
}

void process_finalizable_objects() {
    // FIXME: leak of processed objects.
    for(reference_vector::iterator i = finalizible_objects.begin();
            i != finalizible_objects.end();) {

        Partial_Reveal_Object *obj = *i;
        assert (obj);

        int info = obj->obj_info();
        if (info & heap_mark_phase) {
            // marked
            TRACE2("gc.debug", "0x" << obj << " referenced from finalizible objects (marked)");
            gc_add_root_set_entry((Managed_Object_Handle*)&*i, false);
        } else {
            // not marked
            TRACE2("gc.debug", "0x" << obj << " referenced from finalizible objects (unmarked)");
            to_finalize.push_back(obj);

            // removing this object from vector, replacing it with last one.
            *i = finalizible_objects.pop_back();
            continue; // without promotion of iterator
        }
        ++i;
    }

    if (!to_finalize.empty()) {
        for(reference_vector::iterator i = to_finalize.begin();
                i != to_finalize.end(); ++i) {
            gc_add_root_set_entry((Managed_Object_Handle*)&*i, false);
        }

        pending_finalizers = true;
        TRACE2("gc.finalize", to_finalize.count() << " objects to be finalized");
    }
}

void finalize_objects() {
    for(reference_vector::iterator i = to_finalize.begin();
            i != to_finalize.end(); ++i) {
        vm_finalize_object((Managed_Object_Handle)*i);
    }
    to_finalize.clear();
}

void process_special_roots(slots_vector& array) {
    for(slots_vector::iterator i = array.begin();
            i != array.end(); ++i) {
        Partial_Reveal_Object **ref = *i;

        Partial_Reveal_Object* obj = *ref;

        if (obj == 0) {
            // reference already cleared
            continue;
        }

        int info = obj->obj_info();
        if (info & heap_mark_phase) {
            // object marked, is it moved?
            int vt = obj->vt();
            if (!(vt & FORWARDING_BIT)) continue;
            // moved, updating referent field
            *ref = fw_to_pointer(vt & ~FORWARDING_BIT);
            assert((((POINTER_SIZE_INT)*ref) & 3) == 0);
            continue;
        }

        // object not marked, clear ref
        *ref = 0;
    }
    array.clear();
}

void process_special_references(reference_vector& array) {
    if (array.empty()) return;
    pending_finalizers = true;

    for(reference_vector::iterator i = array.begin();
            i != array.end(); ++i) {
        Partial_Reveal_Object *ref = *i;

        Slot referent( (Reference*) ((Ptr)ref + global_referent_offset) );
        Partial_Reveal_Object* obj = referent.read();

        if (obj == heap_null) {
            // reference already cleared
            continue;
        }

        unsigned info = obj->obj_info();
        if (info & heap_mark_phase) {
            // object marked, is it moved?
            unsigned vt = obj->vt();
            if (!(vt & FORWARDING_BIT)) continue;
            // moved, updating referent field
            referent.write( fw_to_pointer(vt & ~FORWARDING_BIT) );
            continue;
        }

        // object not marked
        referent.write(heap_null);
        TRACE2("gc.ref", "process_special_references: reference enqueued");
        vm_enqueue_reference((Managed_Object_Handle)ref);
    }
    array.clear();
}

// scan only finalizible object, without enqueue
void scan_finalizible_objects() {
    for(reference_vector::iterator i = finalizible_objects.begin();
            i != finalizible_objects.end();++i) {
        gc_add_root_set_entry((Managed_Object_Handle*) &*i, false);
    }
}

static void prepare_gc() {
    notify_gc_start(); 

    object_count = 0;
    assert (soft_references.empty());
    assert (weak_references.empty());
    assert (phantom_references.empty());
    soft_refs = weak_refs = phantom_refs = 0;
}

unsigned char*
try_alloc(int size) {
    unsigned char *res;
    TRACE2("gc.oome", "heap.pos = " << heap.pos << " heap.base = " << heap.base <<
            " max_heap_size = " << heap.max_size << " heap.pos - heap.base = " << (heap.pos - heap.base));
    TRACE2("gc.oome", "max_heap_size / 100 = " << (heap.max_size) / 100
            << " (heap.pos - heap.base) / 85 = " << (heap.pos - heap.base) / 85);
#if 1
    if ((size_t)(heap.pos - heap.base) / 85 > (heap.max_size) / 100) {
        TRACE2("gc.oome", "Returning Zero");
        res = 0;
    } else
#endif
    if (heap.pos + size <= heap.pos_limit) {
        res = heap.pos;
        heap.pos += size;
    } else {
        res = allocate_from_chunk(size);
    }
    return res;
}
    

unsigned char *full_gc(int size) {
    Timer gc_time("FULL_GC", "gc.time.total");
    heap.old_objects.end = heap.old_objects.pos = heap.old_objects.pos_limit = heap.base + RESERVED_FOR_HEAP_NULL;
    unsigned char *res = slide_gc(size);

    heap.Tcompact = (float) gc_time.dt();
    heap.working_set_size = (float) (heap.old_objects.pos - heap.base);
    return res;
}

static unsigned char*
finish_slide_gc(int size, int stage, int dis_count) {
    if (stage == 0) {
        gc_slide_process_special_references(soft_references);
        gc_slide_process_special_references(weak_references);
        gc_slide_process_special_roots(weak_roots);
        process_finalizable_objects();
    }
    gc_slide_process_special_references(soft_references);
    gc_slide_process_special_references(weak_references);
    gc_slide_process_special_references(phantom_references);
    gc_slide_process_special_roots(phantom_roots);

    TIME(gc_slide_move_all,());
    roots_update();
    gc_slide_postprocess_special_references();
    gc_deallocate_mark_bits();
    finalize_objects();

    heap_mark_phase ^= 3;
    // reset thread-local allocation areas
    clear_thread_local_buffers();
    after_slide_gc();

    unsigned char *res = try_alloc(size);

    vm_resume_threads_after();
    hythread_set_suspend_disable(dis_count);
    notify_gc_end();
    TRACE2("gc.mem", "finish_slide_compact = " << res);
    return res;
}

unsigned char *slide_gc(int size) {
    Timer gc_time("SLIDE_GC", "gc.time.total");
    prepare_gc();

    pinned_areas.clear();
    pinned_areas_unsorted.clear();
    roots_clear();
    gc_type = GC_SLIDE_COMPACT;
    gc_allocate_mark_bits();

    int disable_count = hythread_reset_suspend_disable();
    TIME(enumerate_universe,());
    return finish_slide_gc(size, 0, disable_count);
}

void transition_copy_to_sliding_compaction(fast_list<Slot,65536>& slots) {
    INFO2("gc.verbose", "COPY -> COMP on go transition");
    gc_type = GC_SLIDE_COMPACT;
    gc_allocate_mark_bits();
    gc_slide_process_transitional_slots(slots);
}

unsigned char *copy_gc(int size) {
    Timer gc_time("COPY_GC", "gc.time.total");
    prepare_gc();
    TRACE2("gc.debug", "limits 0x" << heap.compaction_region_start() << " 0x" << heap.compaction_region_end());

    pinned_areas.clear();
    pinned_areas_unsorted.clear();
    roots_clear();

    int disable_count = hythread_reset_suspend_disable();
    gc_type = GC_COPY;
    TIME(enumerate_universe,());

    if (gc_type == GC_SLIDE_COMPACT) {
        unsigned char *res = finish_slide_gc(size, 0, disable_count);
        heap.Tcopy = (float) gc_time.dt();
        return res;
    }
    process_special_references(soft_references);
    process_special_references(weak_references);
    process_special_roots(weak_roots);
    process_finalizable_objects();
    if (gc_type == GC_SLIDE_COMPACT) {
        unsigned char *res = finish_slide_gc(size, 1, disable_count);
        heap.Tcopy = (float) gc_time.dt();
        return res;
    }
    process_special_references(soft_references);
    process_special_references(weak_references);
    process_special_references(phantom_references);
    process_special_roots(phantom_roots);
    roots_update();
    finalize_objects();

    heap_mark_phase ^= 3;
    gc_copy_update_regions();
    heap.Tcopy = (float) gc_time.dt();
    after_copy_gc();
    // reset thread-local allocation areas
    clear_thread_local_buffers();

    unsigned char *res = try_alloc(size);
    vm_resume_threads_after();
    hythread_set_suspend_disable(disable_count);
    notify_gc_end();
    TRACE2("gc.mem", "copy_gc = " << res);
    return res;
}

void force_gc() {
    Timer gc_time("FORCE_GC", "gc.time.total");
    prepare_gc();

    roots_clear();

    int disable_count = hythread_reset_suspend_disable();
    gc_type = GC_FORCED;
    TIME(enumerate_universe,());
    TIME(process_special_references,(soft_references));
    TIME(process_special_references,(weak_references));
    TIME(process_special_roots,(weak_roots));
    TIME(process_finalizable_objects,());
    TIME(process_special_references,(soft_references));
    TIME(process_special_references,(weak_references));
    TIME(process_special_references,(phantom_references));
    TIME(process_special_roots,(phantom_roots));
    roots_update();
    TIME(finalize_objects,());

    heap_mark_phase ^= MARK_BITS; // toggle mark bits
    // reset thread-local allocation areas
    //clear_thread_local_buffers();

    TIME(vm_resume_threads_after,());
    hythread_set_suspend_disable(disable_count);
    notify_gc_end();
}

void gc_add_weak_root_set_entry(Managed_Object_Handle *_slot, 
    Boolean is_pinned, Boolean is_short_weak) {
    Partial_Reveal_Object **slot = (Partial_Reveal_Object**) _slot;
    (*slot)->vt();
    assert(!is_pinned);
    if (is_short_weak) {
        weak_roots.push_back(slot);
    } else {
        phantom_roots.push_back(slot);
    }
}
