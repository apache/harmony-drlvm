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

#include "gc_common.h"

/* Setter functions for the gc class property field. */
void gc_set_prop_alignment_mask (GC_VTable_Info *gcvt, unsigned int the_mask)
{
  gcvt->gc_class_properties |= the_mask;
}
void gc_set_prop_non_ref_array (GC_VTable_Info *gcvt)
{
  gcvt->gc_class_properties |= CL_PROP_NON_REF_ARRAY_MASK;
}
void gc_set_prop_array (GC_VTable_Info *gcvt)
{
  gcvt->gc_class_properties |= CL_PROP_ARRAY_MASK;
}
void gc_set_prop_pinned (GC_VTable_Info *gcvt)
{
  gcvt->gc_class_properties |= CL_PROP_PINNED_MASK;
}
void gc_set_prop_finalizable (GC_VTable_Info *gcvt)
{
  gcvt->gc_class_properties |= CL_PROP_FINALIZABLE_MASK;
}


/* A comparison function for qsort() called below to order offset slots. */
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

static int *build_ref_offset_array(Class_Handle ch, GC_VTable_Info *gcvt)
{
  unsigned num_ref_fields = 0;
  unsigned num_fields = class_num_instance_fields_recursive(ch);

  unsigned idx;
  for(idx = 0; idx < num_fields; idx++) {
    Field_Handle fh = class_get_instance_field_recursive(ch, idx);
    if(field_is_reference(fh)) {
      num_ref_fields++;
    }
  }

  if( num_ref_fields )   
    gcvt->gc_object_has_ref_field = true;
  else 
    return NULL;
     
  /* add a null-termination slot */
  unsigned int size = (num_ref_fields+1) * sizeof (unsigned int);

  /* alloc from gcvt pool */
  int *result = (int*) STD_MALLOC(size);
  assert(result);

  int *new_ref_array = result;
  for(idx = 0; idx < num_fields; idx++) {
    Field_Handle fh = class_get_instance_field_recursive(ch, idx);
    if(field_is_reference(fh)) {
      *new_ref_array = field_get_offset(fh);
      new_ref_array++;
    }
  }

  /* ref array is NULL-terminated */
  *new_ref_array = 0;

  gcvt->gc_number_of_ref_fields = num_ref_fields;

  /* offsets were built with idx, may not be in order. Let's sort it anyway.
     FIXME: verify_live_heap depends on ordered offset array. */
  qsort(result, num_ref_fields, sizeof(*result), intcompare);

  gcvt->gc_ref_offset_array  = result;
  
  return new_ref_array;
}

void gc_class_prepared (Class_Handle ch, VTable_Handle vth) 
{
  GC_VTable_Info *gcvt;  
  assert(ch);
  assert(vth);

  Partial_Reveal_VTable *vt = (Partial_Reveal_VTable *)vth;
  
  /* FIXME: gcvts are too random is memory */
  gcvt = (GC_VTable_Info *) STD_MALLOC(sizeof(GC_VTable_Info));
  assert(gcvt);
  vtable_set_gcvt(vt, gcvt);
  memset((void *)gcvt, 0, sizeof(GC_VTable_Info));
  gcvt->gc_clss = ch;
  gcvt->gc_class_properties = 0;
  gcvt->gc_object_has_ref_field = false;
  
  gc_set_prop_alignment_mask(gcvt, class_get_alignment(ch));

  if(class_is_array(ch)) {
    Class_Handle array_element_class = class_get_array_element_class(ch);
    gc_set_prop_array(gcvt);
    gcvt->gc_array_element_size = class_element_size(ch);
    unsigned int the_offset = vector_first_element_offset_unboxed(array_element_class);
    gcvt->gc_array_first_element_offset = the_offset;
  
    if (class_is_non_ref_array (ch)) {
      gc_set_prop_non_ref_array(gcvt);
    }else{
      gcvt->gc_object_has_ref_field = true;
    }
  }

  if (class_is_finalizable(ch)) {
    gc_set_prop_finalizable(gcvt);
  }

  unsigned int size = class_get_boxed_data_size(ch);
  gcvt->gc_allocated_size = size;
  
  /* Build the offset array */
  build_ref_offset_array(ch, gcvt);

  gcvt->gc_class_name = class_get_name(ch);
  assert (gcvt->gc_class_name);

}  /* gc_class_prepared */


