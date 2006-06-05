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
 * @version $Revision: 1.24.8.4.4.3 $
 */

#include "Ia32IRManager.h"
#include "Ia32Encoder.h"
#include "Ia32Printer.h"
#include "Log.h"
#include "Ia32Printer.h"
#include "Ia32CodeGenerator.h"
namespace Jitrino
{
namespace Ia32{

//_________________________________________________________________________________________________
const char * newString(MemoryManager& mm, const char * str, uint32 length)
{
	assert(str!=NULL);
	if (length==EmptyUint32)
		length=strlen(str);
	char * psz=new(mm) char[length+1];
	strncpy(psz, str, length);
	psz[length]=0;
	return psz;
}


//_____________________________________________________________________________________________
IRManager::IRManager(MemoryManager& memManager, TypeManager& tm, MethodDesc& md, CompilationInterface& compIface)
	:CFG(memManager), typeManager(tm), methodDesc(md), compilationInterface(compIface),
		opndId(0), instId(0),
		opnds(memManager), gpTotalRegUsage(0), entryPointInst(NULL), _hasLivenessInfo(false),
		internalHelperInfos(memManager), infoMap(memManager), verificationLevel(0),
		hasCalls(false), hasNonExceptionCalls(false)

{  
	for (uint32 i=0; i<lengthof(regOpnds); i++) regOpnds[i]=NULL;
	initInitialConstraints();
	registerInternalHelperInfo("printRuntimeOpndInternalHelper", IRManager::InternalHelperInfo((void*)&printRuntimeOpndInternalHelper, &CallingConvention_STDCALL));
}


//_____________________________________________________________________________________________
void IRManager::addOpnd(Opnd * opnd)
{ 
	assert(opnd->id>=opnds.size());
	opnds.push_back(opnd);
	opnd->id=opnds.size()-1;
}

//_____________________________________________________________________________________________
Opnd * IRManager::newOpnd(Type * type)
{
	Opnd * opnd=new(memoryManager) Opnd(opndId++, type, getInitialConstraint(type));
	addOpnd(opnd);
	return opnd;
}

//_____________________________________________________________________________________________
Opnd * IRManager::newOpnd(Type * type, Constraint c)
{
	c.intersectWith(Constraint(OpndKind_Any, getTypeSize(type)));
	assert(!c.isNull());
	Opnd * opnd=new(memoryManager) Opnd(opndId++, type, c);
	addOpnd(opnd);
	return opnd;
}

//_____________________________________________________________________________________________
Opnd * IRManager::newImmOpnd(Type * type, int64 immediate)
{
	Opnd * opnd = newOpnd(type);
	opnd->assignImmValue(immediate);
	return opnd;
}

//____________________________________________________________________________________________
Opnd * IRManager::newImmOpnd(Type * type, Opnd::RuntimeInfo::Kind kind, void * arg0, void * arg1, void * arg2, void * arg3)
{
	Opnd * opnd=newImmOpnd(type, 0);
	opnd->setRuntimeInfo(new(memoryManager) Opnd::RuntimeInfo(kind, arg0, arg1, arg2, arg3));
	return opnd;
}

//_____________________________________________________________________________________________
ConstantAreaItem * IRManager::newConstantAreaItem(float f)
{
	return new(memoryManager) ConstantAreaItem(
		ConstantAreaItem::Kind_FPSingleConstantAreaItem, sizeof(float), 
		new(memoryManager) float(f)
	);
}

//_____________________________________________________________________________________________
ConstantAreaItem *	IRManager::newConstantAreaItem(double d)
{
	return new(memoryManager) ConstantAreaItem(
		ConstantAreaItem::Kind_FPDoubleConstantAreaItem, sizeof(double), 
		new(memoryManager) double(d)
	);
}

//_____________________________________________________________________________________________
ConstantAreaItem *	IRManager::newSwitchTableConstantAreaItem(uint32 numTargets)
{
	return new(memoryManager) ConstantAreaItem(
		ConstantAreaItem::Kind_SwitchTableConstantAreaItem, sizeof(BasicBlock*)*numTargets, 
		new(memoryManager) BasicBlock*[numTargets]
	);
}

//_____________________________________________________________________________________________
ConstantAreaItem *	IRManager::newInternalStringConstantAreaItem(const char * str)
{
	if (str==NULL)
		str="";
	return new(memoryManager) ConstantAreaItem(
		ConstantAreaItem::Kind_InternalStringConstantAreaItem, strlen(str)+1, 
		(void*)newInternalString(str)
	);
}

//_____________________________________________________________________________________________
ConstantAreaItem * IRManager::newBinaryConstantAreaItem(uint32 size, const void * pv)
{
	return new(memoryManager) ConstantAreaItem(ConstantAreaItem::Kind_BinaryConstantAreaItem, size, pv);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newFPConstantMemOpnd(float f)
{
	ConstantAreaItem * item=newConstantAreaItem(f);
	Opnd * addr=newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_ConstantAreaItem, item);
	return newMemOpndAutoKind(typeManager.getSingleType(), MemOpndKind_ConstantArea, addr);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newFPConstantMemOpnd(double d)
{
	ConstantAreaItem * item=newConstantAreaItem(d);
	Opnd * addr=newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_ConstantAreaItem, item);
	return newMemOpndAutoKind(typeManager.getDoubleType(), MemOpndKind_ConstantArea, addr);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newSwitchTableConstantMemOpnd(uint32 numTargets, Opnd * index)
{
	ConstantAreaItem * item=newSwitchTableConstantAreaItem(numTargets);
	Opnd * switchTable=newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_ConstantAreaItem, item);
	return newMemOpnd(typeManager.getIntPtrType(), MemOpndKind_ConstantArea, 0, index, newImmOpnd(typeManager.getInt32Type(), 4), switchTable);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newInternalStringConstantImmOpnd(const char * str)
{
	ConstantAreaItem * item=newInternalStringConstantAreaItem(str);
	return newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_ConstantAreaItem, item);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newBinaryConstantImmOpnd(uint32 size, const void * pv)
{
	ConstantAreaItem * item=newBinaryConstantAreaItem(size, pv);
	return newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_ConstantAreaItem, item);
}

//_____________________________________________________________________________________________
SwitchInst * IRManager::newSwitchInst(uint32 numTargets, Opnd * index)
{
	SwitchInst * inst=new(memoryManager, 1) SwitchInst(Mnemonic_JMP, instId++);
	assert(numTargets>0);
	assert(index!=NULL);
	Opnd * targetOpnd=newSwitchTableConstantMemOpnd(numTargets, index);
	inst->insertOpnd(0, targetOpnd, Inst::OpndRole_Explicit);
	inst->assignOpcodeGroup(this);
	inst->verify();
	return inst;
}

//_____________________________________________________________________________________________
Opnd * IRManager::newRegOpnd(Type * type, RegName reg)
{
	Opnd * opnd = newOpnd(type, Constraint(getRegKind(reg)));
	opnd->assignRegName(reg);
	return opnd;
}

//_____________________________________________________________________________________________
Opnd * IRManager::newMemOpnd(Type * type, MemOpndKind k, Opnd * base, Opnd * index, Opnd * scale, Opnd * displacement, RegName baseReg)
{
	Opnd * opnd = newOpnd(type);
	opnd->assignMemLocation(k,base,index,scale,displacement);
        if (baseReg != RegName_Null)
            opnd->setBaseReg(baseReg);
	return opnd;
}

//_________________________________________________________________________________________________
Opnd * IRManager::newMemOpnd(Type * type, Opnd * base, Opnd * index, Opnd * scale, Opnd * displacement, RegName baseReg)
{
	return newMemOpnd(type, MemOpndKind_Heap, base, index, scale, displacement, baseReg);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newMemOpnd(Type * type, MemOpndKind k, Opnd * base, int32 displacement, RegName baseReg)
{
	return newMemOpnd(type, k, base, 0, 0, newImmOpnd(typeManager.getInt32Type(), displacement), baseReg);
}

//_____________________________________________________________________________________________
Opnd * IRManager::newMemOpndAutoKind(Type * type, MemOpndKind k, Opnd * opnd0, Opnd * opnd1, Opnd * opnd2)
{  
	Opnd * base=NULL, * displacement=NULL;

	Constraint c=opnd0->getConstraint(Opnd::ConstraintKind_Current);
	if (!(c&OpndKind_GPReg).isNull()){
		base=opnd0;
	}else if (!(c&OpndKind_Imm).isNull()){
		displacement=opnd0;
	}else
		assert(0);

	if (opnd1!=NULL){
		c=opnd1->getConstraint(Opnd::ConstraintKind_Current);
		if (!(c&OpndKind_GPReg).isNull()){
			base=opnd1;
		}else if (!(c&OpndKind_Imm).isNull()){
			displacement=opnd1;
		}else
			assert(0);
	}

	return newMemOpnd(type, k, base, 0, 0, displacement);
}

//_____________________________________________________________________________________________
void IRManager::initInitialConstraints()
{
	for (uint32 i=0; i<lengthof(initialConstraints); i++)
		initialConstraints[i] = createInitialConstraint((Type::Tag)i);
}

//_____________________________________________________________________________________________
Constraint IRManager::createInitialConstraint(Type::Tag t)const
{
	OpndSize sz=getTypeSize(t);
	if (t==Type::Single||t==Type::Double||t==Type::Float)
		return Constraint(OpndKind_XMMReg, sz)|Constraint(OpndKind_Mem, sz);
	if (sz<=Constraint::getDefaultSize(OpndKind_GPReg))
		return Constraint(OpndKind_GPReg, sz)|Constraint(OpndKind_Mem, sz)|Constraint(OpndKind_Imm, sz);
	if (sz==OpndSize_64)
		return Constraint(OpndKind_Mem, sz)|Constraint(OpndKind_Imm, sz); // imm before lowering
	return Constraint(OpndKind_Memory, sz);
}

//_____________________________________________________________________________________________
Inst * IRManager::newInst(Mnemonic mnemonic, Opnd * opnd0, Opnd * opnd1, Opnd * opnd2)
{
	Inst * inst = new(memoryManager, 4) Inst(mnemonic, instId++, Inst::Form_Native);
	uint32 i=0;
	Opnd ** opnds = inst->getOpnds();
	uint32 * roles = inst->getOpndRoles();
	if (opnd0!=NULL){ opnds[i] = opnd0;	roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd1!=NULL){ opnds[i] = opnd1;	roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd2!=NULL){ opnds[i] = opnd2;	roles[i] = Inst::OpndRole_Explicit; i++;
	}}}
	inst->opndCount = i;
	inst->assignOpcodeGroup(this);
	inst->verify();
	return inst;
}

//_____________________________________________________________________________________________
Inst * IRManager::newInst(Mnemonic mnemonic, 
		Opnd * opnd0, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, 
		Opnd * opnd4, Opnd * opnd5, Opnd * opnd6, Opnd * opnd7
	)
{
	Inst * inst = new(memoryManager, 8) Inst(mnemonic, instId++, Inst::Form_Native);
	uint32 i=0;
	Opnd ** opnds = inst->getOpnds();
	uint32 * roles = inst->getOpndRoles();
	if (opnd0!=NULL){ opnds[i] = opnd0; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd1!=NULL){ opnds[i] = opnd1; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd2!=NULL){ opnds[i] = opnd2; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd3!=NULL){ opnds[i] = opnd3; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd4!=NULL){ opnds[i] = opnd4; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd5!=NULL){ opnds[i] = opnd5; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd6!=NULL){ opnds[i] = opnd6; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd7!=NULL){ opnds[i] = opnd7; roles[i] = Inst::OpndRole_Explicit; i++;
	}}}}}}}};	
	inst->opndCount = i;
	inst->assignOpcodeGroup(this);
	inst->verify();
	return inst;
}

