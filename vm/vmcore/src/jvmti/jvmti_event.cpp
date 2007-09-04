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
 * @version $Revision: 1.1.2.2.4.5 $
 */
/*
 * JVMTI events API
 */

#define LOG_DOMAIN "jvmti"
#include "cxxlog.h"

#include "open/gc.h"
#include "jvmti_direct.h"
#include "jvmti_internal.h"
#include "jvmti_utils.h"
#include "environment.h"
#include "interpreter_exports.h"
#include "interpreter_imports.h"
#include "classloader.h"
#include "open/jthread.h"
#include "suspend_checker.h"
#include "jit_intf_cpp.h"
#include "vm_log.h"
#include "cci.h"
#include "compile.h"
#include "jvmti_break_intf.h"
#include "stack_iterator.h"
#include "m2n.h"

/*
 * Set Event Callbacks
 *
 * Set the functions to be called for each event. The callbacks
 * are specified by supplying a replacement function table.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiSetEventCallbacks(jvmtiEnv* env,
                       const jvmtiEventCallbacks* callbacks,
                       jint size_of_callbacks)
{
    TRACE2("jvmti.event", "SetEventCallbacks called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    if (size_of_callbacks <= 0)
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    // FIXME: There should be a thread synchronization in order to
    // make this atomic operation as required by spec.

    if (NULL != callbacks)
        memcpy(&(((TIEnv*)env)->event_table), callbacks, sizeof(jvmtiEventCallbacks));
    else
        memset(&(((TIEnv*)env)->event_table), 0, sizeof(jvmtiEventCallbacks));

    return JVMTI_ERROR_NONE;
}

jvmtiError add_event_to_thread(jvmtiEnv *env, jvmtiEvent event_type, jthread event_thread)
{
    TIEnv *p_env = (TIEnv *)env;
    hythread_t p_thread = jthread_get_native_thread(event_thread);
    TIEventThread *et = p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];

    // Find out if this environment is already registered on this thread on this event type
    while (NULL != et)
    {
        if (et->thread == p_thread)
            return JVMTI_ERROR_NONE;
        et = et->next;
    }

    TIEventThread *newet;
    jvmtiError errorCode = _allocate(sizeof(TIEventThread), (unsigned char **)&newet);
    if (JVMTI_ERROR_NONE != errorCode)
        return errorCode;
    newet->thread = p_thread;

    // FIXME: dynamic list modification without synchronization
    newet->next = p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
    p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL] = newet;
    return JVMTI_ERROR_NONE;
}

void remove_event_from_thread(jvmtiEnv *env, jvmtiEvent event_type, jthread event_thread)
{
    TIEnv *p_env = (TIEnv *)env;
    hythread_t p_thread = jthread_get_native_thread(event_thread);
    TIEventThread *et = p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];

    if (NULL == et)
        return;

    // FIXME: dynamic list modification without synchronization
    if (et->thread == p_thread)
    {
        p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL] = et->next;
        _deallocate((unsigned char *)et);
        return;
    }

    // Find out if this environment is already registered on this thread on this event type
    while (NULL != et->next)
    {
        if (et->next->thread == p_thread)
        {
            TIEventThread *oldet = et->next;
            et->next = oldet->next;
            _deallocate((unsigned char *)oldet);
            return;
        }
        et = et->next;
    }
}

void add_event_to_global(jvmtiEnv *env, jvmtiEvent event_type)
{
    TIEnv *p_env = (TIEnv *)env;
    p_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL] = true;
}

void remove_event_from_global(jvmtiEnv *env, jvmtiEvent event_type)
{
    TIEnv *p_env = (TIEnv *)env;
    p_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL] = false;
}

// disable all events except VM_DEATH
static void disable_all_events(TIEnv* p_env)
{
    for (jvmtiEvent event_type = JVMTI_MIN_EVENT_TYPE_VAL; 
            event_type <= JVMTI_MAX_EVENT_TYPE_VAL ; 
            event_type = (jvmtiEvent) (event_type + 1)) {
        // skip VM_DEATH
        if (JVMTI_EVENT_VM_DEATH == event_type)
            continue;

        // disable environment global events
        p_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL] = false;

        // disable thread specific events
        TIEventThread* et = p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
        p_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL] = NULL;

        while (NULL != et) {
            TIEventThread* next_et = et->next;

            _deallocate((unsigned char*) et);

            et = next_et;
        }
    }
}

/**
 * Return true if event is enabled in any of TI environments even for one
 * thread.
 */
jboolean event_enabled(jvmtiEvent event_type) {
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if ( !ti->isEnabled() ) return false;

    for (TIEnv * ti_env = ti -> getEnvironments(); NULL != ti_env; ti_env = ti_env -> next) {
        if (ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) return true;
        if (ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL] != 0) return true;
    }

    return false;
}

static inline bool
check_event_is_disable( jvmtiEvent event_type,
                        DebugUtilsTI *ti)
{
    for (TIEnv *ti_env = ti->getEnvironments(); ti_env;
         ti_env = ti_env->next)
    {
        if (ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]
            || ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            return false;
        }
    }
    return true;
}

