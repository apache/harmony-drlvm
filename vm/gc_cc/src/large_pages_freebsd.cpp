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

#ifdef FREEBSD

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

void *map_large_pages(const char *path, size_t size) {
    INFO2("gc.lp", "gc.lp: large pages not supported on freebsd (yet)");
    return NULL;
}

void *alloc_large_pages(size_t size, const char *hint) {
    INFO2("gc.lp", "gc.lp: large pages not supported on freebsd (yet)");
    return NULL;
}

#endif
