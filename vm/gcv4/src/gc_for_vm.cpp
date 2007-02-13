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
 * @version $Revision: 1.1.2.4.4.3 $
 */  


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// System header files
#include <iostream>

// VM interface header files
#include "port_malloc.h"
#include "platform_lowlevel.h"
#include "open/vm_gc.h"
#include "open/gc.h"
#include "jit_intf.h"

// GC header files
#include "gc_cout.h"
#include "gc_header.h"
#include "gc_v4.h"
#include "remembered_set.h"
#include "block_store.h"
#include "object_list.h"
#include "work_packet_manager.h"
#include "garbage_collector.h"
#include "gc_globals.h"
#include "characterize_heap.h"
#include "gc_version.h"

/* NOTE:
 *   There're also exported functions in allocation.cpp and scan_object.cpp.
 */
Boolean gc_supports_compressed_references ();
void gc_next_command_line_argument (const char *name, const char *arg);
void gc_class_loaded (VTable_Handle vth);
void gc_init();
void gc_vm_initialized();
void gc_wrapup();
Boolean gc_requires_barriers();
void gc_write_barrier(Managed_Object_Handle p_base_of_object_holding_ref);
void gc_heap_wrote_object (Managed_Object_Handle p_base_of_object);
void gc_heap_slot_write_ref (Managed_Object_Handle p_base_of_object_with_slot,
                         Managed_Object_Handle *p_slot,
                         Managed_Object_Handle value);

void gc_heap_slot_write_ref_compressed(Managed_Object_Handle p_base_of_object_with_slot,
                                                  uint32 *p_slot,
                                                  Managed_Object_Handle value);

void gc_heap_write_global_slot(Managed_Object_Handle *p_slot,
                               Managed_Object_Handle value);
void gc_heap_write_global_slot_compressed(uint32 *p_slot,
                               Managed_Object_Handle value);

void gc_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned);
void gc_add_root_set_entry(Managed_Object_Handle *ref1, Boolean is_pinned);
void gc_add_weak_root_set_entry(Managed_Object_Handle *ref1, Boolean is_pinned,Boolean is_short_weak);
void gc_add_compressed_root_set_entry(uint32 *ref, Boolean is_pinned);
Managed_Object_Handle gc_alloc(unsigned size, Allocation_Handle ah, void *tp);
Managed_Object_Handle gc_pinned_malloc_noclass(unsigned size) ;
void gc_thread_init(void *gc_information) ;
void gc_thread_kill(void *gc_information) ;
void gc_force_gc() ;
int64 gc_total_memory(); 
int64 gc_free_memory();
unsigned int gc_time_since_last_gc();

void * gc_heap_base_address();
void * gc_heap_ceiling_address();
Boolean gc_is_object_pinned (Managed_Object_Handle obj);
void gc_class_prepared (Class_Handle ch, VTable_Handle vth);

Boolean gc_supports_frontier_allocation (unsigned *offset_of_current, unsigned *offset_of_limit);

void gc_suppress_finalizer(Managed_Object_Handle obj);
void gc_register_finalizer(Managed_Object_Handle obj);

////////////////////////// GC EXPORT FUNCTION LIST////////////////////////////////



//*****************************************************
//
// Some helper functions.
// If these need to be changed then the class.h function
// probable need to be looked at also.
//*****************************************************

inline unsigned int get_instance_data_size (unsigned int encoded_size) 
{
    return (encoded_size & NEXT_TO_HIGH_BIT_CLEAR_MASK);
}
 
// ******************************************************************
/****
*
*  Routines to support the initialization and termination of GC.
* 
*****/


//
// API for the VM to hand to the GC any arguments starting with -gc. 
// It is up to the GC to parse these arguments.
//
// Input: name - a string holding the name of the parameter 
//               assumed to begin with "-gc"
//        arg  - a string holding the argument following name on 
//               the command line.
//
extern bool compaction_enabled;
extern bool fullheapcompact_at_forcegc;
extern int incremental_compaction;
extern bool machine_is_hyperthreaded;
extern bool mark_scan_load_balanced;
extern bool verify_live_heap;
extern int force_gc_worker_thread_number;
extern bool cross_block_compaction;
extern bool use_large_pages;
extern bool sweeps_during_gc;
extern bool force_unaligned_heap;
extern bool ignore_finalizers;

static long parse_size_string(const char* size_string) {
    size_t len = strlen(size_string);
    int unit = 1;
    if (tolower(size_string[len - 1]) == 'k') {
        unit = 1024;
    } else if (tolower(size_string[len - 1]) == 'm') {
        unit = 1024 * 1024;
    } else if (tolower(size_string[len - 1]) == 'g') {
        unit = 1024 * 1024 * 1024;
    }
    long size = atol(size_string);
    long res = size * unit;
    if (res / unit != size || size <= 0) {
        // the value is either overflowed or non-positive
        return 0;
    }
    return res;
}

static bool get_property_value_boolean(char* name) {
    const char* value = vm_get_property_value(name);
    return (NULL != value 
        && 0 != value[0]
        && strcmp("0", value) != 0
        && strcmp("off", value) != 0 
        && strcmp("false", value) != 0);
}

static int get_property_value_int(char* name) {
    const char* value = vm_get_property_value(name);
    return (NULL == value) ? 0 : atoi(value);
}

static bool is_property_set(char* name) {
    const char* value = vm_get_property_value(name);
    return (NULL != value && 0 != value[0]);
}

