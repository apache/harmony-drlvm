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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.4.2.2.2.3 $
 */  
#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"
#include "vm_log.h"

#include "lock_manager.h"
#include "classloader.h"
#include "method_lookup.h"
#include "exceptions.h"
#include "native_overrides.h"
#include "jit_intf_cpp.h"
#include "em_intf.h"
#include "heap.h"
#include "open/thread.h"
#include "vm_stats.h"
#include "vm_strings.h"
#include "compile.h"
#include "jit_runtime_support.h"
#include "lil_code_generator.h"
#include "stack_iterator.h"
#include "interpreter.h"

extern bool parallel_jit;

#ifndef NDEBUG
#include "dump.h"
extern bool dump_stubs;
#endif


#define METHOD_NAME_BUF_SIZE 512

Global_Env* compile_handle_to_environment(Compile_Handle h)
{
    return ((Compilation_Handle*)h)->env;
}


////////////////////////////////////////////////////////////////////////
// begin Forward declarations

// A MethodInstrumentationProc that records calls in a call graph weighted by the call counts between methods.
void count_method_calls(CodeChunkInfo *callee);

// end Forward declarations
////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////
// begin CodeChunkInfo


CodeChunkInfo::CodeChunkInfo()
{
    _jit    = 0;
    _method = 0;
    _id     = 0;
    _relocatable = TRUE;
    _num_target_exception_handlers = 0;
    _target_exception_handlers     = NULL;
    _has_been_loaded_for_vtune     = false;
    _callee_info = _static_callee_info;
    _max_callees = NUM_STATIC_CALLEE_ENTRIES;
    _num_callees = 0;
    _heat        = 0;
    _code_block     = NULL;
    _jit_info_block = NULL;
    _code_block_size      = 0;
    _jit_info_block_size  = 0;
    _code_block_alignment = 0;
    _data_blocks = NULL;
    _dynopt_info = NULL;
    _next        = NULL;
#ifdef VM_STATS
    num_throws  = 0;
    num_catches = 0;
    num_unwind_java_frames_gc = 0;
    num_unwind_java_frames_non_gc = 0;
#endif
} //CodeChunkInfo::CodeChunkInfo


void CodeChunkInfo::initialize_code_chunk(CodeChunkInfo *chunk)
{
    memset(chunk, 0, sizeof(CodeChunkInfo));
    chunk->_callee_info = chunk->_static_callee_info;
    chunk->_max_callees = NUM_STATIC_CALLEE_ENTRIES;
    chunk->_relocatable = TRUE;
} //CodeChunkInfo::initialize_code_chunk



unsigned CodeChunkInfo::get_num_target_exception_handlers()
{
    if (_id==0) {
        return _num_target_exception_handlers;
    } else {
        return _method->get_num_target_exception_handlers(_jit);
    }
} //get_num_target_exception_handlers


Target_Exception_Handler_Ptr CodeChunkInfo::get_target_exception_handler_info(unsigned eh_num)
{
    if (_id==0) {
        return _target_exception_handlers[eh_num];
    } else {
        return _method->get_target_exception_handler_info(_jit, eh_num);
    }
} //get_target_exception_handler_info


