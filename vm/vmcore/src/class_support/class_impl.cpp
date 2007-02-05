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
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"

#include "class_interface.h"
#include "Class.h"
#include "classloader.h"
#include "environment.h"

/**
 * Function returns class major version.
 */
unsigned short
class_get_version( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->get_version();
} // class_get_version

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
 * Function checks if classes have the same package.
 */
unsigned
class_is_same_package(class_handler klass1, class_handler klass2)
{
    assert( klass1 );
    assert( klass2 );
    Class* clss1 = (Class*)klass1;
    Class* clss2 = (Class*)klass2;
    return (clss1->get_package() == clss2->get_package()) ? 1 : 0;
} // class_is_same_package

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
 * Function checks if class extends current class with given name.
 */
unsigned
class_is_extending_class( class_handler klass, char *super_name )
{
    assert( klass );
    assert( super_name );
    for( Class *clss = (Class*)klass; clss; clss = clss->get_super_class() ) {
        if( !strcmp( clss->get_name()->bytes, super_name ) ) {
            // found class with given name
            return 1;
        }
    }
    return 0;
} // class_is_extending_class

/**
 * Function returns number of methods for current class.
 */
unsigned short
class_get_method_number( class_handler klass )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->get_number_of_methods();
} // class_get_method_number

/** 
 * Function returns method of current class.
 */
method_handler
class_get_method( class_handler klass, unsigned short index )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return (method_handler)clss->get_method(index);
} // class_get_method

/** 
 * Function returns class constant pool size.
 */
unsigned short
class_get_cp_size( class_handler klass )
{
    assert(klass);
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_size();
} // class_get_cp_size

/** 
 * Function returns constant pool entry tag.
 */
unsigned char
class_get_cp_tag( class_handler klass, unsigned short index )
{
    assert( klass );
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_tag(index);
} // class_get_cp_tag

/** 
 * Function returns class name entry index in constant pool.
 */
unsigned short
class_get_cp_class_name_index( class_handler klass, unsigned short index )
{
    assert( klass );
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_class_name_index(index);
} // class_get_cp_class_name_index

/** 
 * Function returns class name entry index in constant pool.
 */
unsigned short
class_get_cp_ref_class_index( class_handler klass, unsigned short index )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->get_constant_pool().get_ref_class_index(index);
} // class_get_cp_ref_class_index

/** 
 * Function returns name_and_type entry index in constant pool.
 */
unsigned short
class_get_cp_ref_name_and_type_index( class_handler klass, unsigned short index )
{
    assert( klass );
    Class *clss = (Class*)klass;
    return clss->get_constant_pool().get_ref_name_and_type_index(index);
} // class_get_cp_ref_name_and_type_index

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
 * Function returns name entry index in constant pool.
 */
unsigned short
class_get_cp_name_index( class_handler klass, unsigned short index )
{
    assert( klass );
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_name_and_type_name_index(index);
} // class_get_cp_name_index

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
 * Function returns bytes for UTF8 constant pool entry.
 */
const char*
class_get_cp_utf8_bytes( class_handler klass, unsigned short index )
{
    assert( klass );
    Class* clss = (Class*)klass;
    return clss->get_constant_pool().get_utf8_chars(index);
} // class_get_cp_utf8_bytes

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
 * Function resolves class nonstatic method for constant pool entry.
 */
method_handler
class_resolve_method( class_handler klass, unsigned short index )
{
    assert(klass);
    assert(index);
    Class* clss = (Class*)klass;
    Method* method = clss->_resolve_method(VM_Global_State::loader_env, index);
    return (method_handler)method;
} // class_resolve_method

/**
 * Function resolves class nonstatic field for constant pool entry.
 */
field_handler
class_resolve_nonstatic_field( class_handler klass, unsigned short index )
{
    assert( klass );
    assert( index );
    Class *clss = (Class*)klass;
    Field *field = class_resolve_nonstatic_field( clss, index );
    return (field_handler)field;
} // class_resolve_nonstatic_field

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
method_get_code_length( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->get_byte_code_size();
} // method_get_code_length

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
 * Function checks if method is protected.
 */
unsigned
method_is_protected( method_handler hmethod )
{
    assert( hmethod );
    Method *method = (Method*)hmethod;
    return method->is_protected();
} // method_is_protected

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
 * Function obtains method exception handler info.
 */
void
method_get_exc_handler_info( method_handler hmethod, unsigned short index,
                             unsigned short *start_pc, unsigned short *end_pc,
                             unsigned short *handler_pc, unsigned short *catch_type )
{
    assert( hmethod );
    assert( start_pc );
    assert( end_pc );
    assert( handler_pc );
    assert( catch_type );
    Method *method = (Method*)hmethod;
    assert( index < method->num_bc_exception_handlers() );
    Handler *handler = method->get_bc_exception_handler_info( index );
    *start_pc = (unsigned short)handler->get_start_pc();
    *end_pc =   (unsigned short)handler->get_end_pc();
    *handler_pc = (unsigned short)handler->get_handler_pc();
    *catch_type = (unsigned short)handler->get_catch_type_index();
    return;
} // method_get_exc_handler_info

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

/**
 * Function sets verify data in class loader.
 */
void
cl_set_verify_data_ptr( classloader_handler classloader, void *data )
{
    assert(classloader);
    ClassLoader *cl = (ClassLoader*)classloader;
    cl->SetVerifyData( data );
    return;
} // cl_set_verify_data_ptr

/**
 * Function returns verify data in class loader.
 */
void *
cl_get_verify_data_ptr( classloader_handler classloader )
{
    assert(classloader);
    ClassLoader *cl = (ClassLoader*)classloader;
    return cl->GetVerifyData();
} // cl_get_verify_data_ptr

/**
 * Function locks class loader.
 */
void
cl_acquire_lock( classloader_handler classloader )
{
    assert(classloader);
    ClassLoader *cl = (ClassLoader*)classloader;
    cl->Lock();
    return;
} // cl_acquire_lock

/**
 * Function releases class loader.
 */
void
cl_release_lock( classloader_handler classloader )
{
    assert(classloader);
    ClassLoader *cl = (ClassLoader*)classloader;
    cl->Unlock();
    return;
} // cl_release_lock

/**
 * Function returns loaded class in class loader.
 */
class_handler
cl_get_class( classloader_handler classloader, const char *name )
{
    assert(classloader);
    assert(name);
    ClassLoader *cl = (ClassLoader*)classloader;
    Global_Env *env = VM_Global_State::loader_env;
    String *class_name = env->string_pool.lookup( name );
    return (class_handler)cl->LookupClass( class_name );
} // cl_get_class

/**
 * Function returns loaded class in class loader.
 */
class_handler
cl_load_class( classloader_handler classloader, const char *name )
{
    assert(classloader);
    assert(name);
    ClassLoader *cl = (ClassLoader*)classloader;
    Global_Env *env = VM_Global_State::loader_env;
    String *class_name = env->string_pool.lookup( name );
    Class *klass = cl->LoadClass(env, class_name);
    return (class_handler)klass;
} // cl_load_class


/**
 * Function checks if a given field is protected.
 */
unsigned
field_is_protected( field_handler hfield )
{
    assert( hfield );
    Field *field = (Field*)hfield;
    return field->is_protected();
} // field_is_protected