/*
 * Set Event Notification Mode
 *
 * Control the generation of events.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiSetEventNotificationMode(jvmtiEnv* env,
                              jvmtiEventMode mode,
                              jvmtiEvent event_type,
                              jthread event_thread,
                              ...)
{
    TRACE2("jvmti.event", "SetEventNotificationMode called, event type = " << event_type <<
        " mode = " << mode);
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};
    CHECK_EVERYTHING();

    // Check that events can be set on thread basis and that thread is alive
    if (NULL != event_thread)
    {
        if ((JVMTI_EVENT_VM_INIT == event_type) ||
            (JVMTI_EVENT_VM_START == event_type) ||
            (JVMTI_EVENT_VM_DEATH == event_type) ||
            (JVMTI_EVENT_THREAD_START == event_type) ||
            (JVMTI_EVENT_COMPILED_METHOD_LOAD == event_type) ||
            (JVMTI_EVENT_COMPILED_METHOD_UNLOAD == event_type) ||
            (JVMTI_EVENT_DYNAMIC_CODE_GENERATED == event_type) ||
            (JVMTI_EVENT_DATA_DUMP_REQUEST == event_type))
            return JVMTI_ERROR_ILLEGAL_ARGUMENT;

        if (!is_valid_thread_object(event_thread))
            return JVMTI_ERROR_INVALID_THREAD;

        jint thread_state;
        IDATA UNUSED status = jthread_get_jvmti_state(event_thread, &thread_state);
        assert(status == TM_ERROR_NONE);

        if (!(thread_state & JVMTI_THREAD_STATE_ALIVE))
            return JVMTI_ERROR_THREAD_NOT_ALIVE;
    }

    bool old_state = event_enabled(event_type);

    if (JVMTI_ENABLE == mode)
    {
        // Capabilities checking
        jvmtiCapabilities capa;
        jvmtiGetCapabilities(env, &capa);
        switch (event_type)
        {
        case JVMTI_EVENT_FIELD_MODIFICATION:
            if (!capa.can_generate_field_modification_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_FIELD_ACCESS:
            if (!capa.can_generate_field_access_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_SINGLE_STEP:
            if (!capa.can_generate_single_step_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_EXCEPTION:
        case JVMTI_EVENT_EXCEPTION_CATCH:
            if (!capa.can_generate_exception_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_FRAME_POP:
            if (!capa.can_generate_frame_pop_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_BREAKPOINT:
            if (!capa.can_generate_breakpoint_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_METHOD_ENTRY:
            if (!capa.can_generate_method_entry_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_METHOD_EXIT:
            if (!capa.can_generate_method_exit_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_COMPILED_METHOD_LOAD:
        case JVMTI_EVENT_COMPILED_METHOD_UNLOAD:
            if (!capa.can_generate_compiled_method_load_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_MONITOR_CONTENDED_ENTER:
        case JVMTI_EVENT_MONITOR_CONTENDED_ENTERED:
        case JVMTI_EVENT_MONITOR_WAIT:
        case JVMTI_EVENT_MONITOR_WAITED:
            if (!capa.can_generate_monitor_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_VM_OBJECT_ALLOC:
            if (!capa.can_generate_vm_object_alloc_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_NATIVE_METHOD_BIND:
            if (!capa.can_generate_native_method_bind_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_GARBAGE_COLLECTION_START:
        case JVMTI_EVENT_GARBAGE_COLLECTION_FINISH:
            if (!capa.can_generate_garbage_collection_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        case JVMTI_EVENT_OBJECT_FREE:
            if (!capa.can_generate_object_free_events)
                return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;
            break;
        default:
            // FIXME: Valentin Al. Sitnick
            // Additional conditions for checking of event type.
            // It is needed because between JVMTI_MIN_EVENT_TYPE_VAL
            // and JVMTI_MAX_EVENT_TYPE_VAL there are some values
            // for reserved and unsupported in current JVMTI version
            // event types: 72, 77, 78, 79, 80.
            if (event_type < JVMTI_MIN_EVENT_TYPE_VAL ||
                    event_type > JVMTI_MAX_EVENT_TYPE_VAL ||
                    72 == event_type || ((event_type >= 77) && (event_type <= 80)))
                return JVMTI_ERROR_INVALID_EVENT_TYPE;
        }

        if (NULL == event_thread)
            add_event_to_global(env, event_type);
        else
            add_event_to_thread(env, event_type, event_thread);
    }
    else if (JVMTI_DISABLE == mode)
        if (NULL == event_thread)
            remove_event_from_global(env, event_type);
        else
            remove_event_from_thread(env, event_type, event_thread);
    else
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    DebugUtilsTI *ti = ((TIEnv *)env)->vm->vm_env->TI;
    if (event_enabled(event_type) != (jboolean)old_state) {
        if (interpreter_enabled())
            interpreter.interpreter_ti_set_notification_mode(event_type, !old_state);
        else
        {
            if (JVMTI_EVENT_SINGLE_STEP == event_type)
            {
                if (JVMTI_ENABLE == mode && !ti->is_single_step_enabled())
                {
                    TRACE2("jvmti.break.ss", "SingleStep event is enabled");
                    jvmtiError errorCode = ti->jvmti_single_step_start();

                    if (JVMTI_ERROR_NONE != errorCode)
                        return errorCode;
                }
                else if (JVMTI_DISABLE == mode && ti->is_single_step_enabled())
                {
                    // Check that no environment has SingleStep enabled
                    LMAutoUnlock lock(&ti->TIenvs_lock);
                    TRACE2("jvmti.break.ss",
                        "SingleStep event is disabled locally for env: "
                        << env);
                    bool disable = check_event_is_disable(JVMTI_EVENT_SINGLE_STEP, ti);
                    if (disable) {
                        TRACE2("jvmti.break.ss", "SingleStep event is disabled");
                        jvmtiError errorCode = ti->jvmti_single_step_stop();
                        if (JVMTI_ERROR_NONE != errorCode)
                            return errorCode;
                    }
                }
            }
            else if(JVMTI_EVENT_METHOD_ENTRY == event_type)
            {
                if (JVMTI_ENABLE == mode)
                {
                    TRACE2("jvmti.event.method.entry", "ENABLED global method entry flag");
                    ti->set_method_entry_flag(1);
                }
                else if (JVMTI_DISABLE == mode && ti->get_method_entry_flag() != 0)
                {
                    LMAutoUnlock lock(&ti->TIenvs_lock);
                    bool disable = check_event_is_disable(JVMTI_EVENT_METHOD_ENTRY, ti);
                    if (disable) {
                        TRACE2("jvmti.event.method.entry", "DISABLED global method entry flag");
                        ti->set_method_entry_flag(0);
                    }
                }
            }
            else if(JVMTI_EVENT_METHOD_EXIT == event_type || JVMTI_EVENT_FRAME_POP == event_type)
            {
                if (JVMTI_ENABLE == mode)
                {
                    TRACE2("jvmti.event.method.exit", "ENABLED global method exit flag");
                    ti->set_method_exit_flag(1);
                }
                else if (JVMTI_DISABLE == mode && ti->get_method_exit_flag() != 0)
                {
                    LMAutoUnlock lock(&ti->TIenvs_lock);
                    bool disable = check_event_is_disable(JVMTI_EVENT_METHOD_EXIT, ti) ||
                        check_event_is_disable(JVMTI_EVENT_FRAME_POP, ti);
                    if (disable) {
                        TRACE2("jvmti.event.method.exit", "DISABLED global method exit flag");
                        ti->set_method_exit_flag(0);
                    }
                }
            }
        }
        if( JVMTI_EVENT_DATA_DUMP_REQUEST == event_type ) {
            if(JVMTI_ENABLE == mode) {
                LMAutoUnlock lock(&ti->TIenvs_lock);
                jvmti_create_event_thread();
            } else {
                LMAutoUnlock lock(&ti->TIenvs_lock);
                bool disable =
                    check_event_is_disable(JVMTI_EVENT_DATA_DUMP_REQUEST, ti);
                if (disable) {
                    jvmti_destroy_event_thread();
                }
            }
        }
    }

    return JVMTI_ERROR_NONE;
}

void jvmti_send_vm_start_event(Global_Env *env, JNIEnv *jni_env)
{
    DebugUtilsTI *ti = env->TI;
    if (!ti->isEnabled())
        return;

    // Switch phase to VM_Start and sent VMStart event
    ti->nextPhase(JVMTI_PHASE_START);
    // Send VM_Start TI events
    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_VM_START - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventVMStart func = (jvmtiEventVMStart)ti_env->get_event_callback(JVMTI_EVENT_VM_START);
            if (NULL != func)
            {
                TRACE2("jvmti.event.vs", "Callback JVMTI_PHASE_START called");
                func((jvmtiEnv*)ti_env, jni_env);
                TRACE2("jvmti.event.vs", "Callback JVMTI_PHASE_START finished");
            }
        }

        ti_env = next_env;
    }
    // send notify events
    unsigned index;
    for( index = 0; index < env->TI->GetNumberPendingNotifyLoadClass(); index++ ) {
        Class *klass = env->TI->GetPendingNotifyLoadClass( index );
        jvmti_send_class_load_event(env, klass);
    }
    for( index = 0; index < env->TI->GetNumberPendingNotifyPrepareClass(); index++ ) {
        Class *klass = env->TI->GetPendingNotifyPrepareClass( index );
        jvmti_send_class_prepare_event(klass);
    }
    env->TI->ReleaseNotifyLists();
}

void jvmti_send_vm_init_event(Global_Env *env)
{
    assert(hythread_is_suspend_enabled());
    // Switch phase to VM_Live event
    DebugUtilsTI *ti = env->TI;
    if (!ti->isEnabled())
        return;

    // Switch phase to VM_Live and sent VMLive event
    ti->nextPhase(JVMTI_PHASE_LIVE);
    tmn_suspend_disable();
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_self()->object;
    tmn_suspend_enable();
    // Send VM_INIT TI events
    TIEnv *ti_env = ti->getEnvironments();
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_VM_INIT - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventVMInit func = (jvmtiEventVMInit)ti_env->get_event_callback(JVMTI_EVENT_VM_INIT);
            if (NULL != func)
            {
                TRACE2("jvmti.event.vi", "Callback JVMTI_EVENT_VM_INIT called");
                func((jvmtiEnv*)ti_env, jni_env, (jthread)hThread);
                TRACE2("jvmti.event.vi", "Callback JVMTI_EVENT_VM_INIT finished");
            }
        }
        ti_env = next_env;
    }
    assert(hythread_is_suspend_enabled());
    oh_discard_local_handle(hThread);
}

void jvmti_send_region_compiled_method_load_event(Method *method, uint32 codeSize, 
                                  const void* codeAddr, uint32 mapLength, 
                                  const AddrLocation* addrLocationMap, 
                                  const void* compileInfo)
{
    SuspendEnabledChecker sec;

    if (codeSize <= 0 || NULL == codeAddr)
        return;

    // convert AddrLocation[] to jvmtiAddrLocationMap[]
    jvmtiAddrLocationMap* jvmtiLocationMap = (jvmtiAddrLocationMap*) 
            STD_MALLOC( mapLength * sizeof(jvmtiAddrLocationMap) );
    for (uint32 i = 0; i < mapLength; i++) {
        jlocation location = (jlocation) addrLocationMap[i].location;

        if (location == 65535)
            location = -1;

        jvmtiLocationMap[i].location = location;

        jvmtiLocationMap[i].start_address = addrLocationMap[i].start_addr;
    }

    TIEnv *ti_env = VM_Global_State::loader_env->TI->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_COMPILED_METHOD_LOAD - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventCompiledMethodLoad func =
                (jvmtiEventCompiledMethodLoad)ti_env->get_event_callback(JVMTI_EVENT_COMPILED_METHOD_LOAD);
            if (NULL != func)
            {
                TRACE2("jvmti.event.cml",
                    "Callback JVMTI_EVENT_COMPILED_METHOD_LOAD calling, method = " <<
                    method->get_class()->get_name()->bytes << "." << method->get_name()->bytes <<
                    method->get_descriptor()->bytes);

                func((jvmtiEnv*)ti_env, (jmethodID)method, codeSize,
                    codeAddr, mapLength, jvmtiLocationMap, NULL);

                TRACE2("jvmti.event.cml",
                    "Callback JVMTI_EVENT_COMPILED_METHOD_LOAD finished, method = " <<
                    method->get_class()->get_name()->bytes << "." << method->get_name()->bytes <<
                    method->get_descriptor()->bytes);
            }
        }

        ti_env = next_env;
    }

    STD_FREE(jvmtiLocationMap);
} // jvmti_send_region_compiled_method_load_event

void jvmti_send_inlined_compiled_method_load_event(Method *method)
{
    SuspendEnabledChecker sec;

    method->send_inlined_method_load_events(method);
} // jvmti_send_inlined_compiled_method_load_event

void jvmti_send_chunks_compiled_method_load_event(Method *method)
{
    SuspendEnabledChecker sec;

    TIEnv *ti_env = VM_Global_State::loader_env->TI->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_COMPILED_METHOD_LOAD - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventCompiledMethodLoad func =
                (jvmtiEventCompiledMethodLoad)ti_env->get_event_callback(JVMTI_EVENT_COMPILED_METHOD_LOAD);
            if (NULL != func)
            {
                TRACE2("jvmti.event.cml",
                    "Emitting JVMTI_EVENT_COMPILED_METHOD_LOAD for chuncks of " <<
                    method->get_class()->get_name()->bytes << "." << method->get_name()->bytes <<
                    method->get_descriptor()->bytes);

                for (CodeChunkInfo* cci = method->get_first_JIT_specific_info();  cci;  cci = cci->_next)
                {
                    // FIXME64: no support for large methods
                    // with compiled code size greater than 2GB
                    jint code_size = (jint)cci->get_code_block_size();
                    const void* code_addr = cci->get_code_block_addr();

                    if ( code_size <= 0 || code_addr == NULL)
                        continue;

                    JIT* jit = cci->get_jit();

                    jvmtiAddrLocationMap* jvmtiLocationMap = (jvmtiAddrLocationMap*) 
                            STD_MALLOC( code_size * sizeof(jvmtiAddrLocationMap) );

                    jint map_length = 0;

                    for (int i = 0; i < code_size; i++)
                    {
                        NativeCodePtr ip = (NativeCodePtr) (((uint8*) code_addr) + i);
                        uint16 bc = 12345;

                        OpenExeJpdaError UNREF result =
                            jit->get_bc_location_for_native(
                                method, ip, &bc);

                        jlocation location = (jlocation) bc;

                        if (result != EXE_ERROR_NONE) 
                            location = -1;

                        if ( i == 0 || location != jvmtiLocationMap[map_length - 1].location )
                        {
                            jvmtiLocationMap[map_length].location = location;
                            jvmtiLocationMap[map_length].start_address = ip;
                            map_length ++;
                        }
                    }

                    func((jvmtiEnv*)ti_env, (jmethodID)method, code_size,
                            code_addr, map_length, jvmtiLocationMap, NULL);

                    STD_FREE(jvmtiLocationMap);
                }

                TRACE2("jvmti.event.cml",
                    "Emitting JVMTI_EVENT_COMPILED_METHOD_LOAD done for chuncks of " <<
                    method->get_class()->get_name()->bytes << "." << method->get_name()->bytes <<
                    method->get_descriptor()->bytes);
            }
        }

        ti_env = next_env;
    }
} // jvmti_send_chunks_compiled_method_load_event

void jvmti_send_dynamic_code_generated_event(const char *name, const void *address, jint length)
{
    TIEnv *ti_env = VM_Global_State::loader_env->TI->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_DYNAMIC_CODE_GENERATED - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventDynamicCodeGenerated func =
                (jvmtiEventDynamicCodeGenerated)ti_env->get_event_callback(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
            if (NULL != func)
            {
                TRACE2("jvmti.event.dcg", "Callback JVMTI_EVENT_DYNAMIC_CODE_GENERATED called");
                func((jvmtiEnv*)ti_env, name, address, length);
                TRACE2("jvmti.event.dcg", "Callback JVMTI_EVENT_DYNAMIC_CODE_GENERATED finished");
            }
        }

        ti_env = next_env;
    }
}

static jvmtiError generate_events_compiled_method_load(jvmtiEnv* env)
{
    // Capabilities checking
    jvmtiCapabilities capa;
    jvmtiGetCapabilities(env, &capa);
    if (!capa.can_generate_compiled_method_load_events)
        return JVMTI_ERROR_MUST_POSSESS_CAPABILITY;

    ClassLoader::LockLoadersTable();

    /**
    * First class loader is bootstrap class loader
    */
    int index = 0;
    int cl_count = ClassLoader::GetClassLoaderNumber();
    ClassLoader *classloader = VM_Global_State::loader_env->bootstrap_class_loader;
    do
    {
        /**
        * Create jclass handle for classes and set in jclass table
        */
        classloader->Lock();
        ClassTable* tbl;
        Class **klass;
        ClassTable::iterator it;
        tbl = classloader->GetLoadedClasses();
        for(it = tbl->begin(); it != tbl->end(); it++)
        {
            klass = &it->second;
            TRACE2("jvmti.event", "Class = " << (*klass)->get_name()->bytes);
            if((*klass)->is_primitive())
                continue;
            if((*klass)->get_class_loader() != classloader)
                continue;
            if((*klass)->is_at_least_prepared())
                for (int jjj = 0; jjj < (*klass)->get_number_of_methods(); jjj++)
                {
                    Method* method = (*klass)->get_method(jjj);
                    TRACE2("jvmti.event", "    Method = " << method->get_name()->bytes <<
                        method->get_descriptor()->bytes <<
                        (method->get_state() == Method::ST_Compiled ? " compiled" : " not compiled"));
                    if (method->get_state() == Method::ST_Compiled) {
                        jvmti_send_chunks_compiled_method_load_event(method);
                        jvmti_send_inlined_compiled_method_load_event(method);
                    }
                }
        }
        classloader->Unlock();

        /**
        * Get next class loader
        */
        if( index < cl_count ) {
            classloader = (ClassLoader::GetClassLoaderTable())[index++];
        } else {
            break;
        }
    } while(true);

    ClassLoader::UnlockLoadersTable();

    return JVMTI_ERROR_NONE;
} // generate_events_compiled_method_load

