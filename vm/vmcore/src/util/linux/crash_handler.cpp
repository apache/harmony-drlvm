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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <semaphore.h>

#include "vm_core_types.h"
#include "environment.h"
#include "port_sysinfo.h"
#include "platform_lowlevel.h"
#include "exception_filter.h"
#include "port_thread.h"

#include "crash_handler.h"

static char* g_executable = NULL;// Executable file name
static sem_t g_sem_started;     // Prevent forking debugger more than once
static bool g_prepared = false; // Flag is set if gdb crash handler is prepared
static bool g_enabled = false;  // vm.crash_handler value is stored here


#if defined (__INTEL_COMPILER)
#pragma warning ( push )
#pragma warning (disable:869)
#endif


bool is_gdb_crash_handler_enabled()
{
    return (g_prepared && g_enabled);
}

bool gdb_crash_handler(Registers* regs)
{
    if (!g_prepared || !g_enabled ||
        !g_executable ||
        0 != sem_trywait(&g_sem_started)) // gdb was already started
        return false;

    // Print register info
    print_reg_state(regs);

    static const int tid_len = 10;
    char tid[tid_len];
    snprintf(tid, tid_len, "%d", gettid());

    if (fork() == 0)
    {
        fprintf(stderr, "----------------------------------------\n"
                        "gdb %s %s\n"
                        "----------------------------------------\n"
            , g_executable, tid);
        fflush(stderr);

        execlp("gdb", "gdb", g_executable, tid, NULL);
        perror("Can't run gdb");
    }
    else
    {
        // give gdb chance to start before the default handler kills the app
        sleep(10);
    }
}
#if defined (__INTEL_COMPILER)
#pragma warning ( pop )
#endif

static int get_executable_name()
{
    if (port_executable_name(&g_executable) != 0)
        return -1;

    return g_executable ? 0 : -1;
}


void init_gdb_crash_handler()
{
    if (sem_init(&g_sem_started, 0, 1) != 0 ||
        get_executable_name() != 0)
    {
        g_prepared = false;
        return;
    }

    if (!VM_Global_State::loader_env)
        g_enabled = true;
    else
        g_enabled = get_boolean_property("vm.crash_handler", FALSE, VM_PROPERTIES);

    g_prepared = true;
}