// 20040224 Support for recording which methods (actually, CodeChunkInfo's) call which other methods.
void CodeChunkInfo::record_call_to_callee(CodeChunkInfo *callee, void *caller_ip)
{
    assert(callee);
    assert(caller_ip);

    // The weighted call graph is undirected, so we register the call on both the caller_method and callee's list of calls.
    // 20040422 No, the backedge isn't needed yet. Just record one direction now, and create the other direction later.

    // Acquire a lock to ensure that growing the callee array is safe.
    p_method_call_lock->_lock();                    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

    // Is the <callee, caller_ip> pair already in our list?
    unsigned i;
    for (i = 0;  i < _num_callees;  i++) { 
        Callee_Info *c = &(_callee_info[i]);
        if ((c->callee == callee) && (c->caller_ip == caller_ip)) {
            c->num_calls++;
            p_method_call_lock->_unlock();          // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            return;
        }
    }

    // Add a new Callee_Info entry to the array
    assert(_callee_info != NULL);
    assert(_num_callees <= _max_callees);
    if (_num_callees == _max_callees) {
        // grow the array of callee information
        unsigned old_max = _max_callees;
        unsigned new_max = (2 * _max_callees);
        Callee_Info *new_array = (Callee_Info *)STD_MALLOC(new_max * sizeof(Callee_Info));
        // Initialize the new array, with zeros at the end of the array
        memcpy(new_array, _callee_info, (old_max * sizeof(Callee_Info)));
        memset(&(new_array[old_max]), 0, (_max_callees * sizeof(Callee_Info)));
        if (_callee_info != _static_callee_info) {
            STD_FREE(_callee_info);
        }
        _callee_info = new_array;
        _max_callees = new_max;
        assert(_num_callees < _max_callees);
    }

    Callee_Info *c = &(_callee_info[_num_callees]);
    c->callee    = callee;
    c->caller_ip = caller_ip;
    c->num_calls = 1;
    _num_callees++;

    p_method_call_lock->_unlock();                  // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
} //CodeChunkInfo::record_call_to_callee


uint64 CodeChunkInfo::num_calls_to(CodeChunkInfo *other_chunk)
{
    assert(other_chunk);
    for (unsigned i = 0;  i < _num_callees;  i++) { 
        Callee_Info *info = &(_callee_info[i]);
        CodeChunkInfo *callee = info->callee;
        assert(callee);
        if (callee == other_chunk) {
            return info->num_calls;
        }
    }
    // the other chunk wasn't found on our callee list
    return 0;
} //CodeChunkInfo::num_calls_to


void CodeChunkInfo::print_name()
{
    Method *meth = get_method();
    assert(meth);
    const char* c = class_get_name(method_get_class(meth));
    const char* m = method_get_name(meth);
    const char* d = method_get_descriptor(meth); 
    printf("%d:%d:%s.%s%s", get_jit_index(), get_id(), c, m, d);
} //CodeChunkInfo::print_name


void CodeChunkInfo::print_name(FILE *file)
{
    Method *meth = get_method();
    assert(meth);
    const char* c = class_get_name(method_get_class(meth));
    const char* m = method_get_name(meth);
    const char* d = method_get_descriptor(meth); 
    fprintf(file, "%d:%d:%s.%s%s", get_jit_index(), get_id(), c, m, d);
} //CodeChunkInfo::print_name


void CodeChunkInfo::print_info(bool print_ellipses)
{
    size_t code_size = get_code_block_size();
    print_name();
    printf(", %d bytes%s\n", (unsigned)code_size, (print_ellipses? "..." : ""));
} //CodeChunkInfo::print_info


void CodeChunkInfo::print_callee_info()
{
    for (unsigned i = 0;  i < _num_callees;  i++) { 
        Callee_Info *info = &(_callee_info[i]);
        // Don't print the "back edges" (e.g., b calls a whenever a calls b) added to make the graph symmetric 
        if (info->caller_ip != NULL) { 
            CodeChunkInfo *callee = info->callee;
            assert(callee);
            unsigned call_offset = (unsigned)((char *)info->caller_ip - (char *)_code_block);
            assert(call_offset < _code_block_size);
            printf("%10" FMT64 "u calls at %u to ", info->num_calls, call_offset);
            callee->print_name();
            printf("\n");
        } 
    }
} //CodeChunkInfo::print_callee_info


// end CodeChunkInfo
////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////
// begin JIT management


JIT *jit_compilers[] = {0, 0, 0, 0, 0, 0, 0};

