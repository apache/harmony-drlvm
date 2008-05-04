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
#include <stdio.h>
#include "verifier.h"
#include "ver_utils.h"
#include "../java5/context_5.h"
#include "../java6/context_6.h"
#include "time.h"

#include "open/vm_class_manipulation.h"
#include "open/vm_method_access.h"
#include "open/vm_class_loading.h"

/**
* Provides initial java-5 verification of class.
*
* If when verifying the class a check of type "class A must be assignable to class B" needs to be done 
* and either A or B is not loaded at the moment then a constraint 
* "class A must be assignable to class B" is recorded into the classloader's data
*/
vf_Result
vf_verify5_class(Class_Handle klass, unsigned verifyAll, char** error)
{
    int index;
    vf_Result result = VF_OK;

    // Create context
    SharedClasswideData classwide(klass);
    vf_Context_5 context(classwide);

    // Verify method
    for( index = 0; index < class_get_method_number( klass ); index++ ) {
        result = context.verify_method(class_get_method( klass, index ));

        if (result != VF_OK) {
            vf_create_error_message(class_get_method(klass, index), context, error);
            break;
        }
    }

    /**
    * Set method constraints
    */
    context.set_class_constraints();

    return result;
} // vf_verify5_class


/**
* Function provides initial java-6 verification of class.
*
* If when verifying the class a check of type "class A must be assignable to class B" needs to be done 
* and either A or B is not loaded at the moment then a constraint 
* "class A must be assignable to class B" is recorded into the classloader's data
*/
vf_Result
vf_verify6_class(Class_Handle klass, unsigned verifyAll, char **error )
{
    int index;
    vf_Result result = VF_OK;

    // Create contexts
    SharedClasswideData classwide(klass);
    vf_Context_5 context5(classwide);
    vf_Context_6 context6(classwide);

    bool skip_java6_verification_attempt = false;

    // Verify method
    for( index = 0; index < class_get_method_number( klass ); index++ ) {
        Method_Handle method = class_get_method( klass, index );

        //try Java6 verifying (using StackMapTable attribute)
        if( !skip_java6_verification_attempt || method_get_stackmaptable(method) ) {
            result = context6.verify_method(method);

            if (result != VF_OK) {
                //skip Java6 attempts for further methods unless they have StackMapTable
                skip_java6_verification_attempt = true;
                if (result == VF_ErrorStackmap) {
                    //corrupted StackMapTable ==> throw an Error?
                    vf_create_error_message(method, context6, error);
                    return result;
                }
            }
        }

        if( result != VF_OK ) {
            //try Java5 verifying
            result = context5.verify_method(method);
            if (result != VF_OK) {
                vf_create_error_message(method, context5, error);
                return result;
            }
        }
    }

    /**
    * Set method constraints
    */
    context5.set_class_constraints();

    return result;
} // vf_verify6_class


/**
* Provides initial verification of a class.
*
* If when verifying the class a check of type "class A must be assignable to class B" needs to be done 
* and either A or B is not loaded at the moment then a constraint 
* "class A must be assignable to class B" is recorded into the classloader's data
*/
vf_Result
vf_verify_class( Class_Handle klass, unsigned verifyAll, char **error ) {
    return class_get_version(klass) >= 50 ? vf_verify6_class(klass, verifyAll, error) : vf_verify5_class(klass, verifyAll, error);
}

/**
* Function verifies all the constraints "class A must be assignable to class B"
* that are recorded into the classloader for the given class
* If some class is not loaded yet -- load it now
*/
vf_Result
vf_verify_class_constraints(Class_Handle klass, unsigned verifyAll, char** error)
{

    // get class loader of current class
    Class_Loader_Handle class_loader = class_get_class_loader(klass);

    // get class loader verify data
    vf_ClassLoaderData_t *cl_data =
        (vf_ClassLoaderData_t*)class_loader_get_verifier_data_ptr(class_loader);

    // check class loader data
    if( cl_data == NULL ) {
        // no constraint data
        return VF_OK;
    }

    // get class hash and memory pool
    vf_Hash *hash = cl_data->hash;

    // get constraints for class
    vf_HashEntry_t *hash_entry = hash->Lookup( class_get_name( klass ) );
    if( !hash_entry || !hash_entry->data_ptr ) {
        // no constraint data
        return VF_OK;
    }

    // check method constraints
    vf_TypeConstraint *constraint = (vf_TypeConstraint*)hash_entry->data_ptr;
    for( ; constraint; constraint = constraint->next )
    {
        vf_Result result = vf_force_check_constraint( klass, constraint );
        if( result != VF_OK ) {
            vf_create_error_message(klass, constraint, error);
            return result;
        }
    }

    return VF_OK;
} // vf_verify_method_constraints


void vf_release_error_message(void* error)
{
    tc_free(error);
}

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

/**
 * Creates error message to be reported with verify error
 */
void vf_create_error_message(Method_Handle method, vf_Context_Base context, char** error_msg)
{
    char pass[12], instr[12];
    sprintf(pass, "%d", context.pass);
    sprintf(instr, "%d", context.processed_instruction);
    const char* cname = class_get_name(method_get_class(method));
    const char* mname = method_get_name(method);
    const char* mdesc = method_get_descriptor(method);
    const char *format = "%s.%s%s, pass: %s, instr: %s, reason: %s";
    unsigned msg_len = strlen(cname) + strlen(mname) + strlen(mdesc)
        + strlen(pass) + strlen(instr) + strlen(context.error_message) +
        strlen(format);
    *error_msg = (char*)tc_malloc(msg_len);
    if(*error_msg != NULL) {
        sprintf(*error_msg, format,
            cname, mname, mdesc, pass, instr, context.error_message);
    }
}

void vf_create_error_message(Class_Handle klass, vf_TypeConstraint* constraint, char** error_msg)
{
    const char* cname = class_get_name(klass);
    const char *format = "constraint check failed, class: %s, source: %s, target: %s";
    unsigned msg_len = strlen(cname) +
        + strlen(constraint->source)
        + strlen(constraint->target) + strlen(format);
    *error_msg = (char*)tc_malloc(msg_len);
    sprintf(*error_msg, format,
        cname, constraint->source, constraint->target);
}