//_____________________________________________________________________________________________
Inst * IRManager::newInstEx(Mnemonic mnemonic, uint32 defCount, Opnd * opnd0, Opnd * opnd1, Opnd * opnd2)
{
	Inst * inst = new(memoryManager, 4) Inst(mnemonic, instId++, Inst::Form_Extended);
	uint32 i=0;
	Opnd ** opnds = inst->getOpnds();
	uint32 * roles = inst->getOpndRoles();
	if (opnd0!=NULL){ 
		opnds[i] = opnd0; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd1!=NULL){ 
		opnds[i] = opnd1; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd2!=NULL){ 
		opnds[i] = opnd2; roles[i] = Inst::OpndRole_Explicit; i++;
	}}}
	inst->opndCount = i;
	inst->defOpndCount=defCount;
	inst->assignOpcodeGroup(this);
	inst->verify();
	return inst;
}

//_____________________________________________________________________________________________
Inst * IRManager::newInstEx(Mnemonic mnemonic, uint32 defCount, 
		Opnd * opnd0, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, 
		Opnd * opnd4, Opnd * opnd5, Opnd * opnd6, Opnd * opnd7
	)
{
	Inst * inst = new(memoryManager, 8) Inst(mnemonic, instId++, Inst::Form_Extended);
	uint32 i=0;
	Opnd ** opnds = inst->getOpnds();
	uint32 * roles = inst->getOpndRoles();
	if (opnd0!=NULL){ 
		opnds[i] = opnd0; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd1!=NULL){ 
		opnds[i] = opnd1; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd2!=NULL){ 
		opnds[i] = opnd2; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd3!=NULL){ 
		opnds[i] = opnd3; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd4!=NULL){ 
		opnds[i] = opnd4; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd5!=NULL){ 
		opnds[i] = opnd5; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd6!=NULL){ 
		opnds[i] = opnd6; roles[i] = Inst::OpndRole_Explicit; i++;
	if (opnd7!=NULL){ 
		opnds[i] = opnd7; roles[i] = Inst::OpndRole_Explicit; i++;
	}}}}}}}};	
	inst->opndCount = i;
	inst->defOpndCount=defCount;
	inst->assignOpcodeGroup(this);
	inst->verify();
	return inst;
}

//_________________________________________________________________________________________________
Inst * IRManager::newI8PseudoInst(Mnemonic mnemonic, uint32 defCount,
			Opnd * opnd0, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3
		)
{
	Inst * inst=new  (memoryManager, 4) Inst(mnemonic, instId++, Inst::Form_Extended);
	inst->kind = Inst::Kind_I8PseudoInst;
	uint32 i=0;
	Opnd ** opnds = inst->getOpnds();
	assert(opnd0->getType()->isInteger());
	if (opnd0!=NULL){ 		opnds[i] = opnd0; i++;
	if (opnd1!=NULL){ 		opnds[i] = opnd1; i++;
	if (opnd2!=NULL){ 		opnds[i] = opnd2; i++;
	if (opnd3!=NULL){ 		opnds[i] = opnd3; i++;		assert(opnd3->getSize()==OpndSize_64);
	}}}};	
	inst->defOpndCount=defCount;
	inst->opndCount = i;
	inst->assignOpcodeGroup(this);
	return inst;
}

//_____________________________________________________________________________________________
SystemExceptionCheckPseudoInst * IRManager::newSystemExceptionCheckPseudoInst(CompilationInterface::SystemExceptionId exceptionId, Opnd * opnd0, Opnd * opnd1, bool checksThisForInlinedMethod)
{
	SystemExceptionCheckPseudoInst * inst=new  (memoryManager, 8) SystemExceptionCheckPseudoInst(exceptionId, instId++, checksThisForInlinedMethod);
	uint32 i=0;
	Opnd ** opnds = inst->getOpnds();
	if (opnd0!=NULL){ opnds[i++] = opnd0; 
	if (opnd1!=NULL){ opnds[i++] = opnd1;
	}}		
	inst->opndCount = i;
	inst->assignOpcodeGroup(this);
	return inst;
}



//_________________________________________________________________________________________________
BranchInst * IRManager::newBranchInst(Mnemonic mnemonic, Opnd * targetOpnd)
{
	BranchInst * inst=new(memoryManager, 2) BranchInst(mnemonic, instId++);
	if (targetOpnd==0)
		targetOpnd=newImmOpnd(typeManager.getIntPtrType(), 0);
	inst->insertOpnd(0, targetOpnd, Inst::OpndRole_Explicit);
	inst->assignOpcodeGroup(this);
	inst->verify();
	return inst;
}

//_________________________________________________________________________________________________
EntryPointPseudoInst * IRManager::newEntryPointPseudoInst(const CallingConvention * cc)
{
	// there is nothing wrong with calling this method several times.
	// it's just a self-check, as the currently assumed behaviour is that this method is invoked only once.
	assert(NULL == entryPointInst);

	EntryPointPseudoInst * inst=new(memoryManager, methodDesc.getMethodSig()->getNumParams() * 2) EntryPointPseudoInst(this, instId++, cc);
	getPrologNode()->appendInsts( inst );

	inst->assignOpcodeGroup(this);
	entryPointInst = inst;
	return inst;
}

//_________________________________________________________________________________________________
CallInst * IRManager::newCallInst(Opnd * targetOpnd, const CallingConvention * cc, 
		uint32 argCount, Opnd ** args, Opnd * retOpnd, InlineInfo* ii)
{
	CallInst * callInst=new(memoryManager, (argCount + (retOpnd ? 1 : 0)) * 2 + 1) CallInst(this, instId++, cc, ii);
	CallingConventionClient & ccc = callInst->callingConventionClient;
	uint32 i=0;
	if (retOpnd!=NULL){
		ccc.pushInfo(Inst::OpndRole_Def, retOpnd->getType()->tag);
		callInst->insertOpnd(i++, retOpnd, Inst::OpndRole_Auxilary|Inst::OpndRole_Def);
	}
	callInst->defOpndCount = i;
	callInst->insertOpnd(i++, targetOpnd, Inst::OpndRole_Explicit|Inst::OpndRole_Use);

	if (argCount>0){
		for (uint32 j=0; j<argCount; j++){
			ccc.pushInfo(Inst::OpndRole_Use, args[j]->getType()->tag);
			callInst->insertOpnd(i++, args[j], Inst::OpndRole_Auxilary|Inst::OpndRole_Use);
		}
	}
	callInst->opndCount = i;
	callInst->assignOpcodeGroup(this);
	return callInst;
}

//_________________________________________________________________________________________________
CallInst * IRManager::newRuntimeHelperCallInst(CompilationInterface::RuntimeHelperId helperId, 
	uint32 numArgs, Opnd ** args, Opnd * retOpnd)
{
	Opnd * target=newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_HelperAddress, (void*)helperId);
	const CallingConvention * cc=getCallingConvention(helperId);
	return newCallInst(target, cc, numArgs, args, retOpnd);
}

//_________________________________________________________________________________________________
CallInst * IRManager::newInternalRuntimeHelperCallInst(const char * internalHelperID, uint32 numArgs, Opnd ** args, Opnd * retOpnd)
{
	const InternalHelperInfo * info=getInternalHelperInfo(internalHelperID);
	assert(info!=NULL);
	Opnd * target=newImmOpnd(typeManager.getIntPtrType(), Opnd::RuntimeInfo::Kind_InternalHelperAddress, (void*)newInternalString(internalHelperID));
	const CallingConvention * cc=info->callingConvention;
	return newCallInst(target, cc, numArgs, args, retOpnd);
}

//_________________________________________________________________________________________________
void IRManager::registerInternalHelperInfo(const char * internalHelperID, const InternalHelperInfo& info)
{
	assert(internalHelperID!=NULL && internalHelperID[0]!=0);
	internalHelperInfos[newInternalString(internalHelperID)]=info;
}

//_________________________________________________________________________________________________
RetInst * IRManager::newRetInst(Opnd * retOpnd)
{
	RetInst * retInst=new  (memoryManager, 4) RetInst(this, instId++);
	assert( NULL != entryPointInst && NULL != entryPointInst->getCallingConventionClient().getCallingConvention() );
	retInst->insertOpnd(0, newImmOpnd(typeManager.getInt16Type(), 0), Inst::OpndRole_Explicit|Inst::OpndRole_Use);
	if (retOpnd!=NULL){
		retInst->insertOpnd(1, retOpnd, Inst::OpndRole_Auxilary|Inst::OpndRole_Use);
		retInst->getCallingConventionClient().pushInfo(Inst::OpndRole_Use, retOpnd->getType()->tag);
		retInst->opndCount = 2;
	} else 	retInst->opndCount = 1;

	retInst->assignOpcodeGroup(this);
	return retInst;
}


