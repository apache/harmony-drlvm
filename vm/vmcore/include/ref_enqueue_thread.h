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
 * @author Li-Gang Wang, 2006/11/15
 */

#ifndef _REF_ENQUEUE_THREAD_H_
#define _REF_ENQUEUE_THREAD_H_

#include <assert.h>
#include "jni_types.h"
#include "open/hythread_ext.h"
#include "open/types.h"


#define REF_ENQUEUE_THREAD_PRIORITY HYTHREAD_PRIORITY_USER_MAX
#define REF_ENQUEUE_THREAD_NUM 1


typedef struct Ref_Enqueue_Thread_Info {
    hysem_t pending_sem;
    Boolean shutdown;
    volatile unsigned int thread_attached;
}Ref_Enqueue_Thread_Info;


extern void ref_enqueue_thread_init(JavaVM *java_vm);
extern void ref_enqueue_shutdown(void);
extern void activate_ref_enqueue_thread(void);

#endif // _REF_ENQUEUE_THREAD_H_
