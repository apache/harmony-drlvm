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
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.16.8.2.4.3 $
 */

#include "Ia32IRManager.h"
#include "Ia32CodeGenerator.h"

namespace Jitrino
{
namespace Ia32{

/**
    class DCE performs Dead code elimination
*/
class DCE : public SessionAction {
public:
    void runImpl();
    uint32 getSideEffects() const {return 0;}
    uint32 getNeedInfo()const {return 0;}
};

static ActionFactory<DCE> _dce("cg_dce");


//========================================================================================
// class DCE
//========================================================================================

//_________________________________________________________________________________________________
void DCE::runImpl()
{   
    bool early = false;
    getArg("early", early);
    if (early && !irManager->getCGFlags()->earlyDCEOn) {
        return;
    }

    irManager->updateLivenessInfo();
    irManager->calculateOpndStatistics();
    BitSet ls(irManager->getMemoryManager(), irManager->getOpndCount());
    const Nodes& nodes = irManager->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()){
            irManager->getLiveAtExit(node, ls);
            for (Inst * inst=(Inst*)node->getLastInst(), * prevInst=NULL; inst!=NULL; inst=prevInst){
                prevInst=inst->getPrevInst();
                bool deadInst=!inst->hasSideEffect();
                if (deadInst){
                    if (inst->hasKind(Inst::Kind_CopyPseudoInst)){
                        Opnd * opnd=inst->getOpnd(1);
                        if (opnd->getType()->isFP() && opnd->getDefiningInst()!=NULL && opnd->getDefiningInst()->getMnemonic()==Mnemonic_CALL){
                            deadInst=false;
                        }
                    }
                    if (deadInst){
                        Inst::Opnds opnds(inst, Inst::OpndRole_All);
                        for (Inst::Opnds::iterator ito = opnds.begin(); ito != opnds.end(); ito = opnds.next(ito)){
                            Opnd * opnd = inst->getOpnd(ito);
                            if ((ls.getBit(opnd->getId()) && (inst->getOpndRoles(ito) & Inst::OpndRole_Def)) ||
                                (((opnd->getMemOpndKind()&(MemOpndKind_Heap|MemOpndKind_StackManualLayout))!=0) && (inst->getMnemonic() != Mnemonic_LEA))) {
                                deadInst=false;
                                break;
                            }
                        }
                    }
                }
                if (deadInst) {
                    inst->unlink();
                } else {
                    irManager->updateLiveness(inst, ls);
                }
            }
            irManager->getLiveAtEntry(node)->copyFrom(ls);
        }
    }

    irManager->eliminateSameOpndMoves();

    irManager->getFlowGraph()->purgeEmptyNodes();
    irManager->getFlowGraph()->mergeAdjacentNodes(true, false);
    irManager->getFlowGraph()->purgeUnreachableNodes();

    irManager->packOpnds();
    irManager->invalidateLivenessInfo();
}

}}; //namespace Ia32

