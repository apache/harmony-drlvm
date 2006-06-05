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
 * @author Intel, George A. Timoshenko
 * @version $Revision: 1.9.16.2.4.4 $
 *
 */

#ifndef _JAVAFLOWGRAPHBUILDER_
#define _JAVAFLOWGRAPHBUILDER_

#include "FlowGraph.h"

namespace Jitrino {


class JavaFlowGraphBuilder {
public:
    // regular version
    JavaFlowGraphBuilder(MemoryManager&, IRBuilder &);
    // version for IR inlining
    JavaFlowGraphBuilder(MemoryManager&, IRBuilder &, CFGNode *, FlowGraph *);
    CFGNode*     genBlock(LabelInst* blockLabel);
    CFGNode*     genBlockAfterCurrent(LabelInst *label);
    void         build();
    void         buildIRinline(Opnd *ret, FlowGraph *, Inst *callsite); // version for IR inlining
    CFGNode*     createDispatchNode();
    FlowGraph*   getCFG() { return fg; }
private:
    void         createCFGEdges();
    CFGNode*     edgesForBlock(CFGNode* block);
    void         edgesForHandler(CFGNode* entry);
    void         edgeForFallthrough(CFGNode* block);
    void         eliminateUnnestedLoopsOnDispatch();
    bool         lastInstIsMonitorExit(CFGNode* node);
    void         resolveWhileTrue();
    void         reverseDFS( StlVector<int8>& state, CFGNode* targetNode, uint32* NodesCounter );
    void         forwardDFS( StlVector<int8>& state, CFGNode* srcNode );
    //
    // private fields
    //
    MemoryManager&  memManager;
    CFGNode*        currentBlock;
    FlowGraph*      fg;
    IRBuilder&      irBuilder;
};


} //namespace Jitrino 

#endif // _JAVAFLOWGRAPHBUILDER_
