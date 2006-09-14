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
class_verify(const Global_Env* env, Class *clss)
{
    // fast path
    if(clss->is_verified)
        return true;

    LMAutoUnlock aulock(clss->m_lock);
    if(clss->is_verified)
        return true;

    /**
     * Get verifier enable status
     */
    Boolean is_forced = env->verify_all;
    Boolean is_bootstrap = clss->class_loader->IsBootstrap();
    Boolean is_enabled = vm_get_boolean_property_value_with_default("vm.use_verifier");

    /**
     * Verify class
     */
    if(is_enabled == TRUE
        && !class_is_interface(clss)
       && (is_bootstrap == FALSE || is_forced == TRUE))
    {
        char *error;
        Verifier_Result result = vf_verify_class((class_handler)clss, is_forced, &error );
        if( result != VER_OK ) {
            aulock.ForceUnlock();
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
                "java/lang/VerifyError", error);
            return false;
        }
        clss->is_verified = 1;
    } else {
        clss->is_verified = 2;
    }

    return true;
} // class_verify


bool
class_verify_constraints(const Global_Env* env, Class* clss)
{
    if(clss->state == ST_Error)
        return true;

    assert(clss->is_verified >= 1);

    // fast path
    if(clss->is_verified == 2)
        return true;

    // lock class
    clss->m_lock->_lock();

    // check verification stage
    if(clss->is_verified == 2) {
        clss->m_lock->_unlock();
        return true;
    }

    // get verifier enable status
    Boolean verify_all = env->verify_all;

    // unlock a class before calling of verifier
    clss->m_lock->_unlock();

    // check method constraints
    char *error;
    Verifier_Result result =
        vf_verify_class_constraints( (class_handler)clss, verify_all, &error );

    // lock class and check result
    clss->m_lock->_lock();
    if( clss->state == ST_Error ) {
        clss->m_lock->_unlock();
        return false;
    }
    if( result != VER_OK ) {
        clss->m_lock->_unlock();
        if( result == VER_ErrorLoadClass ) {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
            VM_Global_State::loader_env->JavaLangNoClassDefFoundError_String->bytes,
            error);
        } else {
            REPORT_FAILED_CLASS_CLASS(clss->class_loader, clss,
                "java/lang/VerifyError", error);
        }
        return false;
    }
    clss->is_verified = 2;

    // unlock class
    clss->m_lock->_unlock();

    return true;
} // class_verify_method_constraints
