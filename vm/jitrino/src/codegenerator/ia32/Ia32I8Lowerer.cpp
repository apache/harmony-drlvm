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
 * @version $Revision: 1.12.6.2.4.3 $
 */

#include "Ia32I8Lowerer.h"
#include "Ia32Inst.h"
namespace Jitrino
{
namespace Ia32 {

//_______________________________________________________________________________________________________________
// I8 operation internal helpers
int64 __stdcall imul64(const int64 src1, const int64 src2)  stdcall__;
int64 __stdcall imul64(const int64 src1, const int64 src2)
{	return src1*src2;	}

int64 __stdcall idiv64(const int64 src1, const int64 src2)  stdcall__;
int64 __stdcall idiv64(const int64 src1, const int64 src2)
{	return src1/src2;	}

int __stdcall cmp64	(int64 src1, int64 src2) stdcall__;
int __stdcall cmp64	(int64 src1, int64 src2) 
{	return src1<src2?-1:src1>src2?1:0;	}

//_______________________________________________________________________________________________________________
void I8Lowerer::runImpl() 
{

// I8 operation internal helpers
	irManager.registerInternalHelperInfo("imul64", IRManager::InternalHelperInfo((void*)&imul64,&CallingConvention_STDCALL));
	irManager.registerInternalHelperInfo("idiv64", IRManager::InternalHelperInfo((void*)&idiv64,&CallingConvention_STDCALL));
	irManager.registerInternalHelperInfo("cmp64", IRManager::InternalHelperInfo((void*)&cmp64, &CallingConvention_STDCALL));


	StlMap<Opnd *,Opnd **> pairs(irManager.getMemoryManager());
	StlVector<Inst *> i8Insts(irManager.getMemoryManager());

	for(CFG::NodeIterator it(irManager, CFG::OrderType_Topological); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)) {
			BasicBlock * bb = (BasicBlock*)node;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getFirst(); inst!=NULL; inst = insts.getNext(inst)) {
				if (inst->hasKind(Inst::Kind_I8PseudoInst) || (inst->getMnemonic()==Mnemonic_CALL || inst->getMnemonic()==Mnemonic_RET ||inst->hasKind(Inst::Kind_EntryPointPseudoInst))){
					i8Insts.push_back(inst);
					foundI8Opnds = ~(uint32)0;
				}
			}
		}
	}

	for(StlVector<Inst *>::iterator it = i8Insts.begin(); it != i8Insts.end(); it++) {
		processOpnds(*it, pairs);
	}
	irManager.purgeEmptyBlocks();
}

