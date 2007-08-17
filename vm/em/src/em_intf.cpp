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
* @author Mikhail Y. Fursov, Alexei Fedotov
* @version $Revision: 1.1.2.3.4.3 $
*/

#ifndef _EM_CPP_H_
#define _EM_CPP_H_

#include "open/types.h"
#include "open/em_vm.h"
#include "open/compmgr.h"
#include <apr_pools.h>
#include <apr_strings.h>

#include "DrlEMImpl.h"

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif


static void
ExecuteMethod(jmethodID meth, jvalue  *return_value, jvalue *args)
{
    DrlEMFactory::getEMInstance()->executeMethod(meth, return_value, args);
}

static JIT_Result
CompileMethod(Method_Handle method_handle)
{
    return DrlEMFactory::getEMInstance()->compileMethod(method_handle);
}

static void
ProfilerThreadTimeout() 
{
    DrlEMFactory::getEMInstance()->tbsTimeout();
}

static void 
ClassloaderUnloadingCallback(ClassLoaderHandle class_handle) {
    DrlEMFactory::getEMInstance()->classloaderUnloadingCallback(class_handle);        
}

static const char*
GetName() {
    return OPEN_EM;
}

static const char*
GetEmVersion() {
    return OPEN_EM_VERSION;
}

static const char*
GetDescription() {
    return "Execution manager ...";
}

static const char*
GetVendor() {
    return "Intel";
}

char* tbs_timeout = NULL;
static apr_pool_t* em_pool = NULL;

static const char*
GetProperty(const char* key) {
    if (!strcmp(key, OPEN_EM_VM_PROFILER_NEEDS_THREAD_SUPPORT)) {
        return DrlEMFactory::getEMInstance()->needTbsThreadSupport() ? "true" : "false";
    } else if (!strcmp(key, OPEN_EM_VM_PROFILER_THREAD_TIMEOUT)) {
        if (NULL == tbs_timeout) {
            tbs_timeout = apr_itoa(em_pool, DrlEMFactory::getEMInstance()->getTbsTimeout());
        }
        return tbs_timeout;
    } else {
        return NULL;
    }
}

static const char* interface_names[] = {
    OPEN_INTF_EM_VM,
    NULL
};

static const char**
ListInterfaceNames() {
    return interface_names;
}

static OpenComponentHandle component_interface = NULL;
static OpenInstanceAllocatorHandle allocator_interface = NULL;
static OpenEmVmHandle em_vm_interface = NULL;

static int
GetInterface(OpenInterfaceHandle* p_intf,
             const char* intf_name) {
    if (!strcmp(intf_name, OPEN_INTF_EM_VM)) {
        *p_intf = (OpenInterfaceHandle) em_vm_interface;
        return JNI_OK;
    } else {
        return JNI_ERR;
    }
}

static int
Free() {
    return JNI_OK;
}

static int
CreateInstance(OpenInstanceHandle* p_instance,
               apr_pool_t* pool) {
    struct _OpenInstance* instance = (struct _OpenInstance*) apr_palloc(pool, sizeof(_OpenComponent));

    instance->intf = component_interface;
    *p_instance = instance;
    if (DrlEMFactory::createAndInitEMInstance()!=NULL) {
        return JNI_OK;
    } else {
        return JNI_ERR;
    }
}

static int
FreeInstance(OpenInstanceHandle instance) {
    DrlEMFactory::deinitEMInstance();
    return JNI_OK;
}


EMEXPORT
int EmInitialize(OpenComponentHandle* p_component,
                 OpenInstanceAllocatorHandle* p_allocator,
                 apr_pool_t* pool)
{
    if (component_interface || allocator_interface || em_vm_interface) {
        /* Virtual table already initialized */
        return JNI_ERR;
    }
    em_pool = pool;

    struct _OpenComponent* c_intf = (struct _OpenComponent*) apr_palloc(pool, sizeof(_OpenComponent));

    c_intf->GetName = GetName;
    c_intf->GetVersion = GetEmVersion;
    c_intf->GetDescription = GetDescription;
    c_intf->GetVendor = GetVendor;
    c_intf->GetProperty = GetProperty;
    c_intf->ListInterfaceNames = ListInterfaceNames;
    c_intf->GetInterface = GetInterface;
    c_intf->Free = Free;

    _OpenInstanceAllocator* a_intf = (_OpenInstanceAllocator*)
        apr_palloc(pool, sizeof(_OpenInstanceAllocator));
    a_intf->CreateInstance = CreateInstance;
    a_intf->FreeInstance = FreeInstance;

    _OpenEmVm* vm_intf = (_OpenEmVm*) apr_palloc(pool, sizeof(_OpenEmVm));
    vm_intf->ExecuteMethod = ExecuteMethod;
    vm_intf->CompileMethod = CompileMethod;
    vm_intf->ProfilerThreadTimeout = ProfilerThreadTimeout;
    vm_intf->ClassloaderUnloadingCallback = ClassloaderUnloadingCallback;

    component_interface = (OpenComponentHandle) c_intf;
    allocator_interface = (OpenInstanceAllocatorHandle) a_intf;
    em_vm_interface = (OpenEmVmHandle) vm_intf;

    *p_component = component_interface;
    *p_allocator = allocator_interface;
    return JNI_OK;
}



#ifdef __cplusplus
}
#endif

#endif 

