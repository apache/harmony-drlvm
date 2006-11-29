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

#ifndef _gc_header_H_
#define _gc_header_H_

#include <assert.h>
#include "platform_lowlevel.h"
#include <apr_atomic.h>

#include "open/types.h"
#include "open/vm_gc.h"
#include "open/vm.h"
#include "gc_cout.h"
#include "open/hythread_ext.h"

#include "hash_table.h"

// Define USE_COMPRESSED_VTABLE_POINTERS here to enable compressed vtable
// pointers within objects.
#if defined _IPF_ || defined _EM64T_
#define USE_COMPRESSED_VTABLE_POINTERS
#endif // _IPF_

//
// Use the contention bit of object header to lock stuff for now... Is this OKAY???
//
#define GC_OBJECT_MARK_BIT_MASK 0x00000080
#define FORWARDING_BIT_MASK 0x1


//
// This is thread local per Java/VM thread and is used by the GC to stuff in whatever it needs.
// Current free is the free point in the curr_alloc_block. current_ceiling is the ceiling of that area.
// This will be the first two works in this structure for IA32 but might be moved into reserved registers 
// for IPF.
//

typedef void *Thread_Handle; // Used to communicate with VM about threads.

#ifdef USE_COMPRESSED_VTABLE_POINTERS
typedef uint32 Obj_Info_Type;
#else // !USE_COMPRESSED_VTABLE_POINTERS
typedef POINTER_SIZE_INT Obj_Info_Type;
#endif // !USE_COMPRESSED_VTABLE_POINTERS

