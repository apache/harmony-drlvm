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
#include <assert.h>
#include "open/vm_method_access.h"
#include "open/vm_field_access.h"
#include "open/vm_class_manipulation.h"
#include "open/vm_class_loading.h"
#include "open/vm_util.h" // for VM_Global_State ???!!!
#include "Class.h"
#include "class_member.h"
#include "classloader.h"

unsigned short class_cp_get_size(Class_Handle klass)
{
    assert(klass);
    return klass->get_constant_pool().get_size();
} // class_cp_get_size

unsigned char class_cp_get_tag(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_tag(index);
} // class_cp_get_tag

const char* class_cp_get_utf8_bytes(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_utf8_chars(index);
} // class_cp_get_utf8_bytes

unsigned short class_cp_get_class_name_index(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_class_name_index(index);
} // class_cp_get_class_name_index

unsigned short class_cp_get_descriptor_index(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_name_and_type_descriptor_index(index);
} // class_cp_get_descriptor_index

unsigned short class_cp_get_ref_name_and_type_index(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_ref_name_and_type_index(index);
} // class_cp_get_ref_name_and_type_index

unsigned short class_cp_get_ref_class_index(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_ref_class_index(index);
} // class_cp_get_ref_class_index

unsigned short class_cp_get_name_index(Class_Handle klass, unsigned short index)
{
    assert(klass);
    return klass->get_constant_pool().get_name_and_type_name_index(index);
} // class_cp_get_name_index

Method_Handle class_resolve_method(Class_Handle klass, unsigned short index)
{
    assert(klass);
    assert(index);
    return klass->_resolve_method(VM_Global_State::loader_env, index);
} // class_resolve_method

BOOLEAN class_is_same_package(Class_Handle klass1, Class_Handle klass2)
{
    assert(klass1);
    assert(klass2);
    return (klass1->get_package() == klass2->get_package()) ? TRUE : FALSE;
} // class_is_same_package

unsigned short class_get_method_number(Class_Handle klass)
{
    assert(klass);
    return klass->get_number_of_methods();
} // class_get_method_number

Method_Handle class_get_method(Class_Handle klass, unsigned short index)
{
    assert(klass);
    if(index >= klass->get_number_of_methods())
    {
        assert(index < klass->get_number_of_methods());
        return NULL;
    }
    return klass->get_method(index);
} // class_get_method_number

unsigned char* method_get_stackmaptable(Method_Handle hmethod)
{
    assert(hmethod);
    return hmethod->get_stackmap();
} // method_get_stackmaptable

BOOLEAN method_is_protected(Method_Handle hmethod)
{
    assert(hmethod);
    Method *method = (Method*)hmethod;
    return hmethod->is_protected();
} // method_is_protected

BOOLEAN field_is_protected(Field_Handle hfield)
{
    assert(hfield);
    return hfield->is_protected();
} // field_is_protected

void class_loader_set_verifier_data_ptr(ClassLoaderHandle classloader, void* data)
{
    assert(classloader);
    classloader->SetVerifyData(data);
} // class_loader_set_verifier_data_ptr

void* class_loader_get_verifier_data_ptr(ClassLoaderHandle classloader)
{
    assert(classloader);
    return classloader->GetVerifyData();
} // class_loader_get_verifier_data_ptr

void class_loader_lock(ClassLoaderHandle classloader)
{
    assert(classloader);
    classloader->Lock();
} // class_loader_lock

void class_loader_unlock(ClassLoaderHandle classloader)
{
    assert(classloader);
    classloader->Unlock();
} // class_loader_unlock

Class_Handle class_loader_lookup_class(ClassLoaderHandle classloader, const char* name)
{
    assert(classloader);
    assert(name);
    Global_Env *env = VM_Global_State::loader_env;
    String* class_name = env->string_pool.lookup( name );
    return classloader->LookupClass(class_name);
} // class_loader_lookup_class

Class_Handle class_loader_load_class(ClassLoaderHandle classloader, const char* name)
{
    assert(classloader);
    assert(name);
    Global_Env *env = VM_Global_State::loader_env;
    String* class_name = env->string_pool.lookup(name);
    return classloader->LoadClass(env, class_name);
} // class_loader_load_class

#if 0

/**
 * Function returns class name.
 */
const char *
class_get_name( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->get_name()->bytes;
} // class_get_name_bytes

/** 
 * Function returns class loader.
 */
classloader_handler
class_get_class_loader( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return (classloader_handler)clss->get_class_loader();
} // class_get_classloader

/** 
 * Function returns super class of current class.
 */
