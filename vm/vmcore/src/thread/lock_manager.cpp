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


#include "platform.h"
#include "lock_manager.h"
#include "vm_threads.h"
#include "exceptions.h"
#include "vm_synch.h"

//void vm_thread_enumerate_from_native(VM_thread *thread); // unused anywhere

Lock_Manager *p_jit_a_method_lock;
Lock_Manager *p_vtable_patch_lock;
Lock_Manager *p_meth_addr_table_lock;
Lock_Manager *p_thread_lock;
Lock_Manager *p_method_call_lock;
Lock_Manager *p_tm_lock;
 



Lock_Manager::Lock_Manager()
{
}

Lock_Manager::~Lock_Manager()
{
}

void Lock_Manager::_lock()
{
    DEBUG_CONTENDS_LOCK (this);
    _critical_section.lock();
    DEBUG_PUSH_LOCK (this);
}

bool Lock_Manager::_tryLock()
{   if(_critical_section.tryLock()) {
        DEBUG_PUSH_LOCK (this);
        return true;
    }
    return false;
}

void Lock_Manager::_unlock()
{
    _critical_section.unlock();
    DEBUG_POP_LOCK (this);
}


void Lock_Manager::_lock_enum()
{   
    Lock_Manager::_lock_enum_or_null(0);
}


void Lock_Manager::_unlock_enum()
{
    _critical_section.unlock();
    DEBUG_POP_LOCK (this);
}


//
// If a lock is immediately available return true
//      otherwise return NULL
//
//  Use unlock_or_null so that the names match.
//

bool Lock_Manager::_lock_or_null()
{
    bool stat = _critical_section.tryLock();
    if (!stat) {
        return false;
    }
    DEBUG_PUSH_LOCK (this);
    return true;
}


void Lock_Manager::_unlock_or_null()
{
    
    _critical_section.unlock();
    DEBUG_POP_LOCK (this);
}


void Lock_Manager::_unlock_enum_or_null() 
{
    _critical_section.unlock();
    DEBUG_POP_LOCK (this);
} 

bool Lock_Manager::_lock_enum_or_null(bool UNREF return_null_on_fail)
{
    DEBUG_CONTENDS_LOCK (this);
    _critical_section.lock();
    DEBUG_PUSH_LOCK (this);
    return true;
}
