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
 * @author Petr Ivanov, Ilya Berezhniuk
 * @version $Revision: 1.1.2.1 $
 */

#include "open/types.h"
#include "port_malloc.h"
#include "native_modules.h"


void clear_native_modules(native_module_t** list_ptr)
{
    native_module_t* cur = *list_ptr;

    while (cur)
    {
        native_module_t* next = cur->next;

        if (cur->filename)
            STD_FREE(cur->filename);

        STD_FREE(cur);
        cur = next;
    }

    *list_ptr = NULL;
}

void native_clear_raw_list(raw_module* list)
{
    if (list->name)
        STD_FREE(list->name);

    raw_module* cur = list->next;

    while (cur)
    {
        raw_module* next = cur->next;
        STD_FREE(cur);
        cur = next;
    }
}

raw_module* native_add_raw_segment(raw_module* last,
                                    void* start, void* end,
                                    char acc_r, char acc_x)
{
    if (last->next == NULL)
    {
        last->next = (raw_module*)STD_MALLOC(sizeof(raw_module));
        if (last->next == NULL)
            return NULL;

        last->next->name = NULL;
        last->next->next = NULL;
    }

    last = last->next;

    last->start = start;
    last->end = end;
    last->acc_r = (acc_r == 'r');
    last->acc_x = (acc_x == 'x');

    return last;
}

native_module_t* native_fill_module(raw_module* rawModule, size_t count)
{
    native_module_t* module =
        (native_module_t*)STD_MALLOC(sizeof(native_module_t) + sizeof(native_segment_t)*(count - 1));

    if (module == NULL)
        return NULL;

    module->seg_count = count;
    module->filename = rawModule->name;
    rawModule->name = NULL;
    module->next = NULL;

    for (size_t i = 0; i < count; i++)
    {
        if (rawModule->acc_x)
            module->segments[i].type = SEGMENT_TYPE_CODE;
        else if (rawModule->acc_r)
            module->segments[i].type = SEGMENT_TYPE_DATA;
        else
            module->segments[i].type = SEGMENT_TYPE_UNKNOWN;

        module->segments[i].base = rawModule->start;
        module->segments[i].size =
            (size_t)((POINTER_SIZE_INT)rawModule->end -
                        (POINTER_SIZE_INT)rawModule->start);

        rawModule = rawModule->next;
    }

    return module;
}

