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

#include "open/hythread_ext.h"
#include "native_stack.h"
#include "stack_dump.h"
#include "port_filepath.h"

#ifndef NO_DBGHELP
#include <dbghelp.h>
#pragma comment(linker, "/defaultlib:dbghelp.lib")
#endif


static hymutex_t g_lock;

#ifndef NO_DBGHELP
typedef BOOL (WINAPI *SymFromAddr_type)
   (IN  HANDLE              hProcess,
    IN  DWORD64             Address,
    OUT PDWORD64            Displacement,
    IN OUT PSYMBOL_INFO     Symbol    );
typedef BOOL (WINAPI *SymGetLineFromAddr64_type)
   (IN  HANDLE                  hProcess,
    IN  DWORD64                 qwAddr,
    OUT PDWORD                  pdwDisplacement,
    OUT PIMAGEHLP_LINE64        Line64);

typedef BOOL (WINAPI *SymGetLineFromAddr_type)
   (IN  HANDLE                hProcess,
    IN  DWORD                 dwAddr,
    OUT PDWORD                pdwDisplacement,
    OUT PIMAGEHLP_LINE        Line    );

static SymFromAddr_type g_SymFromAddr = NULL;
static SymGetLineFromAddr64_type g_SymGetLineFromAddr64 = NULL;
static SymGetLineFromAddr_type g_SymGetLineFromAddr = NULL;
#endif // #ifndef NO_DBGHELP


bool sd_initialize(hymutex_t** p_lock)
{
    static int initialized = 0;

    if (!initialized)
    {
        IDATA err = hymutex_create(&g_lock, APR_THREAD_MUTEX_NESTED);

        if (err != APR_SUCCESS)
            return false;

#ifndef NO_DBGHELP
// Preventive initialization does not work
//        if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
//            return false;

        HMODULE hdbghelp = ::LoadLibrary("dbghelp");

        if (hdbghelp)
        {
            g_SymFromAddr = (SymFromAddr_type)::GetProcAddress(hdbghelp, "SymFromAddr");
            g_SymGetLineFromAddr64 = (SymGetLineFromAddr64_type)::GetProcAddress(hdbghelp, "SymGetLineFromAddr64");
            g_SymGetLineFromAddr = (SymGetLineFromAddr_type)::GetProcAddress(hdbghelp, "SymGetLineFromAddr");
        }
#endif // #ifndef NO_DBGHELP

        initialized = true;
    }

    if (p_lock)
        *p_lock = &g_lock;

    return true;
}


static const char* sd_get_region_access_info(MEMORY_BASIC_INFORMATION* pinfo)
{
    if ((pinfo->State & MEM_COMMIT) == 0)
        return "not committed";

    if ((pinfo->Protect & PAGE_GUARD) != 0)
        return "guard page occured";

    if ((pinfo->Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                          PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY |
                          PAGE_READWRITE | PAGE_READONLY)) != 0)
        return "";

    if ((pinfo->Protect & (PAGE_READWRITE | PAGE_READONLY)) != 0)
        return "without execution rights";

    return "without read rights";;
}

void sd_parse_module_info(native_module_t* module, void* ip)
{
    fprintf(stderr, "\nCrashed module:\n");

    if (module)
    {
        native_segment_t* segment = module->segments;

        assert(module->filename);

        if (!module->filename)
        { // We should not reach this code
            fprintf(stderr, "Unknown memory region 0x%"W_PI_FMT"X:0x%"W_PI_FMT"X%s\n",
                    (size_t)segment->base, (size_t)segment->base + segment->size,
                    (segment->type == SEGMENT_TYPE_CODE) ? "" : " without execution rights");
            return;
        }

        // Common shared module
        const char* short_name = port_filepath_basename(module->filename);
        const char* module_type = sd_get_module_type(short_name);
        fprintf(stderr, "%s\n(%s)\n", module->filename, module_type);
        return;
    }

    // module == NULL
    size_t start_addr, end_addr, region_size;
    MEMORY_BASIC_INFORMATION mem_info;

    VirtualQuery(ip, &mem_info, sizeof(mem_info));
    start_addr = (size_t)mem_info.BaseAddress;
    region_size = (size_t)mem_info.RegionSize;
    end_addr = start_addr + region_size;

    fprintf(stderr, "Memory region 0x%"W_PI_FMT"X:0x%"W_PI_FMT"X %s\n",
                start_addr, end_addr, sd_get_region_access_info(&mem_info));
}


void sd_get_c_method_info(MethodInfo* info, native_module_t* UNREF module, void* ip)
{
    *info->method_name = 0;
    *info->file_name = 0;
    info->line = -1;

#ifndef NO_DBGHELP

    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
        return;

    BYTE smBuf[sizeof(SYMBOL_INFO) + SD_MNAME_LENGTH - 1];
    PSYMBOL_INFO pSymb = (PSYMBOL_INFO)smBuf;
    pSymb->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymb->MaxNameLen = SD_MNAME_LENGTH;
    DWORD64 funcDispl;

    if (g_SymFromAddr &&
        g_SymFromAddr(GetCurrentProcess(), (DWORD64)(POINTER_SIZE_INT)ip, &funcDispl, pSymb))
    {
        strcpy(info->method_name, pSymb->Name);
    }

    if (g_SymGetLineFromAddr64)
    {
        DWORD offset;
        IMAGEHLP_LINE64 lineinfo;
        if (g_SymGetLineFromAddr64(GetCurrentProcess(),
                                   (DWORD64)(POINTER_SIZE_INT)ip,
                                   &offset, &lineinfo))
        {
            info->line = lineinfo.LineNumber;
            strncpy(info->file_name, lineinfo.FileName, sizeof(info->file_name));
            return;
        }
    }

    if (g_SymGetLineFromAddr)
    {
        DWORD offset;
        IMAGEHLP_LINE lineinfo;
        if (g_SymGetLineFromAddr(GetCurrentProcess(),
                                 (DWORD)(POINTER_SIZE_INT)ip,
                                 &offset, &lineinfo))
        {
            info->line = lineinfo.LineNumber;
            strncpy(info->file_name, lineinfo.FileName, sizeof(info->file_name));
        }
    }

#endif // #ifndef NO_DBGHELP
}

int sd_get_cur_tid()
{
    return GetCurrentThreadId();
}
