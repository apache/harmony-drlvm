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
 * @version $Revision: 1.1.2.1.4.4 $
 *
 * @file
 * The convenience macros for logging
 */  
#ifndef _GC_COUT_H
#define _GC_COUT_H

/// default domain for "standard" information messages
#define LOG_DOMAIN "gc.verbose"

#include "cxxlog.h"

/// Convenience macro for "worker" information
#define INFOW(gc_thread,x) INFO2("gc.worker", "Worker " << gc_thread->get_id() << " " << x);

#define ASSERT_OBJECT(p_obj) ASSERT((p_obj) >= vm_heap_base_address() &&  \
        (p_obj) < vm_heap_ceiling_address(), "p_obj = " << (p_obj));

#ifdef _DEBUG

void trace_object (void *obj_to_trace);

bool is_object_traced(void *obj);

#define gc_trace(obj, message) do {                                     \
    void *_object_to_trace = (obj);                                     \
    if (is_object_traced(_object_to_trace)) {                           \
        TRACE2("gc.trace", " GC Trace "                                 \
                << _object_to_trace << " " << message);                 \
    }                                                                   \
} while(0);

void gc_trace_block (void *, const char *);
void gc_trace_slot (void **, void *, const char *);
void gc_trace_allocation (void *, const char *);

#else 

inline void trace_object (void *obj_to_trace) {
    return;
}

inline void gc_trace_slot (void **object_slot, void *object, const char *string_x)
{
    return;
}

#define gc_trace(obj,message)

static inline void gc_trace_block (void *foo, const char *moo) 
{
    return;
}

inline void gc_trace_allocation (void *object, const char *string_x)
{
    return;
}

#endif // _DEBUG
#endif // _GC_COUT_H
