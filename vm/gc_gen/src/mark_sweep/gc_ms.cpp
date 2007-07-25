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

#include "../common/gc_common.h"

#ifdef ONLY_SSPACE_IN_HEAP

#include "gc_ms.h"
#include "port_sysinfo.h"

#include "../finalizer_weakref/finalizer_weakref.h"
#include "../common/compressed_ref.h"
#ifdef USE_32BITS_HASHCODE
#include "../common/hashcode.h"
#endif

static void gc_ms_get_system_info(GC_MS *gc_ms)
{
  gc_ms->_machine_page_size_bytes = (unsigned int)port_vmem_page_sizes()[0];
  gc_ms->_num_processors = port_CPUs_number();
  gc_ms->_system_alloc_unit = vm_get_system_alloc_unit();
  SPACE_ALLOC_UNIT = max(gc_ms->_system_alloc_unit, GC_BLOCK_SIZE_BYTES);
}

void gc_ms_initialize(GC_MS *gc_ms, POINTER_SIZE_INT min_heap_size, POINTER_SIZE_INT max_heap_size)
{
  assert(gc_ms);
  gc_ms_get_system_info(gc_ms);
  
  max_heap_size = round_down_to_size(max_heap_size, SPACE_ALLOC_UNIT);
  min_heap_size = round_up_to_size(min_heap_size, SPACE_ALLOC_UNIT);
  assert(max_heap_size <= max_heap_size_bytes);
  assert(max_heap_size >= min_heap_size_bytes);
  
  void *sspace_base;
  sspace_base = vm_reserve_mem(0, max_heap_size);
  sspace_initialize((GC*)gc_ms, sspace_base, max_heap_size, max_heap_size);
  
  HEAP_NULL = (POINTER_SIZE_INT)sspace_base;
  
  gc_ms->heap_start = sspace_base;
  gc_ms->heap_end = (void*)((POINTER_SIZE_INT)sspace_base + max_heap_size);
  gc_ms->reserved_heap_size = max_heap_size;
  gc_ms->committed_heap_size = max_heap_size;
  gc_ms->num_collections = 0;
  gc_ms->time_collections = 0;
}

void gc_ms_destruct(GC_MS *gc_ms)
{
  Sspace *sspace = gc_ms->sspace;
  void *sspace_start = sspace->heap_start;
  sspace_destruct(sspace);
  gc_ms->sspace = NULL;
  vm_unmap_mem(sspace_start, space_committed_size((Space*)sspace));
}

void gc_ms_reclaim_heap(GC_MS *gc)
{
  sspace_collection(gc_ms_get_sspace(gc));
  
  /* FIXME:: clear root set here to support verify */
#ifdef COMPRESS_REFERENCE
  gc_set_pool_clear(gc->metadata->gc_uncompressed_rootset_pool);
#endif
}

void gc_ms_iterate_heap(GC_MS *gc)
{
}

unsigned int gc_ms_get_processor_num(GC_MS *gc)
{ return gc->_num_processors; }

#endif // ONLY_SSPACE_IN_HEAP