void I8Lowerer::processOpnds(Inst * inst, StlMap<Opnd *,Opnd **>& pairs)
{
	Opnd * newOp1 = NULL, *newOp2 = NULL;
	Opnd * dst_1 = NULL, * dst_2 = NULL, * src1_1 = NULL, * src1_2 = NULL, * src2_1 = NULL, * src2_2 = NULL;
	Mnemonic mn = inst->getMnemonic();
	if (mn==Mnemonic_CALL || mn==Mnemonic_RET ||inst->hasKind(Inst::Kind_EntryPointPseudoInst)) {
		for(uint32 i = 0; i < inst->getOpndCount(); i++) {
			Opnd * opnd = inst->getOpnd(i);
			if (!isI8Type(opnd->getType()))
				continue;
			prepareNewOpnds(opnd,pairs,newOp1,newOp2);
			uint32 roles = inst->getOpndRoles(i);
			inst->setOpnd(i++, newOp1);
			inst->insertOpnd(i, newOp2, roles);
		}
	} else if (inst->hasKind(Inst::Kind_I8PseudoInst)){
		uint32	defCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def),
				useCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Use);
		Opnd * dst = defCount > 0 ? inst->getOpnd(0) : NULL;
		Opnd * src1 = useCount> 0 ? inst->getOpnd(defCount): NULL;
		Opnd * src2 = useCount> 1 ? inst->getOpnd(defCount+1): NULL;

		if (
			mn!=Mnemonic_SAL && mn!=Mnemonic_SAR && mn!=Mnemonic_SHR && mn!=Mnemonic_IDIV && mn!=Mnemonic_IMUL) {
			if (dst)
				prepareNewOpnds(dst,pairs,dst_1,dst_2);
			if (src1)
				prepareNewOpnds(src1,pairs,src1_1,src1_2);
			if (src2)
				prepareNewOpnds(src2,pairs,src2_1,src2_2);
		}

		BasicBlock * bb=inst->getBasicBlock();

		switch(mn) {
			case Mnemonic_ADD	:
				assert(dst_1 && src1_1 && src2_1);
				assert(dst_2 && src1_2 && src2_2);
				bb->prependInsts(irManager.newInstEx(Mnemonic_ADD, 1, dst_1, src1_1, src2_1),inst);
				bb->prependInsts(irManager.newInstEx(Mnemonic_ADC, 1, dst_2, src1_2, src2_2),inst);
				bb->removeInst(inst);
				break;
			case Mnemonic_SUB	:
				assert(dst_1 && src1_1 && src2_1);
				assert(dst_2 && src1_2 && src2_2);
				bb->prependInsts(irManager.newInstEx(Mnemonic_SUB, 1, dst_1, src1_1, src2_1),inst);
				bb->prependInsts(irManager.newInstEx(Mnemonic_SBB, 1, dst_2, src1_2, src2_2),inst);
				bb->removeInst(inst);
				break;
			case Mnemonic_AND	:
			case Mnemonic_OR	:
			case Mnemonic_XOR	:
				assert(dst_1 && src1_1 && src2_1);
				assert(dst_2 && src1_2 && src2_2);
			case Mnemonic_NOT	:
				assert(dst_1 && src1_1);
				assert(dst_2 && src1_2);
				bb->prependInsts(irManager.newInstEx(mn, 1, dst_1, src1_1, src2_1),inst);
				bb->prependInsts(irManager.newInstEx(mn, 1, dst_2, src1_2, src2_2),inst);
				bb->removeInst(inst);
				break;
			case Mnemonic_MOV	:
				assert(dst_1 && src1_1);
				bb->prependInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_1, src1_1), inst);
				if (dst_2 && src1_2)
					bb->prependInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_2, src1_2), inst);
				bb->removeInst(inst);
				break;
			case Mnemonic_MOVSX	:
			case Mnemonic_MOVZX	:
				assert(dst_1 && dst_2 && src1_1);
				assert(!src1_2);
				if (src1_1->getSize()<OpndSize_32){
					bb->prependInsts(irManager.newInstEx(mn, 1, dst_1, src1_1), inst);
				}else{
					assert(src1_1->getSize()==OpndSize_32);
					bb->prependInsts(irManager.newInstEx(Mnemonic_MOV, 1, dst_1, src1_1), inst);
				}
				if (mn==Mnemonic_MOVSX){
					bb->prependInsts(irManager.newInstEx(Mnemonic_CDQ, 1, dst_2, dst_1), inst);
					bb->removeInst(inst);
				}
				break;
			case Mnemonic_PUSH	:
				assert(src1_1);
				assert(src1_2);
				bb->prependInsts(irManager.newInstEx(Mnemonic_PUSH, 0, src1_2),inst);
				bb->prependInsts(irManager.newInstEx(Mnemonic_PUSH, 0, src1_1),inst);
				bb->removeInst(inst);
				break;
			case Mnemonic_POP	:
				assert(dst_1);
				assert(dst_2);
				bb->prependInsts(irManager.newInstEx(Mnemonic_POP, 1, dst_1),inst);
				bb->prependInsts(irManager.newInstEx(Mnemonic_POP, 1, dst_2),inst);
				bb->removeInst(inst);
				break;
			case Mnemonic_SHL	:
			{
				assert(dst && src1 && src2);
				Opnd * args[2]={ src1, src2 };
				CallInst * callInst=irManager.newRuntimeHelperCallInst(CompilationInterface::Helper_ShlI64, 2, args, dst);
				bb->prependInsts(callInst,inst);
				processOpnds(callInst, pairs);
				bb->removeInst(inst);
				break;
			}
			case Mnemonic_SHR	:
			case Mnemonic_SAR	:
			{
				assert(dst && src1 && src2);
				Opnd * args[2]={ src1, src2 };
				CallInst * callInst=irManager.newRuntimeHelperCallInst(mn==Mnemonic_SAR?CompilationInterface::Helper_ShrI64:CompilationInterface::Helper_ShruI64, 2, args, dst);
				bb->prependInsts(callInst,inst);
				processOpnds(callInst, pairs);
				bb->removeInst(inst);
				break;
			}
			case Mnemonic_CMP	:
			{
				assert(src1 && src2);
				const Insts& insts = bb->getInsts();
				Inst * condInst = insts.getNext(inst);
				Mnemonic mnem = getBaseConditionMnemonic(condInst->getMnemonic());
				if (condInst->getMnemonic() == Mnemonic_MOV) {
					condInst = insts.getNext(condInst);
					mnem = getBaseConditionMnemonic(condInst->getMnemonic());
				}
				if (mnem != Mnemonic_NULL) {
							if(condInst->hasKind(Inst::Kind_BranchInst)) {
								buildJumpSubGraph(inst,src1_1,src1_2,src2_1,src2_2,condInst);
							} else {
								buildSetSubGraph(inst,src1_1,src1_2,src2_1,src2_2,condInst);
							}
				} else {
					buildComplexSubGraph(inst,src1_1,src1_2,src2_1,src2_2);
				}
				inst->getBasicBlock()->removeInst(inst);
				break;
			}
			case Mnemonic_IMUL	:
			{
				assert(dst && src1 && src2);
				Opnd * args[2]={ src1, src2 };
				CallInst * callInst = irManager.newInternalRuntimeHelperCallInst("imul64", 2, args, dst);
				bb->prependInsts(callInst,inst);
				processOpnds(callInst, pairs);
				bb->removeInst(inst);
				break;
			}
			case Mnemonic_IDIV	:
			{
				assert(dst && src1 && src2);
				Opnd * args[2]={ src1, src2 };
				CallInst * callInst = irManager.newInternalRuntimeHelperCallInst("idiv64", 2, args, dst);
				bb->prependInsts(callInst,inst);
				processOpnds(callInst, pairs);
				bb->removeInst(inst);
				break;
			}
			default	:	
				assert(0);
		}//end switch by mnemonics
	}
}
void I8Lowerer::buildSimpleSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2) {
	CFG* subCFG = new(irManager.getMemoryManager()) CFG(irManager.getMemoryManager());
	BasicBlock* bbHMain = subCFG->getPrologNode();

	BasicBlock* bbLMain = subCFG->newBasicBlock();

	bbHMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_2, src2_2));
	bbHMain->appendInsts(irManager.newBranchInst(Mnemonic_JNZ));

	bbLMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_1, src2_1));
	//TODO: sink node is useless
    BasicBlock* sinkNode =  subCFG->newBasicBlock();

	ExitNode* exit = subCFG->getExitNode();

	subCFG->newDirectBranchEdge(bbHMain, sinkNode, 0.1);
	subCFG->newFallThroughEdge(bbHMain, bbLMain, 0.9);
	subCFG->newFallThroughEdge(bbLMain,sinkNode, 1);
	subCFG->newEdge(sinkNode, exit, 1);

	irManager.mergeGraphs(&irManager, subCFG, inst, 0);
}

