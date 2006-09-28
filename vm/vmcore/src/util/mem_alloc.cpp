/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

#include <assert.h>
#include <vector>

#include "environment.h"
#include "nogc.h"
#include "open/vm.h" // for the declaration of vm_get_vtable_base()
#include "mem_alloc.h"
#include "vm_stats.h"
#include "port_malloc.h"

static const unsigned default_data_pool_size = 512*1024;

static Byte   *vtable_pool_start = NULL;
static size_t  default_initial_code_pool_size = 1024*1024;

unsigned system_page_size = 0;
unsigned page_size_for_allocation = 0;
size_t   initial_code_pool_size = 0;

Pool_Descriptor* jit_code_pool = NULL;
Pool_Descriptor* vtable_data_pool = NULL;

static apr_pool_t* aux_pool;
static apr_thread_mutex_t* aux_mutex;
static apr_thread_mutex_t* vtable_mutex;
static apr_thread_mutex_t* jit_code_mutex;


// this vector is used to store ptrs of allocated memory to free it in vm_exit
static std::vector<port_vmem_t *> m_allocated_memory_ptrs;


static size_t round_up_to_page_size_multiple(size_t size, size_t page_size)
{
    return ((size + page_size - 1) / page_size) * page_size;
} //round_up_to_page_size_multiple



static void allocate_pool_storage(Pool_Descriptor *p_pool, size_t size, size_t page_size)
{
    bool is_code = p_pool->is_code;
    void *pool_storage = NULL;

    size = round_up_to_page_size_multiple(size, page_size);
    unsigned int mem_protection = PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE;
    if (is_code) {
        mem_protection |= PORT_VMEM_MODE_EXECUTE;
    }
    size_t ps = (!is_code && VM_Global_State::loader_env->use_large_pages) ? 
        PORT_VMEM_PAGESIZE_LARGE : PORT_VMEM_PAGESIZE_DEFAULT;
    
    VERIFY(APR_SUCCESS == apr_thread_mutex_lock(aux_mutex), \
        "Cannot lock the pool reallocation mutex");
    apr_status_t status = port_vmem_reserve(&p_pool->descriptor, &pool_storage, 
        size, mem_protection, ps, aux_pool);
    VERIFY(APR_SUCCESS == apr_thread_mutex_unlock(aux_mutex), \
        "Cannot unlock the pool reallocation mutex");
    if (APR_SUCCESS == status) {
        status = port_vmem_commit(&pool_storage, size, p_pool->descriptor);
    }
    
    if (status != APR_SUCCESS || pool_storage == NULL)  {
        DIE("Cannot allocate pool storage: " << (void *)size 
            << " bytes of virtual memory for code or data.\n"
            "Error code = " << status);
    }

#ifdef VM_STATS
    p_pool->num_pool_allocations++;
    p_pool->total_pool_size += size;
    if (is_code) {
        VM_Statistics::get_vm_stats().codemgr_total_code_pool_size += size;
    } else {
        VM_Statistics::get_vm_stats().codemgr_total_data_pool_size += size;
    }
#endif //VM_STATS

    p_pool->start  = (Byte*)pool_storage;
    p_pool->end = ((Byte*)(pool_storage) + size);
    m_allocated_memory_ptrs.push_back(p_pool->descriptor);
} //allocate_pool_storage



static void init_pool(Pool_Descriptor *p_pool, size_t page_size, size_t init_size, bool is_code)
{
    p_pool->default_size             = (size_t)(round_up_to_page_size_multiple(init_size, page_size) + 0.5);
    p_pool->is_code                  = is_code;
#ifdef VM_STATS
    p_pool->num_allocations          = 0;
    p_pool->num_pool_allocations     = 0;
    p_pool->total_pool_size          = 0;
    p_pool->total_size_allocated     = 0;
    p_pool->num_resizes              = 0;
    p_pool->current_alloc_size       = p_pool->default_size;
#endif //VM_STATS
} //init_pool



