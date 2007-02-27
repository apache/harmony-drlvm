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

#include <assert.h>
#include <iostream>
#include <open/vm_gc.h>
#include <open/gc.h>
#include <cxxlog.h>
#include "gc_types.h"
#include "fast_list.h"
#include "port_atomic.h"
#include "open/hythread_ext.h"

volatile int thread_list_lock;
int num_threads = 0;
Ptr vtable_base;

size_t tls_offset_current=MAX_UINT32, tls_offset_clean=MAX_UINT32, tls_offset_ceiling=MAX_UINT32;
static hythread_tls_key_t tls_key_current=MAX_UINT32, tls_key_clean=MAX_UINT32, tls_key_ceiling=MAX_UINT32;

fast_list<Partial_Reveal_Object*, 1024> finalizible_objects;

#ifdef POINTER64
GCExport Boolean gc_supports_compressed_references() {
    vtable_base = (Ptr) vm_get_vtable_base();
    return true;
}
#endif

GCExport void gc_write_barrier(Managed_Object_Handle p_base_of_obj_with_slot) {
    TRACE2("gc.wb", "gc_write_barrier");
}
GCExport void gc_heap_wrote_object (Managed_Object_Handle p_base_of_object_just_written) {
    // NOTE: looks like the function is redundant for now, it called for all
    // heap objects accesses not only the objects with slots.
    //TRACE2("gc.wb", "gc_heap_wrote_object");
}

GCExport void gc_heap_write_ref (Managed_Object_Handle p_base_of_object_with_slot,
                                 unsigned offset,
                                 Managed_Object_Handle value) {
    TRACE2("gc.wb", "gc_heap_write_ref");
    Managed_Object_Handle *p_slot = 
        (Managed_Object_Handle *)(((char *)p_base_of_object_with_slot) + offset);
    assert (p_base_of_object_with_slot != NULL);
    *p_slot = value;
}
GCExport void gc_heap_slot_write_ref (Managed_Object_Handle p_base_of_object_with_slot,
                                      Managed_Object_Handle *p_slot,
                                      Managed_Object_Handle value) {
    TRACE2("gc.wb", "gc_heap_slot_write_ref");
    assert (p_base_of_object_with_slot != NULL);
    *p_slot = value;
}
//GCExport void gc_heap_slot_write_ref_compressed (Managed_Object_Handle p_base_of_object_with_slot,
//                                                 uint32 *p_slot,
//                                                 Managed_Object_Handle value);




// GCExport void gc_test_safepoint(); optional
Boolean gc_supports_frontier_allocation(unsigned *offset_of_current, unsigned *offset_of_limit) {
    return false;
}

//GCExport void gc_add_root_set_entry_managed_pointer(void **slot,
//                                                    Boolean is_pinned); //  optional

// classloader sometimes sets the bit for finalizible objects (?)
inline unsigned int get_instance_data_size (unsigned int encoded_size) 
{
    return (encoded_size & NEXT_TO_HIGH_BIT_CLEAR_MASK);
}

unsigned char* allocate_from_chunk(int size) {
    unsigned char *res;

    TRACE2("gc.mem", "get next chunk: pinned_areas_pos = " << pinned_areas_pos
            << " pinned_areas.size() = " << pinned_areas.size());
    // lets try next chunk
    while (pinned_areas_pos < pinned_areas.size()) {
        assert(heap.pos_limit <= pinned_areas[pinned_areas_pos]);
        heap.pos = pinned_areas[pinned_areas_pos];
        unsigned char *new_limit = heap.allocation_region_end();

        if (pinned_areas_pos + 1 < pinned_areas.size()) {
            new_limit = pinned_areas[pinned_areas_pos + 1];
        }
        assert(heap.pos_limit <= new_limit);
        //assert(heap.pos <= new_limit);
        heap.pos_limit = new_limit;
        TRACE2("gc.mem", "next chunk[" << pinned_areas_pos << "] = " << heap.pos << " : " << heap.pos_limit);

        pinned_areas_pos += 2;

        if (heap.pos_limit > heap.allocation_region_end()) {
            heap.pos_limit = heap.allocation_region_end();
        }

        if (heap.pos + size <= heap.pos_limit) {
            res = heap.pos;
            heap.pos += size;
            return res;
        }
        // we have unspent memory chunks
    }
    return 0;
}

