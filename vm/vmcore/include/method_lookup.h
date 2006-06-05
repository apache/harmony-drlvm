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
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#ifndef _METHOD_LOOKUP_H_
#define _METHOD_LOOKUP_H_

#include "Class.h"


enum VM_Code_Type {
    VM_TYPE_JAVA,
    VM_TYPE_NATIVE_STUB,
    VM_TYPE_UNKNOWN
};


VMEXPORT VM_Code_Type vm_identify_eip(void *addr);
VM_Code_Type           vm_identify_eip_deadlock_free(void *addr);



class Method_Lookup_Table
{
public:
    Method_Lookup_Table();
    ~Method_Lookup_Table();

    unsigned       size()           { return _next_free_entry; }

    CodeChunkInfo *get(unsigned i);
    void           add(CodeChunkInfo *m);

    // Resembles add, but appends the new entry m at the end of the table. The new entry must have a starting address above all entries
    // in the table. This method does not acquire p_meth_addr_table_lock, so insertion must be protected by another lock or scheme.
    void           append_unlocked(CodeChunkInfo *m);

    VMEXPORT CodeChunkInfo *find(void *addr, bool is_ip_past = false);
    CodeChunkInfo *find_deadlock_free(void *addr);

    void           unload_all();

    // An iterator for methods compiled by the specific JIT.
    CodeChunkInfo *get_first_method_jit(JIT *jit);
    CodeChunkInfo *get_next_method_jit(CodeChunkInfo *prev_info);

    // An iterator for all methods regardless of which JIT compiled them.
    CodeChunkInfo *get_first_code_info();
    CodeChunkInfo *get_next_code_info(CodeChunkInfo *prev_info);

#ifdef _DEBUG
    void           dump();
    void           verify();
#endif //_DEBUG

#ifdef VM_STATS
    void           print_stats();
#endif

private:
    void           reallocate(unsigned new_capacity);
    unsigned       find_index(void *addr);

    unsigned        _capacity;
    unsigned        _next_free_entry;
    CodeChunkInfo **_table;
    CodeChunkInfo **_cache;
}; //class Method_Lookup_Table


VMEXPORT extern Method_Lookup_Table *vm_methods;


#endif //_METHOD_LOOKUP_H_
