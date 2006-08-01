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
 * @author Pavel Afremov
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#ifndef _STACK_TRACE_H_
#define _STACK_TRACE_H_

// This module provides stack traces.
// A stack trace is a sequence of frames starting from the topmost on the stack down to the bottom most.
// It inlcudes one frame for each managed stack frame, and one frame for each M2nFrame that has an associated method.
// For each frame, the method and ip is provided, and optionally the file and line number.

#include <stdio.h>

#include "open/vm_util.h"

// Defines the StackTraceFrame structure
#include "exception.h"

#ifdef __cplusplus
extern "C" {
#endif

// Return the length of the stack trace for the current thread.
VMEXPORT unsigned st_get_depth();

// Return the stack frame frame at the given depth (zero based) for the current thread.
// Returns true on success, false if depth is greater than or equal to the current thread's stack trace length.
VMEXPORT bool st_get_frame(unsigned depth, StackTraceFrame*);

// Allocate required num of frames. Used internaly by interpreter to avoid
// memory allocation / deallocation conflicts in VM and DLLS in Win32
VMEXPORT StackTraceFrame* st_alloc_frames(int num);

// Return the stack trace for the current thread.
// The caller is responsible for freeing the memory.
VMEXPORT void st_get_trace(unsigned* depth, StackTraceFrame**);

// Function takes method and ip and fills in source file and line number
VMEXPORT void get_file_and_line(Method_Handle method, void *ip, const char **file, int *line);

#ifdef __cplusplus
}
#endif

// Append to buf a suitable human readable version of the frame.
void st_print_frame(ExpandableMemBlock* buf, StackTraceFrame*);

// Print the current thread's trace.
// This function includes all M2nFrames.
// It is intended for debugging purposes.
void st_print(FILE*);

void st_print();

Method_Handle get_method(StackIterator* si);


uint32 si_get_inline_depth(StackIterator* si);


#endif //!_STACK_TRACE_H_