void vm_add_jit(JIT *jit)
{
    int max_jit_num = sizeof(jit_compilers) / sizeof(JIT *) - 2;
    if(jit_compilers[max_jit_num]) {
        ASSERT(0, "Can't add new JIT");
        return;
    }

    // Shift the jits
    for(int i = max_jit_num; i > 0; i--) {
        jit_compilers[i] = jit_compilers[i - 1];
    }

    jit_compilers[0] = jit;
    assert(jit_compilers[max_jit_num + 1] == 0);
} //vm_add_jit


void vm_delete_all_jits()
{
    JIT **jit;
    for(jit = jit_compilers; *jit; jit++) {
        delete (*jit);
        *jit = 0;
    }

} //vm_delete_all_jits


void vm_initialize_all_jits()
{
    JIT **jit;
    for(jit = jit_compilers; *jit; jit++) {
        (*jit)->jit_flags.insert_write_barriers = (gc_requires_barriers());
    }
} //vm_initialize_all_jits


int get_index_of_jit(JIT *jit)
{
    int idx;
    JIT **j;
    for (j = jit_compilers, idx = 0;  *j;  j++, idx++) {
        if (*j == jit) {
            return idx;
        }
    }
    return -999;
} //get_index_of_jit


// end JIT management
////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// JNI Stubs

static bool is_reference(Type_Info_Handle tih)
{
    return type_info_is_reference(tih) || type_info_is_vector(tih);
} //is_reference

// Implementation note: don't use l2 (use l3, l4 instead if required) since its
// space can be used in case of 64-bit return value.
NativeCodePtr compile_create_lil_jni_stub(Method_Handle method, void* func, NativeStubOverride nso)
{
    ASSERT_NO_INTERPRETER;
    const Class_Handle clss = method_get_class(method);
    bool is_static = (method_is_static(method) ? true : false);
    bool is_synchronised = (method_is_synchronized(method) ? true : false);
    Method_Signature_Handle msh = method_get_signature(method);
    unsigned num_args = method_args_get_number(msh);
    VM_Data_Type ret_type = type_info_get_type(method_ret_type_get_type_info(msh));
    unsigned i;

    unsigned num_ref_args = 0; // among original args, does not include jclass for static methods
    for(i=0; i<num_args; i++)
        if (is_reference(method_args_get_type_info(msh, i))) num_ref_args++;

    //***** Part 1: Entry, Stats, Override, push m2n, allocate space for handles
    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:%0m;",
                                          method);
    assert(cs);

    // Increment stats (total number of calls)
#ifdef VM_STATS
    cs = lil_parse_onto_end(cs,
                            "inc [%0i:pint];",
                            &((Method*)method)->num_accesses);
    assert(cs);
#endif //VM_STATS

    // Do stub override here
    if (nso) cs = nso(cs, method);
    assert(cs);

    // Increment stats (number of nonoverriden calls)
#ifdef VM_STATS
    cs = lil_parse_onto_end(cs,
                            "inc [%0i:pint];",
                            &((Method*)method)->num_slow_accesses);
    assert(cs);