//_________________________________________________________________________________________________
void IRManager::applyCallingConventions()
{
	for (NodeIterator it(*this); it!=NULL; ++it){
		if (((Node*)it)->hasKind(Node::Kind_BasicBlock)){
			BasicBlock * bb=(BasicBlock*)(Node*)it;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
				if (inst->hasKind(Inst::Kind_EntryPointPseudoInst)){
					EntryPointPseudoInst * eppi=(EntryPointPseudoInst*)inst;
					eppi->callingConventionClient.finalizeInfos(Inst::OpndRole_Def, CallingConvention::ArgKind_InArg);
					eppi->callingConventionClient.layoutAuxilaryOpnds(Inst::OpndRole_Def, OpndKind_Memory);

				}else if (inst->hasKind(Inst::Kind_CallInst)){
					CallInst * callInst=(CallInst*)inst;

					callInst->callingConventionClient.finalizeInfos(Inst::OpndRole_Use, CallingConvention::ArgKind_InArg);
					callInst->callingConventionClient.layoutAuxilaryOpnds(Inst::OpndRole_Use, OpndKind_Any);

					callInst->callingConventionClient.finalizeInfos(Inst::OpndRole_Def, CallingConvention::ArgKind_RetArg);
					callInst->callingConventionClient.layoutAuxilaryOpnds(Inst::OpndRole_Def, OpndKind_Null);
					
				}else if (inst->hasKind(Inst::Kind_RetInst)){
					RetInst * retInst=(RetInst*)inst;
					retInst->callingConventionClient.finalizeInfos(Inst::OpndRole_Use, CallingConvention::ArgKind_RetArg);
					retInst->callingConventionClient.layoutAuxilaryOpnds(Inst::OpndRole_Use, OpndKind_Null);

					if (retInst->getCallingConventionClient().getCallingConvention()->calleeRestoresStack()){
						uint32 stackDepth=getEntryPointInst()->getArgStackDepth();
						retInst->getOpnd(0)->assignImmValue(stackDepth);
					}
				}
			}
		}
	}
}

//_________________________________________________________________________________________________
CatchPseudoInst * IRManager::newCatchPseudoInst(Opnd * exception)
{
	CatchPseudoInst * inst=new  (memoryManager, 1) CatchPseudoInst(instId++);
	inst->insertOpnd(0, exception, Inst::OpndRole_Def);
	inst->setConstraint(0, RegName_EAX);
	inst->defOpndCount = 1;
	inst->opndCount = 1;
	inst->assignOpcodeGroup(this);
	return inst;
}

//_________________________________________________________________________________________________
GCInfoPseudoInst* IRManager::newGCInfoPseudoInst(const StlVector<Opnd*>& basesAndMptrs) {
#ifdef _DEBUG
    for (StlVector<Opnd*>::const_iterator it = basesAndMptrs.begin(), end = basesAndMptrs.end(); it!=end; ++it) {
        Opnd* opnd = *it;
        assert(opnd->getType()->isObject() || opnd->getType()->isManagedPtr());
    }
#endif
    GCInfoPseudoInst* inst = new(memoryManager, basesAndMptrs.size()) GCInfoPseudoInst(this, instId++);
	Opnd ** opnds = inst->getOpnds();
	Constraint * constraints = inst->getConstraints();
	for (uint32 i = 0, n = basesAndMptrs.size(); i < n; i++){
		Opnd * opnd = basesAndMptrs[i];
		opnds[i] = opnd;
		constraints[i] = Constraint(OpndKind_Any, opnd->getSize());
	}
	inst->opndCount = basesAndMptrs.size();
    inst->assignOpcodeGroup(this);
    return inst;
}

//_________________________________________________________________________________________________
Inst * IRManager::newCopyPseudoInst(Mnemonic mn, Opnd * opnd0, Opnd * opnd1)
{ 
	assert(mn==Mnemonic_MOV||mn==Mnemonic_PUSH||mn==Mnemonic_POP);
	Inst * inst=new  (memoryManager, 2) Inst(mn, instId++, Inst::Form_Extended);
	inst->kind = Inst::Kind_CopyPseudoInst;
	assert(opnd0!=NULL);
	assert(opnd1==NULL||opnd0->getSize()<=opnd1->getSize());

	Opnd ** opnds = inst->getOpnds();
	Constraint * opndConstraints = inst->getConstraints();

	opnds[0] = opnd0;
	opndConstraints[0] = Constraint(OpndKind_Any, opnd0->getSize());
	if (opnd1!=NULL){
		opnds[1] = opnd1;
		opndConstraints[1] = Constraint(OpndKind_Any, opnd0->getSize());
		inst->opndCount = 2;
	}else
		inst->opndCount = 1;
	if (mn != Mnemonic_PUSH)
		inst->defOpndCount = 1;
	inst->assignOpcodeGroup(this);
	return inst;
}

//_________________________________________________________________________________________________
AliasPseudoInst * IRManager::newAliasPseudoInst(Opnd * targetOpnd, Opnd * sourceOpnd, uint32 offset)
{
	assert(sourceOpnd->isPlacedIn(OpndKind_Memory));
	assert(!targetOpnd->hasAssignedPhysicalLocation());
	assert(targetOpnd->canBePlacedIn(OpndKind_Memory));

	Type * sourceType=sourceOpnd->getType();
	OpndSize sourceSize=getTypeSize(sourceType);

	Type * targetType=targetOpnd->getType();
	OpndSize targetSize=getTypeSize(targetType);

#ifdef _DEBUG
	uint32 sourceByteSize=getByteSize(sourceSize);
	uint32 targetByteSize=getByteSize(targetSize);
	assert(getByteSize(sourceSize)>0 && getByteSize(targetSize)>0);
	assert(offset+targetByteSize<=sourceByteSize);
#endif

	AliasPseudoInst * inst=new  (memoryManager, 2) AliasPseudoInst(instId++);

	inst->getOpnds()[0] = targetOpnd;
	inst->getConstraints()[0] = Constraint(OpndKind_Mem, targetSize);

	inst->getOpnds()[1] = sourceOpnd;
	inst->getConstraints()[1] = Constraint(OpndKind_Mem, sourceSize);

	inst->defOpndCount = 1;
	inst->opndCount = 2;

	inst->offset=offset;

	layoutAliasPseudoInstOpnds(inst);

	inst->assignOpcodeGroup(this);
	return inst;
}

//_________________________________________________________________________________________________
AliasPseudoInst * IRManager::newAliasPseudoInst(Opnd * targetOpnd, uint32 sourceOpndCount, Opnd ** sourceOpnds)
{
	assert(targetOpnd->isPlacedIn(OpndKind_Memory));

	Type * targetType=targetOpnd->getType();
	OpndSize targetSize=getTypeSize(targetType);
	assert(getByteSize(targetSize)>0);

	AliasPseudoInst * inst=new  (memoryManager, sourceOpndCount + 1) AliasPseudoInst(instId++);

	Opnd ** opnds = inst->getOpnds();
	Constraint * opndConstraints = inst->getConstraints();

	opnds[0] = targetOpnd;
	opndConstraints[0] = Constraint(OpndKind_Mem, targetSize);


	uint32 offset=0;
	for (uint32 i=0; i<sourceOpndCount; i++){
		assert(!sourceOpnds[i]->hasAssignedPhysicalLocation());
		assert(sourceOpnds[i]->canBePlacedIn(OpndKind_Memory));
		Type * sourceType=sourceOpnds[i]->getType();
		OpndSize sourceSize=getTypeSize(sourceType);
		uint32 sourceByteSize=getByteSize(sourceSize);
		assert(sourceByteSize>0);
		assert(offset+sourceByteSize<=getByteSize(targetSize));

		opnds[1 + i] = sourceOpnds[i];
		opndConstraints[1 + i] = Constraint(OpndKind_Mem, sourceSize);

		offset+=sourceByteSize;
	}
	inst->defOpndCount = 1;
	inst->opndCount = sourceOpndCount + 1;

	layoutAliasPseudoInstOpnds(inst);
	inst->assignOpcodeGroup(this);
	return inst;
}

//_________________________________________________________________________________________________
void IRManager::layoutAliasPseudoInstOpnds(AliasPseudoInst * inst)
{
	assert(inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def) == 1);
	Opnd * const * opnds = inst->getOpnds();
	Opnd * defOpnd=opnds[0];
	Opnd * const * useOpnds = opnds + 1; 
	uint32 useCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Use);
	assert(useCount > 0);
	if (inst->offset==EmptyUint32){
		uint32 offset=0;
		for (uint32 i=0; i<useCount; i++){
			Opnd * innerOpnd=useOpnds[i];
			assignInnerMemOpnd(defOpnd, innerOpnd, offset);
			offset+=getByteSize(innerOpnd->getSize());
		}
	}else
		assignInnerMemOpnd(useOpnds[0], defOpnd, inst->offset);
}

//_________________________________________________________________________________________________
void IRManager::addAliasRelation(AliasRelation * relations, Opnd * outerOpnd, Opnd * innerOpnd, uint32 offset)
{
	if (outerOpnd==innerOpnd){
		assert(offset==0);
		return;
	}

	const AliasRelation& outerRel=relations[outerOpnd->getId()];
	if (outerRel.outerOpnd!=NULL){
		addAliasRelation(relations, outerRel.outerOpnd, innerOpnd, outerRel.offset+offset);
		return;
	}

	AliasRelation& innerRel=relations[innerOpnd->getId()];
	if (innerRel.outerOpnd!=NULL){
		addAliasRelation(relations, outerOpnd, innerRel.outerOpnd, offset-(int)innerRel.offset);
	}

#ifdef _DEBUG
	Type * outerType=outerOpnd->getType();
	OpndSize outerSize=getTypeSize(outerType);
	uint32 outerByteSize=getByteSize(outerSize);
	assert(offset<outerByteSize);

	Type * innerType=innerOpnd->getType();
	OpndSize innerSize=getTypeSize(innerType);
	uint32 innerByteSize=getByteSize(innerSize);
	assert(outerByteSize>0 && innerByteSize>0);
	assert(offset+innerByteSize<=outerByteSize);
#endif

	innerRel.outerOpnd=outerOpnd;
	innerRel.offset=offset;

}

