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
#include "open/thread.h"
#include "object_generic.h"
#include "root_set_enum_internal.h"
#include "lock_manager.h"
#include "open/gc.h"

#define LOG_DOMAIN "thread_dump"
#include "cxxlog.h"


static std::set<void *> unique_references;
static int stack_key;

enum reference_types {
    root_reference = 1,
    compresses_root_reference,
    managed_reference,
    managed_reference_with_base
};

static void td_print_thread_dumps(FILE* f);
#ifdef _DEBUG
static void td_print_native_dumps(FILE* f);
#endif
static void td_attach_thread(void( *printer)(FILE *), FILE *out);

/**
 * The thread dump entry poin, this function being called from the signal handler
 */
void td_dump_all_threads(FILE *out) {
#ifdef _DEBUG
    td_print_native_dumps(out); 
#endif
    td_attach_thread(td_print_thread_dumps, out);
}

/**
 * Attaches current thread to vm and runs the given "Thread dump" thread function.
 */
static void td_attach_thread(void( *printer)(FILE *), FILE *out) {
    VM_thread *current;
    current = get_a_thread_block();

    if (NULL == current) {
        WARN("Thread dump: can't get a thread block");
        return;
    }

    p_thread_lock->_lock_enum();
    
    set_TLS_data(current);
    #ifdef PLATFORM_POSIX
    current->thread_handle = getpid();
    //MVM
    current->thread_id = GetCurrentThreadId();
    #endif
    tmn_suspend_enable();
    
    {
        NativeObjectHandles lhs;
        gc_thread_init(&current->_gc_private_information);

        printer(out);
    }
    
    free_this_thread_block(current);
    set_TLS_data(NULL);
    gc_thread_kill(&current->_gc_private_information);
    p_thread_lock->_unlock_enum();
}

/**
 * Returns java.lang.Thread pointer associated with te given VM_thread
 */
static jthread get_jthread(VM_thread * thread) {
    if (thread->p_java_lang_thread) {
        tmn_suspend_enable();
        ObjectHandle hThread = oh_allocate_global_handle();
        tmn_suspend_disable();
        hThread->object = (struct ManagedObject *)thread->p_java_lang_thread;
        
        tmn_suspend_enable();

        return (jthread)hThread;
    } else {
        return NULL;
    }
}

/**
 * Prints monitors owned by the currently processed thread.
 * During stack traversing JIT being called for live object enumeration. Object returned,
 * being checked if their "lockword" contains thread key of the current thread. Those objects which
 * maches being collected in the unique references set.
 *
 * This function prints objects collected and frees the set for the next thread;
 */ 
static void td_print_owned_monitors(FILE *out) {
    if (unique_references.size()) {
        std::set<void *>::const_iterator it;
        fprintf(out, "Thread owns following monitors:\n");
        ObjectHandle jmon       = oh_allocate_local_handle();
        ObjectHandle jmon_class = oh_allocate_local_handle();
        for (it = unique_references.begin(); it != unique_references.end(); it++) {
            ManagedObject *p_obj = (ManagedObject *)*it;
            
            tmn_suspend_disable();       //---------------------------------v
            jmon_class->object = struct_Class_to_java_lang_Class(p_obj->vt()->clss);
            jmon->object = p_obj;
            tmn_suspend_enable();        //---------------------------------^
            

            JNIEnv *jenv = (JNIEnv *)jni_native_intf;
            jmethodID mon_name     = jenv -> GetMethodID(jmon_class, "getName","()Ljava/lang/String;");
            jstring  _mon_name = jenv -> CallObjectMethod (jmon, mon_name);
            char *class_name = (char *)jenv -> GetStringUTFChars (_mon_name, false);
              
            fprintf(stderr, " - %s@0x%ld\n", class_name, generic_hashcode(p_obj));
        }

        unique_references.clear();
    }

}

/**
 * Prints symbolic name for the given numeric java state.
 */
static char *print_thread_state(java_state state) {
    static char *names[8] = {"zip",
    "sleeping",
    "waiting",
    "timed_waiting", 
    "birthing",
    "running",
    "blocked",
    "dying"};

    return names[state];
}

