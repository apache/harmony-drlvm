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
 * @author Nikolay Kuznetsov
 * @version $Revision: 1.1.2.2.4.4 $
 */

#include <set>
#include "thread_dump.h"
#include "m2n.h"
#include "stack_iterator.h"
#include "stack_trace.h"
#include "mon_enter_exit.h"
#include "jni_utils.h"
#include "jit_intf_cpp.h"
#include "dll_jit_intf.h"

#include "object_generic.h"
#include "Class.h"
#include "vtable.h"
#include "environment.h"
#include "root_set_enum_internal.h"
#include "lock_manager.h"
#include "open/gc.h"
#include "cci.h"

#define LOG_DOMAIN "thread_dump"
#include "cxxlog.h"


static std::set < void *>unique_references;

enum reference_types
{
    root_reference = 1,
    compresses_root_reference,
    managed_reference,
    managed_reference_with_base
};

void td_attach_thread(void (*printer) (FILE *), FILE * out);

VMEXPORT void vm_check_if_monitor(void **reference,
                                  void **base_reference,
                                  U_32 *compressed_reference, 
                                  size_t  slotOffset, 
                                  BOOLEAN pinned,
                                  U_32    type)
{
}

/**
 * The thread dump entry poin, this function being called from the signal handler
 */
void td_dump_all_threads(FILE * out)
{
}
