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
 * @author Artem Aliev
 * @version $Revision: 1.1.2.1.4.5 $
 */
#include "hythreads.h"
#include "log_macro.h"


#ifdef WIN32
#include <windows.h>
#endif

#define GLOBAL_MONITOR_NAME "global_monitor"

#define RET_ON_ERROR(stat) if(stat) { return -1; }

extern "C" 
{

JNIEXPORT void  __cdecl hythread_init(void*  lib);

#ifdef WIN32
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpres) {
  if (dwReason == DLL_PROCESS_ATTACH) {
     hythread_init (NULL);
   }
   return TRUE;
}
#else
void __cdecl hythread_library_init(void) {
    hythread_init(NULL);

}
#endif


hythread_monitor_t p_global_monitor; 
// Will do all initilalization of thread library.
// init global_monitor now.
JNIEXPORT void  __cdecl hythread_init (void*  lib)  {
    apr_initialize();
    hythread_monitor_init_with_name(&p_global_monitor, 0, "Thread Global Monitor");
    hythread_monitor_t *mon = (hythread_monitor_t*)hythread_global(GLOBAL_MONITOR_NAME);
    *mon = p_global_monitor;
}



// ========================== MONITOR impl ================================================

static apr_pool_t* get_pool() {
    apr_pool_t *pool;
    apr_pool_create(&pool, 0);
    return pool;
}

JNIEXPORT int  __cdecl hythread_monitor_init_with_name (hythread_monitor_t* handle, unsigned flags, char* name) {
    apr_status_t stat;

    apr_pool_t *pool;
    apr_pool_create(&pool, 0);
        *handle = (hythread_monitor_t) apr_palloc(pool, sizeof(struct hymonitor));
    if(*handle == NULL) {
        return -1;
    }
    stat = apr_thread_mutex_create(&((*handle)->mutex), APR_THREAD_MUTEX_NESTED, pool);
    RET_ON_ERROR(stat)
    stat = apr_thread_cond_create(&((*handle)->cond), pool);
    RET_ON_ERROR(stat)
        (*handle)->name = name;
        return 0;
}

JNIEXPORT int __cdecl
hythread_monitor_destroy (hythread_monitor_t monitor){          
    apr_status_t stat;
    apr_pool_t *pool = apr_thread_mutex_pool_get (monitor->mutex);
    if(pool == NULL) {
        return -1;
    }
    stat = apr_thread_mutex_destroy(monitor->mutex);
    RET_ON_ERROR(stat)
    stat = apr_thread_cond_destroy(monitor->cond);
    RET_ON_ERROR(stat)
    apr_pool_destroy(pool);
    return 0;
}

JNIEXPORT int __cdecl 
hythread_monitor_try_enter (hythread_monitor_t monitor){ 
// TODO implement
    return -1; 
}
JNIEXPORT int __cdecl 
hythread_monitor_enter (hythread_monitor_t monitor){ 
    apr_status_t stat = apr_thread_mutex_lock(monitor->mutex);
    RET_ON_ERROR(stat)
    return 0; 
}
JNIEXPORT int __cdecl 
hythread_monitor_exit (hythread_monitor_t monitor) { 
    apr_status_t stat = apr_thread_mutex_unlock(monitor->mutex);
    RET_ON_ERROR(stat)
    return 0; 
}
JNIEXPORT int __cdecl
hythread_monitor_notify(hythread_monitor_t monitor){ 
    apr_status_t stat = apr_thread_cond_signal(monitor->cond);
    RET_ON_ERROR(stat)
    return 0; 
}
JNIEXPORT int __cdecl
hythread_monitor_notify_all (hythread_monitor_t monitor){ 
    apr_status_t stat = apr_thread_cond_broadcast(monitor->cond);
    RET_ON_ERROR(stat)
    return 0; 
}
JNIEXPORT int __cdecl
hythread_monitor_wait (hythread_monitor_t monitor){ 
    apr_status_t stat = apr_thread_cond_wait(monitor->cond, monitor->mutex);
    RET_ON_ERROR(stat)
    return 0; 
}

// ======================= ATTACH ======================================