class_handler
class_get_super_class( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return (class_handler)clss->get_super_class();
} // class_get_super_class

/** 
 * Function checks if classes are equal.
 */
unsigned
class_is_same_class( class_handler klass1, class_handler klass2 )
{
    assert( klass1 );
    assert( klass2 );
    return (klass1 == klass2) ? 1 : 0;
} // class_is_same_class

/**
 * Function checks if current class is interface.
 */
// FIXME - There is a macro class_is_interface in Class.h
unsigned
class_is_interface_( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->is_interface() ? 1 : 0;
} // class_is_interface_

/**
 * Function checks if current class is array.
 */
unsigned
class_is_array( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->is_array();
} // class_is_array

/**
 * Function checks if current class is final.
 */
// FIXME - There is a macro class_is_final in Class.h
unsigned
class_is_final_( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->is_final() ? 1 : 0;
} // class_is_final_

/**
 * Function receives number of super interfaces of class.
 */
unsigned short
class_get_superinterface_number(class_handler klass)
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->get_number_of_superinterfaces();
} // class_get_superinterface_number

/**
 * Function receives super interface of class.
 */
class_handler
class_get_superinterface( class_handler klass, unsigned short index )
{
    assert( klass );
    Class *clss = (Class*)klass;
    assert(index < clss->get_number_of_superinterfaces());
    return (class_handler)clss->get_superinterface(index);
} // class_get_superinterface

/**
 * Function receives element class of array class.
 * @param klass - class handler
 * @return Element class of array class.
 * @note Assertion is raised if klass is equal to null or isn't array class.
 */
class_handler
class_get_array_element_class( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    assert(clss->is_array());
    return (class_handler)clss->get_array_element_class();
} // class_get_array_element_class

/** 
 * Function returns string entry index in constant pool.
 */
unsigned short
class_get_cp_string_index(class_handler klass, unsigned short index)
{
    assert(klass);
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_string_index(index);
} // class_get_cp_string_index

/** 
 * Function returns descriptor entry index in constant pool.
 */
unsigned short
class_get_cp_descriptor_index( class_handler klass, unsigned short index )
{
    assert( klass );
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_name_and_type_descriptor_index(index);
} // class_get_cp_descriptor_index

/**
 * Function sets verify data to a given class.
 */
void class_set_verify_data_ptr(class_handler klass, void* data)
{
    assert(klass);
    Class* clss = (Class*)klass;
    clss->set_verification_data(data);
} // class_set_verify_data_ptr

/**
 * Function returns verify data for a given class.
 */
void* class_get_verify_data_ptr(class_handler klass)
{
    assert(klass);
    Class* clss = (Class*)klass;
    return clss->get_verification_data();
} // class_get_verify_data_ptr

/**
 * Function returns a class in which the method is declared.
 */
class_handler
method_get_class( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return (class_handler)method->get_class();
} // method_get_class

/**
 * Function returns method name.
 */
const char *
method_get_name( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->get_name()->bytes;
} // method_get_name_bytes

/**
 * Function returns method description.
 */
const char *
method_get_descriptor( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->get_descriptor()->bytes;
} // method_get_description_bytes

/**
 * Function returns method code length.
 */
unsigned
method_get_bytecode_length( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->get_byte_code_size();
} // method_get_bytecode_length

/**
 * Function returns method bytecode array.
 */
unsigned char *
method_get_bytecode( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return (unsigned char*)method->get_byte_code_addr();
} // method_get_bytecode

/**
 * Function returns maximal local variables number of method.
 */
unsigned short
method_get_max_local( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return (unsigned short)method->get_max_locals();
} // method_get_max_local

/**
 * Function returns maximal stack deep of method.
 */
unsigned short
method_get_max_stack( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return (unsigned short)method->get_max_stack();
} // method_get_max_stack

/**
 * Function checks if method is static.
 */
unsigned
method_is_static( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->is_static();
} // method_is_static

/**
 * Function returns number of method exception handlers.
 */
unsigned short
method_get_exc_handler_number( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return (unsigned short)method->num_bc_exception_handlers();
} // method_get_exc_handler_number

/**
 * Gets number of exceptions a method can throw.
 */
unsigned short
method_get_number_exc_method_can_throw( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->num_exceptions_method_can_throw();
} // method_get_number_exc_method_can_throw

/**
 * Gets name of exception a method can throw.
 */
const char *
method_get_exc_method_can_throw( method_handler hmethod,
                                 unsigned short index)
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->get_exception_name( index )->bytes;
} // method_get_exc_method_can_throw

#endif
