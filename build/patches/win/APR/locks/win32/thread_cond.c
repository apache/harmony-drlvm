/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_private.h"
#include "apr_general.h"
#include "apr_strings.h"
#include "win32/apr_arch_thread_mutex.h"
#include "win32/apr_arch_thread_cond.h"
#include "apr_portable.h"

// define this constants to synchronized waiting queue access
// it is neccessery becuase signal() is not requiried to hold mutex
#define LOCK_QUEUE apr_thread_mutex_lock(cond->queue_mutex)
#define UNLOCK_QUEUE apr_thread_mutex_unlock(cond->queue_mutex)

static apr_status_t thread_cond_cleanup(void *data)
{
    apr_thread_mutex_destroy(((apr_thread_cond_t *)data)->queue_mutex);
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_thread_cond_create(apr_thread_cond_t **cond,
                                                 apr_pool_t *pool)
{
    *cond = apr_pcalloc(pool, sizeof(**cond));
    (*cond)->pool = pool;
    (*cond)-> dummy_node.next = (*cond)-> dummy_node.prev =  &((*cond)-> dummy_node);
    apr_thread_mutex_create(&((*cond)->queue_mutex),  APR_THREAD_MUTEX_NESTED, pool);
    return APR_SUCCESS;
}

static void _enqueue (apr_thread_cond_t *cond, struct waiting_node *node) {
     node->next = &(cond->dummy_node);
     node->prev = cond->dummy_node.prev;
     node->prev->next = node;
     cond->dummy_node.prev = node;

}

static int _remove_from_queue (apr_thread_cond_t *cond, struct waiting_node *node) {
    if (node->next == NULL || node->prev == NULL) {
        // already dequeued (by signal)
        return -1;
    }
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = NULL; 
    node->prev = NULL;
    return 0;
}

//dequeue 
// return NULL if queue is empty
static struct waiting_node* _dequeue (apr_thread_cond_t *cond) {
    struct waiting_node* node;
    if (cond->dummy_node.next == &(cond->dummy_node)) {
        // the queue is empty
        return NULL;
    }
    node=cond->dummy_node.next;
    _remove_from_queue (cond,node);
    return node;
}
static APR_INLINE apr_status_t _thread_cond_timedwait(apr_thread_cond_t *cond,
                                                      apr_thread_mutex_t *mutex,
                                                      DWORD timeout_ms )
{
    DWORD res;
    apr_status_t rv = APR_SUCCESS;
    struct waiting_node node;

    // add waiting
    node.event = CreateEvent(NULL, TRUE, FALSE, NULL);
    LOCK_QUEUE;
    _enqueue (cond, &node);
    UNLOCK_QUEUE;

    // release mutex and wait for signal 
    apr_thread_mutex_unlock(mutex);
    res = WaitForSingleObject(node.event, timeout_ms);
    if (res != WAIT_OBJECT_0) {
        if (res == WAIT_TIMEOUT) {
            rv = APR_TIMEUP;
        } else {
            rv = apr_get_os_error();
        }
    }
    apr_thread_mutex_lock(mutex);
    LOCK_QUEUE;
    _remove_from_queue (cond, &node);
    CloseHandle(node.event);
    UNLOCK_QUEUE;

    return rv;
}

APR_DECLARE(apr_status_t) apr_thread_cond_wait(apr_thread_cond_t *cond,
                                               apr_thread_mutex_t *mutex)
{
    return _thread_cond_timedwait(cond, mutex, INFINITE);
}

APR_DECLARE(apr_status_t) apr_thread_cond_timedwait(apr_thread_cond_t *cond,
                                                    apr_thread_mutex_t *mutex,
                                                    apr_interval_time_t timeout)
{
    DWORD timeout_ms = (DWORD) apr_time_as_msec(timeout);
    if (timeout % 1000) {
        timeout_ms++;
    }  
    return _thread_cond_timedwait(cond, mutex, timeout_ms);
}

APR_DECLARE(apr_status_t) apr_thread_cond_signal(apr_thread_cond_t *cond)
{
    apr_status_t rv = APR_SUCCESS;
    DWORD res;
    struct waiting_node* node;

    LOCK_QUEUE;
    node = _dequeue (cond);
    if (node != NULL) {
        res = SetEvent(node->event);
        if (res == 0) {
             rv = apr_get_os_error();
        }
    }
    UNLOCK_QUEUE;
    return rv;
}

APR_DECLARE(apr_status_t) apr_thread_cond_broadcast(apr_thread_cond_t *cond)
{
    apr_status_t rv = APR_SUCCESS;
    DWORD res;
    struct waiting_node* node;

    LOCK_QUEUE;
    for (node = _dequeue (cond); node != NULL; node = _dequeue (cond)) {
        res = SetEvent(node->event);
        if (res == 0) {
              rv = apr_get_os_error();
        }
    }
    UNLOCK_QUEUE;
    return rv;
}

APR_DECLARE(apr_status_t) apr_thread_cond_destroy(apr_thread_cond_t *cond)
{
    return apr_pool_cleanup_run(cond->pool, cond, thread_cond_cleanup);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_cond)

