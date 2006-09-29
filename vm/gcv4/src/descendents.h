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
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#ifndef _DESCENDENTS_H_
#define _DESCENDENTS_H_

//
// Format of the descendents of an object.
//


#include "platform_lowlevel.h"

#include "gc_header.h"
#include "remembered_set.h"
#include "object_list.h" // needed by garbage_collector.h
#include "garbage_collector.h" // for the declaration of p_global_gc

// inline them for release code where GC_DEBUG>0.

//
// Return the last offset in the array that points to a reference.
// This will be decremented with each p_next_ref call.
//

inline unsigned int
init_array_scanner (Partial_Reveal_Object *p_array) 
{
    assert(!is_array_of_primitives(p_array));
 
    assert (is_array(p_array));
 
    unsigned the_number_of_descendents = vector_get_length((Vector_Handle)p_array);
    Slot last_element(vector_get_element_address_ref((Vector_Handle)p_array, the_number_of_descendents - 1));
    return (unsigned int) (((char *) last_element.get_address()) - ((char *) p_array));
}

//
//  Return a pointer to the array of object offsets associated
//  with this object, excluding weak referent field if applicable.
//

inline int *
init_strong_object_scanner (Partial_Reveal_Object *the_object) {
    return the_object->vt()->get_gcvt()->gc_strong_ref_offset_array;
}

//  Return a pointer to the array of object offsets associated
//  with this object, including weak referent field.
//
inline int *
init_object_scanner (Partial_Reveal_Object *the_object) {
    return the_object->vt()->get_gcvt()->gc_ref_offset_array;
}
//
// Given an offset and the pointer gotten from initObjectScanner return
// the next location of a pointer in the object. If none are left return null;
//

// Offsets are signed int which should be a structure that is independent
// of the word length.

inline int *
p_next_ref (int *offset) {
    return (int *)((Byte *)offset + sizeof (int));
}

inline void *
p_get_ref(int *offset, Partial_Reveal_Object *myObject) {
    if (*offset == 0) { // We are at the end of the offsets.
        return NULL;
    }
    // extract the next location
    return (void *) ((Byte *) myObject + *offset);
}


//
// Initials interator for fusing objects.
//
inline int *
init_fused_object_scanner (Partial_Reveal_Object *the_object) {
    assert(the_object->vt()->get_gcvt()->gc_fuse_info != NULL);
    return the_object->vt()->get_gcvt()->gc_fuse_info;
}

#endif // _DESCENDENTS_H_
