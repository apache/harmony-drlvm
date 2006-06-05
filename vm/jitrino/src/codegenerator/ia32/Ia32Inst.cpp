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
 * @version $Revision: 1.14.12.1.4.4 $
 */

#include "Ia32Inst.h"
#include "Ia32CFG.h"
#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{



//=================================================================================================
// class Opnd
//=================================================================================================

//_________________________________________________________________________________________________
void Opnd::assignRegName(RegName r)
{
	r=constraints[ConstraintKind_Calculated].getAliasRegName(r);
	assert(r!=RegName_Null);
	memOpndKind=MemOpndKind_Null;
	regName=r;
	constraints[ConstraintKind_Location]=Constraint(r);
	checkConstraints();
}

//_________________________________________________________________________________________________
void Opnd::assignImmValue(int64 v)
{
	memOpndKind=MemOpndKind_Null;
	if (constraints[ConstraintKind_Location].getKind() != OpndKind_Imm)
		runtimeInfo=NULL;
	immValue=v;
	constraints[ConstraintKind_Location]=Constraint(OpndKind_Imm, constraints[ConstraintKind_Initial].getSize());
	checkConstraints();
}

//_________________________________________________________________________________________________
void Opnd::assignMemLocation(MemOpndKind k, Opnd * _base, Opnd * _index, Opnd * _scale, Opnd * _displacement)
{
	assert(_base||_index||_displacement);
	assert(!_scale||_index);

	constraints[ConstraintKind_Location]=Constraint(OpndKind_Mem, constraints[ConstraintKind_Initial].getSize());
	memOpndKind=k;

	checkConstraints();

	if (_base) 
		setMemOpndSubOpnd(MemOpndSubOpndKind_Base, _base);
	else 
		memOpndSubOpnds[MemOpndSubOpndKind_Base]=NULL;
	if (_index) 
		setMemOpndSubOpnd(MemOpndSubOpndKind_Index, _index);
	else 
		memOpndSubOpnds[MemOpndSubOpndKind_Index]=NULL;
	if (_scale) 
		setMemOpndSubOpnd(MemOpndSubOpndKind_Scale, _scale);
	else 
		memOpndSubOpnds[MemOpndSubOpndKind_Scale]=NULL;
	if (_displacement) 
		setMemOpndSubOpnd(MemOpndSubOpndKind_Displacement, _displacement);
	else 
		memOpndSubOpnds[MemOpndSubOpndKind_Displacement]=NULL;

}

//_________________________________________________________________________________________________
void Opnd::setMemOpndSubOpnd(MemOpndSubOpndKind so, Opnd * opnd)
{
	assert(opnd);
	assert(!(opnd->getConstraint(ConstraintKind_Initial)&getMemOpndSubOpndConstraint(so)).isNull()); // first-level filter
	assert(Constraint(OpndKind_Any, getMemOpndSubOpndConstraint(so).getSize()).contains(opnd->getConstraint(ConstraintKind_Location))); 
	assert(memOpndKind!=MemOpndKind_Null);
	memOpndSubOpnds[so]=opnd;
}

//_________________________________________________________________________________________________
bool Opnd::replaceMemOpndSubOpnd(Opnd * opndOld, Opnd * opndNew)
{
	bool replaced = false;
	if (memOpndKind != MemOpndKind_Null){
		assert(isPlacedIn(OpndKind_Mem));
		for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++)
			if (memOpndSubOpnds[i]!=NULL && memOpndSubOpnds[i]==opndOld) {
				setMemOpndSubOpnd((MemOpndSubOpndKind)i, opndNew);
				replaced = true;
			}
	}
	return replaced;
}

//_________________________________________________________________________________________________
bool Opnd::replaceMemOpndSubOpnds(Opnd * const * opndMap)
{
	bool replaced = false;
	if (memOpndKind != MemOpndKind_Null){
		assert(isPlacedIn(OpndKind_Mem));
		for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++)
			if (memOpndSubOpnds[i]!=NULL && opndMap[memOpndSubOpnds[i]->id]!=NULL){
				setMemOpndSubOpnd((MemOpndSubOpndKind)i, opndMap[memOpndSubOpnds[i]->id]);
				replaced = true;
			}
	}
	return replaced;
}