static jvmtiError generate_events_dynamic_code_generated(jvmtiEnv* env)
{
    // FIXME: linked list usage without sync
    for (DynamicCode *dcList = compile_get_dynamic_code_list();
        NULL != dcList; dcList = dcList->next)
        jvmti_send_dynamic_code_generated_event(dcList->name,
            dcList->address, (jint)dcList->length);

    return JVMTI_ERROR_NONE;
} // generate_events_dynamic_code_generated

/*
 * Generate Events
 *
 * Generate events to represent the current state of the VM.
 * For example, if event_type is JVMTI_EVENT_COMPILED_METHOD_LOAD,
 * a CompiledMethodLoad event will be sent for each currently
 * compiled method.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiGenerateEvents(jvmtiEnv* env,
                    jvmtiEvent event_type)
{
    TRACE2("jvmti.event", "GenerateEvents called");
    SuspendEnabledChecker sec;
    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    // FIXME: Valentin Al. Sitnick
    // Additional conditions for checking of event type.
    // It is needed because between JVMTI_MIN_EVENT_TYPE_VAL
    // and JVMTI_MAX_EVENT_TYPE_VAL there are some values
    // for reserved and unsupported in current JVMTI version
    // event types: 72, 77, 78, 79, 80.
    if (event_type < JVMTI_MIN_EVENT_TYPE_VAL ||
           event_type > JVMTI_MAX_EVENT_TYPE_VAL ||
           72 == event_type || ((event_type >= 77) && (event_type <= 80)))
        return JVMTI_ERROR_INVALID_EVENT_TYPE;

    if (JVMTI_EVENT_COMPILED_METHOD_LOAD == event_type) {
        return generate_events_compiled_method_load(env);

    } else if (JVMTI_EVENT_DYNAMIC_CODE_GENERATED == event_type) {
        return generate_events_dynamic_code_generated(env);
    }

    return JVMTI_ERROR_ILLEGAL_ARGUMENT;
} // jvmtiGenerateEvents

VMEXPORT void
jvmti_process_method_entry_event(jmethodID method) {
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_METHOD_ENTRY))
        return;

    tmn_suspend_enable();
    jvmtiEvent event_type = JVMTI_EVENT_METHOD_ENTRY;
    hythread_t curr_thread = hythread_self();

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        // check that event is enabled in this environment.
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) {
            TIEventThread *thr = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr)
            {
                if (thr->thread == curr_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }

        // event is enabled in this environment
        jthread thread = getCurrentThread();
        JNIEnv *jni_env = p_TLS_vmthread->jni_env;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.MethodEntry)
            ti_env->event_table.MethodEntry(jvmti_env, jni_env, thread, method);

        ti_env = next_env;
    }
    tmn_suspend_disable();
}

static void
jvmti_process_method_exit_event_internal(jmethodID method,
                                         jboolean was_popped_by_exception,
                                         jvalue ret_val)
{
    // processing MethodExit event
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    tmn_suspend_enable();
    jvmtiEvent event_type = JVMTI_EVENT_METHOD_EXIT;
    hythread_t curr_native_thread = hythread_self();

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        // check that event is enabled in this environment.
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) {
            TIEventThread *thr = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr) {
                if (thr->thread == curr_native_thread) break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }

        // event is enabled in this environment
        jthread thread = getCurrentThread();
        JNIEnv *jni_env = p_TLS_vmthread->jni_env;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;
        if (NULL != ti_env->event_table.MethodExit) {
            TRACE2("jvmti.stack", "Calling MethodExit callback for method: "
                << class_get_name(method_get_class((Method*)method))
                << "." << method_get_name((Method*)method)
                << method_get_descriptor((Method*)method));
            ti_env->event_table.MethodExit(jvmti_env, jni_env, thread, method, was_popped_by_exception, ret_val);
        }
        ti_env = next_env;
    }

    if (interpreter_enabled())
    {
        tmn_suspend_disable();
        return;
    }


    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_FRAME_POP_NOTIFICATION))
    {
        tmn_suspend_disable();
        return;
    }

    // processing PopFrame event
    VM_thread *curr_thread = p_TLS_vmthread;
    jint depth = get_thread_stack_depth(curr_thread);

#ifndef NDEBUG
    if( curr_thread->jvmti_thread.frame_pop_listener ) {
        TRACE2("jvmti.stack", "Prepare to PopFrame callback for thread: "
            << curr_thread << ", method: " << class_get_name(method_get_class((Method*)method))
            << "." << method_get_name((Method*)method)
            << method_get_descriptor((Method*)method)
            << ", depth: " << depth
            << (was_popped_by_exception == JNI_TRUE ? " by exception" : ""));
    }
#endif // NDEBUG

    jvmti_frame_pop_listener *last = NULL;
    for( jvmti_frame_pop_listener *fpl = curr_thread->jvmti_thread.frame_pop_listener;
         fpl;
         last = fpl, (fpl = fpl ? fpl->next : curr_thread->jvmti_thread.frame_pop_listener) )
    {
        TRACE2("jvmti.stack", "-> Look through listener: "
            << fpl << ", env: " << fpl->env << ", depth: " << fpl->depth);

        if (fpl->depth == depth)
        {
            jvmti_frame_pop_listener *report = fpl;
            if(last) {
                last->next = fpl->next;
            } else {
                curr_thread->jvmti_thread.frame_pop_listener = fpl->next;
            }
            fpl = last;

            TRACE2("jvmti.stack", "Calling PopFrame callback for thread: "
                << curr_thread << ", listener: " << report
                << ", env: " << report->env << ", depth: " << report->depth
                << " -> " << class_get_name(method_get_class((Method*)method))
                << "." << method_get_name((Method*)method)
                << method_get_descriptor((Method*)method));

            jvmti_process_frame_pop_event(
                reinterpret_cast<jvmtiEnv *>(report->env),
                method,
                was_popped_by_exception);
            STD_FREE(report);
        }
    }

    tmn_suspend_disable();
}

VMEXPORT void
jvmti_process_method_exit_event(jmethodID method,
                                jboolean was_popped_by_exception,
                                jvalue ret_val)
{
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_METHOD_EXIT))
        return;

    // process method exit event
    jvmti_process_method_exit_event_internal(method, was_popped_by_exception, ret_val);
}

VMEXPORT void
jvmti_process_method_exception_exit_event(jmethodID method,
                                          jboolean was_popped_by_exception,
                                          jvalue ret_val,
                                          StackIterator* si)
{
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_METHOD_EXIT))
        return;

    // save context from stack iteration to m2n frame
    Registers regs;
    si_copy_to_registers(si, &regs);
    M2nFrame* m2nf = m2n_get_last_frame();
    set_pop_frame_registers(m2nf, &regs);
    
    // save old type of m2n frame
    frame_type old_type = m2n_get_frame_type(m2nf);
    
    // enable modified stack context in m2n frame 
    m2n_set_frame_type(m2nf, frame_type(FRAME_MODIFIED_STACK | old_type));

    // process method exit event
    jvmti_process_method_exit_event_internal(method, was_popped_by_exception, ret_val);

    // restore old frame type and switch off context modification
    m2n_set_frame_type(m2nf, old_type);
    set_pop_frame_registers(m2nf, NULL);
}

VMEXPORT void
jvmti_process_frame_pop_event(jvmtiEnv *jvmti_env, jmethodID method, jboolean was_popped_by_exception)
{
    assert(hythread_is_suspend_enabled());

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() ) return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    TIEnv *ti_env = (TIEnv*) jvmti_env;
    jthread thread = getCurrentThread();
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    assert(method);
    TRACE2("jvmti.event.popframe", "PopFrame event is called for method:"
        << class_get_name(method_get_class((Method*)method)) << "."
        << method_get_name((Method*)method)
        << method_get_descriptor((Method*)method) );

    if (NULL != ti_env->event_table.FramePop)
        ti_env->event_table.FramePop(jvmti_env, jni_env, thread, method,
            was_popped_by_exception);

}

VMEXPORT void
jvmti_process_native_method_bind_event(jmethodID method, NativeCodePtr address, NativeCodePtr* new_address_ptr)
{
    SuspendEnabledChecker sec;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if( !ti->isEnabled() )
        return;

    //Checking current phase
    jvmtiPhase phase = ti->getPhase();
    if( phase != JVMTI_PHASE_START && phase != JVMTI_PHASE_LIVE &&
        phase != JVMTI_PHASE_PRIMORDIAL)
        return;

    hythread_t thread = hythread_self();
    jthread j_thread = jthread_get_java_thread(thread);
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    TIEnv* next_env;
    for (TIEnv* env = ti->getEnvironments(); env; env = next_env)
    {
        next_env = env->next;

        jvmtiEventNativeMethodBind callback =
            env->event_table.NativeMethodBind;

        if (NULL == callback)
            continue;

        if (env->global_events[JVMTI_EVENT_NATIVE_METHOD_BIND - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            TRACE2("jvmti.event.bind", "Calling global NativeMethodBind event for method:"
                << (method ? class_get_name(method_get_class((Method*)method)) : "(nil)") << "." 
                << (method ? method_get_name((Method*)method) : "(nil)") 
                << (method ? method_get_descriptor((Method*)method) : "" ) );


            callback((jvmtiEnv*)env, jni_env,
                j_thread, method, address, new_address_ptr);

            continue;
        }

        TIEventThread *next_et;
        TIEventThread *first_et =
            env->event_threads[JVMTI_EVENT_NATIVE_METHOD_BIND - JVMTI_MIN_EVENT_TYPE_VAL];

        for (TIEventThread *et = first_et; NULL != et; et = next_et)
        {
            next_et = et->next;

            if (et->thread == thread)
            {
                TRACE2("jvmti.event.bind", "Calling local NativeMethodBind event for method:"
                    << (method ? class_get_name(method_get_class((Method*)method)) : "(nil)") << "." 
                    << (method ? method_get_name((Method*)method) : "(nil)") 
                    << (method ? method_get_descriptor((Method*)method) : "" ) );


                callback((jvmtiEnv *)env, jni_env,
                    j_thread, method, address, new_address_ptr);
            }
        }
    }
}

VMEXPORT void
jvmti_process_single_step_event(jmethodID method, jlocation location) {
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if ( !ti->isEnabled() ) return;

    if (ti->getPhase() != JVMTI_PHASE_LIVE) return;

    // Check for global flag
    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_SINGLE_STEP))
        return;

    jvmtiEvent event_type = JVMTI_EVENT_SINGLE_STEP;
    hythread_t curr_thread = hythread_self();

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        // check that event is enabled in this environment.
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) {
            TIEventThread *thr = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr)
            {
                if (thr->thread == curr_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }

        // event is enabled in this environment
        jthread thread = getCurrentThread();
        JNIEnv *jni_env = p_TLS_vmthread->jni_env;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        TRACE2("jvmti.break.ss", "Calling SingleStep callback for env " << jvmti_env << ": " <<
            class_get_name(method_get_class((Method*)method)) << "." <<
            method_get_name((Method*)method) <<
            method_get_descriptor((Method*)method) << " :" << location);

        if (NULL != ti_env->event_table.SingleStep)
            ti_env->event_table.SingleStep(jvmti_env, jni_env, thread, method, location);

        TRACE2("jvmti.break.ss", "Finished SingleStep callback for env " << jvmti_env << ": " <<
            class_get_name(method_get_class((Method*)method)) << "." <<
            method_get_name((Method*)method) <<
            method_get_descriptor((Method*)method) << " :" << location);
        ti_env = next_env;
    }
}

VMEXPORT void jvmti_process_field_access_event(Field_Handle field,
    jmethodID method, jlocation location, ManagedObject* managed_object)
{
    SuspendDisabledChecker sdc;

    // create handle for object
    jobject object = NULL;

    if (NULL != managed_object) {
        object = oh_allocate_local_handle();
        object->object = managed_object;
    }

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_FIELD_ACCESS_EVENT))
        return;

    tmn_suspend_enable();

    // get field class
    //Type_Info_Handle field_type = field_get_type_info_of_field_value(field);
    //Class_Handle clss = type_info_get_class(field_type);
    //ASSERT(clss, "Can't get class handle for field type.");
    //jclass field_klass = struct_Class_to_java_lang_Class_Handle(clss);

    // field_klass seems to be declaring class, not the field type
    jclass field_klass = struct_Class_to_java_lang_Class_Handle(field->get_class());

    jvmtiEvent event_type = JVMTI_EVENT_FIELD_ACCESS;
    hythread_t curr_thread = hythread_self();
    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        // check that event is enabled in this environment.
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) {
            TIEventThread *thr = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr)
            {
                if (thr->thread == curr_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }

        // event is enabled in this environment
        jthread thread = getCurrentThread();
        JNIEnv *jni_env = p_TLS_vmthread->jni_env;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.FieldAccess)
            ti_env->event_table.FieldAccess(jvmti_env, jni_env, thread,
                    method, location, field_klass, object, (jfieldID) field);
        ti_env = next_env;
    }

    tmn_suspend_disable();
} // jvmti_process_field_access_event

VMEXPORT void jvmti_process_field_modification_event(Field_Handle field,
    jmethodID method, jlocation location, ManagedObject* managed_object, jvalue new_value)
{
    SuspendDisabledChecker sdc;

    // create handle for object
    jobject object = NULL;

    if (NULL != managed_object) {
        object = oh_allocate_local_handle();
        object->object = managed_object;
    }

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_FIELD_MODIFICATION_EVENT))
        return;

    // get field class
    tmn_suspend_enable();

    //Type_Info_Handle field_type = field_get_type_info_of_field_value(field);
    //Class_Handle clss = type_info_get_class(field_type);
    //ASSERT(clss, "Can't get class handle for field type.");
    //jclass field_klass = struct_Class_to_java_lang_Class_Handle(clss);

    // field_klass seems to be declaring class, not the field type
    jclass field_klass = struct_Class_to_java_lang_Class_Handle(field->get_class());

    // get signature type
    char signature_type = (char) field->get_java_type();

    jvmtiEvent event_type = JVMTI_EVENT_FIELD_MODIFICATION;
    hythread_t curr_thread = hythread_self();
    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        // check that event is enabled in this environment.
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) {
            TIEventThread *thr = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr)
            {
                if (thr->thread == curr_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }

        // event is enabled in this environment
        jthread thread = getCurrentThread();
        JNIEnv *jni_env = p_TLS_vmthread->jni_env;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.FieldModification)
            ti_env->event_table.FieldModification(jvmti_env, jni_env, thread,
                    method, location, field_klass, object, (jfieldID) field,
                    signature_type, new_value);
        ti_env = next_env;
    }

    tmn_suspend_disable();
} // jvmti_process_field_modification_event

static void jvmti_process_vm_object_alloc_event(ManagedObject** p_managed_object,
    Class* object_clss, unsigned size)
{
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    // check the j.l.Thread is already initialized
    hythread_t curr_thread = hythread_self();
    jthread thread = jthread_get_java_thread(curr_thread);
    if (NULL == thread)
        return;

    ManagedObject* managed_object = *p_managed_object;
    // create handle for object
    jobject object = NULL;

    if (NULL != managed_object) {
        object = oh_allocate_local_handle();
        object->object = managed_object;
    }

    tmn_suspend_enable();

    // get class
    jclass klass = struct_Class_to_java_lang_Class_Handle(object_clss);

    jvmtiEvent event_type = JVMTI_EVENT_VM_OBJECT_ALLOC;
    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        // check that event is enabled in this environment.
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]) {
            TIEventThread *thr = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr)
            {
                if (thr->thread == curr_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }

        // event is enabled in this environment
        JNIEnv *jni_env = p_TLS_vmthread->jni_env;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.VMObjectAlloc)
            ti_env->event_table.VMObjectAlloc(jvmti_env, jni_env, thread,
                    object, klass, (jlong)size);

        ti_env = next_env;
    }

    tmn_suspend_disable();

    // restore object pointer. As et could be relocated by GC.
    *p_managed_object = object->object;
} // jvmti_process_vm_object_alloc_event

VMEXPORT Managed_Object_Handle vm_alloc_and_report_ti(unsigned size, 
    Allocation_Handle p_vtable, void *thread_pointer, Class* object_class)
{
    SuspendDisabledChecker sdc;
    Managed_Object_Handle handle = gc_alloc(size, p_vtable, thread_pointer);

    if (handle) 
        jvmti_process_vm_object_alloc_event((ManagedObject**) &handle, object_class, size);

    return handle;
} // vm_alloc_and_report_ti

/*
 * Send Exception event
 */
