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
 * @author Gregory Shimansky
 * @version $Revision: 1.1.2.1.4.5 $
 */  
/*
 * JVMTI extensions API
 */

#include "jvmti_internal.h"
#include "jvmti_utils.h"
#include "open/vm_util.h"
#include "cxxlog.h"
#include "suspend_checker.h"

struct JvmtiExtension
{
    JvmtiExtension *next;
    jvmtiExtensionFunctionInfo info;
};

static JvmtiExtension *jvmti_extension_list = NULL;

static const jint extensions_number = 0;

static void free_allocated_extension_array(jvmtiExtensionFunctionInfo *array,
                                           jint number)
{
    for (int iii = 0; iii < number; iii++)
    {
        _deallocate((unsigned char *)array[iii].id);
        _deallocate((unsigned char *)array[iii].short_description);
        _deallocate((unsigned char *)array[iii].params->name);
        _deallocate((unsigned char *)array[iii].params);
        _deallocate((unsigned char *)array[iii].errors);
    }
    _deallocate((unsigned char *)array);
}

/*
 * Get Extension Functions
 *
 * Returns the set of extension functions.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetExtensionFunctions(jvmtiEnv* env,
                           jint* extension_count_ptr,
                           jvmtiExtensionFunctionInfo** extensions)
{
    TRACE2("jvmti.extension", "GetExtensionFunctions called");
    SuspendEnabledChecker sec;
    // check environment, phase, pointers
    jvmtiError errorCode;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == extension_count_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL == extensions)
        return JVMTI_ERROR_NULL_POINTER;

    *extension_count_ptr = extensions_number;

    if (0 == extensions_number)
        *extensions = NULL;
    else
    {
        jvmtiExtensionFunctionInfo *array;
        errorCode = _allocate(sizeof(jvmtiExtensionFunctionInfo) *
                              extensions_number, (unsigned char **)&array);

        if (JVMTI_ERROR_NONE != errorCode)
            return errorCode;

        JvmtiExtension *ex = jvmti_extension_list;
        for (int iii = 0; iii < extensions_number; iii++)
        {
            jvmtiExtensionFunctionInfo *info = &ex->info;

            array[iii].func = info->func;
            array[iii].param_count = info->param_count;
            array[iii].error_count = info->error_count;

            errorCode = _allocate(strlen(info->id) + 1,
                                  (unsigned char **)&(array[iii].id));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                free_allocated_extension_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(strlen(info->short_description) + 1,
                                  (unsigned char **)&(array[iii].short_description));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                free_allocated_extension_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(info->param_count * sizeof(jvmtiParamInfo),
                                  (unsigned char **)&(array[iii].params));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                _deallocate((unsigned char *)array[iii].short_description);
                free_allocated_extension_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(strlen(info->params->name) + 1,
                                  (unsigned char **)&(array[iii].params->name));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                _deallocate((unsigned char *)array[iii].short_description);
                _deallocate((unsigned char *)array[iii].params);
                free_allocated_extension_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(info->error_count * sizeof(jvmtiError),
                                  (unsigned char **)&(array[iii].errors));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                _deallocate((unsigned char *)array[iii].short_description);
                _deallocate((unsigned char *)array[iii].params->name);
                _deallocate((unsigned char *)array[iii].params);
                free_allocated_extension_array(array, iii);
                return errorCode;
            }

            strcpy(array[iii].id, info->id);
            strcpy(array[iii].short_description, info->short_description);
            strcpy(array[iii].params->name, info->params->name);
            memcpy(array[iii].params, info->params,
                   info->param_count * sizeof(jvmtiParamInfo));
            memcpy(array[iii].errors, info->errors,
                   info->error_count * sizeof(jvmtiError));

            ex = ex->next;
        }

        *extensions = array;
    }

    return JVMTI_ERROR_NONE;
}

struct JvmtiExcensionEvent
{
    JvmtiExcensionEvent *next;
    jvmtiExtensionEventInfo info;
};

static JvmtiExcensionEvent *jvmti_exntension_event_list = NULL;

static const jint extensions_events_number = 0;

static void free_allocated_extension_event_array(jvmtiExtensionEventInfo *array,
                                                 jint number)
{
    for (int iii = 0; iii < number; iii++)
    {
        _deallocate((unsigned char *)array[iii].id);
        _deallocate((unsigned char *)array[iii].short_description);
        _deallocate((unsigned char *)array[iii].params->name);
        _deallocate((unsigned char *)array[iii].params);
    }
    _deallocate((unsigned char *)array);
}

/*
 * Get Extension Events
 *
 * Returns the set of extension events.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGetExtensionEvents(jvmtiEnv* env,
                        jint* extension_count_ptr,
                        jvmtiExtensionEventInfo** extensions)
{
    TRACE2("jvmti.extension", "GetExtensionEvents called");
    SuspendEnabledChecker sec;
    // check environment, phase, pointers
    jvmtiError errorCode;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (NULL == extension_count_ptr)
        return JVMTI_ERROR_NULL_POINTER;

    if (NULL == extensions)
        return JVMTI_ERROR_NULL_POINTER;

    *extension_count_ptr = extensions_events_number;

    if (0 == extensions_events_number)
        *extensions = NULL;
    else
    {
        jvmtiExtensionEventInfo *array;
        errorCode = _allocate(sizeof(jvmtiExtensionEventInfo) *
                              extensions_events_number, (unsigned char **)&array);

        if (JVMTI_ERROR_NONE != errorCode)
            return errorCode;

        JvmtiExcensionEvent *ex = jvmti_exntension_event_list;
        for (int iii = 0; iii < extensions_events_number; iii++)
        {
            jvmtiExtensionEventInfo *info = &ex->info;

            array[iii].extension_event_index = info->extension_event_index;
            array[iii].param_count = info->param_count;

            errorCode = _allocate(strlen(info->id) + 1,
                (unsigned char **)&(array[iii].id));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                free_allocated_extension_event_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(strlen(info->short_description) + 1,
                (unsigned char **)&(array[iii].short_description));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                free_allocated_extension_event_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(info->param_count * sizeof(jvmtiParamInfo),
                                  (unsigned char **)&(array[iii].params));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                _deallocate((unsigned char *)array[iii].short_description);
                free_allocated_extension_event_array(array, iii);
                return errorCode;
            }

            errorCode = _allocate(strlen(info->params->name) + 1,
                (unsigned char **)&(array[iii].params->name));
            if (JVMTI_ERROR_NONE != errorCode)
            {
                _deallocate((unsigned char *)array[iii].id);
                _deallocate((unsigned char *)array[iii].short_description);
                _deallocate((unsigned char *)array[iii].params);
                free_allocated_extension_event_array(array, iii);
                return errorCode;
            }

            strcpy(array[iii].id, info->id);
            strcpy(array[iii].short_description, info->short_description);
            strcpy(array[iii].params->name, info->params->name);
            memcpy(array[iii].params, info->params,
                info->param_count * sizeof(jvmtiParamInfo));

            ex = ex->next;
        }

        *extensions = array;
    }

    return JVMTI_ERROR_NONE;
}

jvmtiError TIEnv::allocate_extension_event_callbacks_table()
{
    return _allocate(extensions_events_number * sizeof(jvmtiExtensionEvent),
        (unsigned char **)&extension_event_table);
}

/*
 * Set Extension Event Callback
 *
 * Sets the callback function for an extension event and enables
 * the event. Or, if the callback is NULL, disables the event.
 * Note that unlike standard events, setting the callback and
 * nabling the event are a single operation.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiSetExtensionEventCallback(jvmtiEnv* env,
                               jint extension_event_index,
                               jvmtiExtensionEvent callback)
{
    TRACE2("jvmti.extension", "SetExtensionEventCallback called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (extension_event_index > extensions_events_number)
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    TIEnv *ti_env = (TIEnv *)env;
    ti_env->extension_event_table[extension_event_index] = callback;

    return JVMTI_ERROR_NONE;
}

