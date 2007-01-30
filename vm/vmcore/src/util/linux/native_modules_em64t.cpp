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
 * @author Petr Ivanov, Ilya Berezhniuk
 * @version $Revision: 1.1.2.1 $
 */

#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <unistd.h>
#include "platform_lowlevel.h"
#include "open/types.h"
#include "port_malloc.h"
#include "native_modules.h"

bool get_all_native_modules(native_module_t** list_ptr, int* count_ptr)
{
    char buf[_MAX_PATH];

    if (list_ptr == NULL || count_ptr == NULL)
        return false;

    pid_t pid = getpid();
    sprintf(buf, "/proc/%d/maps", pid);

    FILE* file = fopen(buf, "rt");
    if (!file)
        return false;

    POINTER_SIZE_INT start, end;
    char acc_r, acc_x;
    char filename[_MAX_PATH];
    raw_module module; // First raw module
    raw_module* lastseg = &module; // Address of last filled segment
    size_t segment_count = 0;
    int module_count = 0;
    native_module_t** cur_next_ptr = list_ptr;
    module.name = NULL;
    module.next = NULL;
    *list_ptr = NULL;

    while (!feof(file) && (fgets(buf, sizeof(buf), file)))
    {
        int res = sscanf(buf, "%" PI_FMT "x-%" PI_FMT "x %c%*c%c%*c %*" PI_FMT "x %*02x:%*02x %*u %s",
            &start, &end, &acc_r, &acc_x, filename);

        if (res < 5)
            continue;

        if (module.name == NULL || // First module, first record
            strcmp(module.name, filename) != 0) // Next module
        {
            if (segment_count) // Add previous module
            {
                native_module_t* filled =
                    native_fill_module(&module, segment_count);

                if (!filled)
                {
                    native_clear_raw_list(&module);
                    clear_native_modules(list_ptr);
                    fclose(file);
                    return false;
                }

                *cur_next_ptr = filled;
                cur_next_ptr = &filled->next;
            }

            module.name = (char*)STD_MALLOC(strlen(filename) + 1);
            if (module.name == NULL)
            {
                native_clear_raw_list(&module);
                clear_native_modules(list_ptr);
                fclose(file);
                return false;
            }

            strcpy(module.name, filename);

            // Store new module information
            module.start = (void*)start;
            module.end =  (void*)end;
            module.acc_r = (acc_r == 'r');
            module.acc_x = (acc_x == 'x');
            module.next = NULL;
            ++module_count;

            lastseg = &module;
            segment_count = 1; 
        }
        else
        {
            lastseg = native_add_raw_segment(lastseg,
                                (void*)start, (void*)end, acc_r, acc_x);

            if (lastseg == NULL)
            {
                native_clear_raw_list(&module);
                clear_native_modules(list_ptr);
                fclose(file);
                return false;
            }

            ++segment_count;
        }
    }

    if (segment_count) // To process the last module
    {
        native_module_t* filled = native_fill_module(&module, segment_count);

        if (!filled)
        {
            native_clear_raw_list(&module);
            clear_native_modules(list_ptr);
            fclose(file);
            return false;
        }

        *cur_next_ptr = filled;
    }

    native_clear_raw_list(&module);
    fclose(file);

    *count_ptr = module_count;
    return true;
}
