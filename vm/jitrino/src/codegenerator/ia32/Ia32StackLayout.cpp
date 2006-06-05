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
 * @author Intel, Nikolay A. Sidelnikov
 * @version $Revision: 1.10.14.1.4.4 $
 */

#include "Ia32StackLayout.h"
namespace Jitrino
{
namespace Ia32 {

StackLayouter::StackLayouter (IRManager& irm, const char * params)
	:IRTransformer(irm, params), 
	localBase(0),
	localEnd(0),
	fcalleeBase(0),
	fcalleeEnd(0),
	acalleeBase(0),
	acalleeEnd(0),
	icalleeBase(0),
	icalleeEnd(0),
	icallerBase(0),
	icallerEnd(0),
	retEIPBase(0),
	retEIPEnd(0),
    inargBase(0),
	inargEnd(0),
	frameSize(0),
	outArgSize(0),
	stackInfo(new(irm.getMemoryManager()) StackInfo(irm.getMemoryManager())),
	memoryManager(0x100, "StackLayouter")
{
	irm.setInfo("stackInfo", stackInfo);
};

void StackLayouter::runImpl()
{
	IRManager & irm=getIRManager();
	irm.calculateOpndStatistics();
#ifdef _DEBUG
	checkUnassignedOpnds();
#endif
	irm.calculateTotalRegUsage(OpndKind_GPReg);
	createProlog();
	createEpilog();
	irm.calculateStackDepth();
	irm.layoutAliasOpnds();

	//fill StackInfo object
	stackInfo->frameSize = getFrameSize();

	stackInfo->icalleeMask = irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg) & irm.getTotalRegUsage(OpndKind_GPReg);
	stackInfo->icalleeOffset = getIntCalleeBase();
	stackInfo->fcallee = irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_FPReg);
	stackInfo->foffset = getFloatCalleeBase();
	stackInfo->acallee = 0; //VSH: TODO - get rid off appl regs irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_ApplicationReg);
	stackInfo->aoffset = getApplCalleeBase();
	stackInfo->localOffset = getLocalBase();
	stackInfo->eipOffset = getRetEIPBase();
}

void StackLayouter::checkUnassignedOpnds()
{
	for (uint32 i=0, n=irManager.getOpndCount(); i<n; i++)
		assert(irManager.getOpnd(i)->hasAssignedPhysicalLocation());
}

