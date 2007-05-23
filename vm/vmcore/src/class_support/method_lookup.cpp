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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.1.2.1.2.3 $
 */  



#define LOG_DOMAIN "vm.methods"
#include "cxxlog.h"

#include "environment.h"
#include <assert.h>
#include "lock_manager.h"
#include "nogc.h"
#include "vm_stats.h"
#include "cci.h"
#include "class_member.h"

#include "method_lookup.h"
#include "port_threadunsafe.h"

#define EIP_CACHE_SIZE 1024
#define EIP_ALIGNMENT     4

Method_Lookup_Table::Method_Lookup_Table()
{
    _next_free_entry = 0;
    _capacity        = 0;
    _table           = 0;
    _cache           = (CodeChunkInfo **)STD_MALLOC(EIP_CACHE_SIZE * sizeof(CodeChunkInfo *));
    assert (_cache);
    memset(_cache, 0, (EIP_CACHE_SIZE * sizeof(CodeChunkInfo *)));
    reallocate(511);
} //Method_Lookup_Table::Method_Lookup_Table



Method_Lookup_Table::~Method_Lookup_Table()
{
    if (_table != NULL) {
        STD_FREE((void*)_table);
    }
    if (_cache != NULL) {
        STD_FREE((void*)_cache);
    }
} //Method_Lookup_Table::~Method_Lookup_Table



void Method_Lookup_Table::reallocate(unsigned new_capacity)
{
    CodeChunkInfo **new_table = (CodeChunkInfo **)STD_MALLOC(new_capacity * sizeof(CodeChunkInfo *));
    assert(new_table != NULL);
    assert(_next_free_entry <= _capacity);
    assert(_next_free_entry < new_capacity);
    memcpy(new_table, _table, (_next_free_entry * sizeof(CodeChunkInfo *)));
    if (_table != NULL) {
        STD_FREE(_table);
    }
    _table    = new_table;
    _capacity = new_capacity;
} //Method_Lookup_Table::reallocate



void Method_Lookup_Table::add(CodeChunkInfo *m)
{
    Global_Env * vm_env = VM_Global_State::loader_env;

    vm_env->p_meth_addr_table_lock->_lock();

    void *code_block_addr = m->get_code_block_addr();

    // If the table is full, allocate more memory.
    if (_next_free_entry >= _capacity) {
        reallocate(2 * _capacity);
    }

    // Figure out the index idx where the new entry should go
    unsigned idx = find_index(code_block_addr);
    // Shift entries starting at idx one slot to the right, then insert the new entry at idx
    for (unsigned i = _next_free_entry;  i > idx;  i--) {
        _table[i] = _table[i-1];
    }
    _table[idx] = m;
    _next_free_entry++;

    vm_env->p_meth_addr_table_lock->_unlock();
} //Method_Lookup_Table::add

#define USE_METHOD_LOOKUP_CACHE

void Method_Lookup_Table::remove(CodeChunkInfo *m)
{
    Global_Env * vm_env = VM_Global_State::loader_env;

    void* addr = m->get_code_block_addr();
    if (addr == NULL) {
        return;
    }

#ifdef USE_METHOD_LOOKUP_CACHE
    // First remove from cache.  
    for (unsigned i = 0; i < EIP_CACHE_SIZE; i++){
        if (_cache[i]){
            void *guess_start = _cache[i]->get_code_block_addr();
            void *guess_end   = ((char *)_cache[i]->get_code_block_addr()) + _cache[i]->get_code_block_size();
            if ((addr >= guess_start) && (addr < guess_end)) {
                _cache[i] = NULL;
            }
        }
    }
#endif //USE_METHOD_LOOKUP_CACHE

    vm_env->p_meth_addr_table_lock->_lock();

    unsigned L = 0, R = _next_free_entry;
    while (L < R) {
        unsigned M = (L + R) / 2;
        CodeChunkInfo *m = _table[M];
        void  *code_block_addr = m->get_code_block_addr();
        size_t code_block_size = m->get_code_block_size();
        void  *code_end_addr   = (void *)((char *)code_block_addr + code_block_size);

        if (addr < code_block_addr) {
            R = M;
        } else if (addr >= code_end_addr) {
            // Should this be (addr >= code_end_addr)?
            L = M + 1;
        } else {
            // Shift entries starting at idx one slot to the right, then insert the new entry at idx
            for (unsigned i = M;  i <  (_next_free_entry - 1);  i++) {
                _table[i] = _table[i+1];
            }
            _next_free_entry--;

            vm_env->p_meth_addr_table_lock->_unlock();
            return;
        }
    }

    vm_env->p_meth_addr_table_lock->_unlock();
} //Method_Lookup_Table::remove


