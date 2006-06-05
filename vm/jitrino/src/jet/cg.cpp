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
 * @version $Revision: 1.4.12.3.4.4 $
 */
#include "compiler.h"
#include "trace.h"

#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>

#include <jit_import.h>

/**
 * @file
 * @brief Platform-independent utilities for code generator.
 */

namespace Jitrino {
namespace Jet {

char *  VMRuntimeConsts::rt_helper_throw = NULL;
char *  VMRuntimeConsts::rt_helper_throw_out_of_bounds = NULL;
char *  VMRuntimeConsts::rt_helper_throw_npe = NULL;
char *  VMRuntimeConsts::rt_helper_throw_linking_exc = NULL;
char *  VMRuntimeConsts::rt_helper_throw_div_by_zero_exc = NULL;

char *  VMRuntimeConsts::rt_helper_monitor_enter = NULL;
char *  VMRuntimeConsts::rt_helper_monitor_exit = NULL;
char *  VMRuntimeConsts::rt_helper_monitor_enter_static = NULL;
char *  VMRuntimeConsts::rt_helper_monitor_exit_static = NULL;

char *  VMRuntimeConsts::rt_helper_ldc_string = NULL;
char *  VMRuntimeConsts::rt_helper_new = NULL;
char *  VMRuntimeConsts::rt_helper_new_array = NULL;
char *  VMRuntimeConsts::rt_helper_init_class = NULL;
char *  VMRuntimeConsts::rt_helper_aastore = NULL;
char *  VMRuntimeConsts::rt_helper_multinewarray = NULL;
char *  VMRuntimeConsts::rt_helper_get_vtable = NULL;
char *  VMRuntimeConsts::rt_helper_checkcast = NULL;
char *  VMRuntimeConsts::rt_helper_instanceof = NULL;
char *  VMRuntimeConsts::rt_helper_ti_method_exit = NULL;
char *  VMRuntimeConsts::rt_helper_ti_method_enter = NULL;
char *  VMRuntimeConsts::rt_helper_gc_safepoint = NULL;
char *  VMRuntimeConsts::rt_helper_get_thread_suspend_ptr = NULL;

unsigned VMRuntimeConsts::rt_array_length_offset = NOTHING;
unsigned VMRuntimeConsts::rt_suspend_req_flag_offset = NOTHING;


void Compiler::ip(char * _ip)
{
    m_codeStream.ip(_ip);
    if (m_patchID != -1) {
        CodePatchItem& cpi = m_patchItems[m_patchID];
        cpi.instr_len = m_codeStream.ipoff() - m_curr_bb->ipoff - cpi.bboff;
        if (cpi.instr_len == 2 || cpi.instr_len == 3) {
            assert(CHAR_MIN <= cpi.target_offset && 
                   cpi.target_offset <= CHAR_MAX);
        }
        m_patchID = -1;
    }
}

unsigned Compiler::reg_patch(void)
{
    assert(m_patchID == -1);

    CodePatchItem cpi;
    cpi.pc = m_pc;
    cpi.bb = m_curr_bb->start;
    cpi.bboff = m_codeStream.ipoff() - m_curr_bb->ipoff;
    cpi.target_ip = NULL;
    cpi.target_pc = NOTHING;
    cpi.relative = true;
    cpi.target_offset = 0;
    cpi.data = 0;
    cpi.data_addr = NULL;
    cpi.is_table_switch = false;
    
    m_patchID = m_patchItems.size();
    m_patchItems.push_back(cpi);
    return (unsigned)m_patchID;
}

void Compiler::reg_data_patch(RefType type, unsigned cp_idx)
{
    unsigned pid = reg_patch();
    assert(pid == m_patchItems.size()-1 && pid == (unsigned)m_patchID);
	pid=pid; // make compiler happy

    CodePatchItem& cpi = m_patchItems[m_patchID];
    cpi.data = ref_key(type, cp_idx);
    m_lazyRefs[cpi.data] = NULL;
}

unsigned Compiler::reg_patch(unsigned target_pc)
{
    unsigned pid = reg_patch();
    assert(pid == m_patchItems.size()-1 && pid == (unsigned)m_patchID);
    CodePatchItem& cpi = m_patchItems[m_patchID];
    cpi.target_pc = target_pc;
    return pid;
}

unsigned Compiler::reg_patch(unsigned target_pc, bool relative)
{
    unsigned pid = reg_patch();
    assert(pid == m_patchItems.size()-1 && pid == (unsigned)m_patchID);
    CodePatchItem& cpi = m_patchItems[m_patchID];
    cpi.target_pc = target_pc;
    cpi.relative = relative;
    return pid;
}

unsigned Compiler::reg_patch(const void * target_ip)
{
    unsigned pid = reg_patch();
    assert(pid == m_patchItems.size()-1 && pid == (unsigned)m_patchID);
    CodePatchItem& cpi = m_patchItems[m_patchID];
    cpi.target_ip = (char*)target_ip;
    return pid;
}

void Compiler::patch_set_target(unsigned pid, char * _ip)
{
    CodePatchItem& cpi = m_patchItems[pid];
    assert(cpi.bb == m_curr_bb->start);
    
    // the direct addresses in the code stream can not be used as targets, 
    // as the stream's buffer may move, making the addresses invalid
    // use patch_set_target(, unsigned ipoff) instead ...
#ifdef _DEBUG    
    if (m_codeStream.data() <= _ip && 
        _ip < m_codeStream.data() + m_codeStream.size()) {
        // _ip lies into the m_codeStream ...
        // ... it's only allowed when both the CodePatchItem and _ip 
        // belong to the same basic block
        // As we're currently generating m_curr_bb, then only need to check
        // that _ip lies after the beginning of the current BB - and thus 
        // belong to the current BB
        assert(m_codeStream.data() + m_curr_bb->ipoff <= _ip);
    }
#endif
    char * inst_ip = m_codeStream.data() + m_curr_bb->ipoff + cpi.bboff;
    cpi.target_offset = _ip - inst_ip;
}

void Compiler::patch_set_target(unsigned pid, unsigned _ipoff)
{
    CodePatchItem& cpi = m_patchItems[pid];
    assert(cpi.bb == m_curr_bb->start);
    cpi.target_offset = _ipoff - (m_curr_bb->ipoff + cpi.bboff);
}

void Compiler::reg_table_switch_patch(void)
{
    reg_patch();
    CodePatchItem& cpi = m_patchItems[m_patchID];
    cpi.is_table_switch = true;
}

void Compiler::cg_patch_code(void)
{
    unsigned didx = 0;
    char ** dstart = (char**)m_infoBlock.get_lazy_refs();
    
    for (unsigned i=0, n=m_patchItems.size(); i<n; i++) {
        CodePatchItem& cpi = m_patchItems[i];
        const BBInfo& bbinfo = m_bbs[cpi.bb];
        char * bc_start_ip = m_vmCode + bbinfo.ipoff;
        cpi.ip =  bc_start_ip + cpi.bboff;
        
        if (cpi.is_table_switch) {
            const JInst& jinst = m_insts[cpi.pc];
            assert(jinst.opcode == OPCODE_TABLESWITCH);
            // Allocate table
            unsigned targets = jinst.get_num_targets();
            char * theTable = (char*)method_allocate_data_block(
                                m_method, jit_handle, 
                                targets * sizeof(void*), 16);
            char ** ptargets = (char**)theTable;
            // Fill out the table with targets
            for (unsigned j=0; j<targets; j++, ptargets++) {
                unsigned pc = jinst.get_target(j);
                *ptargets = (char*)m_infoBlock.get_ip(pc);
            }
            cpi.data_addr = theTable;
        }
        else if (cpi.data != 0) {
            void *& ptr = m_lazyRefs[cpi.data];
            if (ptr == NULL) {
                ptr = dstart + didx;
                ++didx;
            }
            cpi.data_addr = (char*)ptr;
        }
        gen_patch(m_vmCode, cpi);
    }
    assert(didx == m_infoBlock.get_num_refs());
}

}};	// ~namespace Jitrino::Jet