void I8Lowerer::buildJumpSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst) {
	BasicBlock * bb=inst->getBasicBlock();

	CFG* subCFG = new(irManager.getMemoryManager()) CFG(irManager.getMemoryManager());
	BasicBlock* bbHMain = subCFG->getPrologNode();

	BasicBlock* bbLMain = subCFG->newBasicBlock();

	bbHMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_2, src2_2));
	bbHMain->appendInsts(irManager.newBranchInst(Mnemonic_JNZ));

	bbLMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_1, src2_1));
	//todo: sinkNode is useless
    BasicBlock* sinkNode =  subCFG->newBasicBlock();

	ExitNode* exit = subCFG->getExitNode();

	subCFG->newEdge(sinkNode, exit, 1);

	Edge * ftEdge = bb->getFallThroughEdge();
	Edge * dbEdge = bb->getDirectBranchEdge();
	BasicBlock * bbFT = (BasicBlock *)ftEdge->getNode(Direction_Head);
	BasicBlock * bbDB = (BasicBlock *)dbEdge->getNode(Direction_Head);

	Mnemonic mnem = Mnemonic_NULL;
	switch(condInst->getMnemonic()) {
		case Mnemonic_JL : mnem = Mnemonic_JB; break;
		case Mnemonic_JLE : mnem = Mnemonic_JBE; break;
		case Mnemonic_JG : mnem = Mnemonic_JA; break;
		case Mnemonic_JGE : mnem = Mnemonic_JAE; break;
		default: mnem = condInst->getMnemonic(); break;
	}
	bbLMain->appendInsts(irManager.newBranchInst(mnem));

	subCFG->newDirectBranchEdge(bbHMain, sinkNode, 0.1);
	subCFG->newFallThroughEdge(bbHMain, bbLMain, 0.9);
	subCFG->newFallThroughEdge(bbLMain, bbFT, 0.1);
	subCFG->newDirectBranchEdge(bbLMain, bbDB, 0.9);

	irManager.mergeGraphs(&irManager, subCFG, inst, 0);
}

