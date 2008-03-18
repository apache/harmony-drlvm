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

#include <string>

#define LOG_DOMAIN "signals"
#include "cxxlog.h"
#include "open/platform_types.h"
#include "Class.h"
#include "interpreter.h"
#include "environment.h"
#include "exceptions.h"
#include "exceptions_jit.h"
#include "compile.h"
#include "signals.h"


/*
 * Information about stack
 */
static inline void* find_stack_addr() {
    void* stack_addr;
    size_t reg_size;
    MEMORY_BASIC_INFORMATION memory_information;

    VirtualQuery(&memory_information, &memory_information, sizeof(memory_information));
    reg_size = memory_information.RegionSize;
    stack_addr =((char*) memory_information.BaseAddress) + reg_size;

    return stack_addr;
}

static inline size_t find_guard_page_size() {
    size_t  guard_size;
    SYSTEM_INFO system_info;

    GetSystemInfo(&system_info);
    guard_size = system_info.dwPageSize;

    return guard_size;
}

static inline size_t find_guard_stack_size() {
#   ifdef _EM64T_
       //this code in future should be used on both platforms x86-32 and x86-64
        return 64*1024;
#   else
        // guaerded stack size on windows 32 can be equals one page size only :(
        return find_guard_page_size();
#   endif
}

static size_t common_guard_stack_size;
static size_t common_guard_page_size;

static inline void* get_stack_addr() {
    return jthread_self_vm_thread_unsafe()->stack_addr;
}

static inline size_t get_stack_size() {
  return jthread_self_vm_thread_unsafe()->stack_size;
}

static inline size_t get_guard_stack_size() {
    return common_guard_stack_size;
}

static inline size_t get_guard_page_size() {
    return common_guard_page_size;
}


void init_stack_info() {
    vm_thread_t vm_thread = jthread_self_vm_thread_unsafe();
    vm_thread->stack_addr = find_stack_addr();
    vm_thread->stack_size = hythread_get_thread_stacksize(hythread_self());
    common_guard_stack_size = find_guard_stack_size();
    common_guard_page_size = find_guard_page_size();


    //this code in future should be used on both platforms x86-32 and x86-64
#   ifdef _EM64T_
    ULONG guard_stack_size_param = (ULONG)common_guard_stack_size;

    if (!SetThreadStackGuarantee(&guard_stack_size_param)) {
        // should be successful always
        assert(0);
    }
#   endif
}

static inline size_t get_mem_protect_stack_size() {
    return 0x0100;
}

static inline size_t get_restore_stack_size() {
    return get_mem_protect_stack_size() + 0x0100;
}

bool set_guard_stack() {
    void* stack_addr = get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t page_size = get_guard_page_size();
    size_t guard_stack_size = get_guard_stack_size();
  
    if (((size_t)(&stack_addr) - get_mem_protect_stack_size())
            < ((size_t)((char*)stack_addr - stack_size 
            + 2 * page_size + guard_stack_size))) {
        return false;
    }

    if (!VirtualFree((char*)stack_addr - stack_size + page_size,
        page_size, MEM_DECOMMIT)) {
        // should be successful always
        assert(0);
    }

    if (!VirtualAlloc( (char*)stack_addr - stack_size + page_size + guard_stack_size,
        page_size, MEM_COMMIT, PAGE_GUARD | PAGE_READWRITE)) {
        // should be successful always
        assert(0);
    }
    jthread_self_vm_thread_unsafe()->restore_guard_page = false;

    return true;
}

void remove_guard_stack(vm_thread_t vm_thread) {
    void* stack_addr = get_stack_addr();
    size_t stack_size = get_stack_size();
    size_t page_size = get_guard_page_size();
    size_t guard_stack_size = get_guard_stack_size();
    DWORD oldProtect;

    assert(((size_t)(&stack_addr)) > ((size_t)((char*)stack_addr - stack_size + 3 * page_size)));
    vm_thread->restore_guard_page = true;

    if (!VirtualProtect((char*)stack_addr - stack_size + page_size + guard_stack_size,
        page_size, PAGE_READWRITE, &oldProtect)) {
        // should be successful always
        assert(0);
    }
}

