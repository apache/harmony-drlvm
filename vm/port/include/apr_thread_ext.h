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

APR_DECLARE(apr_status_t) apr_get_thread_time(apr_thread_t *thread, apr_int64_t* nanos_ptr);

#endif  /* APR_EXT_H */
