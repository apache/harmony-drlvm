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
 * @author Intel, Evgueni Brevnov, Ivan Volosyuk
 * @version $Revision: 1.1.2.5.4.4 $
 */


#define LOG_DOMAIN "vm.helpers"
#include "cxxlog.h"


#include <assert.h>
#include <float.h>

//MVM
#include <iostream>

using namespace std;

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"

#include "Class.h"
#include "environment.h"
#include "exceptions.h"
#include "open/gc.h"
#include "ini.h"
#include "jit_runtime_support.h"
#include "jit_runtime_support_common.h"
#include "jni_utils.h"
#include "lil.h"
#include "lil_code_generator.h"
#include "m2n.h"
#include "method_lookup.h"
#include "object_layout.h"
#include "object_handles.h"
#include "vm_arrays.h"
#include "vm_stats.h"
#include "vm_strings.h"
#include "vm_synch.h"
#include "vm_threads.h"
#include "open/types.h"
#include "open/bytecodes.h"
#include "open/vm_util.h"
#include "open/thread.h"
#include "jvmti_interface.h"

#include "compile.h"

// TODO: FIXME: We should not include internal stuff
#ifdef _IPF_
#include "../m2n_ipf_internal.h"
#elif defined _EM64T_
#include "../m2n_em64t_internal.h"
#else
#include "../m2n_ia32_internal.h"
#endif

extern bool dump_stubs;

//////////////////////////////////////////////////////////////////////////
// Object Creation

///////////////////////////////////////////////////////////
// New Object and New Array

// Create a multidimensional array
// The M2nFrame arguments holds the number of dimensions and lengths for each one
// Returns -1 if there is a negative size
static Vector_Handle rth_multianewarrayhelper()
{
#ifdef VM_STATS
    vm_stats_total.num_multianewarray++;  
#endif

#ifdef _EM64T_
    return (Vector_Handle)NULL;
#else
    M2nFrame* m2nf = m2n_get_last_frame();
    const unsigned max_dim = 255;
    int lens[max_dim];

#ifdef _IPF_
    Class* c = (Class*)*m2n_get_arg_word(m2nf, 0);
    unsigned dims = (unsigned)(*m2n_get_arg_word(m2nf, 1) & 0xFFFFffff);
    assert(dims<=max_dim);
    for(unsigned i=0; i<dims; i++) {
        int len = (int)(*m2n_get_arg_word(m2nf, i+2) & 0xFFFFffff);
        if (len<0) return (Vector_Handle)-1;
        lens[dims-i-1] = (int)len;
    }
#else
    // FIXME: em64t not tested
    POINTER_SIZE_INT* p_args = m2n_get_args(m2nf);
    unsigned dims = p_args[1];
    assert(dims<=max_dim);
    uint32* lens_base = (uint32*)(p_args+2);
    for(unsigned i=0; i<dims; i++) {
        int len = lens_base[dims-i-1];
        if (len<0) return (Vector_Handle)-1;
        lens[i] = len;
    }
    Class* c = (Class*)p_args[0];
#endif
    return vm_multianewarray_recursive(c, lens, dims);
#endif // _EM64T_
}

