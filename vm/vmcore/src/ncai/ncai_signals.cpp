/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#define LOG_DOMAIN "ncai.signal"
#include "cxxlog.h"
#include "suspend_checker.h"
#include "jvmti_internal.h"
#include "environment.h"

#include "ncai_utils.h"
#include "ncai_direct.h"
#include "ncai_internal.h"


ncaiError JNICALL
ncaiGetSignalCount(ncaiEnv *env, jint* count_ptr)
{
    TRACE2("ncai.signal", "GetSignalCount called");
    SuspendEnabledChecker sec;

    if (env == NULL)
        return NCAI_ERROR_INVALID_ENVIRONMENT;

    if (count_ptr == NULL)
        return NCAI_ERROR_NULL_POINTER;

    *count_ptr = (jint)ncai_get_signal_count();
    return NCAI_ERROR_NONE;
}

ncaiError JNICALL
ncaiGetSignalInfo(ncaiEnv *env, jint signal, ncaiSignalInfo* info_ptr)
{
    TRACE2("ncai.signal", "GetSignalInfo called");
    SuspendEnabledChecker sec;

    if (env == NULL)
        return NCAI_ERROR_INVALID_ENVIRONMENT;

    if (info_ptr == NULL)
        return NCAI_ERROR_NULL_POINTER;

    if (!ncai_is_signal_in_range(signal))
        return NCAI_ERROR_ILLEGAL_ARGUMENT;

    char* name = ncai_get_signal_name(signal);
    if (name == NULL)
        return NCAI_ERROR_ILLEGAL_ARGUMENT;

    size_t name_size = ncai_get_signal_name_size(signal);
    char* out_name = (char*)ncai_alloc(name_size);

    if (out_name == NULL)
        return NCAI_ERROR_OUT_OF_MEMORY;

    memcpy(out_name, name, name_size);
    info_ptr->name = out_name;
    return NCAI_ERROR_NONE;
}

void ncai_process_signal_event(NativeCodePtr addr,
        jint code, bool is_internal, bool* p_handled)
{
    if (!GlobalNCAI::isEnabled())
        return;

    DebugUtilsTI* ti = VM_Global_State::loader_env->TI;

    hythread_t hythread = hythread_self();
    ncaiThread thread = reinterpret_cast<ncaiThread>(hythread);
    bool skip_handler = false;

    TIEnv* next_env;
    for (TIEnv* env = ti->getEnvironments(); env; env = next_env)
    {
        next_env = env->next;
        NCAIEnv* ncai_env = env->ncai_env;

        if (NULL == ncai_env)
            continue;

        ncaiSignal func =
            (ncaiSignal)ncai_env->get_event_callback(NCAI_EVENT_SIGNAL);

        if (NULL != func)
        {
            if (ncai_env->global_events[NCAI_EVENT_SIGNAL - NCAI_MIN_EVENT_TYPE_VAL])
            {
                TRACE2("ncai.signal", "Calling global Signal callback, address = "
                    << addr << ", code = " << (void*)(size_t)code);

                jboolean is_h = *p_handled;
                func((ncaiEnv*)ncai_env, thread, (void*)addr, code, is_internal, &is_h);

                if (!(*p_handled) && is_h)
                    skip_handler = true;

                TRACE2("ncai.signal", "Finished global Signal callback, address = "
                    << addr << ", code = " << (void*)(size_t)code);

                continue;
            }

            ncaiEventThread* next_et;
            ncaiEventThread* first_et =
                ncai_env->event_threads[NCAI_EVENT_SIGNAL - NCAI_MIN_EVENT_TYPE_VAL];

            for (ncaiEventThread* et = first_et; NULL != et; et = next_et)
            {
                next_et = et->next;

                if (et->thread == thread)
                {
                    TRACE2("ncai.signal", "Calling local Signal callback, address = "
                        << addr << ", code = " << (void*)(size_t)code);

                    jboolean is_h = *p_handled;
                    func((ncaiEnv*)ncai_env, thread, (void*)addr, code, is_internal, &is_h);

                    if (!(*p_handled) && is_h)
                        skip_handler = true;

                    TRACE2("ncai.signal", "Finished local Signal callback, address = "
                        << addr << ", code = " << (void*)(size_t)code);
                }

                et = next_et;
            }
        }
    }
}