#endif

    // Push M2nFrame
    cs = lil_parse_onto_end(cs, "push_m2n %0i, %1i, handles; locals 2;", method, FRAME_JNI);
    assert(cs);

    // Allocate space for handles
    unsigned number_of_object_handles = num_ref_args + (is_static ? 1 : 0);
    cs = oh_gen_allocate_handles(cs, number_of_object_handles, "l0", "l1");
    assert(cs);

    //***** Part 2: Initialise object handles

    if (is_static) {
        void *jlc = &((Class*)clss)->class_handle;
        cs = lil_parse_onto_end(cs,
                                "ld l1,[%0i:pint];"
                                "ld l1,[l1+0:ref];",
                                jlc);
        assert(cs);
        cs = oh_gen_init_handle(cs, "l0", 0, "l1", false);
        assert(cs);
    } else {
        cs = oh_gen_init_handle(cs, "l0", 0, "i0", true);
    }

    // The remaining handles are for the proper arguments (not including this)
    // Loop over the arguments, skipping 0th argument for instance methods. If argument is a reference, generate code
    unsigned hn = 1;
    for(i=(is_static?0:1); i<num_args; i++) {
        if (is_reference(method_args_get_type_info(msh, i))) {
            char buf[20];
            sprintf(buf, "i%d", i);
            cs = oh_gen_init_handle(cs, "l0", hn, buf, true);
            assert(cs);
            hn++;
        }
    }

    //***** Part 3: Synchronise
    if (is_synchronised) {
        if (is_static) {
            cs = lil_parse_onto_end(cs,
                                    "out managed:pint:void;"
                                    "o0=%0i;"
                                    "call %1i;",
                                    clss,
                                    lil_npc_to_fp(vm_get_rt_support_addr(VM_RT_MONITOR_ENTER_STATIC)));
            assert(cs);
        } else {
            cs = lil_parse_onto_end(cs,
                                    "out managed:ref:void;"
                                    "o0=i0;"
                                    "call %0i;",
                                    lil_npc_to_fp(vm_get_rt_support_addr(VM_RT_MONITOR_ENTER)));
            assert(cs);
        }
    }

    //***** Part 4: Enable GC
    cs = lil_parse_onto_end(cs,
                            "out platform::void;"
                            "call %0i;",
                            tmn_suspend_enable);
    assert(cs);

    //***** Part 5: Set up arguments

    // Setup outputs, set JNIEnv, set class/this handle
    cs = lil_parse_onto_end(cs,
                            "out jni:%0j;"
                            "o0=%1i; o1=l0+%2i;",
                            method, jni_native_intf, oh_get_handle_offset(0));
    assert(cs);

    // Loop over arguments proper, setting rest of outputs
    unsigned arg_base = 1 + (is_static ? 1 : 0);
    hn = 1;
    for(i=(is_static?0:1); i<num_args; i++) {
        if (is_reference(method_args_get_type_info(msh, i))) {
            unsigned handle_offset = oh_get_handle_offset(hn);
            if (VM_Global_State::loader_env->compress_references) {
                cs = lil_parse_onto_end(cs,
                                        "jc i%0i=%1i:ref,%n;"
                                        "o%2i=l0+%3i;"
                                        "j %o;"
                                        ":%g;"
                                        "o%4i=0;"
                                        ":%g;",
                                        i, Class::managed_null,
                                        arg_base+i, handle_offset, arg_base+i);
            } else {
                cs = lil_parse_onto_end(cs,
                                        "jc i%0i=0:ref,%n;"
                                        "o%1i=l0+%2i;"
                                        "j %o;"
                                        ":%g;"
                                        "o%3i=0;"
                                        ":%g;",
                                        i, arg_base+i, handle_offset,
                                        arg_base+i);
            }
            hn++;
        } else {
            cs = lil_parse_onto_end(cs, "o%0i=i%1i;", arg_base+i, i);
        }
        assert(cs);
    }

    //***** Part 6: Call
    cs = lil_parse_onto_end(cs,
                            "call %0i;",
                            func);
    assert(cs);

    //***** Part 7: Save return, widening if necessary
    switch (ret_type) {
    case VM_DATA_TYPE_VOID:
        break;
    case VM_DATA_TYPE_INT32:
        cs = lil_parse_onto_end(cs, "l1=r;");
        break;
    case VM_DATA_TYPE_BOOLEAN:
        cs = lil_parse_onto_end(cs, "l1=zx1 r;");
        break;
    case VM_DATA_TYPE_INT16:
        cs = lil_parse_onto_end(cs, "l1=sx2 r;");
        break;
    case VM_DATA_TYPE_INT8:
        cs = lil_parse_onto_end(cs, "l1=sx1 r;");
        break;
    case VM_DATA_TYPE_CHAR:
        cs = lil_parse_onto_end(cs, "l1=zx2 r;");
        break;
    default:
        cs = lil_parse_onto_end(cs, "l1=r;");
        break;
    }
    assert(cs);

    //***** Part 8: Disable GC
    cs = lil_parse_onto_end(cs,
                            "out platform::void;"
                            "call %0i;",
                            tmn_suspend_disable);
    assert(cs);

    //***** Part 9: Synchronise
    if (is_synchronised) {
        if (is_static) {
            cs = lil_parse_onto_end(cs,
                "out managed:pint:void;"
                "o0=%0i;"
                "call %1i;",
                clss,
                lil_npc_to_fp(vm_get_rt_support_addr(VM_RT_MONITOR_EXIT_STATIC)));
        } else {
            cs = lil_parse_onto_end(cs,
                "ld i0,[l0+%0i:ref];"
                "out managed:ref:void; o0=i0; call %1i;",
                oh_get_handle_offset(0),
                lil_npc_to_fp(vm_get_rt_support_addr(VM_RT_MONITOR_EXIT)));
        }
        assert(cs);
    }

    //***** Part 10: Unhandle the return if it is a reference
    if (is_reference(method_ret_type_get_type_info(msh))) {
        cs = lil_parse_onto_end(cs,
                                "jc l1=0,ret_done;"
                                "ld l1,[l1+0:ref];"
                                ":ret_done;");
        if (VM_Global_State::loader_env->compress_references) {
            cs = lil_parse_onto_end(cs,
                                    "jc l1!=0,done_translating_ret;"
                                    "l1=%0i:ref;"
                                    ":done_translating_ret;",
                                    Class::managed_null);
        }
        assert(cs);
    }

    //***** Part 11: Rethrow exception
    unsigned eoo = (unsigned)(POINTER_SIZE_INT)&((VM_thread*)0)->p_exception_object;
    cs = lil_parse_onto_end(cs,
                            "l0=ts;"
                            "ld l0,[l0+%0i:ref];"
                            "jc l0=0,no_exn;"
                            "m2n_save_all;"
                            "out platform::void;"
                            "call.noret %1i;"
                            ":no_exn;",
                            eoo, rethrow_current_thread_exception);
    assert(cs);

    //***** Part 12: Restore return variable, pop_m2n, return
    if (ret_type != VM_DATA_TYPE_VOID) {
        cs = lil_parse_onto_end(cs, "r=l1;");
        assert(cs);
    }
    cs = lil_parse_onto_end(cs,
                            "pop_m2n;"
                            "ret;");
    assert(cs);

    //***** Now generate code

    assert(lil_is_valid(cs));

