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

#ifndef _PORT_DSO_H_
#define _PORT_DSO_H_

#include "open/types.h"
#include "port_general.h"
#include <apr_pools.h>
#include <apr_dso.h>

#ifdef __cplusplus
extern "C" {
#endif


#define PORT_DSO_DEFAULT 0
#define PORT_DSO_BIND_NOW 0x1
#define PORT_DSO_BIND_DEFER 0x2

/**
* Loads shared binary (executable or library).
*/
APR_DECLARE(apr_status_t) port_dso_load_ex(apr_dso_handle_t** handle,
                                      const char* path,
                                      uint32 mode,
                                      apr_pool_t* pool);


/**
* Returns list of directories in which OS searches for libraries.
*/
APR_DECLARE(apr_status_t) port_dso_search_path(char** path,
                                        apr_pool_t* pool);

/**
 * Decorate shared library name (.dll <-> lib*.so).
 * @non_apr
 */
APR_DECLARE(char *) port_dso_name_decorate(const char* dl_name,
                            apr_pool_t* pool);

#ifdef __cplusplus
}
#endif
#endif /*_PORT_DSO_H_*/
