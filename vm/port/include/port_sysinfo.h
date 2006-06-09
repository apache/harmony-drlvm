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
* @author Alexey V. Varlamov
* @version $Revision: 1.1.2.1.4.3 $
*/  

#ifndef _PORT_SYSINFO_H_
#define _PORT_SYSINFO_H_

#include "open/types.h"
#include "port_general.h"
#include <apr_pools.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determines absolute path of the executing process.
 */
APR_DECLARE(apr_status_t) port_executable_name(char** self_name,
                                   apr_pool_t* pool);

/**
* Returns number of processors in the system.
*/
APR_DECLARE(int) port_CPUs_number(void);

/**
* Returns name of CPU architecture.
*/
APR_DECLARE(const char *) port_CPU_architecture(void);

/**
* Returns OS name and version.
*/
APR_DECLARE(apr_status_t) port_OS_name_version(char** os_name, char** os_ver, 
                                   apr_pool_t* pool);

/**
* Returns name of active account.
*/
APR_DECLARE(apr_status_t) port_user_name(char** account,
                             apr_pool_t* pool);

/**
* Returns home path of active account.
*/
APR_DECLARE(apr_status_t) port_user_home(char** path,
                             apr_pool_t* pool);

/**
 * Returns name of current system time zone.
 */
APR_DECLARE(apr_status_t) port_user_timezone(char** tzname,
                                             apr_pool_t* pool);

#ifdef __cplusplus
}
#endif
#endif //_PORT_SYSINFO_H_