static void parse_configuration_properties() {

    if (is_property_set("gc.ms")) {
        long sz = parse_size_string(vm_get_property_value("gc.ms"));
        if (sz > 0) {
            initial_heap_size_bytes = sz;
        } else {
            LECHO(18, "WARNING: Incorrect {0} gc size specified, using default" << minimum);
        }
    }
    
    if (is_property_set("gc.mx")) {
        long sz = parse_size_string(vm_get_property_value("gc.mx"));
        if (sz > 0) {
            final_heap_size_bytes = sz;
        } else {
            LECHO(18, "WARNING: Incorrect {0} gc size specified, using default" << "maximum");
        }
    }

    // set the initial and maximum heap size based on provided options and defaults
    const int Mb = 1048576;
    if (0 == final_heap_size_bytes && 0 == initial_heap_size_bytes) {
        // defaults -Xms64m -Xmx256m
        final_heap_size_bytes = 256*Mb;
        initial_heap_size_bytes = 64*Mb;
    } else if (0 == initial_heap_size_bytes) {
        // -Xms64m or same as mx, whichever is less
        if (final_heap_size_bytes > 64*Mb) 
            initial_heap_size_bytes = 64*Mb;
        else 
            initial_heap_size_bytes = final_heap_size_bytes;
    } else if (0 == final_heap_size_bytes) {
        // -Xmx256m or same as ms, whichever is greater
        if (initial_heap_size_bytes < 256*Mb)
            final_heap_size_bytes = 256*Mb;
        else
            final_heap_size_bytes = initial_heap_size_bytes;
    }

    // provide sensible minimum values
    if (initial_heap_size_bytes < 1*Mb) {
        initial_heap_size_bytes = 1*Mb;
        WARN("Increasing heap size to minimum supported size of "
                << (initial_heap_size_bytes/Mb) << " Mb");
        if (final_heap_size_bytes < initial_heap_size_bytes)
            final_heap_size_bytes = initial_heap_size_bytes;
    }

    // read stats_gc from logger configuration
    stats_gc = is_info_enabled("gc.stats");
    
    if (get_property_value_boolean("gc.fixed")) {
        compaction_enabled = false;
        INFO2("gc.compact", "GC will not move any objects -- fixed collector ");
    } else {

        if (get_property_value_boolean("gc.no_full_compaction_at_force_gc")) {
            INFO2("gc.compact", "GC will not compact the full heap when gc_force_gc() is called.");
            fullheapcompact_at_forcegc = false;
        }
        if (is_property_set("gc.incremental_compaction"))
            incremental_compaction = get_property_value_int("gc.incremental_compaction");
        if (incremental_compaction) {
            INFO2("gc.compact", "GC will incrementally slide compact at each GC,"
                    " using algorithm = " << incremental_compaction);
        } else {
            INFO2("gc.compact", "GC will slide compact whole heap at each GC");
        }
    }

    if (is_property_set("gc.verify"))
        verify_gc = get_property_value_boolean("gc.verify");
    if (verify_gc) 
        INFO2("gc.verify", "GC will verify heap and GC data structures before and after GC.");

    
    if (is_property_set("gc.load_balanced"))
        mark_scan_load_balanced = get_property_value_boolean("gc.load_balanced");
    if (mark_scan_load_balanced)
        INFO2("gc.balance", "GC will try to load balance effectively");

    if (is_property_set("gc.verify_live_heap"))
        verify_live_heap = get_property_value_boolean("gc.verify_live_heap");
    if (verify_live_heap)
        INFO2("gc.verify", "GC will verify the live heap thoroughly before and after GC");

    if (is_property_set("gc.thread_number")) {
        force_gc_worker_thread_number = atoi(vm_get_property_value("gc.thread_number"));
        INFO2("gc.threads", "GC will use " << force_gc_worker_thread_number 
                << " thread" << (force_gc_worker_thread_number > 1 ? "s":"") 
                << " for collection");
    }
    if (get_property_value_boolean("gc.ignore_finalizers")) {
        ignore_finalizers = true;
        INFO2("gc.finalize", "GC will ignore finalizers");
    }

    if (is_property_set("gc.cross_block_compaction"))
        cross_block_compaction = get_property_value_boolean("gc.cross_block_compaction");
    if (!cross_block_compaction)
        INFO2("gc.compact", "GC will slide objects only within a block");

    if (is_property_set("gc.sweeps_during_gc"))
        sweeps_during_gc = get_property_value_boolean("gc.sweeps_during_gc");
    INFO2("gc.sweep", "Chunks will be swept " << (sweeps_during_gc ? "during GC":"on allocation"));


#ifdef _IPF_
    if (is_property_set("gc.large_pages"))
        use_large_pages = get_property_value_boolean("gc.large_pages");
    if (use_large_pages)
        INFO2("gc.heap", "GC will use large pages");
#endif // _IPF_

    if (is_property_set("gc.unalign_heap"))
        force_unaligned_heap = get_property_value_boolean("gc.unalign_heap");
    if (force_unaligned_heap)
        INFO2("gc.heap", "Heap will be forcedly unaligned");
}

//
// gc_init initialized the GC internal data structures.
// This routine is called after the last call to gc_next_command_line_argument.
// The VM should call this *before* any other calls to this interface except
// calls to gc_next_command_line_argument.
//
// 
// gc_init needs a couple of routines to build empty classes so it can
// generated filler objects when it needs to align an object.
//

// THESE ROUTINES GO OUTSIDE THE EXPECTED GC/ORE INTERFACES AND SHOULD
// BE ELIMINATED......

//
// A couple of simple helper function to support gc_init.
//

//
// A function that is not part of the interface but can be used to make sure 
// our compile flags make sense.
// 

void check_compile_flags()
{
    // For not this is basically a noop.
    int collectors_defined = 0;

    collectors_defined++;
 
    if (collectors_defined != 1) {
        DIE("Error: One and only One collector can be defined.");
    }
}

//
// API for the VM to ask the GC to initialize itself.
//

POINTER_SIZE_INT Partial_Reveal_Object::vtable_base;
POINTER_SIZE_INT Partial_Reveal_Object::heap_base = 0;

//
// This also needs to do the work of gc_thread_init for the main thread.
//
void gc_init()
{
    INFO(GC_NAME " " GC_VERSION);
    TRACE2("gc.sizeof", "sizeof(block_info) = " << sizeof(block_info));
    TRACE2("gc.sizeof", "GC_BLOCK_INFO_SIZE_BYTES = " << GC_BLOCK_INFO_SIZE_BYTES);
    assert(sizeof(block_info) <= GC_BLOCK_INFO_SIZE_BYTES);
#ifdef _WIN32
    if (!vm_get_boolean_property_value_with_default("vm.assert_dialog"))
    {
        TRACE2("init", "disabling assertion dialogs in GC");
        disable_assert_dialogs();
    }
#endif

    parse_configuration_properties();

    // At development time, check to ensure that the compile time
    // flags were set correctly. If verbosegc is on announce what
    // gc is running.
    //
    if (vm_number_of_gc_bytes_in_vtable() < sizeof(void *))
    {
        DIE("GC_V4 error: vm_number_of_gc_bytes_in_vtable() returns " << (unsigned)(vm_number_of_gc_bytes_in_vtable()) <<
            " bytes, minimum " << (unsigned)sizeof(Partial_Reveal_VTable) << " needed.");
    }

    if (vm_number_of_gc_bytes_in_thread_local() < sizeof(GC_Thread_Info))
    {
        DIE("GC_V4 error: vm_number_of_gc_bytes_in_thread_local() returns " << (unsigned)(vm_number_of_gc_bytes_in_thread_local()) <<
            " bytes, minimum " << (unsigned)sizeof(GC_Thread_Info) << " needed.");
    }

    bool compressed = (vm_vtable_pointers_are_compressed() ? true : false);
    if (Partial_Reveal_Object::use_compressed_vtable_pointers() != compressed)
    {
        DIE("GC error: mismatch between Partial_Reveal_Object::use_compressed_vtable_pointers()="
            << Partial_Reveal_Object::use_compressed_vtable_pointers() << "\n"
            << "          and vm_vtable_pointers_are_compressed()=" << compressed);
    }
    Partial_Reveal_Object::vtable_base = vm_get_vtable_base();

#ifndef DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
    gc_references_are_compressed = (vm_references_are_compressed() ? true : false);
#endif // !DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES

    check_compile_flags();

    //
    // -Dcharacterize_heap=on creates *histogram.txt files that characterize the heap.
    // These files can be "imported" to excel since they are comma delimited.
    // the characterize_heap.xls file is from excel and can be used to visualize these files and the data.
    //
    if ((strcmp (vm_get_property_value("characterize_heap"), "on") == 0) ||
        (strcmp (vm_get_property_value("characterize_heap"), "ON") == 0)) {
        INFO("-- -------------------------------------------------- Turning characterize_heap (and verify) code on.");
        characterize_heap = true; // This only works with _DEBUG on
    }
    
    if ((strcmp (vm_get_property_value("characterize_heap"), "off") == 0) ||
        (strcmp (vm_get_property_value("characterize_heap"), "OFF") == 0)) {
        INFO("-- -------------------------------------------------- Turning characterize_heap code off.");
        characterize_heap = false;   
    }


    // The following flag will be set to true 
    // (by the VM calling gc_vm_initialized) when the VM is fully initialized
    // Till then we can't do a stop-the-world.
    //
    vm_initialized = false;

    interior_pointer_table = new slot_offset_list();
    compressed_pointer_table = new slot_offset_list();

    assert(p_global_gc == NULL);

    // Creates the garbage Collector
    p_global_gc = new Garbage_Collector(initial_heap_size_bytes, final_heap_size_bytes, GC_BLOCK_SIZE_BYTES);

    assert(p_global_gc);

    // Create and initialize the garbage collector
    p_global_gc->gc_v4_init();

    // create a nursery for the main thread
    // gc_thread_init(vm_get_gc_thread_local());
    
    assert (sizeof(block_info) <= 4096); 
    // If we hit this we should take a close look at our defines. While it is possible
    // to use more than one page that seems real expensive.
}


