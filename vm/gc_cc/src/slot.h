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

#ifndef __SLOT_H__
#define __SLOT_H__

#include "open/types.h"
#include "fast_list.h"

struct Partial_Reveal_VTable;
class Partial_Reveal_Object;
typedef unsigned char *Ptr;
extern Ptr heap_base;
extern size_t max_heap_size;

// Bits in object headers
#define FORWARDING_BIT 1
#define RESCAN_BIT 2
#define GC_OBJECT_MARK_BIT_MASK 0x00000080
#define MARK_BITS 3

#define HASHCODE_IS_ALLOCATED_BIT 4
#define HASHCODE_IS_SET_BIT 8
#define OBJECT_IS_PINNED_BITS (7 << 4)
#define OBJECT_IS_PINNED_INCR (1 << 4)

void init_slots();

typedef uint32 Reference;

class Slot {
    Reference *s;
    public:
        
    Slot(Reference *slot) { s = slot; }
    void* ptr() { return (void*) s; }

#ifdef POINTER64
    void write(Partial_Reveal_Object *obj) { *s = (Reference)((Ptr)obj - heap_base); }
    Partial_Reveal_Object *read() { return (Partial_Reveal_Object*)(heap_base + *s); }

    void write_raw(Reference data) { *s = data; }
    Reference read_raw() { return *s; }

    Reference addr() { return (Reference) ((Ptr)s - (Ptr)heap_base); }

#else
    Slot(Partial_Reveal_Object **root) { s = (Reference*)root; }
    void write(Partial_Reveal_Object *obj) { *s = (Reference)obj; }
    Partial_Reveal_Object *read() { return (Partial_Reveal_Object*)*s; }

    void write_raw(Reference data) { *s = data; }
    Reference read_raw() { return *s; }

    Reference addr() { return (Reference)s; }
#endif
};

#ifdef POINTER64
  extern Partial_Reveal_Object *heap_null;
  extern Ptr vtable_base;
  inline Partial_Reveal_VTable *ah_to_vtable(Allocation_Handle ah) {
      return (Partial_Reveal_VTable*) (vtable_base + (uint32)ah);
      
  }

  inline Partial_Reveal_Object *fw_to_pointer(Allocation_Handle ah) {
      assert(ah > 0 && ah < max_heap_size);
      return (Partial_Reveal_Object*) (heap_base + (uint32)ah);
  }

  inline Reference pointer_to_fw(Partial_Reveal_Object *obj) {
      return (Reference)((Ptr)obj - (Ptr) heap_base) + FORWARDING_BIT;
  }
      
  extern Slot make_direct_root(Partial_Reveal_Object **root);
#else
#define heap_null ((Partial_Reveal_Object*)0)
  inline Partial_Reveal_VTable *ah_to_vtable(Allocation_Handle ah) {
      return (Partial_Reveal_VTable*) ah;
  }

  inline Partial_Reveal_Object *fw_to_pointer(Allocation_Handle ah) {
      return (Partial_Reveal_Object*) ah;
  }

  inline Reference pointer_to_fw(Partial_Reveal_Object *obj) {
      return (Reference) obj | FORWARDING_BIT;
  }

  static inline Slot make_direct_root(Partial_Reveal_Object **root) {
      return Slot(root);
  }
#endif

void gc_add_root_set_entry_slot(Slot root);

#endif /* __SLOT_H__ */