#ifdef _DEBUG
//_________________________________________________________________________________________________
void Opnd::checkConstraints()
{
	assert(getConstraint(ConstraintKind_Size).contains(getConstraint(ConstraintKind_Initial)));
	assert(constraints[ConstraintKind_Initial].contains(constraints[ConstraintKind_Calculated]));
	assert(constraints[ConstraintKind_Calculated].contains(constraints[ConstraintKind_Location]));
}
#endif

//_________________________________________________________________________________________________
void Opnd::setCalculatedConstraint(Constraint c)
{
	constraints[ConstraintKind_Calculated]=c & constraints[ConstraintKind_Initial];
	checkConstraints();
}

//_________________________________________________________________________________________________
void Opnd::addRefCount(uint32& index, uint32 blockExecCount)
{
	if (blockExecCount==0)
		blockExecCount=1;
	refCount++;
	if (id==EmptyUint32)
		id=index++;
	if (memOpndKind != MemOpndKind_Null){
		for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++)
			if (memOpndSubOpnds[i]!=NULL){
				memOpndSubOpnds[i]->addRefCount(index, blockExecCount);
			}
	}
}

//_________________________________________________________________________________________________
void Opnd::setDefiningInst(Inst * inst)
{
	if (defScope==DefScope_Temporary||defScope==DefScope_Null){
		if (definingInst==NULL){
			defScope=DefScope_Temporary;
			definingInst=inst;
		}else{
			if (inst->getBasicBlock()==definingInst->getBasicBlock())
				defScope=DefScope_SemiTemporary;
			else{
				defScope=DefScope_Variable;
				definingInst=NULL;
			}
		}
	}else if (defScope==DefScope_SemiTemporary){
		assert(definingInst!=NULL);		
		if (inst->getBasicBlock()!=definingInst->getBasicBlock()){
			defScope=DefScope_Variable;
			definingInst=NULL;
		}
	}
}

//=================================================================================================
// class Inst
//=================================================================================================

//_________________________________________________________________________________________________
void Inst::insertOpnd(uint32 idx, Opnd * opnd, uint32 roles)
{
	assert(opndCount < allocatedOpndCount);
	
	if (idx < defOpndCount)
		roles |= OpndRole_Def;

	if (roles & OpndRole_Def){
		if (idx > defOpndCount)
			idx = defOpndCount;
		defOpndCount++;
	}
	
	if (idx > opndCount)
		idx = opndCount;

	uint32 * opndRoles = getOpndRoles();
	Constraint * opndConstraints = getConstraints();

	for (uint32 i = opndCount; i > idx; i--){
		opnds[i] = opnds[i - 1];
		opndRoles[i] = opndRoles[i - 1];
		opndConstraints[i] = opndConstraints[i - 1];
	}

	opnds[idx] = opnd;
	opndRoles[idx] = roles;
	
	opndCount++;
}

//_________________________________________________________________________________________________
uint32 Inst::countOpnds(uint32 roles)const
{
	uint32 count=0;
	if ( (roles & OpndRole_InstLevel) != 0 ){
		const uint32 * opndRoles = getOpndRoles();
		for (uint32 i = 0, n = opndCount; i < n; i++){
			uint32 r = opndRoles [i];
			if ( (r & roles & OpndRole_FromEncoder) != 0 && (r & roles & OpndRole_ForIterator) != 0 )
				count++;
		}
	}
	if ( (roles & OpndRole_OpndLevel) != 0 && (roles & OpndRole_Use) != 0  ){
		for (uint32 i = 0, n = opndCount; i < n; i++){
			const Opnd * opnd = opnds[i];
			if ( opnd->memOpndKind != MemOpndKind_Null ){
				const Opnd * const * subOpnds = opnd->getMemOpndSubOpnds();
				for (uint32 j = 0; j < MemOpndSubOpndKind_Count; j++){
					const Opnd * subOpnd = subOpnds[j];
					if (subOpnd != NULL)
						count++;
				}
			}
		}
	}
	return count;
}

