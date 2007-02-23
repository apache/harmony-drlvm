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
 * @author Intel, Evgueni Brevnov
 * @version $Revision$
 */  

/*    MONITOR ENTER RUNTIME SUPPORT    */

#include <assert.h>

#include "environment.h"

#include "open/hythread_ext.h"
#include "lil.h"
#include "lil_code_generator.h"
#include "jit_runtime_support.h"
#include "Class.h"
#include "mon_enter_exit.h"
#include "exceptions.h"
#include "exceptions_jit.h"

#define LOG_DOMAIN "vm.helpers"
#include "cxxlog.h"

#include "vm_stats.h"
#include "dump.h"

static LilCodeStub * rth_get_lil_monitor_enter_generic(LilCodeStub * cs) {
    if(VM_Global_State::loader_env->TI->isEnabled() &&
            VM_Global_State::loader_env->TI->get_global_capability(
                            DebugUtilsTI::TI_GC_ENABLE_MONITOR_EVENTS) ) {
        return lil_parse_onto_end(cs,
            "push_m2n 0, 0;"
            "out platform:ref:void;"
            "o0 = l0;"
            "call %0i;"
            "pop_m2n;"
            "ret;",
            vm_monitor_enter);
    } else {
        return lil_parse_onto_end(cs,
            "out platform:ref:g4;"
            "o0 = l0;"
            "call %0i;"
            "jc r!=%1i,slow_path;"
            "ret;"
            ":slow_path;"
            "push_m2n 0, 0;"
            "out platform:ref:void;"
            "o0 = l0;"
            "call %2i;"
            "pop_m2n;"
            "ret;",
            vm_monitor_try_enter,
            TM_ERROR_NONE,
            vm_monitor_enter);
    }
}

NativeCodePtr rth_get_lil_monitor_enter_static() {    
    static NativeCodePtr addr = NULL;
    
    if (addr != NULL) {
        return addr;
    }    

    LilCodeStub * cs = lil_parse_code_stub("entry 0:managed:pint:void;");
#ifdef VM_STATS
//    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_ENTER_STATIC, 0, NULL);
//    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
//    assert(cs);
#endif
        // convert struct Class into java_lang_Class
    cs = lil_parse_onto_end(cs,
        "in2out platform:ref;"
        "call %0i;"
        "locals 1;"
        "l0 = r;",
        struct_Class_to_java_lang_Class
    );
    assert(cs);

    // append generic monitor enter code
    cs = rth_get_lil_monitor_enter_generic(cs);
    assert(cs && lil_is_valid(cs));

    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "monitor_enter_static", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);
    return addr;
}

NativeCodePtr rth_get_lil_monitor_enter() {
    static NativeCodePtr addr = NULL;
    
    if (addr != NULL) {
        return addr;
    }    

    LilCodeStub * cs = lil_parse_code_stub("entry 0:managed:ref:void;");

#ifdef VM_STATS
//    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_ENTER, 0, NULL);
//    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
//    assert(cs);
#endif
    // check if object is null
    cs = lil_parse_onto_end(cs,
        "jc i0 = %0i:ref, throw_null_pointer;"
        "locals 1;"
        "l0 = i0;",
        (ManagedObject *) VM_Global_State::loader_env->managed_null
    );
    assert(cs);
    
    // append generic monitor enter code
    cs = rth_get_lil_monitor_enter_generic(cs);
    assert(cs);
    
    // throw NullPointerException
    cs = lil_parse_onto_end(cs,
        ":throw_null_pointer;"
        "out managed::void;"
        "call.noret %0i;",
        lil_npc_to_fp(exn_get_rth_throw_null_pointer())
    );
    assert(cs && lil_is_valid(cs));
    
    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB((char *)addr, "monitor_enter", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);
    return addr;
}

// this function doesn't throw NullPointerException in case of null object
NativeCodePtr rth_get_lil_monitor_enter_non_null() {
    static NativeCodePtr addr = NULL;
    
    if (addr != NULL) {
        return addr;
    }    

    LilCodeStub * cs = lil_parse_code_stub(
        "entry 0:managed:ref:void;"
        "locals 1;"
        "l0 = i0;"
    );    
    assert(cs);

#ifdef VM_STATS
//    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_ENTER_NON_NULL, 0, NULL);
//    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
//    assert(cs);
#endif
    // append generic monitor enter code
    cs = rth_get_lil_monitor_enter_generic(cs);
    assert(cs && lil_is_valid(cs));
    
    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "monitor_enter_non_null", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);
    return addr;
}


/*    MONITOR EXIT RUNTIME SUPPORT    */