void I8Lowerer::buildSetSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst) {

	CFG* subCFG = new(irManager.getMemoryManager()) CFG(irManager.getMemoryManager());
	BasicBlock* bbHMain = subCFG->getPrologNode();

	BasicBlock* bbLMain = subCFG->newBasicBlock();

	bbHMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_2, src2_2));
	bbHMain->appendInsts(irManager.newBranchInst(Mnemonic_JNZ));

	bbLMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_1, src2_1));
	//TODO: sinkNode is useless
    BasicBlock* sinkNode =  subCFG->newBasicBlock();

	ExitNode* exit = subCFG->getExitNode();


	//TODO: add split-block-on-inst functionality to CFG
    CFG* tmpSubCFG = new(irManager.getMemoryManager()) CFG(irManager.getMemoryManager());
	BasicBlock* tmpBB = tmpSubCFG->getPrologNode();
	ExitNode* tmpExit = tmpSubCFG->getExitNode();
	tmpSubCFG->newEdge(tmpBB, tmpExit, 1);
	irManager.mergeGraphs(&irManager, tmpSubCFG, condInst, 0);

	BasicBlock* bbHF = subCFG->newBasicBlock();

	Mnemonic mnem = getBaseConditionMnemonic(condInst->getMnemonic());

	Inst * nextInst = inst->getBasicBlock()->getInsts().getNext(inst);
	if(nextInst->getMnemonic() == Mnemonic_MOV) {
		bbHF->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, nextInst->getOpnd(0), nextInst->getOpnd(1)));
		bbLMain->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, nextInst->getOpnd(0), nextInst->getOpnd(1)));
		nextInst->getBasicBlock()->removeInst(nextInst);
	} else {
		assert(nextInst == condInst);
	}
	bbHF->appendInsts(irManager.newInst(condInst->getMnemonic(), condInst->getOpnd(0), mnem == Mnemonic_CMOVcc? condInst->getOpnd(1): NULL));
	bbHF->appendInsts(irManager.newBranchInst(Mnemonic_JMP));
	
	ConditionMnemonic condMnem = ConditionMnemonic(condInst->getMnemonic()-mnem);
	switch(condMnem) {
		case ConditionMnemonic_L : condMnem = ConditionMnemonic_B; break;
		case ConditionMnemonic_LE : condMnem = ConditionMnemonic_BE; break;
		case ConditionMnemonic_G : condMnem = ConditionMnemonic_A; break;
		case ConditionMnemonic_GE : condMnem = ConditionMnemonic_AE; break;
		default: break;
	}
	bbLMain->appendInsts(irManager.newInst(Mnemonic(mnem+condMnem), condInst->getOpnd(0), mnem == Mnemonic_CMOVcc? condInst->getOpnd(1): NULL));
	bbLMain->appendInsts(irManager.newBranchInst(Mnemonic_JMP));
	
	subCFG->newDirectBranchEdge(bbHMain, bbHF, 0.1);
	subCFG->newFallThroughEdge(bbHMain, bbLMain, 0.9);
	subCFG->newDirectBranchEdge(bbLMain, sinkNode, 1);
	subCFG->newDirectBranchEdge(bbHF, sinkNode, 1);
	subCFG->newEdge(sinkNode, exit, 1);
	condInst->getBasicBlock()->removeInst(condInst);

	irManager.mergeGraphs(&irManager, subCFG, inst, 0);
}

