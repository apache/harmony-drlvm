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
 * @author Alexander Astapchuk
 * @version $Revision$
 */
/**
 * @file
 * @brief CodeGen's routines for instrumentation and profiling.
 */
 
#include "cg.h"
#include "trace.h"

namespace Jitrino {
namespace Jet {

void CodeGen::gen_prof_be(void)
{
    if (!is_set(JMF_PROF_ENTRY_BE)) {
        return;
    }

#ifdef _IA32_
    AR addr = ar_x;
    int off = (int)m_p_backedge_counter;
#else
    AR addr = valloc(jobj);
    movp(addr, m_p_backedge_counter);
    int off = 0;
#endif
    alu(alu_add, Opnd(i32, addr, off), 1);
}

void CodeGen::gen_gc_safe_point()
{
    if (!is_set(JMF_BBPOLLING)) {
        return;
    }
    if (m_bbstate->seen_gcpt) {
        if (is_set(DBG_TRACE_CG)) {
            dbg(";;>gc.safepoint - skipped\n");
        }
        return;
    }

    m_bbstate->seen_gcpt = true;
    // On Windows we could use a bit, but tricky way - we know about VM 
    // internals and we know how Windows manages TIB, and thus we can get a 
    // direct access to the flag, without need to call VM:
    //      mov eax, fs:14
    //      test [eax+rt_suspend_req_flag_offset], 0
    // I don't believe this will gain any improvements for .jet, so using 
    // portable and 'official' way:
    gen_call_vm(cs_v, rt_helper_get_thread_suspend_ptr, 0);
    // The address of flag is now in gr_ret
    Opnd mem(i32, gr_ret, 0);
    alu(alu_cmp, mem, Opnd(0));
    unsigned br_off = br(z, 0, 0, taken);
    gen_call_vm_restore(false, cs_v, rt_helper_gc_safepoint, 0);
    patch(br_off, ip());
}



}}; // ~namespace Jitrino::Jet