//_________________________________________________________________________________________________
Constraint Inst::getConstraint(ConstraintKind ck, uint32 idx, OpndSize size)const
{ 
	Constraint c(OpndKind_Any); 
	if (idx<opndCount){
		const uint32 * opndRoles = getOpndRoles();
		if ( ck==ConstraintKind_Weak || (opndRoles[idx] & OpndRole_Explicit) == 0 ){
			c = getConstraints()[idx];
		}else{
			idx = getExplicitOpndIndexFromOpndRoles(opndRoles[idx]);
			if (ck==ConstraintKind_Current){
				if (form==Form_Extended)
					idx=opcodeGroupDescription->extendedToNativeMap[idx]; 
				assert(idx<IRMaxNativeOpnds);
				Encoder::FindInfo fi;
				initFindInfo(fi, Opnd::ConstraintKind_Location);
				c=Constraint();
				for (uint32 i=0, n=opcodeGroupDescription->opcodeDescriptionCount; i<n; i++)
					if (Encoder::matches(opcodeGroupDescription->opcodeDescriptions + i, fi))
						c.unionWith(opcodeGroupDescription->opcodeDescriptions[i].opndConstraints[idx]);
				assert(!c.isNull());
			}else{
				if (form==Form_Extended)
					idx=opcodeGroupDescription->extendedToNativeMap[idx]; 
				assert(idx<IRMaxNativeOpnds);
				if (ck==ConstraintKind_Weak)
					c=opcodeGroupDescription->weakOpndConstraints[idx];
				else
					c=opcodeGroupDescription->strongOpndConstraints[idx];
			}
		}
	}else{
		uint32 diffIndex = idx - opndCount;
		Opnd * instOpnd = *(Opnd * const *)(((Byte*)opnds) + (diffIndex & ~3));
		c = instOpnd->getMemOpndSubOpndConstraint((MemOpndSubOpndKind)(diffIndex & 3));
	}
	return size==OpndSize_Null?c:c.getAliasConstraint(size);
}

//_________________________________________________________________________________________________
void Inst::setOpnd(uint32 index, Opnd * opnd)
{
	Opnd ** opnds = getOpnds();
	if (index < opndCount)
		opnds[index] = opnd;
	else{
		uint32 diffIndex = index - opndCount;
		Opnd * instOpnd = *(Opnd * const *)(((Byte*)opnds) + (diffIndex & ~3));
		instOpnd->setMemOpndSubOpnd((MemOpndSubOpndKind)(diffIndex & 3), opnd);
	}
	verify();
}

//_________________________________________________________________________________________________
bool Inst::replaceOpnd(Opnd * oldOpnd, Opnd * newOpnd, uint32 opndRoleMask)
{
	bool replaced = false;
	assert(newOpnd != NULL);
	for (uint32 i=0, n=getOpndCount(OpndRole_InstLevel|OpndRole_UseDef); i<n; i++){
		uint32 opndRole=getOpndRoles(i);
		if (
			(opndRole&opndRoleMask&OpndRole_FromEncoder)!=0 && 
			(opndRole&opndRoleMask&OpndRole_ForIterator)!=0
			){
			Opnd * opnd=getOpnd(i);
			if (opnd==oldOpnd){
				assert((opndRole&OpndRole_Changeable)!=0);
				setOpnd(i, newOpnd);
				opnd=newOpnd;
				replaced = true;
			}
			if ((opndRoleMask&OpndRole_OpndLevel)!=0)
				replaced |= opnd->replaceMemOpndSubOpnd(oldOpnd, newOpnd);
		}
	}

	return replaced;
}

//_________________________________________________________________________________________________
bool Inst::replaceOpnds(Opnd * const * opndMap, uint32 opndRoleMask)
{
	bool replaced = false;
	for (uint32 i=0, n=getOpndCount(OpndRole_InstLevel|OpndRole_UseDef); i<n; i++){
		uint32 opndRole=getOpndRoles(i);
		if (
			(opndRole&opndRoleMask&OpndRole_FromEncoder)!=0 && 
			(opndRole&opndRoleMask&OpndRole_ForIterator)!=0
			){
			Opnd * opnd=getOpnd(i);
			Opnd * newOpnd=opndMap[opnd->getId()];
			if (newOpnd!=NULL){
				assert((opndRole&OpndRole_Changeable)!=0);
				setOpnd(i, newOpnd);
				opnd=newOpnd;
				replaced = true;
			}
			if ((opndRoleMask&OpndRole_OpndLevel)!=0)
				replaced |= opnd->replaceMemOpndSubOpnds(opndMap);
		}
	}
	return replaced;
}

//_________________________________________________________________________________________________
void Inst::swapOperands(uint32 idx0, uint32 idx1)
{
	Opnd * opnd0=getOpnd(idx0), * opnd1=getOpnd(idx1);
	setOpnd(idx1, opnd0);
	setOpnd(idx0, opnd1);
	verify();
}