Managed_Object_Handle gc_alloc_fast(unsigned in_size, 
                                             Allocation_Handle _ah,
                                             void *thread_pointer) {
    VT32 ah = (VT32) _ah;

    //TRACE2("gc.alloc", "gc_alloc_fast");
    assert((in_size % GC_OBJECT_ALIGNMENT) == 0);
    assert (ah);
    unsigned char *next;

    unsigned char* tls_base = (unsigned char*)hythread_self();
    GC_Thread_Info  info(tls_base);
    Partial_Reveal_VTable *vtable = ah_to_vtable(ah);
    GC_VTable_Info *gcvt = vtable->get_gcvt();
    unsigned char *cleaned = info.get_tls_current_cleaned();
    unsigned char *res = info.get_tls_current_free();

    if (res + in_size <= cleaned) {
        if (gcvt->is_finalizible()) return 0;

        info.set_tls_current_free(res + in_size);
        *(VT32*)res = ah;

        assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
        return res;
    }

    if (gcvt->is_finalizible()) return 0;

    unsigned char *ceiling = info.get_tls_current_ceiling();


    if (res + in_size <= ceiling) {
        next = info.get_tls_current_free() + in_size;
        info.set_tls_current_free(next);

        // cleaning required
        unsigned char *cleaned_new = next + THREAD_LOCAL_CLEANED_AREA_SIZE;
        if (cleaned_new > ceiling) cleaned_new = ceiling;
        info.set_tls_current_cleaned(cleaned_new);
        memset(cleaned, 0, cleaned_new - cleaned);
        *(VT32*)res = ah;

        assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
        return res;
    }

    return 0;
}

Managed_Object_Handle gc_alloc(unsigned in_size, 
                                        Allocation_Handle _ah,
                                        void *thread_pointer) {
    VT32 ah = (VT32) _ah;
    TRACE2("gc.alloc", "gc_alloc: " << in_size);
    assert((in_size % GC_OBJECT_ALIGNMENT) == 0);
    assert (ah);

    unsigned char* tls_base = (unsigned char*)hythread_self();
    GC_Thread_Info  info(tls_base);
    Partial_Reveal_VTable *vtable = ah_to_vtable(ah);
    GC_VTable_Info *gcvt = vtable->get_gcvt();
    unsigned char *res = info.get_tls_current_free();
    unsigned char *cleaned = info.get_tls_current_cleaned();

    if (!gcvt->is_finalizible()) {

        if (res + in_size <= cleaned) {
            info.set_tls_current_free(res + in_size);
            *(VT32*)res = ah;

            assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
            return res;
        }

        unsigned char *ceiling = info.get_tls_current_ceiling();

        if (res + in_size <= ceiling) {
            unsigned char *next;
            next = info.get_tls_current_free() + in_size;
            info.set_tls_current_free(next);

            // cleaning required
            unsigned char *cleaned_new = next + THREAD_LOCAL_CLEANED_AREA_SIZE;
            if (cleaned_new > ceiling) cleaned_new = ceiling;
            info.set_tls_current_cleaned(cleaned_new);
            memset(cleaned, 0, cleaned_new - cleaned);

            *(VT32*)res = ah;
            assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
            return (Managed_Object_Handle)res;
        }
    }

    // TODO: can reproduce problems of synchronization of finalizer threads
    // if remove atomic exchange
    if (pending_finalizers) {
        bool run = apr_atomic_xchg32((volatile uint32*)&pending_finalizers, 0);
        if (run) {
            vm_hint_finalize();
        }
    }

    vm_gc_lock_enum();
    
    unsigned size = get_instance_data_size(in_size);

    if (gcvt->is_finalizible()) {
        unsigned char *obj;
        unsigned char *endpos;
        bool allocated = place_into_old_objects(obj, endpos, size);
        if (allocated) {
            memset(obj, 0, size);
            finalizible_objects.push_back((Partial_Reveal_Object*) obj);
            vm_gc_unlock_enum();
            *(VT32*)obj = ah;
            assert(((POINTER_SIZE_INT)obj & (GC_OBJECT_ALIGNMENT - 1)) == 0);
            return (Managed_Object_Handle)obj;
        }

        // reload cached values after possible GC
        res = info.get_tls_current_free();
        cleaned = info.get_tls_current_cleaned();

        if (res + size <= info.get_tls_current_ceiling()) {
            unsigned char *next = info.get_tls_current_free() + size;
            info.set_tls_current_free(next); 
            finalizible_objects.push_back((Partial_Reveal_Object*) res);

            if (cleaned < next) {
                memset(cleaned, 0, next - cleaned);
                info.set_tls_current_cleaned(next);
            }
            vm_gc_unlock_enum();
            *(VT32*)res = ah;
            assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
            return (Managed_Object_Handle)res;
        }
    }

    res = heap.pos;
    if (res + size >= heap.pos_limit) {
        // lets try next chunk
        res = allocate_from_chunk(size);

        if (!res) {
            res = select_gc(size);
        }

        if (!res) {
            vm_gc_unlock_enum();
            vm_hint_finalize();
            TRACE2("gc.verbose", "OutOfMemoryError!\n");
            return 0;
        }

        if (/* in_size != size && */ gcvt->is_finalizible()) {
            finalizible_objects.push_back((Partial_Reveal_Object*) res);
        }
        vm_gc_unlock_enum();
        if (cleaning_needed) memset(res, 0, size);
        *(VT32*)res = ah; // NOTE: object partially initialized, should not be moved!!
                         //       problems with arrays
                         //       no way to call vm_hint_finalize() here
        assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
        return res;
    }

    heap.pos = res + size;

    if (/* in_size != size && */ gcvt->is_finalizible()) {
        finalizible_objects.push_back((Partial_Reveal_Object*) res);
    }

    if (info.get_tls_current_free() + chunk_size / 8 < info.get_tls_current_ceiling()) {
        // chunk is not expired yet, reuse it
        vm_gc_unlock_enum();
        if (cleaning_needed) memset(res, 0, size);
        *(VT32*)res = ah;
        assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
        return (Managed_Object_Handle)res;
    }

    info.set_tls_current_free(heap.pos);
    info.set_tls_current_ceiling(heap.pos + chunk_size);
    if (info.get_tls_current_ceiling() > heap.pos_limit)
        info.set_tls_current_ceiling(heap.pos_limit);
    heap.pos = info.get_tls_current_ceiling();
    if (cleaning_needed) info.set_tls_current_cleaned(info.get_tls_current_free());
    else info.set_tls_current_cleaned(info.get_tls_current_ceiling());

    vm_gc_unlock_enum();
    if (cleaning_needed) memset(res, 0, size);

    *(VT32*)res = ah;
    assert(((POINTER_SIZE_INT)res & (GC_OBJECT_ALIGNMENT - 1)) == 0);
    return (Managed_Object_Handle)res;
}