//_________________________________________________________________________________________________
void IRManager::getAliasRelations(AliasRelation * relations)
{
	for(CFG::NodeIterator it(*this); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)) {
			BasicBlock * bb = (BasicBlock*)node;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)) {
				if (inst->hasKind(Inst::Kind_AliasPseudoInst)){
					AliasPseudoInst * aliasInst=(AliasPseudoInst *)inst;
					Opnd * const * opnds = inst->getOpnds();
					uint32 useCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Use);
					assert(inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def) == 1 && useCount > 0);
					Opnd * defOpnd=opnds[0];
					Opnd * const * useOpnds = opnds + 1; 
					if (aliasInst->offset==EmptyUint32){
						uint32 offset=0;
						for (uint32 i=0; i<useCount; i++){
							Opnd * innerOpnd=useOpnds[i];
							addAliasRelation(relations, defOpnd, innerOpnd, offset);
							offset+=getByteSize(innerOpnd->getSize());
						}
					}else{
						addAliasRelation(relations, useOpnds[0], defOpnd, aliasInst->offset);
					}
				}
			}
		}
	}
}

//_________________________________________________________________________________________________
void IRManager::layoutAliasOpnds() 
{
	MemoryManager mm(0x1000, "layoutAliasOpnds");
	uint32 opndCount=getOpndCount();
	AliasRelation * relations=new  (memoryManager) AliasRelation[opndCount];
	getAliasRelations(relations);
	for (uint32 i=0; i<opndCount; i++){
		if (relations[i].outerOpnd!=NULL){
			Opnd * innerOpnd=getOpnd(i);
			assert(innerOpnd->isPlacedIn(OpndKind_Mem));
			assert(relations[i].outerOpnd->isPlacedIn(OpndKind_Mem));
			Opnd * innerDispOpnd=innerOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
			if (innerDispOpnd==NULL){
				innerDispOpnd=newImmOpnd(typeManager.getInt32Type(), 0);
				innerOpnd->setMemOpndSubOpnd(MemOpndSubOpndKind_Displacement, innerDispOpnd);
			}
			Opnd * outerDispOpnd=relations[i].outerOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
			uint32 outerDispValue=(uint32)(outerDispOpnd!=NULL?outerDispOpnd->getImmValue():0);
			innerDispOpnd->assignImmValue(outerDispValue+relations[i].offset);
		}
	}
}

//_________________________________________________________________________________________________
uint32 IRManager::assignInnerMemOpnd(Opnd * outerOpnd, Opnd* innerOpnd, uint32 offset)
{
	assert(outerOpnd->isPlacedIn(OpndKind_Memory));

	Opnd * outerDisp=outerOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
	MemOpndKind outerMemOpndKind=outerOpnd->getMemOpndKind();
	Opnd::RuntimeInfo * outerDispRI=outerDisp!=NULL?outerDisp->getRuntimeInfo():NULL;
	uint64 outerDispValue=outerDisp!=NULL?outerDisp->getImmValue():0;

	Opnd * outerBase=outerOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Base);
	Opnd * outerIndex=outerOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Index);
	Opnd * outerScale=outerOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Scale);

	OpndSize innerSize = innerOpnd->getSize();
	uint32 innerByteSize = getByteSize(innerSize);
		
	Opnd * innerDisp=newImmOpnd(outerDisp!=NULL?outerDisp->getType():typeManager.getInt32Type(), outerDispValue+offset);
	if (outerDispRI){
		Opnd::RuntimeInfo * innerDispRI=new(memoryManager) 
			Opnd::RuntimeInfo(
				outerDispRI->getKind(),
				outerDispRI->getValue(0),
				outerDispRI->getValue(1),
				outerDispRI->getValue(2),
				outerDispRI->getValue(3),
				offset
			);
		innerDisp->setRuntimeInfo(innerDispRI);
	}
	innerOpnd->assignMemLocation(outerMemOpndKind, outerBase, outerIndex, outerScale, innerDisp);
	return innerByteSize;
}

//_________________________________________________________________________________________________
void IRManager::assignInnerMemOpnds(Opnd * outerOpnd, Opnd** innerOpnds, uint32 innerOpndCount)
{
#ifdef _DEBUG
	uint32 outerByteSize = getByteSize(outerOpnd->getSize());
#endif
	for (uint32 i=0, offset=0; i<innerOpndCount; i++){
		offset+=assignInnerMemOpnd(outerOpnd, innerOpnds[i], offset);
		assert(offset<=outerByteSize);
	}
}

//_________________________________________________________________________________________________
uint32 getLayoutOpndAlignment(Opnd * opnd)
{
	OpndSize size=opnd->getSize();
	if (size==OpndSize_80 || size==OpndSize_128)
		return 16;
	if (size==OpndSize_64)
		return 8;
	else
		return 4;
}


//_________________________________________________________________________________________________

Inst * IRManager::newCopySequence(Mnemonic mn, Opnd * opnd0, Opnd * opnd1, uint32 regUsageMask)
{
	if (mn==Mnemonic_MOV)
		return newCopySequence(opnd0, opnd1, regUsageMask);
	else if (mn==Mnemonic_PUSH||mn==Mnemonic_POP)
		return newPushPopSequence(mn, opnd0, regUsageMask);
	assert(0);
	return NULL;
}



//_________________________________________________________________________________________________

Inst * IRManager::newMemMovSequence(Opnd * targetOpnd, Opnd * sourceOpnd, uint32 regUsageMask, bool checkSource)
{
	Inst * instList=NULL;
	RegName tmpRegName=RegName_Null, unusedTmpRegName=RegName_Null;
	bool registerSetNotLocked = !isRegisterSetLocked(OpndKind_GPReg);

	for (uint32 reg = RegName_EAX; reg<=(uint32)(targetOpnd->getSize()<OpndSize_32?RegName_EBX:RegName_EDI); reg++) {
		RegName regName = (RegName) reg;
		if (regName == RegName_ESP)
			continue;
		Opnd * subOpnd = targetOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Base);
		if(subOpnd && subOpnd->isPlacedIn(regName))
			continue;
		subOpnd = targetOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Index);
		if(subOpnd && subOpnd->isPlacedIn(regName))
			continue;

		if (checkSource){
			subOpnd = sourceOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Base);
			if(subOpnd && subOpnd->isPlacedIn(regName))
				continue;
			subOpnd = sourceOpnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Index);
			if(subOpnd && subOpnd->isPlacedIn(regName))
				continue;
		}

		tmpRegName=regName;
		if (registerSetNotLocked || (getRegMask(tmpRegName)&regUsageMask)==0){
			unusedTmpRegName=tmpRegName;
			break;
		}
	}

    assert(tmpRegName!=RegName_Null);
	Opnd * tmp=getRegOpnd(tmpRegName);
	Opnd * tmpAdjusted=newRegOpnd(targetOpnd->getType(), tmpRegName);
	Opnd * tmpRegStackOpnd = newMemOpnd(tmp->getType(), MemOpndKind_StackAutoLayout, getRegOpnd(RegName_ESP), 0); 


	if (unusedTmpRegName==RegName_Null)
		appendToInstList(instList, newInst(Mnemonic_MOV, tmpRegStackOpnd, tmp));

	appendToInstList(instList, newInst(Mnemonic_MOV, tmpAdjusted, sourceOpnd)); // must satisfy constraints
	appendToInstList(instList, newInst(Mnemonic_MOV, targetOpnd, tmpAdjusted)); // must satisfy constraints

	if (unusedTmpRegName==RegName_Null)
		appendToInstList(instList, newInst(Mnemonic_MOV, tmp, tmpRegStackOpnd));

	return instList;
}



//_________________________________________________________________________________________________

Inst * IRManager::newCopySequence(Opnd * targetBOpnd, Opnd * sourceBOpnd, uint32 regUsageMask)

{ 
	Opnd * targetOpnd=(Opnd*)targetBOpnd, * sourceOpnd=(Opnd*)sourceBOpnd;

	Constraint targetConstraint = targetOpnd->getConstraint(Opnd::ConstraintKind_Location);
	Constraint sourceConstraint = sourceOpnd->getConstraint(Opnd::ConstraintKind_Location);
	
	if (targetConstraint.isNull() || sourceConstraint.isNull()){
		return newCopyPseudoInst(Mnemonic_MOV, targetOpnd, sourceOpnd);
	}

	OpndSize targetSize=targetConstraint.getSize();
	OpndSize sourceSize=sourceConstraint.getSize();
	uint32 sourceByteSize=getByteSize(sourceSize);
	uint32 targetByteSize=getByteSize(targetSize);
	OpndKind targetKind=(OpndKind)targetConstraint.getKind();
	OpndKind sourceKind=(OpndKind)sourceConstraint.getKind();

	assert(targetSize<=sourceSize); // only same size or truncating conversions are allowed

	if ( (targetKind==OpndKind_GPReg||targetKind==OpndKind_Mem) && 
		 (sourceKind==OpndKind_GPReg||sourceKind==OpndKind_Mem||sourceKind==OpndKind_Imm)
	){
		if (sourceKind==OpndKind_Mem && targetKind==OpndKind_Mem){
			Inst * instList=NULL;
			if (sourceByteSize<=4){
				instList=newMemMovSequence(targetOpnd, sourceOpnd, regUsageMask);
			}else{
				Opnd * targetOpnds[IRMaxOperandByteSize/4]; // limitation because we are currently don't support large memory operands
				uint32 targetOpndCount = 0;
				for (uint32 cb=0; cb<sourceByteSize && cb<targetByteSize; cb+=4)
					targetOpnds[targetOpndCount++] = newOpnd(typeManager.getInt32Type());
				AliasPseudoInst * targetAliasInst=newAliasPseudoInst(targetOpnd, targetOpndCount, targetOpnds);
				layoutAliasPseudoInstOpnds(targetAliasInst);
				for (uint32 cb=0, targetOpndSlotIndex=0; cb<sourceByteSize && cb<targetByteSize; cb+=4, targetOpndSlotIndex++){  
					Opnd * sourceOpndSlot=newOpnd(typeManager.getInt32Type());
					appendToInstList(instList, newAliasPseudoInst(sourceOpndSlot, sourceOpnd, cb));
   					Opnd * targetOpndSlot=targetOpnds[targetOpndSlotIndex];
					appendToInstList(instList, newMemMovSequence(targetOpndSlot, sourceOpndSlot, regUsageMask, true));
				}
				appendToInstList(instList, targetAliasInst);
			}
			assert(instList!=NULL);
			return instList;
		}else{
			assert(sourceByteSize<=4);
			return newInst(Mnemonic_MOV, targetOpnd, sourceOpnd); // must satisfy constraints
		}
	}else if ( 
		(targetKind==OpndKind_XMMReg||targetKind==OpndKind_Mem) && 
		(sourceKind==OpndKind_XMMReg||sourceKind==OpndKind_Mem)
	){
		if (sourceByteSize==4){
			return newInst(Mnemonic_MOVSS,targetOpnd, sourceOpnd);
		}else if (sourceByteSize==8){
			return newInst(Mnemonic_MOVSD,targetOpnd, sourceOpnd);
		}
	}else if (targetKind==OpndKind_FPReg && sourceKind==OpndKind_Mem){
		return newInst(Mnemonic_FLD, targetOpnd, sourceOpnd);
	}else if (targetKind==OpndKind_Mem && sourceKind==OpndKind_FPReg){
		return newInst(Mnemonic_FSTP, targetOpnd, sourceOpnd);
	}else if (
		(targetKind==OpndKind_FPReg && sourceKind==OpndKind_XMMReg)||
		(targetKind==OpndKind_XMMReg && sourceKind==OpndKind_FPReg)
	){
		Inst * instList=NULL;
		Opnd * tmp = newMemOpnd(targetOpnd->getType(), MemOpndKind_StackAutoLayout, getRegOpnd(RegName_ESP), 0); 
		appendToInstList(instList, newCopySequence(tmp, sourceOpnd, regUsageMask));
		appendToInstList(instList, newCopySequence(targetOpnd, tmp, regUsageMask));
		return instList;
	}
	assert(0);
	return NULL;
}