static void init_pools(size_t page_size)
{
    jit_code_pool = new Pool_Descriptor;
    init_pool(jit_code_pool, page_size, initial_code_pool_size, /*is_code*/ true);
    allocate_pool_storage(jit_code_pool, jit_code_pool->default_size, page_size);

    vtable_data_pool = new Pool_Descriptor;
    // 20040511: The vtable pool must be bigger for jAppServer for compresses vtable pointers (can't be resized)
    unsigned size = (vm_vtable_pointers_are_compressed() ? 8*1024*1024 : default_data_pool_size);
    init_pool(vtable_data_pool, page_size, size, /*is_code*/ false);
    allocate_pool_storage(vtable_data_pool, vtable_data_pool->default_size, page_size);
} //init_pools



static void *allocate_from_pool(Pool_Descriptor *p_pool, size_t size, size_t alignment, size_t page_size,
                                bool is_resize_allowed, Code_Allocation_Action action)
{
    // Make sure alignment is a power of 2.
    assert((alignment & (alignment-1)) == 0);
    size_t mask = alignment - 1;

    // align the requested size
    size = (size + mask) & ~mask;

    Byte *pool_start = p_pool->start;   // (misnamed) this actually points to the next free byte in the pool
    pool_start = (Byte *) ((POINTER_SIZE_INT)(pool_start + mask) & ~(POINTER_SIZE_INT)mask);
    Byte *pool_end = p_pool->end;

    size_t mem_left_in_pool = (pool_end - pool_start);
    if (size > mem_left_in_pool) {
        if (action == CAA_Simulate) {
            // Return NULL if we're simulating the allocation and it would have caused a pool resize.
            return NULL;
        }
        if (!is_resize_allowed && pool_start != NULL) {
            DIE("Error: Resizing of the memory pool is not allowed.\n");
        }
        assert(p_pool->default_size);
        size_t new_pool_size = ((size > p_pool->default_size)? size : p_pool->default_size);
        new_pool_size += mask;
#ifdef VM_STATS
        p_pool->num_resizes++;
        p_pool->current_alloc_size = new_pool_size;
#endif //VM_STATS
        allocate_pool_storage(p_pool, new_pool_size, page_size);
        pool_start = p_pool->start;
        pool_start = (Byte *) ((POINTER_SIZE_INT)(pool_start + mask) & ~(POINTER_SIZE_INT)mask);
        pool_end = p_pool->end;
    }
    void *p = pool_start;
    if (action != CAA_Simulate) {
        // Don't update the pool start pointer if we're only simulating allocation.
        p_pool->start = pool_start + size;
    }
#ifdef VM_STATS
    p_pool->num_allocations++;
    p_pool->total_size_allocated += size;
    if (p_pool->is_code) {
        VM_Statistics::get_vm_stats().codemgr_total_code_allocated += size;
    } else {
        VM_Statistics::get_vm_stats().codemgr_total_data_allocated += size;
    }
#endif //VM_STATS
    return p;
} //allocate_from_pool


//////////////////////////////////////////////////////////////////////////////////////////////
// Beginning of publicly exported functions.
//////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// begin allocating memory for code

void *malloc_fixed_code_for_jit(size_t size, size_t alignment, unsigned heat, Code_Allocation_Action action)
{
    vm_init_mem_alloc();
    assert (jit_code_pool);
    VERIFY(APR_SUCCESS == apr_thread_mutex_lock(jit_code_mutex), \
        "Cannot lock the jit mutex");
    void *p = allocate_from_pool(jit_code_pool, size, alignment, page_size_for_allocation, /*is_resize_allowed*/ true, action);
    VERIFY(APR_SUCCESS == apr_thread_mutex_unlock(jit_code_mutex), \
        "Cannot unlock the jit mutex");
    return p;
} //malloc_fixed_code_for_jit