/**
 * checks if the thread is alive.
 * Note:
 *      Only alive threads being processed during the thread dump.
 */
static int td_is_alive(VM_thread *thread) {
    if ( !thread ) {
        return 0; // don't try to isAlive() non-existant thread
    }

    if (thread->app_status == zip) {
        return 0;  // don't try to isAlive() non-existant thread
    }
    java_state as = thread->app_status;  

    // According to JAVA spec, this method should return true, if and
    // only if the thread has been started and not yet died. 
    switch (as) {
        case thread_is_sleeping:
        case thread_is_waiting:
        case thread_is_timed_waiting:
        case thread_is_blocked:
        case thread_is_running:
            return 1; 
        case thread_is_dying:
        case thread_is_birthing:   
            return 0; 
        default:
            return 0;
    }
}

/**
 * Print java.lang.Thread info in the following format:
 * "<thread name>" prio=<thread priority> id=<thread id> <java state>
 * ^--- <monitor name> - if blocked or waiting
 *
 * Returns true if jthread header was successfully printed;
 */
static bool td_print_java_thread_info(FILE *f, VM_thread *thread, bool *is_header_printed) {
    static JNIEnv *jenv = (JNIEnv *)jni_native_intf;
    static jclass cl = jenv -> FindClass("java/lang/Thread");
    static jmethodID method_name     = jenv -> GetMethodID(cl, "getName","()Ljava/lang/String;");
    static jmethodID method_prio = jenv -> GetMethodID(cl, "getPriority","()I");
   
    
    jthread _jthread = get_jthread(thread);
    if (!_jthread) {
        return false;
    } else {
        if (!*is_header_printed) {
            *is_header_printed = true;
            fprintf(f,"=== FULL THREAD DUMP\n");
        }
    }
    
    jstring  name = jenv -> CallObjectMethod (_jthread, method_name);
    char *_name = (char *)jenv -> GetStringUTFChars (name, false);
    jint prio = jenv -> CallIntMethod(_jthread, method_prio);
    java_state thread_st = thread->app_status;
        
    fprintf(f,"\"%s\" prio=%d id=0x%lX skey=0x%lX %s\n", 
            _name, prio, (long)thread->thread_id, thread->stack_key, print_thread_state(thread_st));
    
    if (thread_st == 2 
            || thread_st == 3
            || thread_st == 6) {
        //blocked/waiting on monitor
        ////
        ManagedObject *p_mon = mon_enter_array[thread->thread_index].p_obj;
        if (p_mon) {
            ObjectHandle jmon       = oh_allocate_local_handle();
            ObjectHandle jmon_class = oh_allocate_local_handle();
            tmn_suspend_disable();       //---------------------------------v
            jmon_class->object= struct_Class_to_java_lang_Class(p_mon->vt()->clss);
            jmon->object = p_mon;
            tmn_suspend_enable();        //---------------------------------^
            

            jmethodID mon_name     = jenv -> GetMethodID(jmon_class, "getName","()Ljava/lang/String;");
            jstring  _mon_name = jenv -> CallObjectMethod (jmon, mon_name);
            char *class_name = (char *)jenv -> GetStringUTFChars (_mon_name, false);

            fprintf(f,"^--- %s on monitor : %s@%lx\n", print_thread_state(thread_st), class_name, generic_hashcode(p_mon));
        }
    }
    return true;
}

/**
 * Prints stack trace entry in the following format:
 *
 *   at <class name>.<method name><method descripto>(<source info>)@<instruction pointer>,
 *   
 *   where source info is on of the following:
 *   - <java source file name>:<line number>
 *   - Native source
 *   - Unknown source
 */
static void td_print_entry(FILE *f, Method_Handle m, int ip, bool is_native) {
    const char *file_name;
    
    fprintf(f, "  at %s.%s%s(", class_get_name(method_get_class(m)), method_get_name(m), method_get_descriptor(m));
    
    if (is_native) {
        fprintf(f,"Native Method)\n");
    } else {
        file_name = class_get_source_file_name(method_get_class(m));
        
        if (file_name) {
            fprintf(f,"%s : %d", file_name, m->get_line_number((uint16)ip));
        } else {
            fprintf(f,"Unknown Source");
        }
        fprintf(f,")@0x%X\n", ip);
    }
}


