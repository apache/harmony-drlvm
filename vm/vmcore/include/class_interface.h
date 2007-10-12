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

/**
 * Define class handler
 */
typedef struct Class_ * class_handler;

/**
 * Define field handler
 */
typedef struct Field_ * field_handler;

/**
 * Define method handler
 */
typedef struct Method_ * method_handler;

/**
 * Define class loader handler
 */
typedef struct ClassLoader_ * classloader_handler;

/**
 * Enum of constant pool tags
 */
typedef enum {
    _CONSTANT_Unknown               = 0,
    _CONSTANT_Utf8                  = 1,
    _CONSTANT_Integer               = 3,
    _CONSTANT_Float                 = 4,
    _CONSTANT_Long                  = 5,
    _CONSTANT_Double                = 6,
    _CONSTANT_Class                 = 7,
    _CONSTANT_String                = 8,
    _CONSTANT_Fieldref              = 9,
    _CONSTANT_Methodref             = 10,
    _CONSTANT_InterfaceMethodref    = 11,
    _CONSTANT_NameAndType           = 12
} ClassConstantPoolTags;

/**
 * Class interface
 */

/**
 * Function returns class major version.
 * @param klass - class handler
 * @return Class name bytes.
 * @note Assertion is raised if klass is equal to null.
 */
unsigned short
class_get_version( class_handler klass );

/** 
 * Function returns class name.
 * @param klass - class handler
 * @return Class name bytes.
 * @note Assertion is raised if klass is equal to null.
 */
const char *
class_get_name( class_handler klass );

/** 
 * Function returns class loader.
 * @param klass - class handler
 * @return Class class loader handler.
 * @note Assertion is raised if klass is equal to null.
 */
classloader_handler
class_get_class_loader( class_handler klass );

/** 
 * Function returns super class of current class.
 * @param klass - class handler
 * @return Super class of current class.
 * @note Assertion is raised if klass is equal to null.
 */
class_handler
class_get_super_class( class_handler klass );

/** 
 * Function checks if classes are equal.
 * @param klass1 - class handler
 * @param klass2 - class handler
 * @return If classes are equal returns <code>true</code>, else returns <code>false</code>.
 * @note Assertion is raised if klass1 or klass2 are equal to null.
 */
unsigned
class_is_same_class( class_handler klass1, class_handler klass2 );

/** 
 * Function checks if classes have the same package.
 * @param klass1 - class handler
 * @param klass2 - class handler
 * @return If classes have the same package returns <code>true</code>, else returns <code>false</code>.
 * @note Assertion is raised if klass1 or klass2 are equal to null.
 */
unsigned
class_is_same_package( class_handler klass1, class_handler klass2 );

/**
 * Function checks if current class is interface.
 * @param klass - class handler
 * @return If class is interface returns <code>true</code>, else returns <code>false</code>.
 * @note Assertion is raised if klass is equal to null.
 */
// FIXME - There is a macro class_is_interface in Class.h
unsigned
class_is_interface_( class_handler klass );

/**
 * Function checks if current class is array.
 * @param klass - class handler
 * @return If class is array returns <code>true</code>, else returns <code>false</code>.
 * @note Assertion is raised if klass is equal to null.
 */
unsigned
class_is_array( class_handler klass );

/**
 * Function checks if current class is final.
 * @param klass - class handler
 * @return If class is final returns <code>true</code>, else returns <code>false</code>.
 * @note Assertion is raised if klass is equal to null.
 */
// FIXME - There is a macro class_is_final in Class.h
unsigned
class_is_final_( class_handler klass );

/**
 * Function receives number of super interfaces of class.
 * @param klass - class handler
 * @return Number of super interfaces of class.
 * @note Assertion is raised if klass is equal to null.
 */
unsigned short
class_get_superinterface_number( class_handler klass );

/**
 * Function receives super interface of class.
 * @param klass - class handler
 * @param index - super interface number
 * @return Super interface of class.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
class_handler
class_get_superinterface( class_handler klass, unsigned short index );

/**
 * Function receives element class of array class.
 * @param klass - class handler
 * @return Element class of array class.
 * @note Assertion is raised if klass is equal to null or isn't array class.
 */
class_handler
class_get_array_element_class( class_handler klass );

/**
 * Function checks if class extends current class with given name.
 * @param klass      - checked klass
 * @param super_name - parent class name
 * @return If given class extends current class with given name,
 *         function returns 1, else function returns 0.
 * @note Assertion is raised if <i>klass</i> or <i>super_name</i> are equal to null.
 */
unsigned
class_is_extending_class( class_handler klass, char *super_name );

/**
 * Function returns number of methods for current class.
 * @param klass - class handler
 * @return Number of methods for class.
 * @note Assertion is raised if klass is equal to null.
 */
unsigned short
class_get_method_number( class_handler klass );

