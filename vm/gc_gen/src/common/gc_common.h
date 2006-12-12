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
 * @author Xiao-Feng Li, 2006/10/05
 */

#ifndef _GC_COMMON_H_
#define _GC_COMMON_H_

#include <assert.h>
#include <map>

#include "port_vmem.h"

#include "platform_lowlevel.h"

#include "open/types.h"
#include "open/vm_gc.h"
#include "open/vm.h"
#include "open/gc.h"
#include "port_malloc.h"

#include "gc_for_class.h"
#include "gc_platform.h"

#define null 0

#define MB  1048576
#define KB  1024

#define BYTES_PER_WORD 4
#define BITS_PER_BYTE 8 
#define BITS_PER_WORD 32

#define MASK_OF_BYTES_PER_WORD (BYTES_PER_WORD-1) /* 0x11 */
#define WORD_SIZE_ROUND_UP(addr)  (((unsigned int)addr+MASK_OF_BYTES_PER_WORD)& ~MASK_OF_BYTES_PER_WORD) 

#define BIT_SHIFT_TO_BYTES_PER_WORD 2 /* 2 */
#define BIT_SHIFT_TO_BITS_PER_BYTE 3
#define BIT_SHIFT_TO_BITS_PER_WORD 5
#define BIT_SHIFT_TO_KILO 10 

#define BIT_MASK_TO_BITS_PER_WORD ((1<<BIT_SHIFT_TO_BITS_PER_WORD)-1)

#define GC_OBJ_SIZE_THRESHOLD (4*KB)

typedef void (*TaskType)(void*);

typedef std::map<Partial_Reveal_Object*, Obj_Info_Type> ObjectMap;

enum Collection_Kind {
  MINOR_COLLECTION,
  MAJOR_COLLECTION  
};

enum GC_CAUSE{
  GC_CAUSE_NIL,
  GC_CAUSE_NOS_IS_FULL,
  GC_CAUSE_LOS_IS_FULL,
  GC_CAUSE_RUNTIME_FORCE_GC
};

inline unsigned int vm_object_size(Partial_Reveal_Object *obj)
{
  Boolean arrayp = object_is_array (obj);
  if (arrayp) {
    return vm_vector_size(obj_get_class_handle(obj), vector_get_length((Vector_Handle)obj));
  } else {
    return nonarray_object_size(obj);
  }
}

inline POINTER_SIZE_INT round_up_to_size(POINTER_SIZE_INT size, int block_size) 
{  return (size + block_size - 1) & ~(block_size - 1); }

inline POINTER_SIZE_INT round_down_to_size(POINTER_SIZE_INT size, int block_size) 
{  return size & ~(block_size - 1); }

inline Boolean obj_is_in_gc_heap(Partial_Reveal_Object *p_obj)
{
  return p_obj >= gc_heap_base_address() && p_obj < gc_heap_ceiling_address();
}

/* Return a pointer to the ref field offset array. */
inline int *init_object_scanner (Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  return gcvt->gc_ref_offset_array;
}

inline void *offset_get_ref(int *offset, Partial_Reveal_Object *obj) 
{    return (*offset == 0)? NULL: (void*)((Byte*) obj + *offset); }

inline int *offset_next_ref (int *offset) 
{  return (int *)((Byte *)offset + sizeof (int)); }

Boolean obj_is_forwarded_in_vt(Partial_Reveal_Object *obj);
inline Boolean obj_is_marked_in_vt(Partial_Reveal_Object *obj) 
{  return ((POINTER_SIZE_INT)obj->vt_raw & MARK_BIT_MASK); }

inline void obj_mark_in_vt(Partial_Reveal_Object *obj) 
{  obj->vt_raw = (Partial_Reveal_VTable *)((POINTER_SIZE_INT)obj->vt_raw | MARK_BIT_MASK);
   assert(!obj_is_forwarded_in_vt(obj));
}

inline void obj_unmark_in_vt(Partial_Reveal_Object *obj) 
{
  assert(!obj_is_forwarded_in_vt(obj));
  assert(obj_is_marked_in_vt(obj)); 
  obj->vt_raw = (Partial_Reveal_VTable *)((POINTER_SIZE_INT)obj->vt_raw & ~MARK_BIT_MASK);
}

inline void obj_set_forward_in_vt(Partial_Reveal_Object *obj) 
{
  assert(!obj_is_marked_in_vt(obj));
  obj->vt_raw = (Partial_Reveal_VTable *)((POINTER_SIZE_INT)obj->vt_raw | FORWARDING_BIT_MASK);
}

