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
 * @author Vladimir Nenashev
 * @version $Revision$
 */  


#ifndef __STACK_DUMP_H_
#define __STACK_DUMP_H_

#include "open/hythread_ext.h"
#include "vm_core_types.h"
#include "jni.h"
#include "native_modules.h"


#ifdef _DEBUG
#define SD_UPDATE_MODULES
#endif

#ifdef SD_UPDATE_MODULES
#define sd_update_modules() sd_update_modules()
#else
#define sd_update_modules()
#endif

#ifdef PLATFORM_POSIX
#include <strings.h>
#define strcmp_case strcasecmp
#else // Win
#include <string.h>
#define strcmp_case _stricmp
#endif

#include "platform_lowlevel.h"

#define SD_MNAME_LENGTH 2048

// Symbolic method info: method name, source file name and a line number of an instruction within the method
struct MethodInfo {
    char method_name[SD_MNAME_LENGTH];
    char file_name[_MAX_PATH];
    int line;
};



/**
 * Prints a stack trace using given register context for current thread
 */
void sd_print_stack(Registers* regs);

/**
 * Updates modules list for crash reporting
 */
#ifdef SD_UPDATE_MODULES
void sd_update_modules();
#endif

// platform-dependent functions
bool sd_initialize(hymutex_t** p_lock);
void sd_parse_module_info(native_module_t* module, void* ip);
void sd_get_c_method_info(MethodInfo* info, native_module_t* module, void* ip);
int sd_get_cur_tid();

// general functions to call from platform-dependent code
const char* sd_get_module_type(const char* short_name);

#endif //!__STACK_DUMP_H_
