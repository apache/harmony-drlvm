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
 * @version $Revision: 1.1.2.2.4.4 $
 */  


#ifndef _VM_THREADS_H_
#define _VM_THREADS_H_


#ifdef PLATFORM_POSIX
#include <semaphore.h>
#include "platform.h"
#else
#include "vm_process.h"
#endif

#include "open/types.h"
#include "vm_core_types.h"
#include "object_layout.h"
#include "open/vm_gc.h"
#include "jvmti.h"

#define GC_BYTES_IN_THREAD_LOCAL (20 * sizeof(void *))
#define MAX_OWNED_MONITORS 100


// for SMP use the below
#define vm_lock_prefix lock

enum java_state {
    zip = 0,
    thread_is_sleeping,
    thread_is_waiting,
    thread_is_timed_waiting, 
    thread_is_birthing,
    thread_is_running,
    thread_is_blocked,
    thread_is_dying
};
// These are thread level gc states. 
enum gc_state {
    zero = 0,
    gc_moving_to_safepoint,
    gc_at_safepoint,
    gc_enumeration_done
};

#if defined(PLATFORM_POSIX) && defined(_IPF_)
enum register_state {
    NOT_VALID = (uint64)-1,
};

enum suspend_state {
    NOT_SUSPENDED = 0,
    SUSPENDED_IN_SIGNAL_HANDLER = 1,
    SUSPENDED_IN_DISABLE_GC_FOR_THREAD = 2,
};
#endif

typedef struct _thread_array {
    VM_thread *p_vmthread;
} thread_array;

typedef struct {
    jvmtiEnv * env;
    void * data;
} JVMTILocalStorage;

struct jvmti_frame_pop_listener;

class VmRegisterContext;

class VM_thread {

public:
    // FIXME: workaround for strange bug. If this line is removed Visual Stidio
    // has corrupted list of modules and no debug info at all.
    void* system_private_data;

    // Pointer to java.lang.Thread associated with this VM_thread
    // TODO: replace with jthread?
    volatile Java_java_lang_Thread*   p_java_lang_thread;

    // In case exception is thrown, Exception object is put here
    // TODO: Needs to be replaced with jobject!
    volatile ManagedObject*           p_exception_object;

    // Should JVMTI code be notified about exception in p_exception_object
    bool                              ti_exception_callback_pending;

    VM_thread*                        p_free;
    
    // Next active thread in the global tread list
    VM_thread*                        p_active;

    // ??
    java_state                        app_status;

    // ??
    gc_state                          gc_status; 

    // Thread states as specified by JVMTI (alive, running, e.t.c.)
    unsigned                          jvmti_thread_state;

    // Flag indicating whether thread stopped. Duplicate of thread_state?
    bool                              is_stoped;

    // ??
    bool                              interrupt_a_waiting_thread;


    // JVMTI support. Seems to be duplicate.
    jobject                           jvmti_owned_monitors[MAX_OWNED_MONITORS];
    int                               jvmti_owned_monitor_count;
    JVMTILocalStorage                 jvmti_local_storage;
    jvmti_frame_pop_listener          *frame_pop_listener;

    // CPU registers.
    Registers                         regs;

    // This field is private the to M2nFrame module, init code should set it to NULL
    // Informational frame - created when native is called from Java,
    // used to store local handles (jobjects) + registers.
    // =0 if there is no m2n frame.
    M2nFrame*                         last_m2n_frame;

    // GC Information
    Byte                              _gc_private_information[GC_BYTES_IN_THREAD_LOCAL];
    NativeObjectHandles*              native_handles;
    GcFrame*                          gc_frames;
    //gc_enable_disable_state           gc_enabled_status;
    bool                              gc_wait_for_enumeration;
    bool                              restore_context_after_gc_and_resume;
    
    int                              finalize_thread_flags;

    VmEventHandle                    event_handle_monitor;
    VmEventHandle                    event_handle_sleep;
    VmEventHandle                    event_handle_interrupt;
    VmThreadHandle                   thread_handle;
    VmEventHandle                    jvmti_resume_event_handle;
    pthread_t                         thread_id;

    VmEventHandle                    park_event;
    // OS specifics goes below
    
        // ============== new SUSPEND related variables: =====================
    volatile int           suspend_enabled_status; // reentrant implementation
                                              // 0 --enable, >0 disable
    volatile int suspend_request; // request to suspend the thread, countable
                                 // should relplace suspend_count
    
    VmEventHandle suspended_event; // set when thread is suspended 
                                   // should replace is_suspended

    VmEventHandle resume_event;
    //sem_t sem_resume;              // post it to resume
                                   // should replace suspend_sem
    //old suspend related variables 
        // How many times thread was suspended? Need to be resumed same amount of time. 
    int                               suspend_count;

    // Flag indicating whether thread suspended. Duplicate of thread_state?
    bool                              is_suspended;

#if defined(PLATFORM_POSIX)
    sem_t                             yield_other_sem;
#endif    
    
#if defined(PLATFORM_POSIX) && defined(_IPF_)
    // Linux/IPF
    sem_t                             suspend_sem;     // To suspend thread in signal handler
    sem_t                             suspend_self;    // To suspend current thread for signal handler
    uint64                            suspended_state; // Flag to indicate how the one thread is suspended
                                                       // Possible values:
                               // NOT_SUSPENDED, 
                               // SUSPENDED_IN_SIGNAL_HANDLER,
                               // SUSPENDED_IN_DISABLE_GC_FOR_THREAD
    uint64                            t[2]; // t[0] <= rnat, t[1] <= bspstore for current thread context
                                            // t[0] <= rnat, t[1] <= bsp      for other   thread context
#endif

    // ??
    int                               thread_index;

