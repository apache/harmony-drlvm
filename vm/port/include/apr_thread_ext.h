/**
 * @author Andrey Chernyshev
 * @version $Revision$
 */


#ifndef APR_EXT_H
#define APR_EXT_H

#include <apr.h>
#include <apr_errno.h>
#include <apr_thread_proc.h>
#include <apr_portable.h>

APR_DECLARE(apr_status_t) apr_thread_set_priority(apr_thread_t *thread, apr_int32_t priority);

APR_DECLARE(void) apr_memory_rw_barrier();

APR_DECLARE(apr_status_t) apr_thread_yield_other(apr_thread_t *thread);

APR_DECLARE(apr_status_t) apr_thread_times(apr_thread_t *thread, 
                                apr_time_t * kernel_time, apr_time_t * user_time);

APR_DECLARE(apr_status_t) apr_thread_cancel(apr_thread_t *thread);


#endif  /* APR_EXT_H */