void Method_Lookup_Table::append_unlocked(CodeChunkInfo *m)
{
    void  *code_block_addr = m->get_code_block_addr();
    size_t code_block_size = m->get_code_block_size();
    void  *code_end_addr   = (void *)((char *)code_block_addr + code_block_size);

    // If the table is full, allocate more memory.
    if (_next_free_entry >= _capacity) {
        reallocate(2 * _capacity);
    }

    if (_next_free_entry > 0) {
        unsigned last_entry = (_next_free_entry - 1);
        CodeChunkInfo *last = _table[last_entry];
        void  *last_code_addr = last->get_code_block_addr();
        size_t last_size      = last->get_code_block_size();
        void  *last_end_addr  = (void *)((char *)last_code_addr + last_size);
        if (code_block_addr < last_end_addr) {
            printf("Method_Lookup_Table::append_unlocked: New entry [%p..%p] is before last table entry [%p..%p]\n",
                   code_block_addr, code_end_addr, last_code_addr, last_end_addr);
            ABORT("New entry is before last table entry"); 
        }
    }

    _table[_next_free_entry] = m;
    _next_free_entry++;
} //Method_Lookup_Table::append_unlocked



unsigned Method_Lookup_Table::find_index(void *addr)
{
    unsigned L = 0, R = _next_free_entry;
    while (L < R) {
        unsigned M = (L + R) / 2;
        CodeChunkInfo *m = _table[M];
        void  *code_block_addr = m->get_code_block_addr();
        size_t code_block_size = m->get_code_block_size();
        void  *code_end_addr   = (void *)((char *)code_block_addr + code_block_size);
        if (addr < code_block_addr) {
            R = M;
        } else if (addr >= code_end_addr) {
            L = M + 1;
        } else {
            return M;
        }
    }
    return L;
} //Method_Lookup_Table::find_index



CodeChunkInfo *Method_Lookup_Table::find(void *addr, bool is_ip_past)
{
    Global_Env * vm_env = VM_Global_State::loader_env;

    if (addr == NULL) {
        return NULL;
    }
    if (is_ip_past) {
        addr = (void *)((Byte*)addr - 1);
    }

#ifdef USE_METHOD_LOOKUP_CACHE
    // First try the cache.  There's no need for a lock.
    unsigned cache_idx = (unsigned)(((POINTER_SIZE_INT)addr / EIP_ALIGNMENT) % EIP_CACHE_SIZE);
    CodeChunkInfo *guess = _cache[cache_idx];
    if (guess != NULL) {
        void *guess_start = guess->get_code_block_addr();
        void *guess_end   = ((char *)guess->get_code_block_addr()) + guess->get_code_block_size();
        if ((addr >= guess_start) && (addr < guess_end)) {
#ifdef VM_STATS
            UNSAFE_REGION_START
            VM_Statistics::get_vm_stats().num_method_lookup_cache_hit++;
            UNSAFE_REGION_END
#endif //VM_STATS
            return guess;
        }
    }
#endif //USE_METHOD_LOOKUP_CACHE
#ifdef VM_STATS
    VM_Statistics::get_vm_stats().num_method_lookup_cache_miss++;
#endif //VM_STATS

    vm_env->p_meth_addr_table_lock->_lock();

    unsigned L = 0, R = _next_free_entry;
    while (L < R) {
        unsigned M = (L + R) / 2;
        CodeChunkInfo *m = _table[M];
        void  *code_block_addr = m->get_code_block_addr();
        size_t code_block_size = m->get_code_block_size();
        void  *code_end_addr   = (void *)((char *)code_block_addr + code_block_size);

        if (addr < code_block_addr) {
            R = M;
        } else if (addr >= code_end_addr) {
            // Should this be (addr >= code_end_addr)?
            L = M + 1;
        } else {
#ifdef USE_METHOD_LOOKUP_CACHE
            _cache[cache_idx] = m;
#endif //USE_METHOD_LOOKUP_CACHE
            vm_env->p_meth_addr_table_lock->_unlock();
            return m;
        }
    }

    vm_env->p_meth_addr_table_lock->_unlock();
    return NULL;
} //Method_Lookup_Table::find



