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
#ifndef __CLASS_INTERFACE_H__
#define __CLASS_INTERFACE_H__

//#include "open/types.h"
//#include "open/common.h"

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
 * Class loader interface
 */
#if 0
/**
 * Function sets verify data in class loader.
 * @param classloader - class loader handler
 * @param data        - verify data
 * @note Assertion is raised if classloader is equal to null.
 * @note Function makes non thread save operation and 
 *       must be called in thread safe point.
 */
void
cl_set_verify_data_ptr( ClassLoaderHandle classloader, void *data );

/**
 * Function returns verify data in class loader.
 * @param classloader - class loader handler
 * @return Verify data in class loader.
 * @note Assertion is raised if classloader is equal to null.
 */
void *
cl_get_verify_data_ptr( ClassLoaderHandle classloader );

/**
 * Function locks class loader.
 * @param classloader - class loader handler
 * @note Assertion is raised if classloader is equal to null.
 */
void
cl_acquire_lock( ClassLoaderHandle classloader );

/**
 * Function releases class loader.
 * @param classloader - class loader handler
 * @note Assertion is raised if classloader is equal to null.
 */
void
cl_release_lock( ClassLoaderHandle classloader );

/**
 * Function returns loaded class in class loader.
 * @param classloader - class loader handler
 * @param name        - class name
 * @return Loaded class in classloader or null if class isn't loaded in class loader.
 * @note Assertion is raised if classloader or name are equal to null.
 */
Class_Handle
cl_get_class( ClassLoaderHandle classloader, const char *name );

/**
 * Function returns loaded class in class loader.
 * @param classloader - class loader handler
 * @param name        - class name
 * @return Loaded class in classloader if class isn't loaded in class loader 
 *         function loads it.
 * @note Assertion is raised if classloader or name are equal to null.
 */
Class_Handle
cl_load_class( ClassLoaderHandle classloader, const char *name );

#endif

#endif