#ifndef NDEBUG
    NativeCodePtr addr = LilCodeGenerator::get_platform()->compile(cs, "jni_stub", dump_stubs);
#else
    NativeCodePtr addr = LilCodeGenerator::get_platform()->compile(cs, "", false);
#endif

#ifdef VM_STATS
    vm_stats_total.jni_stub_bytes += lil_cs_get_code_size(cs);
#endif

    lil_free_code_stub(cs);
    return addr;
} // compile_create_lil_jni_stub


//////////////////////////////////////////////////////////////////////////
// PInvoke Stubs



/////////////////////////////////////////////////////////////////
// begin Support for stub override code sequences 


static int get_override_index(Method *method)
{
    const char *clss_name = &(method->get_class()->name->bytes[0]);
    const char *meth_name = method->get_name()->bytes;
    const char *meth_desc = method->get_descriptor()->bytes;
    for (int i = 0;  i < sizeof_stub_override_entries;  i++) {
        if ((strcmp(clss_name, stub_override_entries[i].class_name)  == 0) &&
            (strcmp(meth_name, stub_override_entries[i].method_name) == 0) &&
            (strcmp(meth_desc, stub_override_entries[i].descriptor)  == 0)) {
            return i;
        }
    }
    return -1;
}

bool needs_override(Method *method) {
    int idx = get_override_index(method);
    if (idx < 0)
        return false;
    Override_Generator *override_generator = stub_override_entries[idx].override_generator;
    return (override_generator != NULL);
}    

// end Support for stub override code sequences
/////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// Direct Call Support
// 20040113: This is deprecated and will go away soon

