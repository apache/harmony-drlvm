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
 * @author Pavel Rebriy
 * @version $Revision: 1.1.2.1.4.3 $
 */  
#ifndef _VERIFIER_H_
#define _VERIFIER_H_

#include "class_interface.h"

enum Verifier_Result
{
    VER_OK,
    VER_ErrorUnknown,               // unknown error occured
    VER_ErrorInstruction,           // instruction coding error
    VER_ErrorConstantPool,          // bad constant pool format
    VER_ErrorLocals,                // incorrect usage of local variables
    VER_ErrorBranch,                // incorrect local branch offset
    VER_ErrorStackOverflow,         // stack overflow
    VER_ErrorStackUnderflow,        // stack underflow
    VER_ErrorStackDepth,            // inconstant stack deep to basic block
    VER_ErrorCodeEnd,               // falling off the end of the code
    VER_ErrorHandler,               // error in method handler
    VER_ErrorDataFlow,              // data flow error
    VER_ErrorIncompatibleArgument,  // incompatible argument to function
    VER_ErrorLoadClass,             // error load class
    VER_ErrorResolve,               // error resolve field/method
    VER_ErrorJsrRecursive,          // found a recursive subroutine call sequence
    VER_ErrorJsrMultipleRet,        // subroutine splits execution into several
                                    // <code>ret</code> instructions
    VER_ErrorJsrLoadRetAddr,        // loaded return address from local variable
    VER_ErrorJsrOther,              // invalid subroutine
    VER_ClassNotLoaded,             // verified class not loaded yet
    VER_ErrorInternal,              // error in verification process
    VER_Continue                    // intermediate status, continue analysis
};

/**
 * Function provides initial verification of class.
 * @param klass     - class handler
 * @param verifyAll - if flag is set, verifier provides full verification checks
 * @param error     - error message of verifier
 * @return Class verification result.
 * @note Assertion is raised if klass is equal to null.
 */
Verifier_Result
vf_verify_class( class_handler klass, unsigned verifyAll, char **error );

/**
 * Function provides final constraint checks for a given class.
 * @param klass     - klass handler
 * @param verifyAll - if flag is set, verifier provides full verification checks
 * @param error     - error message of verifier
 * @return Class verification result.
 * @note Assertion is raised if klass or error_message are equal to null.
 */
Verifier_Result
vf_verify_class_constraints( class_handler klass, unsigned verifyAll, char **error );

/**
 * Function provides final constraint checks for a given class.
 * @param error - error message of verifier
 * @note Assertion is raised if error_message is equal to null.
 */
void
vf_release_error_message( void *error );

/**
 * Function releases verify data in class loader.
 * @param data - verify data
 * @note Assertion is raised if data is equal to null.
 */
void
vf_release_verify_data( void *data );

#endif // _VERIFIER_H_