//_________________________________________________________________________________________________
void Inst::changeInstCondition(ConditionMnemonic cc, IRManager * irManager)
{
	assert(getProperties()&Properties_Conditional);
	Mnemonic baseMnemonic=getBaseConditionMnemonic(mnemonic);
	assert(baseMnemonic!=Mnemonic_Null);
	mnemonic=(Mnemonic)(baseMnemonic+cc);
	assignOpcodeGroup(irManager);
	assert(opcodeGroupDescription!=NULL);
}

//_________________________________________________________________________________________________
void Inst::reverse(IRManager * irManager)
{
	assert(canReverse());
	Mnemonic baseMnemonic=getBaseConditionMnemonic(mnemonic);
	assert(baseMnemonic!=Mnemonic_Null);
	changeInstCondition(reverseConditionMnemonic((ConditionMnemonic)(mnemonic-baseMnemonic)), irManager);
}

//_________________________________________________________________________________________________
uint8* Inst::emit(uint8* stream)const
{
	if (hasKind(Inst::Kind_PseudoInst))
		return stream;
	assert(mnemonic!=Mnemonic_Null);
	assert(form==Form_Native);
	return Encoder::emit(stream, this);
}

//_________________________________________________________________________________________________
void Inst::initFindInfo(Encoder::FindInfo& fi, Opnd::ConstraintKind opndConstraintKind)const
{
	Opnd * const * opnds = getOpnds();
	const uint32 * roles = getOpndRoles();
	uint32 count = 0, defCount = 0, defCountFromInst = defOpndCount;
	for (uint32 i=0, n=opndCount; i<n; i++){
		uint32 r = roles[i];
		if (r & Inst::OpndRole_Explicit){
			fi.opndConstraints[count++] = opnds[i]->getConstraint(opndConstraintKind);
			if (i < defCountFromInst)
				defCount++;
		}
	}
	fi.defOpndCount = defCount;
	fi.opndCount = count;
	fi.opcodeGroupDescription = opcodeGroupDescription;
	fi.mnemonic = mnemonic;
	fi.isExtended = (Form)form == Form_Extended;
}

//_________________________________________________________________________________________________
void Inst::fixOpndsForOpcodeGroup(IRManager * irManager)
{
	uint32 handledExplicitOpnds = 0, handledImplicitOpnds = 0;

	uint32 * roles = getOpndRoles();
	Constraint * constraints = getConstraints();

	Form f = getForm();

	for (uint32 i=0, n=opndCount; i<n; i++){
		if (roles[i] & Inst::OpndRole_Explicit){
			uint32 idx, r;  
			if (f == Form_Native){
				idx = handledExplicitOpnds; 
				r = Encoder::getOpndRoles(opcodeGroupDescription->opndRoles, idx);
				if ( (r & OpndRole_Def) != 0 && defOpndCount<=i)
					defOpndCount = i + 1;
			}else{
				idx = opcodeGroupDescription->extendedToNativeMap[handledExplicitOpnds];
				r = i < defOpndCount ? OpndRole_Def : OpndRole_Use;
			}
			r |= Inst::OpndRole_Explicit | (handledExplicitOpnds << 16);
			roles[i] = r;
			constraints[i] = opcodeGroupDescription->weakOpndConstraints[idx];
			handledExplicitOpnds++;
		}else if (roles[i] & Inst::OpndRole_Implicit){
			assert(handledImplicitOpnds < opcodeGroupDescription->implicitOpndRoles.count);
			assert(opnds[i]->getRegName() == opcodeGroupDescription->implicitOpndRegNames[handledImplicitOpnds]);
			handledImplicitOpnds++;
		}
	}

	for (uint32 i = handledImplicitOpnds; i < opcodeGroupDescription->implicitOpndRoles.count; i++){
		RegName iorn = opcodeGroupDescription->implicitOpndRegNames[i];
		Opnd * implicitOpnd = irManager->getRegOpnd(iorn);
		uint32 implicitOpndRoles = 
			Encoder::getOpndRoles(opcodeGroupDescription->implicitOpndRoles, i) | Inst::OpndRole_Implicit;
		if (implicitOpndRoles & OpndRole_Def){
			insertOpnd(defOpndCount, implicitOpnd, implicitOpndRoles);
			constraints[defOpndCount - 1] = Constraint(iorn);
		}else{
			insertOpnd(opndCount, implicitOpnd, implicitOpndRoles);
			constraints[opndCount - 1] = Constraint(iorn);
		}
	}
}

