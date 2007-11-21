/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

/**
 * @file thread_ncai_common.c
 * @brief NCAI basic thread functions
 */

#include <open/hythread_ext.h>
#include "thread_private.h"
#include <open/ncai_thread.h>
#include <open/jthread.h>


/**
 * Suspend thread as OS native thread.
 *
 * @param[in] thread The thread to suspend.
 */
IDATA VMCALL hythread_suspend_thread_native(hythread_t thread)
{
    if (thread == NULL)
        return TM_ERROR_NULL_POINTER;

    if (!hythread_is_alive(thread))
        return TM_ERROR_INTERNAL;

    assert(thread->os_handle);

    return os_thread_suspend(thread->os_handle);
}

/**
 * Resume thread as OS native thread.
 *
 * @param[in] thread The thread to resume.
 */
IDATA VMCALL hythread_resume_thread_native(hythread_t thread)
{
    if (thread == NULL)
        return TM_ERROR_NULL_POINTER;

    if (!hythread_is_alive(thread))
        return TM_ERROR_INTERNAL;

    assert(thread->os_handle);

    return os_thread_resume(thread->os_handle);
}

/**
 * Returns suspend count for given thread.
 *
 * @param[in] thread The thread to process.
 * @return -1 if error have occured
 */
int VMCALL hythread_get_suspend_count_native(hythread_t thread)
{
    if (thread == NULL)
        return -1;

    if (!hythread_is_alive(thread))
        return -1;

    assert(thread->os_handle);

    return os_thread_get_suspend_count(thread->os_handle);
}

/**
 * Returns the platform-dependent thread context.
 *
 * @param[in] thread to get context.
 * @param[out] pointer to context structure.
 */
IDATA VMCALL hythread_get_thread_context(hythread_t thread, os_thread_context_t* pcontext)
{
    if (pcontext == NULL || thread == NULL)
        return TM_ERROR_NULL_POINTER;

    if (!hythread_is_alive(thread))
        return TM_ERROR_INTERNAL;

    assert(thread->os_handle);

    return os_thread_get_context(thread->os_handle, pcontext);
}

/**
 * Sets the context for given thread.
 *
 * @param[in] thread to set context.
 * @param[in] pointer to context structure.
 */
IDATA VMCALL hythread_set_thread_context(hythread_t thread, os_thread_context_t* pcontext)
{
    if (pcontext == NULL || thread == NULL)
        return TM_ERROR_NULL_POINTER;

    if (!hythread_is_alive(thread))
        return TM_ERROR_INTERNAL;

    assert(thread->os_handle);

    return os_thread_get_context(thread->os_handle, pcontext);
}