//_________________________________________________________________________________________________

Inst * IRManager::newPushPopSequence(Mnemonic mn, Opnd * opnd, uint32 regUsageMask)
{
	assert(opnd!=NULL);

	Constraint constraint = opnd->getConstraint(Opnd::ConstraintKind_Location);
	if (constraint.isNull())
		return newCopyPseudoInst(mn, opnd);

	OpndKind kind=(OpndKind)constraint.getKind();
	OpndSize size=constraint.getSize();

	Inst * instList=NULL;

	if ( kind==OpndKind_GPReg||kind==OpndKind_Mem||kind==OpndKind_Imm ){
		if (size==OpndSize_32){
			return newInst(mn, opnd);
		}else if (size<OpndSize_32){ 
		}else if (size==OpndSize_64){
			if (mn==Mnemonic_PUSH){
				Opnd * opndLo=newOpnd(typeManager.getUInt32Type());
				appendToInstList(instList, newAliasPseudoInst(opndLo, opnd, 0)); 
				Opnd * opndHi=newOpnd(typeManager.getIntPtrType());
				appendToInstList(instList, newAliasPseudoInst(opndHi, opnd, 4)); 
				appendToInstList(instList, newInst(Mnemonic_PUSH, opndHi));
				appendToInstList(instList, newInst(Mnemonic_PUSH, opndLo));
			}else{
				Opnd * opnds[2]={ newOpnd(typeManager.getUInt32Type()), newOpnd(typeManager.getInt32Type()) };
				appendToInstList(instList, newInst(Mnemonic_POP, opnds[0])); 
				appendToInstList(instList, newInst(Mnemonic_POP, opnds[1]));
				appendToInstList(instList, newAliasPseudoInst(opnd, 2, opnds));
			}
			return instList;
		}
	}
	Opnd * espOpnd=getRegOpnd(RegName_ESP);
	Opnd * tmp=newMemOpnd(opnd->getType(), MemOpndKind_StackManualLayout, espOpnd, 0); 
	uint32 cb=getByteSize(size);
	uint32 slotSize=4; 
	cb=(cb+slotSize-1)&~(slotSize-1);
	Opnd * sizeOpnd=newImmOpnd(typeManager.getInt32Type(), cb);
	if (mn==Mnemonic_PUSH){
		appendToInstList(instList, newInst(Mnemonic_SUB, espOpnd, sizeOpnd));
		appendToInstList(instList, newCopySequence(tmp, opnd, regUsageMask));
	}else{
		appendToInstList(instList, newCopySequence(opnd, tmp, regUsageMask));
		appendToInstList(instList, newInst(Mnemonic_ADD, espOpnd, sizeOpnd));
	}

	return instList;
}

//_________________________________________________________________________________________________
const CallingConvention * IRManager::getCallingConvention(CompilationInterface::RuntimeHelperId helperId)const
{
	CompilationInterface::VmCallingConvention callConv=compilationInterface.getRuntimeHelperCallingConvention(helperId);
	switch (callConv){
		case CompilationInterface::CallingConvention_Drl:
			return &CallingConvention_DRL;
		case CompilationInterface::CallingConvention_Stdcall:
			return &CallingConvention_STDCALL;
		case CompilationInterface::CallingConvention_Cdecl:
			return &CallingConvention_CDECL;
		default:
			assert(0);
			return NULL;
	}
}

//_________________________________________________________________________________________________
const CallingConvention * IRManager::getCallingConvention(MethodDesc * methodDesc)const
{
	return &CallingConvention_DRL;
}

//_________________________________________________________________________________________________
Opnd * IRManager::defArg(Type * type, uint32 position)
{
	assert(NULL != entryPointInst);
	Opnd * opnd=newOpnd(type);
	entryPointInst->insertOpnd(position, opnd, Inst::OpndRole_Auxilary|Inst::OpndRole_Def);
	entryPointInst->callingConventionClient.pushInfo(Inst::OpndRole_Def, type->tag);
	return opnd;
}

//_________________________________________________________________________________________________
Opnd * IRManager::getRegOpnd(RegName regName)
{
	assert(getRegSize(regName)==Constraint::getDefaultSize(getRegKind(regName))); // are we going to change this?
	uint32 idx=( (getRegKind(regName) & 0x1f) << 4 ) | ( getRegIndex(regName)&0xf );
	if (!regOpnds[idx]){
		regOpnds[idx]=newRegOpnd(typeManager.getUInt32Type(), regName);
	}
	return regOpnds[idx];
}
void IRManager::calculateTotalRegUsage(OpndKind regKind) {
	assert(regKind == OpndKind_GPReg);
	uint32 opndCount=getOpndCount();
	for (uint32 i=0; i<opndCount; i++){
		Opnd * opnd=getOpnd(i);
		if (opnd->isPlacedIn(regKind))
			gpTotalRegUsage |= getRegMask(opnd->getRegName());
	}
}
//_________________________________________________________________________________________________
uint32 IRManager::getTotalRegUsage(OpndKind regKind)const {
	return gpTotalRegUsage;
}
//_________________________________________________________________________________________________
bool IRManager::isPreallocatedRegOpnd(Opnd * opnd)
{
	RegName regName=opnd->getRegName();
	if (regName==RegName_Null || getRegSize(regName)!=Constraint::getDefaultSize(getRegKind(regName)))
		return false;
	uint32 idx=( (getRegKind(regName) & 0x1f) << 4 ) | ( getRegIndex(regName)&0xf );
	return regOpnds[idx]==opnd;
}

//_______________________________________________________________________________________________________________
Type * IRManager::getManagedPtrType(Type * sourceType)
{
	return typeManager.getManagedPtrType(sourceType); 
}

//_________________________________________________________________________________________________
Type * IRManager::getTypeFromTag(Type::Tag tag)const
{
    switch (tag) {
		case Type::Void:    
		case Type::Tau:		
		case Type::Int8:    
		case Type::Int16:   
		case Type::Int32:   
		case Type::IntPtr:  
		case Type::Int64:   
		case Type::UInt8:   
		case Type::UInt16:  
		case Type::UInt32:  
		case Type::UInt64:  
		case Type::Single:  
		case Type::Double:  
		case Type::Boolean:  
		case Type::Float:   return typeManager.getPrimitiveType(tag);
		default:            return new(memoryManager) Type(tag);
	}
}

//_____________________________________________________________________________________________
OpndSize IRManager::getTypeSize(Type::Tag tag)
{
	OpndSize size;
    switch (tag) {
        case Type::Int8:
        case Type::UInt8:
        case Type::Boolean:
			size = OpndSize_8;
            break;
        case Type::Int16:   
        case Type::UInt16:
        case Type::Char:
			size = OpndSize_16;
            break;
        case Type::Int64:   
        case Type::UInt64:
			size = OpndSize_64;
            break;
        case Type::Double:
			size = OpndSize_64;
			break;
        case Type::Float:
			size = OpndSize_80;
			break;
        default:
			size = OpndSize_32;
            break;
    }
    return size;

}

//_____________________________________________________________________________________________
void IRManager::indexInsts(OrderType order)
{
	Direction dir=order==CFG::OrderType_Postorder?Direction_Backward:Direction_Forward;

	uint32 idx=0;
	for (NodeIterator it(*this, order); it!=NULL; ++it){
		Node * node=it.getNode();
		if (node->hasKind(Node::Kind_BasicBlock)){
			const Insts& insts=((BasicBlock*)node)->getInsts();
			for (Inst * inst=insts.getFirst(dir); inst!=NULL; inst=insts.getNext(dir, inst))
				inst->index=idx++;
		}
	}
}

//_____________________________________________________________________________________________
uint32 IRManager::calculateOpndStatistics(bool reindex)
{
	POpnd * arr=&opnds.front();
	for (uint32 i=0, n=opnds.size(); i<n; i++){
		Opnd * opnd=arr[i];
		if (opnd==NULL) continue;
		opnd->defScope=Opnd::DefScope_Temporary;
		opnd->definingInst=NULL;
		opnd->refCount=0;
		if (reindex)
			opnd->id=EmptyUint32;
	}

	uint32 index=0;
	uint32 instIdx=0;
	for (NodeIterator it(*this, CFG::OrderType_Topological); it!=NULL; ++it){
		Node * node=it;
		if (!node->hasKind(Node::Kind_BasicBlock)) continue;
		int32 execCount=(int32)node->getExecCnt()*100;
		if (execCount<=1)
			execCount=1;
		const Insts& insts=((BasicBlock*)node)->getInsts();
		for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
			inst->index=instIdx++;
			for (uint32 i=0, n=inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_UseDef); i<n; i++){
				Opnd * opnd=inst->getOpnd(i);
				opnd->addRefCount(index, execCount);
				if ((inst->getOpndRoles(i)&Inst::OpndRole_Def)!=0){
					opnd->setDefiningInst(inst);
				}
			}
		}
	}

	for (uint32 i=0; i<IRMaxRegNames; i++){ // update predefined regOpnds to prevent losing them from the ID space
		if (regOpnds[i]!=NULL)
			regOpnds[i]->addRefCount(index, 1);
	}
	return index;
}

