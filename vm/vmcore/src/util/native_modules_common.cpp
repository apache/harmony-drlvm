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

#include "native_modules.h"

native_module_t* find_native_module(native_module_t* modules, void* code_ptr)
{
    for (native_module_t* module = modules; NULL != module;
            module = module->next) {
        for (size_t s = 0; s < module->seg_count; s++) {
            void* base = module->segments[s].base;
            size_t size = module->segments[s].size;

            if (code_ptr >= base && code_ptr < (char*) base + size)
                return module;
        }
    }

    // no matching module
    return NULL;
}