typedef struct Partial_Reveal_Object {
#ifdef USE_COMPRESSED_VTABLE_POINTERS
private:
    uint32 vt_offset;
    Obj_Info_Type obj_info;
public:

    Obj_Info_Type get_obj_info() { return obj_info; }
    void set_obj_info(Obj_Info_Type new_obj_info) { obj_info = new_obj_info; }
    Obj_Info_Type * obj_info_addr() { return &obj_info; }

    struct Partial_Reveal_VTable *vtraw() { 
        if (0 == vt_offset) 
            return NULL;
        else
            return (struct Partial_Reveal_VTable *) (vt_offset + vtable_base); 
    }
    struct Partial_Reveal_VTable *vt() { assert(vt_offset); return (struct Partial_Reveal_VTable *) (vt_offset + vtable_base); }
    void set_vtable(Allocation_Handle ah)
    { 
        // vtables are allocated from a fixed-size pool in the VM
        // see the details in mem_alloc.cpp, grep for vtable_data_pool.
        vt_offset = (uint32)ah;
    }

private:
    // This function returns the number of bits that an address may be
    // right-shifted after subtracting the heap base, to compress it to
    // 32 bits.  In the current implementation, objects are aligned at
    // 8-bit(??? you mean 8 byte right??) boundaries, leaving 3 lower bits of the address that are
    // guaranteed to be 0.  However, one bit must be reserved to mark
    // the value as a forwarding pointer.
    static unsigned forwarding_pointer_compression_shift() {
        return 2;
    }
public:

    struct Partial_Reveal_Object *get_forwarding_pointer() {
        assert(obj_info & FORWARDING_BIT_MASK);
        POINTER_SIZE_INT offset = ((POINTER_SIZE_INT)(obj_info & ~FORWARDING_BIT_MASK)) << forwarding_pointer_compression_shift();
        return (struct Partial_Reveal_Object *) (heap_base + offset);
    }
    
    void set_forwarding_pointer(void *dest) {
        assert((POINTER_SIZE_INT)dest % (1<<forwarding_pointer_compression_shift()) == 0);
        obj_info = (Obj_Info_Type)(((POINTER_SIZE_INT)dest - heap_base) >> forwarding_pointer_compression_shift()) | FORWARDING_BIT_MASK;
    }
    
    Obj_Info_Type compare_exchange(Obj_Info_Type new_value, Obj_Info_Type old_value) {
        return (Obj_Info_Type) apr_atomic_cas32(&obj_info, new_value, old_value);
    }

    bool set_forwarding_pointer_atomic(void *dest) {
        Obj_Info_Type old_val = obj_info;
        Obj_Info_Type new_val = (Obj_Info_Type)(((POINTER_SIZE_INT)dest - heap_base) >> forwarding_pointer_compression_shift()) | FORWARDING_BIT_MASK;
        if ((old_val & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK) {
            return false; // Somebody else grabbed the forwarding pointer.
        }
        Obj_Info_Type returned_val = compare_exchange(new_val, old_val);
        return (returned_val == old_val); // if compare exchange fails return false otherwise true.
    }
    static bool use_compressed_vtable_pointers() { return true; }
    static unsigned object_overhead_bytes() { return sizeof(uint32) + sizeof(Obj_Info_Type); }
    static unsigned vtable_bytes() { return sizeof(uint32); }
    static struct Partial_Reveal_VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return (Partial_Reveal_VTable *) ((POINTER_SIZE_INT)ah + vtable_base);
    }
    static uint64 max_supported_heap_size() { return (0x100000000) << forwarding_pointer_compression_shift(); }
#else // !USE_COMPRESSED_VTABLE_POINTERS
private:
    struct Partial_Reveal_VTable *vt_raw;
    Obj_Info_Type obj_info;

public:
    Obj_Info_Type get_obj_info() { return obj_info; }
    void set_obj_info(Obj_Info_Type new_obj_info) { obj_info = new_obj_info; }
    Obj_Info_Type * obj_info_addr() { return &obj_info; }

    struct Partial_Reveal_VTable *vtraw() { return vt_raw; }
    struct Partial_Reveal_VTable *vt() { ASSERT(vt_raw, "incorrect object at " << this); return vt_raw; }
    void set_vtable(Allocation_Handle ah) { vt_raw = (struct Partial_Reveal_VTable *)ah; }

    struct Partial_Reveal_Object *get_forwarding_pointer() {
        assert(get_obj_info() & FORWARDING_BIT_MASK);
        return (struct Partial_Reveal_Object *) (get_obj_info() & ~FORWARDING_BIT_MASK);
    }
    void set_forwarding_pointer(void *dest) {
        set_obj_info((Obj_Info_Type)dest | FORWARDING_BIT_MASK);
    }
    Obj_Info_Type compare_exchange(Obj_Info_Type new_value, Obj_Info_Type old_value) {
        return (Obj_Info_Type) apr_atomic_casptr(
            (volatile void **)obj_info_addr(), (void *) new_value, (void *) old_value); 
    }
     bool set_forwarding_pointer_atomic(void *dest) {
        Obj_Info_Type old_val = get_obj_info();
        Obj_Info_Type new_val = (Obj_Info_Type)dest | FORWARDING_BIT_MASK;
        if ((old_val & FORWARDING_BIT_MASK) == FORWARDING_BIT_MASK) {
            return false; // Somebody else grabbed the forwarding pointer.
        }
        Obj_Info_Type returned_val = compare_exchange(new_val, old_val);
        return (returned_val == old_val); // if compare exchange fails return false otherwise true.
    }

    static bool use_compressed_vtable_pointers() { return false; }
    static unsigned object_overhead_bytes() { return sizeof(struct Partial_Reveal_VTable *) + sizeof(Obj_Info_Type); }
    static unsigned vtable_bytes() { return sizeof(struct Partial_Reveal_VTable *); }
    static struct Partial_Reveal_VTable *allocation_handle_to_vtable(Allocation_Handle ah) {
        return (Partial_Reveal_VTable *) ah;
    }
    static uint64 max_supported_heap_size() { return ~((uint64)0); }
#endif // !USE_COMPRESSED_VTABLE_POINTERS
    
    static POINTER_SIZE_INT vtable_base;
    static POINTER_SIZE_INT heap_base;

public:
    inline const char* class_name();
    inline bool has_slots();
} Partial_Reveal_Object;

// This is thread local per Java/VM thread and is used by the GC to stuff in whatever it needs.
typedef struct GC_Thread_Info {
    void *tls_current_free;
    void *tls_current_ceiling;
    void *chunk;
    void *curr_alloc_block;
    Thread_Handle thread_handle;   // This thread;
    Thread_Handle p_active_thread; // The next active thread. same as this->p_active_gc_thread_info->thread_handle
    GC_Thread_Info *p_active_gc_thread_info; // The gc info area associated with the next active thread.
} GC_Thread_Info;


struct GC_VTable_Info {
    unsigned int gc_object_has_slots;
    unsigned int gc_number_of_slots;
    unsigned int gc_number_of_strong_slots;
    
    // If this object type demonstrates temporal locality (is fused somehow) with some 
    // other object type then this is non-zero and is the offset in this object of the offset
    // that that should be used to reach the fused object.

    // if gc_fuse_info == NULL: don't fuse
    // else
    //    it stores fused field offset array, end with 0
    int *gc_fuse_info;
    unsigned int delinquent_type_id;

    uint32 gc_class_properties;    // This is the same as class_properties in VM's VTable.