//_____________________________________________________________________________________________
void IRManager::packOpnds()
{
	static CountTime packOpndsTimer("timer:ia32::packOpnds");
	AutoTimer tm(packOpndsTimer);

    _hasLivenessInfo=false;

	uint32 maxIndex=calculateOpndStatistics(true);

	uint32 opndsBefore=opnds.size();
	opnds.resize(opnds.size()+maxIndex);
	POpnd * arr=&opnds.front();
	for (uint32 i=0; i<opndsBefore; i++){
		Opnd * opnd=arr[i];
		if (opnd->id!=EmptyUint32)
			arr[opndsBefore+opnd->id]=opnd;
	}
	opnds.erase(opnds.begin(), opnds.begin()+opndsBefore);

	_hasLivenessInfo=false;
}

//_____________________________________________________________________________________________
void IRManager::fixLivenessInfo( uint32 * map )
{
	uint32 opndCount = getOpndCount();
	for (NodeIterator it(*this); it!=NULL; ++it){
		Node * node=it;
		LiveSet * ls = node->getLiveAtEntry();
		ls->resize(opndCount);
	}	
}

//_____________________________________________________________________________________________
void IRManager::calculateLivenessInfo()
{
	static CountTime livenessTimer("timer:ia32::liveness");
	AutoTimer tm(livenessTimer);

    _hasLivenessInfo=false;
    updateLoopInfo();
    const uint32 opndCount = getOpndCount();	

    const Nodes& nodes = getNodesPostorder();
    //clean all prev. liveness info
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        node->getLiveAtEntry()->resizeClear(opndCount);
    }
    uint32 loopDepth = getMaxLoopDepth();
    uint32 nIterations = loopDepth + 1;
#ifdef _DEBUG
    nIterations++; //one more extra iteration to prove that nothing changed
#endif 
    LiveSet tmpLs(memoryManager, opndCount);
    bool changed = true;
    for (uint32 iteration=0; iteration < nIterations; iteration++) {
        changed = false;
        for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
            Node* node = *it;
            if (node==exitNode) {
				if (!methodDesc.isStatic() 
                    && (methodDesc.isSynchronized() || methodDesc.isMethodClassIsLikelyExceptionType())) 
                {
					LiveSet * exitLs  = node->getLiveAtEntry();
					EntryPointPseudoInst * entryPointInst = getEntryPointInst();
					Opnd * thisOpnd = entryPointInst->getOpnd(0);
					exitLs->setBit(thisOpnd->getId(), true);
				} 
				continue;
            }
            bool processNode = true;
            if (iteration > 0) {
                uint32 depth = node->getLoopDepth();
                processNode = iteration <= depth;
#ifdef _DEBUG
                processNode = processNode || iteration == nIterations-1; //last iteration will check all blocks
#endif
            }
            if (processNode) {
                getLiveAtExit(node, tmpLs);
                if (node->hasKind(Node::Kind_BasicBlock)){
                    const Insts& insts=((BasicBlock*)node)->getInsts();
                    for (Inst * inst=insts.getLast(); inst!=NULL; inst=insts.getPrev(inst)){
                        updateLiveness(inst, tmpLs);
                    }
                }
                LiveSet * ls = node->getLiveAtEntry();
                if (iteration == 0 || !ls->isEqual(tmpLs)) {
                    changed = true;
                    ls->copyFrom(tmpLs);
                }
            }
        }
        if (!changed) {
            break;
        }
    }
#ifdef _DEBUG
    assert(!changed);
#endif
    _hasLivenessInfo=true;
}

//_____________________________________________________________________________________________
bool IRManager::ensureLivenessInfoIsValid() {
	return true;
}

//_____________________________________________________________________________________________
void IRManager::getLiveAtExit(const Node * node, LiveSet & ls) const
{
	assert(ls.getSetSize()<=getOpndCount());
	const Edges& edges=node->getEdges(Direction_Out);
	uint32 i=0;
	for (Edge * edge=edges.getFirst(); edge!=NULL; edge=edges.getNext(edge), i++){
		Node * succ=edge->getNode(Direction_Head);
		const LiveSet * succLs=succ->getLiveAtEntry();
		if (i==0) {
		    ls.copyFrom(*succLs);
        } else {
		    ls.unionWith(*succLs);
        }
	}
}

//_____________________________________________________________________________________________
void IRManager::updateLiveness(const Inst * inst, LiveSet & ls) const 
{
	const Opnd * const * opnds = inst->getOpnds();
	const uint32 * roles = inst->getOpndRoles();
	uint32 opndCount = inst->getOpndCount();
	uint32 properties = inst->getProperties();

	for (uint32 i = 0; i < opndCount; i++){
		uint32 r = roles [i];
		const Opnd * opnd = opnds[i];
		uint32 id = opnd->getId();
		if ( (r & Inst::OpndRole_UseDef) == Inst::OpndRole_Def && (properties & Inst::Properties_Conditional) == 0 )
			ls.setBit(id, false);
		else if ( opnd->isSubjectForLivenessAnalysis() )
			ls.setBit(id, true);
	}
	for (uint32 i = 0; i < opndCount; i++){
		const Opnd * opnd = opnds[i];
		if (opnd->getMemOpndKind() != MemOpndKind_Null){
			const Opnd * const * subOpnds = opnd->getMemOpndSubOpnds();
			for (uint32 j = 0; j < MemOpndSubOpndKind_Count; j++){
				const Opnd * subOpnd = subOpnds[j];
				if (subOpnd != NULL && subOpnd->isSubjectForLivenessAnalysis())
					ls.setBit(subOpnd->getId(), true);
			}
		}
	}

}

//_____________________________________________________________________________________________
uint32 IRManager::getRegUsageFromLiveSet(LiveSet * ls, OpndKind regKind)const
{
	assert(ls->getSetSize()<=getOpndCount());
	uint32 mask=0;
	LiveSet::IterB ib(*ls);
	for (int i = ib.getNext(); i != -1; i = ib.getNext()){
		Opnd * opnd=getOpnd(i);
		if (opnd->isPlacedIn(regKind))
			mask |= getRegMask(opnd->getRegName());
	}
	return mask;
}


//_____________________________________________________________________________________________
void IRManager::getRegUsageAtExit(const Node * node, OpndKind regKind, uint32 & mask)const
{
	const Edges& edges=node->getEdges(Direction_Out);
	mask=0;
	for (Edge * edge=edges.getFirst(); edge!=NULL; edge=edges.getNext(edge))
		mask |= getRegUsageAtEntry(edge->getNode(Direction_Head), regKind);
}

//_____________________________________________________________________________________________
void IRManager::updateRegUsage(const Inst * inst, OpndKind regKind, uint32 & mask)const
{
	Inst::Opnds opnds(inst, Inst::OpndRole_All);
	for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
		Opnd * opnd=inst->getOpnd(it);
		if (opnd->isPlacedIn(regKind)){
			uint32 m=getRegMask(opnd->getRegName());
			if (inst->isLiveRangeEnd(it))
				mask &= ~m;
	        else if (inst->isLiveRangeStart(it))
				mask |= m;
		}
	}
}

//_____________________________________________________________________________________________
void IRManager::resetOpndConstraints()
{
	for (uint32 i=0, n=getOpndCount(); i<n; i++){
		Opnd * opnd=getOpnd(i);
		opnd->setCalculatedConstraint(opnd->getConstraint(Opnd::ConstraintKind_Initial));
	}
}

//_____________________________________________________________________________________________
void IRManager::addToOpndCalculatedConstraints(Constraint c)
{
	for (uint32 i=0, n=getOpndCount(); i<n; i++){
		Opnd * opnd=getOpnd(i);
		opnd->setCalculatedConstraint(opnd->getConstraint(Opnd::ConstraintKind_Calculated)|c);
	}
}

void IRManager::finalizeCallSites()
{
	for(CFG::NodeIterator it(*this); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)) {
			BasicBlock * bb = (BasicBlock*)node;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getLast(), * prevInst=NULL; inst!=NULL; inst=prevInst) {
				prevInst=insts.getPrev(inst);
				if (inst->getMnemonic() == Mnemonic_CALL) {
					const CallInst * callInst=(const CallInst*)inst;
					const StlVector<CallingConventionClient::StackOpndInfo>& stackOpndInfos = 
						callInst->getCallingConventionClient().getStackOpndInfos(Inst::OpndRole_Use);
					Inst * instToPrepend=inst;
					Opnd * const * opnds = callInst->getOpnds();
					for (uint32 i=0, n=stackOpndInfos.size(); i<n; i++) {
						Inst * pushInst=newCopyPseudoInst(Mnemonic_PUSH, opnds[stackOpndInfos[i].opndIndex]);
						bb->prependInsts(pushInst, instToPrepend);
						instToPrepend=pushInst;
					}
					if(!((CallInst *)inst)->getCallingConventionClient().getCallingConvention()->calleeRestoresStack()) {
						bb->appendInsts(newInst(Mnemonic_ADD, getRegOpnd(RegName_ESP), newImmOpnd(typeManager.getInt32Type(), ((CallInst *)inst)->getArgStackDepth())), inst);
					}
				}
			}
		}
	}
}

//_____________________________________________________________________________________________
void IRManager::removeRedundantStackOperations()
{
	for(CFG::NodeIterator it(*this); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)) {
			BasicBlock * bb = (BasicBlock*)node;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getFirst(); inst!=NULL; ){
				Inst * nextInst = insts.getNext(inst);
				if(nextInst && ((inst->getMnemonic() == Mnemonic_PUSH && nextInst->getMnemonic() == Mnemonic_POP && inst->getOpnd(0) == nextInst->getOpnd(0))||
					(inst->getMnemonic() == Mnemonic_POP && nextInst->getMnemonic() == Mnemonic_PUSH && inst->getOpnd(0) == nextInst->getOpnd(0)))) {
					Inst * tmpInst = insts.getPrev(inst);
					bb->removeInst(inst);
					bb->removeInst(nextInst);
					nextInst = tmpInst;
				}
				inst = nextInst;
			}
		}
	}
}