static LilCodeStub * rth_get_lil_monitor_exit_generic(LilCodeStub * cs) {
    if(VM_Global_State::loader_env->TI->isEnabled() &&
            VM_Global_State::loader_env->TI->get_global_capability(
                            DebugUtilsTI::TI_GC_ENABLE_MONITOR_EVENTS) ) {
        return lil_parse_onto_end(cs,
            "locals 1;"
            "l0 = o0;"
            "push_m2n 0, %0i;"
            "out platform:ref:void;"
            "o0 = l0;"
            "call %1i;"
            "pop_m2n;"
            "ret;",
            FRAME_NON_UNWINDABLE,
            vm_monitor_exit);
    } else {
        return lil_parse_onto_end(cs,
            "call %0i;"
            "jc r!=%1i, illegal_monitor;"
            "ret;"
            ":illegal_monitor;"
            "out managed::void;"
            "call.noret %2i;",
            vm_monitor_try_exit,
            TM_ERROR_NONE,
            lil_npc_to_fp(exn_get_rth_throw_illegal_monitor_state()));
    }
}

NativeCodePtr rth_get_lil_monitor_exit_static() {    
    static NativeCodePtr addr = NULL;
    
    if (addr != NULL) {
        return addr;
    }    

    LilCodeStub * cs = lil_parse_code_stub("entry 0:managed:pint:void;");
#ifdef VM_STATS
//    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_EXIT_STATIC, 0, NULL);
//    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
//    assert(cs);
#endif
        // convert struct Class into java_lang_Class
    cs = lil_parse_onto_end(cs,
        "in2out platform:ref;"
        "call %0i;"
        "out platform:ref:g4;"
        "o0 = r;",
        struct_Class_to_java_lang_Class
    );
    assert(cs);
    
    // append generic monitor enter code
    cs = rth_get_lil_monitor_exit_generic(cs);
    assert(cs && lil_is_valid(cs));

    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "monitor_exit_static", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);
    return addr;
}


NativeCodePtr rth_get_lil_monitor_exit() {
    static NativeCodePtr addr = NULL;
    
    if (addr != NULL) {
        return addr;
    }    

    LilCodeStub * cs = lil_parse_code_stub("entry 0:managed:ref:void;");

#ifdef VM_STATS
//    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_EXIT, 0, NULL);
//    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
//    assert(cs);
#endif
    // check if object is null
    cs = lil_parse_onto_end(cs,
        "jc i0 = %0i:ref, throw_null_pointer;"
        "in2out platform:g4;",
        (ManagedObject *) VM_Global_State::loader_env->managed_null
    );

    assert(cs);
    
    // append generic monitor enter code
    cs = rth_get_lil_monitor_exit_generic(cs);
    assert(cs);
    
    // throw NullPointerException
    cs = lil_parse_onto_end(cs,
        ":throw_null_pointer;"
        "out managed::void;"
        "call.noret %0i;",
        lil_npc_to_fp(exn_get_rth_throw_null_pointer())
    );
    assert(cs && lil_is_valid(cs));
    
    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "monitor_exit", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);
    return addr;
}

// this function doesn't throw NullPointerException in case of null object
NativeCodePtr rth_get_lil_monitor_exit_non_null() {
    static NativeCodePtr addr = NULL;
    
    if (addr != NULL) {
        return addr;
    }    

    LilCodeStub * cs = lil_parse_code_stub(
        "entry 0:managed:ref:void;"
        "in2out platform:g4;"
    );    
    assert(cs);

#ifdef VM_STATS
//    int * value = VM_Statistics::get_vm_stats().rt_function_calls.lookup_or_add((void*)VM_RT_MONITOR_EXIT_NON_NULL, 0, NULL);
//    cs = lil_parse_onto_end(cs, "inc [%0i:pint];", value);
//    assert(cs);
#endif
    // append generic monitor enter code
    cs = rth_get_lil_monitor_exit_generic(cs);
    assert(cs && lil_is_valid(cs));
    
    addr = LilCodeGenerator::get_platform()->compile(cs);

    DUMP_STUB(addr, "monitor_exit_non_null", lil_cs_get_code_size(cs));

    lil_free_code_stub(cs);
    return addr;
}


Boolean jit_may_inline_object_synchronization(unsigned * UNREF thread_id_register,
                                              unsigned * UNREF sync_header_offset,
                                              unsigned * UNREF sync_header_width,
                                              unsigned * UNREF lock_owner_offset,
                                              unsigned * UNREF lock_owner_width,
                                              Boolean  * UNREF jit_clears_ccv) {
    return FALSE;
}
