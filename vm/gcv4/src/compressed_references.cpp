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


#include "open/vm.h"
#include "gc_header.h"
#include "remembered_set.h"
#include "compressed_references.h"


#ifndef DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
bool gc_references_are_compressed = false;
#endif // !DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES


void *Slot::cached_heap_base = NULL;
void *Slot::cached_heap_ceiling = NULL;



// This function returns TRUE if references within objects and vector elements 
// will be treated by GC as uint32 offsets rather than raw pointers.
Boolean gc_supports_compressed_references ()
{
#ifdef DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
    return gc_references_are_compressed ? TRUE : FALSE;
#else // !DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
    return vm_references_are_compressed();
#endif // !DISALLOW_RUNTIME_SELECTION_OF_COMPRESSED_REFERENCES
}
