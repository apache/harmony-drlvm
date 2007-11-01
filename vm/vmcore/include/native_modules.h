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
 * @author Ilya Berezhniuk
 * @version $Revision: 1.1.2.1 $
 */

#ifndef _NATIVE_MODULES_H_
#define _NATIVE_MODULES_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SEGMENT_TYPE_UNKNOWN,
    SEGMENT_TYPE_CODE,
    SEGMENT_TYPE_DATA
} native_seg_type_t;

typedef struct {
    native_seg_type_t   type;
    void*               base;
    size_t              size;
} native_segment_t;

typedef struct native_module_t native_module_t;

struct native_module_t {
    char*               filename;
    size_t              seg_count;
    native_module_t*    next;
    native_segment_t    segments[1];
};


bool get_all_native_modules(native_module_t**, int*);
void dump_native_modules(native_module_t* modules, FILE *out);
void clear_native_modules(native_module_t**);
native_module_t* find_native_module(native_module_t* modules, void* code_ptr);

#ifdef PLATFORM_POSIX
typedef struct _raw_module raw_module;

// Structure to accumulate several segments for the same module
struct _raw_module
{
    void*               start;
    void*               end;
    bool                acc_r;
    bool                acc_x;
    char*               name;
    raw_module*         next;
};

void native_clear_raw_list(raw_module*);
raw_module* native_add_raw_segment(raw_module*, void*, void*, char, char);
native_module_t* native_fill_module(raw_module*, size_t);
#endif // PLATFORM_POSIX


#ifdef __cplusplus
}
#endif

#endif // _NATIVE_MODULES_H_
