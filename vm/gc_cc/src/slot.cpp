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
 * @author Ivan Volosyuk
 */

#include "gc_types.h"
#include "collect.h"

#ifdef POINTER64

#define ROOTS_CAPACITY 64

Partial_Reveal_Object *heap_null;

struct RootArray {
    Reference *heap_chunk;
    int pos;

    RootArray() : pos(0), heap_chunk(0) {}

    void allocate() {
        pos = 0;
        heap_chunk = (Reference*) heap.roots_pos;
        heap.roots_pos += ROOTS_CAPACITY * sizeof(Reference);
        if (heap.roots_pos > heap.roots_end) {
            LDIE2("gc", 3, "not enough reserved space for roots");
        }
    }

    Slot make_slot(Partial_Reveal_Object *obj) {
        Slot slot(heap_chunk + pos);
        slot.write(obj);
        pos++;
        return slot;
    }

};

template <typename RefType>
struct Roots : private RootArray {
    Roots<RefType> *next;
    RefType roots[ROOTS_CAPACITY];
    static Roots<RefType> *base;
    static Roots<RefType> *active;

    Roots() : next(0) {}

    void update_one() {
        for(int i = 0; i < pos; i++) {
            Slot slot(heap_chunk + i);
            Partial_Reveal_Object *obj = slot.read();
            assert((Ptr)obj >= heap.base && (Ptr)obj < heap.ceiling);
            assert(((size_t)obj & (GC_OBJECT_ALIGNMENT - 1)) == 0);
            assert(obj == heap_null || obj->vtable());
            store(roots[i], obj);
        }
    }

    Partial_Reveal_Object *fetch(RefType r);
    void store(RefType r, Partial_Reveal_Object *obj);

    public:

    Slot add_root(RefType ref) {
        roots[pos] = ref;
        Partial_Reveal_Object *obj = fetch(ref);
            assert((Ptr)obj >= heap.base && (Ptr)obj < heap.ceiling);
            assert(((size_t)obj & (GC_OBJECT_ALIGNMENT - 1)) == 0);
            assert(obj == heap_null || obj->vt());
        Slot slot = make_slot(obj);

        if (pos >= ROOTS_CAPACITY) {
            if (next == 0) next = new Roots<RefType>();
            next->allocate();
            active = next;
        }
        return slot;
    }

    static void update() {
        for(Roots<RefType> *r = base; true; r = r->next) {
            r->update_one();
            if (r->pos != ROOTS_CAPACITY) return;
        }
    }

    static void clear() {
        if (base == 0) base = new Roots<RefType>();
        base->allocate();
        active = base;
    }

    static int count() {
        int res = 0;
        for(Roots<RefType> *r = base; true; r = r->next) {
            res += r->pos;
            if (r->pos != ROOTS_CAPACITY) return res;
        }
    }
};

struct InteriorPointer {
    void **slot;
    int offset;
};

typedef Roots<Partial_Reveal_Object**> DirectRoots;
typedef Roots<InteriorPointer> InteriorRoots;
typedef Roots<uint32*> CompressedRoots;

template<> DirectRoots *DirectRoots::active = 0;
template<> DirectRoots *DirectRoots::base = 0;
template<> CompressedRoots *CompressedRoots::active = 0;
template<> CompressedRoots *CompressedRoots::base = 0;
template<> InteriorRoots *InteriorRoots::active = 0;
template<> InteriorRoots *InteriorRoots::base = 0;

template<>
Partial_Reveal_Object* DirectRoots::fetch(Partial_Reveal_Object **r) {
    assert(*r != 0);
    return *r;
}
template<>
void DirectRoots::store(Partial_Reveal_Object **r, Partial_Reveal_Object *o) {
    assert(o != 0);
    assert(*r == o || o != heap_null);
    *r = o;
}
template<>
Partial_Reveal_Object* InteriorRoots::fetch(InteriorPointer p) {
    Partial_Reveal_Object *res = (Partial_Reveal_Object*) (*(Ptr*)p.slot - p.offset);
    assert(res != heap_null);
    if (res == 0) return heap_null;
    return res;
}
template<>
void InteriorRoots::store(InteriorPointer p, Partial_Reveal_Object *o) {
    assert(o != 0);
    if (o == heap_null) o = 0;
    *(Ptr*)p.slot = (Ptr)o + p.offset;
}

