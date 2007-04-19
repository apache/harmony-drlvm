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
 * @version $Revision: 1.8.24.4 $
 *
 */

#ifndef _DEADCODEELIMINATOR_H_
#define _DEADCODEELIMINATOR_H_

#include "Stl.h"
#include "optpass.h"
#include "LoopTree.h"

namespace Jitrino {

class IRManager;
class Inst;
class Opnd;
class ControlFlowGraph;
class Node;
class BitSet;

class DeadCodeEliminator {
public:
    DeadCodeEliminator(IRManager& irm);
    void eliminateDeadCode(bool keepEmptyNodes);
    static void copyPropagate(Inst*);
    static Opnd* copyPropagate(Opnd*);
    bool eliminateUnreachableCode(); // returns true if any node is eliminated
    void removeExtraPseudoThrow();
private:
    void sweepInst(Node* node, Inst* inst, BitSet& usefulInstSet, BitSet& usefulVarSet, uint8 *usedInstWidth, uint32 minInstId, uint32 maxInstId, bool canRemoveStvars);
    void sweepInst1(Node* node, Inst* inst, BitSet& usefulInstSet, BitSet& usefulVarSet,
                    uint32 minInstId, uint32 maxInstId, bool canRemoveStvars); // if we're skipping instWidth
    static Opnd* findDefiningTemp(Opnd* var);
    void markEssentialPseudoThrows(LoopNode* loopNode, BitSet& essentialNodes);

    IRManager& irManager;
    ControlFlowGraph& flowGraph;
    Opnd* returnOpnd;
    bool preserveCriticalEdges;
};

} //namespace Jitrino 

#endif // _DEADCODEELIMINATOR_H_