Managed_Object_Handle gc_pinned_malloc_noclass(unsigned size) {
    TRACE2("gc.alloc", "gc_pinned_malloc_noclass - NOT IMPLEMENTED");
    abort();
    return 0;
}

Managed_Object_Handle gc_alloc_pinned(unsigned size, Allocation_Handle type, void *thread_pointer) {
    TRACE2("gc.alloc", "gc_alloc_pinned - NOT IMPLEMENTED");
    abort();
    return 0;
}

Boolean gc_requires_barriers() {
    // SPAM TRACE2("gc.init", "gc_requires_barriers - NO");
    return false;
}

void gc_thread_init(void *gc_information) {
    TRACE2("gc.thread", "gc_thread_init " << gc_information);

    spin_lock(&thread_list_lock);
    if (tls_key_current == MAX_UINT32) {
        //allocate TLS data
        hythread_tls_alloc(&tls_key_current);
        hythread_tls_alloc(&tls_key_clean);
        hythread_tls_alloc(&tls_key_ceiling);

        //cache TLS offsets
        tls_offset_current = hythread_tls_get_offset(tls_key_current);
        tls_offset_clean = hythread_tls_get_offset(tls_key_clean);
        tls_offset_ceiling = hythread_tls_get_offset(tls_key_ceiling);
    }
    unsigned char* tls_base = (unsigned char*)hythread_self();
    GC_Thread_Info  info(tls_base);
    info.set_tls_current_free(0);
    info.set_tls_current_ceiling(0);
    info.set_tls_current_cleaned(0);

    int n = ++num_threads;
    chunk_size = round_down(heap.size / (10 * n),128);
    spin_unlock(&thread_list_lock);

}

void gc_thread_kill(void *gc_information) {
    TRACE2("gc.thread", "gc_thread_kill " << gc_information);
    
    spin_lock(&thread_list_lock);
    int n = --num_threads;
    if (n != 0)
        chunk_size = round_down(heap.size / (10 * n),128);
    spin_unlock(&thread_list_lock);
}

void gc_force_gc() {
    TRACE2("gc.collect", "gc_force_gc");
    select_force_gc();
}

int64 gc_total_memory() 
{
    return heap.size;
}

int64 gc_max_memory()
{
    return heap.max_size;
}

int64 gc_free_memory() 
{
    return (int64) ((heap.allocation_region_end() - heap.pos) + (heap.old_objects.end - heap.old_objects.pos));
}

void gc_pin_object (Managed_Object_Handle* p_object) {
#if 0
    // FIXME: overflow check and handling
    Partial_Reveal_Object *obj = *(Partial_Reveal_Object**) p_object;

    volatile uint8 *info = (volatile uint8 *)&obj->obj_info_byte();
    uint8 value = *info;
    if ((value & OBJECT_IS_PINNED_BITS) == OBJECT_IS_PINNED_BITS) {
        LDIE2("gc", 1, "no handling for pin overflow");
    }

    while (true) {
        uint8 old_value = port_atomic_cas8(info, value + OBJECT_IS_PINNED_INCR, value);
        if (old_value == value) return;
        value = old_value;
    }
#endif
}

