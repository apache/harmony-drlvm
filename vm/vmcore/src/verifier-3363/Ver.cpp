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
 * @author Mikhail Loenko, Vladimir Molotkov
 */  



#include <iostream>

using namespace std;

#include "verifier.h"
#include "context.h"
#include "time.h"

using namespace CPVerifier;
char  err_message[5000];

/**
* Function provides initial verification of class.
*
* If when verifying the class a check of type "class A must be assignable to class B" needs to be done 
* and either A or B is not loaded at the moment then a constraint 
* "class A must be assignable to class B" is recorded into the classloader's data
*/
vf_Result
vf_verify_class( class_handler klass, unsigned verifyAll, char **error )
{
    int index;
    vf_Result result = VF_OK;

    // Create context
    vf_Context_t context(klass);

    // Verify method
    for( index = 0; index < class_get_method_number( klass ); index++ ) {
        result = context.verify_method(class_get_method( klass, index ));

        if (result != VF_OK) {
            *error = &(err_message[0]);
            method_handler method = class_get_method( klass, index );
            sprintf(*error, "%s/%s%s, pass: %d, instr: %d, reason: %s", class_get_name( klass ), method_get_name( method ), 
                method_get_descriptor( method ), context.pass, context.processed_instruction, context.error_message );
            break;
        }
    }

    /**
    * Set method constraints
    */
    context.set_class_constraints();

    return result;
} // vf_verify_class


/**
* Function verifies all the constraints "class A must be assignable to class B"
* that are recorded into the classloader for the given class
* If some class is not loaded yet -- load it now
*/
vf_Result
vf_verify_class_constraints( class_handler klass, unsigned verifyAll, char **error )
{

    // get class loader of current class
    classloader_handler class_loader = class_get_class_loader( klass );

    // get class loader verify data
    vf_ClassLoaderData_t *cl_data =
        (vf_ClassLoaderData_t*)cl_get_verify_data_ptr( class_loader );

    // check class loader data
    if( cl_data == NULL ) {
        // no constraint data
        return VF_OK;
    }

    // get class hash and memory pool
    vf_Hash_t *hash = cl_data->hash;

    // get constraints for class
    vf_HashEntry_t *hash_entry = hash->Lookup( class_get_name( klass ) );
    if( !hash_entry || !hash_entry->data ) {
        // no constraint data
        return VF_OK;
    }

    // check method constraints
    vf_TypeConstraint_t *constraint = (vf_TypeConstraint_t*)hash_entry->data;
    for( ; constraint; constraint = constraint->next )
    {
        vf_Result result = vf_force_check_constraint( klass, constraint );
        if( result != VF_OK ) {
            *error = &(err_message[0]);
            sprintf(*error, "constraint check failed, class: %s, source: %s, target: %s", class_get_name( klass ), constraint->source, constraint->target);
            return result;
        }
    }

    return VF_OK;
} // vf_verify_method_constraints


/**
* Function releases verify data in class loader (used to store constraints)
*/
void
vf_release_verify_data( void *data )
{
    vf_ClassLoaderData_t *cl_data = (vf_ClassLoaderData_t*)data;

    delete cl_data->string;
    delete cl_data->hash;
    delete cl_data->pool;
} // vf_release_verify_data