//_____________________________________________________________________________________________
void IRManager::calculateStackDepth()
{
	MemoryManager mm(0x100, "calculateStackDepth");
	StlVector<int32> stackDepths(mm, getMaxNodeId());
	for (uint32 i=0; i<stackDepths.size(); i++)
		stackDepths[i]=-1;

	for(CFG::NodeIterator it(*this, OrderType_Topological); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)||node->hasKind(Node::Kind_DispatchNode)) {

			int32 stackDepth=-1;
			const Edges& edges=node->getEdges(Direction_In);
			for (Edge * edge=edges.getFirst(); edge!=NULL; edge=edges.getNext(edge)){
				Node * pred=edge->getNode(Direction_Tail);
				int32 predStackDepth=stackDepths[pred->getId()];
				if (predStackDepth>=0){
					assert(stackDepth==-1 || stackDepth==predStackDepth);
					stackDepth=predStackDepth;
				}
			}
			
			if (stackDepth<0)	
				stackDepth=0;
			if (node->hasKind(Node::Kind_BasicBlock)){
				BasicBlock * bb = (BasicBlock*)node;
				const Insts& insts=bb->getInsts();
				for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
					inst->setStackDepth(stackDepth);

					Inst::Opnds opnds(inst, Inst::OpndRole_Explicit | Inst::OpndRole_Auxilary | Inst::OpndRole_UseDef);
					Inst::Opnds::iterator it = opnds.begin();
					if (it != opnds.end() && inst->getOpnd(it)->isPlacedIn(RegName_ESP)) {
						if (inst->getMnemonic()==Mnemonic_ADD)
							stackDepth -= (uint32)inst->getOpnd(opnds.next(it))->getImmValue();
						else if (inst->getMnemonic()==Mnemonic_SUB)
							stackDepth += (uint32)inst->getOpnd(opnds.next(it))->getImmValue();
						else
							assert(0);
					}else{
						if(inst->getMnemonic()==Mnemonic_PUSH) {
							stackDepth+=getByteSize(inst->getOpnd(it)->getSize());
						} else if (inst->getMnemonic() == Mnemonic_POP) {
							stackDepth-=getByteSize(inst->getOpnd(it)->getSize());
						} else if (inst->getMnemonic() == Mnemonic_CALL && ((CallInst *)inst)->getCallingConventionClient().getCallingConvention()->calleeRestoresStack()) {
							stackDepth -= ((CallInst *)inst)->getArgStackDepth();
						}
					}
				}
				assert(stackDepth>=0);
			}
			stackDepths[node->getId()]=stackDepth;
		}
	}
}

//_____________________________________________________________________________________________
bool IRManager::isOnlyPrologSuccessor(BasicBlock * bb) {
	BasicBlock * predBB = bb;
	for(; ; ) {
		if(predBB == getPrologNode())
			return true;
		if (predBB != bb) {
			const Edges& outedges=predBB->getEdges(Direction_Out);
			uint32 i=0;
			for (Edge * edge=outedges.getFirst(); edge!=NULL; edge=outedges.getNext(edge), i++){
			}
			if (i>1) {
				return false;
			}
		} 
		const Edges& inedges=predBB->getEdges(Direction_In);
		uint32 i=0;
		for (Edge * edge=inedges.getFirst(); edge!=NULL; edge=inedges.getNext(edge), i++){
			predBB = (BasicBlock *)edge->getNode(Direction_Tail);
		}
		if (i>1) {
			return false;
		}
	}
}
//_____________________________________________________________________________________________
void IRManager::expandSystemExceptions(uint32 reservedForFlags)
{  // this is a temporary prototype implementation to be improved in the nearest future and moved into a separate transformer
	calculateOpndStatistics();
	StlMap<Opnd *, int32> checkOpnds(getMemoryManager());
	StlVector<Inst *> excInsts(getMemoryManager());
	for (CFG::NodeIterator it(*this); it!=NULL; ++it){
		Node * node=it;
		if (node->hasKind(Node::Kind_BasicBlock)){
			BasicBlock * bb=(BasicBlock*)node;

			const Insts& insts=bb->getInsts();
			Inst * inst=insts.getLast();
			if (inst && inst->hasKind(Inst::Kind_SystemExceptionCheckPseudoInst)) {
				Node * dispatchNode = bb->getNode(Direction_Out, (Node::Kind)(Node::Kind_DispatchNode));
				excInsts.push_back(inst);
				if (!dispatchNode)
					checkOpnds[inst->getOpnd(0)] = 
						((SystemExceptionCheckPseudoInst*)inst)->checksThisOfInlinedMethod() ?
						-1:
						(int32)inst;
				
			}

			if(checkOpnds.size() == 0)
				continue;

			for (inst=insts.getFirst(); inst!=NULL; inst = insts.getNext(inst)){
				if (inst->getMnemonic() == Mnemonic_CALL && !inst->hasKind(Inst::Kind_SystemExceptionCheckPseudoInst) && ((CallInst *)inst)->isDirect()) {
					Opnd::RuntimeInfo * rt = inst->getOpnd(((ControlTransferInst*)inst)->getTargetOpndIndex())->getRuntimeInfo();
					if(rt->getKind() == Opnd::RuntimeInfo::Kind_MethodDirectAddr &&  !((MethodDesc *)rt->getValue(0))->isStatic()) {
						Inst::Opnds opnds(inst, Inst::OpndRole_Auxilary | Inst::OpndRole_Use);
						for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
							Opnd * opnd = inst->getOpnd(it);
							if(checkOpnds.find(opnd) != checkOpnds.end()) 
								checkOpnds[opnd] = -1;
						}
					}
				} 
			}
		}
	}
	for(StlVector<Inst *>::iterator it = excInsts.begin(); it != excInsts.end(); it++) {
		Inst * lastInst = *it;
		BasicBlock * bb = lastInst->getBasicBlock();
		bb->removeInst(lastInst);
		switch (((SystemExceptionCheckPseudoInst*)lastInst)->getExceptionId()){
			case CompilationInterface::Exception_NullPointer:
				{
					Node * dispatchNode = bb->getNode(Direction_Out, (Node::Kind)(Node::Kind_DispatchNode));
					Opnd * opnd = lastInst->getOpnd(0);
					if (dispatchNode!=NULL || (checkOpnds[opnd] == -1)){
						BasicBlock * throwBasicBlock = newBasicBlock(0);
						throwBasicBlock->appendInsts(newRuntimeHelperCallInst(
								CompilationInterface::Helper_NullPtrException, 0, NULL, NULL));
						bb->appendInsts(newInst(Mnemonic_CMP, opnd, newImmOpnd(opnd->getType(), 0)));
						bb->appendInsts(newBranchInst(Mnemonic_JZ));
                        newDirectBranchEdge(bb, throwBasicBlock, EdgeProbValue_Exception);
						newEdge(throwBasicBlock, dispatchNode ? dispatchNode : bb->getNode(Direction_Out, (Node::Kind)(Node::Kind_UnwindNode)), 1 - EdgeProbValue_Exception);
						if((checkOpnds[opnd] == -1) && (opnd->getDefScope() == Opnd::DefScope_Temporary) && isOnlyPrologSuccessor(bb)) {
							checkOpnds[opnd] = (int32)lastInst;
						}
					}
					break;
				}
			default:
				assert(0);
		}
	}
}
//_____________________________________________________________________________________________
void IRManager::translateToNativeForm()
{
    const Nodes& nodes = getNodes();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)){
			const Insts & insts=((BasicBlock*)node)->getInsts();
			for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
                if (inst->getForm()==Inst::Form_Extended) {
					inst->makeNative(this);
                }
			}
		}
	}
}

//_____________________________________________________________________________________________
void IRManager::eliminateSameOpndMoves()
{
	const Nodes& nodes = getNodes();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)){
			const Insts & insts=((BasicBlock*)node)->getInsts();
			for (Inst * inst=insts.getFirst(), * nextInst=NULL; inst!=NULL; inst=nextInst){
				nextInst=insts.getNext(inst);
                if (inst->getMnemonic()==Mnemonic_MOV && inst->getOpnd(0)==inst->getOpnd(1)) {
					((BasicBlock*)node)->removeInst(inst);
                }
			}
		}
	}
}

//_____________________________________________________________________________________________
void IRManager::resolveRuntimeInfo()
{
	for (uint32 i=0, n=getOpndCount(); i<n; i++){
		Opnd * opnd=getOpnd(i);
        resolveRuntimeInfo(opnd);
	}
}