// Multianewarray helper
static NativeCodePtr rth_get_lil_multianewarray(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed::ref;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
         cs = lil_parse_onto_end(cs,
            "push_m2n 0, 0;"
            "out platform::ref;"
            "call %0i;"
            "pop_m2n;"
            "jc r=%1i:ref,negsize;"
            "ret;"
            ":negsize;"
            "tailcall %2i;",
            rth_multianewarrayhelper, (POINTER_SIZE_INT)-1, lil_npc_to_fp(exn_get_rth_throw_negative_array_size()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_multianewarray", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

///////////////////////////////////////////////////////////
// Load Constant String

static NativeCodePtr rth_get_lil_ldc_string(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        ManagedObject* (*p_instantiate_string)(Class*,unsigned) = vm_instantiate_cp_string_slow;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:g4,pint:ref;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "push_m2n 0, 0;"
            "out platform:pint,g4:ref;"
            "o0=i1;"
            "o1=i0;"
            "call %0i;"
            "pop_m2n;"
            "ret;",
            p_instantiate_string);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_ldc_string", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

///////////////////////////////////////////////////////////
// Checkcast and Instanceof

// The generic type test sequence first checks for whether the fast scheme
// can be used, if so it uses it, otherwise it calls into the VM to do the test.
// This is parameterised by:
//   * Is there a null test, if so is null in the type or not
//   * Should the test outcome be returned or should an exception be thrown on failure, object returned on success
//   * Is the type known
//   * Is the sequence for a stub or for inlining (affects how exceptions are delt with)
//   * Stats stuff: dyn_count for the dynamic call counter, and a stats update function with prototype void (*)(ManagedObject*, Class*)
// The object is in i0, and class in i1 (even if fixed unless there is no stats updater function)

enum RthTypeTestNull { RTTN_NoNullCheck, RTTN_NullMember, RTTN_NullNotMember };
enum RthTypeTestResult { RTTR_ReturnOutcome, RTTR_ThrowStub, RTTR_ThrowInline };
typedef void (*RthTypeTestStatsUpdate)(ManagedObject*, Class*);

static LilCodeStub* rth_gen_lil_type_test(LilCodeStub* cs, RthTypeTestNull null, RthTypeTestResult res, Class* type,
                                          int* dyn_count, RthTypeTestStatsUpdate update_fn)
{
    // We need to get the vtable in more than one place below
    // Here are some macros to helper smooth over the compressed vtables
    const POINTER_SIZE_INT vtable_off = object_get_vtable_offset();
    const POINTER_SIZE_INT vtable_add = vm_vtable_pointers_are_compressed() ? vm_get_vtable_base() : 0;
    // Update Stats
#ifdef VM_STATS
    assert(dyn_count);
    cs = lil_parse_onto_end(cs, "inc [%0i:pint]; in2out platform:void; call %1i;", dyn_count, update_fn);
    if (!cs) return NULL;
#endif

    // Setup locals
    cs = lil_parse_onto_end(cs, (type ? "locals 1;" : "locals 2;"));
    if (!cs) return NULL;

    // Null check
    if (null!=RTTN_NoNullCheck) {
        cs = lil_parse_onto_end(cs, (null==RTTN_NullMember ? "jc i0=%0i:ref,success;" : "jc i0=%0i:ref,failed;"), Class::managed_null);
        if (!cs) return NULL;
    }

    // Fast sequence
    const POINTER_SIZE_INT is_fast_off = (POINTER_SIZE_INT)&((Class*)NULL)->is_suitable_for_fast_instanceof;
    const POINTER_SIZE_INT depth_off = (POINTER_SIZE_INT)&((Class*)NULL)->depth;
    const POINTER_SIZE_INT supertable_off = (POINTER_SIZE_INT)&((VTable*)NULL)->superclasses;
    bool do_slow = true;
    bool success_before = false;
    if (type) {
        if (type->is_suitable_for_fast_instanceof) {
            cs = lil_parse_onto_end(cs,
                vm_vtable_pointers_are_compressed() ? "ld l0,[i0+%0i:g4],zx;" : "ld l0,[i0+%0i:pint];",
                vtable_off);
            if (!cs) return NULL;
            cs = lil_parse_onto_end(cs,
                "ld l0,[l0+%1i:pint];"
                "jc l0!=%2i,failed;",
                vtable_add+supertable_off+sizeof(Class*)*(type->depth-1), type);
            if (!cs) return NULL;
            do_slow = false;
        }
    } else {
        cs = lil_parse_onto_end(cs,
            "ld l0,[i1+%0i:g4];"
            "jc l0=0,slowpath;"
            "ld l1,[i1+%1i:g4],zx;",
            is_fast_off, depth_off);
        if (!cs) return NULL;
        cs = lil_parse_onto_end(cs,
            vm_vtable_pointers_are_compressed() ? "ld l0,[i0+%0i:g4],zx;" : "ld l0,[i0+%0i:pint];",
            vtable_off);
        if (!cs) return NULL;
        cs = lil_parse_onto_end(cs,
            "ld l0,[l0+%0i*l1+%1i:pint];"
            "jc l0!=i1,failed;",
            sizeof(Class*), vtable_add+supertable_off-sizeof(Class*));  // -4/8 because we want to index with depth-1
        if (!cs) return NULL;
        success_before = true;
    }

    //*** Success, if before slowpath
    if (success_before) {
        cs = lil_parse_onto_end(cs, (res==RTTR_ReturnOutcome ? ":success; r=1:g4; ret;" : ":success; r=i0; ret;"));
        if (!cs) return NULL;
    }

    //*** Slow sequence
    const POINTER_SIZE_INT clss_off = (POINTER_SIZE_INT)&((VTable*)NULL)->clss;
    Boolean (*p_subclass)(Class_Handle, Class_Handle) = class_is_subclass;
    if (do_slow) {
        cs = lil_parse_onto_end(cs,
            ":slowpath;"
            "out platform:pint,pint:g4;");
        if (!cs) return NULL;
        cs = lil_parse_onto_end(cs,
            vm_vtable_pointers_are_compressed() ? "ld l0,[i0+%0i:g4],zx;" : "ld l0,[i0+%0i:pint];",
            vtable_off);
        if (!cs) return NULL;
        cs = lil_parse_onto_end(cs,
            "ld o0,[l0+%0i:pint];"
            "o1=i1;"
            "call %1i;",
            vtable_add+clss_off, p_subclass);
        if (!cs) return NULL;
        cs = lil_parse_onto_end(cs, (res==RTTR_ReturnOutcome ? "ret;" : success_before ? "jc r!=0,success;" : "jc r=0,failed;"));
        if (!cs) return NULL;
    }

    //*** Success, if after slowpath
    if (!success_before) {
        cs = lil_parse_onto_end(cs, (res==RTTR_ReturnOutcome ? ":success; r=1:g4; ret;" : ":success; r=i0; ret;"));
        if (!cs) return NULL;
    }

    //*** Failure
    if (res==RTTR_ReturnOutcome) {
        cs = lil_parse_onto_end(cs, ":failed; r=0:g4; ret;");
    } else {
        GenericFunctionPointer thrw = lil_npc_to_fp(exn_get_rth_throw_class_cast_exception());
        cs = lil_parse_onto_end(cs, (res==RTTR_ThrowStub ? ":failed; tailcall %0i;" : ":failed; call.noret %0i;"), thrw);
    }
    if (!cs) return NULL;

    return cs;
}

// General stats update
#ifdef VM_STATS
static void rth_type_test_update_stats(VTable* sub, Class* super)
{
    vm_stats_total.num_type_checks ++;
    if (sub->clss == super)
        vm_stats_total.num_type_checks_equal_type ++;
    if (super->is_suitable_for_fast_instanceof)
        vm_stats_total.num_type_checks_fast_decision ++;
    else if (super->is_array)
        vm_stats_total.num_type_checks_super_is_array ++;
    else if (class_is_interface(super))
        vm_stats_total.num_type_checks_super_is_interface ++;
    else if (super->depth >= vm_max_fast_instanceof_depth())
        vm_stats_total.num_type_checks_super_is_too_deep ++;
}
#endif

// Checkcast stats update
static void rth_update_checkcast_stats(ManagedObject* o, Class* super)
{
#ifdef VM_STATS
    vm_stats_total.num_checkcast ++;
    if (o == (ManagedObject*)Class::managed_null) {
        vm_stats_total.num_checkcast_null++;
    } else {
        if (o->vt()->clss == super)
            vm_stats_total.num_checkcast_equal_type ++;
        if (super->is_suitable_for_fast_instanceof)
            vm_stats_total.num_checkcast_fast_decision ++;
        rth_type_test_update_stats(o->vt(), super);
    }
#endif // VM_STATS
}

// Checkcast helper
static NativeCodePtr rth_get_lil_checkcast(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:ref,pint:ref;");
        cs = rth_gen_lil_type_test(cs, RTTN_NullMember, RTTR_ThrowStub, NULL, dyn_count, rth_update_checkcast_stats);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_checkcast", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

// Instanceof stats update
static void rth_update_instanceof_stats(ManagedObject* o, Class* super)
{
#ifdef VM_STATS
    vm_stats_total.num_instanceof++;
    super->num_instanceof_slow++;
    if (o == (ManagedObject*)Class::managed_null) {
        vm_stats_total.num_instanceof_null++;
    } else {
        if (o->vt()->clss == super)
            vm_stats_total.num_instanceof_equal_type ++;
        if (super->is_suitable_for_fast_instanceof)
            vm_stats_total.num_instanceof_fast_decision ++;
        rth_type_test_update_stats(o->vt(), super);
    }
#endif // VM_STATS
}

// Instanceof Helper
static NativeCodePtr rth_get_lil_instanceof(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:ref,pint:g4;");
        cs = rth_gen_lil_type_test(cs, RTTN_NullNotMember, RTTR_ReturnOutcome, NULL, dyn_count, rth_update_instanceof_stats);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_instanceof", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

///////////////////////////////////////////////////////////
// Array Stores

// Store a reference into an array at a given index and return NULL,
// or return the Class* for the exception to throw.
static Class* rth_aastore(ManagedObject* elem, int idx, Vector_Handle array)
{
#ifdef VM_STATS
    vm_stats_total.num_aastore ++;
#endif // VM_STATS

    Global_Env *env = VM_Global_State::loader_env;
    ManagedObject* null_ref = (ManagedObject*)Class::managed_null;
    if (array == null_ref) {
        return env->java_lang_NullPointerException_Class;
    } else if (((uint32)idx) >= (uint32)get_vector_length(array)) {
        return env->java_lang_ArrayIndexOutOfBoundsException_Class;
    } else if (elem != null_ref) {
        assert(get_vector_vtable(array));
        Class* array_class = get_vector_vtable(array)->clss;
        assert(array_class);
        assert(array_class->is_array);
#ifdef VM_STATS
        VTable* vt = get_vector_vtable(array);
        if (vt == cached_object_array_vtable_ptr)
            vm_stats_total.num_aastore_test_object_array++;
        if (elem->vt()->clss == array_class->array_element_class)
            vm_stats_total.num_aastore_equal_type ++;
        if (array_class->array_element_class->is_suitable_for_fast_instanceof)
            vm_stats_total.num_aastore_fast_decision ++;
#endif // VM_STATS
        if (class_is_subtype_fast(elem->vt(), array_class->array_element_class)) {
            STORE_REFERENCE((ManagedObject*)array, get_vector_element_address_ref(array, idx), (ManagedObject*)elem);
        } else {
            return env->java_lang_ArrayStoreException_Class;
        }
    } else {
        // elem is null. We don't have to check types for a null reference. We also don't have to record stores of null references.
#ifdef VM_STATS
        vm_stats_total.num_aastore_null ++;
#endif // VM_STATS
        if (VM_Global_State::loader_env->compress_references) {
            COMPRESSED_REFERENCE* elem_ptr = (COMPRESSED_REFERENCE*)get_vector_element_address_ref(array, idx);
            *elem_ptr = (COMPRESSED_REFERENCE)NULL;
        } else {
            ManagedObject** elem_ptr = get_vector_element_address_ref(array, idx);
            *elem_ptr = (ManagedObject*)NULL;
        }
    }
    return NULL;
} //rth_aastore

static NativeCodePtr rth_get_lil_aastore(int * dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        Class* (*p_aastore)(ManagedObject*, int, Vector_Handle) = rth_aastore;
        // The args are the element ref to store, the index, and the array to store into\n"
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:ref,pint,ref:void;");
#ifdef VM_STATS
        assert(dyn_count);
        cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
        assert(cs);
#endif
        cs = lil_parse_onto_end(cs,
            "in2out platform:pint;"
            // rth_aastore either returns NULL or the ClassHandle of an exception to throw \n
            "call %0i;"
            "jc r!=0,aastore_failed;"
            "ret;"
            ":aastore_failed;"
            "std_places 1;"
            "sp0=r;"
            "tailcall %1i;",
            p_aastore,
            lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_aastore", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static bool rth_aastore_test(ManagedObject* elem, Vector_Handle array)
{
#ifdef VM_STATS
    vm_stats_total.num_aastore_test++;
#endif // VM_STATS

    ManagedObject* null_ref = (ManagedObject*)Class::managed_null;
    if (array == null_ref) {
        return false;
    }
    if (elem == null_ref) {
#ifdef VM_STATS
        vm_stats_total.num_aastore_test_null++;
#endif // VM_STATS
        return true;
    }

    VTable* vt = get_vector_vtable(array);
    if (vt == cached_object_array_vtable_ptr) {
#ifdef VM_STATS
        vm_stats_total.num_aastore_test_object_array++;
#endif // VM_STATS
        return true;
    }

    Class* array_class = vt->clss;
    assert(array_class);
    assert(array_class->is_array);

#ifdef VM_STATS
    if (elem->vt()->clss == array_class->array_element_class)
        vm_stats_total.num_aastore_test_equal_type ++;
    if (array_class->array_element_class->is_suitable_for_fast_instanceof)
        vm_stats_total.num_aastore_test_fast_decision ++;
#endif // VM_STATS
    return (class_is_subtype_fast(elem->vt(), array_class->array_element_class) ? true : false);
} //rth_aastore_test

static NativeCodePtr rth_get_lil_aastore_test(int * dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        bool (*p_aastore_test)(ManagedObject*, Vector_Handle) = rth_aastore_test;
        // The args are the element ref to store and the array to store into\n
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:ref,ref:void;");
        assert(cs);
#ifdef VM_STATS
        assert(dyn_count);
        cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
        assert(cs);
#endif
        cs = lil_parse_onto_end(cs,
            "in2out platform:pint;"
            "call %0i;"
            "ret;",
            p_aastore_test);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_aastore_test", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////////
// Misc

///////////////////////////////////////////////////////////
// Throw linking exception helper

static NativeCodePtr rth_get_lil_throw_linking_exception(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        void (*p_throw_linking_error)(Class_Handle ch, unsigned index, unsigned opcode) =
            class_throw_linking_error;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:pint,g4,g4:void;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "push_m2n 0, 0;"
            "m2n_save_all;"
            "in2out platform:void;"
            "call.noret %0i;",
            p_throw_linking_error);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_throw_linking_exception", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
} // rth_get_lil_throw_linking_exception

///////////////////////////////////////////////////////////
// Get interface vtable

// Return the interface vtable for interface iid within object obj
// Or NULL if no such interface exists for obj
static void* rth_get_interface_vtable(ManagedObject* obj, Class* iid)
{
    assert(obj && obj->vt());
    Class* clss = obj->vt()->clss;
    assert(clss);
    unsigned num_intfc = clss->n_intfc_table_entries;
#ifdef VM_STATS
    vm_stats_total.num_invokeinterface_calls++;
    switch(num_intfc) {
    case 1:  vm_stats_total.num_invokeinterface_calls_size_1++;    break;
    case 2:  vm_stats_total.num_invokeinterface_calls_size_2++;    break;
    default: vm_stats_total.num_invokeinterface_calls_size_many++; break;
    }
    if(num_intfc > vm_stats_total.invokeinterface_calls_size_max)
        vm_stats_total.invokeinterface_calls_size_max = num_intfc;
#endif
    for(unsigned i = 0; i < num_intfc; i++) {
        Class* intfc = clss->intfc_table_descriptors[i];
        if(intfc == iid) {
#ifdef VM_STATS
            switch(i) {
            case 0:  vm_stats_total.num_invokeinterface_calls_searched_1++;    break;
            case 1:  vm_stats_total.num_invokeinterface_calls_searched_2++;    break;
            default: vm_stats_total.num_invokeinterface_calls_searched_many++; break;
            }
            if(i > vm_stats_total.invokeinterface_calls_searched_max)
                vm_stats_total.invokeinterface_calls_searched_max = i;
#endif
            unsigned char **table = clss->vtable->intfc_table->entry[i].table;
            return (void *)table;
        }
    }
    return NULL;
}

// Get interface vtable helper
static NativeCodePtr rth_get_lil_get_interface_vtable(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        void* (*p_get_ivtable)(ManagedObject*, Class*) = rth_get_interface_vtable;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:ref,pint:pint;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "jc i0=%0i:ref,null;"
            "in2out platform:pint;"
            "call %1i;"
            "jc r=0,notfound;"
            "ret;"
            ":notfound;"
            "tailcall %2i;"
            ":null;"
            "r=0;"
            "ret;",
            Class::managed_null, p_get_ivtable, lil_npc_to_fp(exn_get_rth_throw_incompatible_class_change_exception()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_get_interface_vtable", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

///////////////////////////////////////////////////////////
// Class Initialise

// Is a class initialised
static POINTER_SIZE_INT is_class_initialized(Class *clss)
{
#ifdef VM_STATS
    vm_stats_total.num_is_class_initialized++;
    clss->num_class_init_checks++;
#endif // VM_STATS
    assert(!tmn_is_suspend_enabled());
    return clss->state == ST_Initialized;
} //is_class_initialized

// Initialise class helper
static NativeCodePtr rth_get_lil_initialize_class(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        POINTER_SIZE_INT (*p_is_inited)(Class*) = is_class_initialized;
        void (*p_init)(Class*) = class_initialize;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:pint:void;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:pint;"
            "call %0i;"
            "jc r=0,not_initialized;"
            "ret;"
            ":not_initialized;"
            "push_m2n 0, 0;"
            "m2n_save_all;"
            "in2out platform:void;"
            "call %1i;"
            "pop_m2n;"
            "ret;",
            p_is_inited, p_init);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_get_initialize_class", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

///////////////////////////////////////////////////////////
// Copy Array

static int32 f2i(float f)
{
#ifdef PLATFORM_POSIX
    if (isnan(f))
#else
    if (_isnan(f))
#endif
        return 0;
    if (f>(double)2147483647)
        return 2147483647;      // maxint
    if (f<(double)(int32)0x80000000)
        return (int32)0x80000000;     // minint
    return (int32)f;
}

static NativeCodePtr rth_get_lil_f2i(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int32 (*p_f2i)(float) = f2i;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:f4:g4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g4;"
            "call %0i;"
            "ret;",
            p_f2i);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_f2i", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

#if defined (__INTEL_COMPILER) || defined (_MSC_VER)
#pragma warning( push )
#pragma warning (disable:4146)// disable warning 4146: unary minus operator applied to unsigned type, result still unsigned
#endif
static int64 f2l(float f)
{
#ifdef PLATFORM_POSIX
    if (isnan(f))
#else
    if (_isnan(f))
#endif
        return 0;

    if (f >= (double)(__INT64_C(0x7fffffffffffffff))) {
        return __INT64_C(0x7fffffffffffffff);      // maxint
    } else if (f < -(double)__INT64_C(0x8000000000000000)) {
            return -__INT64_C(0x8000000000000000);     // minint
    }

    return (int64)f;
}
#if defined (__INTEL_COMPILER) || defined (_MSC_VER)
#pragma warning( pop )
#endif

static NativeCodePtr rth_get_lil_f2l(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_f2l)(float) = f2l;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:f4:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g8;"
            "call %0i;"
            "ret;",
            p_f2l);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_f2l", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int32 d2i(double f)
{
#ifdef PLATFORM_POSIX
    if (isnan(f))
#else
    if (_isnan(f))
#endif
        return 0;
 
    if (f>(double)2147483647)
        return 2147483647;      // maxint
    if (f<(double)(int32)0x80000000)
        return (int32)0x80000000;     // minint
 
    return (int32)f;
}

static NativeCodePtr rth_get_lil_d2i(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int32 (*p_d2i)(double) = d2i;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:f8:g4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g4;"
            "call %0i;"
            "ret;",
            p_d2i);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_d2i", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

#if defined (__INTEL_COMPILER) || defined (_MSC_VER)
#pragma warning( push )
#pragma warning (disable:4146)// disable warning 4146: unary minus operator applied to unsigned type, result still unsigned
#endif
static int64 d2l(double f)
{
#ifdef PLATFORM_POSIX
    if (isnan(f))
#else
    if (_isnan(f))
#endif
    return 0;
 
    if(f >= (double)(__INT64_C(0x7fffffffffffffff))) {
        return __INT64_C(0x7fffffffffffffff);      // maxint
    } else if(f < -(double)__INT64_C(0x8000000000000000)) {
            return -__INT64_C(0x8000000000000000);     // minint
    }

    return (int64)f;
}
#if defined (__INTEL_COMPILER) || defined (_MSC_VER)
#pragma warning( pop )
#endif

static NativeCodePtr rth_get_lil_d2l(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_d2l)(double) = d2l;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:f8:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g8;"
            "call %0i;"
            "ret;",
            p_d2l);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_d2l", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int64 lshl(int64 v, int32 c)
{
    return v<<(c&0x3f);
}

static NativeCodePtr rth_get_lil_lshl(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_lshl)(int64, int32) = lshl;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:g8,g4:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g8;"
            "call %0i;"
            "ret;",
            p_lshl);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_lshl", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int64 lshr(int64 v, int32 c)
{
    return v>>(c&0x3f);
}

static NativeCodePtr rth_get_lil_lshr(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_lshr)(int64, int32) = lshr;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:g8,g4:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g8;"
            "call %0i;"
            "ret;",
            p_lshr);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_lshr", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static uint64 lushr(uint64 v, uint32 c)
{
    return v>>(c&0x3f);
}

static NativeCodePtr rth_get_lil_lushr(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        uint64 (*p_lushr)(uint64, uint32) = lushr;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:g8,g4:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g8;"
            "call %0i;"
            "ret;",
            p_lushr);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_lushr", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int64 lmul(int64 m, int64 n)
{
    return m*n;
}

static NativeCodePtr rth_get_lil_lmul(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_lmul)(int64, int64) = lmul;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g8,g8:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g8;"
            "call %0i;"
            "ret;",
            p_lmul);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_lmul", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int64 lrem(int64 m, int64 n)
{
    assert(n);
    return m%n;
}

static NativeCodePtr rth_get_lil_lrem(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_lrem)(int64, int64) = lrem;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g8,g8:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "jc i1=0,remzero;"
            "in2out platform:g8;"
            "call %0i;"
            "ret;"
            ":remzero;"
            "tailcall %1i;",
            p_lrem, lil_npc_to_fp(exn_get_rth_throw_arithmetic()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_lrem", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int64 ldiv(int64 m, int64 n)
{
    assert(n);
    return m/n;
}

static NativeCodePtr rth_get_lil_ldiv(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int64 (*p_ldiv)(int64, int64) = ldiv;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g8,g8:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "jc i1=0,divzero;"
            "in2out platform:g8;"
            "call %0i;"
            "ret;"
            ":divzero;"
            "tailcall %1i;",
            p_ldiv, lil_npc_to_fp(exn_get_rth_throw_arithmetic()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_ldiv", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static uint64 ludiv(uint64 m, uint64 n)
{
    assert(n);
    return m/n;
}

static NativeCodePtr rth_get_lil_ludiv(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        uint64 (*p_ludiv)(uint64, uint64) = ludiv;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g8,g8:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "jc i1=0,divzero;"
            "in2out platform:g8;"
            "call %0i;"
            "ret;"
            ":divzero;"
            "tailcall %1i;",
            p_ludiv, lil_npc_to_fp(exn_get_rth_throw_arithmetic()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_ludiv", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static NativeCodePtr rth_get_lil_ldiv_const(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        // This constant must be kept in sync with MAGIC in ir.cpp
        POINTER_SIZE_INT divisor_offset = 40;
        int64 (*p_ldiv)(int64, int64) = ldiv;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g8,pint:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "out platform:g8,g8:g8;"
            "o0=i0;"
            "ld o1,[i1+%0i:g8];"
            "call %1i;"
            "ret;",
            divisor_offset, p_ldiv);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_ldiv_const", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static NativeCodePtr rth_get_lil_lrem_const(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        // This constant must be kept in sync with MAGIC in ir.cpp
        POINTER_SIZE_INT divisor_offset = 40;
        int64 (*p_lrem)(int64, int64) = lrem;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g8,pint:g8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "out platform:g8,g8:g8;"
            "o0=i0;"
            "ld o1,[i1+%0i:g8];"
            "call %1i;"
            "ret;",
            divisor_offset, p_lrem);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_lrem_const", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int32 imul(int32 m, int32 n)
{
    return m*n;
}

static NativeCodePtr rth_get_lil_imul(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int32 (*p_imul)(int32, int32) = imul;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g4,g4:g4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:g4;"
            "call %0i;"
            "ret;",
            p_imul);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_imul", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int32 irem(int32 m, int32 n)
{
    assert(n);
    return m%n;
}

static NativeCodePtr rth_get_lil_irem(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int32 (*p_irem)(int32, int32) = irem;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g4,g4:g4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "jc i1=0,remzero;"
            "in2out platform:g4;"
            "call %0i;"
            "ret;"
            ":remzero;"
            "tailcall %1i;",
            p_irem, lil_npc_to_fp(exn_get_rth_throw_arithmetic()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_irem", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static int32 idiv(int32 m, int32 n)
{
    assert(n);
    return m/n;
}

static NativeCodePtr rth_get_lil_idiv(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        int32 (*p_idiv)(int32, int32) = idiv;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:g4,g4:g4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "jc i1=0,remzero;"
            "in2out platform:g4;"
            "call %0i;"
            "ret;"
            ":remzero;"
            "tailcall %1i;",
            p_idiv, lil_npc_to_fp(exn_get_rth_throw_arithmetic()));
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_idiv", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}


const int nan_data = 0xffc00000;
#define NANF  (*(float*)&nan_data)

inline static bool is_finite_f(float f) 
{
#ifdef PLATFORM_NT
    return _finite(f);
#else
    return finite(f);
#endif // PLATFORM_NT
}

bool is_infinite_f(float f) 
{
#ifdef PLATFORM_NT
    return (! _finite(f)) && (! _isnan(f));
#else
    return isinf(f);
#endif // PLATFORM_NT
}

static float frem(float m, float n)
{
    if ( is_finite_f(m) ) {
        if ( is_infinite_f(n) ) {
            return m;
        }
        if ( (n > 0 || n < 0) ) {
            return (float)fmod(m, n);
        }
    }

    return NANF;
}

static NativeCodePtr rth_get_lil_frem(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        float (*p_frem)(float, float) = frem;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:f4,f4:f4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:f4;"
            "call %0i;"
            "ret;",
            p_frem);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_frem", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static float fdiv(float m, float n)
{
    return m/n;
}

static NativeCodePtr rth_get_lil_fdiv(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        float (*p_fdiv)(float, float) = fdiv;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:f4,f4:f4;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:f4;"
            "call %0i;"
            "ret;",
            p_fdiv);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_fdiv", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static double my_drem(double m, double n)
{
    return fmod(m, n);
}

static NativeCodePtr rth_get_lil_drem(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        double (*p_drem)(double, double) = my_drem;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:f8,f8:f8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:f8;"
            "call %0i;"
            "ret;",
            p_drem);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_drem", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}

static double ddiv(double m, double n)
{
    return m/n;
}

static NativeCodePtr rth_get_lil_ddiv(int* dyn_count)
{
    static NativeCodePtr addr = NULL;

    if (!addr) {
        double (*p_ddiv)(double, double) = ddiv;
        LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:f8,f8:f8;");
        assert(cs);
        if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
        cs = lil_parse_onto_end(cs,
            "in2out platform:f8;"
            "call %0i;"
            "ret;",
            p_ddiv);
        assert(cs && lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "rth_ddiv", dump_stubs);
        lil_free_code_stub(cs);
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////////
// Get LIL version of Runtime Helper

static NativeCodePtr rth_wrap_exn_throw(int* dyncount, const char* name, NativeCodePtr stub)
{
#ifdef VM_STATS
    static SimpleHashtable wrappers(13);
    int _junk;
    NativeCodePtr wrapper;

    assert(dyncount);

    if (wrappers.lookup(stub, &_junk, &wrapper)) return wrapper;

    LilCodeStub* cs = lil_parse_code_stub(
        "entry 0:managed:arbitrary;"
        "inc [%0i:g4];"
        "tailcall %1i;",
        dyncount, lil_npc_to_fp(stub));
    assert(cs && lil_is_valid(cs));
    wrapper = LilCodeGenerator::get_platform()->compile(cs, name, dump_stubs);
    lil_free_code_stub(cs);

    wrappers.add(stub, 0, wrapper);
    return wrapper;
#else
    return stub;
#endif
}

static NativeCodePtr rth_get_lil_gc_safe_point(int * dyn_count) {
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }
    void (*tmn_safe_point_ptr)() = tmn_safe_point;
    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed::void;");
    assert(cs);
    if (dyn_count) {
        cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
        assert(cs);
    }
    cs = lil_parse_onto_end(cs,
        "push_m2n 0, 0;"
        "out platform::void;"
        "call %0i;"
        "pop_m2n;"
        "ret;",
        tmn_safe_point_ptr);
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs, "rth_gc_safe_point", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}

static NativeCodePtr rth_get_lil_gc_thread_suspend_flag_ptr(int * dyn_count) {
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed::pint;");
    assert(cs);
    if (dyn_count) {
        cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
        assert(cs);
    }

    cs = lil_parse_onto_end(cs,
        "r=ts;"
        "r=r+%0i:pint;"
        "ret;",
        thread_get_suspend_request_offset());
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs, 
                        "rth_get_lil_gc_thread_suspend_flag_ptr", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}

static void * rth_resolve(Class_Handle klass, unsigned cp_idx,
                          JavaByteCodes opcode)
{
    Compilation_Handle comp_handle;
    comp_handle.env = VM_Global_State::loader_env;
    comp_handle.jit = NULL;
    Compile_Handle ch = (Compile_Handle)&comp_handle;
    
    void * ret = NULL;
    tmn_suspend_enable();
    switch(opcode) {
    case OPCODE_INVOKEINTERFACE:
        ret = resolve_interface_method(ch, klass, cp_idx);
        break;
    case OPCODE_INVOKEVIRTUAL:
    case OPCODE_INVOKESPECIAL:
        ret = resolve_virtual_method(ch, klass, cp_idx);
        break;
    case OPCODE_INSTANCEOF:
    case OPCODE_CHECKCAST:
    case OPCODE_MULTIANEWARRAY:
        ret = resolve_class(ch, klass, cp_idx);
        break;
    case OPCODE_ANEWARRAY:
        ret = resolve_class(ch, klass, cp_idx);
        if (ret != NULL) {
            ret = class_get_array_of_class((Class_Handle)ret);
        }
        break;
    case OPCODE_NEW:
        ret = resolve_class_new(ch, klass, cp_idx);
        break;
    case OPCODE_GETFIELD:
    case OPCODE_PUTFIELD:
        ret = resolve_nonstatic_field(ch, klass, cp_idx, 
                                      opcode == OPCODE_PUTFIELD);
        break;
    case OPCODE_PUTSTATIC:
    case OPCODE_GETSTATIC:
        ret = resolve_static_field(ch, klass, cp_idx, 
                                   opcode == OPCODE_PUTSTATIC);
        if (ret != NULL) {
            Class_Handle that_class = method_get_class((Method_Handle)ret);
            tmn_suspend_disable();
            if (class_needs_initialization(that_class)) {
                assert(!exn_raised());
                class_initialize_ex(that_class, false);
            }
            return ret;
        }
        break;
    case OPCODE_INVOKESTATIC:
        ret = resolve_static_method(ch, klass, cp_idx);
        if (ret != NULL) {
            Class_Handle that_class = method_get_class((Method_Handle)ret);
            tmn_suspend_disable();
            if (class_needs_initialization(that_class)) {
                assert(!exn_raised());
                class_initialize_ex(that_class, false);
            }
            return ret;
        }
        break;
    default:    assert(false);
    } // ~switch(opcode)
    
    tmn_suspend_disable();
    if (ret == NULL) {
        class_throw_linking_error(klass, cp_idx, opcode);
        assert(false); // must be unreachanble
    }
    return ret;
}

static NativeCodePtr rth_get_lil_resolve(int * dyn_count)
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }
    LilCodeStub* cs = lil_parse_code_stub("entry 0:rth:pint,pint,pint:void;");
    assert(cs);
    if (dyn_count) {
        cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
        assert(cs);
    }
    
    cs = lil_parse_onto_end(cs,
        "push_m2n 0, 0;"
        "in2out platform:pint;"
        "call %0i;"
        "pop_m2n;"
        "ret;",
        (void*)&rth_resolve);
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs, "rth_resolve", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}

static NativeCodePtr rth_get_lil_jvmti_method_enter_callback(int * dyn_count) {
        static NativeCodePtr addr = NULL;
        if (addr) {
                return addr;
            }
    void (*jvmti_method_enter_callback_ptr)(Method_Handle) = jvmti_method_enter_callback;
    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:pint:void;");
    assert(cs);
    if (dyn_count) {
            cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
            assert(cs);
        }
    cs = lil_parse_onto_end(cs,
        "push_m2n 0, 0;"
        "in2out platform:void;"
        "call %0i;"
        "pop_m2n;"
        "ret;",
        jvmti_method_enter_callback_ptr);
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs, "rth_jvmti_method_enter_callback", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}

static NativeCodePtr rth_get_lil_jvmti_method_exit_callback(int * dyn_count) {
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }
    void (*jvmti_method_exit_callback_ptr)(Method_Handle, jvalue *) = jvmti_method_exit_callback;
    LilCodeStub* cs = lil_parse_code_stub("entry 0:managed:pint,pint:void;");
    assert(cs);
    if (dyn_count) {
        cs = lil_parse_onto_end(cs, "inc [%0i:pint];", dyn_count);
        assert(cs);
    }
    cs = lil_parse_onto_end(cs,
        "push_m2n 0, 0;"
        "out platform:pint,pint:void;"
        "o0=i1;"
        "o1=i0;"
        "call %0i;"
        "pop_m2n;"
        "ret;",
        jvmti_method_exit_callback_ptr);
    assert(cs && lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs, "rth_jvmti_method_exit_callback", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}


NativeCodePtr rth_get_lil_helper(VM_RT_SUPPORT f)
{
    int* dyn_count = NULL;
#ifdef VM_STATS
    dyn_count = vm_stats_total.rt_function_calls.lookup_or_add((void*)f, 0, NULL);
#endif

    switch(f) {
        // Object creation
    case VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE:
            return NULL;
    case VM_RT_NEW_VECTOR_USING_VTABLE:
            return NULL;
    case VM_RT_MULTIANEWARRAY_RESOLVED:
        return rth_get_lil_multianewarray(dyn_count);
    case VM_RT_LDC_STRING:
        return rth_get_lil_ldc_string(dyn_count);
        // Exceptions
    case VM_RT_THROW:
    case VM_RT_THROW_SET_STACK_TRACE:
        return rth_wrap_exn_throw(dyn_count, "rth_throw", exn_get_rth_throw());
    case VM_RT_THROW_LAZY:
        return rth_wrap_exn_throw(dyn_count, "rth_throw_lazy", exn_get_rth_throw_lazy());
    case VM_RT_IDX_OUT_OF_BOUNDS:
        return rth_wrap_exn_throw(dyn_count, "rth_throw_index_out_of_bounds", exn_get_rth_throw_array_index_out_of_bounds());
    case VM_RT_NULL_PTR_EXCEPTION:
        return rth_wrap_exn_throw(dyn_count, "rth_throw_null_pointer", exn_get_rth_throw_null_pointer());
    case VM_RT_DIVIDE_BY_ZERO_EXCEPTION:
        return rth_wrap_exn_throw(dyn_count, "rth_throw_arithmetic", exn_get_rth_throw_arithmetic());
    case VM_RT_ARRAY_STORE_EXCEPTION:
        return rth_wrap_exn_throw(dyn_count, "rth_throw_array_store", exn_get_rth_throw_array_store());
    case VM_RT_THROW_LINKING_EXCEPTION:
        return rth_get_lil_throw_linking_exception(dyn_count);
        // Synchronisation
    case VM_RT_MONITOR_ENTER:
    case VM_RT_MONITOR_ENTER_NO_EXC:
            return NULL;
    case VM_RT_MONITOR_EXIT:
    case VM_RT_MONITOR_EXIT_NON_NULL:
            return NULL;
    case VM_RT_MONITOR_ENTER_STATIC:
            return NULL;
    case VM_RT_MONITOR_EXIT_STATIC:
            return NULL;
        // Type tests
    case VM_RT_CHECKCAST:
        return rth_get_lil_checkcast(dyn_count);
    case VM_RT_INSTANCEOF:
        return rth_get_lil_instanceof(dyn_count);
    case VM_RT_AASTORE:
        return rth_get_lil_aastore(dyn_count);
    case VM_RT_AASTORE_TEST:
        return rth_get_lil_aastore_test(dyn_count);
        // Misc
    case VM_RT_GET_INTERFACE_VTABLE_VER0:
        return rth_get_lil_get_interface_vtable(dyn_count);
    case VM_RT_INITIALIZE_CLASS:
        return rth_get_lil_initialize_class(dyn_count);
    case VM_RT_GC_HEAP_WRITE_REF:
        return NULL;
    case VM_RT_GC_SAFE_POINT:
        return rth_get_lil_gc_safe_point(dyn_count);
    case VM_RT_GC_GET_THREAD_SUSPEND_FLAG_PTR:
        return rth_get_lil_gc_thread_suspend_flag_ptr(dyn_count);
        // JVMTI
    case VM_RT_JVMTI_METHOD_ENTER_CALLBACK:
        return rth_get_lil_jvmti_method_enter_callback(dyn_count);
    case VM_RT_JVMTI_METHOD_EXIT_CALLBACK:
        return rth_get_lil_jvmti_method_exit_callback(dyn_count);
        // Non-VM
    case VM_RT_F2I:
        return rth_get_lil_f2i(dyn_count);
    case VM_RT_F2L:
        return rth_get_lil_f2l(dyn_count);
    case VM_RT_D2I:
        return rth_get_lil_d2i(dyn_count);
    case VM_RT_D2L:
        return rth_get_lil_d2l(dyn_count);
    case VM_RT_LSHL:
        return rth_get_lil_lshl(dyn_count);
    case VM_RT_LSHR:
        return rth_get_lil_lshr(dyn_count);
    case VM_RT_LUSHR:
        return rth_get_lil_lushr(dyn_count);
    case VM_RT_LMUL:
#ifdef VM_LONG_OPT
    case VM_RT_LMUL_CONST_MULTIPLIER:  // Not optimised, but functional
#endif
        return rth_get_lil_lmul(dyn_count);
    case VM_RT_LREM:
        return rth_get_lil_lrem(dyn_count);
    case VM_RT_LDIV:
        return rth_get_lil_ldiv(dyn_count);
    case VM_RT_ULDIV:
        return rth_get_lil_ludiv(dyn_count);
    case VM_RT_CONST_LDIV:             // Not optimised, but functional
        return rth_get_lil_ldiv_const(dyn_count);
    case VM_RT_CONST_LREM:             // Not optimised, but functional
        return rth_get_lil_lrem_const(dyn_count);
    case VM_RT_IMUL:
        return rth_get_lil_imul(dyn_count);
    case VM_RT_IREM:
        return rth_get_lil_irem(dyn_count);
    case VM_RT_IDIV:
        return rth_get_lil_idiv(dyn_count);
    case VM_RT_FREM:
        return rth_get_lil_frem(dyn_count);
    case VM_RT_FDIV:
        return rth_get_lil_fdiv(dyn_count);
    case VM_RT_DREM:
        return rth_get_lil_drem(dyn_count);
    case VM_RT_DDIV:
        return rth_get_lil_ddiv(dyn_count);
    case VM_RT_CHAR_ARRAYCOPY_NO_EXC:
        return NULL;
    case VM_RT_RESOLVE:
        return rth_get_lil_resolve(dyn_count);
    default:
        ABORT("Unexpected helper id");
        return NULL;
    }
}

//**********************************************************************//
// End new LIL runtime support
//**********************************************************************//


/////////////////////////////////////////////////////////////

// begin Java object allocation

/**
 * intermediary function for allocation slow path to check
 * for out of memory conditions. Throws OutOfMemoryError
 * if allocation function returns null.
 */
void *vm_malloc_with_thread_pointer(
        unsigned size, Allocation_Handle ah, void *tp) {
    assert(!tmn_is_suspend_enabled());
    void *result = gc_alloc(size,ah,tp);
    if (!result) {
        exn_throw(VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return 0; // whether this return is reached or not is solved via is_unwindable state
    }
    return result;
}



// This function is deprecated.
ManagedObject *class_alloc_new_object_or_null(Class * UNREF c)
{
    return NULL;
} //class_alloc_new_object_or_null


ManagedObject *class_alloc_new_object(Class *c)
{
    assert(!tmn_is_suspend_enabled());
    //tmn_suspend_disable();
    assert(strcmp(c->name->bytes, "java/lang/Class")); 
#ifdef VM_STATS
    vm_stats_total.num_class_alloc_new_object++;
    c->num_allocations++;
    c->num_bytes_allocated += get_instance_data_size(c);
#endif //VM_STATS
    ManagedObject* o = (ManagedObject *)
        gc_alloc(c->instance_data_size, 
            c->allocation_handle, vm_get_gc_thread_local());
    if (!o) {
        exn_throw(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        //tmn_suspend_enable();
        return 0; // whether this return is reached or not is solved via is_unwindable state
    }
     //tmn_suspend_enable();
     return o;
} //class_alloc_new_object


ManagedObject *class_alloc_new_object_using_vtable(VTable *vtable)
{
    assert(!tmn_is_suspend_enabled());
    assert(strcmp(vtable->clss->name->bytes, "java/lang/Class")); 
#ifdef VM_STATS
    vm_stats_total.num_class_alloc_new_object++;
    vtable->clss->num_allocations++;
    vtable->clss->num_bytes_allocated += vtable->allocated_size;
#endif //VM_STATS
    // From vm_types.h: this is the same as instance_data_size with the constraint bit cleared.
    ManagedObject* o = (ManagedObject *)
        gc_alloc(vtable->allocated_size, 
            vtable->clss->allocation_handle, vm_get_gc_thread_local());
    if (!o) {
        exn_throw(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return NULL; // reached by interpreter and from JNI
    }
    return o;
} //class_alloc_new_object_using_vtable



ManagedObject *class_alloc_new_object_and_run_default_constructor(Class *clss)
{
    return class_alloc_new_object_and_run_constructor(clss, 0, 0);
} //class_alloc_new_object_and_run_default_constructor




ManagedObject *
class_alloc_new_object_and_run_constructor(Class *clss,
                                           Method *constructor,
                                           uint8 *constructor_args)
{
    assert(!tmn_is_suspend_enabled());
    assert(strcmp(clss->name->bytes, "java/lang/Class"));
 
    ObjectHandle obj = oh_allocate_local_handle();
    obj->object = (ManagedObject*)
        gc_alloc(clss->instance_data_size, clss->allocation_handle, vm_get_gc_thread_local());
    if (!obj->object) {
        exn_throw_only(
            VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return 0; // should never be reached
    }
#ifdef VM_STATS
    clss->num_allocations++;
    clss->num_bytes_allocated += get_instance_data_size(clss);
#endif //VM_STATS

    if(!constructor) {
        // Get the default constructor
        Global_Env *env = VM_Global_State::loader_env;
        constructor = class_lookup_method(clss, env->Init_String, env->VoidVoidDescriptor_String);
        assert(constructor);
    }


    // Every argument is at least 4 bytes long
    int num_args_estimate = constructor->get_num_arg_bytes() / 4;
    jvalue *args = (jvalue*)STD_MALLOC(num_args_estimate * sizeof(jvalue));
    args[0].l = (jobject) obj;

    int arg_num = 1;
    uint8 *argp = constructor_args;
    Arg_List_Iterator iter = constructor->get_argument_list();
    Java_Type typ;
    while((typ = curr_arg(iter)) != JAVA_TYPE_END) {
        switch(typ) {
        case JAVA_TYPE_BOOLEAN:
            argp -= sizeof(jint);
            args[arg_num].z = *(jboolean *)argp;
            break;
        case JAVA_TYPE_BYTE:
            argp -= sizeof(jint);
            args[arg_num].b = *(jbyte *)argp;
            break;
        case JAVA_TYPE_CHAR:
            argp -= sizeof(jint);
            args[arg_num].c = *(jchar *)argp;
            break;
        case JAVA_TYPE_SHORT:
            argp -= sizeof(jint);
            args[arg_num].s = *(jshort *)argp;
            break;
        case JAVA_TYPE_INT:
            argp -= sizeof(jint);
            args[arg_num].i = *(jint *)argp;
            break;
        case JAVA_TYPE_LONG:
            argp -= sizeof(jlong);
            args[arg_num].j = *(jlong *)argp;
            break;
        case JAVA_TYPE_DOUBLE:
            argp -= sizeof(jdouble);
            args[arg_num].d = *(jdouble *)argp;
            break;
        case JAVA_TYPE_FLOAT:
            argp -= sizeof(jfloat);
            args[arg_num].f = *(jfloat *)argp;
            break;
        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
            {
                argp -= sizeof(ManagedObject*);
                ObjectHandle h = oh_allocate_local_handle();
                h->object = *(ManagedObject**) argp;
                args[arg_num].l = h;
            }
            break;
        default:
            ABORT("Unexpected java type");
            break;
        }
        iter = advance_arg_iterator(iter);
        arg_num++;
        assert(arg_num <= num_args_estimate);
    }
    assert(!tmn_is_suspend_enabled());
    vm_execute_java_method_array((jmethodID) constructor, 0, args);
    
    if (exn_raised()) {
        DIE("class constructor has thrown an exception");
    }

    STD_FREE(args);

    return obj->object;
} //class_alloc_new_object_and_run_constructor


// end Java object allocation
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
// begin instanceof


static void update_general_type_checking_stats(VTable *sub, Class *super)
{
#ifdef VM_STATS
    vm_stats_total.num_type_checks ++;
    if (sub->clss == super)
        vm_stats_total.num_type_checks_equal_type ++;
    if (super->is_suitable_for_fast_instanceof)
        vm_stats_total.num_type_checks_fast_decision ++;
    else if (super->is_array)
        vm_stats_total.num_type_checks_super_is_array ++;
    else if (class_is_interface(super))
        vm_stats_total.num_type_checks_super_is_interface ++;
    else if (super->depth >= vm_max_fast_instanceof_depth())
        vm_stats_total.num_type_checks_super_is_too_deep ++;
#endif // VM_STATS
}

void vm_instanceof_update_stats(ManagedObject *obj, Class *super)
{
#ifdef VM_STATS
    vm_stats_total.num_instanceof++;
    super->num_instanceof_slow++;
    if (obj == (ManagedObject *)Class::managed_null)
        vm_stats_total.num_instanceof_null++;
    else
    {
        if (obj->vt()->clss == super)
            vm_stats_total.num_instanceof_equal_type ++;
        if (super->is_suitable_for_fast_instanceof)
            vm_stats_total.num_instanceof_fast_decision ++;
        update_general_type_checking_stats(obj->vt(), super);
    }
#endif
}

void vm_checkcast_update_stats(ManagedObject *obj, Class *super)
{
#ifdef VM_STATS
    vm_stats_total.num_checkcast ++;
    if (obj == (ManagedObject *)Class::managed_null)
        vm_stats_total.num_checkcast_null++;
    else
    {
        if (obj->vt()->clss == super)
            vm_stats_total.num_checkcast_equal_type ++;
        if (super->is_suitable_for_fast_instanceof)
            vm_stats_total.num_checkcast_fast_decision ++;
        update_general_type_checking_stats(obj->vt(), super);
    }
#endif
}



/************************************************************
 * Auxiliary functions and macros, used by the generators of
 * LIL checkcast and instanceof stubs
 */


// macro that gets the offset of a certain field within a struct or class type
#define OFFSET(Struct, Field) \
  ((POINTER_SIZE_SINT) (&(((Struct *) NULL)->Field) - NULL))

// macro that gets the size of a field within a struct or class
#define SIZE(Struct, Field) \
  (sizeof(((Struct *) NULL)->Field))


// appends code that throws a ClassCastException to a LIL stub
static LilCodeStub *gen_lil_throw_ClassCastException(LilCodeStub *cs) {
    // if instanceof returns false, throw an exception
    LilCodeStub *cs2 = lil_parse_onto_end
        (cs,
         "std_places 1;"
         "sp0 = %0i;\n"
         "tailcall %1i;",
         exn_get_class_cast_exception_type(),
         lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(cs2 != NULL);
    return cs2;
}  // gen_lil_throw_ClassCastException


// appends code that throws a ClassCastException to a LIL stub
static LilCodeStub *gen_lil_throw_ClassCastException_for_inlining(LilCodeStub *cs) {
    ABORT("The funclion is depricated, should be never called");
    // if instanceof returns false, throw an exception
    LilCodeStub *cs2 = lil_parse_onto_end
        (cs,
         "out lil::void;"
         "call.noret %0i;",
         NULL);
    assert(cs2 != NULL);
    return cs2;
}  // gen_lil_throw_ClassCastException_for_inlining


// emits the slow path of the instanceof / checkcast check
// notice that this is different from gen_lil_instanceof_stub_slow;
// the former is meant to be used as part of a bigger stub that decides
// that the slow path is appropriate, whereas the latter is a self-contained
// stub.
static LilCodeStub* gen_lil_typecheck_slowpath(LilCodeStub *cs,
                                               bool is_checkcast) {
    /* slow case; call class_is_subtype(obj->vtable()->clss, super)
     * Here's how to do this, normally *OR* with compressed refs
     *
     * l0 = obj->vtable *OR* l0 = obj->vt_offset
     * o0 = l0->clss    *OR* o0 = (l0+vtable_base)->clss
     * o1 = i1
     * call class_is_subtype(o0, o1)
     */
    LilCodeStub *cs2;
    if (vm_vtable_pointers_are_compressed())
    {
        cs2 = lil_parse_onto_end
            (cs,
            "ld l0, [i0+%0i:g4],zx;",
            /*OFFSET(ManagedObject, vt_offset)*/object_get_vtable_offset());
    }
    else
    {
        cs2 = lil_parse_onto_end
            (cs, 
            "ld l0, [i0+%0i:ref];",
            /*OFFSET(ManagedObject, vt_raw)*/object_get_vtable_offset());
    }

    cs2 = lil_parse_onto_end
        (cs2,
         "out platform:ref,pint:g4;"
         "ld o0, [l0+%0i:ref];"
         "o1 = i1;"
         "call %1i;",
         OFFSET(VTable, clss) + (vm_vtable_pointers_are_compressed() ? vm_get_vtable_base() : 0),
         (void*) class_is_subtype);

    if (is_checkcast) {
        // if class_is_subtype returns true, return the object, otherwise jump to exception code
        cs2 = lil_parse_onto_end
            (cs2,
             "jc r=0, failed;"
             "r = i0;"
             "ret;");
    }
    else {
        // just return whatever class_is_subtype returned
        cs2 = lil_parse_onto_end
            (cs2,
             "ret;");
    }

    assert(cs2 != NULL);
    return cs2;
}  // gen_lil_typecheck_slowpath


// emits the fast path of the instanceof / checkcast check
// presupposes that all necessary checks have already been performed,
// and that the fast path is appropriate.
static LilCodeStub* gen_lil_typecheck_fastpath(LilCodeStub *cs,
                                               bool is_checkcast) {

    /* fast case; check whether
     *  (obj->vt()->superclasses[super->depth-1] == super)
     * Here's how to do this, normally *OR* with compressed refs
     *
     * l0 = obj->vtable *OR* l0 = obj->vt_offset
     * l1 = i1->depth
     * l2 = l0->superclasses[l1-1] *OR*
     *     (l0+vtable_base)->superclasses[l1-1]
     * check if i1 == l2
     */
    LilCodeStub *cs2;
    if (vm_vtable_pointers_are_compressed())
    {
        cs2 = lil_parse_onto_end
            (cs,
            "ld l0, [i0+%0i:g4],zx;",
            object_get_vtable_offset());
    }
    else
    {
        cs2 = lil_parse_onto_end
            (cs, 
            "ld l0, [i0+%0i:ref];",
            object_get_vtable_offset());
    }

    cs2 = lil_parse_onto_end
        (cs2,
         "ld l1, [i1 + %0i: g4],zx;"
         "ld l2, [l0 + %1i*l1 + %2i: pint];"
         "jc i1 != l2, failed;",
         OFFSET(Class, depth),
         sizeof(Class*),
         OFFSET(VTable, superclasses) - sizeof(Class*) + (vm_vtable_pointers_are_compressed() ? vm_get_vtable_base() : 0)
         );

    if (is_checkcast) {
        // return the object on success
        cs2 = lil_parse_onto_end
            (cs2,
             "r = i0;"
             "ret;");
    }
    else {
        // instanceof; return 1 on success, 0 on failure
        cs2 = lil_parse_onto_end
            (cs2,
             "r=1:g4;"
             "ret;"
             ":failed;"
             "r=0:g4;"
             "ret;");
    }

    assert(cs != NULL);
    return cs2;
}  // gen_lil_typecheck_fastpath


/*
 * Auxiliary functions for LIL and Instanceof - END
 ***************************************************/


/*****************************************************************
 * Functions that generate LIL stubs for checkcast and instanceof
 */


// creates a LIL code stub for checkcast or instanceof
// can be used by both IA32 and IPF code
LilCodeStub *gen_lil_typecheck_stub(bool is_checkcast) {

    // assert that some sizes are what we expect them to be
    assert(SIZE(Class, is_suitable_for_fast_instanceof) == 4);
    assert(SIZE(Class, depth) == 4);

    LilCodeStub* cs = NULL;

    // check if object address is NULL
    if (is_checkcast) {
        // args: ManagedObject *obj, Class *super; returns a ManagedObject*
        cs = lil_parse_code_stub
        ("entry 0:rth:ref,pint:ref;"
         "jc i0!=%0i:ref,nonnull;"
         "r=i0;"  // return obj if obj==NULL
         "ret;",
         Class::managed_null);
    }
    else {
        // args: ManagedObject *obj, Class *super; returns a boolean
        cs = lil_parse_code_stub
        ("entry 0:rth:ref,pint:g4;"
         "jc i0!=%0i:ref,nonnull;"
         "r=0:g4;"  // return FALSE if obj==NULL
         "ret;",
         Class::managed_null);
    }

    // check whether the fast or the slow path is appropriate
    cs = lil_parse_onto_end
        (cs,
         ":nonnull;"
         "locals 3;"
         // check if super->is_suitable_for_fast_instanceof
         "ld l0, [i1 + %0i: g4];"
         "jc l0!=0:g4, fast;",
         OFFSET(Class, is_suitable_for_fast_instanceof));

    // append the slow path right here
    cs = gen_lil_typecheck_slowpath(cs, is_checkcast);

    // generate a "fast:" label, followed by the fast path
    cs = lil_parse_onto_end(cs, ":fast;");
    cs = gen_lil_typecheck_fastpath(cs, is_checkcast);

    if (is_checkcast) {
        // if the check has failed, throw an exception
        cs = lil_parse_onto_end(cs, ":failed;");
        cs = gen_lil_throw_ClassCastException(cs);
    }

    assert(lil_is_valid(cs));
    return cs;
}  // gen_lil_typecheck_stub



// creates a SPECIALIZED LIL code stub for checkcast or instanceof
// it assumes that the class is suitable for fast instanceof checks.
// The result is a different fast stub for every class.
// will_inline should be set to TRUE if this stub will be inlined
// in a JIT, and false if it will be passed to a code generator
// (this is due to the slightly different treatment of exceptions)
LilCodeStub *gen_lil_typecheck_stub_specialized(bool is_checkcast,
                                                bool will_inline,
                                                Class *superclass) {
    LilCodeStub *cs = NULL;

    // check if object address is NULL
    if (is_checkcast) {
        // args: ManagedObject *obj, Class *super; returns a ManagedObject*
        cs = lil_parse_code_stub
        ("entry 0:rth:ref,pint:ref;"
         "jc i0!=%0i,nonnull;"
         "r=i0;"  // return obj if obj==NULL
         "ret;",
         Class::managed_null);
    }
    else {
        // args: ManagedObject *obj, Class *super; returns a boolean
        cs = lil_parse_code_stub
        ("entry 0:rth:ref,pint:g4;"
         "jc i0!=%0i,nonnull;"
         "r=0:g4;"  // return FALSE if obj==NULL
         "ret;",
         Class::managed_null);
    }

    /* fast case; check whether
     *  (obj->vt()->superclasses[super->depth-1] == super)
     * Here's how to do this, normally *OR* with compressed refs
     *
     * l0 = obj->vt_raw *OR* l0 = obj->vt_offset
     * l1 = l0->superclasses[superclass->depth-1] *OR*
     *     (l0+vtable_base)->superclasses[superclass->depth-1]
     * check if l1 == superclass
     */
    cs = lil_parse_onto_end(cs,
                            ":nonnull;"
                            "locals 2;");
    if (vm_vtable_pointers_are_compressed())
    {
        cs = lil_parse_onto_end
            (cs,
            "ld l0, [i0+%0i:g4];",
            object_get_vtable_offset());
    }
    else
    {
        cs = lil_parse_onto_end
            (cs,
            "ld l0, [i0+%0i:ref];",
            object_get_vtable_offset());
    }

    cs = lil_parse_onto_end
        (cs,
         "ld l1, [l0 + %0i: ref];"
         "jc l1 != %1i, failed;",
         OFFSET(VTable, superclasses) + (vm_vtable_pointers_are_compressed() ? vm_get_vtable_base() : 0)
         + sizeof(Class*) * (superclass->depth-1),
         (POINTER_SIZE_INT) superclass);

    if (is_checkcast) {
        // return the object on success
        cs = lil_parse_onto_end
            (cs,
             "r = i0;"
             "ret;");
        assert(cs != NULL);
    }
    else {
        // instanceof; return 1 on success, 0 on failure
        cs = lil_parse_onto_end
            (cs,
             "r=1:g4;"
             "ret;"
             ":failed;"
             "r=0:g4;"
             "ret;");
        assert(cs != NULL);
    }

    if (is_checkcast) {
        // if the check has failed, throw an exception
        cs = lil_parse_onto_end(cs, ":failed;");
        if (will_inline)
            cs = gen_lil_throw_ClassCastException_for_inlining(cs);
        else
            cs = gen_lil_throw_ClassCastException(cs);
    }

    assert(lil_is_valid(cs));
    return cs;
}  // gen_lil_typecheck_stub_specialized

/*
 * Functions that generate LIL stubs for checkcast and instanceof - END
 ***********************************************************************/


void vm_aastore_test_update_stats(ManagedObject *elem, Vector_Handle array)
{
#ifdef VM_STATS
    vm_stats_total.num_aastore_test++;
    if (elem == (ManagedObject *)Class::managed_null)
    {
        vm_stats_total.num_aastore_test_null ++;
        return;
    }
    VTable *vt = get_vector_vtable(array);
    if (vt == cached_object_array_vtable_ptr)
    {
        vm_stats_total.num_aastore_test_object_array++;
        return;
    }
    Class *array_class = vt->clss;
    if (elem->vt()->clss == array_class->array_element_class)
        vm_stats_total.num_aastore_test_equal_type ++;
    if (array_class->array_element_class->is_suitable_for_fast_instanceof)
        vm_stats_total.num_aastore_test_fast_decision ++;
    update_general_type_checking_stats(elem->vt(), array_class->array_element_class);
#endif
}


// Returns TRUE if "sub" represents a class that is a subtype of "super",
// according to the Java instanceof rules.
// "sub" must correspond to a non-interface type, since interfaces
// currently have a null vtable.
// 
// No VM_STATS calculations are done here.  Call class_is_subtype_fast
// if stats are needed.
 static Boolean class_is_subtype_fast_no_stats(VTable *sub, Class *super)
{
    assert(sub != NULL); // Might happen if vtable of an interface class is accidentally taken.
    if (super->is_suitable_for_fast_instanceof)
    {
        return sub->superclasses[super->depth-1] == super ? TRUE : FALSE;
    }
    return class_is_subtype(sub->clss, super);
} // class_is_subtype_fast_no_stats



Boolean class_is_subtype_fast(VTable *sub, Class *super)
{
    update_general_type_checking_stats(sub, super);
    return class_is_subtype_fast_no_stats(sub, super);
} // class_is_subtype_fast


Boolean vm_instanceof_class(Class *s, Class *t) {
    return class_is_subtype(s, t);
}


// Returns TRUE if "s" represents a class that is a subtype of "t",
// according to the Java instanceof rules.
//
// No VM_STATS values are modified here.
Boolean class_is_subtype(Class *s, Class *t)
{
    if (s == t) {
        return TRUE;
    }

    Global_Env *env = VM_Global_State::loader_env;

    Class *object_class = env->JavaLangObject_Class;
    assert(object_class != NULL);

    if(s->is_array) {
        assert(*s->name->bytes == '[');
        if (t == object_class) {
            return TRUE;
        }
        if(t == env->java_io_Serializable_Class) {
            return TRUE;
        }
        if(t == env->java_lang_Cloneable_Class) {
            return TRUE;
        }
        if(!t->is_array) {
            return FALSE;
        }

        return class_is_subtype(s->array_element_class, t->array_element_class);
    } else {
        assert(s->name->bytes[0] != '[');
        if(!class_is_interface(t)) {
            for(Class *c = s; c; c = c->super_class) {
                if(c == t){
                    return TRUE;
                }
            }
        } else {
            for(Class *c = s; c; c = c->super_class) {
                unsigned n_intf = c->n_superinterfaces;
                for(unsigned i = 0; i < n_intf; i++) {
                    Class *intf = c->superinterfaces[i].clss;
                    assert(intf);
                    assert(class_is_interface(intf));
                    if(class_is_subtype(intf, t)) {
                        return TRUE;
                    }
                }
            }
        }
    }

    return FALSE;
} //class_is_subtype



// 20030321 The instanceof JIT support routines expect to be called directly from managed code. 
VMEXPORT // temporary solution for interpreter unplug
int __stdcall vm_instanceof(ManagedObject *obj, Class *c)
{
    assert(!tmn_is_suspend_enabled());

#ifdef VM_STATS
    vm_stats_total.num_instanceof++;
    c->num_instanceof_slow++;
#endif

    ManagedObject *null_ref = (ManagedObject *)Class::managed_null;
    if (obj == null_ref) {
#ifdef VM_STATS
        vm_stats_total.num_instanceof_null++;
#endif
        return 0;
    }
    assert(obj->vt());
#ifdef VM_STATS
    if (obj->vt()->clss == c)
        vm_stats_total.num_instanceof_equal_type ++;
    if (c->is_suitable_for_fast_instanceof)
        vm_stats_total.num_instanceof_fast_decision ++;
#endif // VM_STATS
    return class_is_subtype_fast(obj->vt(), c);
} //vm_instanceof

// end instanceof
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
// begin aastore and aastore_test


// 20030321 This JIT support routine expects to be called directly from managed code. 
// The return value is 1 if the element reference can be stored into the array, or 0 otherwise.
int __stdcall
vm_aastore_test(ManagedObject *elem,
                 Vector_Handle array)
{
#ifdef VM_STATS
    vm_stats_total.num_aastore_test++;
#endif // VM_STATS

    ManagedObject *null_ref = (ManagedObject *)Class::managed_null;
    if (array == null_ref) {
        return 0;
    }
    if (elem == null_ref) {
#ifdef VM_STATS
        vm_stats_total.num_aastore_test_null++;
#endif // VM_STATS
        return 1;
    }

    VTable *vt = get_vector_vtable(array);
    if (vt == cached_object_array_vtable_ptr) {
#ifdef VM_STATS
        vm_stats_total.num_aastore_test_object_array++;
#endif // VM_STATS
        return 1;
    }

    Class *array_class = vt->clss;
    assert(array_class);
    assert(array_class->is_array);

#ifdef VM_STATS
    if (elem->vt()->clss == array_class->array_element_class)
        vm_stats_total.num_aastore_test_equal_type ++;
    if (array_class->array_element_class->is_suitable_for_fast_instanceof)
        vm_stats_total.num_aastore_test_fast_decision ++;
#endif // VM_STATS
    return class_is_subtype_fast(elem->vt(), array_class->array_element_class);
} //vm_aastore_test


// 20030505 This JIT support routine expects to be called directly from managed code. 
// The return value is either NULL or the ClassHandle for an exception to throw.
void * __stdcall
vm_rt_aastore(ManagedObject *elem, int idx, Vector_Handle array)
{
#ifdef VM_STATS
    vm_stats_total.num_aastore ++;
#endif // VM_STATS

    Global_Env *env = VM_Global_State::loader_env;
    ManagedObject *null_ref = (ManagedObject *)Class::managed_null;
    if (array == null_ref) {
        return env->java_lang_NullPointerException_Class;
    } else if (((uint32)idx) >= (uint32)get_vector_length(array)) {
        return env->java_lang_ArrayIndexOutOfBoundsException_Class;
    } else if (elem != null_ref) {
        Class *array_class = get_vector_vtable(array)->clss;
        assert(array_class);
        assert(array_class->is_array);
#ifdef VM_STATS
        // XXX - Should update vm_stats_total.num_aastore_object_array
        if (elem->vt()->clss == array_class->array_element_class)
            vm_stats_total.num_aastore_equal_type ++;
        if (array_class->array_element_class->is_suitable_for_fast_instanceof)
            vm_stats_total.num_aastore_fast_decision ++;
#endif // VM_STATS
        if (class_is_subtype_fast(elem->vt(), array_class->array_element_class)) {
            STORE_REFERENCE((ManagedObject *)array, get_vector_element_address_ref(array, idx), (ManagedObject *)elem);
        } else {
            return env->java_lang_ArrayStoreException_Class;
        }
    } else {
#ifdef VM_STATS
        vm_stats_total.num_aastore_null ++;
#endif // VM_STATS
        // elem is null. We don't have to check types for a null reference. We also don't have to record stores of null references.
        if (VM_Global_State::loader_env->compress_references) {
            COMPRESSED_REFERENCE *elem_ptr = (COMPRESSED_REFERENCE *)get_vector_element_address_ref(array, idx);
            *elem_ptr = (COMPRESSED_REFERENCE)NULL;
        } else {
            ManagedObject **elem_ptr = get_vector_element_address_ref(array, idx);
            *elem_ptr = (ManagedObject *)NULL;
        }
    }
    return NULL;
} //vm_rt_aastore


// end aastore and aastore_test
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
// begin get_interface_vtable

void *vm_get_interface_vtable(ManagedObject *obj, Class *iid)
{
    return rth_get_interface_vtable(obj, iid);
} //vm_get_interface_vtable


// end get_interface_vtable
/////////////////////////////////////////////////////////////


/**
 * @brief Generates an VM's helper to invoke the provided function.
 *
 * The helper takes 'void*' parameter which is passed to the function after
 * some preparation made (namely GC and stack info are prepared to allow GC
 * to work properly).
 *
 * The function must follow stdcall convention, which takes 'void*' and
 * returns 'void*', so does the helper.
 * On a return from the function, the helper checks whether an exception
 * was raised for the current thread, and rethrows it if necessary.
 */
VMEXPORT void * vm_create_helper_for_function(void* (*fptr)(void*))
{
    static const char * lil_stub =
        "entry 0:stdcall:pint:pint;"    // the single argument is 'void*'
        "push_m2n 0, 0;"                // create m2n frame
        "out stdcall::void;"
        "call %0i;"                     // call tmn_suspen_enable()
        "in2out stdcall:pint;"          // reloads input arg into output
        "call %1i;"                     // call the foo
        "locals 2;"                     //
        "l0 = r;"                       // save result
        "out stdcall::void;"
        "call %2i;"                     // call tmn_suspen_disable()
        "l1 = ts;"
        "ld l1, [l1 + %3i:ref];"
        "jc l1 != 0,_exn_raised;"          // test whether an exception happened
        "pop_m2n;"                      // pop out m2n frame
        "r = l0;"                       // no exceptions pending, restore ..
        "ret;"                          // ret value and exit
        ":_exn_raised;"                 //
        "out platform::void;"           //
        "call.noret %4i;";              // re-throw exception

    void * fptr_rethrow = (void*)&rethrow_current_thread_exception;
    void * fptr_suspend_enable = (void*)&tmn_suspend_enable;
    void * fptr_suspend_disable = (void*)&tmn_suspend_disable;

    LilCodeStub* cs = lil_parse_code_stub(
        lil_stub,
        fptr_suspend_enable, (void*)fptr, fptr_suspend_disable,
        OFFSET(VM_thread, p_exception_object), fptr_rethrow);
    assert(lil_is_valid(cs));
    void * addr = LilCodeGenerator::get_platform()->compile(cs,
                                            "generic_wrapper", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
};
