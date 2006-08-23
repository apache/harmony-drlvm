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

#include "jvmti_types.h"
#include "apr_thread_proc.h"
#include "open/hycomp.h"
#include "open/jthread.h"

// Tested Thread Phases
#define TT_PHASE_NONE 0
#define TT_PHASE_DEAD 1
#define TT_PHASE_OK 2
#define TT_PHASE_ERROR 3
#define TT_PHASE_SLEEPING 4
#define TT_PHASE_WAITING 5
#define TT_PHASE_IN_CRITICAL_SECTON 7
#define TT_PHASE_WAITING_ON_MONITOR 8
#define TT_PHASE_WAITING_ON_WAIT 9
#define TT_PHASE_WAITING_ON_JOIN 10
#define TT_PHASE_RUNNING 11
#define TT_PHASE_PARKED 12
#define TT_PHASE_ATTACHED 13
#define TT_PHASE_ATTACHED_TWICE 14
#define TT_PHASE_STEP_1 15
#define TT_PHASE_DETACHED 16
#define TT_PHASE_INTERRUPTED 17
#define TT_PHASE_ANY 18

#define TTS_INIT_COMMON_MONITOR 0
#define TTS_INIT_DIFFERENT_MONITORS 1

#define MAX_TESTED_THREAD_NUMBER 5
#define MAX_CLICKS_TO_WAIT 30
#define CLICK_TIME_MSEC 10
#define MAX_OWNED_MONITORS_NMB 2
#define SLEEP_MSEC 10

typedef void (*run_method_t)(void);

typedef struct _jjobject{
    void *data;
    jboolean daemon;
    char *name;
    int lockword;
    run_method_t run_method;
}_jjobject;

typedef struct _jobject{
    _jjobject *object;
}_jobject;

typedef struct {
    int my_index;
    jthread java_thread;
    jobject monitor;
    jrawMonitorID raw_monitor;
    JNIEnv *jni_env;
    void * jvmti_start_proc_arg;
    int clicks;
    int phase;
    int stop;
    jint peak_count;
    jthread_threadattr_t attrs;
    jclass excn;
}tested_thread_sturct_t;

extern tested_thread_sturct_t *current_thread_tts;
extern tested_thread_sturct_t *dummy_tts;
void jni_init();
VMEXPORT void *vm_jthread_get_tm_data(jthread thread);
void sleep_a_click(void);
void test_java_thread_setup(void);
void test_java_thread_teardown(void);
void tested_threads_init(int mode);
void tested_threads_run(run_method_t run_method_param);
void tested_threads_run_common(run_method_t run_method_param);
void tested_threads_run_with_different_monitors(run_method_t run_method_param);
void tested_threads_run_with_jvmti_start_proc(jvmtiStartFunction jvmti_start_proc);
void tested_os_threads_run(apr_thread_start_t run_method_param);
int tested_threads_destroy();
int tested_threads_stop();
tested_thread_sturct_t *get_tts(int tts_index);
int next_tested_thread(tested_thread_sturct_t **tts);
int prev_tested_thread(tested_thread_sturct_t **tts);
void reset_tested_thread_iterator(tested_thread_sturct_t ** tts);
int check_tested_thread_structures(tested_thread_sturct_t *tts);
int check_tested_thread_phase(tested_thread_sturct_t *tts, int phase);
int tested_thread_is_running(tested_thread_sturct_t *tts);
int compare_threads(jthread *threads, int thread_nmb, int compare_from_end);
int compare_pointer_sets(void ** set_a, void ** set_b, int nmb);
int check_exception(jobject excn);
jobject new_jobject();
void delete_jobject(jobject obj);
void set_phase(tested_thread_sturct_t *tts, int phase);
void default_run_for_test(void);
JNIEnv * new_JNIEnv();