void StackLayouter::createProlog()
{
	IRManager & irm=getIRManager();

	for (uint32 i = 0; i<irm.getOpndCount(); i++)//create or reset displacements for stack memory operands
	{
		if(irm.getOpnd(i)->getMemOpndKind() == MemOpndKind_StackAutoLayout) {
			Opnd * dispOpnd=irm.getOpnd(i)->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
			if (dispOpnd==NULL){
				dispOpnd=irm.newImmOpnd(irm.getTypeManager().getInt32Type(), 0);
				irm.getOpnd(i)->setMemOpndSubOpnd(MemOpndSubOpndKind_Displacement, dispOpnd);
			}
			dispOpnd->assignImmValue(0);
		}
	}//end for

	int offset = 0;

	//return EIP area
	retEIPBase = offset;
	offset += 4;

	retEIPEnd = inargBase = offset;

	BasicBlock * bb = irm.getPrologNode();

	uint32 slotSize=4; 

	EntryPointPseudoInst * entryPointInst = irManager.getEntryPointInst();
	assert(entryPointInst->getBasicBlock()==irManager.getPrologNode());
	if (entryPointInst) {//process entry-point instruction
		const StlVector<CallingConventionClient::StackOpndInfo>& stackOpndInfos = 
					((const EntryPointPseudoInst*)entryPointInst)->getCallingConventionClient().getStackOpndInfos(Inst::OpndRole_Def);

		for (uint32 i=0, n=stackOpndInfos.size(); i<n; i++) {//assign displacements for input operands
			Opnd * opnd=entryPointInst->getOpnd(stackOpndInfos[i].opndIndex);
			Opnd * disp = opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
			disp->assignImmValue(offset+stackOpndInfos[i].offset);
		}
	}


	inargEnd = offset;

	icalleeEnd = offset = 0;

	uint32 calleeSavedRegs=irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg);

	uint32 usageRegMask = irManager.getTotalRegUsage(OpndKind_GPReg);

	Inst * lastPush = NULL;

	for (uint32 reg = RegName_EDI; reg>=RegName_EAX; reg--) {//push callee-save registers onto stack
		uint32 mask = getRegMask((RegName)reg);
		if((mask & calleeSavedRegs) && (usageRegMask & mask)) {
			Inst * inst = irm.newInst(Mnemonic_PUSH, irm.getRegOpnd((RegName)reg));
			if (!lastPush)
				lastPush = inst;
			bb->appendInsts(inst, entryPointInst);
			offset -= 4;
		}
	}

	icalleeBase = fcalleeEnd = fcalleeBase = acalleeEnd = acalleeBase = localEnd = offset;

	IRManager::AliasRelation * relations = new(irm.getMemoryManager()) IRManager::AliasRelation[irm.getOpndCount()];
	irm.getAliasRelations(relations);// retrieve relations no earlier than all memory locations are assigned

	for (uint32 i = 0; i<irm.getOpndCount(); i++)//assign displacements for local variable operands
   	{
   		Opnd * opnd = irm.getOpnd(i);
   		if (opnd->getRefCount() == 0)
   			continue;
		if(opnd->getMemOpndKind() == MemOpndKind_StackAutoLayout) {
			Opnd * dispOpnd=opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
			if (dispOpnd->getImmValue()==0) {
				if (relations[opnd->getId()].outerOpnd == NULL) {
					uint32 cb=getByteSize(opnd->getSize());
					cb=(cb+slotSize-1)&~(slotSize-1);
					offset -= cb;
					dispOpnd->assignImmValue(offset);
				}
			}
		}
	}

	localBase = icallerEnd = offset;

	icallerBase = offset;
	if (localEnd>localBase)
		bb->appendInsts(irm.newInst(Mnemonic_SUB, irm.getRegOpnd(RegName_ESP), irm.newImmOpnd(irm.getTypeManager().getInt32Type(), localEnd-localBase)),lastPush ? lastPush : entryPointInst); //set "real" ESP

	frameSize = icalleeEnd -localBase;

}		

void StackLayouter::createEpilog()
{ // Predeccessors of en and irm.isEpilog(en->pred)
    IRManager & irm=getIRManager();
	uint32 calleeSavedRegs=irm.getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg);
	const Edges& inEdges = irm.getExitNode()->getEdges(Direction_In);
	uint32 usageRegMask = irManager.getTotalRegUsage(OpndKind_GPReg);
    for (Edge * edge = inEdges.getFirst(); edge != NULL; edge = inEdges.getNext(edge)) {//check predecessors for epilog(s)
		if (irm.isEpilog(edge->getNode(Direction_Tail))) {
			BasicBlock * epilog = (BasicBlock*)edge->getNode(Direction_Tail);
			Inst * retInst=epilog->getInsts().getLast();
			assert(retInst->hasKind(Inst::Kind_RetInst));
			if (localEnd>localBase)
				epilog->prependInsts(irm.newInst(Mnemonic_ADD, irm.getRegOpnd(RegName_ESP), irm.newImmOpnd(irm.getTypeManager().getInt32Type(), localEnd-localBase)), retInst);//restore stack pointer
			for (uint32 reg = RegName_EDI; reg >= RegName_EAX ; reg--) {//pop callee-save registers
				uint32 mask = getRegMask((RegName)reg);
				if ((mask & calleeSavedRegs) &&  (usageRegMask & mask)) {
					epilog->prependInsts(irm.newInst(Mnemonic_POP, irm.getRegOpnd((RegName)reg)), retInst);
				}
			}
        }
    }
}

}} //namespace Ia32
