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
 * @author Intel, Natalya V. Golovleva
 * @version $Revision: 1.4.22.4 $
 *
 */

#ifndef _LAZYEXCEPTION_H_
#define _LAZYEXCEPTION_H_

#include "open/types.h"
#include "optpass.h"
#include "Inst.h"
#include "BitSet.h"
#include "VMInterface.h"

namespace Jitrino {

DEFINE_OPTPASS(LazyExceptionOptPass)

class LazyExceptionOpt {
public:
    LazyExceptionOpt(IRManager &ir_manager, MemoryManager& mem_manager);
    void doLazyExceptionOpt();

private:
    void printInst1(::std::ostream& os, Inst* inst, std::string txt);
    bool addOptCandidates(uint32 id, Inst* inst);
    bool instSideEffect(Inst* inst);
    void fixOptCandidates(BitSet* bs);
    void printOptCandidates(::std::ostream& os);
    bool checkMethodCall(Inst* inst); 
    void removeNode(CFGNode* node);
    bool removeInsts(Inst* oinst,Inst* iinst);
    bool checkField(Inst* inst);
    bool isEqualExceptionNodes(Inst* oi, Inst* ti);
    bool checkInSideEff(Inst* throw_inst, Inst* init_inst);
    bool mayBeNullArg(Inst* call_inst, Inst* src_inst);
    bool checkArg(CFGNode* node);

private:
    IRManager     &irManager;
    MemoryManager &memManager;
    MemoryManager leMemManager;
    CompilationInterface &compInterface;
    bool isExceptionInit;
    bool isArgCheckNull;
#ifdef _DEBUG
    MethodDesc* mtdDesc;
#endif
    typedef StlList<Inst*> ThrowInsts;	
    struct OptCandidate {
        uint32 opndId;
        Inst* objInst;
        Inst* initInst;
        ThrowInsts* throwInsts;
    };
    typedef StlList<OptCandidate*> OptCandidates;	
    OptCandidates* optCandidates;
    static int level;
    struct NodeSet {
        Inst* arg_src_inst;
        Inst* call_inst;
        Inst* check_inst;
        BitSet* nodes;
        Inst* reset_inst;
    };
    NodeSet* nodeSet;
};

} // namespace Jitrino

#endif // _LAZYEXCEPTION_H_