//
// This API is used by the VM to notify the GC that the
// VM has completed bootstrapping and initialization, and 
// is henceforth ready to field requests for enumerating 
// live references.
//
// Prior to this function being called the GC might see some
// strange sights such as NULL or incomplete vtables. The GC will
// need to consider these as normal and work with the VM to ensure 
// that bootstrapping works. This means that the GC will make few
// demands on the VM prior to this routine being called.
//
// However, once called the GC will feel free to do 
// stop-the-world collections and will assume that the entire
// vm_for_gc interface is available and fully functioning.
//
// If this routine is called twice the result is undefined.
//
void gc_vm_initialized()
{
    if (vm_initialized) {
        WARN(" Internal error - Multiple calls to gc_vm_initialized encountered.");
    }
    vm_initialized = true;
}


//
// This is called once the VM has no use for the heap or the 
// garbage collector data structures. The assumption is that the 
// VM is exiting but needs to give the GC time to run destructors 
// and free up memory it has gotten from the OS.
// After this routine has been called the VM can not relie on any
// data structures created by the GC.
//
// Errors: If gc_enumerate_finalizable_objects has been called and
//         gc_wrapup gc discovers an object that has not had it
//         finalizer run then it will attempt to report an error.
//

 

void gc_wrapup() 
{
    p_global_gc->notify_gc_shutdown();

    if (characterize_heap) {
        heapTraceFinalize();    
    }

    if (interior_pointer_table)
        delete interior_pointer_table;
    if (compressed_pointer_table)
        delete compressed_pointer_table;
    if (p_global_gc != NULL)
        delete p_global_gc;
}

/****
*
*  Routines to support barriers such as read and write barriers.
* 
*****/

//
// This API is used by the VM (JIT) to find out if
// the GC requires read or write barriers. If the GC requests
// write barriers and the JIT does not support write barriers the results
// are undefined.
//
// Output: 
//         1 if the garbage collector expects to be notified whenever
//              a slot holding a pointer to an object is modified in the heap.
//         0 otherwise.
//
// Comments: Future version might extend possible return values 
//           so that the system can support other barriers such as a 
//           read barrier or a write barrier on all heap writes, not 
//           just pointer writes.
//
//


Boolean gc_requires_barriers()
{
    return FALSE;
}

//
// If gc_requires_write_barriers() returns 1, the Jit-ted code 
// should call this after doing the putfield or an array store or
// a reference.
//
// If gc_requires_write_barrier() returns 2 then this is called
// whenever anything is stored into the heap, not only references.
// 
//
// Likewise the VM needs to call it whenever it stores a pointer into the
// heap.
//
// Input: p_base_of_obj_with_ref - the base of the object that
//                                 had a pointer stored in it.
//
// Concurrent or generational GC will make use of the provided information.
//
void __fastcall gc_write_barrier_fastcall(Partial_Reveal_Object *p_base_of_object_holding_ref) 
{
    gc_write_barrier(p_base_of_object_holding_ref);
} //gc_write_barrier_fastcall



void gc_write_barrier(Managed_Object_Handle UNREF p_base_of_object_holding_ref) 
{
}



//
// The following routines are the only way to alter any value in the gc heap.  
//

// In a place with gc disabled an entire object was written, for example inside
// clone. This means that the entire object must be scanned and treated as if
// all the fields had been altered. 

void gc_heap_wrote_object (Managed_Object_Handle p_base_of_object)
{
    gc_write_barrier(p_base_of_object);
}

GCExport
/*inline*/ void gc_heap_write_ref (Managed_Object_Handle p_base_of_object_with_slot,
                         unsigned offset,
                         Managed_Object_Handle value)
{ 
    assert (p_base_of_object_with_slot != NULL);

    Managed_Object_Handle *p_slot = 
        (Managed_Object_Handle *)(((char *)p_base_of_object_with_slot) + offset);
    *p_slot = value;
    gc_write_barrier (p_base_of_object_with_slot);
}

void gc_heap_slot_write_ref (Managed_Object_Handle p_base_of_object_with_slot,
                         Managed_Object_Handle *p_slot,
                         Managed_Object_Handle value)
{
    assert (p_base_of_object_with_slot != NULL);

    *p_slot = value;
    gc_write_barrier (p_base_of_object_with_slot);
}

void gc_heap_slot_write_ref_compressed(Managed_Object_Handle p_base_of_object_with_slot,
                                                  uint32 *p_slot,
                                                  Managed_Object_Handle value)
{
    assert(gc_references_are_compressed);
    assert(p_slot != NULL);
    if (value == NULL)
    {
        *p_slot = 0;
    }
    else
    {
        *p_slot = (uint32) ((POINTER_SIZE_INT)value - (POINTER_SIZE_INT)gc_heap_base_address());
    }
    gc_write_barrier (p_base_of_object_with_slot);
}
  
// There are some global slots that are shared by different threads.
// Concurrent gc needs to know about writes to these slots. One example
// of such slots is in the string pools used by the class loader.
void gc_heap_write_global_slot(Managed_Object_Handle *p_slot,
                               Managed_Object_Handle value)
{
    *p_slot = value;
}

// p_slot is the address of a 32 bit global slot holding the offset of a referenced object in the heap.
// That slot is being updated, so store the heap offset of "value"'s object. If value is NULL, store a 0 offset.
void gc_heap_write_global_slot_compressed(uint32 *p_slot,
                               Managed_Object_Handle value)
{
    assert(gc_references_are_compressed);
    assert(p_slot != NULL);
    if (value == NULL)
    {
        *p_slot = 0;
    }
    else
    {
        *p_slot = (uint32) ((POINTER_SIZE_INT)value - (POINTER_SIZE_INT)gc_heap_base_address());
    }
    // 20030501 No write barrier is needed because this function is only called
    // when updating static fields, and these are explicitly enumerated as
    // GC roots.
    //
    // 20030502
    // While this is true for STW collectors, concurrent collectors need to be
    // aware of all pointers that can be passed from one thread to another. For
    // example this is needed to ensure the no black to white invariant or
    // whatever tri color invariant you are using.
}

/****
*
*  Routines to support read barriers
*
****/

/****
*
*  Routines to support building the root set during a collection.
* 
*****/

