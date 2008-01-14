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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "open/hythread_ext.h"
#include "native_stack.h"
#include "port_filepath.h"
#include "port_dso.h"
#include "stack_dump.h"


static hymutex_t g_lock;


bool sd_initialize(hymutex_t** p_lock)
{
    static int initialized = 0;

    if (!initialized)
    {
        IDATA err = hymutex_create(&g_lock, APR_THREAD_MUTEX_NESTED);

        if (err != APR_SUCCESS)
            return false;

        initialized = true;
    }

    if (p_lock)
        *p_lock = &g_lock;

    return true;
}


static bool sd_is_predefined_name(const char* name)
{
    if (*name != '[')
        return false;

    return true;
//    return (!strcmp(name, "[heap]") ||
//            !strcmp(name, "[stack]") ||
//            !strcmp(name, "[vdso]"));
}

static inline native_segment_t* sd_find_segment(native_module_t* module, void* ip)
{
    for (size_t i = 0; i < module->seg_count; i++)
    {
        if (module->segments[i].base <= ip &&
            (char*)module->segments[i].base + module->segments[i].size > ip)
            return &module->segments[i];
    }

    assert(0);
    return NULL;
}

void sd_parse_module_info(native_module_t* module, void* ip)
{
    fprintf(stderr, "\nCrashed module:\n");

    if (!module)
    { // Unknown address
        fprintf(stderr, "Unknown address 0x%"W_PI_FMT"X\n",
                (POINTER_SIZE_INT)ip);
        return;
    }

    native_segment_t* segment = sd_find_segment(module, ip);

    if (!module->filename)
    {
        fprintf(stderr, "Unknown memory region 0x%"W_PI_FMT"X:0x%"W_PI_FMT"X%s\n",
                (size_t)segment->base, (size_t)segment->base + segment->size,
                (segment->type == SEGMENT_TYPE_CODE) ? "" : " without execution rights");
        return;
    }

    if (sd_is_predefined_name(module->filename))
    { // Special memory region
        fprintf(stderr, "%s memory region 0x%"W_PI_FMT"X:0x%"W_PI_FMT"X%s\n",
                module->filename,
                (size_t)segment->base, (size_t)segment->base + segment->size,
                (segment->type == SEGMENT_TYPE_CODE) ? "" : " without execution rights");
        return;
    }

    // Common shared module
    const char* short_name = port_filepath_basename(module->filename);
    const char* module_type = sd_get_module_type(short_name);

    fprintf(stderr, "%s\n(%s)\n", module->filename, module_type);
}


void sd_get_c_method_info(MethodInfo* info, native_module_t* module, void* ip)
{
    *info->method_name = 0;
    *info->file_name = 0;
    info->line = -1;

    if (!module || !module->filename)
        return;

    POINTER_SIZE_INT offset = (POINTER_SIZE_INT)ip;

    if (strstr(module->filename, PORT_DSO_EXT) != NULL) // Shared object
    { // IP for addr2line should be an offset within shared library
        native_segment_t* seg = sd_find_segment(module, ip);
        offset -= (POINTER_SIZE_INT)seg->base;
    }

    int po[2];
    pipe(po);

    char ip_str[20];
    sprintf(ip_str, "0x%"PI_FMT"x\n", offset);

    if (!fork())
    {
        close(po[0]);
        dup2(po[1], 1);
        execlp("addr2line", "addr2line", "-f", "-s", "-e", module->filename, "-C", ip_str, NULL);
        //fprintf(stderr, "Warning: Cannot run addr2line. No symbolic information will be available\n");
        printf("??\n??:0\n"); // Emulate addr2line output
        exit(-1);
    }
    else
    {
        close(po[1]);
        char buf[sizeof(info->method_name) + sizeof(info->file_name)];
        int status;
        wait(&status);
        int count = read(po[0], buf, sizeof(buf) - 1);
        close(po[0]);

        if (count < 0)
        {
            fprintf(stderr, "read() failed during addr2line execution\n");
            return;
        }

        while (isspace(buf[count-1]))
            count--;

        buf[count] = '\0';
        int i = 0;

        for (; i < count; i++)
        {
            if (buf[i] == '\n')
            { // Function name is limited by '\n'
                buf[i] = '\0';
                strncpy(info->method_name, buf, sizeof(info->method_name));
                break;
            }
        }

        if (i == count)
            return;

        char* fn = buf + i + 1;

        for (; i < count && buf[i] != ':'; i++); // File name and line number are separated by ':'

        if (i == count)
            return;

        buf[i] = '\0';
        strncpy(info->file_name, fn, sizeof(info->file_name));

        info->line = atoi(buf + i + 1); // Line number

        if (info->line == 0)
            info->line = -1;
    }
}

int sd_get_cur_tid()
{
    return gettid();
}