CodeChunkInfo *Method_Lookup_Table::find_deadlock_free(void *addr)
{
    Global_Env * vm_env = VM_Global_State::loader_env;

    bool ok = vm_env->p_meth_addr_table_lock->_lock_or_null();             // vvv
    if (ok) {
        // We acquired the lock.  Can use the fast lookup.
        CodeChunkInfo *m = find(addr);
        vm_env->p_meth_addr_table_lock->_unlock_or_null();                 // ^^^
        return m;
    } else {
        // We failed to acquire the lock.  Use slow linear search.
        // The linear search is safe even is someone else is adding a method
        // because of the way the table is modified:
        // 1. If necessary, the table is reallocated.  This operation is
        //    atomic, so we never see a partially initialized table.
        // 2. Space is made for the new method by shifting all methods
        //    with higher addresses by 1.  The shift is done from the right,
        //    so we never lose an element in the linear search from the left.
        //    We could see the same method twice, but this is safe.
        for (unsigned i = 0;  i < _next_free_entry;  i++) {
            CodeChunkInfo *m = _table[i];
            void  *code_block_addr = m->get_code_block_addr();
            size_t code_block_size = m->get_code_block_size();
            void  *code_end_addr   = (void *)((char *)code_block_addr + code_block_size);
            if ((addr >= code_block_addr) && (addr <= code_end_addr)) {
                return m;
            }
        }
        return NULL;
    }
} //Method_Lookup_Table::find_deadlock_free



void Method_Lookup_Table::unload_all()
{
} //Method_Lookup_Table::unload_all

#ifdef VM_STATS
void Method_Lookup_Table::print_stats()
{
    size_t code_block_size     = 0;
    size_t jit_info_block_size = 0;
    unsigned i;
    int num;

    if (vm_print_total_stats_level > 1) {
        num = 0;
        printf("Methods throwing exceptions:\n");
        for(i = 0;  i < _next_free_entry;  i++) {
            CodeChunkInfo *m = _table[i];
            if(m->num_throws) {
                num++;
                printf("%8" FMT64 "u :::: %s.%s%s\n",
                       m->num_throws,
                       m->get_method()->get_class()->get_name()->bytes,
                       m->get_method()->get_name()->bytes,
                       m->get_method()->get_descriptor()->bytes);
            }
        }
        printf("(A total of %d methods)\n", num);

        num = 0;
        printf("Methods catching exceptions:\n");
        for(i = 0;  i < _next_free_entry;  i++) {
            CodeChunkInfo *m = _table[i];
            code_block_size     += m->_code_block_size;
            jit_info_block_size += m->_jit_info_block_size;
            if(m->num_catches) {
                num++;
                printf("%8" FMT64 "u :::: %s.%s%s\n",
                       m->num_catches,
                       m->get_method()->get_class()->get_name()->bytes,
                       m->get_method()->get_name()->bytes,
                       m->get_method()->get_descriptor()->bytes);
            }
        }
        printf("(A total of %d methods)\n", num);
    }

    if (vm_print_total_stats_level > 2) {
        num = 0;
        printf("Methods unwinds (GC):\n");
        for(i = 0;  i < _next_free_entry;  i++) {
            CodeChunkInfo *m = _table[i];
            if(m->num_unwind_java_frames_gc) {
                num++;
                printf("%8" FMT64 "u :::: %s.%s%s\n",
                       m->num_unwind_java_frames_gc,
                       m->get_method()->get_class()->get_name()->bytes,
                       m->get_method()->get_name()->bytes,
                       m->get_method()->get_descriptor()->bytes);
            }
        }
        printf("(A total of %d methods)\n", num);

        num = 0;
        printf("Methods unwinds (non-GC):\n");
        for(i = 0;  i < _next_free_entry;  i++) {
            CodeChunkInfo *m = _table[i];
            if(m->num_unwind_java_frames_non_gc) {
                num++;
                printf("%8" FMT64 "u :::: %s.%s%s\n",
                       m->num_unwind_java_frames_non_gc,
                       m->get_method()->get_class()->get_name()->bytes,
                       m->get_method()->get_name()->bytes,
                       m->get_method()->get_descriptor()->bytes);
            }
        }
        printf("(A total of %d methods)\n", num);
    }

    printf("%11d ::::Total size of code blocks\n", (unsigned) code_block_size);
    printf("%11d ::::          JIT info blocks\n", (unsigned) jit_info_block_size);
} //Method_Lookup_Table::print_stats
#endif