#define REWRITE_INVALID_TYPE                        0  
#define REWRITE_PATCH_COMPILE_ME_STUB               1
#define REWRITE_PATCH_CALLER                        2
#define REWRITE_PATCH_OLD_CODE_TO_REWRITER          3

//////////////////////////////////////////////////////////////////////////
// Compilation of Methods

static NativeCodePtr compile_create_jni_stub(Method_Handle method, GenericFunctionPointer func, NativeStubOverride nso)
{
    return compile_create_lil_jni_stub(method, (void*)func, nso);
} //compile_create_jni_stub

static JIT_Result compile_prepare_native_method(Method* method, JIT_Flags flags)
{
    TRACE2("compile", "compile_prepare_native_method(" << method_get_name(method) << ")");
#ifdef VM_STATS
    vm_stats_total.num_native_methods++;
#endif
    assert(method->is_native());

    GenericFunctionPointer func = classloader_find_native(method);

    if (!func)
        return JIT_FAILURE;

    Class* cl = method->get_class();

    NativeStubOverride nso = nso_find_method_override(VM_Global_State::loader_env,
                                                    cl->name, method->get_name(),
                                                    method->get_descriptor());

    NativeCodePtr stub = compile_create_jni_stub(method, func, nso);

    if (!stub)
        return JIT_FAILURE;

    cl->m_lock->_lock();
    method->set_code_addr(stub);
    cl->m_lock->_unlock();

    return JIT_SUCCESS;
} //compile_prepare_native_method


JIT_Result compile_do_compilation_jit(Method* method, JIT* jit)
{
    assert(method);
    assert(jit);

    if (!parallel_jit) {
        p_jit_a_method_lock->_lock();
        // MikhailF reports that each JIT in recompilation chain has its own
        // JIT* pointer.
        // If in addition to recompilation chains one adds recompilation loops,
        // this check can be skipped, or main_code_chunk_id should be
        // modified.

        if (NULL != method->get_chunk_info_no_create_mt(jit, CodeChunkInfo::main_code_chunk_id)) {
            p_jit_a_method_lock->_unlock();
            return JIT_SUCCESS;
        }
    }

    OpenMethodExecutionParams flags = {0}; 
    flags.exe_insert_write_barriers = gc_requires_barriers();
    Compilation_Handle ch;
    ch.env = VM_Global_State::loader_env;
    ch.jit = jit;

    JIT_Result res = jit->compile_method_with_params(&ch, method, flags);

    if (JIT_SUCCESS != res) {
        if (!parallel_jit) {
            p_jit_a_method_lock->_unlock();
        }
        return res;
    }

    method->lock();
    for (CodeChunkInfo* cci = method->get_first_JIT_specific_info();  cci;  cci = cci->_next) {
        if (cci->get_jit() == jit) {
            compile_flush_generated_code_block((Byte*)cci->get_code_block_addr(), cci->get_code_block_size());
            // We assume the main chunk starts from entry point
            if (cci->get_id() == CodeChunkInfo::main_code_chunk_id) {
                method->set_code_addr(cci->get_code_block_addr());
            }
        }
    }

    // Commit the compilation by setting the method's code address
    method->set_state(Method::ST_Compiled);
    method->do_jit_recompiled_method_callbacks();
    method->apply_vtable_patches();
    method->unlock();
    if (!parallel_jit) {
        p_jit_a_method_lock->_unlock();
    }

    // Call TI callbacks
    DebugUtilsTI *ti = VM_Global_State::loader_env->TI;
    if (ti->isEnabled() && ti->getPhase() == JVMTI_PHASE_LIVE) {
        jvmti_send_compiled_method_load_event(method);
    }
    return JIT_SUCCESS;
}