template<>
Partial_Reveal_Object* CompressedRoots::fetch(uint32 *cr) {
    return (Partial_Reveal_Object*) (heap.base + *cr);
}
template<>
void CompressedRoots::store(uint32 *cr, Partial_Reveal_Object *o) {
    assert(o != 0);
    *cr = (uint32)((Ptr)o - heap.base);
}

Slot make_direct_root(Partial_Reveal_Object **root) {
    return DirectRoots::active->add_root(root);
}

static inline Slot make_interior_root(void **slot, int offset) {
    InteriorPointer p;
    p.slot = slot;
    p.offset = offset;
    return InteriorRoots::active->add_root(p);
}

static inline Slot make_compressed_root(uint32 *cr) {
    return CompressedRoots::active->add_root(cr);
}

void init_slots() {
    heap.roots_start = heap.roots_pos = heap.roots_end - 1024 * 1024;
    if (heap.ceiling > heap.roots_start) heap.ceiling = heap.roots_start;
    heap_null = (Partial_Reveal_Object*) heap.base;
}

void roots_clear() {
    heap.roots_pos = heap.roots_start;
    DirectRoots::clear();
    InteriorRoots::clear();
    CompressedRoots::clear();
}

void roots_update() {
    INFO2("gc.roots", "Roots: "
            << DirectRoots::count() << " direct, "
            << InteriorRoots::count() << " interior, "
            << CompressedRoots::count() << " compressed")
    DirectRoots::update();
    InteriorRoots::update();
    CompressedRoots::update();
}

void gc_add_compressed_root_set_entry(uint32 *ref, Boolean is_pinned) {
    assert(!is_pinned);
    Slot root = make_compressed_root(ref);
    gc_add_root_set_entry_slot(root);
}


#else /* No EM64T */

struct InteriorPointer {
    Partial_Reveal_Object *obj;
    int offset;
    Partial_Reveal_Object **interior_ref;
};

fast_list<InteriorPointer,256> interior_pointers;

inline Slot make_interior_root(void **slot, int offset) {
    InteriorPointer ip;
    ip.obj = (Partial_Reveal_Object*) (*(unsigned char**)slot - offset);
    ip.interior_ref = (Partial_Reveal_Object**)slot;
    ip.offset = offset;
    InteriorPointer& ips = interior_pointers.push_back(ip);
    return Slot(&ips.obj);
}

void init_slots() {
}

void roots_clear() {
    interior_pointers.clear();
}

void roots_update() {
    fast_list<InteriorPointer,256>::iterator begin = interior_pointers.begin();
    fast_list<InteriorPointer,256>::iterator end = interior_pointers.end();

    for(fast_list<InteriorPointer,256>::iterator i = begin; i != end; ++i) {
        *(*i).interior_ref = (Partial_Reveal_Object*)((unsigned char*)(*i).obj + (*i).offset);
    }
}
#endif /* EM64T */

void gc_add_root_set_entry_slot(Slot root) {
    switch(gc_type) {
        case GC_COPY: gc_copy_add_root_set_entry(root); break;
        case GC_FORCED: gc_forced_add_root_set_entry(root); break;
        case GC_SLIDE_COMPACT: gc_slide_add_root_set_entry(root); break;
        case GC_CACHE: gc_cache_add_root_set_entry(root); break;
                      
        case GC_FULL:
        default: abort();
    }
}

void gc_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned) {
    assert(!is_pinned);
    Slot root = make_direct_root((Partial_Reveal_Object**) ref);
    gc_add_root_set_entry_slot(root);
}

void gc_add_root_set_entry_interior_pointer (void **slot, int offset, Boolean is_pinned) {
    assert(!is_pinned);
    // FIXME: em64t bug, no handling for null root
    Slot root = make_interior_root(slot, offset);
    gc_add_root_set_entry_slot(root);
}