//_________________________________________________________________________________________________
void Inst::assignOpcodeGroup(IRManager * irManager)
{	
	if (hasKind(Inst::Kind_PseudoInst)){
		assert(getForm() == Form_Extended);
		opcodeGroupDescription = Encoder::getDummyOpcodeGroupDescription();
		uint32 * roles = getOpndRoles();
		uint32 i = 0;
		for (uint32 n = defOpndCount; i < n; i++)
			roles[i] = OpndRole_Auxilary | OpndRole_Def;
		for (uint32 n = opndCount; i < n; i++)
			roles[i] = OpndRole_Auxilary | OpndRole_Use;
	}else{
		Encoder::FindInfo fi;
		initFindInfo(fi, Opnd::ConstraintKind_Initial);
		opcodeGroupDescription=Encoder::findOpcodeGroupDescription(fi);
		assert(opcodeGroupDescription);  // Checks that the requested mnemonic is implemented in Ia32EncodingTable
		fixOpndsForOpcodeGroup(irManager);
	}
	properties = opcodeGroupDescription->properties;
}

//_________________________________________________________________________________________________
void Inst::makeNative(IRManager * irManager)
{
	if (getForm()==Form_Native  || hasKind(Kind_PseudoInst) )
		return;
	assert(opcodeGroupDescription);

	BasicBlock * bb = basicBlock;

	uint32 * opndRoles = getOpndRoles();
	Constraint * constraints = getConstraints();

	int32 defs[IRMaxNativeOpnds]={ -1, -1, -1, -1 };
	for (uint32 i=0; i<opndCount; i++){
		uint32 r = opndRoles[i];
		if ((r & OpndRole_Explicit) == 0) continue;
		uint32 extendedIdx = r >> 16;
		uint32 nativeIdx = opcodeGroupDescription->extendedToNativeMap[extendedIdx];
		assert(nativeIdx < IRMaxNativeOpnds);
		int32 defNativeIdx = defs[nativeIdx];
		if (defNativeIdx != -1){
			assert(i >= defOpndCount);
			opndRoles[defNativeIdx] |= OpndRole_Use;
			if (bb && opnds[defNativeIdx] != opnds[i]){
				Inst * moveInst=irManager->newCopySequence(Mnemonic_MOV, opnds[defNativeIdx], opnds[i]); 
				bb->prependInsts(moveInst, this);
			}
			opnds[i] = NULL;
		}else
			defs[nativeIdx] = i;
	}

	uint32 packedOpnds = 0, explicitOpnds = 0;
	for (uint32 i = 0, n = opndCount; i < n; i++){
		if (opnds[i] == NULL) continue;
		uint32 r = opndRoles[i];
		if ((r & OpndRole_Explicit) != 0){
			r = (r & 0xffff) | (explicitOpnds << 16);
			explicitOpnds++;
		}
		if (i > packedOpnds){
			opnds[packedOpnds] = opnds[i];
			constraints[packedOpnds] = constraints[i];
			opndRoles[packedOpnds] = r;
		}
		packedOpnds++;
	}

	opndCount=packedOpnds;
	form = Form_Native;
}

//_________________________________________________________________________________________________
void * Inst::getCodeStartAddr()const
{
	return basicBlock!=NULL?(uint8*)basicBlock->getCodeStartAddr()+codeOffset:0;
}

#ifdef _DEBUG
//_________________________________________________________________________________________________
void Inst::verify()
{
}
#endif

//=========================================================================================================
void appendToInstList(Inst *& head, Inst * listToAppend)
{
	if (head==NULL)
		head=listToAppend;
	else if (listToAppend!=NULL){
        Inst * inst=NULL, * nextInst=listToAppend, * lastInst=listToAppend->getPrev(); 
		Inst * after=head->getPrev();
        do { 
            inst=nextInst;
            nextInst=inst->getNext();
			assert(inst->basicBlock==NULL);
            inst->unlink();
			inst->insertAfter(after);
			after=inst;
        } while (inst!=lastInst);
	}
}

//=========================================================================================================
//   class GCInfoPseudoInst
//=========================================================================================================

GCInfoPseudoInst::GCInfoPseudoInst(IRManager* irm, int id)
: Inst(Mnemonic_NULL, id, Inst::Form_Extended), 
offsets(irm->getMemoryManager()), desc(NULL)
{ 
    kind = Kind_GCInfoPseudoInst;
}

