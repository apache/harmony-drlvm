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

#include "exceptions.h"
#include "mon_enter_exit.h"
#include "environment.h"
#include "thread_generic.h"
#include "vm_synch.h"
#include "port_atomic.h"
#include "open/jthread.h"

static void vm_monitor_exit_default(ManagedObject *p_obj);
static void vm_monitor_enter_default(ManagedObject *p_obj);


void (*vm_monitor_enter)(ManagedObject *p_obj) = 0;
void (*vm_monitor_exit)(ManagedObject *p_obj) = 0;


void vm_enumerate_root_set_mon_arrays()
{
}

void vm_monitor_init()
{
    vm_monitor_enter = vm_monitor_enter_default;
    vm_monitor_exit = vm_monitor_exit_default;
    }

static void vm_monitor_enter_default(ManagedObject *p_obj)
{
    assert(!hythread_is_suspend_enabled());
    assert(p_obj);
    jobject jobj = oh_allocate_local_handle();
    jobj->object = p_obj;
    jthread_monitor_enter(jobj); 
}

static void vm_monitor_exit_default(ManagedObject *p_obj)
{
    assert(managed_object_is_valid(p_obj));
    jobject jobj = oh_allocate_local_handle();
    jobj->object = p_obj;
    jthread_monitor_exit(jobj);
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