void jvmti_send_exception_event(jthrowable exn_object,
    Method *method, jlocation location,
    Method *catch_method, jlocation catch_location)
{
    assert(!exn_raised());
    assert(!hythread_is_suspend_enabled());

    VM_thread *curr_thread = p_TLS_vmthread;
    hythread_t curr_native_thread=hythread_self();

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    assert(ti->isEnabled());

    // Create local handles frame
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_get_java_thread(curr_native_thread)->object;

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (!ti_env->global_events[JVMTI_EVENT_EXCEPTION -
                JVMTI_MIN_EVENT_TYPE_VAL]) {
            // No global event set; then check thread specific events.
            TIEventThread *thr =
                ti_env->event_threads[JVMTI_EVENT_EXCEPTION -
                JVMTI_MIN_EVENT_TYPE_VAL];
            while (thr)
            {
                if (thr->thread == curr_native_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }
        jvmtiEventException func =
            (jvmtiEventException)ti_env->get_event_callback(JVMTI_EVENT_EXCEPTION);
        if (NULL != func) {
            tmn_suspend_enable();
            assert(hythread_is_suspend_enabled());
            BEGIN_RAISE_AREA;

            func((jvmtiEnv *)ti_env, jni_env,
                reinterpret_cast<jthread>(hThread),
                reinterpret_cast<jmethodID>(method),
                location, exn_object,
                reinterpret_cast<jmethodID>(catch_method),
                catch_location);

            END_RAISE_AREA;
            tmn_suspend_disable();
        }
        ti_env = next_env;
    }
}

void jvmti_interpreter_exception_event_callback_call(
        ManagedObject *exc, Method *method, jlocation location,
        Method *catch_method, jlocation catch_location)
{
    assert(!exn_raised());
    SuspendDisabledChecker sdc;

    VM_thread *curr_thread = p_TLS_vmthread;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return;

    assert(!exn_raised());

    ObjectHandle exception = oh_allocate_local_handle();
    exception->object = exc;

    jvmti_send_exception_event((jthrowable) exception, method, location,
        catch_method, catch_location);
    assert(!hythread_is_suspend_enabled());

    assert(!exn_raised());
}

bool jvmti_is_exception_event_requested()
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return false;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return false;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return false;

    return true;
}

