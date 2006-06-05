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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.9.24.4 $
 *
 */

#ifndef _GLOBALOPNDANALYZER_H_
#define _GLOBALOPNDANALYZER_H_

class IRManager;
class Dominator;
class CFGNode;
class Inst;
class LoopTree;

#include "Stl.h"
#include "irmanager.h"
#include "optpass.h"

namespace Jitrino {

DEFINE_OPTPASS(GlobalOperandAnalysisPass)

//
//  Global operand analyzer marks temporaries whose live range spans
//  a basic block boundary as globals
//
class GlobalOpndAnalyzer {
public:
    GlobalOpndAnalyzer(IRManager& irm, FlowGraph* region=NULL) 
        : irManager(irm), flowGraph((region != NULL) ? *region : irm.getFlowGraph())
    {
    }
    void doAnalysis();
    virtual ~GlobalOpndAnalyzer() {};
protected:
    void getNodesInPostorder();
    void resetGlobalBits();
    virtual void markGlobals();

    IRManager&            irManager;
    FlowGraph&            flowGraph;
    ::std::vector<CFGNode*> nodes;
};

//
//  Advanced operand analyzer marks temporaries whose live range
//  spans a loop boundary as globals
//

class AdvancedGlobalOpndAnalyzer : public GlobalOpndAnalyzer {
public:
    AdvancedGlobalOpndAnalyzer(IRManager& irm, const LoopTree& loopInfo) 
        : GlobalOpndAnalyzer(irm), opndTable(NULL), loopInfo(loopInfo)
    {}
    virtual ~AdvancedGlobalOpndAnalyzer() {};
private:
    bool cfgContainsLoops();
    void analyzeInst(Inst* inst, uint32 loopHeader, uint32 timeStamp);
    void unmarkFalseGlobals();
    void markManagedPointerBases();
    virtual void markGlobals();

    struct OpndInfo;
    class  OpndTable;
    OpndTable *      opndTable;
    const LoopTree&  loopInfo;
};

} //namespace Jitrino 

#endif // _GLOBALOPNDANALYZER_H_
