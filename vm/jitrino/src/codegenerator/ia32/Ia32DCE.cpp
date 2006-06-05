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
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.16.8.2.4.3 $
 */

#include "Ia32DCE.h"
#include "Ia32CodeGenerator.h"

namespace Jitrino
{
namespace Ia32{



//========================================================================================
// class DCE
//========================================================================================

//_________________________________________________________________________________________________
void DCE::runImpl()
{	
	if (parameters && !irManager.getCGFlags()->earlyDCEOn) {
		return;
	}

	IRManager & irm=getIRManager();
    irm.updateLivenessInfo();
    irm.calculateOpndStatistics();
	LiveSet ls(irm.getMemoryManager(), irm.getOpndCount());
    const Nodes& nodes = irm.getNodesPostorder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)){
            irm.getLiveAtExit(node, ls);
			BasicBlock * bb=(BasicBlock *)node;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getLast(), * prevInst=NULL; inst!=NULL; inst=prevInst){
				prevInst=insts.getPrev(inst);
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
						for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
							Opnd * opnd = inst->getOpnd(it);
                            if ((ls.isLive(opnd) && (inst->getOpndRoles(it) & Inst::OpndRole_Def)) ||
								(((opnd->getMemOpndKind()&(MemOpndKind_Heap|MemOpndKind_StackManualLayout))!=0) && (inst->getMnemonic() != Mnemonic_LEA))) {
                                deadInst=false;
                                break;
                            }
                        }
                    }
                }
                if (deadInst) {
                	bb->removeInst(inst);
                } else {
					irm.updateLiveness(inst, ls);
                }
			}
			irm.getLiveAtEntry(node)->copyFrom(ls);
		}
	}
	irm.eliminateSameOpndMoves();
    irManager.packOpnds();
    irm.invalidateLivenessInfo();
}

}}; //namespace Ia32