ManagedObject *jvmti_jit_exception_event_callback_call(ManagedObject *exn_object,
    JIT *jit, Method *method, NativeCodePtr native_location,
    JIT *catch_jit, Method *catch_method, NativeCodePtr native_catch_location)
{
    assert(!exn_raised());
    assert(exn_object);
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return exn_object;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return exn_object;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return exn_object;

    jlocation location = -1, catch_location = -1;

    uint16 bc = 0;
    if (NULL != jit)
    {
        OpenExeJpdaError UNREF result = jit->get_bc_location_for_native(
            method, native_location, &bc);
        TRACE2("jvmti.event.exn",
            "Exception method = " << (Method_Handle)method << " location " << bc);
        if (EXE_ERROR_NONE != result)
            LWARN(38, "JIT {0} {1} returned error {2} for exception method {3} location {4}" 
                << jit << "get_bc_location_for_native"
                << result << (Method_Handle)method << native_location);
        location = bc;
    }

    if (catch_method)
    {
        bc = 0;
        OpenExeJpdaError UNREF result = catch_jit->get_bc_location_for_native(
            catch_method, native_catch_location, &bc);
        TRACE2("jvmti.event.exn",
            "Exception catch method = " << (Method_Handle)catch_method <<
            " location " << bc);
        if (EXE_ERROR_NONE != result)
            LWARN(39, "JIT {0} {1} returned error {2} for catch method {3} location {4}"
                     << jit << "get_bc_location_for_native" << result
                     << (Method_Handle)catch_method << native_catch_location);
        catch_location = bc;
    }

    ObjectHandle exception = oh_allocate_local_handle();
    exception->object = exn_object;

    jvmti_send_exception_event(exception, method, location,
        catch_method, catch_location);

    assert(!hythread_is_suspend_enabled());
    return exception->object;
}

