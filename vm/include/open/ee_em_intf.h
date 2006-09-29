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
 * @author Mikhail Y. Fursov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#ifndef _EE_EM_H_
#define _EE_EM_H_

#include "open/types.h"
#include "open/em.h"


#ifdef __cplusplus
extern "C" {
#endif

// Called once by EM during JIT initialization
JITEXPORT void JIT_init(JIT_Handle jit, const char* name);

// Called once by EM during system shutdown. Optional
JITEXPORT void JIT_deinit(JIT_Handle jit);


//Optional
JITEXPORT void JIT_set_profile_access_interface(JIT_Handle jit, EM_Handle em, struct EM_ProfileAccessInterface* pc_interface);
 
//Optional
JITEXPORT bool JIT_enable_profiling(JIT_Handle jit, PC_Handle pc, EM_JIT_PC_Role role);

#ifdef __cplusplus
}
#endif


#endif

