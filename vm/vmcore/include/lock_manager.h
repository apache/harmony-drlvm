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




#ifndef _lock_manager_H_
#define _lock_manager_H_ 

#include "open/vm_util.h" 
#include "open/types.h"

#ifdef __cplusplus

class Lock_Manager {

public:
    VMEXPORT Lock_Manager();
    VMEXPORT ~Lock_Manager();

    VMEXPORT void _lock();
    VMEXPORT void _unlock();
    bool _tryLock();

    void _lock_enum();
    void _unlock_enum();

    bool _lock_or_null();
    void _unlock_or_null();

    void _unlock_enum_or_null ();
    bool _lock_enum_or_null (bool return_null_on_fail);

private:
    hymutex_t    lock;
};


// Auto-unlocking class for Lock_Manager
class LMAutoUnlock {
public:
    LMAutoUnlock( Lock_Manager* lock ):m_innerLock(lock), m_unlock(true)
    { m_innerLock->_lock(); }
    ~LMAutoUnlock() { if( m_unlock ) m_innerLock->_unlock(); }
    void ForceUnlock() { m_innerLock->_unlock(); m_unlock = false; }
private:
    Lock_Manager* m_innerLock;
    bool m_unlock;
};


extern Lock_Manager *p_jit_a_method_lock;
extern Lock_Manager *p_vtable_patch_lock;
extern Lock_Manager *p_meth_addr_table_lock;
// 20040224 Support for recording which methods (actually, CodeChunkInfo's) call which other methods.
extern Lock_Manager *p_method_call_lock;
extern Lock_Manager *p_handle_lock;
#endif // __cplusplus

#endif // _lock_manager_H_