/** 
 * Function returns method of current class.
 * @param klass - class handler
 * @param index - method index
 * @return Method handler.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
method_handler
class_get_method( class_handler klass, unsigned short index );

/**
 * Constant pool inteface
 */

/** 
 * Function returns class constant pool size.
 * @param klass - class handler
 * @return constant pool size
 * @note Assertion is raised if klass is equal to null.
 */
unsigned short
class_get_cp_size( class_handler klass );

/** 
 * Function returns constant pool entry tag.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return constant pool entry tag
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned char
class_get_cp_tag( class_handler klass, unsigned short index );

/** 
 * Function returns class name entry index in constant pool.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return class name entry index
 * @note Function is legal only for constant pool entry with CONSTANT_Class tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned short
class_get_cp_class_name_index( class_handler klass, unsigned short index );

/** 
 * Function returns class name entry index in constant pool.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return class name entry index
 * @note Function is legal for constant pool entry with 
 *       CONSTANT_Fieldref, CONSTANT_Methodref and CONSTANT_InterfaceMethodref tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned short
class_get_cp_ref_class_index( class_handler klass, unsigned short index );

/** 
 * Function returns name_and_type entry index in constant pool.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return name_and_type entry index
 * @note Function is legal for constant pool entry with 
 *       CONSTANT_Fieldref, CONSTANT_Methodref and CONSTANT_InterfaceMethodref tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned short
class_get_cp_ref_name_and_type_index( class_handler klass, unsigned short index );

/** 
 * Function returns string entry index in constant pool.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return string entry index
 * @note Function is legal for constant pool entry with CONSTANT_String tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned short
class_get_cp_string_index( class_handler klass, unsigned short index );

/** 
 * Function returns name entry index in constant pool.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return name entry index
 * @note Function is legal for constant pool entry with CONSTANT_NameAndType tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned short
class_get_cp_name_index( class_handler klass, unsigned short index );

/** 
 * Function returns descriptor entry index in constant pool.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return descriptor entry index
 * @note Function is legal for constant pool entry with CONSTANT_NameAndType tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
unsigned short
class_get_cp_descriptor_index( class_handler klass, unsigned short index );

/** 
 * Function returns bytes for UTF8 constant pool entry.
 * @param klass - class handler
 * @param index - constant pool entry index
 * @return bytes for UTF8 constant pool entry
 * @note Function is legal for constant pool entry with CONSTANT_UTF8 tags.
 * @note Assertion is raised if klass is equal to null or index is out of range.
 */
const char *
class_get_cp_utf8_bytes( class_handler klass, unsigned short index );

/**
 * Function sets verify data to a given class.
 * @param klass     - class handler
 * @param data      - verify data
 * @note Assertion is raised if class is equal to null.
 * @note Function makes non thread save operation and 
 *       must be called in thread safe point.
 */
void
class_set_verify_data_ptr( class_handler klass, void *data );

/**
 * Function returns verify data for a given class.
 * @param klass - class handler
 * @return Verify data for a given class.
 * @note Assertion is raised if klass is equal to null.
 */
void *
class_get_verify_data_ptr( class_handler klass );

/**
 * Function resolves class nonstatic method for constant pool entry.
 *
 * @param klass - class handle
 * @param index - constant pool entry index
 * @param exc   - pointer to exception
 *
 * @return Return nonstatic method resolved for constant pool entry.
 */
method_handler
class_resolve_method( class_handler klass, unsigned short index );

/**
 * Function resolves class nonstatic field for constant pool entry.
 *
 * @param klass - class handle
 * @param index - constant pool entry index
 * @param exc   - pointer to exception
 *
 * @return Return nonstatic field resolved for constant pool entry.
 */
field_handler
class_resolve_nonstatic_field( class_handler klass, unsigned short index );

/**
 * Method interface
 */

/**
 * Function returns a class in which the method is declared.
 * @param method - method handler
 * @return Return a class in which the method is declared.
 * @note Assertion is raised if <i>method</i> is equal to null.
 */
class_handler
method_get_class( method_handler hmethod );

/**
 * Function returns method name.
 * @param method - method handler
 * @return Method name bytes.
 * @note Assertion is raised if method is equal to null.
 */
const char *
method_get_name( method_handler method );

/**
 * Function returns method descriptor.
 * @param method - method handler
 * @return Method descriptor bytes.
 * @note Assertion is raised if method is equal to null.
 */
const char *
method_get_descriptor( method_handler method );

/**
 * Function returns method code length.
 * @param method - method handler
 * @return Method code length.
 * @note Assertion is raised if method is equal to null.
 */
unsigned
method_get_code_length( method_handler method );

/**
 * Function returns method bytecode array.
 * @param method - method handler
 * @return Method bytecode array.
 * @note Assertion is raised if method is equal to null.
 */
unsigned char *
method_get_bytecode( method_handler method );