// end allocating memory for code
////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////
//
// begin memory allocation for class-related data structures such as class statics and vtables.

void *allocate_vtable_data_from_pool(size_t size)
{
    bool is_resize_allowed = true;
    if (vm_vtable_pointers_are_compressed()) {
        is_resize_allowed = false;
    }
    assert (vtable_data_pool);
    VERIFY(APR_SUCCESS == apr_thread_mutex_lock(vtable_mutex), \
        "Cannot lock the vtable mutex");
    void *p = allocate_from_pool(vtable_data_pool, size, 16, page_size_for_allocation, is_resize_allowed, CAA_Allocate);
    VERIFY(APR_SUCCESS == apr_thread_mutex_unlock(vtable_mutex), \
        "Cannot unlock the vtable mutex");
    return p;
} //allocate_class_data_from_area

// end allocating memory for data
//
////////////////////////////////////////////////////////////////////////////////////////////////


void vm_init_mem_alloc()
{
    static int initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    VERIFY(APR_SUCCESS == apr_pool_create(&aux_pool, 0), \
        "Cannot initialize a memory pool");
    VERIFY(APR_SUCCESS == apr_thread_mutex_create(&aux_mutex, APR_THREAD_MUTEX_NESTED, aux_pool), \
        "Cannot initialize pool reallocation mutex");
    VERIFY(APR_SUCCESS == apr_thread_mutex_create(&jit_code_mutex, APR_THREAD_MUTEX_NESTED, aux_pool), \
        "Cannot initialize jit table mutex");
    VERIFY(APR_SUCCESS == apr_thread_mutex_create(&vtable_mutex, APR_THREAD_MUTEX_NESTED, aux_pool), \
        "Cannot initialize vtable mutex");

    size_t *ps = port_vmem_page_sizes();
    if (ps[1] != 0 && VM_Global_State::loader_env->use_large_pages) {
        page_size_for_allocation = ps[1];
    }
    else {
        page_size_for_allocation = ps[0];
    }

    default_initial_code_pool_size = round_up_to_page_size_multiple(default_initial_code_pool_size, page_size_for_allocation);
    initial_code_pool_size = default_initial_code_pool_size;
    assert(initial_code_pool_size);

#ifdef VM_STATS
    VM_Statistics::get_vm_stats().codemgr_total_code_pool_size = 0;
    VM_Statistics::get_vm_stats().codemgr_total_code_allocated = 0;
    VM_Statistics::get_vm_stats().codemgr_total_data_pool_size = 0;
    VM_Statistics::get_vm_stats().codemgr_total_data_allocated = 0;
#endif //VM_STATS

    init_pools(page_size_for_allocation);
    vtable_pool_start = vtable_data_pool->start;
} //vm_init_mem_alloc


void vm_mem_dealloc()
{
    delete vtable_data_pool;
    vtable_data_pool = NULL;
    delete jit_code_pool;
    jit_code_pool = NULL;
    std::vector<port_vmem_t *>::iterator it;
    for (it = m_allocated_memory_ptrs.begin(); it != m_allocated_memory_ptrs.end(); it++)
    {
        port_vmem_release(*it);
    }
    VERIFY(APR_SUCCESS == apr_thread_mutex_destroy(aux_mutex), \
        "Cannot destroy the mutex");
    VERIFY(APR_SUCCESS == apr_thread_mutex_destroy(jit_code_mutex), \
        "Cannot destroy the mutex");
    VERIFY(APR_SUCCESS == apr_thread_mutex_destroy(vtable_mutex), \
        "Cannot destroy the mutex");
    apr_pool_destroy(aux_pool);
}


POINTER_SIZE_INT vm_get_vtable_base()
{
    Byte *base = vtable_pool_start;
    assert (base);
    // Subtract a small number (like 1) from the real base so that
    // no valid vtable offsets will ever be 0.
    return (POINTER_SIZE_INT) (base - 8);
} //vm_get_vtable_base
