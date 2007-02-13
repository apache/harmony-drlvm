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
 * @author Ivan Volosyuk
 */

#ifdef _WIN32

#include "gc_types.h"

void trace_msg(char *msg) {
    int error = GetLastError();
    char buffer[1024];

    DWORD n = FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            0,
            error,
            LANG_SYSTEM_DEFAULT,
            buffer,
            1024,
            0);
    INFO2("gc.lp", "gc.lp: " << msg << buffer);
}

bool large_pages_token(bool obtain) {
    HANDLE process = GetCurrentProcess();
    HANDLE token;
    TOKEN_PRIVILEGES tp;

    bool res = OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES, &token);

    if (!res) {
        trace_msg("OpenProcessToken(): ");
        return false;
    }

    size_t size = 4096 * 1024;
    // FIXME: not defined on WinXp
    //size = GetLargePageMinimum();

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = obtain ? SE_PRIVILEGE_ENABLED : 0;

    res = LookupPrivilegeValue( NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid);

    if (!res) {
        trace_msg("LookupPrivilegeValue(): ");
        CloseHandle(token);
        return false;
    }

    
    if (AdjustTokenPrivileges( token, FALSE, &tp, 0, NULL, 0) == ERROR_NOT_ALL_ASSIGNED) {
        trace_msg("AdjustTokenPrivileges(): ");
        CloseHandle(token);
        return false;
    }
    return true;
}

void *alloc_large_pages(size_t size, const char *hint) {
    bool priv = large_pages_token(true);
    void *res = NULL;

    if (priv) {
        res = VirtualAlloc(NULL, size,
                MEM_RESERVE | MEM_COMMIT|MEM_LARGE_PAGES, PAGE_READWRITE);
        if (res == NULL) {
            INFO2("gc.lp", "gc.lp: No required number of large pages found, reboot!\n\n");
        }
    }

    if (res == NULL) {
        if (is_info_enabled("gc.lp")) {
            INFO2("gc.lp", "gc.lp: Check that you have permissions:\n"
                           "gc.lp:  Control Panel->Administrative Tools->Local Security Settings->\n"
                           "gc.lp:  ->User Rights Assignment->Lock pages in memory\n"
                           "gc.lp: Start VM as soon after reboot as possible, because large pages\n"
                           "gc.lp: become fragmented and unusable after a while\n"
                           "gc.lp: Heap size should be multiple of large page size");
        } else {
            LWARN2("gc.lp", 1, "large pages allocation failed, use -verbose:gc.lp for more info");
        }
        return NULL;
    } else {
        INFO2("gc.lp", "gc.lp: large pages are allocated\n");
    }
    large_pages_token(false);
    return res;
}

#endif
