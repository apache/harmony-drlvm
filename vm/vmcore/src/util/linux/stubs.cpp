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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#define LOG_DOMAIN "port.old"
#include "cxxlog.h"

#include "vm_threads.h"
#include "exceptions.h"
#include "method_lookup.h"
#include "open/gc.h"

//wgs: I wonder if we could give it a errno
#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:584) // omission of exception specification is incompatible with previous function "__errno_location" (declared at line 38 of "/usr/include/bits/errno.h")
#endif

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif
