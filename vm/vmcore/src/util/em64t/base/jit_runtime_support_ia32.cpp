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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.5 $
 */  



//MVM
#include <iostream>

using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#define LOG_DOMAIN "vm.helpers"
#include "cxxlog.h"

#include "object_layout.h"
#include "open/types.h"
#include "Class.h"
#include "environment.h"
#include "lil.h"
#include "lil_code_generator.h"
#include "method_lookup.h"
#include "exceptions.h"
#include "vm_synch.h"
#include "open/gc.h"
#include "ini.h"
#include "nogc.h"
#include "encoder.h"
#include "open/vm_util.h"

#include "vm_threads.h"
#include "mon_enter_exit.h"
#include "vm_arrays.h"
#include "vm_strings.h"
#include "compile.h"

#include "mon_enter_exit.h"

#include "sync_bits.h"

#include "vm_stats.h"
#include "internal_jit_intf.h"
#include "jit_runtime_support_common.h"
#include "jit_runtime_support.h"

#include "../m2n_em64t_internal.h"

#include "open/vm_util.h"

// gets the offset of a certain field within a struct or class type
#define OFFSET(Struct, Field) \
  ((int) (&(((Struct *) NULL)->Field) - NULL))

// gets the size of a field within a struct or class
#define SIZE(Struct, Field) \
  (sizeof(((Struct *) NULL)->Field))

extern bool dump_stubs;

void * getaddress__vm_monitor_enter_naked();
void * getaddress__vm_monitor_enter_static_naked();
void * getaddress__vm_monitor_exit_naked();
void * getaddress__vm_monitor_exit_static_naked();
void * get_generic_rt_support_addr_ia32(VM_RT_SUPPORT f);


/////////////////////////////////////////////////////////////////
// begin VM_Runtime_Support
/////////////////////////////////////////////////////////////////
/*
static void vm_throw_java_lang_ClassCastException()
{
    assert(!hythread_is_suspend_enabled());
    throw_java_exception("java/lang/ClassCastException");
} //vm_throw_java_lang_ClassCastException

#ifdef VM_STATS // exclude remark in release mode (defined but not used)
static void update_checkcast_stats(ManagedObject *obj, Class *c)
{
    vm_stats_total.num_checkcast ++;
    if (obj == NULL)
        vm_stats_total.num_checkcast_null ++;
    if (obj != NULL && obj->vt()->clss == c)
        vm_stats_total.num_checkcast_equal_type ++;
    if (obj != NULL && c->is_suitable_for_fast_instanceof)
        vm_stats_total.num_checkcast_fast_decision ++;
} //update_checkcast_stats
#endif
*/

// 20030321 This JIT support routine expects to be called directly from managed code.
// NOTE: We do not translate null references since vm_instanceof() also expects to be 
//       called directly by managed code.
static void *getaddress__vm_checkcast_naked()
{
    static void *addr = 0;
    return addr;
}  //getaddress__vm_checkcast_naked




// This function is added so that we can have a LIL version of
// instanceof.  If LIL is turned off, the address of vm_instanceof is
// returned, just like before.
static void *getaddress__vm_instanceof()
{
    static void *addr = 0;
    if (addr) {
        return addr;
    }

    if (VM_Global_State::loader_env->use_lil_stubs)
    {
        LilCodeStub *cs = gen_lil_typecheck_stub(false);
        assert(lil_is_valid(cs));
        addr = LilCodeGenerator::get_platform()->compile(cs, "vm_instanceof", dump_stubs);
        lil_free_code_stub(cs);
        return addr;
    }

    // just use vm_instanceof
    addr = (void *) vm_instanceof;
    return addr;
}

/*
static Boolean is_class_initialized(Class *clss)
{
#ifdef VM_STATS
    vm_stats_total.num_is_class_initialized++;
    clss->num_class_init_checks++;
#endif // VM_STATS
    assert(!hythread_is_suspend_enabled());
    return clss->state == ST_Initialized;
} //is_class_initialized
*/


static void *getaddress__vm_initialize_class_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_initialize_class_naked



//////////////////////////////////////////////////////////////////////
// Object allocation
//////////////////////////////////////////////////////////////////////

static void *getaddress__vm_alloc_java_object_resolved_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_alloc_java_object_resolved_naked


static void *generate_object_allocation_stub_with_thread_pointer(char *fast_obj_alloc_proc,
                                                                 char *slow_obj_alloc_proc,
                                                                 char *stub_name)
{
    return (void *)NULL;
} //generate_object_allocation_stub_with_thread_pointer