JNIEXPORT int __cdecl 
hythread_attach (hythread_t* handle){
    if(handle) {
        *handle=hythread_self();
    }
    return 0; 
}
JNIEXPORT void __cdecl 
hythread_detach (hythread_t handle){}
JNIEXPORT hythread_t __cdecl hythread_self() {
    return apr_os_thread_current();
}

// very simple Map implementation
// current scenario use only one global so it works well
// need to be hashtable in the future
// TODO use hashtable

#define  TABLE_SIZE 256 
char *names [TABLE_SIZE];
unsigned data [TABLE_SIZE];
int size = 0;
// return index in array if found, -1 otherwise
int find_entry (char* name) {
    // quick pass
    int i;
    for (i = 0; i < size; i++) {
        if (names[i] == name) {
            return i;
        }
    }
    // strcmp pass.
    for (i = 0; i < size; i++) {
        if (strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}
//add entry to the end of the array
// retrun new entry index,  -1 if failed.
int add_entry(char* name) {
    int index = size++;
    if(index >= TABLE_SIZE-1) {
        return -1;
    }
    names[index] = name;
    data[index] = 0;
    return index;
}

JNIEXPORT unsigned* __cdecl 
hythread_global (char* name) { 
    //hythread_monitor_enter(*p_global_monitor);
    int index = find_entry(name);
    if(index == -1) {
        index = add_entry(name);
        assert(index >=0);
        if (index < 0) {
            //hythread_monitor_exit(*p_global_monitor);
            return NULL;
        }
    }
    //hythread_monitor_exit(*p_global_monitor);
    return data+index;
}

// ================== TLS =======================================


JNIEXPORT int __cdecl 
hythread_tls_alloc (hythread_tls_key_t* handle){  
    apr_status_t stat = apr_threadkey_private_create(handle,  NULL, get_pool());
    RET_ON_ERROR(stat)
    return 0; 
}

JNIEXPORT int __cdecl 
hythread_tls_free (hythread_tls_key_t key){ 
        
    apr_status_t stat = apr_threadkey_private_delete(key);
    RET_ON_ERROR(stat)
    return 0; 
}

// TODO method work only for current thread
JNIEXPORT void* __cdecl 
hythread_tls_get (hythread_t thread, hythread_tls_key_t key) { 
    void* result; 
    apr_status_t stat = apr_threadkey_private_get(&result, key);
    if(stat != 0) {
        return NULL;
    }
    return result; 
}

// TODO method work only for current thread
JNIEXPORT int __cdecl 
hythread_tls_set (hythread_t thread, hythread_tls_key_t key, void* value){ 
    apr_status_t stat = apr_threadkey_private_set(value, key);
    RET_ON_ERROR(stat)
    return 0; 
}
//  ==================== create thread ==========================
//#ifndef WIN32
static void* APR_THREAD_FUNC hystart_wrapper(apr_thread_t* t , void* args) {
    hythread_entrypoint_t entrypoint = (hythread_entrypoint_t)*((void **)args);
    void* hyargs = *((void**)args+1);
    return (void*) entrypoint(hyargs);
}

JNIEXPORT int  __cdecl
hythread_create(hythread_t* handle, unsigned stacksize, unsigned priority, unsigned suspend, hythread_entrypoint_t entrypoint, void* entryarg) {
    apr_thread_t *new_thread;
    apr_pool_t *pool = get_pool();
    static void **port_args = (void**)apr_palloc(pool, sizeof(void*)*2);
    port_args[0] = (void*)entrypoint;
    port_args[1] = (void*)entryarg;
    apr_status_t stat = apr_thread_create(&new_thread, NULL, hystart_wrapper,
                                            port_args, pool);
    RET_ON_ERROR(stat);
    stat = apr_os_thread_get(&handle, new_thread);
    RET_ON_ERROR(stat);
    return 0;
}


JNIEXPORT int __cdecl
hythread_exit (hythread_monitor_t monitor) {
   apr_status_t stat;
   apr_os_thread_t aott;
   apr_thread_t *att = NULL;

   // att = (apr_thread_t*)apr_pcalloc(get_pool(), sizeof(apr_thread_t*));	
   aott = apr_os_thread_current();
   stat = apr_os_thread_put(&att, &aott, get_pool());
   RET_ON_ERROR(stat);
    
   if (monitor) {
      hythread_monitor_exit(monitor);
   }

   return apr_thread_exit(att, 0);
}




}// extern "C"
