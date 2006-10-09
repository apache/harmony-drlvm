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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.3.4.4 $
 */  
/*
 * JVMTI property API
 */

#include <apr_file_info.h>

#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "environment.h"
#include "properties.h"
#include "cxxlog.h"
#include "port_filepath.h"
#include "suspend_checker.h"

/*
 * Add To Bootstrap Class Loader Search
 *
 * After the bootstrap class loader unsuccessfully searches for
 * a class, the specified platform-dependent search path segment
 * will be searched as well. This function can be used to cause
 * instrumentation classes to be defined by the bootstrap class
 * loader.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiAddToBootstrapClassLoaderSearch(jvmtiEnv* env,
                                     const char* segment)
{
    TRACE2("jvmti.property", "AddToBootstrapClassLoaderSearch called, segment = " << segment);
    SuspendEnabledChecker sec;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD};

    CHECK_EVERYTHING();

    if (NULL == segment)
        return JVMTI_ERROR_NULL_POINTER;

    // create temp pool for apr functions
    apr_pool_t *tmp_pool;
    apr_pool_create(&tmp_pool, NULL);

    // check existance of a given path
    apr_finfo_t finfo;
    if(apr_stat(&finfo, segment, APR_FINFO_SIZE, tmp_pool) != APR_SUCCESS) {
        // broken path to the file
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;
    }
    // destroy temp pool
    apr_pool_destroy(tmp_pool);

    // get bootclasspath property
    Global_Env *g_env = ((TIEnv*)env)->vm->vm_env;
    Properties *properties = g_env->properties;
    const char *bcp_prop = properties_get_string_property(
        reinterpret_cast<PropertiesHandle>(properties),
        "vm.boot.class.path");

    // create new bootclasspath
    size_t len_bcp = strlen(bcp_prop);
    size_t len_seg = strlen(segment);
    char *new_bcp = (char *)STD_ALLOCA( len_bcp + len_seg + 2);
    memcpy(new_bcp, bcp_prop, len_bcp);
    char *end = new_bcp + len_bcp;
    *end = PORT_PATH_SEPARATOR;
    memcpy(end + 1, segment, len_seg + 1);

    // update bootclasspath property
    add_pair_to_properties(*properties, "vm.boot.class.path", new_bcp);

    return JVMTI_ERROR_NONE;
}

/*
 * Get System Properties
 *
 * The list of VM system property keys which may be used with
 * GetSystemProperty is returned. It is strongly recommended
 * that virtual machines provide the following property keys:
 *    java.vm.vendor
 *    java.vm.version
 *    java.vm.name
 *    java.vm.info
 *    java.library.path
 *    java.class.path
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetSystemProperties(jvmtiEnv* env,
                         jint* count_ptr,
                         char*** property_ptr)
{
    TRACE2("jvmti.property", "GetSystemProperties called");
    SuspendEnabledChecker sec;
    jvmtiError errorCode;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == count_ptr || NULL == property_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    jint properties_count = 0;

    Properties::Iterator *iterator = ((TIEnv*)env)->vm->vm_env->properties->getIterator();
    const Prop_entry *next = NULL;
    while((next = iterator->next()))
        properties_count++;

    char **prop_names_array;
    errorCode = _allocate(sizeof(char*) * properties_count, (unsigned char **)&prop_names_array);
    if (JVMTI_ERROR_NONE != errorCode)
        return errorCode;

    // Copy properties defined in properties list
    iterator = ((TIEnv*)env)->vm->vm_env->properties->getIterator();
    for (int iii = 0; iii < properties_count; iii++)
    {
        next = iterator->next();
        errorCode = _allocate(strlen(next->key) + 1, (unsigned char **)&prop_names_array[iii]);
        if (JVMTI_ERROR_NONE != errorCode)
        {
            // Free everything that was allocated already
            for (int jjj = 0; jjj < iii; jjj++)
                _deallocate((unsigned char *)prop_names_array[iii]);
            _deallocate((unsigned char *)prop_names_array);
            return errorCode;
        }
        strcpy(prop_names_array[iii], next->key);
    }

    *count_ptr = properties_count;
    *property_ptr = prop_names_array;

    return JVMTI_ERROR_NONE;
}

/*
 * Get System Property
 *
 * Return a VM system property value given the property key.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetSystemProperty(jvmtiEnv* env,
                       const char* property,
                       char** value_ptr)
{
    TRACE2("jvmti.property", "GetSystemProperty called, property = " << property);
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == property || NULL == value_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    Prop_Value *prop_value = ((TIEnv*)env)->vm->vm_env->properties->get(property);
    if (NULL == prop_value)
        return JVMTI_ERROR_NOT_AVAILABLE;

    const char *value = prop_value->as_string();
    if (NULL == value)
        return JVMTI_ERROR_NOT_AVAILABLE;

    char *ret;
    jvmtiError errorCode = _allocate(strlen(value) + 1, (unsigned char **)&ret);
    if (errorCode != JVMTI_ERROR_NONE)
        return errorCode;

    strcpy(ret, value);
    *value_ptr = ret;

    return JVMTI_ERROR_NONE;
}

/*
 * Set System Property
 *
 * Set a VM system property value.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiSetSystemProperty(jvmtiEnv* env,
                       const char* property,
                       const char* value)
{
    TRACE2("jvmti.property", "SetSystemProperty called, property = " << property << " value = " << value);
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD};

    CHECK_EVERYTHING();

    if (NULL == property)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL == value)
        return JVMTI_ERROR_NOT_AVAILABLE;

    Global_Env *vm_env = ((TIEnv*)env)->vm->vm_env;
    Prop_String *ps = new Prop_String(strdup(value));
    Prop_entry *e = new Prop_entry();
    e->key = strdup(property);
    e->value = ps;

    Prop_entry *pe = vm_env->properties->get_entry(property);
    if (NULL == pe)
        vm_env->properties->add(e);
    else
        pe->replace(e);

    return JVMTI_ERROR_NONE;
}