//=================================================================================================
// class BranchInst
//=================================================================================================

//_________________________________________________________________________________________________
BasicBlock * BranchInst::getDirectBranchTarget()const
{
	assert(basicBlock!=NULL);
	Edge * directBranchEdge=basicBlock->getDirectBranchEdge();
	return directBranchEdge?(BasicBlock*)directBranchEdge->getNode(Direction_Head):0;
}

//_________________________________________________________________________________________________
void BranchInst::reverse(IRManager * irManager)
{
	ControlTransferInst::reverse(irManager);
	Edge * fte=basicBlock->getFallThroughEdge();
	assert(basicBlock->getDirectBranchEdge()!=NULL && fte!=NULL);
	basicBlock->makeEdgeDirectBranch(fte);
}

//_________________________________________________________________________________________________
bool BranchInst::canReverse()const
{
	return basicBlock!=NULL&&isDirect()&&ControlTransferInst::canReverse();
}

//=========================================================================================================
//   class SwitchInst
//=========================================================================================================
//_________________________________________________________________________________________________
BasicBlock * SwitchInst::getTarget(uint32 i)const
{
	Opnd * opnd=getOpnd(0);
	Opnd * tableAddr=opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
	Opnd::RuntimeInfo * ri=tableAddr->getRuntimeInfo();
	assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
	ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
	assert(i<cai->getSize()/sizeof(BasicBlock*));
	return ((BasicBlock**)cai->getValue())[i];
}

//_________________________________________________________________________________________________
void SwitchInst::setTarget(uint32 i, BasicBlock * bb)
{
	Opnd * opnd=getOpnd(0);
	Opnd * tableAddr=opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
	Opnd::RuntimeInfo * ri=tableAddr->getRuntimeInfo();
	assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
	ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
	assert(i<cai->getSize()/sizeof(BasicBlock*));
	((BasicBlock**)cai->getValue())[i]=bb;
}

//_________________________________________________________________________________________________
void SwitchInst::replaceTarget(BasicBlock * bbFrom, BasicBlock * bbTo)
{
    Opnd * opnd=getOpnd(0);
    Opnd * tableAddr=opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
    Opnd::RuntimeInfo * ri=tableAddr->getRuntimeInfo();
    assert(ri->getKind()==Opnd::RuntimeInfo::Kind_ConstantAreaItem);
    ConstantAreaItem * cai = (ConstantAreaItem *)ri->getValue(0);
    BasicBlock ** bbs=(BasicBlock**)cai->getValue();
    for (uint32 i=0, n=cai->getSize()/sizeof(BasicBlock*); i<n; i++) {
        if (bbs[i]==bbFrom) {
            bbs[i]=bbTo; 
            return;
        }
    }
    assert(0);
}


//=========================================================================================================
//   class CallingConventionClient
//=========================================================================================================
//_________________________________________________________________________________________________
void CallingConventionClient::finalizeInfos(Inst::OpndRole role, CallingConvention::ArgKind argKind)
{
	assert(callingConvention!=NULL);
	StlVector<CallingConvention::OpndInfo> & infos = getInfos(role);
	callingConvention->getOpndInfo(argKind, infos.size(), &infos.front());
	bool lastToFirst=callingConvention->pushLastToFirst();
	uint32 slotNumber=0;
	for (
		uint32 i=lastToFirst?0:infos.size()-1, 
		end=lastToFirst?infos.size():(uint32)-1, 
		inc=lastToFirst?1:-1;
		i!=end;
		i+=inc
		){
		CallingConvention::OpndInfo & info=infos[i];
		for (uint32 j=0; j<info.slotCount; j++){
			if (info.slots[j]==RegName_Null)
                info.slots[j]=(RegName)( (OpndKind_Mem << 24) | (OpndSize_32 << 16) | (0xFFFF & slotNumber++) );
		}
	}
	(role==Inst::OpndRole_Def?defArgStackDepth:useArgStackDepth)=slotNumber*4; // 
}