static void td_print_thread_dumps(FILE* f) {
    bool print_footer = false;
    suspend_all_threads_except_current_generic();
    VM_thread *thread = p_active_threads_list;
    
    
    while(thread) {
        if (!(td_is_alive(thread) && td_print_java_thread_info(f, thread, &print_footer))) {
            thread = thread->p_active;
            continue;
        }
        
        stack_key = thread->stack_key;
        StackIterator* si = si_create_from_native(thread);
        unsigned depth = 0;
        
        while (!si_is_past_end(si)) {
            Method_Handle m = get_method(si);
            if (m) {
                CodeChunkInfo* cci = si_get_code_chunk_info(si);
                if ( cci != NULL ) {
                    cci->get_jit()->get_root_set_for_thread_dump(cci->get_method(), 0, si_get_jit_context(si));
                    uint32 inlined_depth = si_get_inline_depth(si);
                    uint32 offset = (POINTER_SIZE_INT)si_get_ip(si) - (POINTER_SIZE_INT)cci->get_code_block_addr();
                    
                    for (uint32 i = 0; i < inlined_depth; i++) {
                        Method *real_method = cci->get_jit()->get_inlined_method(cci->get_inline_info(), offset, i);
                        
                        td_print_entry(f, real_method, offset, false);
                        depth++;
                    }
                }
                
                td_print_entry(f, m, (uint8*)si_get_ip(si) - (uint8*)((Method *)m)->get_byte_code_addr(), si_is_native(si));
            }
            
            depth++;
            si_goto_previous(si);
        }
        si_free(si);

        td_print_owned_monitors(f);
        
        fprintf(f, "--- End Stack Trace (0x%X, depth=%d)\n\n", thread, depth);
        thread = thread->p_active;

    }
    
    if (print_footer) fprintf(f,"=== END OF THREAD DUMP\n\n");
    resume_all_threads_generic();
}

VMEXPORT void vm_check_if_monitor(void  **reference,
                                           void  **reference_base,
                                           uint32 *compressed_reference, 
                                           int     slotOffset, 
                                           Boolean pinned,
                                           int     type) {
    
    ManagedObject *p_obj = NULL;
    
    if (!(type>0 && type<5)) return;
    
    switch((reference_types)type) {
        case root_reference: {
            if (reference) {
                p_obj = (ManagedObject *)*reference;
            }
            break;
        }
                             
        case compresses_root_reference: {
            COMPRESSED_REFERENCE cref = *compressed_reference;
            ManagedObject* obj = (ManagedObject *)uncompress_compressed_reference(cref);
            if (cref != 0
                    && (((POINTER_SIZE_INT)Class::heap_base <= (POINTER_SIZE_INT)obj) 
                        && ((POINTER_SIZE_INT)obj <= (POINTER_SIZE_INT)Class::heap_end))
               ) {
                p_obj = obj;
            }
            break;
        }
                                        
        case managed_reference_with_base: {
            slotOffset = (int)(POINTER_SIZE_INT)(*((Byte**)reference)-*((Byte**)reference_base));
            
        }

        case managed_reference: {
            p_obj = (ManagedObject *)((Byte*)*reference - slotOffset);
            break;
        }
                                
        default : return;
    }    
    
    if (p_obj) {
        uint16 *sk = P_STACK_KEY(p_obj);

        if(stack_key == *sk) {
            unique_references.insert(p_obj);
        }
    }
}

#ifdef _DEBUG

char* get_lock_name(void* lock) {
    if(p_thread_lock == lock) {
        return "p_thread_lock";
    }
    if(p_jit_a_method_lock == lock) {
        return "p_jit_a_method_lock";
    }
    if(p_vtable_patch_lock == lock) {
        return "p_vtable_patch_lock";
    }
    if(p_meth_addr_table_lock == lock) {
        return "p_meth_addr_table_lock";
    }
    if(p_method_call_lock == lock) {
        return "p_method_call_lock";
    }
    if(p_tm_lock == lock) {
        return "p_tm_lock";
    }
    if (JAVA_CODE_PSEUDO_LOCK == lock) {
        return "java_code";
    }
    return "unknown";
}