size_t get_available_stack_size() {
    char* stack_addr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_addr) - ((size_t)(&stack_addr));
    size_t available_stack_size;

    if (!p_TLS_vmthread->restore_guard_page) {
        available_stack_size = get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    } else {
        available_stack_size = get_stack_size() - used_stack_size - get_guard_page_size();
    }

    if (available_stack_size > 0) {
        return (size_t) available_stack_size;
    } else {
        return 0;
    }
}

bool check_available_stack_size(size_t required_size) {
    size_t available_stack_size = get_available_stack_size();
    if (available_stack_size < required_size) {
        if (available_stack_size < get_guard_stack_size()) {
            remove_guard_stack(p_TLS_vmthread);
        }
        Global_Env *env = VM_Global_State::loader_env;
        exn_raise_by_class(env->java_lang_StackOverflowError_Class);
        return false;
    } else {
        return true;
    }
}

bool check_stack_size_enough_for_exception_catch(void* sp) {
    char* stack_adrr = (char*) get_stack_addr();
    size_t used_stack_size = ((size_t)stack_adrr) - ((size_t)sp);
    size_t available_stack_size =
            get_stack_size() - used_stack_size
            - 2 * get_guard_page_size() - get_guard_stack_size();
    return get_restore_stack_size() < available_stack_size;
}



Boolean stack_overflow_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", ("SOE detected at ip=%p, sp=%p",
                            regs->get_ip(), regs->get_sp()));

    vm_thread_t vmthread = get_thread_ptr();
    Global_Env* env = VM_Global_State::loader_env;
    void* saved_ip = regs->get_ip();
    void* new_ip = NULL;

    if (is_in_ti_handler(vmthread, saved_ip))
    {
        new_ip = vm_get_ip_from_regs(vmthread);
        regs->set_ip(new_ip);
    }

    if (!vmthread || env == NULL)
        return FALSE; // Crash

    vmthread->restore_guard_page = true;

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    ncai_process_signal_event((NativeCodePtr)regs->get_ip(),
                                (jint)signum, false, &is_handled);
    if (is_handled)
    {
        if (new_ip)
            regs->set_ip(saved_ip);
        return TRUE;
    }

    Class* exn_class = env->java_lang_StackOverflowError_Class;

    if (is_in_java(regs))
    {
        signal_throw_java_exception(regs, exn_class);
    }
    else if (is_unwindable())
    {
        if (hythread_is_suspend_enabled())
            hythread_suspend_disable();
        signal_throw_exception(regs, exn_class);
    } else {
        exn_raise_by_class(exn_class);
    }

    if (new_ip && regs->get_ip() == new_ip)
        regs->set_ip(saved_ip);

    return TRUE;
}

Boolean null_reference_handler(port_sigtype UNREF signum, Registers* regs, void* fault_addr)
{
    TRACE2("signals", "NPE detected at " << regs->get_ip());

    vm_thread_t vmthread = get_thread_ptr();
    Global_Env* env = VM_Global_State::loader_env;
    void* saved_ip = regs->get_ip();
    void* new_ip = NULL;

    if (is_in_ti_handler(vmthread, saved_ip))
    {
        new_ip = vm_get_ip_from_regs(vmthread);
        regs->set_ip(new_ip);
    }

    if (!vmthread || env == NULL ||
        !is_in_java(regs) || interpreter_enabled())
        return FALSE; // Crash

    // Pass exception to NCAI exception handler
    bool is_handled = 0;
    ncai_process_signal_event((NativeCodePtr)regs->get_ip(),
                                (jint)signum, false, &is_handled);
    if (is_handled)
    {
        if (new_ip)
            regs->set_ip(saved_ip);
        return TRUE;
    }

    signal_throw_java_exception(regs, env->java_lang_NullPointerException_Class);

    if (new_ip && regs->get_ip() == new_ip)
        regs->set_ip(saved_ip);

    return TRUE;
}
