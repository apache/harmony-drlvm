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
 * @author Ivan Volosyuk
 */

#ifdef __linux__

#include "gc_types.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

static size_t proc_huge_page_size = 4096 * 1024;
static size_t proc_huge_pages_total = (size_t)-1;
static size_t proc_huge_pages_free = 0;
static const char *str_HugePages_Total = "HugePages_Total:";
static const char *str_HugePages_Free = "HugePages_Free:";
static const char *str_Hugepagesize = "Hugepagesize:";

static const char *parse_value(const char *buf, int len, const char *name, int name_len, size_t *value) {
    if (len < name_len) return NULL;
    if (strncmp(buf, name, name_len)) return NULL;
    buf += name_len;
    char *endpos;
    long int res = strtol(buf, &endpos, 10);
    if (endpos == buf) return NULL;
    *value = (size_t) res;
    return endpos;
}

static void parse_proc_meminfo(size_t required_size) {
    if (!is_info_enabled("gc.lp")) return;
    FILE *f = fopen("/proc/meminfo", "r");
    if (f == NULL) {
        INFO2("gc.lp", "gc.lp: Can't open /proc/meminfo: " << strerror(errno)
              << ". Mount /proc filesystem.");
        return;
    }

    size_t size = 128;
    char *buf = (char*) malloc(size);
    while (true) {
        ssize_t len = getline(&buf, &size, f);
        if (len == -1) break;
        parse_value(buf, len, str_HugePages_Total, strlen(str_HugePages_Total), &proc_huge_pages_total);
        parse_value(buf, len, str_HugePages_Free, strlen(str_HugePages_Free), &proc_huge_pages_free);
        const char *end =
            parse_value(buf, len, str_Hugepagesize, strlen(str_Hugepagesize), &proc_huge_page_size);
        if (end && !strncmp(end, " kB", 3)) proc_huge_page_size *= 1024;
    }
    if (buf) free(buf);
    INFO2("gc.lp", "gc.lp: /proc/meminfo: System total huge pages = " << proc_huge_pages_total);
    INFO2("gc.lp", "gc.lp: /proc/meminfo: System free huge pages = " << proc_huge_pages_free);
    INFO2("gc.lp", "gc.lp: /proc/meminfo: Huge page size = " << proc_huge_page_size / 1024 << " kB");
    if (proc_huge_pages_total == (size_t)-1) {
        INFO2("gc.lp", "gc.lp: large pages are not supported by kernel\n"
                       "       CONFIG_HUGETLB_PAGE and CONFIG_HUGETLBFS needs to be enabled");
    } else if (proc_huge_pages_total == 0) {
        INFO2("gc.lp", "gc.lp: no large pages reserved\n"
                       "       Use following command:\n"
                       "             echo 20 > /proc/sys/vm/nr_hugepages\n"
                       "       Do it just after kernel boot before huge pages become"
                       " fragmented");
    } else if (proc_huge_pages_free * proc_huge_page_size < required_size) {
        if (proc_huge_pages_total * proc_huge_page_size >= required_size) {
            INFO2("gc.lp", "gc.lp: not enough free large pages\n"
                    "       some of reserved space is already busy");
        } else {
            INFO2("gc.lp", "gc.lp: not enough reserved large pages")
        }
        INFO2("gc.lp", "gc.lp: " << mb(proc_huge_pages_free * proc_huge_page_size)
                << " mb can be only allocated");
    }
}

void *map_large_pages(const char *path, size_t size) {
    INFO2("gc.lp", "gc.lp: large pages using mmap");
    const char *postfix = "/vm_heap";
    char *buf = (char *) malloc(strlen(path) + strlen(postfix) + 1);
    assert(buf);

    strcpy(buf, path);
    strcat(buf, postfix);

    int fd = open(buf, O_CREAT | O_RDWR, 0700);
    if (fd == -1) {
        INFO2("gc.lp", "gc.lp: can't open " << buf << ": " << strerror(errno) << "\n"
                       "Mount hugetlbfs with: mount none /mnt/huge -t hugetlbfs\n"
                       "Check you have appropriate permissions to /mnt/huge\n"
                       "Use command line switch -Dgc.lp=/mnt/huge");
        free(buf);
        return NULL;
    }
    unlink(buf);

    void *addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        INFO2("gc.lp", "gc.lp: map failed: " << strerror(errno));
        close(fd);
        free(buf);
        return NULL;
    }
    close(fd);
    free(buf);
    INFO2("gc.lp", "gc.lp: large pages successfully allocated");
    return addr;
}


void *alloc_large_pages(size_t size, const char *hint) {
    INFO2("gc.lp", "gc.lp: size = " << mb(size) << " mb, hint = " << hint);
    parse_proc_meminfo(size);
    void *addr = map_large_pages(hint, size);
    if (addr == NULL) {
        if (is_info_enabled("gc.lp")) {
            INFO2("gc.lp", "read also /usr/src/linux/Documentation/vm/hugetlbpage.txt");
        } else {
            LWARN2("gc.lp", 1, "large pages allocation failed, use -verbose:gc.lp for more info");
        }
    }
    return addr;
}

#endif