int get_lock_priority(void* lock) {
    if(p_thread_lock == lock) {
        return 10;
    }
    if(p_vtable_patch_lock == lock) {
        return 30;
    }
    if(p_meth_addr_table_lock == lock) {
        return 40;
    }

    if(p_method_call_lock == lock) {
        return 50;
    }
    if(p_jit_a_method_lock == lock) {
        return 55;
    }
    if (JAVA_CODE_PSEUDO_LOCK == lock) {
        return 70;
    }
    return -1;
}


void check_lock_order(VM_thread *thread) {
    if (!thread) {
        INFO("Cannot check lock order without a thread block");
        return;
    }

    int lock_id = thread->locks_size-1;
    if(lock_id < 1) {
        return;
    }   
    void *last_lock = thread->locks[thread->locks_size-1];
    if(get_lock_priority(last_lock) == -1) {
        return;
    }
       
    int lock_priority;
    void *prev_lock;  
    for (lock_id= lock_id-1; lock_id >=0; lock_id--) {
        prev_lock = thread->locks[lock_id];
        lock_priority =  get_lock_priority(prev_lock);
        if(lock_priority !=-1 && get_lock_priority(last_lock) > get_lock_priority(prev_lock)) {
            LOG2("deadlock", "possible deadlock (incorrect lock order):" << get_lock_name(last_lock) << " after " << get_lock_name(prev_lock) );   
        }
    }
    
}

void push_lock(VM_thread *thread, void* lock) 
{    
    if (!thread) {
        INFO("Attempt to push " << get_lock_name(lock) << " without a thread block");
        return;
    }

    TRACE("Thread " << thread->thread_handle << " acquired " << get_lock_name(lock) << " lock");
    //assert(thread->contendent_lock == lock);
    thread->contendent_lock = NULL;
    thread->locks[thread->locks_size++] = lock;
    ASSERT(thread->locks_size < MAX_THREAD_LOCKS, "Debugging lock stack overflowed. Last locks: " \
        << get_lock_name(thread->locks[thread->locks_size - 1]) << ", " \
        << get_lock_name(thread->locks[thread->locks_size - 2]) << ", " \
        << get_lock_name(thread->locks[thread->locks_size - 3]) << ", " \
        << get_lock_name(thread->locks[thread->locks_size - 4]));
    
    check_lock_order(thread);

}   

void pop_lock(VM_thread *thread, void* lock) {
    if (!thread) {
        INFO("Attempt to pop " << get_lock_name(lock) << " without a thread block");
        return;
    }
    TRACE("Thread " << thread->thread_handle << " released " << get_lock_name(lock) << " lock");
    ASSERT(thread->locks_size > 0, "Lock stack is empty, unexpected unlock "  <<  get_lock_name(lock));

    void *res = thread->locks[--thread->locks_size];
    ASSERT(res == lock, "Incorrect order, expected unlocking " << get_lock_name(res) \
        << " before unlocking " <<  get_lock_name(lock) << ", thread block is " << thread);
}

void contends_lock(VM_thread *thread, void* lock) {
    if (!thread) {
        INFO("Attempt to contend on " << get_lock_name(lock) << " without a thread block");
        return;
    }

    //assert(thread->contendent_lock == NULL);
    thread->contendent_lock=lock;
}

static void td_print_native_dumps(FILE* f) {
    VM_thread *thread = p_active_threads_list;
    fprintf (f, "Native thread locks dump:\n");
    while(thread) {
        fprintf(f, "thread %ld locks:", thread->thread_id);
        if(thread->suspend_enabled_status) {
            fprintf(f, " suspend_disabled%d, ", thread->suspend_enabled_status);
            if(thread->suspend_request) {
                fprintf(f, " suspend_request = %d, ", thread->suspend_request);
            }
        }
        for (int i= 0; i < thread->locks_size; i++) {
            fprintf(f, "%s, ", get_lock_name(thread->locks[i]));
        }
        if(thread->contendent_lock) {
            fprintf(f, "\n\t contends on %s\n", get_lock_name(thread->contendent_lock));
        } else {
            fprintf(f, "\n");
        }
        thread = thread->p_active;
    }
}
#endif