static void *getaddress__vm_alloc_java_object_resolved_using_vtable_and_size_naked()
{
    static void *addr = 0;
    if (addr) {
        return addr;
    }

    addr = generate_object_allocation_stub_with_thread_pointer((char *) gc_alloc_fast,
        (char *) vm_malloc_with_thread_pointer,
        "getaddress__vm_alloc_java_object_resolved_using_thread_pointer_naked");
    
    return addr;
} //getaddress__vm_alloc_java_object_resolved_using_vtable_and_size_naked


static void* vm_aastore_nullpointer()
{
    static NativeCodePtr addr = NULL;
    return addr;
} //vm_aastore_nullpointer


static void* vm_aastore_array_index_out_of_bounds()
{
    static NativeCodePtr addr = NULL;
    return addr;
} //vm_aastore_array_index_out_of_bounds


static void* vm_aastore_arraystore()
{
    static NativeCodePtr addr = NULL;
    return addr;
} //vm_aastore_arraystore



static void *__stdcall
aastore_ia32(volatile ManagedObject *elem,
             int idx,
             Vector_Handle array) stdcall__;


// 20030321 This JIT support routine expects to be called directly from managed code. 
static void *__stdcall
aastore_ia32(volatile ManagedObject *elem,
            int idx,
            Vector_Handle array)
{
    if (VM_Global_State::loader_env->compress_references) {
        // 20030321 Convert a null reference from a managed (heap_base) to an unmanaged null (NULL/0).
        if (elem == (volatile ManagedObject *)Class::heap_base) {
            elem = NULL;
        }
        if (array == (ManagedObject *)Class::heap_base) {
            array = NULL;
        }
    }

    assert ((elem == NULL) || (((ManagedObject *)elem)->vt() != NULL));
#ifdef VM_STATS
    vm_stats_total.num_aastore++;
#endif // VM_STATS
    void *new_eip = 0;
    if (array == NULL) {
        new_eip = vm_aastore_nullpointer();
    } else if ((unsigned)get_vector_length(array) <= (unsigned)idx) {
        new_eip = vm_aastore_array_index_out_of_bounds();
    } else {
        assert(idx >= 0);
        if (elem != NULL) {
            VTable *vt = get_vector_vtable(array);
#ifdef VM_STATS
            if (vt == cached_object_array_vtable_ptr)
                vm_stats_total.num_aastore_object_array ++;
            if (vt->clss->array_element_class->vtable == ((ManagedObject *)elem)->vt())
                vm_stats_total.num_aastore_equal_type ++;
            if (vt->clss->array_element_class->is_suitable_for_fast_instanceof)
                vm_stats_total.num_aastore_fast_decision ++;
#endif // VM_STATS
            if(vt == cached_object_array_vtable_ptr ||
                class_is_subtype_fast(((ManagedObject *)elem)->vt(), vt->clss->array_element_class)) {
                STORE_REFERENCE((ManagedObject *)array, get_vector_element_address_ref(array, idx), (ManagedObject *)elem);
                return 0;           
            }
            new_eip = vm_aastore_arraystore();
        } else {
            // A null reference. No need to check types for a null reference.
            assert(elem == NULL);
#ifdef VM_STATS
            vm_stats_total.num_aastore_null ++;
#endif // VM_STATS
            // 20030502 Someone earlier commented out a call to the GC interface function gc_heap_slot_write_ref() and replaced it
            // by code to directly store a NULL in the element without notifying the GC. I've retained that change here but I wonder if
            // there could be a problem later with, say, concurrent GCs.
            if (VM_Global_State::loader_env->compress_references) {
                COMPRESSED_REFERENCE *elem_ptr = (COMPRESSED_REFERENCE *)get_vector_element_address_ref(array, idx);
                *elem_ptr = (COMPRESSED_REFERENCE)NULL;
            } else {
                ManagedObject **elem_ptr = get_vector_element_address_ref(array, idx);
                *elem_ptr = (ManagedObject *)NULL;
            }
            return 0;
        }
    }

    // This may possibly break if the C compiler applies very aggresive optimizations.
    void **saved_eip = ((void **)&elem) - 1;
    void *old_eip = *saved_eip;
    *saved_eip = new_eip;
    return old_eip;
} //aastore_ia32


