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
#ifndef _EM_H_
#define _EM_H_

#ifdef __cplusplus
extern "C" {
#endif

#define OPEN_EM "em"
#define OPEN_EM_VERSION "1.0"


typedef void *EM_Handle;
typedef void *JIT_Handle;
typedef void *PC_Handle;
typedef void *Method_Profile_Handle;

typedef
enum JIT_Result {
    JIT_SUCCESS,
    JIT_FAILURE
} JIT_Result; //JIT_Result

typedef
enum EM_JIT_PC_Role {
    EM_JIT_PROFILE_ROLE_GEN=1,
    EM_JIT_PROFILE_ROLE_USE=2
} EM_JIT_PC_Role;


#ifdef __cplusplus
}
#endif


#endif
