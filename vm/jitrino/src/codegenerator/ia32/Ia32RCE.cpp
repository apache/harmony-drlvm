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
 * @author Nikolay A. Sidelnikov
 * @version $Revision: 1.9.16.2.4.3 $
 */

#include "Ia32RCE.h"
namespace Jitrino
{
namespace Ia32 {

/**
 *	The algorithm finds conditional instruction first, then corresponded 
 *	CMP instruction and arithmetic instruction which affects flags in the same
 *	way as CMP. Combination is considered as available to be reduced if there 
 *	are no instructions between CMP and arithmetic instruction which influence 
 *	to flags or CMP operands.
 *
 *	Also it tries to change improper conditional instruction to more optimizable
 *	kind.
 */
void
RCE::runImpl() 
{
	Inst * inst, * cmpInst, *condInst;
	IRManager & irm=getIRManager();
	Opnd * cmpOp = NULL; 
	cmpInst = condInst = NULL;
	for(CFG::NodeIterator it(irm); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)) {
			BasicBlock * bb = (BasicBlock*)node;
			if(bb->isEmpty())
				continue;
			const Insts& insts=bb->getInsts();
			inst = insts.getLast();
			cmpInst = condInst = NULL;
			for(inst = insts.getLast(); inst != NULL; inst = insts.getPrev(inst)) {
				//find conditional instruction
				Mnemonic baseMnem = getBaseConditionMnemonic(inst->getMnemonic());
				if (baseMnem != Mnemonic_NULL) {
					condInst = inst;
					cmpInst = NULL;
				} else if (condInst) {
					//find CMP instruction corresponds to conditional instruction
					if(inst->getMnemonic() == Mnemonic_CMP || inst->getMnemonic() == Mnemonic_UCOMISD || inst->getMnemonic() == Mnemonic_UCOMISS) {
						if (cmpInst) {
							//this comparison is redundant because of overrided by cmpInst
							inst->getBasicBlock()->removeInst(inst);
							continue;
						}
						cmpInst = inst;
						uint32 defCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def);
						if(inst->getOpnd(defCount+1)->isPlacedIn(OpndKind_Imm)) {
							//try to change conditional instruction to make combination available to optimize
							cmpOp = inst->getOpnd(defCount);
							Inst * newCondInst; Mnemonic mnem;
							int64 val = inst->getOpnd(defCount+1)->getImmValue();
							
							if (val == 0) {
								continue;
							} else if (val == 1 && ConditionMnemonic(condInst->getMnemonic()-getBaseConditionMnemonic(condInst->getMnemonic())) == ConditionMnemonic_L){
								mnem = Mnemonic((condInst->getMnemonic() - Mnemonic(ConditionMnemonic_L)) + Mnemonic(ConditionMnemonic_LE));
							} else if (val == -1 && ConditionMnemonic(condInst->getMnemonic()-getBaseConditionMnemonic(condInst->getMnemonic())) == ConditionMnemonic_G) {
								mnem = Mnemonic((condInst->getMnemonic() - Mnemonic(ConditionMnemonic_G)) + Mnemonic(ConditionMnemonic_GE));
							} else if (val == -1 && ConditionMnemonic(condInst->getMnemonic()-getBaseConditionMnemonic(condInst->getMnemonic())) == ConditionMnemonic_B) {
								mnem = Mnemonic((condInst->getMnemonic() - Mnemonic(ConditionMnemonic_B)) + Mnemonic(ConditionMnemonic_BE));
							} else {
								continue;
							}
							//replace old conditional instruction
							newCondInst = 
								condInst->hasKind(Inst::Kind_BranchInst) ? 
									irm.newBranchInst(mnem,condInst->getOpnd(0)) : 
									(condInst->getForm() == Inst::Form_Native ? 
										irm.newInst(mnem, condInst->getOpnd(0), condInst->getOpnd(1)) : 
										irm.newInstEx(mnem, condInst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def), condInst->getOpnd(0), condInst->getOpnd(1)));
							condInst->getBasicBlock()->appendInsts(newCondInst, condInst);
							inst->getOpnd(defCount+1)->assignImmValue(0);
							condInst->getBasicBlock()->removeInst(condInst);
							condInst = newCondInst;
						} 
					//find flags affected instruction precedes cmpInst
					} else if (isUsingFlagsAffected(inst, condInst)) {
						if (cmpInst) {
							if (isSuitableToRemove(inst, condInst, cmpInst, cmpOp))							
							{
								bb->removeInst(cmpInst);//replace cmp
							} 
						}
						condInst = NULL;
					} else {
						if (inst->getOpndCount(Inst::OpndRole_Implicit|Inst::OpndRole_Def) || inst->getMnemonic() == Mnemonic_CALL) {
							condInst = NULL;
						} else {
							//check for moving cmpInst operands	
							if ((inst->getMnemonic() == Mnemonic_MOV) && (inst->getOpnd(0) == cmpOp)) {
								cmpOp = inst->getOpnd(1);
							}
						}
					} 
				}//end if/else by condInst
			}//end for() by Insts
		}//end if BasicBlock
	}//end for() by Nodes
}

bool
RCE::isUsingFlagsAffected(Inst * inst, Inst * condInst) 
{
	if (!inst->getOpndCount(Inst::OpndRole_Implicit|Inst::OpndRole_Def))
		//instruction doesn't change flags
		return false;
	switch (inst->getMnemonic()) {
		case Mnemonic_SUB:
			//instruction changes all flags
			return true; 
		case Mnemonic_IDIV:
		case Mnemonic_CALL:
			//instruction changes flags in the way doesn't correspond CMP
			return false;
		default:
			//instruction changes particular flags
			ConditionMnemonic mn = ConditionMnemonic(condInst->getMnemonic()-getBaseConditionMnemonic(condInst->getMnemonic()));
			return ( mn == ConditionMnemonic_Z || mn == ConditionMnemonic_NZ) ? true : false;
	}
}

bool RCE::isSuitableToRemove(Inst * inst, Inst * condInst, Inst * cmpInst, Opnd * cmpOp)
{
	/*	cmpInst can be removed if inst defines the same operand which will be
	 *	compared with zero by cmpInst or inst is SUB with the same use-operands as cmpInst
	 *  Required: Native form of insts
	 */
	uint32 cmpOpCount = cmpInst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_UseDef);
	if ((cmpOp == inst->getOpnd(0)) && cmpInst->getOpnd(cmpOpCount -1)->isPlacedIn(OpndKind_Imm) && (cmpInst->getOpnd(cmpOpCount -1)->getImmValue() == 0)) {
			return true;
	}
	return false;
}

}} //end namespace Ia32