// Assumes that GC is disabled, but a GC-safe point
static JIT_Result compile_do_compilation(Method* method, JIT_Flags flags)
{
    assert(tmn_is_suspend_enabled());
    tmn_suspend_disable();
    class_initialize_ex(method->get_class(), false);
    tmn_suspend_enable();
    
    if (method->get_state() == Method::ST_Compiled) {
        return JIT_SUCCESS;
    } else if (method->get_state()==Method::ST_NotCompiled && exn_raised()) {
        return JIT_FAILURE;
    }

    JIT_Result res = JIT_FAILURE;
    if (method->is_native()) {
        method->lock();
        if (method->get_state() == Method::ST_NotCompiled) {
            method->set_state(Method::ST_BeingCompiled);
            res = compile_prepare_native_method(method, flags);
            compile_flush_generated_code();
            if (res == JIT_SUCCESS) {
                method->set_state(Method::ST_Compiled);
                method->do_jit_recompiled_method_callbacks();
                method->apply_vtable_patches();
            }
        } else {
            res = JIT_SUCCESS;
        }
        method->unlock();
    } else {
        // Call an execution manager to compile the method.
        // An execution manager is safe to call from multiple threads.
        res = VM_Global_State::loader_env->em_interface->CompileMethod(method);
    }
    return res;
}

// Make a suitable exception to throw if compilation fails.
// We try to create the named exception with a message if this is possible,
// otherwise we create with default constructor.
// Then we try to set the cause of the exception to the current thread exception if there is one.
// In all cases we ignore any further sources of exceptions and try to proceed anyway.
static ManagedObject* compile_make_exception(const char* name, Method* method)
{ // FIXME: prototype should be changed to getrid of managed objects as parameters.
  // Now it works in gc disabled mode because of prototype.
    assert(!tmn_is_suspend_enabled());
    ObjectHandle old_exn = oh_allocate_local_handle();
    old_exn->object = get_current_thread_exception();
    clear_current_thread_exception();
    Global_Env* env = VM_Global_State::loader_env;
    String* exc_str = env->string_pool.lookup(name);

    tmn_suspend_enable();
    Class *exc_clss = env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, exc_str);
    assert(exc_clss);
    Method* constr = class_lookup_method(exc_clss, env->Init_String, env->FromStringConstructorDescriptor_String);
    tmn_suspend_disable();

    jvalue args[2];
    ObjectHandle msg = oh_allocate_local_handle();
    ObjectHandle res = oh_allocate_local_handle();

    if (constr) {
        const char* c = method->get_class()->name->bytes;
        const char* m = method->get_name()->bytes;
        const char* d = method->get_descriptor()->bytes;
        size_t sz = 25+strlen(c)+strlen(m)+strlen(d);
        char* msg_raw = (char*)STD_MALLOC(sz);
        assert(msg_raw);
        sprintf(msg_raw, "Error compiling method %s.%s%s", c, m, d);
        assert(strlen(msg_raw) < sz);
        
        msg->object = string_create_from_utf8(msg_raw, (unsigned)strlen(msg_raw));
        assert(msg->object);
        STD_FREE(msg_raw);
        res->object = class_alloc_new_object(exc_clss);
        assert(res->object);
        args[0].l = (jobject) res;
        args[1].l = (jobject) msg;
        vm_execute_java_method_array((jmethodID)constr, 0, args);
    } else {
        res->object = class_alloc_new_object_and_run_default_constructor(exc_clss);
        assert(res->object);
    }
    // Ignore any exceptions from constructor
    clear_current_thread_exception();
    if (old_exn->object) {
        Method* init = class_lookup_method_recursive(exc_clss, env->InitCause_String, env->InitCauseDescriptor_String);
        if (init) {
            args[0].l = res;
            args[1].l = old_exn;
            vm_execute_java_method_array((jmethodID) init, 0, args);
        }
        // Ignore any exceptions from setting cause
        clear_current_thread_exception();
    }
    return res->object;
}

