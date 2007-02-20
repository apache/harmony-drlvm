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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.27.8.1.4.4 $
 *
 */

#include "CodeSelectors.h"
#include "../../vm/drl/DrlVMInterface.h"

namespace Jitrino {


class HIR2LIRSelectorSessionAction: public SessionAction {
public:
    virtual void run ();
};
static ActionFactory<HIR2LIRSelectorSessionAction> _hir2lir("hir2lir");

//
// code generator entry point
//
void HIR2LIRSelectorSessionAction::run() {
    CompilationContext* cc = getCompilationContext();
    IRManager& irManager = *cc->getHIRManager();
    CompilationInterface* ci = cc->getVMCompilationInterface();
    MethodDesc* methodDesc  = ci->getMethodToCompile();
    OpndManager& opndManager = irManager.getOpndManager();
    const OptimizerFlags& optFlags = irManager.getOptimizerFlags();
    VarOpnd* varOpnds   = opndManager.getVarOpnds();
    MemoryManager& mm  = cc->getCompilationLevelMemoryManager();

    MethodCodeSelector* mcs = new (mm) _MethodCodeSelector(irManager,methodDesc,varOpnds,&irManager.getFlowGraph(),
        opndManager, optFlags.sink_constants, optFlags.sink_constants1);
#if defined(_IPF_)
    IPF::CodeGenerator cg(mm, *ci);
#else
    Ia32::CodeGenerator cg;
#endif
    cg.genCode(this, *mcs);
}

POINTER_SIZE_INT
InlineInfoMap::ptr_to_uint64(void *ptr)
{
#ifdef POINTER64
    return (POINTER_SIZE_INT)ptr;
#else
    return (POINTER_SIZE_INT)ptr;
#endif
}

Method_Handle
InlineInfoMap::uint64_to_mh(POINTER_SIZE_INT value)
{
#ifdef POINTER64
    return (Method_Handle)value;
#else
    return (Method_Handle)((uint32)value);
#endif
}

void
InlineInfoMap::registerOffset(uint32 offset, InlineInfo* ii)
{
    assert(ii->countLevels() > 0);
    OffsetPair pair(offset, ii);
    list.push_back(pair);
}

bool
InlineInfoMap::isEmpty() const
{
    return list.size() == 0;
}

//
// offset_cnt ( offset depth mh[depth] )[offset_cnt]
//
// sizeof(offset_cnt|offset|depth|mh) = 8 
// size increased for better portability,
// everybody is welcome to optimize this storage
// 
// size = sizeof(POINTER_SIZE_INT) * (2 * offset_cnt + 1 + total_mh_cnt * 2)
//
uint32
InlineInfoMap::computeSize() const
{
    uint32 total_mh_cnt = 0;
    uint32 offset_cnt = 0;
    InlineInfoList::const_iterator it = list.begin();
    for (; it != list.end(); it++) {
        total_mh_cnt += it->inline_info->countLevels();
        offset_cnt++;
    }
    return sizeof(POINTER_SIZE_INT) * (2 * offset_cnt + 1 + total_mh_cnt * 2);
}

void
InlineInfoMap::write(InlineInfoPtr output)
{
//    assert(((uint64)ptr_to_uint64(output) & 0x7) == 0);

    POINTER_SIZE_INT* ptr = (POINTER_SIZE_INT *)output;
    *ptr++ = (POINTER_SIZE_INT)list.size(); // offset_cnt

    InlineInfoList::iterator it = list.begin();
    for (; it != list.end(); it++) {
        *ptr++ = (POINTER_SIZE_INT) it->offset;
        POINTER_SIZE_INT depth = 0;
        POINTER_SIZE_INT* depth_ptr = ptr++;
        assert(it->inline_info->countLevels() > 0);
        InlineInfo::InlinePairList::iterator desc_it = it->inline_info->inlineChain->begin();
        for (; desc_it != it->inline_info->inlineChain->end(); desc_it++) {
            MethodDesc* mdesc = (MethodDesc*)(*desc_it)->first;
            uint32 bcOffset = (uint32)(*desc_it)->second;
            //assert(dynamic_cast<DrlVMMethodDesc*>(mdesc)); // <-- some strange warning on Win32 here
            *ptr++ = ptr_to_uint64(((DrlVMMethodDesc*)mdesc)->getDrlVMMethod());
            *ptr++ = (POINTER_SIZE_INT)bcOffset;
            depth++;
        }
        assert(depth == it->inline_info->countLevels());
        *depth_ptr = depth;
    }
    assert((POINTER_SIZE_INT)ptr == (POINTER_SIZE_INT)output + computeSize());
}

POINTER_SIZE_INT*
InlineInfoMap::find_offset(InlineInfoPtr ptr, uint32 offset)
{
    assert(((POINTER_SIZE_INT)ptr_to_uint64(ptr) & 0x7) == 0);

    POINTER_SIZE_INT* tmp_ptr = (POINTER_SIZE_INT *)ptr;
    POINTER_SIZE_INT offset_cnt = *tmp_ptr++;

    for (uint32 i = 0; i < offset_cnt; i++) {
        POINTER_SIZE_INT curr_offs = *tmp_ptr++ ;
        if ( offset == curr_offs ) {
            return tmp_ptr;
        }
        POINTER_SIZE_INT curr_depth  = (*tmp_ptr++)*2 ;
        tmp_ptr += curr_depth;
    }

    return NULL;
}

uint32
InlineInfoMap::get_inline_depth(InlineInfoPtr ptr, uint32 offset)
{
    POINTER_SIZE_INT* tmp_ptr = find_offset(ptr, offset);
    if ( tmp_ptr != NULL ) {
        return (uint32)*tmp_ptr;
    }
    return 0;
}

Method_Handle
InlineInfoMap::get_inlined_method(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth)
{
    POINTER_SIZE_INT* tmp_ptr = find_offset(ptr, offset);
    if ( tmp_ptr != NULL ) {
        POINTER_SIZE_INT depth = *tmp_ptr++;
        assert(inline_depth < depth);
        tmp_ptr += ((depth - 1) - inline_depth ) * 2;
        return uint64_to_mh(*tmp_ptr);
    }
    return NULL;
}

uint16
InlineInfoMap::get_inlined_bc(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth)
{
    POINTER_SIZE_INT* tmp_ptr = find_offset(ptr, offset);
    if ( tmp_ptr != NULL ) {
        POINTER_SIZE_INT depth = *tmp_ptr++;
        assert(inline_depth < depth);
        tmp_ptr += ((depth - 1) - inline_depth) * 2 + 1;
        return (uint16)(*tmp_ptr);
    }
    return 0;
}


} //namespace Jitrino 