CodeChunkInfo *Method_Lookup_Table::get_first_method_jit(JIT *jit)
{
    for (unsigned i = 0;  i < _next_free_entry;  i++) {
        CodeChunkInfo *m = _table[i];
        if (m && m->get_jit() == jit) {
            return m;
        }
    }
    return 0;
} //Method_Lookup_Table::get_first_method_jit



CodeChunkInfo *Method_Lookup_Table::get_next_method_jit(CodeChunkInfo *prev_info)
{
    unsigned idx = find_index(prev_info->get_code_block_addr());
    JIT *jit = prev_info->get_jit();

    for (unsigned i = idx + 1;  i < _next_free_entry;  i++) {
        CodeChunkInfo *m = _table[i];
        if(m && m->get_jit() == jit) {
            return m;
        }
    }
    return 0;
} //Method_Lookup_Table::get_next_method_jit



CodeChunkInfo *Method_Lookup_Table::get_first_code_info()
{
    for (unsigned i = 0;  i < _next_free_entry;  i++) {
        CodeChunkInfo *m = _table[i];
        if (m != NULL) {
            return m;
        }
    }
    return NULL;
} //Method_Lookup_Table::get_first_code_info



CodeChunkInfo *Method_Lookup_Table::get_next_code_info(CodeChunkInfo *prev_info)
{
    unsigned idx = find_index(prev_info->get_code_block_addr());
    for (unsigned i = idx + 1;  i < _next_free_entry;  i++) {
        CodeChunkInfo *m = _table[i];
        if (m != NULL) {
            return m;
        }
    }
    return NULL;
} //Method_Lookup_Table::get_next_code_info



CodeChunkInfo *Method_Lookup_Table::get(unsigned i) { 
    if (i < _next_free_entry) {
        return _table[i];
    } else {
        return NULL;
    }
}   // Method_Lookup_Table::get




VM_Code_Type vm_identify_eip(void *addr)
{
    Global_Env *env = VM_Global_State::loader_env;
    if (NULL == env || NULL == env->vm_methods)
        return VM_TYPE_UNKNOWN;
    CodeChunkInfo *m = env->vm_methods->find(addr);
    if (m == NULL) {
        return VM_TYPE_UNKNOWN;
    }

    if (m->get_method()->is_native()) {
        return VM_TYPE_NATIVE_STUB;
    } else {
        return VM_TYPE_JAVA;
    }
} //vm_identify_eip



VM_Code_Type vm_identify_eip_deadlock_free(void *addr)
{
    Global_Env *env = VM_Global_State::loader_env;
    CodeChunkInfo *m = env->vm_methods->find_deadlock_free(addr);
    if (m == NULL) {
        return VM_TYPE_UNKNOWN;
    }

    if (m->get_method()->is_native()) {
        return VM_TYPE_NATIVE_STUB;
    } else {
        return VM_TYPE_JAVA;
    }
} //vm_identify_eip_deadlock_free