    // object wait/notify event
    int                               notify_recursion_count;
    VmEventHandle                    event_handle_notify_or_interrupt;    

    unsigned short                    stack_key;
    // ??
    void *lastFrame;
    void *firstFrame;
    int interpreter_state;

    void* get_ip_from_regs() { return regs.get_ip(); }
    void reset_regs_ip() { return regs.reset_ip(); }

    // TODO: need to be moved from here to the right place
    int setPriority(int priority);
    // is it used??
    void * jvmtiLocalStorage;

    // JVMTI support. Also needs to be moved somewhere.
    jint get_jvmti_thread_state(jthread thread);
    void set_jvmti_thread_state(int one_bit_state);

    // dead-lock checks code
#define MAX_THREAD_LOCKS 256
#ifdef _DEBUG 
    void* locks[MAX_THREAD_LOCKS];
    int locks_size;
    void* contendent_lock;
    void** stack_end;       /// The upper boundary of the stack to scan when verifying stack enumeration
#endif //_DEBUG

};

   // dead-lock checks code

#ifdef _DEBUG 
void push_lock(VM_thread *thread, void* lock);
void pop_lock(VM_thread *thread, void* lock);
void contends_lock(VM_thread *thread, void* lock);
#define JAVA_CODE_PSEUDO_LOCK ((void*)-2)

#define DEBUG_PUSH_LOCK(lock) push_lock(p_TLS_vmthread, lock)
#define DEBUG_POP_LOCK(lock) pop_lock(p_TLS_vmthread, lock)
#define DEBUG_CONTENDS_LOCK(lock) contends_lock(p_TLS_vmthread, lock)
#else //_DEBUG
#define DEBUG_PUSH_LOCK(lock) 
#define DEBUG_POP_LOCK(lock)
#define DEBUG_CONTENDS_LOCK(lock)
#endif //_DEBUG


extern int next_thread_index;

#if defined(PLATFORM_POSIX) || defined(_IPF_) || defined (_IPF) || defined (WIN64)
#define USE_TLS_API
#endif

#ifdef USE_TLS_API
#ifdef PLATFORM_POSIX
extern __thread VM_thread *p_TLS_vmthread;
//#define p_TLS_vmthread ((VM_thread *)pthread_getspecific(TLS_key_pvmthread))
#define init_TLS_data() //pthread_key_create(&TLS_key_pvmthread, NULL) 
#define set_TLS_data(pvmthread) do {    \
    /*pthread_setspecific(TLS_key_pvmthread, (const void *)pvmthread);*/    \
    p_TLS_vmthread = pvmthread; \
    if (pvmthread != (void*)NULL) \
        ((VM_thread *)pvmthread)->stack_key = \
            (unsigned short)((VM_thread *)pvmthread)->thread_index;      \
  } while(0);
#else //PLATFORM_POSIX
extern __declspec( thread ) VM_thread *p_TLS_vmthread;
#define set_TLS_data(pvmthread) do { \
     p_TLS_vmthread = pvmthread;\
    if (pvmthread != (void*)NULL) \
         ((VM_thread *)pvmthread)->stack_key = \
             ((VM_thread *)pvmthread)->thread_index; \
  } while(0);
#endif //!PLATFORM_POSIX 
uint16 get_self_stack_key();
VM_thread *get_thread_ptr();

#else //USE_TLS_API

#pragma warning(push) 
#pragma warning(disable:4035)
inline VM_thread *get_thread_ptr()
{
    VM_thread *p_thr;
    _asm{ mov eax, fs:[0x14]
          mov p_thr, eax 
    }
    return p_thr;

}
#pragma warning(pop) 
inline void set_TLS_data(VM_thread *pvmthread)  
{ 
    _asm{ 
        mov eax, pvmthread
        mov fs:[0x14], eax
    }
    if (pvmthread != (void*)NULL)
        pvmthread->stack_key = pvmthread->thread_index; 
}
#define p_TLS_vmthread (get_thread_ptr())
uint16 get_self_stack_key();
#endif //!USE_TLS_API

#ifdef PLATFORM_POSIX
bool SuspendThread(unsigned tid);
bool ResumeThread(unsigned tid);
#define SUSPEND_THREAD(vmthread) SuspendThread((vmthread)->thread_id)
#define RESUME_THREAD(vmthread) ResumeThread((vmthread)->thread_id)

#else //PLATFORM_POSIX

#define SUSPEND_THREAD(vmthread) SuspendThread((vmthread)->thread_handle)
#define RESUME_THREAD(vmthread) ResumeThread((vmthread)->thread_handle)
#endif //!PLATFORM_POSIX


bool thread_suspend_generic(VM_thread *thread);
bool thread_resume_generic(VM_thread *thread);

void suspend_all_threads_except_current_generic();
void resume_all_threads_generic();

VMEXPORT void suspend_all_threads_except_current();

VMEXPORT void resume_all_threads();

void jvmti_thread_resume(VM_thread *thread);

void object_locks_init();

void __cdecl call_the_run_method( void * p_xx );

/*
 * Thread park/unpark methods for java.util.concurrent.LockSupport;
 */
int park(void);
int parktimed(jlong milis, jint nanos);
int parkuntil(jlong milis, jint nanos);
int unpark(VM_thread *thread);



void thread_gc_suspend_one(VM_thread *);
uint32 thread_gc_number_of_threads(); 
VM_thread *thread_gc_enumerate_one();
Registers *thread_gc_get_context(VM_thread *, VmRegisterContext &);
void thread_gc_set_context(VM_thread *);

struct Global_Env;

#ifdef __cplusplus
extern "C" {
#endif
 
void vm_thread_shutdown();
void vm_thread_init(Global_Env *p_env);

VM_thread * get_a_thread_block();

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

     
#endif //!_VM_THREADS_H_