// *slot is guaranteed not to the beginning of an object unless it has fallen off the
// end of an array.  We'll subtract 1 in places to make sure the pointer is in the
// interior of the object to which it points.
// 
// Managed pointers are like interior pointers except we don't know what the offset is.
void gc_add_root_set_entry_managed_pointer(void **slot, Boolean is_pinned)
{
    // By subtracting one byte we guarantee the pointer is in the memory for the object.
    void *true_slot = ((Byte*)*slot) - 1;

    // Check if the interior pointer points to a heap address managed by this GC
    if ( true_slot > p_global_gc->get_gc_heap_base_address() && true_slot <= p_global_gc->get_gc_heap_ceiling_address() ) {
        // Find the starting block whether it is a single_object_block or not
        // We subtract 1 to guarantee that the pointer is into the object and not
        // one past the end of an array.
        block_info *pBlockInfo = p_global_gc->find_start_of_multiblock(true_slot);
        
        Partial_Reveal_Object *p_obj = NULL;

        // check for single object block
        if ( pBlockInfo->is_single_object_block ) {
            // In a single object block, the object will start at the beginning of the block's allocation area.
            p_obj = (Partial_Reveal_Object*)GC_BLOCK_ALLOC_START(pBlockInfo);
        } else {
            typedef free_area tightly_filled_area;
            tightly_filled_area current_area_being_scanned;
            // current_area_being_scanned.area_base will keep track of the base of the current used area
            current_area_being_scanned.area_base    = GC_BLOCK_ALLOC_START(pBlockInfo);
            current_area_being_scanned.area_ceiling = 0;
            unsigned int i=0;
            
            // Handle the case where there are no free areas in the block.
            if (pBlockInfo->block_free_areas[0].area_base == 0) {
                // Will bypass the following for loop.
                i = GC_MAX_FREE_AREAS_PER_BLOCK;
                // Compute the ending address of this block's allocation area.
                current_area_being_scanned.area_ceiling = GC_BLOCK_CEILING(current_area_being_scanned.area_base);
            }
            
            // loop through the free areas...free areas are the inverse of used areas and demarcate them.
            for (;i<GC_MAX_FREE_AREAS_PER_BLOCK;++i) {
                // Make sure that the array of free areas is ordered.
                // This invariant is supposed to be maintained by Garbage_Collector::sweep_one_block(...) 
                // in GCv4_sweep.cpp.
                // This check will also make sure we haven't failed to find the used block before
                // going into the unused portion of the free areas array.
                assert(current_area_being_scanned.area_base <= pBlockInfo->block_free_areas[i].area_base);
                // Algorithm
                // ---------
                // 1. We already know that *slot must be >= current_area_being_scanned.area_base because we initialized 
                // current_area_being_scanned.area_base to be the start of the allocation block and the list of free areas
                // is monotonically increasing.
                // 2. Any of the "free" areas could be the current allocation area.  The most up-to-date
                // base for the current allocation free area is in TLS so the interior pointer could
                // point into what looks like a free area.
                // 3. Therefore, consider adjacent used and free areas as one big area and see if the
                // interior pointer is in that area.  See diagram of a block below:
                // [block info][used area 0][free area 0][used1][free1][used2][free2]...
                // We will group [used N] with [free N].  Note, this approach handles the case of
                // a block just starting to be allocated where the free area starts at the beginning
                // of the block allocation area.  So, [used area 0] is really optional.
                // 4. This approach seemed simpler than having a loop that first determined whether
                // the interior pointer was in (the optional) used area 0, then checked [free0] then
                // checked [used1], [free1], [used2], etc.  Combining them together makes the loop
                // simpler and reduces the amount of corner case code.
                // 5. We can still extract better bounds later by seeing if the interior pointer is
                // greater or less than pBlockInfo->block_free_areas[i].area_base.  If it is greater then
                // the base is the free area base (this free area must be the current allocation area)
                // and the ceiling is that free area's ceiling.  Otherwise, the base is [used0].base
                // and the ceiling is [free0].base-1.
                //
                // Here, if true we've found the combined area where the interior pointer falls.
                if (true_slot <= pBlockInfo->block_free_areas[i].area_ceiling) {
                    // Check if the interior pointer appears to be in a used or free area.
                    if (true_slot >= pBlockInfo->block_free_areas[i].area_base) {
                        // The interior pointer is in a free area, which must be the current allocation area.
                        current_area_being_scanned.area_base    = pBlockInfo->block_free_areas[i].area_base;
                        current_area_being_scanned.area_ceiling = pBlockInfo->block_free_areas[i].area_ceiling;
                    } else {
                        // The interior pointer is in a used area.
                        current_area_being_scanned.area_ceiling = (Byte*)pBlockInfo->block_free_areas[i].area_base - 1;
                    }
                    break;
                } else {
                    // We haven't found the area yet so save the start of the next used area
                    current_area_being_scanned.area_base = (Byte*)pBlockInfo->block_free_areas[i].area_ceiling + 1;
                }
            }
            assert(current_area_being_scanned.area_ceiling);
            
            // Get the first live object in this used area
            Partial_Reveal_Object *p_next_obj = (Partial_Reveal_Object*)current_area_being_scanned.area_base;
            do {
                p_obj = p_next_obj;
                // Keep getting the next live object until the interior pointer is less than the next object we just found
                p_next_obj = (Partial_Reveal_Object*)((Byte*)p_obj + get_object_size_bytes(p_obj));
                assert(p_next_obj <= current_area_being_scanned.area_ceiling);
            } while (true_slot > p_next_obj);
        }

        // We can reuse the normal code below by now computing the correct offset
        int offset = ((POINTER_SIZE_INT)*slot) - ((POINTER_SIZE_INT)p_obj);

        assert (p_obj->vt());
        interior_pointer_table->add_entry (slot, p_obj, offset);
        // pass the slot from the interior pointer table to the gc.
        p_global_gc->gc_internal_add_root_set_entry(interior_pointer_table->get_last_base_slot());

        if (is_pinned) {
            // Add the current block as one that can't be compacted since it has a pinned root
            // pointing into it.
            p_global_gc->gc_internal_block_contains_pinned_root(GC_BLOCK_INFO(p_obj));
        }
    }
}


// 
// Call from the VM to the gc to enumerate an interior pointer. **ref is a slot holding a pointer
// into the interior of an object. The base of the object is located at *ref - offset. The strategy
// employed is to place the slot, the object base and the offset into a slot_base_offset table. We then
// call gc_add_root_set_entry with the slot in the table holding the base of the object. Upon completion
// of the garbage collection the routine fixup_interior_pointers is called and the slot_base_offset table
// is traversed and the new interior pointer is calculated by adding the base of the object and the offset.
// This new interior pointer value is then placed into the slot.
//
// This routine can be called multiple times with the same interior pointer without any problems.
// The offset is checked to make sure it is positive but the logic is not dependent on this fact.

void gc_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned)
{
    assert(offset > 0);

    Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *)((Byte*)*slot - offset);
    assert (p_obj->vt());
    interior_pointer_table->add_entry (slot, p_obj, offset);
    // pass the slot from the interior pointer table to the gc.
     p_global_gc->gc_internal_add_root_set_entry(interior_pointer_table->get_last_base_slot());

    if (is_pinned) {
        // Add the current block as one that can't be compacted since it has a pinned root
        // pointing into it.
        p_global_gc->gc_internal_block_contains_pinned_root(GC_BLOCK_INFO(p_obj));
    }
}



//
// Call from the VM to the GC to enumerate another
// live reference.
//
// Input: ref - the location of a slot holding a pointer that
//              is NULL or points to a valid object in the heap.
//



void gc_add_root_set_entry(Managed_Object_Handle *ref1, Boolean is_pinned)
{
    TRACE2("gc.roots", "gc_add_root_set_entry " << ref1 << " -> " << *ref1);
    Partial_Reveal_Object **ref = (Partial_Reveal_Object **) ref1;
    p_global_gc->gc_internal_add_root_set_entry(ref);

    if (is_pinned) {
        // Add the current block as one that can't be compacted since it has a pinned root
        // pointing into it.
        p_global_gc->gc_internal_block_contains_pinned_root(GC_BLOCK_INFO(*ref));
    }
}


