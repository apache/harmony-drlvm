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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#ifndef THREAD_MANAGER_HEADER
#define THREAD_MANAGER_HEADER


#ifdef __cplusplus
extern "C" {
#endif

struct tm_iterator_t{
    bool init;
    VM_thread * current;
};

void tm_acquire_tm_lock();
void tm_release_tm_lock();
bool tm_try_acquire_tm_lock();

tm_iterator_t * tm_iterator_create();
int             tm_iterator_release(tm_iterator_t *);
int             tm_iterator_reset(tm_iterator_t *);
VM_thread *     tm_iterator_next(tm_iterator_t *);
 
void vm_thread_shutdown();
void vm_thread_init(Global_Env *p_env);

VM_thread * get_a_thread_block();
void tmn_thread_attach();

void free_this_thread_block(VM_thread *);

extern VM_thread *p_free_thread_blocks;
extern VM_thread *p_active_threads_list;
extern VM_thread *p_threads_iterator;

extern volatile VM_thread *p_the_safepoint_control_thread;  // only set when a gc is happening
extern volatile safepoint_state global_safepoint_status;

extern unsigned non_daemon_thread_count;

extern VmEventHandle non_daemon_threads_dead_handle;
extern VmEventHandle new_thread_started_handle;

extern VmEventHandle non_daemon_threads_are_all_dead;

extern thread_array quick_thread_id[];
extern POINTER_SIZE_INT hint_free_quick_thread_id;

#ifdef __cplusplus
}
#endif

#endif // THREAD_MANAGER_HEADER