void IRManager::resolveRuntimeInfo(Opnd* opnd) const {
	if (!opnd->isPlacedIn(OpndKind_Imm))
		return;
    Opnd::RuntimeInfo * info=opnd->getRuntimeInfo();
    if (info==NULL)
		return;
    int32 value=0;
    switch(info->getKind()){
        case Opnd::RuntimeInfo::Kind_HelperAddress: 
            /** The value of the operand is compilationInterface->getRuntimeHelperAddress */
            value=(int32)compilationInterface.getRuntimeHelperAddress(
                (CompilationInterface::RuntimeHelperId)(uint32)info->getValue(0)
                );
            assert(value!=0);
            break;
        case Opnd::RuntimeInfo::Kind_InternalHelperAddress:
            /** The value of the operand is irManager.getInternalHelperInfo((const char*)[0]).pfn */
            value=(int32)getInternalHelperInfo((const char*)info->getValue(0))->pfn;
            assert(value!=0);
            break;
        case Opnd::RuntimeInfo::Kind_TypeRuntimeId: 
            /*	The value of the operand is [0]->ObjectType::getRuntimeIdentifier() */
            value=(int32)((NamedType*)info->getValue(0))->getRuntimeIdentifier();
            break;
		case Opnd::RuntimeInfo::Kind_MethodRuntimeId: 
			value=(int32)compilationInterface.getRuntimeMethodHandle((MethodDesc*)info->getValue(0));
			break;
        case Opnd::RuntimeInfo::Kind_AllocationHandle: 
            /* The value of the operand is [0]->ObjectType::getAllocationHandle() */
            value=(int32)((ObjectType*)info->getValue(0))->getAllocationHandle();
            break;
        case Opnd::RuntimeInfo::Kind_StringDescription: 
            /* [0] - Type * - the containing class, [1] - string token */
            assert(0);
            break;
        case Opnd::RuntimeInfo::Kind_Size: 
            /* The value of the operand is [0]->ObjectType::getObjectSize() */
            value=(int32)((ObjectType*)info->getValue(0))->getObjectSize();
            break;
        case Opnd::RuntimeInfo::Kind_StaticFieldAddress:
            /** The value of the operand is [0]->FieldDesc::getAddress() */
            value=(int32)((FieldDesc*)info->getValue(0))->getAddress();
            break;
        case Opnd::RuntimeInfo::Kind_FieldOffset:
            /** The value of the operand is [0]->FieldDesc::getOffset() */
            value=(int32)((FieldDesc*)info->getValue(0))->getOffset();
            break;
        case Opnd::RuntimeInfo::Kind_VTableAddrOffset:
            /** The value of the operand is compilationInterface.getVTableOffset(), zero args */
            value=(int32)compilationInterface.getVTableOffset();
            break;
        case Opnd::RuntimeInfo::Kind_VTableConstantAddr:
            /** The value of the operand is [0]->ObjectType::getVTable() */
            value=(int32)((ObjectType*)info->getValue(0))->getVTable();
            break;
        case Opnd::RuntimeInfo::Kind_MethodVtableSlotOffset:
            /** The value of the operand is [0]->MethodDesc::getOffset() */
            value=(int32)((MethodDesc*)info->getValue(0))->getOffset();
            break;
        case Opnd::RuntimeInfo::Kind_MethodIndirectAddr:
            /** The value of the operand is [0]->MethodDesc::getIndirectAddress() */
            value=(int32)((MethodDesc*)info->getValue(0))->getIndirectAddress();
            break;
        case Opnd::RuntimeInfo::Kind_MethodDirectAddr:
            /** The value of the operand is *[0]->MethodDesc::getIndirectAddress() */
            value=*(int32*)((MethodDesc*)info->getValue(0))->getIndirectAddress();
            break;
        case Opnd::RuntimeInfo::Kind_ConstantAreaItem:
            /** The value of the operand is address of constant pool item  ((ConstantPoolItem*)[0])->getAddress() */
            value=(uint32)((ConstantAreaItem*)info->getValue(0))->getAddress();
            break;
        default:
            assert(0);
    }
	opnd->assignImmValue(value+info->getAdditionalOffset());
}

//_____________________________________________________________________________________________
bool IRManager::verify()
{
	updateLivenessInfo();
	if (!verifyLiveness())
		return false;
	if (!verifyHeapAddressTypes())
		return false;
	return true;
}

//_____________________________________________________________________________________________
bool IRManager::verifyLiveness()
{
	BasicBlock * prolog=getPrologNode();
	bool failed=false;
	LiveSet * ls=getLiveAtEntry(prolog);
	assert(ls!=NULL);
	Constraint calleeSaveRegs=Constraint(OpndKind_GPReg, Constraint::getDefaultSize(OpndKind_GPReg), getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg));

	LiveSet::IterB lives(*ls);
	for (int i = lives.getNext(); i != -1; i = lives.getNext()){
		Opnd * opnd=getOpnd(i);
		assert(opnd!=NULL);
		if (
			opnd->isSubjectForLivenessAnalysis() && 
			(opnd->isPlacedIn(RegName_EFLAGS)||!isPreallocatedRegOpnd(opnd))
		){
			VERIFY_OUT("Operand live at entry: " << opnd << ::std::endl);
			VERIFY_OUT("This means there is a use of the operand when it is not yet defined" << ::std::endl);
			failed=true;
		};
	}
	if (failed)
		VERIFY_OUT(::std::endl << "Liveness verification failure" << ::std::endl);
	return !failed;
}

//_____________________________________________________________________________________________
bool IRManager::verifyHeapAddressTypes()
{
	bool failed=false;
	for (uint32 i=0, n=getOpndCount(); i<n; i++){
		Opnd * opnd = getOpnd(i);
		if (opnd->isPlacedIn(OpndKind_Mem) && opnd->getMemOpndKind()==MemOpndKind_Heap){
			Opnd * properTypeSubOpnd=NULL;
			for (uint32 j=0; j<MemOpndSubOpndKind_Count; j++){
				Opnd * subOpnd=opnd->getMemOpndSubOpnd((MemOpndSubOpndKind)j);
				if (subOpnd!=NULL){
					Type * type=subOpnd->getType();
					if (type->isManagedPtr()||type->isObject()||type->isMethodPtr()||type->isVTablePtr()||type->isUnmanagedPtr()){
						if (properTypeSubOpnd!=NULL){
							VERIFY_OUT("Heap operand " << opnd << " contains more than 1 sub-operands of type Object or ManagedPointer "<<::std::endl);
							VERIFY_OUT("Opnd 1: " << properTypeSubOpnd << ::std::endl);
							VERIFY_OUT("Opnd 2: " << subOpnd << ::std::endl);
							failed=true;
							break;
						}
                        properTypeSubOpnd=subOpnd;
					}
				}
			}
			if (failed)
				break;
			if (properTypeSubOpnd==NULL){
				VERIFY_OUT("Heap operand " << opnd << " contains no sub-operands of type Object or ManagedPointer "<<::std::endl);
				failed=true;
				break;
			}
		}
	}
	if (failed)
		VERIFY_OUT(::std::endl << "Heap address type verification failure" << ::std::endl);
	return !failed;
}

//=============================================================================================
// class IRTransformer implementation
//=============================================================================================
uint32 IRTransformer::registerIRTransformer(const char * tag, CreateFunction pfn)
{
	assert(tableLength<MaxIRTransformers);
	table[tableLength++]=IRTransformerRecord(tag, pfn);
	return tableLength;
}

IRTransformer::IRTransformerRecord * IRTransformer::findIRTransformer(const char * tag)
{
	for (uint32 i=0; i<tableLength; i++){
		if (stricmp(tag, table[i].tag)==0)
			return table+i;
		}
	return NULL;
}

IRTransformer *	IRTransformer::newIRTransformer(const char * tag, IRManager& irm, const char * params)
{
	IRTransformerRecord * trr=findIRTransformer(tag);
	if (trr){
		IRTransformer * tr=trr->createFunction(irm, params);
        tr->record=trr;
		return tr;
	}
	return NULL;
}

void IRTransformer::run() 
{
	stageId=Log::getNextStageId();

	if (Log::cat_cg()->isIREnabled())
		Log::printStageBegin(stageId, "IA32", getName(), getTagName());

	uint32 needInfo=getNeedInfo();
	if (needInfo & NeedInfo_LivenessInfo)
		irManager.updateLivenessInfo();
	if (needInfo & NeedInfo_LoopInfo)
		irManager.updateLoopInfo();

	if (record){
		record->xtimer.start();
		runImpl();
		record->xtimer.stop();
	}else
		runImpl();

	uint32 sideEffects=getSideEffects();
	if (sideEffects & SideEffect_InvalidatesLivenessInfo)
		irManager.invalidateLivenessInfo();
	if (sideEffects & SideEffect_InvalidatesLoopInfo)
		irManager.invalidateLoopInfo();

	debugOutput("after");

	if (!verify()){
		VERIFY_OUT(::std::endl<<"Verification failure after "<<getName()<<::std::endl);
		assert(FALSE);
		Jitrino::crash(0);
	}

	if (Log::cat_cg()->isIREnabled())
		Log::printStageEnd(stageId, "IA32", getName(), getTagName());
}

//_________________________________________________________________________________________________
uint32 IRTransformer::getSideEffects()const
{
	return ~(uint32)0;
}

//_________________________________________________________________________________________________
uint32 IRTransformer::getNeedInfo()const
{
	return ~(uint32)0;
}

//_________________________________________________________________________________________________
bool IRTransformer::verify(bool force)
{	
	if (force||irManager.getVerificationLevel()>=2)
		return irManager.verify();
	return true;
}	

//_________________________________________________________________________________________________
void IRTransformer::printIRDumpBegin(const char * subKind)
{
	if (Log::cat_cg()->isIREnabled())
		Log::printIRDumpBegin(stageId, getName(), subKind);
}

//_________________________________________________________________________________________________
void IRTransformer::printIRDumpEnd(const char * subKind)
{
	if (Log::cat_cg()->isIREnabled())
		Log::printIRDumpEnd(stageId, getName(), subKind);
}

//_________________________________________________________________________________________________
void IRTransformer::dumpIR(const char * subKind1, const char * subKind2)
{
	Ia32::dumpIR(&irManager, stageId, "IA32 LIR CFG after ", getName(), getTagName(), 
		subKind1, subKind2);
}

//_________________________________________________________________________________________________
void IRTransformer::printDot(const char * subKind1, const char * subKind2)
{
	Ia32::printDot(&irManager, stageId, "IA32 LIR CFG after ", getName(), getTagName(), 
		subKind1, subKind2);
}

//_________________________________________________________________________________________________
void IRTransformer::debugOutput(const char * subKind)
{
	if (isIRDumpEnabled() && Log::cat_cg()->isIREnabled()){
		irManager.updateLoopInfo();

		if (Log::cat_cg()->isIR2Enabled()){
			irManager.updateLivenessInfo();
			dumpIR(subKind, "opnds");
			dumpIR(subKind, "liveness");
		}
		dumpIR(subKind);

		if (irManager.getCompilationContext()->getIa32CGFlags()->dumpdot) {
			printDot(subKind);
			if (Log::cat_cg()->isIR2Enabled()){
				irManager.updateLivenessInfo();
				printDot(subKind, "liveness");
			}
		}
	}
}


struct IRTimers : public CounterBase
{
	IRTimers ()		:CounterBase("IRTimers") {}

	void write  (CountWriter&);
};


void IRTimers::write (CountWriter& cw)
{
	char key[128] = "timer:ia32::";
	size_t pref = strlen(key);
	for (size_t i = 0; i < IRTransformer::tableLength; i++)
	{
		IRTransformer::IRTransformerRecord& r = IRTransformer::table[i];
		strncpy(key + pref, r.tag, sizeof(key) - pref - 1);
		cw.write(key, r.xtimer.getSeconds());
	}
}


static IRTimers irtimers;

}}; //namespace Ia32

