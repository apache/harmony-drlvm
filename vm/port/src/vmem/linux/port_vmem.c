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

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include "port_vmem.h"

#ifdef __cplusplus
extern "C" {
#endif

struct port_vmem_t {
	apr_pool_t *pool;
	void* start;
	size_t size;
	size_t page;
	unsigned int protection;
};

static int convertProtectionBits(unsigned int mode){
 	int bits = 0;
 	if (mode & PORT_VMEM_MODE_READ) {
 		bits |= PROT_READ;
 	}
 	if (mode & PORT_VMEM_MODE_WRITE) {
 		bits |= PROT_WRITE;
 	}
 	if (mode & PORT_VMEM_MODE_EXECUTE) {
 		bits |= PROT_EXEC;
 	}

    return bits;
}

APR_DECLARE(apr_status_t) port_vmem_reserve(port_vmem_t **block, void **address, 
        size_t size, unsigned int mode, 
        size_t page, apr_pool_t *pool) {

    void *start = 0;
    if (PORT_VMEM_PAGESIZE_DEFAULT == page || PORT_VMEM_PAGESIZE_LARGE == page) {
        page = port_vmem_page_sizes()[0];
    }

    int protection = convertProtectionBits(mode);

    errno = 0;
    size = (size + page - 1) & ~(page - 1); /* Align */

#ifdef MAP_ANONYMOUS
    start = mmap(*address, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#elif defined(MAP_ANON)
    start = mmap(*address, size, protection, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    int fd = open("/dev/zero", O_RDONLY);
    start = mmap(*address, size, protection, MAP_PRIVATE, fd, 0);
    close(fd);
#endif

    if (MAP_FAILED == start) {
        return apr_get_os_error();
    }

    *block = apr_palloc(pool, sizeof(port_vmem_t));
    (*block)->pool = pool;
    (*block)->start = start;
    (*block)->size = size;
    (*block)->page = page;
    (*block)->protection = protection;

    *address = start;

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) port_vmem_commit(void **address, size_t amount, 
										  port_vmem_t *block){
	size_t page = block->page;
	void *aligned = (void *)(((long)*address + page-1) & ~(page-1));
	errno = 0;
	if(mprotect(aligned, amount, block->protection)) {
		return apr_get_os_error();
	}
	*address = aligned;

	return APR_SUCCESS;
}


APR_DECLARE(apr_status_t) port_vmem_decommit(void *address, size_t amount, 
											port_vmem_t *block){
	return APR_SUCCESS;
}


APR_DECLARE(apr_status_t) port_vmem_release(/*void *address, size_t amount,*/ 
										   port_vmem_t *block){
	munmap(block->start, block->size);
	return APR_SUCCESS;
}

APR_DECLARE(size_t *) port_vmem_page_sizes() {

	static size_t page_sizes[2];
	if (!page_sizes[0]) {
		page_sizes[1] = 0;
		page_sizes[0] = sysconf(_SC_PAGE_SIZE);
		if (!page_sizes[0]) {
			page_sizes[0] = 4*1024;
		}
	}
	return page_sizes;
}

#ifdef __cplusplus
}
#endif