    // Offset from the top by CLASS_ALLOCATED_SIZE_OFFSET
    // The number of bytes allocated for this object. It is the same as
    // instance_data_size with the constraint bit cleared. This includes
    // the OBJECT_HEADER_SIZE as well as the OBJECT_VTABLE_POINTER_SIZE
    unsigned int gc_allocated_size;

    unsigned int gc_array_element_size;

    // This is the offset from the start of the object to the first element in the
    // array. It isn't a constant since we pad double words.
    int gc_array_first_element_offset;
    
    // The GC needs access to the class name for debugging and for collecting information
    // about the allocation behavior of certain classes. Store the name of the class here.
    const char *gc_class_name;
    Class_Handle gc_clss;
    
    // This array holds an array of offsets to the pointer fields in 
    // an instance of this class, including the weak referent field.
    // It would be nice if this
    // was located immediately prior to the vtable, since that would 
    // eliminate a dereference.
    int *gc_ref_offset_array;

    // This array holds an array of offsets to the pointer fields in
    // an instance of this class, excluding weak referent field.
    int* gc_strong_ref_offset_array;

    /// the offset of weak referent field, or 0 if the class is not reference
    int referent_offset;

    /// referent field type, if referent is not 0
    WeakReferenceType reference_type;
};

typedef struct Partial_Reveal_VTable {
private:
public:
    struct GC_VTable_Info *gcvt;
    struct GC_VTable_Info *get_gcvt() { return gcvt; }
    void set_gcvt(struct GC_VTable_Info *new_gcvt) { gcvt = new_gcvt; }
    struct GC_VTable_Info **gcvt_address(void) { return &gcvt; }
} Partial_Reveal_VTable;

inline const char* Partial_Reveal_Object::class_name() {
    return vt()->get_gcvt()->gc_class_name;
}

inline bool Partial_Reveal_Object::has_slots() {
    return vt()->get_gcvt()->gc_object_has_slots;
}


// This is what is held in the mark field for fixed objects and card table for moveable objects.
typedef bool MARK;

// The GC heap is divided into blocks that are sized based on several constraints.
// They must be small enough to allow unused blocks not to be a big problem.
// They must be large enough so that we are avoid spending a lot of time requesting additional
// blocks.
// They must be small enough to allow them to be scanned without a lot of problem.
// They must be aligned so that clearing lower bits in any address in the block will get 
// the start of the block.
// There are several tables that must be held in the first page (hardware specific, for IA32 4096 bytes)
// These table have a single entry for every page in the block. This limits the size of the block.
// 


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TUNABLE (heres for lazy people!)
// 16 for 64K
// 18 for 256K
// 19 for 512K
// 20 for 1MB
// 22 for 4MB
#define GC_BLOCK_SHIFT_COUNT 17


// TUNABLE...each thread gets this many blocks per chunk during each request....512K per chunk,
// the actual number of blocks per thread decreases as the number of threads increases.
#define GC_MAX_BLOCKS_PER_CHUNK 4


// FIXED
// The information about the block is held in the first (4096 byte) card (page) of the block.
#define GC_BYTES_PER_MARK_BYTE 32
// FIXED
#define GC_MARK_SHIFT_COUNT 4



#define GC_BLOCK_SIZE_BYTES (1 << GC_BLOCK_SHIFT_COUNT)
// Make this 4K if block is 64K
#define GC_BLOCK_INFO_SIZE_BYTES (GC_BLOCK_SIZE_BYTES / GC_BYTES_PER_MARK_BYTE)


// 2K is the smallest allocation size we will identify after a collection if block is 64K

#define GC_MIN_FREE_AREA_SIZE (GC_BLOCK_INFO_SIZE_BYTES / 2)

#define GC_MAX_FREE_AREAS_PER_BLOCK ((GC_BLOCK_SIZE_BYTES - GC_BLOCK_INFO_SIZE_BYTES) / GC_MIN_FREE_AREA_SIZE)

//Number of blocks for the large object
#define GC_NUM_BLOCKS_PER_LARGE_OBJECT(size) (((size) + GC_BLOCK_INFO_SIZE_BYTES+ GC_BLOCK_SIZE_BYTES - 1) / GC_BLOCK_SIZE_BYTES)

// Number of marks needed for the block_info bytes...we dont need this!
#define GC_NUM_MARK_BYTES_FOR_BLOCK_INFO_SIZE_BYTES (GC_BLOCK_INFO_SIZE_BYTES / GC_BYTES_PER_MARK_BYTE)


//////////////////////////////////////////////////////////////////////////////

#define GC_NUM_BITS_PER_BYTE 8

#ifndef GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK

#ifdef _IPF_xxx
// IPF advisory --> turn on GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK for your first pass at getting compaction functionality up....
// there are assembly code etc..otherwise ..Sree
//.......// need to fill in....
#define GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
#else
//!!!!!!!!!!!!
// On IA32 VM, objects are at least 8 bytes (vtable + header)....but an object can start at any 4-byte aligned address
#define GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES 4 //sizeof(void *)
// This just needs to be 60K/4....now it is 64K/4....we can fix this easily later...(16K cards per block)
// for now i will just throw in asserts against use of first 1/16th of bit string
#define GC_LIVE_OBJECT_CARD_SIZE_BITS 2 ///XXXXXXXXX
#endif // _IPF_
#define GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK (GC_BLOCK_SIZE_BYTES / GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES)
#define GC_LIVE_OBJECT_CARD_INDEX_INTO_GC_BLOCK(P_OBJ) ((((POINTER_SIZE_INT)P_OBJ) & GC_BLOCK_LOW_MASK) >> GC_LIVE_OBJECT_CARD_SIZE_BITS)

#endif // GC_LIVE_OBJECT_LISTS_PER_COMPACTION_BLOCK
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////

// NEW DEFINES!!!!
#define GC_NUM_LIVE_OBJECT_CARDS_IN_GC_BLOCK_INFO (GC_BLOCK_INFO_SIZE_BYTES / GC_LIVE_OBJECT_CARD_SIZE_IN_BYTES)
#define GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK_DATA (GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK - GC_NUM_LIVE_OBJECT_CARDS_IN_GC_BLOCK_INFO)

#define GC_SIZEOF_MARK_BIT_VECTOR_IN_BYTES (GC_NUM_LIVE_OBJECT_CARDS_PER_GC_BLOCK_DATA / GC_NUM_BITS_PER_BYTE)

#define GC_LIVE_OBJECT_CARD_INDEX_INTO_GC_BLOCK_NEW(P_OBJ) (((((POINTER_SIZE_INT)P_OBJ) & GC_BLOCK_LOW_MASK) >> GC_LIVE_OBJECT_CARD_SIZE_BITS) - GC_NUM_LIVE_OBJECT_CARDS_IN_GC_BLOCK_INFO)

//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
#define GC_FRACTION_OF_HEAP_INCREMENTAL_COMPACTED_DURING_EACH_GC 16
#define GC_MIN_COMPACTION_INCREMENT_SIZE 16