inline Boolean obj_is_forwarded_in_vt(Partial_Reveal_Object *obj) 
{  return (POINTER_SIZE_INT)obj->vt_raw & FORWARDING_BIT_MASK; }

inline void obj_clear_forward_in_vt(Partial_Reveal_Object *obj) 
{
  assert(obj_is_forwarded_in_vt(obj) && !obj_is_marked_in_vt(obj));  
  obj->vt_raw = (Partial_Reveal_VTable *)((POINTER_SIZE_INT)obj->vt_raw & ~FORWARDING_BIT_MASK);
}

inline void obj_set_forwarding_pointer_in_vt(Partial_Reveal_Object *obj, void *dest) 
{
  assert(!obj_is_marked_in_vt(obj));
  obj->vt_raw = (Partial_Reveal_VTable *)((POINTER_SIZE_INT)dest | FORWARDING_BIT_MASK);
}

inline Partial_Reveal_Object *obj_get_forwarding_pointer_in_vt(Partial_Reveal_Object *obj) 
{
  assert(obj_is_forwarded_in_vt(obj) && !obj_is_marked_in_vt(obj));
  return (Partial_Reveal_Object *)obj_get_vt(obj);
}

inline Partial_Reveal_Object *get_forwarding_pointer_in_obj_info(Partial_Reveal_Object *obj) 
{
  assert(get_obj_info(obj) & FORWARDING_BIT_MASK);
  return (Partial_Reveal_Object*) (get_obj_info(obj) & ~FORWARDING_BIT_MASK);
}

inline Boolean obj_is_forwarded_in_obj_info(Partial_Reveal_Object *obj) 
{
  return (get_obj_info(obj) & FORWARDING_BIT_MASK);
}

inline void set_forwarding_pointer_in_obj_info(Partial_Reveal_Object *obj,void *dest)
{  set_obj_info(obj,(Obj_Info_Type)dest | FORWARDING_BIT_MASK); }

struct GC;
/* all Spaces inherit this Space structure */
typedef struct Space{
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  GC* gc;
  Boolean move_object;
  Boolean (*mark_object_func)(Space* space, Partial_Reveal_Object* p_obj);
}Space;

inline unsigned int space_committed_size(Space* space){ return space->committed_heap_size;}
inline void* space_heap_start(Space* space){ return space->heap_start; }
inline void* space_heap_end(Space* space){ return space->heap_end; }

inline Boolean address_belongs_to_space(void* addr, Space* space) 
{
  return (addr >= space_heap_start(space) && addr < space_heap_end(space));
}

inline Boolean obj_belongs_to_space(Partial_Reveal_Object *p_obj, Space* space)
{
  return address_belongs_to_space((Partial_Reveal_Object*)p_obj, space);
}

/* all GCs inherit this GC structure */
struct Mutator;
struct Collector;
struct GC_Metadata;
struct Finalizer_Weakref_Metadata;
struct Vector_Block;
typedef struct GC{
  void* heap_start;
  void* heap_end;
  unsigned int reserved_heap_size;
  unsigned int committed_heap_size;
  unsigned int num_collections;
  
  /* mutation related info */
  Mutator *mutator_list;
  SpinLock mutator_list_lock;
  unsigned int num_mutators;

  /* collection related info */    
  Collector** collectors;
  unsigned int num_collectors;
  unsigned int num_active_collectors; /* not all collectors are working */
  
  /* metadata is the pool for rootset, tracestack, etc. */  
  GC_Metadata* metadata;
  Finalizer_Weakref_Metadata *finalizer_weakref_metadata;
  unsigned int collect_kind; /* MAJOR or MINOR */
  /* FIXME:: this is wrong! root_set belongs to mutator */
  Vector_Block* root_set;

  /* mem info */
  apr_pool_t *aux_pool;
  port_vmem_t *allocated_memory;

}GC;

void mark_scan_heap(Collector* collector);

inline void* gc_heap_base(GC* gc){ return gc->heap_start; }
inline void* gc_heap_ceiling(GC* gc){ return gc->heap_end; }
inline Boolean address_belongs_to_gc_heap(void* addr, GC* gc)
{
  return (addr >= gc_heap_base(gc) && addr < gc_heap_ceiling(gc));
}

void gc_parse_options();
void gc_reclaim_heap(GC* gc, unsigned int gc_cause);

#endif //_GC_COMMON_H_