void gc_add_weak_root_set_entry(Managed_Object_Handle *ref1, Boolean is_pinned,Boolean is_short_weak)
{
    Partial_Reveal_Object **ref = (Partial_Reveal_Object **) ref1;
    p_global_gc->gc_internal_add_weak_root_set_entry(ref,is_short_weak);

    if (is_pinned) {
        // Add the current block as one that can't be compacted since it has a pinned root
        // pointing into it.
        p_global_gc->gc_internal_block_contains_pinned_root(GC_BLOCK_INFO(*ref));
    }
}


// Resembles gc_add_root_set_entry() but is passed the address of a slot containing a compressed reference.
void gc_add_compressed_root_set_entry(uint32 *ref, Boolean is_pinned)
{
    if (*ref == 0)
        return;
    Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *) (*ref + (POINTER_SIZE_INT)gc_heap_base_address());
    TRACE2("gc.roots", "gc_add_compressed_root_set_entry " << ref << " -> " << p_obj);
    assert(p_obj->vt());
    compressed_pointer_table->add_entry((void **)ref, p_obj, (POINTER_SIZE_INT)gc_heap_base_address());
    p_global_gc->gc_internal_add_root_set_entry(compressed_pointer_table->get_last_base_slot());

    if (is_pinned) {
        // Add the current block as one that can't be compacted since it has a pinned root
        // pointing into it.
        p_global_gc->gc_internal_block_contains_pinned_root(GC_BLOCK_INFO(p_obj));
    }
}



/****
*
*  Routines to support the allocation and initialization of objects.
* 
*****/

//
// Allocation of objects.
//
// There is a tension between fast allocation of objects and 
// honoring various constraints the VM might place on the object. 
// These constraints include registering the objects for 
// finalization, aligning the objects on multiple word boundaries, 
// pinning objects for performance reasons, registering objects 
// related to weak pointers and so forth.
//
// We have tried to resolve this tension by overloading the 
// size argument that is passed to the allocation routine. If 
// the size of the argument has a high bit of 0, then the 
// allocation routine will assume that no constraints exist 
// on the allocation of this object and allocation can potentially 
// be made very fast. If on the other hand the size is large then 
// the routine will query the class data structure to determine 
// what constraints are being made on the allocation of this object.
//
//
// See vm_for_gc interface for masks that allow the gc to quickly 
// determine the constraints.

//
// This routine is the primary routine used to allocate objects. 
// It assumes nothing about the state of the VM internal data 
// structures or the runtime stack. If gc_alloc_fast is able 
// to allocate the object without invoking a GC or calling the VM
// then it does so. It places p_vtable into the object, ensures 
// that the object is zeroed and then returns a Partial_Reveal_Object 
// pointer to the object. If it is not able to allocate the object 
// without invoking a GC then it returns NULL.
//
// Input: size - the size of the object to allocate. If the high bit
//               set then various constraints as described above are
//               placed on the allocation of this object.
//        p_vtable - a pointer to the vtable of the class being 
//                   allocated. This routine will place this value 
//                   in the appropriate slot of the new object.
//

#ifdef _IPF_
#define ALIGN8
#endif

//
// This routine is used to allocate an object. See the above 
// discussion on the overloading of size.
// The GC assumes that the VM is ready to support a GC if it 
// calls this function.
//
// Input: size - the size of the object to allocate. If the high bit
//               set then various constraints as described above are
//               placed on the allocation of this object.
//        p_vtable - a pointer to the vtable of the class being allocated.
//                   This routine will place this value in the 
//                   appropriate slot of the new object.
//

Managed_Object_Handle gc_malloc_slow(unsigned size, Allocation_Handle ah, void *tp,Boolean do_not_relocate);

Managed_Object_Handle gc_alloc(unsigned size, Allocation_Handle ah, void *tp)
{
    // All chunks of data requested need to be multiples of GC_OBJECT_ALIGNMENT
    assert((size % GC_OBJECT_ALIGNMENT) == 0);
    
    assert (ah);
          
    bool is_finalizable = !ignore_finalizers && class_is_finalizable(allocation_handle_get_class(ah));

    p_global_gc->check_hint_to_finalize();

    unsigned int real_size = get_instance_data_size (size);
    // If the next to high bit is set, that indicates that this object needs to be pinned.
    Partial_Reveal_Object *result = (Partial_Reveal_Object *) gc_malloc_slow (real_size, ah, tp, false);
    if (result != NULL) {

        assert (result->vt());
        gc_trace ((void *)result, "object is allocated");
    
        if (characterize_heap) {
            assert(size);
            assert(ah);
            heapTraceAllocation(result, size);
        }

        // Put the object in the finalize set if it needs a finalizer run eventually.
        if (is_finalizable) {
            TRACE2("gc.finalize", "registering " << result << " for finalization");
            p_global_gc->add_finalize_object(result);
        }
    }

    if (NULL == result) {
        INFO2("gc", "out of memory, can't allocate " << size << " bytes");
    }
    return  result; 
} // gc_alloc


// forward declaration
Managed_Object_Handle gc_malloc_slow_no_constraints (unsigned , Allocation_Handle, void *);


//
// A helper function that is prepared to make sure that space is available for
// the object. We assume that the vm is prepared for a gc to happen within this
// routine.
//
Managed_Object_Handle gc_malloc_slow(unsigned size, Allocation_Handle ah, void *tp,Boolean do_not_relocate) 
{

    Managed_Object_Handle p_return_object = NULL;

    struct Partial_Reveal_VTable *p_vtable = Partial_Reveal_Object::allocation_handle_to_vtable(ah);
    unsigned int class_constraints = p_vtable->get_gcvt()->gc_class_properties;

    if (class_constraints == 0) {
        p_return_object = gc_malloc_slow_no_constraints (size, ah, tp);
        return p_return_object;
    }

    if (class_get_alignment(p_vtable->get_gcvt()->gc_clss)) {
#ifdef _IPF_
        // There is no special alignment hack needed for IPF
        ASSERT(0, "No need for alignment on IPF");
#endif
        
        // We hava a special object that needs 
        //
        // In phase 1 of alignment, re-direct all objects
        // with special alignment needs to the LOS.
        // CLEANUP -- remove this cast....
        
        p_return_object = (Partial_Reveal_Object *)gc_pinned_malloc (size,
                                                  ah,
                                                  false, // returnNullOnFail is false
                                                  true);
    }

    // See what constraints are placed on this allocation and call the appropriate routine.
    // CLEANUP  -- remove this cast.
    if (p_return_object == NULL) {
        if (class_is_pinned(p_vtable->get_gcvt()->gc_clss) || do_not_relocate) {
            p_return_object = (Partial_Reveal_Object *)gc_pinned_malloc(size,
                                                     ah,
                                                     false,
                                                     false);
        }
    }

    if (p_return_object == NULL) {
        
        // alloc up an array or a normal object that needs to be finalized.
        p_return_object = gc_malloc_slow_no_constraints (size, ah, tp);
    }

    gc_trace ((void *)p_return_object, "Allocated in gc_malloc_slow.");

    // Salikh 2004-12-03
    // NULL return means that memory is exhausted and memory allocation stub
    // should throw an OutOfMemoryError
    return p_return_object;
}


