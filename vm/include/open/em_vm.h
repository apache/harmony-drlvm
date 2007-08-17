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
 * @version $Revision: 1.1.2.2.4.4 $
 */  
#ifndef _EM_VM_H_
#define _EM_VM_H_

/**
 * @file
 * EM interface exposed to VM.
 *
 * VM uses the given interface to ask EM to execute and/or compile a method
 * or to notify EM on profiler thread events.
 */
 
#include "open/types.h"
#include "open/em.h"
#include "jni_types.h"


#ifdef __cplusplus
extern "C" {
#endif
/// The runtime name of EM_VM interface.
#   define OPEN_INTF_EM_VM "open.interface.em.vm." OPEN_EM_VERSION
/// The runtime property name to request if EM needs in profiler thread support.
#   define OPEN_EM_VM_PROFILER_NEEDS_THREAD_SUPPORT "open.property.em.vm.profiler_needs_thread_support"
/// The runtime property name to request EM profiler thread timeout.
#   define OPEN_EM_VM_PROFILER_THREAD_TIMEOUT "open.property.em.vm.profiler_thread_timeout"

  /** 
   * The structure comprises all EM to VM interface methods.
   */
    struct _OpenEmVm {

  /** 
   * Requests EM to execute the method.
   *
   * VM uses the given method to execute uncompiled methods.
   * EM is responsible for selecting the appropriate execution engine and 
   * executing the method using the stub provided by the execution engine.
   *
   * @param[in] meth         - the method to execute
   * @param[in] return_value - the place to store the return value of the method
   * @param[in] args         - the parameters to be passed to the method
   */
        void (*ExecuteMethod) (jmethodID meth, jvalue  *return_value, jvalue *args);

  /** 
   * Requests EM to start the compilation of the method
   *
   * VM or JIT method compilation stubs use the given method to 
   * ask EM to start the method compilation.
   * EM is responsible for selecting JIT for the given method and passing the 
   * control under JIT and the specified method to VM.
   *
   * @param[in] method_handle - the handle of the method to compile
   */
        JIT_Result (*CompileMethod) (Method_Handle method_handle);

  /** 
   * The method is used to callback EM from the profiler thread supported
   * by VM on time-out.
   */
        void (*ProfilerThreadTimeout) ();

        void (*ClassloaderUnloadingCallback) (ClassLoaderHandle class_handle);

    };
    typedef const struct _OpenEmVm* OpenEmVmHandle;

#ifdef __cplusplus
}
#endif


#endif

