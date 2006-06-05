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
 * @version $Revision: 1.1.2.2.4.4 $
 */  
#ifndef _EM_VM_H_
#define _EM_VM_H_

#include "open/types.h"
#include "open/em.h"
#include "jni_types.h"


#ifdef __cplusplus
extern "C" {
#endif

#   define OPEN_INTF_EM_VM "open.interface.em.vm." OPEN_EM_VERSION
#   define OPEN_EM_VM_PROFILER_NEEDS_THREAD_SUPPORT "open.property.em.vm.profiler_needs_thread_support"
#   define OPEN_EM_VM_PROFILER_THREAD_TIMEOUT "open.property.em.vm.profiler_thread_timeout"


    struct _OpenEmVm {

        void (*ExecuteMethod) (jmethodID meth, jvalue  *return_value, jvalue *args);

        JIT_Result (*CompileMethod) (Method_Handle method_handle);

        void (*ProfilerThreadTimeout) ();

    };
    typedef const struct _OpenEmVm* OpenEmVmHandle;

#ifdef __cplusplus
}
#endif


#endif