Partial_Reveal_Object *gc_los_malloc_noclass (unsigned size, bool UNREF in_a_gc) 
{
    //
    // The size can't be zero, since there is a requisite VTable pointer
    // word that is the part of every object, and the size supplied
    // to us in this routine should include that VTable pointer word.
    //
    assert(size!=0);
    // Classes loaded before java.lang.Class must use this API, because the
    // vtable for java.lang.Class hasn't been constructed yet.
    // Currently only three classes should be allocated through this call:
    // java.lang.Object, java.io.Serializable and java.lang.Class.
    
    assert ((size % GC_OBJECT_ALIGNMENT) == 0);
    Partial_Reveal_Object *result_start = NULL;

    result_start = gc_pinned_malloc(size, 0, false, false);
    memset (result_start, 0x0, size); 
    return result_start;
}


//
// For bootstrapping situations, when we still don't have
// a class for the object. This routine is only available prior to 
// a call to the call gc_vm_initialized. If it is called after
// the call to gc_vm_initialized then the results are undefined. 
// The GC places NULL in the vtable slot of the newly allocated
// object.
// 
// The object allocated will be pinned, not finalizable and not an array.
//
// Input: size - the size of the object to allocate. The high bit
//               will never be set on this argument.
// Output: The newly allocated object
//
Managed_Object_Handle gc_pinned_malloc_noclass(unsigned size) 
{
   // CLEANUP -- remove this cast.
    Partial_Reveal_Object *p_return = gc_los_malloc_noclass (size, false);
    gc_trace((void *)p_return, "Allocated as a pinned object."); 
    // we can't characterize an object without a vtable.
    return p_return;
}


/****
*
*  Routines to support threads.
* 
*****/
//
// This routine is called during thread startup to set
// an initial nursery for the thread.
//
// Comment - gc_thread_init and gc_thread_kill assume that
//           the current thread is the one we are interested in
//           If we passed in the thread then these things could be
//           cross inited and cross killed.
//

// Code to allow manipulation of p_global_gc->active_thread_gc_info_list in a thread safe manner.


/** 
 * Return a new nursery that the VM requests. This can be during thread
 * initialization so that calls to vm_get_nursery() will have a nursery to
 * return.
 */
static void *gc_get_new_nursery ()
{
    block_info *a_nursery = p_global_gc->p_cycle_chunk(NULL);
    //FIXME: it's nice to allocate the new nursery for the thread,
    // but p_cycle_chunk may not always be able to do this.
    //assert(a_nursery);
    return a_nursery;
}

void gc_thread_init(void *gc_information) 
{
  
    static bool gc_thread_init_bool = true;
    if (gc_thread_init_bool) {
        TRACE("gc_thread_init() : FILL ME");
        gc_thread_init_bool = false;
    }

    GC_Thread_Info *info = (GC_Thread_Info *) gc_information;
    info->chunk = gc_get_new_nursery();
    info->curr_alloc_block = info->chunk;

    info->tls_current_free = NULL;
    info->tls_current_ceiling = NULL;

    get_active_thread_gc_info_list_lock();     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

    p_global_gc->notify_thread_init();

    volatile GC_Thread_Info *temp = p_global_gc->active_thread_gc_info_list;
    while (temp) {
        if ((void *)temp == gc_information) {
            // THIS IS A BUG IF WE GET HERE AND IT WAS FIXED BUT FOR NOW I'M IGNORING IT ? 1-30-03
            TRACE("Why is this gc_information block on the active list already???? ");
            break;
        }
        temp = temp->p_active_gc_thread_info; // Do it for all the threads.
    }
    if (temp == NULL) {
        // If temp == gc_information then this thread is already on the active list
        // so we can't add it again. This is because for some reason the VM
        // is calling thread init twice..
        info->p_active_gc_thread_info = (GC_Thread_Info *)p_global_gc->active_thread_gc_info_list;
        p_global_gc->active_thread_gc_info_list = info;
}
    release_active_thread_gc_info_list_lock(); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    return;
}


//
// This is called just before the thread is reclaimed.
//

void gc_thread_kill(void *gc_information) 
{
    
    GC_Thread_Info *gc_info = (GC_Thread_Info *)gc_information;

    // Return the chunk used for allocation by this thread to the store.
    block_info *spent_block = (block_info *)gc_info->chunk;
    
    while (spent_block) {
        assert(spent_block);
        assert (spent_block->nursery_status == active_nursery);
        spent_block->nursery_status = spent_nursery;
        
        spent_block = spent_block->next_free_block;
    }

    
    get_active_thread_gc_info_list_lock();     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

    p_global_gc->notify_thread_kill();

    volatile GC_Thread_Info *temp_active_thread_gc_info_list = p_global_gc->active_thread_gc_info_list;   
    assert (p_global_gc->active_thread_gc_info_list);

    if ((void *)temp_active_thread_gc_info_list == gc_information) {
        // it is at the head of the list.
        p_global_gc->active_thread_gc_info_list = p_global_gc->active_thread_gc_info_list->p_active_gc_thread_info;
    } else {
        int count = 1;
        while (temp_active_thread_gc_info_list->p_active_gc_thread_info != gc_info) {
            count++;
            temp_active_thread_gc_info_list = temp_active_thread_gc_info_list->p_active_gc_thread_info;
            assert (temp_active_thread_gc_info_list->p_active_gc_thread_info);
        }
        assert (gc_information == temp_active_thread_gc_info_list->p_active_gc_thread_info);
        temp_active_thread_gc_info_list->p_active_gc_thread_info = gc_info->p_active_gc_thread_info;
    }

    release_active_thread_gc_info_list_lock(); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#ifdef GC_CONCURRENT
    // We need to let the GC know about any gray objects in the gc_tls before we return and gc_information becomes
    // invalid.
    WARN(" BUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUG    fill me up gc_thread_kill ");
#endif
    return;
}

/****
*
*  Routines to support the functionality required by the Java language specification.
* 
*****/

//
// API for the VM to force a GC, typically in response to a call to 
// java.lang.Runtime.gc
//
void gc_force_gc() 
{
    vm_gc_lock_enum();

    p_global_gc->reclaim_full_heap(0, fullheapcompact_at_forcegc);

    vm_gc_unlock_enum();
    vm_hint_finalize();
}


// Support for Delinquent Regions and Data EAR work

void * gc_heap_base_address() 
{
    return p_global_gc->get_gc_heap_base_address();
}
 
 
 
void * gc_heap_ceiling_address() 
{
    return p_global_gc->get_gc_heap_ceiling_address();
}
 
/**
 * Returns time in milliseconds since last GC.
 */
unsigned gc_time_since_last_gc() 
{
    // get_time_since_last_gc() returns time in microseconds, as
    // port_time_now() does
    return (unsigned)(p_global_gc->get_time_since_last_gc()/1000);
}


/****
*
*  Routines to support the functionality required by Jini to see if an object is pinned.
* 
*****/


Boolean gc_is_object_pinned (Managed_Object_Handle obj)
{
    /**
     * @note This API is not for interrior pointers.
     */
    Partial_Reveal_Object *p_obj = (Partial_Reveal_Object *) obj;

    if (compaction_enabled) {
        // Moving GC
        if (GC_BLOCK_INFO(p_obj)->pin_count != 0) return TRUE;

        if (GC_BLOCK_INFO(p_obj)->in_los_p || GC_BLOCK_INFO(p_obj)->is_single_object_block) {
            return TRUE;
        } else {
            assert(GC_BLOCK_INFO(p_obj)->in_nursery_p);
            // No guarantee can be offered that it will not be moved...
            return FALSE;
        } 
    } else {
        // FIXED GC -- no object moves
        return TRUE;
    } 
}

