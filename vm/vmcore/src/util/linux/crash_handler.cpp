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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <semaphore.h>
#include <ucontext.h>

#include "crash_handler.h"

static char executable[128];// prepare executable argument
static char pid[128];       // prepare pid argument as a string
static sem_t gdb_started;   // prevent forking debugger more than once

#if defined (__INTEL_COMPILER)
#pragma warning ( push )
#pragma warning (disable:869)
#endif
static void crash_handler (int signum, siginfo_t* info, void* context) {
    if (0 == sem_trywait(&gdb_started)) {
        if (fork() == 0) {
            fprintf(stderr, "----------------------------------------\n"
                            "gdb %s %s\n"
                            "----------------------------------------\n"
                , executable, pid); fflush(stderr);
            execl("/usr/bin/gdb", "gdb", executable, pid, NULL);
            perror("Can't run debugger");
        } else {
            // give gdb chance to start before the default handler kills the app
            sleep(10);
        }
    } else {
        // gdb was already started, 
        // reset the abort handler
        signal(signum, 0);
    }
}
#if defined (__INTEL_COMPILER)
#pragma warning ( pop )
#endif

int get_executable_name(char executable[], int len) {
    int n = readlink("/proc/self/exe", executable, len);
    if (n == -1) {
        perror("Can't determine executable name");
        return -1;
    }
    executable[n] = '\0';
    return 0;
}

static int get_pid(char pid[], int len) {
    return snprintf(pid,len,"%d",getpid());
}

void init_crash_handler() {
    get_executable_name(executable, sizeof(executable));
    get_pid(pid, sizeof(pid));
    sem_init(&gdb_started, 0, 1);
}

void install_crash_handler(int signum) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = crash_handler;
    sigaction(signum, &sa, NULL);
}

