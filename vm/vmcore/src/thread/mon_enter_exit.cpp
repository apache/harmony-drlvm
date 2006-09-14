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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#define LOG_DOMAIN "enumeration"
#include "cxxlog.h"

#include "platform_lowlevel.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;


#include "open/types.h"
#include "open/jthread.h"
#include "object_layout.h"
#include "vm_threads.h"
#include "jit_runtime_support.h"
#include "exceptions.h"

#include "mon_enter_exit.h"
#include "thread_generic.h"

#include "object_generic.h"
#include "vm_synch.h"
#include "vm_stats.h"
#include "object_handles.h"

#include "vm_process.h"
//#include "java_mrte.h"
#include "port_atomic.h"

static void vm_monitor_exit_default(ManagedObject *p_obj);
static void vm_monitor_enter_default(ManagedObject *p_obj);
static uint32 vm_monitor_try_enter_default(ManagedObject *p_obj);
static uint32 vm_monitor_try_exit_default(ManagedObject *p_obj);


void (*vm_monitor_enter)(ManagedObject *p_obj) = 0;
void (*vm_monitor_exit)(ManagedObject *p_obj) = 0;
uint32 (*vm_monitor_try_enter)(ManagedObject *p_obj) = 0;
uint32 (*vm_monitor_try_exit)(ManagedObject *p_obj) = 0;


void vm_enumerate_root_set_mon_arrays()
{
}

void vm_monitor_init()
{
    vm_monitor_enter = vm_monitor_enter_default;
    vm_monitor_try_enter = vm_monitor_try_enter_default;
    vm_monitor_exit = vm_monitor_exit_default;
    vm_monitor_try_exit = vm_monitor_try_exit_default;
}

static void vm_monitor_enter_default(ManagedObject *p_obj)
{
    assert(managed_object_is_valid(p_obj));
    //
    assert(!hythread_is_suspend_enabled());
    assert(p_obj);
    jobject jobj = oh_allocate_local_handle();
    jobj->object = p_obj;
    jthread_monitor_enter(jobj); 
}

static void vm_monitor_exit_default(ManagedObject *p_obj)
{
    assert(managed_object_is_valid(p_obj));
    //
    assert(!hythread_is_suspend_enabled());
    assert(p_obj);
    jobject jobj = oh_allocate_local_handle();
    jobj->object = p_obj;
    jthread_monitor_exit(jobj);
}

static uint32 vm_monitor_try_enter_default(ManagedObject *p_obj) {
    return (uint32)hythread_thin_monitor_try_enter((hythread_thin_monitor_t *)((char *)p_obj+4));
}

static uint32 vm_monitor_try_exit_default(ManagedObject *p_obj) {
    return (uint32)hythread_thin_monitor_exit((hythread_thin_monitor_t *)((char *)p_obj+4));
}


// returns true if the object has its monitor taken...
// asserts if the object header is ill-formed.
// returns false if object's monitor is free.
//
// ASSUMPTION -- CAVEAT -- Should be called only during stop the world GC...
//
//

Boolean verify_object_header(void *ptr)
{
        return FALSE;
    }