ManagedObject *jvmti_exception_catch_event_callback_call(ManagedObject *exn,
    Method *catch_method, jlocation catch_location)
{
    assert(!exn_raised());
    SuspendDisabledChecker sec;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    assert(ti->isEnabled());

    hythread_t curr_thread = hythread_self();

    // Create local handles frame
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    ObjectHandle exn_object = oh_allocate_local_handle();
    exn_object->object = exn;

    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_self()->object;

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (!ti_env->global_events[JVMTI_EVENT_EXCEPTION_CATCH -
                JVMTI_MIN_EVENT_TYPE_VAL]) {
            // No global event set; then check thread specific events.
            TIEventThread *thr =
                ti_env->event_threads[JVMTI_EVENT_EXCEPTION_CATCH -
                JVMTI_MIN_EVENT_TYPE_VAL];

            while (thr)
            {
                if (thr->thread == curr_thread)
                    break;
                thr = thr->next;
            }

            if (!thr)
            {
                ti_env = next_env;
                continue;
            }
        }
        jvmtiEventExceptionCatch func =
            (jvmtiEventExceptionCatch)ti_env->get_event_callback(JVMTI_EVENT_EXCEPTION_CATCH);
        if (NULL != func) {

            tmn_suspend_enable();
            assert(hythread_is_suspend_enabled());
            BEGIN_RAISE_AREA;

            func((jvmtiEnv *)ti_env, jni_env,
                reinterpret_cast<jthread>(hThread),
                reinterpret_cast<jmethodID>(catch_method),
                catch_location, exn_object);

            END_RAISE_AREA;
            exn_rethrow_if_pending();
            tmn_suspend_disable();
        }
        ti_env = next_env;
    }

    return exn_object->object;
}

ManagedObject *jvmti_jit_exception_catch_event_callback_call(ManagedObject *exn_object,
    JIT *catch_jit, Method *catch_method, NativeCodePtr native_catch_location)
{
    assert(!exn_raised());
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return exn_object;

 if (JVMTI_PHASE_LIVE != ti->getPhase())
        return exn_object;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return exn_object;

    uint16 bc = 0;
    jlocation catch_location;

    OpenExeJpdaError result = catch_jit->get_bc_location_for_native(
        catch_method, native_catch_location, &bc);
    TRACE2("jvmti.event.exn",
        "Exception method = " << (Method_Handle)catch_method << " location " << bc);
    if (EXE_ERROR_NONE != result)
        LWARN(38, "JIT {0} {1} returned error {2} for exception method {3} location {4}" 
                  << catch_jit << "get_bc_location_for_native"
                  << result << (Method_Handle)catch_method << native_catch_location);
    catch_location = bc;

    exn_object = jvmti_exception_catch_event_callback_call(exn_object,
        catch_method, catch_location);

    return exn_object;
}

void jvmti_interpreter_exception_catch_event_callback_call(
    ManagedObject *exc, Method *catch_method, jlocation catch_location)
{
    assert(!exn_raised());
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return;

    jvmti_exception_catch_event_callback_call(exc, catch_method, catch_location);
    assert(!exn_raised());
}

void jvmti_send_contended_enter_or_entered_monitor_event(jobject obj,
    bool isEnter)
{
    assert(hythread_is_suspend_enabled());
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled()) {
        return;
    }

    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    tmn_suspend_disable();
    // Create local handles frame
    NativeObjectHandles lhs;
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_self()->object;
    tmn_suspend_enable();

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (isEnter && ti_env->global_events[JVMTI_EVENT_MONITOR_CONTENDED_ENTER -
                JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventMonitorContendedEnter func = (jvmtiEventMonitorContendedEnter)
                ti_env->get_event_callback(JVMTI_EVENT_MONITOR_CONTENDED_ENTER);
            if (NULL != func)
            {
                func((jvmtiEnv*)ti_env, jni_env, (jthread)hThread, obj);
            }
        }
        else if (! isEnter && ti_env->global_events[JVMTI_EVENT_MONITOR_CONTENDED_ENTERED -
                JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventMonitorContendedEntered func = (jvmtiEventMonitorContendedEntered)
                ti_env->get_event_callback(JVMTI_EVENT_MONITOR_CONTENDED_ENTERED);
            if (NULL != func)
            {
                func((jvmtiEnv*)ti_env, jni_env, (jthread)hThread, obj);
            }
        }
        ti_env = next_env;
    }
}

void jvmti_send_class_load_event(const Global_Env* env, Class* clss)
{
    assert(hythread_is_suspend_enabled());
    if(clss->is_array() || clss->is_primitive()) {
        // array class creation and creation of a primitive class
        // do not generate a class load event.
        return;
    }
    // Send Class Load event
    DebugUtilsTI* ti = env->TI;
    if (!ti->isEnabled())
        return;

    if(ti->getPhase() <= JVMTI_PHASE_PRIMORDIAL) {
        // pending notify event
        ti->SetPendingNotifyLoadClass( clss );
        return;
    }

    JNIEnv *jni_env = p_TLS_vmthread->jni_env;
    ObjectHandle hClass = struct_Class_to_jclass(clss);

    tmn_suspend_disable();
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_self()->object;
    tmn_suspend_enable();

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        jvmtiEventClassLoad func =
            (jvmtiEventClassLoad)ti_env->get_event_callback(JVMTI_EVENT_CLASS_LOAD);
        if (NULL != func)
        {
            if (ti_env->global_events[JVMTI_EVENT_CLASS_LOAD - JVMTI_MIN_EVENT_TYPE_VAL])
            {
                TRACE2("jvmti.class.cl", "Class load event, class name = " << clss->get_name()->bytes);
                // fire global event
                func((jvmtiEnv *)ti_env, jni_env, (jthread)hThread, (jclass)hClass);
                ti_env = next_env;
                continue;
            }

            // fire local events
            for( TIEventThread* ti_et = ti_env->event_threads[JVMTI_EVENT_CLASS_LOAD - JVMTI_MIN_EVENT_TYPE_VAL];
                 ti_et != NULL; ti_et = ti_et->next )
                if (ti_et->thread == hythread_self())
                {
                    TRACE2("jvmti.class.cl", "Class load event, class name = " << clss->get_name()->bytes);
                    tmn_suspend_disable();
                    ObjectHandle hThreadLocal = oh_allocate_local_handle();
                    hThreadLocal->object = (Java_java_lang_Thread *)jthread_get_java_thread(ti_et->thread)->object;
                    tmn_suspend_enable();
                    func((jvmtiEnv *)ti_env, jni_env, (jthread)hThreadLocal, (jclass)hClass);
                    oh_discard_local_handle( hThreadLocal );
                }
        }
        ti_env = next_env;
    }
    oh_discard_local_handle(hThread);
    oh_discard_local_handle(hClass);
}


void jvmti_send_class_file_load_hook_event(const Global_Env* env,
    ClassLoader* loader,
    const char* classname,
    int classlen, unsigned char* classbytes,
    int* newclasslen, unsigned char** newclass)
{
    assert(hythread_is_suspend_enabled());
    // Send Class Load event
    DebugUtilsTI* ti = env->TI;
    if (!ti->isEnabled())
        return;

    JNIEnv *jni_env = p_TLS_vmthread->jni_env;

    ObjectHandle hLoader = NULL;
    if (! loader->IsBootstrap()) {
        tmn_suspend_disable();
        hLoader = oh_convert_to_local_handle((ManagedObject*)loader->GetLoader());
        tmn_suspend_enable();
    }

    jint input_len = classlen;
    jint output_len = 0;
    unsigned char* input_class = classbytes;
    unsigned char* output_class = NULL;
    unsigned char* last_redef = NULL;

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        jvmtiEventClassFileLoadHook func =
            (jvmtiEventClassFileLoadHook)ti_env->get_event_callback(JVMTI_EVENT_CLASS_FILE_LOAD_HOOK);
        if (NULL != func)
        {
            if (ti_env->global_events[JVMTI_EVENT_CLASS_FILE_LOAD_HOOK - JVMTI_MIN_EVENT_TYPE_VAL])
            {
                TRACE2("jvmti.class.cflh", "Class file load hook event, class name = " << classname);
                // fire global event
                func((jvmtiEnv*)ti_env, jni_env, NULL, (jobject)hLoader, classname, NULL,
                    input_len, input_class, &output_len, &output_class);
                // make redefined class an input for the next agent
                if( output_class ) {
                    input_len = output_len;
                    input_class = last_redef = output_class;
                    output_len = 0;
                    output_class = NULL;
                }
                *newclasslen = (input_class != classbytes)?input_len:0;
                *newclass =    (input_class != classbytes)?input_class:NULL;
                ti_env = next_env;
                continue;
            }
            // fire local events
            for( TIEventThread* ti_et = ti_env->event_threads[JVMTI_EVENT_CLASS_FILE_LOAD_HOOK - JVMTI_MIN_EVENT_TYPE_VAL];
                 ti_et != NULL; ti_et = ti_et->next )
                if (ti_et->thread == hythread_self())
                {
                    // free memory from previous redefinition
                    if( last_redef && last_redef != input_class ) {
                        ((jvmtiEnv*)ti_env)->Deallocate(last_redef);
                        last_redef = NULL;
                    }
                    TRACE2("jvmti.class.cflh", "Class file load hook event, class name = " << classname);
                    func((jvmtiEnv*)ti_env, jni_env, NULL, (jobject)hLoader, classname, NULL,
                        input_len, input_class, &output_len, &output_class);
                    // make redefined class an input for the next agent
                    if( output_class ) {
                        input_len = output_len;
                        input_class = last_redef = output_class;
                        output_len = 0;
                        output_class = NULL;
                    }
                    *newclasslen = (input_class != classbytes)?input_len:0;
                    *newclass =    (input_class != classbytes)?input_class:NULL;
                }
        }
        ti_env = next_env;
    }
}

