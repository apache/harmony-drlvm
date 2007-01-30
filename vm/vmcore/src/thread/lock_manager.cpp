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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#include "lock_manager.h"
#include "vm_threads.h"
#include "exceptions.h"

//void vm_thread_enumerate_from_native(VM_thread *thread); // unused anywhere
 
extern hythread_library_t hythread_lib;

Lock_Manager::Lock_Manager()
{
    UNREF IDATA stat = hymutex_create (&lock, TM_MUTEX_NESTED);
    assert(stat==TM_ERROR_NONE);
}

Lock_Manager::~Lock_Manager()
{
    UNREF IDATA stat = hymutex_destroy (lock);
    assert(stat==TM_ERROR_NONE);
}

void Lock_Manager::_lock()
{
    UNREF IDATA stat = hymutex_lock(lock);
    assert(stat==TM_ERROR_NONE);
}

bool Lock_Manager::_tryLock()
{     
    IDATA stat = hymutex_trylock(lock);
    return stat==TM_ERROR_NONE;    
}

void Lock_Manager::_unlock()
{
    UNREF IDATA stat = hymutex_unlock(lock);
    assert(stat==TM_ERROR_NONE);
}


void Lock_Manager::_lock_enum()
{   
    Lock_Manager::_lock_enum_or_null(0);
}


void Lock_Manager::_unlock_enum()
{
   Lock_Manager::_unlock();
}


//
// If a lock is immediately available return true
//      otherwise return NULL
//
//  Use unlock_or_null so that the names match.
//

bool Lock_Manager::_lock_or_null()
{
    return Lock_Manager::_tryLock();
}


void Lock_Manager::_unlock_or_null()
{
    Lock_Manager::_unlock_enum();
}


void Lock_Manager::_unlock_enum_or_null() 
{
     Lock_Manager::_unlock_enum();
} 

bool Lock_Manager::_lock_enum_or_null(bool UNREF return_null_on_fail)
{
    IDATA stat = hymutex_lock(lock);
    return stat==TM_ERROR_NONE;
}
