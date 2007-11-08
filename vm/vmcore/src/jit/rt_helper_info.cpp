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

#define LOG_DOMAIN "vm.helpers"
#include "cxxlog.h"
#include "jit_runtime_support.h"
#include <map>

#ifndef _WIN32
    #define strcmpi strcasecmp
#endif

struct JIT_RT_Function_Entry {
    VM_RT_SUPPORT  function;
    const char    *name;
    HELPER_INTERRUPTIBILITY_KIND i_kind;
    HELPER_CALLING_CONVENTION cc_kind;
    int            number_of_args;
};

typedef std::map<VM_RT_SUPPORT, JIT_RT_Function_Entry*> HelperInfoMap;

static JIT_RT_Function_Entry _jit_rt_function_entries_base[] = {
    {VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE, "VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_NEW_VECTOR_USING_VTABLE,            "VM_RT_NEW_VECTOR_USING_VTABLE",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_MULTIANEWARRAY_RESOLVED,            "VM_RT_MULTIANEWARRAY_RESOLVED",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_CDECL,                8},
    {VM_RT_LDC_STRING,                         "VM_RT_LDC_STRING",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},

    {VM_RT_THROW,                              "VM_RT_THROW",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_THROW_LAZY,                         "VM_RT_THROW_LAZY",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_DRL,                  8},
    {VM_RT_IDX_OUT_OF_BOUNDS,                  "VM_RT_IDX_OUT_OF_BOUNDS",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              0},
    {VM_RT_NULL_PTR_EXCEPTION,                 "VM_RT_NULL_PTR_EXCEPTION",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              0},
    {VM_RT_DIVIDE_BY_ZERO_EXCEPTION,           "VM_RT_DIVIDE_BY_ZERO_EXCEPTION",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              0},
    {VM_RT_ARRAY_STORE_EXCEPTION,              "VM_RT_ARRAY_STORE_EXCEPTION",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              0},
    {VM_RT_THROW_LINKING_EXCEPTION,            "VM_RT_THROW_LINKING_EXCEPTION",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              0},
    {VM_RT_THROW_SET_STACK_TRACE,              "VM_RT_THROW_SET_STACK_TRACE",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              1},

    {VM_RT_MONITOR_ENTER,                      "VM_RT_MONITOR_ENTER",
            INTERRUPTIBLE_SOMETIMES,           CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_MONITOR_ENTER_NON_NULL,             "VM_RT_MONITOR_ENTER_NON_NULL",
            INTERRUPTIBLE_SOMETIMES,           CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_MONITOR_EXIT,                       "VM_RT_MONITOR_EXIT",
            INTERRUPTIBLE_SOMETIMES,           CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_MONITOR_EXIT_NON_NULL,              "VM_RT_MONITOR_EXIT_NON_NULL",
            INTERRUPTIBLE_SOMETIMES,           CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_MONITOR_ENTER_STATIC,               "VM_RT_MONITOR_ENTER_STATIC",
            INTERRUPTIBLE_SOMETIMES,           CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_MONITOR_EXIT_STATIC,                "VM_RT_MONITOR_EXIT_STATIC",
            INTERRUPTIBLE_SOMETIMES,           CALLING_CONVENTION_STDCALL,              1},

    {VM_RT_CHECKCAST,                          "VM_RT_CHECKCAST",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_INSTANCEOF,                         "VM_RT_INSTANCEOF",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_AASTORE,                            "VM_RT_AASTORE",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              3},
    {VM_RT_AASTORE_TEST,                       "VM_RT_AASTORE_TEST",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_GET_INTERFACE_VTABLE_VER0,          "VM_RT_GET_INTERFACE_VTABLE_VER0",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},

    {VM_RT_INITIALIZE_CLASS,                   "VM_RT_INITIALIZE_CLASS",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              1},

    {VM_RT_GC_HEAP_WRITE_REF,                  "VM_RT_GC_HEAP_WRITE_REF",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_CDECL,                3},
    {VM_RT_GC_SAFE_POINT,                      "VM_RT_GC_SAFE_POINT",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              0},
    {VM_RT_GC_GET_TLS_BASE,                    "VM_RT_GET_TLS_BASE",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              0},

    {VM_RT_JVMTI_METHOD_ENTER_CALLBACK,        "VM_RT_JVMTI_METHOD_ENTER_CALLBACK",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_JVMTI_METHOD_EXIT_CALLBACK,         "VM_RT_JVMTI_METHOD_EXIT_CALLBACK",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_JVMTI_FIELD_ACCESS_CALLBACK,        "VM_RT_JVMTI_FIELD_ACCESS__CALLBACK",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              4},
    {VM_RT_JVMTI_FIELD_MODIFICATION_CALLBACK,  "VM_RT_JVMTI_FIELD_MODIFICATION_CALLBACK",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              5},

    {VM_RT_NEWOBJ_WITHRESOLVE,                      "VM_RT_NEWOBJ_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         2},
    {VM_RT_NEWARRAY_WITHRESOLVE,                    "VM_RT_NEWARRAY_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         3},
    {VM_RT_GET_NONSTATIC_FIELD_OFFSET_WITHRESOLVE,  "VM_RT_GET_NONSTATIC_FIELD_OFFSET_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         2},
    {VM_RT_GET_STATIC_FIELD_ADDR_WITHRESOLVE,       "VM_RT_GET_STATIC_FIELD_ADDR_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         2},
    {VM_RT_CHECKCAST_WITHRESOLVE,                   "VM_RT_CHECKCAST_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         3},
    {VM_RT_INSTANCEOF_WITHRESOLVE,                  "VM_RT_INSTANCEOF_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         3},
    {VM_RT_GET_INVOKESTATIC_ADDR_WITHRESOLVE,       "VM_RT_GET_INVOKESTATIC_ADDR_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         2},
    {VM_RT_GET_INVOKEINTERFACE_ADDR_WITHRESOLVE,    "VM_RT_GET_INVOKEINTERFACE_ADDR_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         3},
    {VM_RT_GET_INVOKEVIRTUAL_ADDR_WITHRESOLVE,      "VM_RT_GET_INVOKEVIRTUAL_ADDR_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         3},
    {VM_RT_GET_INVOKE_SPECIAL_ADDR_WITHRESOLVE,     "VM_RT_GET_INVOKE_SPECIAL_ADDR_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         2},
    {VM_RT_INITIALIZE_CLASS_WITHRESOLVE,           "VM_RT_INITIALIZE_CLASS_WITHRESOLVE",
            INTERRUPTIBLE_ALWAYS,                   CALLING_CONVENTION_STDCALL,         2},


    {VM_RT_F2I,                                "VM_RT_F2I",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_F2L,                                "VM_RT_F2L",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_D2I,                                "VM_RT_D2I",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_D2L,                                "VM_RT_D2L",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              1},
    {VM_RT_LSHL,                               "VM_RT_LSHL",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_LSHR,                               "VM_RT_LSHR",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_LUSHR,                              "VM_RT_LUSHR",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_LMUL,                               "VM_RT_LMUL",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
#ifdef VM_LONG_OPT
    {VM_RT_LMUL_CONST_MULTIPLIER,              "VM_RT_LMUL_CONST_MULTIPLIER",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
#endif // VM_LONG_OPT
    {VM_RT_LREM,                               "VM_RT_LREM",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_LDIV,                               "VM_RT_LDIV",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_ULDIV,                              "VM_RT_ULDIV",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_CONST_LDIV,                         "VM_RT_CONST_LDIV",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_CONST_LREM,                         "VM_RT_CONST_LREM",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_IMUL,                               "VM_RT_IMUL",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_IREM,                               "VM_RT_IREM",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_IDIV,                               "VM_RT_IDIV",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_FREM,                               "VM_RT_FREM",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_FDIV,                               "VM_RT_FDIV",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_DREM,                               "VM_RT_DREM",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},
    {VM_RT_DDIV,                               "VM_RT_DDIV",
            INTERRUPTIBLE_NEVER,               CALLING_CONVENTION_STDCALL,              2},

    {VM_RT_CHAR_ARRAYCOPY_NO_EXC,              "VM_RT_CHAR_ARRAYCOPY_NO_EXC",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              5},

    {VM_RT_WRITE_BARRIER_FASTCALL,             "VM_RT_WRITE_BARRIER_FASTCALL",
            INTERRUPTIBLE_ALWAYS,              CALLING_CONVENTION_STDCALL,              2},
};

static JIT_RT_Function_Entry *jit_rt_function_entries = &(_jit_rt_function_entries_base[0]);
static int num_jit_rt_function_entries = sizeof(_jit_rt_function_entries_base) / sizeof(_jit_rt_function_entries_base[0]);

static HelperInfoMap* init_helper_map() {
    // TODO: Use proper MM
    HelperInfoMap *map = new HelperInfoMap();
    for (int i = 0;  i < num_jit_rt_function_entries;  i++) {
        VM_RT_SUPPORT hid = jit_rt_function_entries[i].function;
        assert(map->find(hid) == map->end());
        map->insert(HelperInfoMap::value_type(hid, jit_rt_function_entries + i));
    }
    assert(map->size() == num_jit_rt_function_entries);
    return map;
}

static HelperInfoMap *helper_map = init_helper_map();

VMEXPORT 
const char* vm_helper_get_name(VM_RT_SUPPORT id) {
    HelperInfoMap::const_iterator it = helper_map->find(id);
    if (helper_map->end() != it) {
        assert(it->second);
        return it->second->name;
    } else {
        ASSERT(VM_RT_UNKNOWN == id, "Unexpected helper id " << id);
        return "unknown";
    }
}

VMEXPORT 
VM_RT_SUPPORT vm_helper_get_by_name(const char* name) {
    for (int i = 0;  i < num_jit_rt_function_entries;  i++) {
        if( !strcmpi(name, jit_rt_function_entries[i].name)) {
            return jit_rt_function_entries[i].function;
        }
    }
    return VM_RT_UNKNOWN;
}

VMEXPORT 
HELPER_INTERRUPTIBILITY_KIND vm_helper_get_interruptibility_kind(VM_RT_SUPPORT id)
{
    HelperInfoMap::const_iterator it = helper_map->find(id);
    if (helper_map->end() != it) {
        assert(it->second);
        return it->second->i_kind;
    } else {
        ASSERT(false, "Unexpected helper id " << id);
        return INTERRUPTIBLE_SOMETIMES;
    }
}

VMEXPORT 
HELPER_CALLING_CONVENTION vm_helper_get_calling_convention(VM_RT_SUPPORT id) 
{
    HelperInfoMap::const_iterator it = helper_map->find(id);
    if (helper_map->end() != it) {
        assert(it->second);
        return it->second->cc_kind;
    } else {
        ASSERT(false, "Unexpected helper id " << id);
        return CALLING_CONVENTION_STDCALL;
    }
}

VMEXPORT 
int vm_helper_get_numargs(VM_RT_SUPPORT id)
{
    HelperInfoMap::const_iterator it = helper_map->find(id);
    if (helper_map->end() != it) {
        assert(it->second);
        return it->second->number_of_args;
    } else {
        ASSERT(false, "Unexpected helper id " << id);
        return 0;
    }
}