void jvmti_send_class_prepare_event(Class* clss)
{
    assert(hythread_is_suspend_enabled());
    if(clss->is_array() || clss->is_primitive()) {
        // class prepare events are not generated for primitive classes
        // and arrays
        return;
    }
    // Send Class Load event
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    if(ti->getPhase() <= JVMTI_PHASE_PRIMORDIAL) {
        // pending notify event
        ti->SetPendingNotifyPrepareClass( clss );
        return;
    }

    JNIEnv *jni_env = p_TLS_vmthread->jni_env;
    ObjectHandle hClass = struct_Class_to_jclass(clss);
    tmn_suspend_disable(); // -----------vv
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)jthread_self()->object;
    tmn_suspend_enable();   // -----------^^

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        jvmtiEventClassLoad func =
            (jvmtiEventClassLoad)ti_env->get_event_callback(JVMTI_EVENT_CLASS_PREPARE);
        if (NULL != func)
        {
            if (ti_env->global_events[JVMTI_EVENT_CLASS_PREPARE - JVMTI_MIN_EVENT_TYPE_VAL])
            {
                TRACE2("jvmti.class.cp", "Class prepare event, class name = " << clss->get_name()->bytes);
                // fire global event
                func((jvmtiEnv *)ti_env, jni_env, (jthread)hThread, (jclass)hClass);
                TRACE2("jvmti.class.cp", "Class prepare event exited, class name = " << clss->get_name()->bytes);
                ti_env = next_env;
                continue;
            }
            // fire local events
            for(TIEventThread* ti_et = ti_env->event_threads[JVMTI_EVENT_CLASS_PREPARE - JVMTI_MIN_EVENT_TYPE_VAL];
                ti_et != NULL; ti_et = ti_et->next )
                if (ti_et->thread == hythread_self())
                {
                    TRACE2("jvmti.class.cp", "Class prepare event, class name = " << clss->get_name()->bytes);
                    tmn_suspend_disable(); // -------------------------------vv
                    ObjectHandle hThreadLocal = oh_allocate_local_handle();
                    hThreadLocal->object = (Java_java_lang_Thread *)jthread_self()->object;
                    tmn_suspend_enable(); // --------------------------------^^
                    func((jvmtiEnv *)ti_env, jni_env, (jthread)hThreadLocal, (jclass)hClass);
                    oh_discard_local_handle( hThreadLocal );
                }
        }
        ti_env = next_env;
    }
    oh_discard_local_handle(hThread);
    oh_discard_local_handle(hClass);
    assert(hythread_is_suspend_enabled());
}

static void call_callback(jvmtiEvent event_type, JNIEnv *jni_env, TIEnv *ti_env, 
        void *callback_func, va_list args) {
    assert(hythread_is_suspend_enabled());
    switch(event_type) {
        case JVMTI_EVENT_THREAD_START:
        case JVMTI_EVENT_THREAD_END: {
            ((jvmtiEventThreadStart)callback_func)((jvmtiEnv*)ti_env, jni_env, 
                    jthread_self());
            break;
        }
        case JVMTI_EVENT_MONITOR_CONTENDED_ENTER: 
        case JVMTI_EVENT_MONITOR_CONTENDED_ENTERED: {
            jobject monitor = va_arg(args, jobject);
            ((jvmtiEventMonitorContendedEnter) callback_func)((jvmtiEnv*)ti_env, 
                    jni_env, jthread_self(), monitor);

            break;
        }

        case JVMTI_EVENT_MONITOR_WAIT: {
            jobject monitor = va_arg(args, jobject);
            jlong timeout   = va_arg(args, jlong);
            ((jvmtiEventMonitorWait)callback_func)((jvmtiEnv*)ti_env, jni_env, 
                    jthread_self(), monitor, timeout);
            break;
        }

        case JVMTI_EVENT_MONITOR_WAITED: {
            jobject monitor = va_arg(args, jobject);
            jboolean is_timed_out   = va_arg(args, jint);
            ((jvmtiEventMonitorWaited)callback_func)((jvmtiEnv*)ti_env, jni_env, 
                    jthread_self(), monitor, is_timed_out);
            break;
        }
        default: 
            break;
    }
    assert(hythread_is_suspend_enabled());
}

static int is_interested_thread(TIEnv *ti_env, jvmtiEvent event_type) {
    for( TIEventThread* ti_et = ti_env->event_threads[event_type - JVMTI_MIN_EVENT_TYPE_VAL];
            ti_et != NULL; ti_et = ti_et->next) {
        if (ti_et->thread == hythread_self()) return 1;
    }
    
    return 0;    
}

static void process_jvmti_event(jvmtiEvent event_type, int per_thread, ...) {
    va_list args;
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;
    TIEnv *ti_env, *next_env;
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    void *callback_func;
    bool unwindable;
    jthrowable excn = NULL;

    if (! ti->isEnabled()) return;
    unwindable = set_unwindable(false);
    
    if (!unwindable) {
        if (excn = exn_get()) {
            exn_clear();
        }
    }

    ti_env = ti->getEnvironments();
    va_start(args, per_thread);
    while(ti_env) {
        next_env = ti_env->next;
        if (!ti_env->global_events[event_type - JVMTI_MIN_EVENT_TYPE_VAL]
                && (!per_thread || !is_interested_thread(ti_env, event_type))) {
           ti_env = ti_env->next;
           continue;   
        }
        
        callback_func = ti_env->get_event_callback(event_type);
        if (callback_func) call_callback(event_type, jni_env, ti_env, callback_func, args);
        ti_env = next_env;
    }
    
    assert(!exn_get());
    
    if (excn) exn_raise_object(excn);

    set_unwindable(unwindable);
    va_end(args);
}

void jvmti_send_thread_start_end_event(int is_start)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    if (JVMTI_PHASE_LIVE == ti->getPhase() ||
        JVMTI_PHASE_START == ti->getPhase())
    {
        process_jvmti_event(
            (is_start ? JVMTI_EVENT_THREAD_START : JVMTI_EVENT_THREAD_END),
            (is_start ? 0 : 1), 0);
    }

    if (!ti->is_single_step_enabled())
        return;

    jvmti_thread_t jvmti_thread = jthread_self_jvmti();
    if (is_start)
    {
        // Init single step state for the thread
        LMAutoUnlock lock(ti->vm_brpt->get_lock());

        jvmtiError UNREF errorCode = _allocate(sizeof(JVMTISingleStepState),
            (unsigned char **)&jvmti_thread->ss_state);
        assert(JVMTI_ERROR_NONE == errorCode);

        jvmti_thread->ss_state->predicted_breakpoints = NULL;

        // There is no need to set a breakpoint in a thread which
        // is started inside of jvmti_send_thread_start_end_event() function.
        // This function is called when no java code in the new thread is
        // executed yet, so this function just sets single step state for this
        // thread. When this thread will be ran, calling the first java method
        // will set a breakpoint on the first bytecode if this method.
    }
    else
    {
        // Shut down single step state for the thread
        LMAutoUnlock lock(ti->vm_brpt->get_lock());

        jvmti_remove_single_step_breakpoints(ti, jvmti_thread);
        if( jvmti_thread->ss_state ) {
            if( jvmti_thread->ss_state->predicted_breakpoints ) {
                ti->vm_brpt->release_intf(jvmti_thread->ss_state->predicted_breakpoints);
            }
            _deallocate((unsigned char *)jvmti_thread->ss_state);
            jvmti_thread->ss_state = NULL;
        }
    }
}