static void *getaddress__vm_aastore()
{
    assert(VM_Global_State::loader_env->use_lil_stubs);
    static void *addr = NULL;
    if (addr != NULL) {
        return addr;
    }

    LilCodeStub* cs = lil_parse_code_stub(
        "entry 0:managed:ref,pint,ref:void;   // The args are the element ref to store, the index, and the array to store into\n"
        "in2out managed:pint; "
        "call %0i;                            // vm_rt_aastore either returns NULL or the ClassHandle of an exception to throw \n"
        "jc r!=0,aastore_failed; \
         ret; \
         :aastore_failed; \
         std_places 1; \
         sp0=r; \
         tailcall %1i;",
        (void *)vm_rt_aastore,
        exn_get_rth_throw_lazy_trampoline());
    assert(lil_is_valid(cs));
    addr = LilCodeGenerator::get_platform()->compile(cs, "vm_aastore", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
} //getaddress__vm_aastore



static void * gen_new_vector_stub(char *stub_name, char *fast_new_vector_proc, char *slow_new_vector_proc)
{
    return NULL;
} //gen_new_vector_stub


static void *getaddress__vm_new_vector_naked()
{
    static void *addr = 0;
    if (addr) {
        return addr;
    }

    addr = gen_new_vector_stub("getaddress__vm_new_vector_naked", 
        (char *)vm_new_vector_or_null, (char *)vm_new_vector);
    return addr;
} //getaddress__vm_new_vector_naked


static void *getaddress__vm_new_vector_using_vtable_naked() {
    static void *addr = 0;
    if (addr) {
        return addr;
    }
    
    addr = generate_object_allocation_stub_with_thread_pointer((char *)vm_new_vector_or_null_using_vtable_and_thread_pointer,
        (char *)vm_new_vector_using_vtable_and_thread_pointer,
        "getaddress__vm_new_vector_using_vtable_naked");
    return addr;
} //getaddress__vm_new_vector_using_vtable_naked


// This is a __cdecl function and the caller must pop the arguments.
static void *getaddress__vm_multianewarray_resolved_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_multianewarray_resolved_naked



static void *getaddress__vm_instantiate_cp_string_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_instantiate_cp_string_naked


/*
static void vm_throw_java_lang_IncompatibleClassChangeError()
{
    throw_java_exception("java/lang/IncompatibleClassChangeError");
} //vm_throw_java_lang_IncompatibleClassChangeError
*/


// 20030321 This JIT support routine expects to be called directly from managed code. 
void * getaddress__vm_get_interface_vtable_old_naked()  //wjw verify that this works
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_get_interface_vtable_old_naked

/*
static void vm_throw_java_lang_ArithmeticException()
{
    assert(!hythread_is_suspend_enabled());
    throw_java_exception("java/lang/ArithmeticException");
} //vm_throw_java_lang_ArithmeticException

static void* getaddress__setup_java_to_native_frame()
{
    static void *addr = 0;
    return addr;
} //getaddress__setup_java_to_native_frame
*/


VMEXPORT char *gen_setup_j2n_frame(char *s)
{
    return s;
} //setup_j2n_frame

/*
static void* getaddress__pop_java_to_native_frame()
{
    static void *addr = 0;
    return addr;
} //getaddress__pop_java_to_native_frame
*/

VMEXPORT char *gen_pop_j2n_frame(char *s)
{
    return s;
} //setup_j2n_frame


/////////////////////////////////////////////////////////////////
// end VM_Runtime_Support
/////////////////////////////////////////////////////////////////

/*
static void
vm_throw_linking_exception(Class *clss,
                           unsigned cp_index,
                           unsigned opcode)
{
    printf("vm_throw_linking_exception, idx=%d\n", cp_index);
    class_throw_linking_error(clss, cp_index, opcode);
} //vm_throw_linking_exception
*/

void * getaddress__vm_throw_linking_exception_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_throw_linking_exception_naked



// 20030321 This JIT support routine expects to be called directly from managed code. 
void * getaddress__gc_write_barrier_fastcall()
{
    static void *addr = 0;
    return addr;
} //getaddress__gc_write_barrier_fastcall

/*
static int64 __stdcall vm_lrem(int64 m, int64 n) stdcall__;

static int64 __stdcall vm_lrem(int64 m, int64 n)
{
    assert(!hythread_is_suspend_enabled());
    return m % n;
} //vm_lrem
*/

void * getaddress__vm_lrem_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_lrem_naked

/*
static int64 __stdcall vm_ldiv(int64 m, int64 n) stdcall__;

static int64 __stdcall vm_ldiv(int64 m, int64 n)
{
    assert(!hythread_is_suspend_enabled());
    assert(n);
    return m / n;
} //vm_ldiv
*/

static void *getaddress__vm_ldiv_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_ldiv_naked






#ifdef VM_STATS

static void register_request_for_rt_function(VM_RT_SUPPORT f) {
    // Increment the number of times that f was requested by a JIT. This is not the number of calls to that function,
    // but this does tell us how often a call to that function is compiled into JITted code.
    vm_stats_total.rt_function_requests.add((void *)f, /*value*/ 1, /*value1*/ NULL);
} //register_request_for_rt_function

#endif //VM_STATS


void *vm_get_rt_support_addr(VM_RT_SUPPORT f)
{
#ifdef VM_STATS
    register_request_for_rt_function(f);
#endif // VM_STATS

        NativeCodePtr res = rth_get_lil_helper(f);
        if (res) return res;
    

    switch(f) {
    case VM_RT_NULL_PTR_EXCEPTION:
        return exn_get_rth_throw_null_pointer();
    case VM_RT_IDX_OUT_OF_BOUNDS:
        return exn_get_rth_throw_array_index_out_of_bounds();
    case VM_RT_ARRAY_STORE_EXCEPTION:
        return exn_get_rth_throw_array_store();
    case VM_RT_DIVIDE_BY_ZERO_EXCEPTION:
        return exn_get_rth_throw_arithmetic();
    case VM_RT_THROW:
    case VM_RT_THROW_SET_STACK_TRACE:
        return exn_get_rth_throw();
    case VM_RT_THROW_LAZY:
        return exn_get_rth_throw_lazy();
    case VM_RT_LDC_STRING:
        return getaddress__vm_instantiate_cp_string_naked();
    case VM_RT_NEW_RESOLVED:
        return getaddress__vm_alloc_java_object_resolved_naked();
    case VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE:
        return getaddress__vm_alloc_java_object_resolved_using_vtable_and_size_naked(); 
    case VM_RT_MULTIANEWARRAY_RESOLVED:
        return getaddress__vm_multianewarray_resolved_naked();
    case VM_RT_NEW_VECTOR:
        return getaddress__vm_new_vector_naked();
    case VM_RT_NEW_VECTOR_USING_VTABLE:
        return getaddress__vm_new_vector_using_vtable_naked();
    case VM_RT_AASTORE:
        if (VM_Global_State::loader_env->use_lil_stubs) {
            return getaddress__vm_aastore();
        } else {
            return (void *)aastore_ia32;
        }
    case VM_RT_AASTORE_TEST:
        return (void *)vm_aastore_test;
    case VM_RT_WRITE_BARRIER_FASTCALL:
        return getaddress__gc_write_barrier_fastcall();

    case VM_RT_CHECKCAST:
        return getaddress__vm_checkcast_naked();

    case VM_RT_INSTANCEOF:
    return getaddress__vm_instanceof();

    case VM_RT_MONITOR_ENTER:
    case VM_RT_MONITOR_ENTER_NO_EXC:
        return getaddress__vm_monitor_enter_naked();

    case VM_RT_MONITOR_ENTER_STATIC:
        return getaddress__vm_monitor_enter_static_naked();

    case VM_RT_MONITOR_EXIT:
    case VM_RT_MONITOR_EXIT_NON_NULL:
        return getaddress__vm_monitor_exit_naked();

    case VM_RT_MONITOR_EXIT_STATIC:
        return getaddress__vm_monitor_exit_static_naked();

    case VM_RT_GET_INTERFACE_VTABLE_VER0:
        return getaddress__vm_get_interface_vtable_old_naked();  //tryitx
    case VM_RT_INITIALIZE_CLASS:
        return getaddress__vm_initialize_class_naked();
    case VM_RT_THROW_LINKING_EXCEPTION:
        return getaddress__vm_throw_linking_exception_naked();

    case VM_RT_LREM:
        return getaddress__vm_lrem_naked();
    case VM_RT_LDIV:
        return getaddress__vm_ldiv_naked();

    case VM_RT_F2I:
    case VM_RT_F2L:
    case VM_RT_D2I:
    case VM_RT_D2L:
    case VM_RT_LSHL:
    case VM_RT_LSHR:
    case VM_RT_LUSHR:
    case VM_RT_FREM:
    case VM_RT_DREM:
    case VM_RT_LMUL:
#ifdef VM_LONG_OPT
    case VM_RT_LMUL_CONST_MULTIPLIER:
#endif
    case VM_RT_CONST_LDIV:
    case VM_RT_CONST_LREM:
    case VM_RT_DDIV:
    case VM_RT_IMUL:
    case VM_RT_IDIV:
    case VM_RT_IREM:
    case VM_RT_CHAR_ARRAYCOPY_NO_EXC:
        return get_generic_rt_support_addr_ia32(f);

    default:
        ABORT("Unexpected helper id");
        return 0;
    }
} //vm_get_rt_support_addr



/**************************************************
 * The following code has to do with the LIL stub inlining project.
 * Modifying it should not affect anything.
 */


// a structure used in memoizing already created stubs
struct TypecheckStubMemoizer {
    Class *clss;  // the class for which this stub is for
    void *fast_checkcast_stub, *fast_instanceof_stub;
    TypecheckStubMemoizer *next;

    static TypecheckStubMemoizer* head;  // head of the list
    static  tl::MemoryPool mem;  // we'll alocate structures from here

    static void* find_stub(Class *c, bool is_checkcast) {
        // search for an existing struct for this class
        for (TypecheckStubMemoizer *t = head;  t != NULL;  t = t->next) {
            if (t->clss == c) {
                return (is_checkcast) ?
                    t->fast_checkcast_stub : t->fast_instanceof_stub;
            }
        }

        return NULL;
    }

    static void add_stub(Class *c, void *stub, bool is_checkcast) {
        // search for an existing struct for this class
      
      TypecheckStubMemoizer *t;
      for (t = head;  t != NULL;  t = t->next) {
            if (t->clss == c)
                break;
        }

        if (t == NULL) {
            // create new structure
            t = (TypecheckStubMemoizer*) mem.alloc(sizeof(TypecheckStubMemoizer));
            t->clss = c;
            t->fast_checkcast_stub = NULL;
            t->fast_instanceof_stub = NULL;
            t->next = head;
            head = t;
        }

        if (is_checkcast) {
            assert(t->fast_checkcast_stub == NULL);
            t->fast_checkcast_stub = stub;
        }
        else {
            assert(t->fast_instanceof_stub == NULL);
            t->fast_instanceof_stub = stub;
        }
    }
};

static StaticInitializer jit_runtime_initializer;
TypecheckStubMemoizer* TypecheckStubMemoizer::head = NULL;
tl::MemoryPool TypecheckStubMemoizer::mem;  // we'll alocate structures from here

/* 03/07/30: temporary interface change!!! */
void *vm_get_rt_support_addr_optimized(VM_RT_SUPPORT f, Class_Handle c) {
    Class *clss = (Class*) c;
    if (clss == NULL)
    {
        return vm_get_rt_support_addr(f);
    }

    switch (f) {
    case VM_RT_CHECKCAST:
            return vm_get_rt_support_addr(f);
        break;
    case VM_RT_INSTANCEOF:
            return vm_get_rt_support_addr(f);
        break;
    case VM_RT_NEW_RESOLVED:
        if (class_is_finalizable(c))
            return getaddress__vm_alloc_java_object_resolved_naked();
        else
            return vm_get_rt_support_addr(f);
        break;
    case VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE:
        if (class_is_finalizable(c))
            return getaddress__vm_alloc_java_object_resolved_using_vtable_and_size_naked();
        else
            return vm_get_rt_support_addr(f);
        break;
    default:
        return vm_get_rt_support_addr(f);
        break;
    }
}


// instead of returning a stub address, this support function returns
// parsed LIL code.
// May return NULL if the stub requested should not be inlined
VMEXPORT LilCodeStub *vm_get_rt_support_stub(VM_RT_SUPPORT f, Class_Handle c) {
    Class *clss = (Class *) c;

    switch (f) {
    case VM_RT_CHECKCAST:
    {
        if (!clss->is_suitable_for_fast_instanceof)
            return NULL;

        return gen_lil_typecheck_stub_specialized(true, true, clss);
    }
    case VM_RT_INSTANCEOF:
    {
        if (!clss->is_suitable_for_fast_instanceof)
            return NULL;

        return gen_lil_typecheck_stub_specialized(false, true, clss);
    }
    default:
        ABORT("Unexpected hepler id");
        return NULL;
    }
}


/*
 * LIL inlining code - end
 **************************************************/
