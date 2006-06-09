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
 * @author Alexey V. Varlamov
 * @version $Revision: 1.1.2.1.4.3 $
 */  

#ifndef _PORT_VMEM_H_
#define _PORT_VMEM_H_

#include "port_general.h"
#include <apr_pools.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PORT_VMEM_MODE_READ 0x1
#define PORT_VMEM_MODE_WRITE 0x2
#define PORT_VMEM_MODE_EXECUTE 0x4

#define PORT_VMEM_PAGESIZE_DEFAULT 0
#define PORT_VMEM_PAGESIZE_LARGE 1

/**
 * Virtual memory block descriptor. Incomplete type, 
 * runtime instance should be obtained via port_vmem_reserve() call
 */
typedef struct port_vmem_t port_vmem_t;

/**
 * Reserves a continuous memory region in the virtual address space 
 * of the calling process.
 * @param[in,out] block descriptor for the reserved memory, required for 
 * further operations with the memory
 * @param[in,out] address starting address of the region to allocate. If null,
 * system will determine appropriate location. Actual allocated address is 
 * returned on success
 * @param amount size of the region in bytes. For large pages, size must be 
 * multiply of page size. See port_vmem_page_sizes()
 * @param protectionMode bit mask of PORT_VMEM_MODE_* values
 * @param pageSize desired size of memory page. Value should be either
 * any of PORT_VMEM_PAGESIZE_* flags or actual size in bytes.
 * @param pool auxiliary pool
 */
APR_DECLARE(apr_status_t) port_vmem_reserve(port_vmem_t **block, void **address, 
                                           size_t amount, 
                                           unsigned int protectionMode, 
                                           size_t pageSize, apr_pool_t *pool);

/**
* Commits (part of) previously reserved memory region.
* @param[in,out] address starting address of the region to commit. Returned value 
* may differ due to page-alignment.
* @param amount size of the region in bytes
* @param block descriptor to the reserved virtual memory
*/
APR_DECLARE(apr_status_t) port_vmem_commit(void **address, size_t amount, 
                                          port_vmem_t *block);

/**
* Decommits the specified region of committed memory. It is safe to 
* decommit reserved (but not committed) region.
*/
APR_DECLARE(apr_status_t) port_vmem_decommit(void *address, size_t amount, 
                                            port_vmem_t *block);

/**
* Releases previously reserved virtual memory region as a whole.
* If the region was committed, function first decommits it.
*/
APR_DECLARE(apr_status_t) port_vmem_release(/*void *address, size_t amount,*/
                                           port_vmem_t *block);

/**
* Returns zero-terminated array of supported memory page sizes.
* The first element refers to system default size and is guaranteed
* to be non-zero. Subsequent elements (if any) provide large page
* sizes.
*/
APR_DECLARE(size_t *) port_vmem_page_sizes();


#ifdef __cplusplus
}
#endif

#endif /* _PORT_VMEM_H_ */
