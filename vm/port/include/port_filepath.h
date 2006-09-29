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
* @author Alexey V. Varlamov
* @version $Revision: 1.1.2.1.4.3 $
*/  

#ifndef _PORT_FILEPATH_H_
#define _PORT_FILEPATH_H_

#include "port_general.h"
#include <apr_pools.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * File system separators definitions.
 * @non_apr
 */
#ifdef PLATFORM_POSIX
#   define PORT_FILE_SEPARATOR '/'
#   define PORT_PATH_SEPARATOR ':'
#   define PORT_FILE_SEPARATOR_STR "/"
#   define PORT_PATH_SEPARATOR_STR ":"
#elif PLATFORM_NT
#   define PORT_FILE_SEPARATOR '\\'
#   define PORT_PATH_SEPARATOR ';'
#   define PORT_FILE_SEPARATOR_STR "\\"
#   define PORT_PATH_SEPARATOR_STR ";"
#endif
/**
* Sticks together filepath parts.
* @non_apr
*/
APR_DECLARE(char *) port_filepath_merge(const char* root,
                          const char* trail,
                          apr_pool_t* pool);


/**
* Returns canonical form of the specified path.
* @non_apr
*/
APR_DECLARE(char*) port_filepath_canonical(const char* original,
                                      apr_pool_t* pool);

#ifdef __cplusplus
}
#endif
#endif /*_PORT_FILEPATH_H_*/
