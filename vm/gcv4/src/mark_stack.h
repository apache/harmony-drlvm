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


#ifndef _MARK_STACK_H
#define _MARK_STACK_H

//
// Provides the two routines needed to support an mark_stack. 
// mark_stacks are not synchronized and support a single
// writer and a single readers but they can access the mark_stack concurrently
// on the IA32.
//

// This can be reduced or increased. We made it this size so that 
// for IA32 it basically takes up a single page. This is why we have the - 6 which
// takes into account the other fields in the mark stack bucket.

// Buffers can be removed if there is more than one of them. They can be added when
// a thread is through helping out the primary marker. Ultimately it is up to 
// the primary marker pop the elments off the stack.

#define FIELDS_IN_MARK_STACK 4
#define MARK_STACK_ELEMENTS (1024 - FIELDS_IN_MARK_STACK)

// An mark_stack is a chained list of mark_stack buffers, each of holding mark_stack_SIZE entries.
typedef struct mark_stack {
    Partial_Reveal_Object    **p_top_of_stack;       // points to top of stack                    (producer/consumer modifies)
    mark_stack               *next;
    mark_stack               *previous;
    Partial_Reveal_Object    *data[MARK_STACK_ELEMENTS];    // array of objects
    POINTER_SIZE_INT         ignore;           // Location 1 past the data, 
                                               //just so we never point outside this structure with p_top_of_stack++.
} mark_stack;


//
// An mark_stack_container provides safe support for a single reader (consumer) 
// and a single writer (producer). The fielders the reader can modify
// are mark_stack_read_ptr current_mark_stack. It can read the data at *mark_stack_read_ptr.
// 
// The write (consumer) modifies to the data array, *mark_stack_write_ptr, first_mark_stack and
// last_mark_stack. It can only write to the last_mark_stack. 

// For IPF all the fields are volatile which means that a st.rel will be used
// when updating any of these fields. This will make sure that the reader will see
// what the writer has written.
typedef struct mark_stack_container {    
    // Each mark_stack container has an mark_stack pointer. The first slot in an mark_stack holds the next mark_stack
    // or NULL if this is the last mark_stack associated with this thread. These are void *
    // to simplify interface needed by the dll.
    // a pointer to a slot which is a pointer to a pointer, thus the ***
    // Modified by the writer.
    mark_stack    *first_mark_buffer;      // First mark_stack associated with this thread or NULL if none. 
    mark_stack    *last_mark_buffer;        // Last mark_stack associated with this thread of NULL if none.
    // Modified by the reader
    mark_stack    *peeker_mark_buffer;     // The peekers eats at the stack from the bottom up, but place more work
                                          // onto the relevant ssb.
} mark_stack_container;

// 
// Creates a new mark_stack
mark_stack_container *create_mark_stack ();
mark_stack *_get_empty_mark_stack_thread_safe();
void _release_empty_mark_stack_thread_safe(mark_stack *the_mark_stack);

#endif
