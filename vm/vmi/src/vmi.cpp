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
 * @author Euguene Ostrovsky
 * @version $Revision: 1.1.2.1.4.5 $
 */  
#include <assert.h>

#include "platform_lowlevel.h"
#include "vm_trace.h"

#include "zipsup.h"
#include "environment.h"
#include "properties.h"
#include "vmi.h"

extern VMInterfaceFunctions_ vmi_impl;
VMInterface vmi = &vmi_impl;

HyPortLibrary portLib;
HyPortLibrary* portLibPointer = NULL;
HyZipCachePool* zipCachePool = NULL;

VMInterface* JNICALL 
VMI_GetVMIFromJNIEnv(JNIEnv *env) 
{
    TRACE("GetVMIFromJNIEnv(): returning " << &vmi);
    return &vmi;
}

VMInterface* JNICALL 
VMI_GetVMIFromJavaVM(JavaVM *vm) 
{
    TRACE("VMI_GetVMIFromJavaVM(): returning " << &vmi);
    return &vmi;
}

/*
 * FIXME: support for previous drop from partner - to be removed.
 */
char* VMI_bootstrapClassPath(JNIEnv *env)
{
    assert(/* vmi.dll: VMI_bootstrapClassPath unimplemented. */0);
    return NULL;
}

/*
 * FIXME: support for previous drop from partner - to be removed.
 */
void VMI_fullVersionString(JNIEnv *env, char *fullversion, int size)
{
    assert(/* vmi.dll: VMI_fullVersionString unimplemented. */0);
    return;
}

/*
 * FIXME: support for previous drop from partner - to be removed.
 */
char* VMI_VMVersion(JNIEnv *env)
{
    assert(/* vmi.dll: VMI_VMVersion unimplemented. */0);
    return NULL;
}

/*
 * FIXME: support for previous drop from partner - to be removed.
 */
JavaVMInitArgs* VMI_initArgsFromEnv(JNIEnv *env)
{
    assert(/* vmi.dll: VMI_initArgsFromEnv unimplemented. */0);
    return NULL;
}

/*
 * FIXME: support for previous drop from partner - to be removed.
 */
HyZipCachePool* VMI_zipCachePool(JNIEnv *env)
{

    assert(/* vmi.dll:  unimplemented. */0);
    return NULL;
}

//////////////////////////////////////
//  VMI structure member functions  //
//////////////////////////////////////

vmiError JNICALL CheckVersion(VMInterface *vmi, vmiVersion *version)
{
    assert(/* vmi.dll:  unimplemented. */0);
    return VMI_ERROR_UNIMPLEMENTED;
}

JavaVM *JNICALL GetJavaVM(VMInterface *vmi)
{
    assert(/* vmi.dll:  unimplemented. */0);
    return NULL;
}

//#ifdef __cplusplus
//extern "C" {
//#endif
//JNIEXPORT int j9port_allocate_library(void *version, J9PortLibrary **portLibrary);
//#ifdef __cplusplus
//}
//#endif
 
HyPortLibrary *JNICALL GetPortLibrary(VMInterface *vmi)
{
    static int initialized = 0;

    if (! initialized)
    {
        int rc;
        HyPortLibraryVersion portLibraryVersion;
        HYPORT_SET_VERSION(&portLibraryVersion, HYPORT_CAPABILITY_MASK);

        rc = hyport_init_library(&portLib, &portLibraryVersion, 
                sizeof(HyPortLibrary));
        TRACE("vmi->GetPortLibrary() initializing: rc = " << rc);

        if (0 != rc) return NULL;

        initialized = 1;

    // FIXME: portlib is used in VMI_zipCachePool - we need to
    //        know there is portLib is initialized there already.
    portLibPointer = &portLib;
    }
    TRACE("vmi->GetPortLibrary(): returning: " << &portLib);
    return &portLib;
}


UDATA JNICALL HYVMLSAllocKeys(JNIEnv *env, UDATA *pInitCount,...);
void JNICALL HYVMLSFreeKeys(JNIEnv *env, UDATA *pInitCount,...);
void* JNICALL J9VMLSGet(JNIEnv *env, void *key);
void* JNICALL J9VMLSSet(JNIEnv *env, void **pKey, void *value);

HyVMLSFunctionTable vmls_inst = {
    &HYVMLSAllocKeys,
    &HYVMLSFreeKeys,
    &J9VMLSGet,
    &J9VMLSSet
};

/*
 * Returns a pointer to Local Storage Function Table. 
 */
HyVMLSFunctionTable* JNICALL 
GetVMLSFunctions(VMInterface *vmi)
{
    HyVMLSFunctionTable *pl = &vmls_inst;
    TRACE("vmi->GetVMLSFunctions(): returning " << pl);
    return pl;
}

HyZipCachePool* JNICALL GetZipCachePool(VMInterface *vmi)
{
    // FIXME: thread unsafe implementation...
    if (zipCachePool != NULL)
    {
        return zipCachePool;
    }
    assert(portLibPointer);
    zipCachePool = zipCachePool_new(&portLib);
    assert(zipCachePool);
    return zipCachePool;
}

JavaVMInitArgs* JNICALL GetInitArgs(VMInterface *vmi)
{
    return &VM_Global_State::loader_env->vm_arguments;
}

vmiError JNICALL 
GetSystemProperty(VMInterface *vmi, char *key, char **valuePtr)
{
    *valuePtr = const_cast<char *>(
        properties_get_string_property(
            (PropertiesHandle)&VM_Global_State::loader_env->properties, key));
    return VMI_ERROR_NONE;
}

vmiError JNICALL
SetSystemProperty(VMInterface *vmi, char *key, char *value)
{

    /*
     * The possible implemenation might be:
     */
    add_pair_to_properties(VM_Global_State::loader_env->properties, key, value);
    return VMI_ERROR_NONE;
}

vmiError JNICALL CountSystemProperties(VMInterface *vmi, int *countPtr)
{
    Properties &p = VM_Global_State::loader_env->properties;
    Properties::Iterator *iter = p.getIterator();
    int count = 0;
    const Prop_entry *next = NULL;

    while((next = iter->next()))
        count++;

    *countPtr = count;
    return VMI_ERROR_NONE;
}

vmiError JNICALL IterateSystemProperties(VMInterface *vmi,
        vmiSystemPropertyIterator iterator, void *userData)
{
    Properties &p = VM_Global_State::loader_env->properties;
    Properties::Iterator *iter = p.getIterator();
    const Prop_entry *next = NULL;

    while((next = iter->next()))
        iterator(next->key, const_cast<char *>(next->value->as_string()), userData);

    return VMI_ERROR_NONE;
}

VMInterfaceFunctions_ vmi_impl = {
    &CheckVersion,
    &GetJavaVM,
    &GetPortLibrary,
    &GetVMLSFunctions,
    &GetZipCachePool,
    &GetInitArgs,
    &GetSystemProperty,
    &SetSystemProperty,
    &CountSystemProperties,
    &IterateSystemProperties,
};