void I8Lowerer::buildComplexSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst) {

	Opnd * dst_1 = irManager.newOpnd(irManager.getTypeManager().getInt32Type());
	CFG* subCFG = new(irManager.getMemoryManager()) CFG(irManager.getMemoryManager());
	BasicBlock* bbHMain = subCFG->getPrologNode();
	BasicBlock* bbHF = subCFG->newBasicBlock();
	BasicBlock* bbHS = subCFG->newBasicBlock();

	BasicBlock* bbLMain = subCFG->newBasicBlock();
	BasicBlock* bbLF = subCFG->newBasicBlock();
	BasicBlock* bbLS = subCFG->newBasicBlock();

	bbHMain->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), 0)));
	bbHMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_2, src2_2));
	bbHMain->appendInsts(irManager.newBranchInst(Mnemonic_JZ));
	
	bbHF->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), -1)));
	bbHF->appendInsts(irManager.newBranchInst(Mnemonic_JG));
	
	bbHS->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), 1)));
	bbHS->appendInsts(irManager.newBranchInst(Mnemonic_JMP));
	
	bbLMain->appendInsts(irManager.newInst(Mnemonic_CMP, src1_1, src2_1));
	bbLMain->appendInsts(irManager.newBranchInst(Mnemonic_JZ));

	bbLF->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), -1)));
	bbLF->appendInsts(irManager.newBranchInst(Mnemonic_JA));
	
	bbLS->appendInsts(irManager.newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), 1)));
	bbLS->appendInsts(irManager.newBranchInst(Mnemonic_JMP));

	BasicBlock* sinkNode =  subCFG->newBasicBlock();
	sinkNode->appendInsts(irManager.newInst(Mnemonic_CMP, dst_1, irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), 0)));
	
	ExitNode* exit = subCFG->getExitNode();
	
	subCFG->newFallThroughEdge(bbHMain, bbHF, 0.1);
	subCFG->newDirectBranchEdge(bbHMain, bbLMain, 0.9);
	subCFG->newFallThroughEdge(bbHF, sinkNode, 0.5);
	subCFG->newDirectBranchEdge(bbHF, bbHS, 0.5);
	subCFG->newDirectBranchEdge(bbHS, sinkNode, 1);
	subCFG->newFallThroughEdge(bbLMain, bbLF, 0.1);
	subCFG->newDirectBranchEdge(bbLMain, sinkNode, 0.9);
	subCFG->newFallThroughEdge(bbLF, sinkNode, 0.5);
	subCFG->newDirectBranchEdge(bbLF, bbLS, 0.5);
	subCFG->newDirectBranchEdge(bbLS, sinkNode, 1);
	subCFG->newEdge(sinkNode, exit, 1);

	irManager.mergeGraphs(&irManager, subCFG, inst, 0);
}

void I8Lowerer::prepareNewOpnds(Opnd * longOpnd, StlMap<Opnd *,Opnd **>& pairs, Opnd*& newOp1, Opnd*& newOp2)
{
	if (!isI8Type(longOpnd->getType())){
		newOp1=longOpnd;
		newOp2=NULL;
		return;
	}

	if(pairs.find(longOpnd)!=pairs.end()) {
		newOp1 = pairs[longOpnd][0];
		newOp2 = pairs[longOpnd][1];
	} else {
		if(longOpnd->isPlacedIn(OpndKind_Memory)) {
			newOp1 = irManager.newOpnd(irManager.getTypeManager().getUInt32Type());
			newOp2 = irManager.newOpnd(irManager.getTypeManager().getInt32Type());
			Opnd * opnds[2] = { newOp1, newOp2 };
			irManager.assignInnerMemOpnds(longOpnd, opnds, 2);
		} else if(longOpnd->isPlacedIn(OpndKind_Imm)){
			newOp1 = irManager.newImmOpnd(irManager.getTypeManager().getUInt32Type(), (uint32)longOpnd->getImmValue());
			newOp2 = irManager.newImmOpnd(irManager.getTypeManager().getInt32Type(), (int32)(longOpnd->getImmValue()>>32));
		} else {
			newOp1 = irManager.newOpnd(irManager.getTypeManager().getUInt32Type());
			newOp2 = irManager.newOpnd(irManager.getTypeManager().getInt32Type());
		}
		pairs[longOpnd] = new(irManager.getMemoryManager()) Opnd*[2];
		pairs[longOpnd][0]=newOp1;
		pairs[longOpnd][1]=newOp2;
	}
}

}}
