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
 * @version $Revision: 1.1.2.2.4.5 $
 */  
/*
 * JVMTI events API
 */

#define LOG_DOMAIN "jvmti"
#include "cxxlog.h"

#include "jvmti_direct.h"
#include "jvmti_internal.h"
#include "jvmti_utils.h"
#include "thread_generic.h"
#include "environment.h"
#include "interpreter_exports.h"
#include "interpreter_imports.h"
#include "classloader.h"
#include "open/thread.h"
#include "suspend_checker.h"
#include "jit_intf_cpp.h"
#include "vm_log.h"

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
    JNIEnv *jni_env = jni_native_intf;
    VM_thread *p_thread = get_vm_thread_ptr_safe(jni_env, event_thread);
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
    JNIEnv *jni_env = jni_native_intf;
    VM_thread *p_thread = get_vm_thread_ptr_safe(jni_env, event_thread);
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

        jint thread_state = thread_get_thread_state(event_thread);

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

    if (event_enabled(event_type) != (jboolean)old_state) {
        if (interpreter_enabled())
            interpreter.interpreter_ti_set_notification_mode(event_type, !old_state);
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
    assert(tmn_is_suspend_enabled());
    // Switch phase to VM_Live event
    DebugUtilsTI *ti = env->TI;
    if (!ti->isEnabled())
        return;

    // Switch phase to VM_Live and sent VMLive event
    ti->nextPhase(JVMTI_PHASE_LIVE);
    tmn_suspend_disable();
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)p_TLS_vmthread->p_java_lang_thread;
    tmn_suspend_enable();
    // Send VM_INIT TI events
    TIEnv *ti_env = ti->getEnvironments();
    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
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
    assert(tmn_is_suspend_enabled());
    oh_discard_local_handle(hThread);
}

void jvmti_send_compiled_method_load_event(Method *method)
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
                    "Callback JVMTI_EVENT_COMPILED_METHOD_LOAD called, method = " <<
                    method->get_class()->name->bytes << "." << method->get_name()->bytes <<
                    method->get_descriptor()->bytes);

                for (CodeChunkInfo* cci = method->get_first_JIT_specific_info();  cci;  cci = cci->_next)
                    if (cci->get_code_block_size() > 0 &&
                        cci->get_code_block_addr() != NULL)
                        func((jvmtiEnv*)ti_env, (jmethodID)method, cci->get_code_block_size(),
                            cci->get_code_block_addr(), 0, NULL, NULL);

                TRACE2("jvmti.event.cml",
                    "Callback JVMTI_EVENT_COMPILED_METHOD_LOAD finished, method = " <<
                    method->get_class()->name->bytes << "." << method->get_name()->bytes <<
                    method->get_descriptor()->bytes);
            }
        }

        ti_env = next_env;
    }
}

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

void jvmti_add_dynamic_generated_code_chunk(const char *name, const void *address, jint length)
{
    DynamicCode *dcList = VM_Global_State::loader_env->TI->getDynamicCodeList();
    // FIXME linked list modification without synchronization
    DynamicCode *dc = (DynamicCode *)STD_MALLOC(sizeof(DynamicCode));
    assert(dc);
    dc->name = name;
    dc->address = address;
    dc->length = length;
    dc->next = dcList;
    dcList = dc;
}

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

    if (JVMTI_EVENT_COMPILED_METHOD_LOAD != event_type &&
        JVMTI_EVENT_DYNAMIC_CODE_GENERATED != event_type)
        return JVMTI_ERROR_ILLEGAL_ARGUMENT;

    if (JVMTI_EVENT_COMPILED_METHOD_LOAD == event_type)
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
                TRACE2("jvmti.event", "Class = " << (*klass)->name->bytes);
                if((*klass)->is_primitive)
                    continue;
                if((*klass)->class_loader != classloader)
                    continue;
                if((*klass)->state == ST_Prepared ||
                    (*klass)->state == ST_Initializing ||
                    (*klass)->state == ST_Initialized)
                    for (int jjj = 0; jjj < (*klass)->n_methods; jjj++)
                    {
                        Method *method = &(*klass)->methods[jjj];
                        TRACE2("jvmti.event", "    Method = " << method->get_name()->bytes <<
                            method->get_descriptor()->bytes <<
                            (method->get_state() == Method::ST_Compiled ? " compiled" : " not compiled"));
                        if (method->get_state() == Method::ST_Compiled)
                            jvmti_send_compiled_method_load_event(method);
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
        } while( true );

        ClassLoader::UnlockLoadersTable();
    }
    else
    {
        // FIXME: linked list usage without sync
        for (DynamicCode *dcList = ((TIEnv *)env)->vm->vm_env->TI->getDynamicCodeList();
            NULL != dcList; dcList = dcList->next)
            jvmti_send_dynamic_code_generated_event(dcList->name, dcList->address, dcList->length);
    }

    return JVMTI_ERROR_NONE;
}

