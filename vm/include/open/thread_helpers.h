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
 * @author Artem Aliev
 * @version $Revision$
 */

#ifndef OPEN_THREAD_HELPERS_H
#define OPEN_THREAD_HELPERS_H

/**
 * @file 
 * @brief Provides optimized assambly code generators for common monitor functions.
 *
 * For more detailes see thread manager component documentation located at vm/thread/doc/ThreadManager.htm
 */

#include "open/types.h"
#include "open/hythread_ext.h"
#include "encoder.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef void * fast_tls_func();

fast_tls_func* get_tls_helper(hythread_tls_key_t key);
char* gen_hythread_self_helper(char *ss);
char* gen_monitorenter_fast_path_helper(char *ss, const R_Opnd & input_param1);
char* gen_monitorenter_slow_path_helper(char *ss, const R_Opnd & input_param1);
char* gen_monitor_exit_helper(char *ss, const R_Opnd & input_param1);
char* gen_monitorexit_slow_path_helper(char *ss, const R_Opnd & input_param1);

#ifdef __cplusplus
}
#endif

#endif  /* OPEN_THREAD_NATIVE_H */
