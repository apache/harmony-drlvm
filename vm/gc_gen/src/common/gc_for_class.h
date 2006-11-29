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

#ifndef _GC_TYPES_H_
#define _GC_TYPES_H_

#include "open/types.h"

#define FORWARDING_BIT_MASK 0x1
#define MARK_BIT_MASK 0x2

typedef void *Thread_Handle; 
typedef POINTER_SIZE_INT Obj_Info_Type;

typedef struct GC_VTable_Info {
  unsigned int gc_object_has_slots;
  unsigned int gc_number_of_ref_fields;

  uint32 gc_class_properties;    // This is the same as class_properties in VM's VTable.

  unsigned int instance_data_size;

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
  
} GC_VTable_Info;

typedef struct Partial_Reveal_VTable {
  GC_VTable_Info *gcvt;
} Partial_Reveal_VTable;

typedef struct Partial_Reveal_Object {
  Partial_Reveal_VTable *vt_raw;
  Obj_Info_Type obj_info;
} Partial_Reveal_Object;

inline Obj_Info_Type get_obj_info(Partial_Reveal_Object *obj) 
{  return obj->obj_info; }

inline void set_obj_info(Partial_Reveal_Object *obj, Obj_Info_Type new_obj_info) 
{  obj->obj_info = new_obj_info; }

inline Obj_Info_Type *get_obj_info_addr(Partial_Reveal_Object *obj) 
{  return &obj->obj_info; }

inline Partial_Reveal_VTable *obj_get_vtraw(Partial_Reveal_Object *obj) 
{  return obj->vt_raw; }

inline Partial_Reveal_VTable **obj_get_vtraw_addr(Partial_Reveal_Object *obj) 
{  return &obj->vt_raw; }

inline Partial_Reveal_VTable *obj_get_vt(Partial_Reveal_Object *obj) 
{  return (Partial_Reveal_VTable *)((POINTER_SIZE_INT)obj->vt_raw & ~(FORWARDING_BIT_MASK | MARK_BIT_MASK)); }

inline void obj_set_vt(Partial_Reveal_Object *obj, Allocation_Handle ah) 
{  obj->vt_raw = (Partial_Reveal_VTable *)ah; }

inline GC_VTable_Info *vtable_get_gcvt(Partial_Reveal_VTable *vt) 
{  return vt->gcvt; }

inline void vtable_set_gcvt(Partial_Reveal_VTable *vt, GC_VTable_Info *new_gcvt) 
{  vt->gcvt = new_gcvt; }

inline GC_VTable_Info *obj_get_gcvt(Partial_Reveal_Object *obj) 
{
  Partial_Reveal_VTable *vt = obj_get_vt(obj);
  return vtable_get_gcvt(vt);
}

inline Boolean object_has_slots(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);
  return gcvt->gc_object_has_slots;   
}

inline Boolean object_is_array(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);
  return (gcvt->gc_class_properties & CL_PROP_ARRAY_MASK);
  
} 

inline Boolean obj_is_primitive_array(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  return (gcvt->gc_class_properties & CL_PROP_NON_REF_ARRAY_MASK);
}

inline Class_Handle obj_get_class_handle(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  return gcvt->gc_clss;
}

inline unsigned int nonarray_object_size(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  return gcvt->gc_allocated_size;
}

#endif //#ifndef _GC_TYPES_H_