void gc_unpin_object (Managed_Object_Handle* p_object) {
#if 0
    Partial_Reveal_Object *obj = *(Partial_Reveal_Object**) p_object;
    assert((obj->obj_info_byte() & OBJECT_IS_PINNED_BITS) != 0);

    volatile uint8 *info = (volatile uint8 *)&obj->obj_info_byte();
    uint8 value = *info;
    while (true) {
        uint32 old_value = port_atomic_cas8(info, value - OBJECT_IS_PINNED_INCR, value);
        if (old_value == value) return;
        value = old_value;
    }
#endif
}

Boolean gc_is_object_pinned (Managed_Object_Handle p_object) {
    Partial_Reveal_Object *obj = (Partial_Reveal_Object*) p_object;
    assert ((obj->obj_info_byte() & OBJECT_IS_PINNED_INCR) == 0);
    return false;
#if 0
    Partial_Reveal_Object *obj = (Partial_Reveal_Object*) p_object;
    return (obj->obj_info_byte() & OBJECT_IS_PINNED_INCR) != 0;
#endif
}

int32 gc_get_hashcode(Managed_Object_Handle p_object) {
    Partial_Reveal_Object *obj = (Partial_Reveal_Object*) p_object;
    if (!obj) return 0;
    assert((unsigned char*)obj >= heap.base && (unsigned char*)obj < heap.ceiling);
    assert(obj->vtable());
    unsigned char info = obj->obj_info_byte();
    // FIXME: atomic ops need to keep pinning work?
    int hash;
    if (info & HASHCODE_IS_SET_BIT) {
        if (info & HASHCODE_IS_ALLOCATED_BIT) {
            int offset = get_object_size(obj, obj->vtable()->get_gcvt());
            unsigned char *pos = (unsigned char *)obj;
            hash = *(int*) (pos + offset);
            check_hashcode(hash);
        } else {
            hash = gen_hashcode(obj);
        }
    } else {
        obj->obj_info_byte() = info | HASHCODE_IS_SET_BIT;
        hash = gen_hashcode(obj);
    }
    return hash;
}

Managed_Object_Handle gc_get_next_live_object(void *iterator) {
    TRACE2("gc.iter", "gc_get_next_live_object - NOT IMPLEMENTED");
    abort();
    return 0;
}

unsigned int gc_time_since_last_gc() {
    TRACE2("gc.time", "gc_time_since_last_gc");
    return 0;
}

void *gc_heap_base_address() {
    return (void*) heap.base;
}
void *gc_heap_ceiling_address() {
    return (void*) (heap.base + heap.max_size);
}

void gc_finalize_on_exit() {
    process_finalizible_objects_on_exit();
}

/**
 * Iterates all objects in the heap.
 * This function calls vm_iterate_object() for each
 * iterated object.
 * Used for JVMTI Heap Iteration.
 * Should be called only in stop-the-world setting
 *
 * @see vm_gc.h#vm_iterate_object()
 */

#ifdef GC_YUK_JVMTI_HEAP_ITERATION

static bool gc_iterate_region(Ptr pos, Ptr limit) {
    while (pos < limit) {
        int32 vt = *(int32*)pos;
        if (vt == 0) {
            pos += sizeof(int32);
            continue;
        }
        Partial_Reveal_Object *obj = (Partial_Reveal_Object*) pos;
        obj->valid();
        bool cont = vm_iterate_object((Managed_Object_Handle) obj);
        if (!cont) return false;

        pos += get_object_size(obj, obj->vtable()->get_gcvt());

        if (obj->obj_info() & HASHCODE_IS_ALLOCATED_BIT) {
            pos += sizeof(int32);
        }
    }
    return true;
}

void gc_iterate_heap() {
    // data structures in not consistent for heap iteration
    if (!jvmti_heap_iteration) return;
    bool cont;

    // iterate over old objects
    cont = gc_iterate_region(heap.base, heap.old_objects.pos);
    if (!cont) return;

    // iterate over new objects
    cont = gc_iterate_region(heap.old_objects.end, heap.pos);
    if (!cont) return;

    // iterate over pinned objects in evacuation area
    for(unsigned opos = old_pinned_areas_pos; opos < old_pinned_areas.size(); opos += 2) {
        Partial_Reveal_Object *obj = (Partial_Reveal_Object*) old_pinned_areas[opos];
        obj->valid();
        cont = vm_iterate_object((Managed_Object_Handle) obj);
        if (!cont) return;
    }

    // iterate over pinned objects in new objects area
    for (unsigned pos = pinned_areas_pos; pos < pinned_areas.size(); pos += 2) {
        Partial_Reveal_Object *obj = (Partial_Reveal_Object*) pinned_areas[pos];
        obj->valid();
        cont = vm_iterate_object((Managed_Object_Handle) obj);
        if (!cont) return;
    }
}
#endif