VMEXPORT void
jvmti_process_method_entry_event(jmethodID method) {
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_METHOD_ENTRY))
        return;

    jvmtiEvent event_type = JVMTI_EVENT_METHOD_ENTRY;
    VM_thread *curr_thread = p_TLS_vmthread;
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
        JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.MethodEntry)
            ti_env->event_table.MethodEntry(jvmti_env, jni_env, thread, method);

        ti_env = next_env;
    }
}

VMEXPORT void
jvmti_process_method_exit_event(jmethodID method, jboolean was_popped_by_exception, jvalue ret_val) {
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() )
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_METHOD_EXIT))
        return;

    jvmtiEvent event_type = JVMTI_EVENT_METHOD_EXIT;
    VM_thread *curr_thread = p_TLS_vmthread;
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
        JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.MethodExit)
            ti_env->event_table.MethodExit(jvmti_env, jni_env, thread, method, was_popped_by_exception, ret_val);
        ti_env = next_env;
    }

    if (interpreter_enabled())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_FRAME_POP_NOTIFICATION))
        return;

    jvmti_frame_pop_listener *fpl = curr_thread->frame_pop_listener;
    jvmti_frame_pop_listener **prev_fpl = &curr_thread->frame_pop_listener;
    jint skip = 0;
    jint depth = get_thread_stack_depth(curr_thread, &skip);

    while (fpl)
    {
        if (fpl->depth == depth + skip)
        {
            jvmti_process_frame_pop_event(
                reinterpret_cast<jvmtiEnv *>(fpl->env),
                method,
                was_popped_by_exception);
            *prev_fpl = fpl->next;
            STD_FREE(fpl);
        }
        prev_fpl = &fpl->next;
        fpl = fpl->next;
    }
}

VMEXPORT void
jvmti_process_frame_pop_event(jvmtiEnv *jvmti_env, jmethodID method, jboolean was_popped_by_exception)
{
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled() ) return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    TIEnv *ti_env = (TIEnv*) jvmti_env;
    jthread thread = getCurrentThread();
    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;

    if (NULL != ti_env->event_table.FramePop)
        ti_env->event_table.FramePop(jvmti_env, jni_env, thread, method,
            was_popped_by_exception);

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
    VM_thread *curr_thread = p_TLS_vmthread;
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
        JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
        jvmtiEnv *jvmti_env = (jvmtiEnv*) ti_env;

        if (NULL != ti_env->event_table.SingleStep)
            ti_env->event_table.SingleStep(jvmti_env, jni_env, thread, method, location);
        ti_env = next_env;
    }
}

/*
 * Send Exception event
 */
void jvmti_send_exception_event(jthrowable exn_object,
    Method *method, jlocation location,
    Method *catch_method, jlocation catch_location)
{
    assert(!exn_raised());
    assert(!tmn_is_suspend_enabled());

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    assert(ti->isEnabled());

    VM_thread *curr_thread = p_TLS_vmthread;

    curr_thread->ti_exception_callback_pending = false;

    // Create local handles frame
    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;

    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)curr_thread->p_java_lang_thread;

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
        jvmtiEventException func =
            (jvmtiEventException)ti_env->get_event_callback(JVMTI_EVENT_EXCEPTION);
        if (NULL != func) {

            tmn_suspend_enable();
            assert(tmn_is_suspend_enabled());

            func((jvmtiEnv *)ti_env, jni_env,
                reinterpret_cast<jthread>(hThread),
                reinterpret_cast<jmethodID>(method),
                location, exn_object,
                reinterpret_cast<jmethodID>(catch_method),
                catch_location);
            tmn_suspend_disable();
        }
        ti_env = next_env;
    }

    curr_thread->ti_exception_callback_pending = false;
}