extern unsigned int gc_heap_compaction_ratio;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum gc_compaction_type {
    gc_no_compaction,
    gc_full_heap_sliding_compaction,
    gc_incremental_sliding_compaction,
    gc_bogus_compaction
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _IPF_
// Assuming 64K blocks, make room for up to 32 Gig heaps which is about the max real memory available on our machines.
#define GC_MAX_BLOCKS 512 * 1024        
#else
// Even at 64K per block, for 2GB heap we will need max of (2^31/2^16 = 2^15 = 32K blocks in all)
#define GC_MAX_BLOCKS 32 * 1024     
#endif // _IPF_

// If an objects spans at least half the size of the block, I will move it to a separate block for itself
#define GC_MAX_CHUNK_BLOCK_OBJECT_SIZE (GC_BLOCK_ALLOC_SIZE / 2)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef POINTER64
#define GC_CARD_SHIFT_COUNT 13
#else
#define GC_CARD_SHIFT_COUNT 12
#endif


// Mask of 1's that when ANDed in will give the offset into the block
#define GC_BLOCK_LOW_MASK ((POINTER_SIZE_INT)(GC_BLOCK_SIZE_BYTES - 1))
// Mask that gives the base of the block.
#define GC_BLOCK_HIGH_MASK (~GC_BLOCK_LOW_MASK)
// Used to go from an object reference to the block information.
#define GC_BLOCK_INFO(ADDR) ((block_info *)((POINTER_SIZE_INT)ADDR & GC_BLOCK_HIGH_MASK))
// The start of the allocation area for this block, page 0 gets block info page 1 gets objects
#define GC_BLOCK_ALLOC_START(BLOCK_ADDR) ( (void *)((POINTER_SIZE_INT)BLOCK_ADDR + GC_BLOCK_INFO_SIZE_BYTES) )
// The maximum size of an object that can be allocated in this block.
#define GC_BLOCK_ALLOC_SIZE (GC_BLOCK_SIZE_BYTES - GC_BLOCK_INFO_SIZE_BYTES)

#define GC_BLOCK_CEILING(P_OBJ) ( ((char *)(GC_BLOCK_INFO(P_OBJ)) + GC_BLOCK_SIZE_BYTES) - 1 )

  
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct block_info;  // Forward ref

enum nursery_state {
    free_uncleared_nursery = 0,
    thread_clearing_nursery,
    free_nursery,
    active_nursery,
    spent_nursery,
    bogus_nursery
};


typedef struct {
    void *area_base;
    void *area_ceiling;
    unsigned int area_size;
    bool has_been_zeroed;
} free_area;



unsigned int get_object_size_bytes(Partial_Reveal_Object *);

//
// block_info - This is what resides at the start of each block holding the start of an object.
//              This is all blocks except for sequential blocks holding objects that will not
//              fit into a single block.
// 

struct block_info {

    // true if the block is in a nursery.
    bool in_nursery_p; 

    // true if the block is in large object (fixed) space.
    bool in_los_p;     

    // true if block contains only one object (large)
    bool is_single_object_block;        

    // Performance optimization --> to have fast access to block store info table. store it here when block is handed out
    unsigned int block_store_info_index;
    
    // At the end of each GC calculate the number of free bytes in this block and store it here.
    // This is only accurate at the end of each GC.

    unsigned int block_free_bytes;

    unsigned int block_used_bytes;

    nursery_state nursery_status; 

    // if this is a SOB, what is the size of the resident object?
    POINTER_SIZE_INT los_object_size;               

    // has it been swept ...could happen during GC or otherwise
    bool block_has_been_swept;

    //Will be turned on by the GC if this block needs to be sliding compacted.
    bool is_compaction_block;

    // Set to true once this block has been evacuated of all of its "from" objects.
    bool from_block_has_been_evacuated;

    // This block has objects slid into it so it should not be swept.
    bool is_to_block;

    // The free lists in this block...GC_MAX_FREE_AREAS_PER_BLOCK
    free_area *block_free_areas;

    // Number of allocation areas in this block. This can be 0 if there are not free areas in this block.
    int num_free_areas_in_block;

    // The current allocation area. If this is -1 then area 0 will be the next area to use...
    int current_alloc_area;

    // The next free block that will be used by the thread once all areas in this block are exhausted.
    block_info *next_free_block;

    //
    // Not used delete with new tls allocation routines.
    //
    // The frontier allocation pointer in the current allocation area within this block
    void *curr_free;

    // The ceiling/limit pointer in the current allocation area within this block
    void *curr_ceiling; // for allocation is faster when we use this

    /// Pin count. Pin operation increases count, unpin decreases.
    int pin_count;

    /// pin lock. spin lock on this variable protects access to pin_count.
    volatile int pin_lock;

    // Used to mark and keep track of live objects resident in this block for heap tracing activity
    uint8 mark_bit_vector[GC_SIZEOF_MARK_BIT_VECTOR_IN_BYTES];

///////////////////////////////////////////////////////////////////////////////
        
    POINTER_SIZE_INT current_object_offset;
    char *current_block_base;

    char *use_to_get_last_used_word_in_header;
    char *unused;

};

struct obj_lock {
    Partial_Reveal_Object *p_obj;
    Obj_Info_Type obj_header;
    struct obj_lock *p_next;
};

typedef struct obj_lock object_lock_save_info;




static inline bool
is_array(Partial_Reveal_Object *p_obj)
{
    if (p_obj->vt()->get_gcvt()->gc_class_properties & CL_PROP_ARRAY_MASK) {
        return true;
    } else {
        return false;
    }
}


static inline bool
is_array_of_primitives(Partial_Reveal_Object *p_obj)
{
    if (p_obj->vt()->get_gcvt()->gc_class_properties & CL_PROP_NON_REF_ARRAY_MASK) {
        return true;
    } else {
        return false;
    }
}

/// obtains a spinlock.
inline void spin_lock(volatile int* lock) {
    int UNUSED lock_value = *lock;
    assert(0 == lock_value || 1 == lock_value);
    assert(sizeof(int) == 4);
    int count = 10000;  // is this reasonable?
    int r = apr_atomic_cas32((volatile uint32 *)lock, 1, 0);
    // spin actively for a certain number of cycles
    while (1 == r && --count > 0) {
        r = apr_atomic_cas32((volatile uint32 *)lock, 1, 0);
    }
    if (1 == r) {
        // we still haven't got the lock
        // spin & yield until we grabbed the lock
        while (apr_atomic_cas32((volatile uint32 *)lock, 1, 0) == 1) {
            hythread_yield();
        }
    }
}

/// releases a spinlock.
inline void spin_unlock(volatile int* lock) {
    assert(1 == *lock);
    *lock = 0;
}

#endif // _gc_header_H_
