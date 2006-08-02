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
 * @author Artem Aliev
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#include "vm_threads.h"
#include <apr_general.h>
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_thread_proc.h>
#include <apr_portable.h>

#include<stdio.h>
#include<assert.h>

#define GLOBAL_MONITOR_NAME "global_monitor"
extern "C" 
{

JNIEXPORT void  __cdecl hythread_init (void*  lib);
// ========================== MONITOR ================================================
struct hymonitor {
    char* name;
    apr_thread_mutex_t *mutex;
    apr_thread_cond_t *cond;
};

typedef hymonitor* hythread_monitor_t; 

JNIEXPORT int  __cdecl hythread_monitor_init_with_name (hythread_monitor_t* handle, unsigned flags, char* name);
JNIEXPORT int __cdecl
hythread_monitor_destroy (hythread_monitor_t monitor);
JNIEXPORT int __cdecl 
hythread_monitor_try_enter (hythread_monitor_t monitor);
JNIEXPORT int __cdecl 
hythread_monitor_enter (hythread_monitor_t monitor);
JNIEXPORT int __cdecl 
hythread_monitor_exit (hythread_monitor_t monitor);
JNIEXPORT int __cdecl
hythread_monitor_notify (hythread_monitor_t monitor);
JNIEXPORT int __cdecl
hythread_monitor_notify_all (hythread_monitor_t monitor);
JNIEXPORT int __cdecl
hythread_monitor_wait (hythread_monitor_t monitor);

// ======================= ATTACH ======================================
typedef apr_os_thread_t hythread_t;

JNIEXPORT int __cdecl 
hythread_attach (hythread_t* handle);
JNIEXPORT void __cdecl 
hythread_detach (hythread_t handle);
JNIEXPORT hythread_t __cdecl hythread_self();
JNIEXPORT int __cdecl
hythread_exit (hythread_monitor_t monitor);

JNIEXPORT unsigned* __cdecl 
hythread_global (char* name);

// ==================== create thread ====================
typedef int(__cdecl* hythread_entrypoint_t)(void*);
JNIEXPORT int  __cdecl
hythread_create(hythread_t* handle, unsigned stacksize, unsigned priority, unsigned suspend, hythread_entrypoint_t entrypoint, void* entryarg);
// ================== TLS =======================================
typedef apr_threadkey_t* hythread_tls_key_t;

JNIEXPORT int __cdecl 
hythread_tls_alloc (hythread_tls_key_t* handle);
JNIEXPORT int __cdecl 
hythread_tls_free (hythread_tls_key_t key);
JNIEXPORT void* __cdecl 
hythread_tls_get (hythread_t thread, hythread_tls_key_t key);
JNIEXPORT int __cdecl 
hythread_tls_set (hythread_t thread, hythread_tls_key_t key, void* value);

}// extern "C"