/**
 * Function returns maximal local variables number of method.
 * @param method - method handler
 * @return Maximal local variables number of method.
 * @note Assertion is raised if method is equal to null.
 */
unsigned short
method_get_max_local( method_handler method );

/**
 * Function returns maximal stack deep of method.
 * @param method - method handler
 * @return Maximal stack deep of method.
 * @note Assertion is raised if method is equal to null.
 */
unsigned short
method_get_max_stack( method_handler method );

/**
 * Function checks if method is static.
 * @param method - method handler
 * @return If method is static, function returns <code>true</code>,
 *         else returns <code>false</code>.
 * @note Assertion is raised if method is equal to null.
 */
unsigned
method_is_static( method_handler method );

/**
 * Function checks if a given method is protected.
 *
 * @param method - method handle
 *
 * @return Return <code>TRUE</code> if a given method is protected.
 *
 * @note Assertion is raised if <i>method</i> is equal to null.
 */
unsigned
method_is_protected( method_handler method );

/**
 * Method exception handler
 */

/**
 * Function returns number of method exception handlers.
 * @param method - method handler
 * @return Number of method exception handlers.
 * @note Assertion is raised if method is equal to null.
 */
unsigned short
method_get_exc_handler_number( method_handler method );

/**
 * Function obtains method exception handler info.
 * @param method     - method handler
 * @param index      - exception handler index number
 * @param start_pc   - resulting pointer to exception handler start program count
 * @param end_pc     - resulting pointer to exception handler end program count
 * @param handler_pc - resulting pointer to exception handler program count
 * @param catch_type - resulting pointer to constant pool entry index
 * @note Assertion is raised if method is equal to null or
 *       exception handler index is out of range or
 *       any pointer is equal to null.
 */
void
method_get_exc_handler_info( method_handler method, unsigned short index,
                             unsigned short *start_pc, unsigned short *end_pc,
                             unsigned short *handler_pc, unsigned short *catch_type );

/**
 * Gets number of exceptions a method can throw.
 * Parameter <i>hmethod</i> must not equal to <code>NULL</code>.
 *
 * @param hmethod   method handle
 *
 * @return          number of exceptions
 */
unsigned short
method_get_number_exc_method_can_throw( method_handler hmethod );

/**
 * Gets name of exception a method can throw.
 * Parameter <i>hmethod</i> must not equal to <code>NULL</code>.
 * If parameter <i>index</i> is out of range, returns <code>NULL</code>.
 *
 * @param hmethod   method handle
 * @param index     index of exception
 *
 * @return          name of exception
 */
const char *
method_get_exc_method_can_throw( method_handler hmethod, unsigned short index );


/**
 * Gets StackMapTable attribute.
 * Parameter <i>hmethod</i> must not equal to <code>NULL</code>.
 * If parameter <i>index</i> is out of range, returns <code>NULL</code>.
 *
 * @param hmethod   method handle
 *
 * @return          StackMapTable bytes
 */
unsigned char *
method_get_stackmaptable( method_handler hmethod );



/**
 * Class loader interface
 */

/**
 * Function sets verify data in class loader.
 * @param classloader - class loader handler
 * @param data        - verify data
 * @note Assertion is raised if classloader is equal to null.
 * @note Function makes non thread save operation and 
 *       must be called in thread safe point.
 */
void
cl_set_verify_data_ptr( classloader_handler classloader, void *data );

/**
 * Function returns verify data in class loader.
 * @param classloader - class loader handler
 * @return Verify data in class loader.
 * @note Assertion is raised if classloader is equal to null.
 */
void *
cl_get_verify_data_ptr( classloader_handler classloader );

/**
 * Function locks class loader.
 * @param classloader - class loader handler
 * @note Assertion is raised if classloader is equal to null.
 */
void
cl_acquire_lock( classloader_handler classloader );

/**
 * Function releases class loader.
 * @param classloader - class loader handler
 * @note Assertion is raised if classloader is equal to null.
 */
void
cl_release_lock( classloader_handler classloader );

/**
 * Function returns loaded class in class loader.
 * @param classloader - class loader handler
 * @param name        - class name
 * @return Loaded class in classloader or null if class isn't loaded in class loader.
 * @note Assertion is raised if classloader or name are equal to null.
 */
class_handler
cl_get_class( classloader_handler classloader, const char *name );

/**
 * Function returns loaded class in class loader.
 * @param classloader - class loader handler
 * @param name        - class name
 * @return Loaded class in classloader if class isn't loaded in class loader 
 *         function loads it.
 * @note Assertion is raised if classloader or name are equal to null.
 */
class_handler
cl_load_class( classloader_handler classloader, const char *name );

/**
 * Function checks if the field is protected.
 * @param field - field handler
 * @return Returns <code>TRUE</code> if the field is protected.
 */
unsigned
field_is_protected( field_handler field );
