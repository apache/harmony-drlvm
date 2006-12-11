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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.2.4.4 $
 */  


#define LOG_DOMAIN "verifier"
#include "cxxlog.h"

#include "verifier.h"
#include "Class.h"
#include "classloader.h"
#include "environment.h"
#include "open/vm.h"
#include "lock_manager.h"

bool
Class::verify(const Global_Env* env)
{
    // fast path
    if(m_state >= ST_BytecodesVerified)
        return true;

    LMAutoUnlock aulock(m_lock);
    if(m_state >= ST_BytecodesVerified)
        return true;

    /**
     * Get verifier enable status
     */
    Boolean is_forced = env->verify_all;
    Boolean is_bootstrap = m_class_loader->IsBootstrap();
    Boolean is_enabled = get_boolean_property("vm.use_verifier", TRUE, VM_PROPERTIES);

    /**
     * Verify class
     */
    if(is_enabled == TRUE
        && !is_interface()
        && (is_bootstrap == FALSE || is_forced == TRUE))
    {
        char *error;
        Verifier_Result result = vf_verify_class((class_handler)this, is_forced, &error);
        if( result != VER_OK ) {
            aulock.ForceUnlock();
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this,
                "java/lang/VerifyError", error);
            return false;
        }
    }
    m_state = ST_BytecodesVerified;

    return true;
} // Class::verify


bool
Class::verify_constraints(const Global_Env* env)
{
    // fast path
    switch(m_state)
    {
    case ST_ConstraintsVerified:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        return true;
    }

    // lock class
    lock();

    // check verification stage again
    switch(m_state)
    {
    case ST_ConstraintsVerified:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        unlock();
        return true;
    }
    assert(m_state == ST_Prepared);

    // get verifier enable status
    Boolean verify_all = env->verify_all;

    // unlock a class before calling to verifier
    unlock();

    // check method constraints
    char *error;
    Verifier_Result result =
        vf_verify_class_constraints((class_handler)this, verify_all, &error);

    // lock class and check result
    lock();
    switch(m_state)
    {
    case ST_ConstraintsVerified:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        unlock();
        return true;
    }
    if( result != VER_OK ) {
        unlock();
        if( result == VER_ErrorLoadClass ) {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this,
            VM_Global_State::loader_env->JavaLangNoClassDefFoundError_String->bytes,
            error);
        } else {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this,
                "java/lang/VerifyError", error);
        }
        return false;
    }
    m_state = ST_ConstraintsVerified;

    // unlock class
    unlock();

    return true;
} // class_verify_method_constraints
