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
#include <vector>
#include <algorithm>
#include <open/vm_gc.h>
#include <apr_time.h>
#include <jni_types.h>
#include "gc_types.h"
#include "collect.h"
#include "slide_compact.h"
#include "root_set_cache.h"

roots_vector root_set;
fast_list<InteriorPointer,256> interior_pointers;

void gc_cache_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned) {
    assert(!is_pinned);
    assert(ref != NULL);
    assert(*ref == NULL || ((unsigned char*)*ref >= heap.base && (unsigned char*)*ref < heap.ceiling));
    root_set.push_back((Partial_Reveal_Object**)ref);
}

void gc_cache_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned)
{
    assert(!is_pinned);
    InteriorPointer ip;
    ip.obj = (Partial_Reveal_Object*) (*(unsigned char**)slot - offset);
    ip.interior_ref = (Partial_Reveal_Object**)slot;
    ip.offset = offset;
    interior_pointers.push_back(ip);
}

void gc_cache_emit_root_set() {
    for(roots_vector::iterator r = root_set.begin(); r != root_set.end(); ++r) {
        gc_add_root_set_entry((Managed_Object_Handle*)*r, false);
    }

    for(fast_list<InteriorPointer,256>::iterator ip = interior_pointers.begin();
            ip != interior_pointers.end(); ++ip) {
        gc_add_root_set_entry_interior_pointer ((void**)(*ip).interior_ref, (*ip).offset, false);
    }
}

void gc_cache_retrieve_root_set() {
    root_set.clear();
    interior_pointers.clear();
    GC_TYPE orig_gc_type = gc_type;
    gc_type = GC_CACHE;
    vm_enumerate_root_set_all_threads();
    gc_type = orig_gc_type;
    INFO2("gc.verbose", root_set.count() << " roots collected");
    INFO2("gc.verbose", interior_pointers.count() << " interior pointers collected");
}