void jvmti_interpreter_exception_event_callback_call(void)
{
    assert(exn_raised());
    SuspendDisabledChecker sdc;

    VM_thread *curr_thread = p_TLS_vmthread;
    curr_thread->ti_exception_callback_pending = false;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return;

    jthrowable exn_object = exn_get();

    clear_current_thread_exception();

    jlocation location, catch_location;
    Method *method, *catch_method;

    interpreter.interpreter_st_get_interrupted_method(&method,
        &location);
    interpreter.interpreter_st_get_catch_method(&catch_method,
        &catch_location, exn_object);

    jvmti_send_exception_event(exn_object, method, location,
        catch_method, catch_location);
    assert(!tmn_is_suspend_enabled());

    if (!exn_raised())
        set_current_thread_exception(exn_object->object);
}

ManagedObject *jvmti_jit_exception_event_callback_call(ManagedObject *exn_object,
    JIT *jit, Method *method, NativeCodePtr native_location,
    JIT *catch_jit, Method *catch_method, NativeCodePtr native_catch_location)
{
    SuspendDisabledChecker sdc;

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled())
        return exn_object;

    if (JVMTI_PHASE_LIVE != ti->getPhase())
        return exn_object;

    if (!ti->get_global_capability(DebugUtilsTI::TI_GC_ENABLE_EXCEPTION_EVENT))
        return exn_object;

    jlocation location = -1, catch_location = -1;

    if (NULL == jit)
    {
        WARN("Zero interrupted method encountered");
        return exn_object;
    }

    uint16 bc = 0;
    OpenExeJpdaError UNREF result = jit->get_bc_location_for_native(
        method, native_location, &bc);
    TRACE2("jvmti.event.exn",
        "Exception method = " << (Method_Handle)method << " location " << bc);
    if (EXE_ERROR_NONE != result)
        WARN("JIT " << jit <<
            " get_bc_location_for_native returned error " <<
            result << " for exception method " << (Method_Handle)method <<
            " location " << native_location);
    location = bc;

    if (catch_method)
    {
        bc = 0;
        OpenExeJpdaError UNREF result = catch_jit->get_bc_location_for_native(
            catch_method, native_catch_location, &bc);
        TRACE2("jvmti.event.exn",
            "Exception catch method = " << (Method_Handle)catch_method <<
            " location " << bc);
        if (EXE_ERROR_NONE != result)
            WARN("JIT " << jit <<
                " get_bc_location_for_native returned error " <<
                result << " for catch method " << (Method_Handle)catch_method <<
                " location " << native_catch_location);
        catch_location = bc;
    }

    ObjectHandle exception = oh_allocate_local_handle();
    exception->object = exn_object;

    jvmti_send_exception_event(exception, method, location,
        catch_method, catch_location);

    assert(!tmn_is_suspend_enabled());
    return exception->object;
}