//_________________________________________________________________________________________________
void CallingConventionClient::layoutAuxilaryOpnds(Inst::OpndRole role, OpndKind kindForStackArgs)
{
	StlVector<CallingConvention::OpndInfo> & infos = getInfos(role);
	StlVector<StackOpndInfo> & stackOpndInfos = getStackOpndInfos(role);
	uint32 slotSize=4;
	uint32 regArgCount=0, stackArgCount=0;
	Inst::Opnds opnds(ownerInst, Inst::OpndRole_Auxilary|role);
	Inst::Opnds::iterator handledOpnds=opnds.begin();
	for (uint32 i=0, n=infos.size(); i<n; i++){
		const CallingConvention::OpndInfo& info=infos[i];
#ifdef _DEBUG
		bool eachSlotRequiresOpnd=false;
#endif
		uint32 offset=0;
		for (uint32 j=0, cbCurrent=0; j<info.slotCount; j++){
			Opnd * opnd=opnds.getOpnd(handledOpnds);
			OpndSize sz=opnd->getSize();
			uint32 cb=getByteSize(sz);
			RegName r=info.slots[j];
			if ((getRegKind(r)&OpndKind_Reg)!=0) {
				r=Constraint::getAliasRegName(r,sz);
				assert(r!=RegName_Null);
#ifdef _DEBUG
				eachSlotRequiresOpnd=true;
#endif
				cbCurrent+=getByteSize(getRegSize(r));
			}else{
				if (cbCurrent==0)
					offset=(info.slots[j] & 0xffff)*slotSize;
				cbCurrent+=slotSize;  
			}

			if (cbCurrent>=cb){
				if ((getRegKind(r)&OpndKind_Reg)!=0){
					ownerInst->setConstraint(handledOpnds, r);
					regArgCount++;
				}else{
					ownerInst->setConstraint(handledOpnds, Constraint(kindForStackArgs, sz));
					stackArgCount++;
					StackOpndInfo sainfo={ handledOpnds, offset };
					stackOpndInfos.push_back(sainfo);
				}
				handledOpnds = opnds.next(handledOpnds);
#ifdef _DEBUG
				eachSlotRequiresOpnd=false;
#endif
				cbCurrent=0;
			}
#ifdef _DEBUG
			assert(!eachSlotRequiresOpnd); 
#endif
		}
	}
	if (stackArgCount>0)
		sort(stackOpndInfos.begin(), stackOpndInfos.end());
	assert(handledOpnds == opnds.end());
	assert(stackArgCount==stackOpndInfos.size());
}


//=========================================================================================================
//   class EntryPointPseudoInst
//=========================================================================================================
EntryPointPseudoInst::EntryPointPseudoInst(IRManager * irm, int id, const CallingConvention * cc)
	: Inst(Mnemonic_NULL, id, Inst::Form_Extended), callingConventionClient(irm->getMemoryManager(), cc)
{	kind=Kind_EntryPointPseudoInst;  callingConventionClient.setOwnerInst(this); }

//_________________________________________________________________________________________________________
Opnd * EntryPointPseudoInst::getDefArg(uint32 i)const
{ 
	return NULL;
}


//=================================================================================================
// class CallInst
//=================================================================================================
//_________________________________________________________________________________________________
CallInst::CallInst(IRManager * irm, int id, const CallingConvention * cc, InlineInfo* ii)  
		: ControlTransferInst(Mnemonic_CALL, id), callingConventionClient(irm->getMemoryManager(), cc), inlineInfo(NULL)
{ 
    if (ii && (!ii->isEmpty()) ) {
        inlineInfo = ii;
    }
	kind=Kind_CallInst; 
	callingConventionClient.setOwnerInst(this); 
}

//_________________________________________________________________________________________________
Constraint CallInst::getCallerSaveRegs(OpndKind regKind)const 
{
	Constraint calleeSaveRegs=getCalleeSaveRegs(regKind);
	Constraint allRegs=Encoder::getAllRegs(regKind);
	uint32 mask=allRegs.getMask()&~calleeSaveRegs.getMask();
	if (regKind==OpndKind_GPReg){
		mask&=~getRegMask(RegName_ESP);
	}
	return mask!=0?Constraint(regKind, allRegs.getSize(), mask):Constraint();
}


//=================================================================================================
// class RetInst
//=================================================================================================
//_________________________________________________________________________________________________
RetInst::RetInst(IRManager * irm, int id)  
		: ControlTransferInst(Mnemonic_RET, id), 
		callingConventionClient(irm->getMemoryManager(), irm->getCallingConvention())
{ 
	kind=Kind_RetInst; 
	callingConventionClient.setOwnerInst(this); 
}


}};  // namespace Ia32