/****
*
*  Routines to support the the class loader and to allow the GC to seperate out and 
*  control the format of the data structures that it uses.
* 
*****/

Field_Handle 
get_field_handle_from_offset(Partial_Reveal_VTable *vt, unsigned int offset) 
{
     unsigned num_fields = class_num_instance_fields_recursive(vt->get_gcvt()->gc_clss);
     for(unsigned int idx = 0; idx < num_fields; idx++) {
         Field_Handle fh = class_get_instance_field_recursive(vt->get_gcvt()->gc_clss, idx);
         if(field_is_reference(fh)) {
            unsigned int off = field_get_offset(fh);
            if (off == offset) {
                return fh;
            }
        }
     }
    ABORT("Can't find the specified field");
    return 0;
}


// A comparison function for qsort().
static int 
intcompare(const void *vi, const void *vj)
{
    const int *i = (const int *) vi;
    const int *j = (const int *) vj;
    if (*i > *j)
        return 1;
    if (*i < *j)
        return -1;
    return 0;
}


//
// Given a v table build an array of slot offsets used by instantiations of objects
// using this vtable.
// Input
//     ch - the class handle for this object type.
//     vt - vtable for this object type.
//     for_compressed_fields - whether the result should correspond to raw fields or 32-bit compressed fields
// Returns
//  a 0 delimited array of the slots in an object of this type.
//
// Uses
//    num_instance_fields - the number of fields in an object of this type.
//    get_instance_field  - takes an index and returns the associated field.
//    field_is_reference  - True if the field is a slot (ie holds a reference)
//    field_get_offset    - given a field returns its offset from the start of the object.
//

static int *build_slot_offset_array(Class_Handle ch, Partial_Reveal_VTable *vt) 
{
    int *result = NULL;

    unsigned num_ref_fields = 0;
    //
    // Careful this doesn't give you the number of instance fields.
    // Calculate the size needed for the offset table.
    //
    unsigned num_fields = class_num_instance_fields_recursive(ch);

    unsigned idx;
    for(idx = 0; idx < num_fields; idx++) {
        Field_Handle fh = class_get_instance_field_recursive(ch, idx);
        if(field_is_reference(fh)) {
            num_ref_fields++;
        }
    }

    // We need room for the terminating 0 so add 1.
    unsigned int size = (num_ref_fields+1) * sizeof (unsigned int);

    // malloc up the array if we need one.
    int *new_ref_array = (int*) class_alloc_via_classloader(ch, size);

    result = new_ref_array;
    for(idx = 0; idx < num_fields; idx++) {
        Field_Handle fh = class_get_instance_field_recursive(ch, idx);
        if(field_is_reference(fh)) {
            *new_ref_array = field_get_offset(fh);
            new_ref_array++;
        }
    }

    // It is 0 delimited.
    *new_ref_array = 0;

    // Update the number of slots.
    vt->get_gcvt()->gc_number_of_slots += num_ref_fields;

    // The VM doesn't necessarily report the reference fields in
    // memory order, so we sort the slot offset array.  The sorting
    // is required by the verify_live_heap code.
    qsort(result, num_ref_fields, sizeof(*result), intcompare);
    
    return result;
}

// Setter functions for the gc class property field.
void gc_set_prop_alignment_mask (Partial_Reveal_VTable *vt, unsigned int the_mask)
{
    vt->get_gcvt()->gc_class_properties |= the_mask;
}
void gc_set_prop_non_ref_array (Partial_Reveal_VTable *vt)
{
    vt->get_gcvt()->gc_class_properties |= CL_PROP_NON_REF_ARRAY_MASK;
}
void gc_set_prop_array (Partial_Reveal_VTable *vt)
{
    vt->get_gcvt()->gc_class_properties |= CL_PROP_ARRAY_MASK;
}
void gc_set_prop_pinned (Partial_Reveal_VTable *vt)
{
    vt->get_gcvt()->gc_class_properties |= CL_PROP_PINNED_MASK;
}
void gc_set_prop_finalizable (Partial_Reveal_VTable *vt)
{
    vt->get_gcvt()->gc_class_properties |= CL_PROP_FINALIZABLE_MASK;
}

//
// Given a class and a field name find the offset to that field.
// The field has to be a ref field and it must exist.
//

unsigned int get_field_offset (Class_Handle ch, char *field_name) 
{
    //
    // Careful this doesn't give you the number of instance fields.
    // Calculate the size needed for the offset table.
    //
    unsigned num_fields = class_num_instance_fields_recursive(ch);

    unsigned idx;
    for(idx = 0; idx < num_fields; idx++) {
        Field_Handle fh = class_get_instance_field_recursive(ch, idx);
        if(strcmp (field_get_name(fh), field_name) == 0) {
            assert (field_is_reference(fh));
            return field_get_offset(fh);
        }
    }
    ABORT("Can't find the specified field");
    return 0;
}

Class_Handle get_field_class (Class_Handle ch, unsigned int offset) 
{
    //
    // Careful this doesn't give you the number of instance fields.
    // Calculate the size needed for the offset table.
    //
    unsigned num_fields = class_num_instance_fields_recursive(ch);

    unsigned idx;
    for(idx = 0; idx < num_fields; idx++) {
        Field_Handle fh = class_get_instance_field_recursive(ch, idx);
        if (field_get_offset(fh) == offset) {
            assert (field_is_reference(fh));
            return field_get_class_of_field_value(fh);
        }
    }
    ABORT("Can't find the specified field");
    return 0;
}

static bool type1_fuse_type2(Class_Handle type1, Class_Handle type2)
{
    int* gc_fuse_info = ((Partial_Reveal_VTable *)class_get_vtable(type1))->get_gcvt()->gc_fuse_info;
    if (gc_fuse_info == NULL)
        return false;
    unsigned int i = 0;
    while (gc_fuse_info[i] != 0) {
        Class_Handle fch = get_field_class(type1, gc_fuse_info[i]/*fuse offset*/);
        if (fch == type2) {//type1 fuse type2 directly
            return true;
        }
        if (type1_fuse_type2(fch, type2))
            return true;
        ++i;
    }
    return false;
}