NativeCodePtr compile_jit_a_method(Method* method)
{
    { // Start of block with GcFrames
    int tmn_suspend_disable_count();   
    TRACE2("compile", "compile_jit_a_method " << method << " sus_copunt:" << tmn_suspend_disable_count());
    assert(tmn_suspend_disable_count()==1);
 
    ASSERT_NO_INTERPRETER;

    GcFrame gc;
    assert(&gc == p_TLS_vmthread->gc_frames); 
    compile_protect_arguments(method, &gc);
    assert(&gc == p_TLS_vmthread->gc_frames); 
    
    JIT_Flags flags;
    flags.insert_write_barriers = (gc_requires_barriers());
    assert(&gc == p_TLS_vmthread->gc_frames);
    tmn_suspend_enable(); 
    JIT_Result res = compile_do_compilation(method, flags);
    tmn_suspend_disable();

    if (res == JIT_SUCCESS) {
        assert(&gc == p_TLS_vmthread->gc_frames); 
        NativeCodePtr entry_point = method->get_code_addr();
        assert(&gc == p_TLS_vmthread->gc_frames); 
        INFO2("compile.code", "Compiled method " << method
                << ", entry " << method->get_code_addr());
        return entry_point;
    }

    assert(!tmn_is_suspend_enabled());
    
    INFO2("compile", "Could not compile " << method);
    
    const char* exn_class;
    ManagedObject *cause = get_current_thread_exception();
    if (!cause) {
        if (method->is_native()) {
            method->set_state(Method::ST_NotCompiled);
            exn_class = "java/lang/UnsatisfiedLinkError";
        } else {
            exn_class = "java/lang/InternalError";
        }
        ManagedObject* exn = compile_make_exception(exn_class, method);
        set_current_thread_exception(exn);
    }
    } // GcFrames destroyed here, should be before rethrowing exception
    rethrow_current_thread_exception();
    ASSERT(0, "Control flow should never reach this point");
    return NULL;
} //compile_jit_a_method

//////////////////////////////////////////////////////////////////////////
// Instrumentation Stubs

// 20040218 Interpose on calls to the specified method by first calling the specified 
// instrumentation procedure. That procedure must not allocate memory from the collected heap
// or throw exceptions (no m_to_n frame is pushed).
NativeCodePtr compile_do_instrumentation(CodeChunkInfo *callee,
                                         MethodInstrumentationProc instr_proc)
{
    assert(callee);
    NativeCodePtr callee_addr = callee->get_code_block_addr();
    assert(callee_addr);
    LilCodeStub *cs = lil_parse_code_stub(
        "entry 0:managed:arbitrary;"
        "push_m2n 0, 0;"
        "out platform:pint:void;"
        "o0=%0i;"
        "call %1i;"       // call instrumentation procedure
        "pop_m2n;"
        "tailcall %2i;",  // call original entry point
        callee, instr_proc, lil_npc_to_fp(callee_addr));
    assert(cs && lil_is_valid(cs));
    // TODO: 2 & 3 parameters should be removed from the method signature
    // since it makes sense for debugging only
#ifndef NDEBUG
    NativeCodePtr addr = LilCodeGenerator::get_platform()->compile(cs,
        "compile_do_instrumentation", dump_stubs);
#else
    NativeCodePtr addr = LilCodeGenerator::get_platform()->compile(cs,
        "compile_do_instrumentation", false);
#endif
    lil_free_code_stub(cs);
    return addr;
} //compile_do_instrumentation


// A MethodInstrumentationProc that records calls in a weighted method call graph, where each arc between 
// a pair of methods has a weight equal to the call counts between those methods.
void count_method_calls(CodeChunkInfo *callee)
{
    assert(callee);
    // Get the most recent M2nFrame of the given thread
    M2nFrame *caller_frame = m2n_get_last_frame();
    assert(caller_frame);
    // Get the return address from the preceeding managed frame
    NativeCodePtr ret_ip = m2n_get_ip(caller_frame);
    assert(ret_ip);

    // Record call from caller to the callee.
    CodeChunkInfo *caller = vm_methods->find(ret_ip);
    if (caller != NULL) {
        caller->record_call_to_callee(callee, ret_ip);
    }
} //count_method_calls


//////////////////////////////////////////////////////////////////////////
