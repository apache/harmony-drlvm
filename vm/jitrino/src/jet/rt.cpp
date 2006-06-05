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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.5.12.2.4.4 $
 */
 
 /**
  * @file
  * @brief Contains platform-independent routines for runtime support.
  */
#include "compiler.h"
#include "trace.h"

#include <jit_import.h>

namespace Jitrino {
namespace Jet {

MethodInfoBlock::MethodInfoBlock(void)
{
    m_data = NULL;
    //
    rt_inst_addrs = NULL;
    //
    rt_header = &m_header;
    rt_header->m_code_start = NULL;
}

void MethodInfoBlock::init(unsigned bc_size, unsigned stack_max, 
                       unsigned num_locals, unsigned in_slots,
                       unsigned flags)
{

    //
    rt_header->m_bc_size = bc_size;
    rt_header->m_num_locals = num_locals;
    rt_header->m_max_stack_depth = stack_max;
    rt_header->m_in_slots = in_slots;
    rt_header->m_flags = flags;

    // All the values must be initialized *before* get_dyn_size() !
    unsigned dyn_size = get_dyn_size();
    m_data = new char[dyn_size];
    memset(m_data, 0, dyn_size);
    rt_inst_addrs = (const char**)m_data;
    rt_lazy_refs = NULL;
}

void MethodInfoBlock::save(char * to)
{
    memset(to, 0, get_total_size());
    memcpy(to, rt_header, get_hdr_size());
    memcpy(to +  get_hdr_size(), m_data, get_dyn_size());
    rt_inst_addrs = (const char**)(to + get_hdr_size());
    if (rt_header->m_num_refs != 0) {
        rt_lazy_refs = rt_inst_addrs + rt_header->m_bc_size;
    }
    else {
        rt_lazy_refs = NULL;
    }
}

const char * MethodInfoBlock::get_ip(unsigned pc) const
{
    assert(pc < rt_header->m_bc_size);
    return (char*)rt_inst_addrs[pc];
}

unsigned MethodInfoBlock::get_pc(const char * ip) const
{
    const char ** data = rt_inst_addrs;
    //
    // Binary search performed, in its classical form - with 'low' 
    // and 'high'pointers, and plus additional steps specific to the nature 
    // of the data stored.
    // The array where the search is performed, may (and normally does) have
    // repetitive values: i.e. [ABBBCDEEFFF..]. Each region with the same 
    // value relate to the same byte code instruction.
    
    int l = 0;
    int max_idx = (int)rt_header->m_bc_size-1;
    int r = max_idx;
    int m = 0;
    const char * val = NULL;
    //
    // Step 1.
    // Find first element which is above or equal to the given IP.
    //
    while (l<=r) {
        m = (r+l)/2;
        val = *(data+m);
        assert(val != NULL);
        if (ip<val) {
            r = m-1;
        }
        else if(ip>val) {
            l = m+1;
        }
        else {
            break;
        }
    }
    
    //here, 'val'  is '*(data+m)'
    
    // Step 2.
    // If we found an item which is less or equal than key, then this step
    // is omitted.
    // If an item greater than key found, we need small shift to the previous
    // item: 
    // [ABBB..] if we find any 'B', which is > key, then we need to step back
    // to 'A'.
    //
    if (val > ip) {
        // Find very first item of the same IP value (very first 'B' in the 
        // example) - this is the beginning of the bytecode instruction 
        while (m && val == *(data+m-1)) {
            --m;
        }
        // here, 'm' points to the first 'B', and 'val' has its value ...
        if (m) {
            --m;
        }
        // ... and here 'm' points to last 'A'
        val = *(data+m);
    }
    
    // Step 3.
    // Find very first item in the range - this is the start of the bytecode 
    // instruction
    while (m && val == *(data+m-1)) {
        --m;
    }
    return m;
}

bool rt_check_method(JIT_Handle jit, Method_Handle method)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    return MethodInfoBlock::is_valid_data(pinfo);
}

void rt_bc2native(JIT_Handle jit, Method_Handle method, unsigned short bc_pc,
                  void ** pip)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    if (!MethodInfoBlock::is_valid_data(pinfo)) {
        assert(false);
        return;
    }

    MethodInfoBlock rtinfo;
    rtinfo.load(pinfo);
    if (bc_pc >= rtinfo.get_bc_size()) {
        assert(false);
        return;
    };
    *pip = (void*)rtinfo.get_ip(bc_pc); 
    if (rtinfo.get_flags() & DBG_TRACE_RT) {
        dbg("rt.bc2ip: @ %u => %p\n", bc_pc, *pip);
    }
}

void rt_native2bc(JIT_Handle jit, Method_Handle method, const void * ip,
                  unsigned short * pbc_pc)
{
    char * pinfo = (char*)method_get_info_block_jit(method, jit);
    if (!MethodInfoBlock::is_valid_data(pinfo)) {
        assert(false);
        return;
    }
    MethodInfoBlock rtinfo(pinfo);
    *pbc_pc = (unsigned short)rtinfo.get_pc((char*)ip);

    if (rtinfo.get_flags() & DBG_TRACE_RT) {
        dbg("rt.ip2bc: @ 0x%p => %d\n", ip, *pbc_pc);
    }
}

};};    // ~namespace Jitrino::Jet