void gc_class_prepared (Class_Handle ch, VTable_Handle vth)
{
    TRACE2("debug.gc.class_prepared", "gc_class_prepared(" << class_get_name(ch) << ")");
    assert(ch);
    assert(vth);
    Partial_Reveal_VTable *vt = (Partial_Reveal_VTable *)vth;
    void* p = class_alloc_via_classloader(ch, sizeof(GC_VTable_Info));
    vt->set_gcvt((GC_VTable_Info *) p);
    assert(vt->get_gcvt());
    memset((void *) vt->get_gcvt(), 0, sizeof(GC_VTable_Info));
    vt->get_gcvt()->gc_clss = ch;
    vt->get_gcvt()->gc_class_properties = 0; // Clear the properties.
    vt->get_gcvt()->gc_object_has_slots = false;
    // Set the properties.
    gc_set_prop_alignment_mask(vt, class_get_alignment(ch));

    if(class_is_array(ch)) {
        Class_Handle array_element_class = class_get_array_element_class(ch);
        // We have an array so not it.
        gc_set_prop_array(vt);
        // Get the size of an element.
        vt->get_gcvt()->gc_array_element_size = class_element_size(ch);
        
        unsigned int the_offset = vector_first_element_offset_unboxed(array_element_class);
#ifdef NUM_EXTRA_OBJ_HEADER_WORDS
        the_offset = sizeof(VM_Vector);
#else
#ifdef EIGHT_BYTE_ALIGN_ARRAY
        the_offset = ((vt->get_gcvt()->gc_array_element_size == 8) ? 16 : 12);
#else
        the_offset = 12;
#endif
#endif //NUM_EXTRA_OBJ_HEADER_WORDS

        vt->get_gcvt()->gc_array_first_element_offset = the_offset;
        
        if (!class_is_non_ref_array (ch)) {
            vt->get_gcvt()->gc_object_has_slots = true;
        }
    }
    if(class_is_non_ref_array(ch)) {
        assert(class_is_array(ch));
        gc_set_prop_non_ref_array(vt);
    }
    if (class_is_pinned(ch)) {
        gc_set_prop_pinned(vt);
    }
    if (!ignore_finalizers && class_is_finalizable(ch)) {
        gc_set_prop_finalizable(vt);
    }
    unsigned int size = class_get_boxed_data_size(ch);
    vt->get_gcvt()->gc_allocated_size = size;
   
    // Build the offset array.
    vt->get_gcvt()->gc_number_of_slots = 0;
    // build_slot_offset_array modifies gc_number_of_slots as a side effect
    vt->get_gcvt()->gc_ref_offset_array = build_slot_offset_array(ch, vt);

    // Treat all reference as strong now
    vt->get_gcvt()->gc_strong_ref_offset_array = vt->get_gcvt()->gc_ref_offset_array;
    vt->get_gcvt()->gc_number_of_strong_slots = vt->get_gcvt()->gc_number_of_slots;
    vt->get_gcvt()->reference_type = NOT_REFERENCE;
    
    if (vt->get_gcvt()->gc_number_of_slots) {
        vt->get_gcvt()->gc_object_has_slots = true;
    }
    // FIX FIX FIX....class_get_name() return a pointer to a static and will change on next invocation
    vt->get_gcvt()->gc_class_name = class_get_name(ch);
    assert (vt->get_gcvt()->gc_class_name);
    
    // We always fuse strings... If this aborts at some point it is most likely because
    // the string type has been changed not to include a "value"field. So look at the
    // string type and figure out if this optimization is needs to be eliminated.
    
    vt->get_gcvt()->gc_fuse_info = NULL;

    // Treat weak reference objects (descendents of java.lang.Reference) specially:
    // exclude weak referent field from the list of strong slots
    WeakReferenceType type = class_is_reference(ch);
    if (type != NOT_REFERENCE) {
        TRACE2("gc.ref.init", "preparing class " << class_get_name(ch));

        int offset = class_get_referent_offset(ch);
        vt->get_gcvt()->referent_offset = offset;
        vt->get_gcvt()->reference_type = type;

        if (0 == p_global_gc->referent_offset) {
            p_global_gc->referent_offset = offset;
            TRACE2("gc.ref.init", "referent_offset = " << offset);
        } else if (offset != p_global_gc->referent_offset) {
            // NOTE salikh 2005-06-20: we're making an assumption
            // that the referent field in all reference objects
            // is located at the same offset. That's true for java,
            // where all reference objects are descendants of
            // java.lang.ref.Reference with single referent field.
            DIE("referent offset " << offset << " of class "
                << class_get_name(ch) << " differs from "
                << " offset obtained before: " 
                << p_global_gc->referent_offset);
        }

        int gc_number_of_slots = vt->get_gcvt()->gc_number_of_slots;
        int* gc_ref_offset_array = vt->get_gcvt()->gc_ref_offset_array;

        int* gc_strong_ref_offset_array = (int*) class_alloc_via_classloader(ch, gc_number_of_slots * sizeof (unsigned int));
        assert(gc_strong_ref_offset_array);
        
        int i,j;
        for (i = 0, j = 0; i < gc_number_of_slots; i++) {
            // skip referent
            if (offset == gc_ref_offset_array[i]) continue;
            // copy all reference field offsets except weak referent
            gc_strong_ref_offset_array[j] = gc_ref_offset_array[i];
            j++;
        }

        if (j >= gc_number_of_slots) {
            DIE("Can't find referent field in the list of reference fields.\nWeak references will not work.");
        }

        // zero terminate
        gc_strong_ref_offset_array[j] = 0;

        vt->get_gcvt()->gc_strong_ref_offset_array = gc_strong_ref_offset_array;
        vt->get_gcvt()->gc_number_of_strong_slots = j;
        TRACE2("debug.gc.ref", vt->get_gcvt()->gc_class_name << ": gc_number_of_slots = " << gc_number_of_slots
                << ", gc_number_of_strong_slots = " << j);
    }

} //gc_class_prepared


Boolean gc_supports_frontier_allocation (unsigned *offset_of_current, unsigned *offset_of_limit)
{
    GC_Thread_Info *dummy = NULL;
    *offset_of_current = (unsigned)((Byte *)&dummy->tls_current_free - (Byte *)dummy);
    *offset_of_limit = (unsigned)((Byte *)&dummy->tls_current_ceiling - (Byte *)dummy);
    
    return TRUE;
}

void gc_suppress_finalizer(Managed_Object_Handle obj) {
    p_global_gc->remove_finalize_object((Partial_Reveal_Object *)obj);
} // gc_suppress_finalizer

void gc_register_finalizer(Managed_Object_Handle obj) {
    p_global_gc->add_finalize_object((Partial_Reveal_Object *)obj,true);
} // gc_register_finalizer


void gc_pin_object (Managed_Object_Handle* p_object) {
     //Managed_Object_Handle
    block_info* block = GC_BLOCK_INFO(*p_object);
    spin_lock(&block->pin_lock);
     block->pin_count++;
     assert(block->pin_count > 0);        // guard from overflow
    spin_unlock(&block->pin_lock);
}

void gc_unpin_object (Managed_Object_Handle* p_object) {
     block_info* block = GC_BLOCK_INFO(*p_object);
     spin_lock(&block->pin_lock);
     block->pin_count--;
     assert(block->pin_count >= 0);
     spin_unlock(&block->pin_lock);
}

Managed_Object_Handle gc_get_next_live_object(void *iterator) {
    Block_Store *block_store = (Block_Store*) iterator;
    assert(p_global_gc->_p_block_store == block_store);
    return (Managed_Object_Handle) block_store->get_next_live_object();
}

/**
 * Moves all finalizable objects to vm finalization queue
 */
void gc_finalize_on_exit() {
    p_global_gc->finalize_on_exit();
}


// End of code for the supported interface.
//

// *************************************************************************
//
//      ***********************************************************
//      *                                                         *
//      *    Warning                                   Warning    *
//      *           Warning                     Warning           *
//      *                  Warning       Warning                  *
//      *                         Warning                         *
//      *                  Warning       Warning                  *
//      *           Warning                     Warning           *
//      *    Warning                                   Warning    *
//      *                                                         *
//      ***********************************************************
//
// Code found below this line is provided for debugging and tuning 
// the current system. Implementors should not assume that these 
// routines will be available later today, much less tomorrow. The 
// mere thought that they might be around at the next release should 
// be dealt with by discussing if they should be moved above this line.
//
// *************************************************************************

// Uncomment the following when debugging VM live
// reference enumeration problems, to generate 
// verbose debugging information:
// #define CHECK_LIVE_REFS


// end file gc_interface.cpp
