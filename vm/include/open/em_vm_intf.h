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
 * @author Mikhail Y. Fursov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#ifndef _EM_VM_H_
#define _EM_VM_H_

#include "open/types.h"
#include "open/em.h"
#include "jni_types.h"


#ifdef __cplusplus
extern "C" {
#endif

EMEXPORT char EM_init ();

EMEXPORT void EM_deinit();

EMEXPORT void EM_execute_method(jmethodID meth, jvalue  *return_value, jvalue *args);

EMEXPORT JIT_Result EM_compile_method(Method_Handle method_handle);

EMEXPORT  char EM_need_profiler_thread_support();

EMEXPORT void EM_profiler_thread_timeout();

EMEXPORT int EM_get_profiler_thread_timeout();

#ifdef __cplusplus
}
#endif


#endif

