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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#include <assert.h>

#include "port_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

APR_DECLARE(void *) port_atomic_compare_exchange_pointer(volatile void ** data, void * value, const void * comp) {
    return (void *) port_atomic_cas64((uint64 *)data, (uint64)value, (uint64)comp);
}

#ifdef __cplusplus
}
#endif
