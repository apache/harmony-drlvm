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
 * @author Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  

/* All code from this file was inlinied in port_atomic.h */

#include <port_atomic.h>
#include <assert.h>

#if defined(_EM64T_) && defined(_WIN64)
// TODO: these functions need to be implemented
APR_DECLARE(uint8) port_atomic_cas8(volatile uint8 * data, 
                                               uint8 value, uint8 comp) {
	assert(0);
	return 0;
}

APR_DECLARE(uint16) port_atomic_cas16(volatile uint16 * data, 
                                                 uint16 value, uint16 comp) {
	assert(0);
	return 0;
}

APR_DECLARE(uint64) port_atomic_cas64(volatile uint64 * data, 
                                                 uint64 value, uint64 comp) {
	assert(0);
	return 0;
}

#endif
