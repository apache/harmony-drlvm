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
 * @version $Revision: 1.8.12.1.4.3 $
 */

#include "Ia32CopyExpansion.h"

namespace Jitrino
{
namespace Ia32{

//========================================================================================
// class CopyExpansion
//========================================================================================
//_________________________________________________________________________________________________
void CopyExpansion::runImpl()
{
	IRManager & irm=getIRManager();
    CompilationInterface& compIntfc = irm.getCompilationInterface();
    VectorHandler* bc2LIRmapHandler = NULL;
    MemoryManager& mm = irm.getMemoryManager();

    if (compIntfc.isBCMapInfoRequired()) {
        bc2LIRmapHandler = new(mm) VectorHandler(bcOffset2LIRHandlerName, compIntfc.getMethodToCompile());
    }

	irm.finalizeCallSites(); 

    const Nodes& nodes = irm.getNodesPostorder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)){
			BasicBlock * bb=(BasicBlock *)node;
			const Insts& insts=bb->getInsts();
			uint32 regUsageMask = 0;
			bool calculatingRegUsage = false;
			for (Inst * inst=insts.getLast(), * nextInst=NULL; inst!=NULL; inst=nextInst){
				nextInst=insts.getPrev(inst);
				if (inst->hasKind(Inst::Kind_CopyPseudoInst)){
					Mnemonic mn=inst->getMnemonic();
                    Inst *copySequence = NULL;
					if (mn==Mnemonic_MOV){
						Opnd * toOpnd=inst->getOpnd(0);
						Opnd * fromOpnd=inst->getOpnd(1);

						if (toOpnd->isPlacedIn(OpndKind_Reg) && fromOpnd->isPlacedIn(OpndKind_Reg)){
							if (toOpnd->getRegName()==fromOpnd->getRegName())
								continue;
						}else{
							if (!calculatingRegUsage && toOpnd->isPlacedIn(OpndKind_Mem) && fromOpnd->isPlacedIn(OpndKind_Mem)){
                        		restoreRegUsage(bb, inst, regUsageMask);
                        		calculatingRegUsage=true;
							}
                        }
                        copySequence = irManager.newCopySequence(Mnemonic_MOV, toOpnd, fromOpnd, regUsageMask);
					}else if (mn==Mnemonic_PUSH||mn==Mnemonic_POP){
						if (!calculatingRegUsage && inst->getOpnd(0)->isPlacedIn(OpndKind_Mem)){
                        	restoreRegUsage(bb, inst, regUsageMask);
                        	calculatingRegUsage=true;
                        }
                        copySequence = irManager.newCopySequence(mn, inst->getOpnd(0), NULL, regUsageMask);
					}
                    // CopyPseudoInst map entries should be changed by new copy sequence instrutions in byte code map
                    if (compIntfc.isBCMapInfoRequired() && copySequence != NULL) {
                        uint64 instID = inst->getId();
                        uint64 bcOffs = bc2LIRmapHandler->getVectorEntry(instID);
                        if (bcOffs != ILLEGAL_VALUE) {
                            Inst * cpInst=NULL, * nextCpInst=copySequence, * lastCpInst=copySequence->getPrev(); 
                            do { 
                                cpInst=nextCpInst;
                                nextCpInst=cpInst->getNext();
                                uint64 cpInstID = cpInst->getId();
                                bc2LIRmapHandler->setVectorEntry(cpInstID, bcOffs);
                            } while ((cpInst != lastCpInst) && (cpInst != NULL));
                        }
                        bc2LIRmapHandler->removeVectorEntry(instID);
                    }
                    // End of code map change

                    bb->appendInsts(copySequence, inst);
					bb->removeInst(inst);
				};
				if (calculatingRegUsage)
					irManager.updateRegUsage(inst, OpndKind_GPReg, regUsageMask);
			}
		}
	}
	irManager.fixLivenessInfo();
}

//_________________________________________________________________________________________________
void CopyExpansion::restoreRegUsage(BasicBlock * bb, Inst * toInst, uint32& regUsageMask)
{
	const Insts& insts = bb->getInsts();
	Inst * inst = insts.getLast();
	if (inst == NULL)
		return;
	irManager.getRegUsageAtExit(bb, OpndKind_GPReg, regUsageMask);
	for (; inst != toInst; inst = insts.getPrev(inst)){
		irManager.updateRegUsage(inst, OpndKind_GPReg, regUsageMask);
	}
}

}}; //namespace Ia32

