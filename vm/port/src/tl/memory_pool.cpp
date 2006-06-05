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
 * @author Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#include "tl/memory_pool.h"

#define LOG_DOMAIN "tl.memory"
#include "cxxlog.h"

tl::MemoryPool::MemoryPool()
{
    apr_status_t status = apr_pool_create(&pool, NULL);
    VERIFY(APR_SUCCESS == status, "Cannot create a memory pool");
}

tl::MemoryPool::MemoryPool(const MemoryPool * parent)
{
    apr_status_t status = apr_pool_create(&pool, parent->pool);
    VERIFY(APR_SUCCESS == status, "Cannot create a memory pool");
}

tl::MemoryPool::~MemoryPool()
{
    apr_pool_destroy(pool);
}

void * tl::MemoryPool::alloc(size_t size)
{
    return apr_palloc(pool, size);
}

apr_status_t tl::MemoryPool::create_mutex(apr_thread_mutex_t** mutex, unsigned int flags) {
    return apr_thread_mutex_create(mutex, flags, pool);
}


tl::MemoryPoolMT::MemoryPoolMT()
{
    apr_status_t status = unsync_pool.create_mutex(&mutex, APR_THREAD_MUTEX_UNNESTED);
    VERIFY(APR_SUCCESS == status, "Cannot create a pool lock");
}

tl::MemoryPoolMT::MemoryPoolMT(const MemoryPoolMT * parent) :
    unsync_pool(&parent->unsync_pool)
{
    apr_status_t status = unsync_pool.create_mutex(&mutex, APR_THREAD_MUTEX_UNNESTED);
    VERIFY(APR_SUCCESS == status, "Cannot create a pool lock");
}

tl::MemoryPoolMT::MemoryPoolMT(const MemoryPool * parent) : unsync_pool(parent)
{
    apr_status_t status = unsync_pool.create_mutex(&mutex, APR_THREAD_MUTEX_UNNESTED);
    VERIFY(APR_SUCCESS == status, "Cannot create a pool lock");
}

tl::MemoryPoolMT::~MemoryPoolMT()
{
    apr_status_t status = apr_thread_mutex_destroy(mutex);
    VERIFY(APR_SUCCESS == status, "Cannot destroy a pool lock");
}

void * tl::MemoryPoolMT::alloc(size_t size)
{
    apr_thread_mutex_lock(mutex);
    void* ptr = unsync_pool.alloc(size);
    apr_thread_mutex_unlock(mutex);
    return ptr;
}

apr_status_t tl::MemoryPoolMT::create_mutex(apr_thread_mutex_t** m, unsigned int flags) {
    apr_thread_mutex_lock(mutex);
    apr_status_t res = unsync_pool.create_mutex(m, flags);
    apr_thread_mutex_unlock(mutex);
    return res;
}
