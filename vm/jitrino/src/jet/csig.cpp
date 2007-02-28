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
 * @author Alexander Astapchuk
 * @version $Revision$
 */
/**
 * @file
 * @brief Implementation of CallSig routines.
 */
 
#include "csig.h"


namespace Jitrino {
namespace Jet {

const CallSig cs_v(CCONV_STDCALL);

void CallSig::init(void)
{
    // can't have stack alignment in calling convention when callee pops
    // so if any of ALIGN set, then CALLER_POPS must also be set.
    assert( !(m_cc&(CCONV_STACK_ALIGN16|CCONV_STACK_ALIGN_HALF16)) || 
            (m_cc&CCONV_CALLER_POPS));
    
    unsigned num = (unsigned)m_args.size();
    m_data.resize(num);
    unsigned fps = 0, gps = 0;

    //
    // Assign registers
    //
    bool regs = !(m_cc & CCONV_MEM);
    
    // Note: Registers are always assigned in left-to-right order, 
    // regardless of L2R setting in calling convention. This is how all our
    // conventions behave - might want to document it somewhere - TODO.
    
    for (unsigned i=0; i<num; i++) {
        jtype jt = m_args[i];
        if (regs && is_f(jt) && get_cconv_fr(fps) != fr_x) {
            m_data[i] = get_cconv_fr(fps);
            ++fps;
        }
        else if (regs && !is_f(jt) && get_cconv_gr(gps) != gr_x) {
            m_data[i] = get_cconv_gr(gps);
            ++gps;
        }
        else {
            // mark the items that need to be assigned to memory
            m_data[i] = -1;
        }
    }
    
    bool l2r = m_cc & CCONV_L2R;
    int start, end, step;
    if (l2r) {
        start = num-1;
        end = -1;
        step = -1;
    }
    else {
        start = 0;
        end = num;
        step = 1;
    }
    int off = 0;
    m_stack = 0;
    
    for (int i=start; i != end; i+=step) {
        jtype jt = m_args[i];
        if (m_data[i]<0) {
            m_data[i] = off;
            off -= STACK_SIZE(jtypes[jt].size);
        }
    }
    m_stack = -off;
    // Do alignment
    if (m_stack != 0 && 
        (m_cc & (CCONV_STACK_ALIGN16|CCONV_STACK_ALIGN_HALF16))) {
        m_stack = (m_stack+15) & 0xFFFFFFF0;
    }
}


}}; // ~namespace Jitrino::Jet
