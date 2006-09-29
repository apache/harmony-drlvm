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


//
// When colocating objects using a sliding compaction algorithm one
// needs to have a place to temporally store objects until their 
// final location is available.
// During the slide phase of the collector we will place objects in a Stash_Block
// if the target is in another GC threads area and that area is not prepared to accept
// the objects just yet.
//

#include "gc_header.h"
#include "gc_debug.h"
#include "port_malloc.h"

#define STASH_SIZE (65536 - (4 * (sizeof (void *))))
typedef struct stash {
    // Next 64K block to put objects in.
    stash *next;
    // Pointer to end of allcated objects, where the next object is going to be placed.
    Partial_Reveal_Object *frontier;
    // The end of the block. No object can go beyond this point.
    Partial_Reveal_Object *ceiling;
    // The next object to be returned. If this == frontier then there are no objects available
    Partial_Reveal_Object *current;
    // Where the objects go.
    char the_stash[STASH_SIZE];
} stash;

class Stash_Block {
public:

Stash_Block()
{
    assert (sizeof(stash) == 65536);
    block_list = get_new_stash();
    frontier_block = block_list;
    current_block = block_list;
}

void clear_stash (stash *block)
{
    block->ceiling = (Partial_Reveal_Object *)&(block->the_stash[STASH_SIZE - 1]);
    block->current = (Partial_Reveal_Object *)&(block->the_stash[0]);
    block->frontier = (Partial_Reveal_Object *)&(block->the_stash[0]);
}

stash *get_new_stash()
{
    stash *result = (stash *)STD_MALLOC(sizeof(stash));
    clear_stash(result);
    result->next = NULL;
    return result;
}

~Stash_Block()
{
    stash *temp = block_list;
    stash *next = NULL;
    while (temp) {
        next = temp->next;
        STD_FREE(temp);
        temp = next;
    }
    block_list = NULL;
    current_block = NULL;
}

// Reset the stash for a new GC cycle.

void reset()
{
    frontier_block = block_list;
    current_block = block_list;
    stash *block = block_list;
    for (block = block_list; block; block = block->next) {
        clear_stash (block);
    }
}

// Adds an object to this Stash_Block.
void add_object (Partial_Reveal_Object *obj)
{
    assert (frontier_block->current <= frontier_block->frontier);
    assert (frontier_block->frontier <= frontier_block->ceiling);
    gc_trace ((void *)obj, "stash_block->add_object preserving object.");
    POINTER_SIZE_INT size = get_object_size_bytes (obj);

    if ( (((POINTER_SIZE_INT)(frontier_block->frontier) + size) > (POINTER_SIZE_INT)(frontier_block->ceiling)) ){
        if (frontier_block->next) {
            frontier_block = frontier_block->next;
        } else {
            frontier_block->next = get_new_stash();
            frontier_block = frontier_block->next;
        }
    }
    assert (( (POINTER_SIZE_INT)(frontier_block->frontier) + size) <= (POINTER_SIZE_INT)(frontier_block->ceiling));
    void *dest = frontier_block->frontier;
    frontier_block->frontier = (Partial_Reveal_Object *)(((POINTER_SIZE_INT)frontier_block->frontier) + (POINTER_SIZE_INT)size);
    gc_trace (obj, "Stashing this from object, to be moved later into to space.");
    memmove (dest, (void *) obj, size);
    assert (frontier_block->current <= frontier_block->frontier);
    assert (frontier_block->frontier <= frontier_block->ceiling);
}

// Returns object if one is available, otherwise NULL;
Partial_Reveal_Object *get_next_object() 
{
    assert (current_block->current <= current_block->frontier);
    assert (current_block->frontier <= current_block->ceiling);
    Partial_Reveal_Object *result = NULL;
    if ( current_block->current == current_block->frontier ) {
        if (current_block->next) {
            current_block = current_block->next;
            assert (current_block->current <= current_block->frontier);
            assert (current_block->frontier <= current_block->ceiling);
            return get_next_object();
        } else {
            return NULL;
        }
    }
    result = current_block->current;
    current_block->current = (Partial_Reveal_Object *)(((POINTER_SIZE_INT)result) + (POINTER_SIZE_INT)(get_object_size_bytes(result)));
    gc_trace (result, "Returning this stashed object (not in heap), it is about to go to to space.");
    return result;
}

private:
    // Null terminated list of blocks holding objects. Each block is 64K in size. The
    // first word holds the next field.
stash *block_list;
// Block where allocation takes place.
stash *frontier_block;
// Block objects are returned from.
stash *current_block;
};