void jvmti_send_contended_enter_or_entered_monitor_event(jobject obj,
    bool isEnter)
{
    assert(tmn_is_suspend_enabled());
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (!ti->isEnabled()) {
        return;
    }

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;

    tmn_suspend_disable();
    // Create local handles frame
    NativeObjectHandles lhs;
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)p_TLS_vmthread->p_java_lang_thread;
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
    assert(tmn_is_suspend_enabled());
    if( clss->is_array || clss->is_primitive ) {
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

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
    tmn_suspend_disable();
    ObjectHandle hThread = oh_allocate_local_handle();
    ObjectHandle hClass = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)p_TLS_vmthread->p_java_lang_thread;
    hClass->object = struct_Class_to_java_lang_Class(clss);
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
                TRACE2("jvmti.class.cl", "Class load event, class name = " << clss->name->bytes);
                // fire global event
                func((jvmtiEnv *)ti_env, jni_env, (jthread)hThread, (jclass)hClass);
                ti_env = next_env;
                continue;
            }

            // fire local events
            for( TIEventThread* ti_et = ti_env->event_threads[JVMTI_EVENT_CLASS_LOAD - JVMTI_MIN_EVENT_TYPE_VAL];
                 ti_et != NULL; ti_et = ti_et->next )
                if (ti_et->thread == p_TLS_vmthread)
                {
                    TRACE2("jvmti.class.cl", "Class load event, class name = " << clss->name->bytes);
                    tmn_suspend_disable();
                    ObjectHandle hThreadLocal = oh_allocate_local_handle();
                    hThreadLocal->object = (Java_java_lang_Thread *)ti_et->thread->p_java_lang_thread;
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
    assert(tmn_is_suspend_enabled());
    // Send Class Load event
    DebugUtilsTI* ti = env->TI;
    if (!ti->isEnabled())
        return;

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
    tmn_suspend_disable();
    ObjectHandle hLoader = oh_convert_to_local_handle((ManagedObject*)loader->GetLoader());
    if( !(hLoader->object) ) hLoader = NULL;
    tmn_suspend_enable();
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
                if (ti_et->thread == p_TLS_vmthread)
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
    assert(tmn_is_suspend_enabled());
    if( clss->is_array || clss->is_primitive ) {
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

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
    tmn_suspend_disable(); // -----------vv
    ObjectHandle hThread = oh_allocate_local_handle();
    ObjectHandle hClass = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)p_TLS_vmthread->p_java_lang_thread;
    hClass->object = struct_Class_to_java_lang_Class(clss);
    tmn_suspend_enable(); // ------------^^

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
                TRACE2("jvmti.class.cp", "Class prepare event, class name = " << clss->name->bytes);
                // fire global event
                func((jvmtiEnv *)ti_env, jni_env, (jthread)hThread, (jclass)hClass);
                TRACE2("jvmti.class.cp", "Class prepare event exited, class name = " << clss->name->bytes);
                ti_env = next_env;
                continue;
            }
            // fire local events
            for(TIEventThread* ti_et = ti_env->event_threads[JVMTI_EVENT_CLASS_PREPARE - JVMTI_MIN_EVENT_TYPE_VAL];
                ti_et != NULL; ti_et = ti_et->next )
                if (ti_et->thread == p_TLS_vmthread)
                {
                    TRACE2("jvmti.class.cp", "Class prepare event, class name = " << clss->name->bytes);
                    tmn_suspend_disable(); // -------------------------------vv
                    ObjectHandle hThreadLocal = oh_allocate_local_handle();
                    hThreadLocal->object = (Java_java_lang_Thread *)ti_et->thread->p_java_lang_thread;
                    tmn_suspend_enable(); // --------------------------------^^
                    func((jvmtiEnv *)ti_env, jni_env, (jthread)hThreadLocal, (jclass)hClass);
                    oh_discard_local_handle( hThreadLocal );
                }
        }
        ti_env = next_env;
    }
    oh_discard_local_handle(hThread);
    oh_discard_local_handle(hClass);
    assert(tmn_is_suspend_enabled());
}

void jvmti_send_thread_start_end_event(int is_start)
{
    assert(tmn_is_suspend_enabled());

    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (! ti->isEnabled()) return;

    JNIEnv *jni_env = (JNIEnv *)jni_native_intf;

    tmn_suspend_disable();
    ObjectHandle hThread = oh_allocate_local_handle();
    hThread->object = (Java_java_lang_Thread *)p_TLS_vmthread->p_java_lang_thread;
    tmn_suspend_enable();

    TIEnv *ti_env = ti->getEnvironments();
    TIEnv *next_env;
    while (NULL != ti_env)
    {
        next_env = ti_env->next;
        if (is_start && ti_env->global_events[JVMTI_EVENT_THREAD_START - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventThreadStart func = (jvmtiEventThreadStart)ti_env->get_event_callback(JVMTI_EVENT_THREAD_START);
            if (NULL != func)
            {
                func((jvmtiEnv*)ti_env, jni_env, (jthread)hThread);
            }
        }
        else if (! is_start && ti_env->global_events[JVMTI_EVENT_THREAD_END - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventThreadEnd func = (jvmtiEventThreadEnd)ti_env->get_event_callback(JVMTI_EVENT_THREAD_END);
            if (NULL != func)
            {
                func((jvmtiEnv*)ti_env, jni_env, (jthread)hThread);
            }
        }
        ti_env = next_env;
    }
    assert(tmn_is_suspend_enabled());
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
        next_env = ti_env->next;
        if (ti_env->global_events[JVMTI_EVENT_VM_DEATH - JVMTI_MIN_EVENT_TYPE_VAL])
        {
            jvmtiEventVMDeath func = (jvmtiEventVMDeath)ti_env->get_event_callback(JVMTI_EVENT_VM_DEATH);
            JNIEnv *jni_env = (JNIEnv *)jni_native_intf;
            if (NULL != func)
            {
                TRACE2("jvmti.event.vd", "Callback JVMTI_PHASE_DEATH called");
                func((jvmtiEnv *)ti_env, jni_env);
                TRACE2("jvmti.event.vd", "Callback JVMTI_PHASE_DEATH finished");
            }
        }

        ti_env = next_env;
    }
    ti->nextPhase(JVMTI_PHASE_DEAD);
}