void jvmti_send_wait_monitor_event(jobject monitor, jlong timeout) {
    if (JVMTI_PHASE_LIVE != VM_Global_State::loader_env->TI->getPhase())
        return;

    TRACE2("jvmti.monitor.wait", "Monitor wait event, monitor = " << monitor);

    process_jvmti_event(JVMTI_EVENT_MONITOR_WAIT, 1, monitor, timeout);
}

void jvmti_send_waited_monitor_event(jobject monitor, jboolean is_timed_out) {
    if (JVMTI_PHASE_LIVE != VM_Global_State::loader_env->TI->getPhase())
        return;

    TRACE2("jvmti.monitor.waited", "Monitor wait event, monitor = " << monitor);

    process_jvmti_event(JVMTI_EVENT_MONITOR_WAITED, 1, monitor, is_timed_out);
}

void jvmti_send_contended_enter_or_entered_monitor_event(jobject monitor,
        int isEnter) {
    if (JVMTI_PHASE_LIVE != VM_Global_State::loader_env->TI->getPhase())
        return;

    TRACE2("jvmti.monitor.enter", "Monitor enter event, monitor = " << monitor << " is enter= " << isEnter);

    (isEnter)?process_jvmti_event(JVMTI_EVENT_MONITOR_CONTENDED_ENTER, 1, monitor)
            :process_jvmti_event(JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, 1, monitor);
}

void jvmti_send_vm_death_event()
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        // VM_DEATH must be the latest event.
        // Disable all events except VM_DEATH
        disable_all_events(ti_env);

        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_VM_DEATH - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventVMDeath func = (jvmtiEventVMDeath)ti_env->get_event_callback(JVMTI_EVENT_VM_DEATH);
            JNIEnv *jni_env = p_TLS_vmthread->jni_env;
            if (NULL != func)
            {
                TRACE2("jvmti.event.vd", "Callback JVMTI_PHASE_DEATH called");
                func((jvmtiEnv *)ti_env, jni_env);
                TRACE2("jvmti.event.vd", "Callback JVMTI_PHASE_DEATH finished");
            }
        }

        ti_env = next_env;
    }
    
    if (ti->is_single_step_enabled())
    {
        // Stop single step and remove all step breakpoints if there were some
        jvmtiError errorCode = ti->jvmti_single_step_stop();
        assert(JVMTI_ERROR_NONE == errorCode);
    }
    
    // Remove all other breakpoints
    for (ti_env = ti->getEnvironments(); ti_env; ti_env = ti_env->next)
    {
        if (ti_env->brpt_intf)
            ti_env->brpt_intf->remove_all_reference();
    }

    ti->nextPhase(JVMTI_PHASE_DEAD);
}

void jvmti_send_gc_start_event()
{
    Global_Env *env = VM_Global_State::loader_env;
    // this event is sent from stop-the-world setting
    assert(!hythread_is_suspend_enabled());

    DebugUtilsTI *ti = env->TI;
    if (!ti->isEnabled())
        return;

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_GARBAGE_COLLECTION_START - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventGarbageCollectionStart func = 
                (jvmtiEventGarbageCollectionStart)
                ti_env->get_event_callback(JVMTI_EVENT_GARBAGE_COLLECTION_START);

            if (NULL != func)
            {
                TRACE2("jvmti.event.gc", "Callback JVMTI_EVENT_GARBAGE_COLLECTION_START called");
                func((jvmtiEnv*)ti_env);
                TRACE2("jvmti.event.gc", "Callback JVMTI_EVENT_GARBAGE_COLLECTION_START finished");
            }
        }
        ti_env = next_env;
    }
}

void jvmti_send_gc_finish_event()
{
    Global_Env *env = VM_Global_State::loader_env;
    // this event is sent from stop-the-world setting
    assert(!hythread_is_suspend_enabled());

    DebugUtilsTI *ti = env->TI;
    if (!ti->isEnabled())
        return;

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_GARBAGE_COLLECTION_FINISH - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventGarbageCollectionFinish func = 
                (jvmtiEventGarbageCollectionFinish)
                ti_env->get_event_callback(JVMTI_EVENT_GARBAGE_COLLECTION_FINISH);

            if (NULL != func)
            {
                TRACE2("jvmti.event.gc", "Callback JVMTI_EVENT_GARBAGE_COLLECTION_FINISH called");
                func((jvmtiEnv*)ti_env);
                TRACE2("jvmti.event.gc", "Callback JVMTI_EVENT_GARBAGE_COLLECTION_FINISH finished");
            }
        }
        ti_env = next_env;
    }
}

static void
jvmti_process_data_dump_request()
{
    assert(hythread_is_suspend_enabled());
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if( !ti->isEnabled() ) {
        return;
    }

    //Checking current phase
    jvmtiPhase phase = ti->getPhase();
    if( ti->getPhase() != JVMTI_PHASE_LIVE ) {
        return;
    }
    hythread_t thread = hythread_self();

    TIEnv* next_env;
    for (TIEnv* env = ti->getEnvironments(); env; env = next_env) {
        next_env = env->next;

        jvmtiEventDataDumpRequest callback = env->event_table.DataDumpRequest;

        if( NULL == callback ) {
            continue;
        }

        if( env->global_events[JVMTI_EVENT_DATA_DUMP_REQUEST
                               - JVMTI_MIN_EVENT_TYPE_VAL] )
        {
            TRACE2("jvmti.event", "Calling global DataDumpRequest event");
            callback((jvmtiEnv*)env);
            continue;
        }

        TIEventThread *first_et =
            env->event_threads[JVMTI_EVENT_DATA_DUMP_REQUEST - JVMTI_MIN_EVENT_TYPE_VAL];
        for( TIEventThread *et = first_et; NULL != et; et = et->next )
        {
            if( et->thread == thread ) {
                TRACE2("jvmti.event", "Calling local DataDumpRequest event");
                callback((jvmtiEnv*)env);
            }
        }
    }
}

static IDATA VMCALL
jvmti_event_thread_function(void *args)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    JNIEnv* jni_env = (JNIEnv*)args;
    JavaVM* java_vm;
    jni_env->GetJavaVM(&java_vm);

    // attaching native thread to VM
    JavaVMAttachArgs vm_args = {JNI_VERSION_1_2, "TIEventThread", NULL};
    jint status = AttachCurrentThreadAsDaemon(java_vm, (void**)&jni_env, &vm_args);
    if(status != JNI_OK) {
        LDIE(24, "{0} cannot attach current thread" << "jvmti_event_thread_function:");
    }

    assert(hythread_is_suspend_enabled());

    // create wait loop environment
    hymutex_t event_mutex;
    UNREF IDATA stat = hymutex_create(&event_mutex, TM_MUTEX_NESTED);
    assert(stat == TM_ERROR_NONE);
    stat = hycond_create(&ti->event_cond);
    assert(stat == TM_ERROR_NONE);

    // event thread loop
    while(true) {
        hymutex_lock(&event_mutex);
        hycond_wait(&ti->event_cond, &event_mutex);
        hymutex_unlock(&event_mutex);

        if(!ti->event_thread) {
            // event thread is NULL,
            // it's time to terminate the current thread
            break;
        }

        // process data dump request
        jvmti_process_data_dump_request();
    }

    // release wait loop environment
    stat = hymutex_destroy(&event_mutex);
    assert(stat == TM_ERROR_NONE);
    stat = hycond_destroy(&ti->event_cond);
    assert(stat == TM_ERROR_NONE);

    return 0;
}

void
jvmti_create_event_thread()
{
    // IMPORTANT: The function is called under TIenvs_lock
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if(ti->event_thread) {
        // event thread is already created
        return;
    }

    if(ti->getPhase() < JVMTI_PHASE_LIVE) {
        ti->enableEventThreadCreation();
        return;
    }

    // create TI event thread
    JNIEnv *jni_env = p_TLS_vmthread->jni_env;
    ti->event_thread = (hythread_t)STD_CALLOC(1, hythread_get_struct_size());
    assert(ti->event_thread);
    IDATA status = hythread_create_with_group(ti->event_thread, NULL, 0, 0,
        jvmti_event_thread_function, jni_env);
    if( status != TM_ERROR_NONE ) {
        DIE("jvmti_create_event_thread: creating thread is failed!");
    }
    return;
}

void
jvmti_destroy_event_thread()
{
    // IMPORTANT: The function is called under TIenvs_lock
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if(!ti->event_thread) {
        // event thread is already destroyed
        return;
    }

    // notify event thread
    UNREF IDATA stat = hycond_notify(&ti->event_cond);
    assert(stat == TM_ERROR_NONE);
    return;
}

void
jvmti_notify_data_dump_request()
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if( !ti->event_thread) {
        // nothing to do
        return;
    }
    UNREF IDATA stat = hycond_notify(&ti->event_cond);
    assert(stat == TM_ERROR_NONE);
    return;
}
