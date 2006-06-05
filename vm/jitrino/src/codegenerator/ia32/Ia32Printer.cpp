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
 * @version $Revision: 1.11.14.2.4.3 $
 */

#include "Ia32Printer.h"
#include "Log.h"
#include "../../vm/drl/DrlVMInterface.h"
#include "../../shared/PlatformDependant.h"
#include "CGSupport.h"

namespace Jitrino
{
namespace Ia32{


//========================================================================================
// class Printer
//========================================================================================

//_____________________________________________________________________________________________
Printer& Printer::setStream(::std::ostream& _os)
{
	assert(os==NULL);
	os=&_os;
	return *this;
}

//_____________________________________________________________________________________________
Printer& Printer::setDotFileName(const char * fileName)
{
	assert(os==NULL);
	fs.open(fileName, ::std::ios::out|::std::ios::trunc);
	os=&fs;
	return *this;
}

//_____________________________________________________________________________________________
Printer& Printer::createDotFileName(const char * dir, const char * fileSuffix)
{
	assert(os==NULL);
	assert(irManager!=NULL);
	if (dir==NULL) dir="";
	char fileNameBuffer[1024];
	if (dir==NULL||dir[0]==0)
		dir=".";
    sprintf(fileNameBuffer, "%s\\%s.dot", dir, fileSuffix);
	char * fileName=fileNameBuffer+strlen(dir);
    while (fileName != NULL) {
        switch(*fileName) {
        case '/': case '*':  *fileName++ = '_'; break;
        case '<': case '>':  *fileName++ = '_'; break;
        case '(': case ')':  *fileName++ = '_'; break;
        case '{': case '}':  *fileName++ = '_'; break;
        case ';':            *fileName++ = '_'; break;        
        case 0:     fileName=NULL; break;
        default:    fileName++;
        }
    }
	return setDotFileName(fileNameBuffer);
}
	
//_____________________________________________________________________________________________
void Printer::print(uint32 indent)
{
	printHeader(indent);
	printBody(indent);
	printEnd(indent);
}

//_____________________________________________________________________________________________
void Printer::printHeader(uint32 indent)
{
	assert(irManager!=NULL);
	::std::ostream& os = getStream();
	os << ::std::endl; printIndent(indent);
	os << "====================================================================" << ::std::endl; printIndent(indent);
	os << irManager->getMethodDesc().getParentType()->getName()<<"."<<irManager->getMethodDesc().getName()<<" " << (title?title:"") << ::std::endl; printIndent(indent);
	os << "====================================================================" << ::std::endl; printIndent(indent);
	os << ::std::endl; 
}

//_____________________________________________________________________________________________
void Printer::printEnd(uint32 indent)
{
	::std::ostream& os = getStream();
	os << ::std::endl;
	os.flush();
}

//_____________________________________________________________________________________________
void Printer::printBody(uint32 indent)
{
	::std::ostream& os = getStream();
	os << "Printer::printBody is stub implementation"<< ::std::endl;
	os.flush();
}

//========================================================================================
// class IRPrinter
//========================================================================================
//_____________________________________________________________________________________________
void IRPrinter::print(uint32 indent)
{
	Printer::print();
}

//_____________________________________________________________________________________________
void IRPrinter::printBody(uint32 indent)
{
	printCFG(indent);
}

//_____________________________________________________________________________________________
void IRPrinter::printCFG(uint32 indent)
{
	assert(irManager!=NULL);
	::std::ostream& os = getStream();
	for (CFG::NodeIterator it(*irManager, CFG::OrderType_Topological); it!=NULL; ++it){
		printNode(it.getNode(), indent);
		os<<::std::endl;
	}
}

//_____________________________________________________________________________________________
void IRPrinter::printNodeName(const Node * node)
{
	::std::ostream& os = getStream();
	if (!node){
		os<<"NULL";
		return;
	}
	Node::Kind k=node->getKind();
	if (k&Node::Kind_BasicBlock)
        os << "BB_";
    else if (k&Node::Kind_DispatchNode)
        os << "DN_";
    else if (k&Node::Kind_UnwindNode)
        os << "UN_";
    else if (k&Node::Kind_ExitNode)
        os << "EN_";
    else
        os << "??_";
    os << node->getId();
    if (node == node->getCFG()->getPrologNode())
        os << "_prolog";
    else if (node->getCFG()->isEpilog(node))
        os << "_epilog";
}

//_____________________________________________________________________________________________
void IRPrinter::printNodeHeader(const Node * node, uint32 indent)
{
	::std::ostream& os = getStream();
	printIndent(indent);
	printNodeName(node);
	os<<::std::endl; printIndent(indent);

	if (node->getPersistentId()!=UnknownId){
		os << "  PersistentId = " << node->getPersistentId() << ::std::endl;
		printIndent(indent);
	}
    if (node->getExecCnt() >= 0.0) 
        os << "  ExecCnt = " << node->getExecCnt() << ::std::endl;
    else 
        os << "  ExecCnt = Unknown" << ::std::endl;
	printIndent(indent);

	if (node->hasLoopInfo()) {
        os << "  Loop: Depth=" << node->getLoopDepth();
        if (node->isLoopHeader()) 
            os << ", hdr, hdr= ";
        else
            os << ", !hdr, hdr=";
        printNodeName(node->getLoopHeader());
        os << ::std::endl; printIndent(indent);
    }
	{
    os << "  Predcessors: ";
	const Edges& es=node->getEdges(Direction_In);
    for (const Edge * e=es.getFirst(); e!=NULL; e=es.getNext(e)) {
        printNodeName(e->getNode(Direction_Tail));
        os << " ";
    }
    os << ::std::endl; printIndent(indent);
	}
	{
	const Edges& es=node->getEdges(Direction_Out);
    os << "  Successors:  ";
    for (const Edge * e=es.getFirst(); e!=NULL; e=es.getNext(e)) {
        const Node * succ = e->getNode(Direction_Head);
        printNodeName(succ);
        os << " [Prob=" << (double)e->getProbability() << "]";
		if (e->hasKind(Edge::Kind_CatchEdge)) {
            os << "(" << ((CatchEdge *)e)->getPriority() << ",";
            printType(((CatchEdge *)e)->getType());
            os << ")";
        }
        if (e->isBackEdge()) 
			os << "(Backedge)";
        if (e->isLoopExit())
			os << "(LoopExit)";
        const BranchInst * br = e->getBranch();
        if (br) {
            os << "(Br=I" << (int) br->getId() << ")";
        }
        os << " ";
    }
	}
	if (node->hasKind(Node::Kind_BasicBlock)) {
		const BasicBlock * bb=(const BasicBlock*)node;
		if (node->getCFG()->isLaidOut()){
			os << ::std::endl; printIndent(indent);
			os << "Layout Succ: ";
			printNodeName(bb->getLayoutSucc());
			if (irManager->getCodeStartAddr()!=NULL){
				os << ::std::endl; printIndent(indent);
				os << "Block code address: " << (void*)bb->getCodeStartAddr();
			}
		}
	}
}

//_____________________________________________________________________________________________
void IRPrinter::printNodeInstList(const BasicBlock * bb, uint32 indent)
{
	::std::ostream& os = getStream();
	for (Inst * inst = bb->getInsts().getFirst(); inst != NULL; inst = bb->getInsts().getNext(inst)) {
		Inst::Kind kind=inst->getKind();
		if ((kind & instFilter)==(uint32)kind){
			printIndent(indent+1); 
			if (irManager->getCodeStartAddr()!=NULL){
				os<<(void*)inst->getCodeStartAddr()<<' ';
			}
			printInst(inst); 
			os << ::std::endl;
		}
	}
}

//_____________________________________________________________________________________________
void IRPrinter::printNode(const Node * node, uint32 indent)
{
	::std::ostream& os = getStream();
	printNodeHeader(node, indent);
	if (node->hasKind(Node::Kind_BasicBlock)) {
		os << ::std::endl;
		printNodeInstList((BasicBlock*)node, indent);
    }
    os << ::std::endl;
}

//_____________________________________________________________________________________________
void IRPrinter::printEdge(const Edge * edge)
{
}

//_____________________________________________________________________________________________
const char * IRPrinter::getPseudoInstPrintName(Inst::Kind k)
{
	switch(k){
		case Inst::Kind_PseudoInst: return "PseudoInst";
		case Inst::Kind_EntryPointPseudoInst: return "EntryPointPseudoInst";
		case Inst::Kind_AliasPseudoInst: return "AliasPseudoInst";
		case Inst::Kind_CatchPseudoInst: return "CatchPseudoInst";
		case Inst::Kind_CopyPseudoInst: return "CopyPseudoInst";
		case Inst::Kind_I8PseudoInst: return "I8PseudoInst";
        case Inst::Kind_GCInfoPseudoInst: return "GCInfoPseudoInst";
		default: return "";
	}
}

//_____________________________________________________________________________________________
uint32 IRPrinter::printInstOpnds(const Inst * inst, uint32 orf)
{
	::std::ostream& os = getStream();
	if (!(orf&Inst::OpndRole_ForIterator))
		return 0;
	uint32 printedOpnds=0;
	bool explicitOnly=(orf&Inst::OpndRole_ForIterator)==Inst::OpndRole_Explicit;

	Inst::Opnds opnds(inst, orf);
	for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it), printedOpnds++){
		if (printedOpnds)
			os<<",";
		else if (!explicitOnly){
			os<<"("; printOpndRoles(orf); os<<":";
		}
		printOpnd(inst, it, false, false);
	}
	if (printedOpnds && !explicitOnly)
		os<<")";
	return printedOpnds;
}

//_____________________________________________________________________________________________
void IRPrinter::printInst(const Inst * inst)
{
	::std::ostream& os = getStream();
	os<<"I"<<inst->getId()<<": ";

	if (opndRolesFilter & Inst::OpndRole_Def){
		uint32 printedOpndsTotal=0, printedOpnds=0;
		if (inst->getForm()==Inst::Form_Extended)
			printedOpnds=printInstOpnds(inst, (Inst::OpndRole_Def|Inst::OpndRole_Explicit)&opndRolesFilter);
		if (printedOpnds){ os<<" "; printedOpndsTotal+=printedOpnds; }
		printedOpnds=printInstOpnds(inst, (Inst::OpndRole_Def|Inst::OpndRole_Auxilary)&opndRolesFilter);
		if (printedOpnds){ os<<" "; printedOpndsTotal+=printedOpnds; }
		printedOpnds=printInstOpnds(inst, (Inst::OpndRole_Def|Inst::OpndRole_Implicit)&opndRolesFilter);
		if (printedOpnds){ os<<" "; printedOpndsTotal+=printedOpnds; }
		
		if (printedOpndsTotal)
			os<<"=";
	}

	if (inst->hasKind(Inst::Kind_PseudoInst)){
		os<<getPseudoInstPrintName(inst->getKind());
	}else{
		if( inst->getMnemonic() != Mnemonic_Null )
			os<< Encoder::getMnemonicString( inst->getMnemonic() );
		if (inst->hasKind(Inst::Kind_BranchInst) && inst->getBasicBlock()!=NULL){
			BasicBlock * target=((BranchInst*)inst)->getDirectBranchTarget();
			if (target){
				os<<" "; printNodeName(target);
			}
		}
	}

	os<<" ";
	uint32 printedOpndsTotal=0, printedOpnds=0;
	printedOpnds=printInstOpnds(inst, ((inst->getForm()==Inst::Form_Extended?Inst::OpndRole_Use:Inst::OpndRole_UseDef)|Inst::OpndRole_Explicit)&opndRolesFilter);
	if (printedOpnds){ os<<" "; printedOpndsTotal+=printedOpnds; }
	printedOpnds=printInstOpnds(inst, (Inst::OpndRole_Use|Inst::OpndRole_Auxilary)&opndRolesFilter);
	if (printedOpnds){ os<<" "; printedOpndsTotal+=printedOpnds; }
	printedOpnds=printInstOpnds(inst, (Inst::OpndRole_Use|Inst::OpndRole_Implicit)&opndRolesFilter);
	if (printedOpnds){ os<<" "; printedOpndsTotal+=printedOpnds; }

    if (inst->hasKind(Inst::Kind_GCInfoPseudoInst)) {
        const GCInfoPseudoInst* gcInst = (GCInfoPseudoInst*)inst;
        Opnd * const * uses = gcInst->getOpnds();
        const StlVector<int32>& offsets = gcInst->offsets;
        os<<"[phase:"<<gcInst->desc<<"]";
        os<<"(";
        assert(!offsets.empty());
        for (uint32 i = 0, n = offsets.size(); i<n; i++) {
                int32 offset = offsets[i];
                Opnd* opnd = uses[i];
                if (i>0) {
                	os<<",";
                }
                os << "["; printOpndName(opnd); os <<"," <<offset <<"]";
        }
        os<<") ";
    }
}


//_________________________________________________________________________________________________
void IRPrinter::printOpndRoles(uint32 roles)
{
	::std::ostream& os=getStream();
	if (roles&Inst::OpndRole_Explicit)			os<<"E"; 
	if (roles&Inst::OpndRole_Auxilary)			os<<"A"; 
	if (roles&Inst::OpndRole_Implicit)			os<<"I"; 
	if (roles&Inst::OpndRole_MemOpndSubOpnd)	os<<"M"; 
	OpcodeDescriptionPrinter::printOpndRoles(roles);
}

//_____________________________________________________________________________________________
void IRPrinter::printOpndName(const Opnd * opnd)
{
	::std::ostream& os = getStream();
	Opnd::DefScope ds=opnd->getDefScope();
	os<<(
		ds==Opnd::DefScope_Variable?"v":
		ds==Opnd::DefScope_SemiTemporary?"s":
		ds==Opnd::DefScope_Temporary?"t":
		"o"
	)<<opnd->getFirstId();
}

//_________________________________________________________________________________________________
uint32 IRPrinter::getOpndNameLength(Opnd * opnd)
{
	uint32 id=opnd->getFirstId();
	uint32 idLength=id<10?1:id<100?2:id<1000?3:id<10000?4:5;
	return 1+idLength;
}

//_____________________________________________________________________________________________
void IRPrinter::printOpnd(const Inst * inst, uint32 idx, bool isLiveBefore, bool isLiveAfter)
{
	printOpnd(inst->getOpnd(idx), inst->getOpndRoles(idx), isLiveBefore, isLiveAfter);
}

//_____________________________________________________________________________________________
void IRPrinter::printRuntimeInfo(const Opnd::RuntimeInfo * info)
{
	::std::ostream& os = getStream();
	switch(info->getKind()){
		case Opnd::RuntimeInfo::Kind_HelperAddress: 
			/** The value of the operand is compilationInterface->getRuntimeHelperAddress */
			{
			os<<"h:"<<
				irManager->getCompilationInterface().getRuntimeHelperName(
					(CompilationInterface::RuntimeHelperId)(uint32)info->getValue(0)
				);
			}break;
		case Opnd::RuntimeInfo::Kind_InternalHelperAddress:
			/** The value of the operand is irManager.getInternalHelperInfo((const char*)[0]).pfn */
			{
			os<<"ih:"<<(const char*)info->getValue(0)<<":"<<(void*)irManager->getInternalHelperInfo((const char*)info->getValue(0))->pfn;
			}break;
		case Opnd::RuntimeInfo::Kind_TypeRuntimeId: 
			/*	The value of the operand is [0]->ObjectType::getRuntimeIdentifier() */
			{
			os<<"id:"; printType((NamedType*)info->getValue(0));
			}break;
		case Opnd::RuntimeInfo::Kind_AllocationHandle: 
			/* The value of the operand is [0]->ObjectType::getAllocationHandle() */
			{
			os<<"ah:"; printType((ObjectType*)info->getValue(0));
			}break;
		case Opnd::RuntimeInfo::Kind_StringDescription: 
			/* [0] - Type * - the containing class, [1] - string token */
			assert(0);
			break;
		case Opnd::RuntimeInfo::Kind_Size: 
			/* The value of the operand is [0]->ObjectType::getObjectSize() */
			{
			os<<"sz:"; printType((ObjectType*)info->getValue(0));
			}break;
		case Opnd::RuntimeInfo::Kind_StaticFieldAddress:
			/** The value of the operand is [0]->FieldDesc::getAddress() */
			{
			FieldDesc * fd=(FieldDesc*)info->getValue(0);
			os<<"&f:"; 
			os<<fd->getParentType()->getName()<<"."<<fd->getName();
			}break;
		case Opnd::RuntimeInfo::Kind_FieldOffset:
			/** The value of the operand is [0]->FieldDesc::getOffset() */
			{
			FieldDesc * fd=(FieldDesc*)info->getValue(0);
			os<<"fo:"; 
			os<<fd->getParentType()->getName()<<"."<<fd->getName();
			}break;
		case Opnd::RuntimeInfo::Kind_VTableAddrOffset:
			/** The value of the operand is compilationInterface.getVTableOffset(), zero args */
			{
			os<<"vtao"; 
			}break;
		case Opnd::RuntimeInfo::Kind_VTableConstantAddr:
			/** The value of the operand is [0]->ObjectType::getVTable() */
			{
			os<<"vtca:"; printType((ObjectType*)info->getValue(0));
			}break;
		case Opnd::RuntimeInfo::Kind_MethodVtableSlotOffset:
			/** The value of the operand is [0]->MethodDesc::getOffset() */
			{
			MethodDesc * md=(MethodDesc*)info->getValue(0);
			os<<"vtso:"; 
			os<<md->getParentType()->getName()<<"."<<md->getName();
			}break;
		case Opnd::RuntimeInfo::Kind_MethodIndirectAddr:
			/** The value of the operand is [0]->MethodDesc::getIndirectAddress() */
			{
			MethodDesc * md=(MethodDesc*)info->getValue(0);
			os<<"&m:"; 
			os<<md->getParentType()->getName()<<"."<<md->getName();
			}break;
		case Opnd::RuntimeInfo::Kind_MethodDirectAddr:
			/** The value of the operand is *[0]->MethodDesc::getIndirectAddress() */
			{
			MethodDesc * md=(MethodDesc*)info->getValue(0);
			os<<"m:"; 
			os<<md->getParentType()->getName()<<"."<<md->getName();
			}break;
		case Opnd::RuntimeInfo::Kind_ConstantAreaItem:
			break;
		case Opnd::RuntimeInfo::Kind_MethodRuntimeId: 
			{
			MethodDesc * md=(MethodDesc*)info->getValue(0);
			os<<"vtso:"; 
			os<<md->getParentType()->getName()<<"."<<md->getName();
			}break;
		default:
			assert(0);
	}
	uint32 additionalOffset=info->getAdditionalOffset();
	if (additionalOffset>0)
		os<<"+"<<additionalOffset;
}

//_____________________________________________________________________________________________
void IRPrinter::printOpnd(const Opnd * opnd, uint32 roles, bool isLiveBefore, bool isLiveAfter)
{
	::std::ostream& os = getStream();

	if (isLiveBefore) os<<".";
	printOpndName(opnd);
	if (isLiveAfter) os<<".";

	if (opndFlavor&OpndFlavor_Location){
		if (opnd->isPlacedIn(OpndKind_Reg)){
			os<<"("; printRegName(opnd->getRegName()); os<<")";
		}else if(opnd->isPlacedIn(OpndKind_Mem)){
			os<<"[";
			uint32 oldOpndFlavor=opndFlavor;
			opndFlavor&=~OpndFlavor_Type;
			bool append=false;
			for (uint32 i=0; i<MemOpndSubOpndKind_Count; i++){
				Opnd * subOpnd=opnd->getMemOpndSubOpnd((MemOpndSubOpndKind)i);
				if (subOpnd){
					if (append){
						if ((MemOpndSubOpndKind)i==MemOpndSubOpndKind_Scale)
							os<<"*";
						else
							os<<"+";
					}
					printOpnd(subOpnd);
					append=true;
				}
			}
			opndFlavor=oldOpndFlavor;
			os<<"]";
		}else if(opnd->isPlacedIn(OpndKind_Imm)){
			os<<"("<<opnd->getImmValue();
			if (opndFlavor & OpndFlavor_RuntimeInfo){
				Opnd::RuntimeInfo * ri=opnd->getRuntimeInfo();
				if (ri!=NULL){
					os<<":";
					printRuntimeInfo(ri);
				}
			}
			os<<")";
		}
	}
	if (opndFlavor&OpndFlavor_Type){
		os<<":"; printType(opnd->getType());
	}

}

//_____________________________________________________________________________________________
void IRPrinter::printType(const Type * type)
{
	::std::ostream& os = getStream();
	((Type*)type)->print(os);
}

//========================================================================================
// class IRLivenessPrinter
//========================================================================================
//_____________________________________________________________________________________________
void IRLivenessPrinter::printNode(const Node * node, uint32 indent)
{
	assert(irManager!=NULL);
	::std::ostream& os = getStream();
	printNodeHeader(node, indent);
    os << ::std::endl;

	LiveSet * ls=irManager->getLiveAtEntry(node);
	os<<"Live at entry: "; printLiveSet(ls); os<<::std::endl;

	MemoryManager mm(0x100, "IRLivenessPrinter::printNode");
	ls=new(mm) LiveSet(mm, irManager->getOpndCount());
	irManager->getLiveAtExit(node, *ls);
	os<<"Live at exit: "; printLiveSet(ls); os<<::std::endl;

	os << ::std::endl;
}

//_____________________________________________________________________________________________
void IRLivenessPrinter::printLiveSet(const LiveSet * ls)
{
	::std::ostream& os = getStream();
	assert(irManager!=NULL);
	if (ls==NULL){
		os<<"Null";
		return;
	}
	for (uint32 i=0, n=irManager->getOpndCount(); i<n; i++){
		Opnd * opnd=irManager->getOpnd(i);
		if (ls->isLive(opnd)){
			printOpndName(opnd); os<<"("<<opnd->getId()<<")"<<" ";
		}
	}
}

//========================================================================================
// class IROpndPrinter
//========================================================================================
//_____________________________________________________________________________________________
void IROpndPrinter::printBody(uint32 indent)
{
	assert(irManager!=NULL);
	::std::ostream& os = getStream();
	for (uint32 i=0, n=irManager->getOpndCount(); i<n; i++){
		printIndent(indent);
		Opnd * opnd=irManager->getOpnd(i);
		printOpnd(opnd);
		os<<'\t';
		os<<"addr="<<opnd;
		os<<::std::endl;
		printIndent(indent+1);
		os<<"Initial constraint: ";
		printConstraint(opnd->getConstraint(Opnd::ConstraintKind_Initial));
		os<<::std::endl;
		printIndent(indent+1);
		os<<"Calculated constraint: ";
		printConstraint(opnd->getConstraint(Opnd::ConstraintKind_Calculated));
		os<<::std::endl;
		printIndent(indent+1);
		os<<"Location constraint: ";
		printConstraint(opnd->getConstraint(Opnd::ConstraintKind_Location));
		os<<::std::endl;
		printIndent(indent);
	}
}

//_____________________________________________________________________________________________
void IROpndPrinter::printHeader(uint32 indent)
{
	assert(irManager!=NULL);
	::std::ostream& os = getStream();
	os << ::std::endl; printIndent(indent);
	os << "...................................................................." << ::std::endl; printIndent(indent);
	os << irManager->getMethodDesc().getParentType()->getName()<<"."<<irManager->getMethodDesc().getName()<<": Operands in " << (title?title:"???") << ::std::endl; printIndent(indent);
	os << "...................................................................." << ::std::endl; printIndent(indent);
	os << ::std::endl; 
}


//========================================================================================
// class IRInstConstraintPrinter
//========================================================================================

//_____________________________________________________________________________________________
void IRInstConstraintPrinter::printOpnd(const Inst * inst, uint32 idx, bool isLiveBefore, bool isLiveAfter)
{
	::std::ostream& os = getStream();
	Opnd * opnd=inst->getOpnd(idx);
	printOpndName(opnd);
	os<<"(";		
	printConstraint(inst->getConstraint(Inst::ConstraintKind_Current, idx));
	os<<")";
}



//========================================================================================
// class OpcodeDescriptionPrinter
//========================================================================================
//_____________________________________________________________________________________________
void OpcodeDescriptionPrinter::printConstraint(Constraint c)
{
	::std::ostream& os = getStream();
	if (c.isNull()){
		os<<"Null";
		return;
	}
	os<<getOpndSizeString(c.getSize())<<":";
	bool written=false;
	uint32 kind=c.getKind();
	if (kind & OpndKind_Imm){
		if (written) os<<"|";
		os << "Imm";
		written=true;
	}
	if (kind & OpndKind_Mem){
		if (written) os<<"|";
		os << "Mem";
		written=true;
	}

	if (kind & OpndKind_Reg){
		if (written) os<<"|";
		os << getOpndKindString((OpndKind)(kind & OpndKind_Reg));
		os <<"{";
		{
			bool written=false;
			uint32 mask=c.getMask();
			for (uint32 i=1, idx=0; i; i<<=1, idx++){
				if (mask&i){
					const char * regName=getRegNameString(getRegName((OpndKind)(kind&OpndKind_Reg), c.getSize(), idx));
					if (regName!=NULL){
						if (written) os<<"|";
						os << regName;
						written=true;
					}
				}
			}
		}
		os <<"}";
		written=true;
	}
}

//_____________________________________________________________________________________________
void OpcodeDescriptionPrinter::printRegName(const RegName regName)
{
	const char * regNameString=getRegNameString(regName);
	getStream()<<(regNameString?regNameString:"???");
}

//_________________________________________________________________________________________________
void OpcodeDescriptionPrinter::printOpndRoles(uint32 roles)
{
	::std::ostream& os=getStream();
	if (roles&Inst::OpndRole_Def)	os<<"D";
	if (roles&Inst::OpndRole_Use)	os<<"U";
}

//_________________________________________________________________________________________________
void OpcodeDescriptionPrinter::printOpndRolesDescription(const Encoder::OpndRolesDescription * ord)
{
	::std::ostream& os=getStream();
	os<<"count: "<<(uint32)ord->count<<" (D:"<<ord->defCount<<",U:"<<ord->useCount<<"); roles: "; 
	for (uint32 i=0; i<ord->count; i++){
		if (i>0)
			os<<',';
		printOpndRoles(Encoder::getOpndRoles(*ord, i));
	}
}

//========================================================================================
// class IRDotPrinter
//========================================================================================

void IRDotPrinter::printNode(const Node * node)
{
	::std::ostream& out=getStream();
    printNodeName(node); 
    out << " [label=\"";

	BasicBlock * bb=node->hasKind(Node::Kind_BasicBlock)?(BasicBlock*)node:NULL;
	if (bb)out << "{";
	printNodeName(node);
	if (node->getPersistentId()!=UnknownId)
		out << " pid: " << node->getPersistentId() << " ";
	if(node->getExecCnt() > 0)
        out << " ec:" << node->getExecCnt() << " ";

	if (node->hasLoopInfo() && (node->isLoopHeader()||node->getLoopHeader())) {
        out << "  Loop: Depth=" << node->getLoopDepth();
        if (node->isLoopHeader()) 
            out << ", hdr, hdr= ";
        else
            out << ", !hdr, hdr=";
        printNodeName(node->getLoopHeader());
    }

	if (bb!=NULL && irManager->getCodeStartAddr()!=NULL){
		out<<", code="<<(void*)bb->getCodeStartAddr();
	}

	if (bb) {
        out << "\\l|\\" << ::std::endl;
        VectorHandler* lirMapHandler = NULL;
        if (irManager->getCompilationInterface().isBCMapInfoRequired()) {
            MethodDesc* meth = irManager->getCompilationInterface().getMethodToCompile();
            lirMapHandler = new(irManager->getMemoryManager()) VectorHandler(bcOffset2LIRHandlerName, meth);
            assert(lirMapHandler);
        }
		for (Inst * inst = bb->getInsts().getFirst(); inst != NULL; inst = bb->getInsts().getNext(inst)) {
			Inst::Kind kind=inst->getKind();
			if ((kind & instFilter)==(uint32)kind){
				printInst(inst);
                if (lirMapHandler != NULL) {
                    uint64 bcOffset = 0;
                    uint64 instID = inst->getId();
                    bcOffset = lirMapHandler->getVectorEntry(instID);
                    if (bcOffset != ILLEGAL_VALUE) out<<" bcOff: "<< (uint16)bcOffset << " ";
                }
                out << "\\l\\" << ::std::endl;
			}
		}
        out << "}";
    }
    out << "\"";
    if (node->hasKind(Node::Kind_DispatchNode))
        out << ",shape=diamond,color=blue";
    else if (node->hasKind(Node::Kind_UnwindNode))
        out << ",shape=diamond,color=red";
    else if (node->hasKind(Node::Kind_ExitNode))
        out << ",shape=ellipse,color=green";
    out << "]" << ::std::endl;
}

//_________________________________________________________________________________________________
void IRDotPrinter::printEdge(const Edge * edge)
{
	::std::ostream& out=getStream();
	Node * from=edge->getNode(Direction_Tail);
	Node * to=edge->getNode(Direction_Head);
	printNodeName(from);
	out<<" -> ";
	printNodeName(to);
	out<<" [taillabel=\"";
	if (edge->getProbability()>=0.0)
		out<<"p: "<<edge->getProbability();
	out<<"\"";

	if (edge->isFallThroughEdge())
		out<<",style=bold";
	else if (edge->isDirectBranchEdge())
		;
	else if (to->hasKind(Node::Kind_DispatchNode))
		out<<",style=dotted,color=blue";
	else if (to->hasKind(Node::Kind_UnwindNode)||from->hasKind(Node::Kind_UnwindNode))
		out<<",style=dotted,color=red";
	else if (to->hasKind(Node::Kind_ExitNode))
		out<<",style=dotted,color=green";
	if (edge->isLoopExit())
		out<<",arrowtail=inv";

	if (edge->hasKind(Edge::Kind_CatchEdge)){
		out<<",color=blue,headlabel=\"Type: ";
		printType(((CatchEdge*)edge)->getType());
		out<<" pri:"<<((CatchEdge*)edge)->getPriority()<<"\"";
	}
	out<<"];"<<::std::endl;
}

//_________________________________________________________________________________________________
void IRDotPrinter::printLayoutEdge(const BasicBlock * from, const BasicBlock * to)
{
	::std::ostream& out=getStream();
	printNodeName(from);
	out<<" -> ";
	printNodeName(to);
	out<<" [";
	out<<"style=dotted,color=gray";
	out<<"]";
}

//_________________________________________________________________________________________________
void IRDotPrinter::printHeader(uint32 indent)
{
	assert(irManager!=NULL);
	getStream() << "digraph dotgraph {" << ::std::endl
        << "center=TRUE;" << ::std::endl
        << "margin=\".2,.2\";" << ::std::endl
        << "ranksep=\".25\";" << ::std::endl
        << "nodesep=\".20\";" << ::std::endl
        << "page=\"200,260\";" << ::std::endl
        << "ratio=auto;" << ::std::endl
        << "fontpath=\"c:\\winnt\\fonts\";" << ::std::endl
        << "node [shape=record,fontname=\"Courier\",fontsize=9];" << ::std::endl
        << "edge [minlen=2];" << ::std::endl
        << "label=\""
        << irManager->getMethodDesc().getParentType()->getName()
        << "::"
        << irManager->getMethodDesc().getName()
		<<" - "<<title
        << "\";" << ::std::endl;

}

//_________________________________________________________________________________________________
void IRDotPrinter::printEnd(uint32 indent)
{
    getStream() << "}" << ::std::endl;
}

//_________________________________________________________________________________________________
void IRDotPrinter::printLiveness()
{
	::std::ostream& out=getStream();

	out<<"subgraph cluster_liveness {"<<::std::endl;
	out<<"label=liveness"<<::std::endl;
	for(CFG::NodeIterator it(*irManager, CFG::OrderType_Topological); it!=NULL; ++it) {
		const Node * node=it;
		const LiveSet * ls=irManager->getLiveAtEntry(node);
		out<<"liveness_"; printNodeName(node); 
		out << " [label=\""; printNodeName(node); out<<":";
		if (ls){
			for (uint32 i = 0; i < ls->getSetSize(); i++) {
				if (ls->getBit(i))
					out << " " << i;
			}
		}else
			out<<" UNKNOWN";
		out <<"\"]"; out<<::std::endl;
	}

	Node * lastNode=0;
	for(CFG::NodeIterator it(*irManager, CFG::OrderType_Topological); it!=NULL; ++it) {
		if (lastNode){
			out<<"liveness_"; printNodeName(lastNode);
			out<<" -> ";
			out<<"liveness_"; printNodeName(it); 
			out<<";"<<::std::endl;
		}
		lastNode=it;
	}

	out<<"}"<<::std::endl;
}

//_________________________________________________________________________________________________
void IRDotPrinter::printTraversalOrder(CFG::OrderType orderType)
{
	::std::ostream& out=getStream();

	const char * prefix=
		orderType==CFG::OrderType_Topological?"topological":
		orderType==CFG::OrderType_Postorder?"postorder":
		orderType==CFG::OrderType_Layout?"layout":
		orderType==CFG::OrderType_Arbitrary?"arbitrary":
		"order";

	out<<"subgraph cluster_"<<prefix<<" {"<<::std::endl;
	out<<"label="<<prefix<<::std::endl;

	for(CFG::NodeIterator it(*irManager, orderType); it!=NULL; ++it) {
		out<<prefix<<"_"; printNodeName(it); out<<"[label="; printNodeName(it); out<<"]"<<::std::endl;
	}

	Node * lastNode=0;
	for(CFG::NodeIterator it(*irManager, orderType); it!=NULL; ++it) {
		if (lastNode){
			out<<prefix<<"_"; printNodeName(lastNode);
			out<<" -> ";
			out<<prefix<<"_"; printNodeName(it); 
			out<<";"<<::std::endl;
		}
		lastNode=it;
	}
	out<<"}"<<::std::endl;
}

//_________________________________________________________________________________________________
void IRDotPrinter::printBody(uint32 indent)
{
	assert(irManager!=NULL);

	printTraversalOrder(CFG::OrderType_Topological);
	printTraversalOrder(CFG::OrderType_Postorder);
	if (irManager->isLaidOut())
		printTraversalOrder(CFG::OrderType_Layout);
	printCFG(0);
	printLiveness();

}

//_________________________________________________________________________________________________
void IRDotPrinter::printCFG(uint32 indent)
{
	assert(irManager!=NULL);
	for(CFG::NodeIterator it(*irManager); it!=NULL; ++it) 
        printNode(it.getNode());
    for(CFG::NodeIterator it(*irManager); it!=NULL; ++it) {
		const Edges& es=it.getNode()->getEdges(Direction_Out);
        for(Edge * e=es.getFirst(); e!=NULL; e=es.getNext(e))  
            printEdge(e);
    }

	if (irManager->isLaidOut()){
		for(CFG::NodeIterator it(*irManager); it!=NULL; ++it) {
			if (it.getNode()->hasKind(Node::Kind_BasicBlock)){
				BasicBlock * from=(BasicBlock*)it.getNode();
				BasicBlock * to=from->getLayoutSucc();
				if (to!=NULL)
					printLayoutEdge(from, to);
			}
		}	
	}

}

//_________________________________________________________________________________________________
void IRDotPrinter::print(uint32 indent)
{
	IRPrinter::print();
}

//_________________________________________________________________________________________________

//========================================================================================
// class IRLivenessDotPrinter
//========================================================================================

//_________________________________________________________________________________________________
void IRLivenessDotPrinter::printBody(uint32 indent)
{
	assert(irManager!=NULL);
	setOpndFlavor(OpndFlavor_Location);
	printCFG(0);
}

//_________________________________________________________________________________________________
char * IRLivenessDotPrinter::getRegString(char * str, Constraint c, StlVector<Opnd *> opnds)
{
	char * retStr=NULL;
	uint32 si=0;
	for (uint32 i=0, n=opnds.size(); i<n; i++){
		Opnd * o=opnds[i];
		if (o->isPlacedIn(c)){
			retStr=str;
			RegName r=o->getRegName();
			str[si++]=(char)('0'+getRegIndex(r));
		}else
			str[si++]='_';
		for (uint32 j=0, l=getOpndNameLength(o)-1; j<l; j++) 
			str[si++]='_';
	}
	str[si++]=0;
	return retStr;
}

//_________________________________________________________________________________________________
void IRLivenessDotPrinter::printNode(const Node * node)
{
	::std::ostream& out=getStream();
	MemoryManager mm(0x2000, "IRLivenessDotPrinter::printNode");
    printNodeName(node); 
    out << " [label=\"";

	BasicBlock * bb=node->hasKind(Node::Kind_BasicBlock)?(BasicBlock*)node:NULL;
	if (bb!=NULL) out << "{";
	printNodeName(node);
	if (node->getPersistentId()!=UnknownId)
		out << " pid: " << node->getPersistentId() << " ";
	if(node->getExecCnt() > 0)
        out << " ec:" << node->getExecCnt() << " ";

	if (node->hasLoopInfo() && (node->isLoopHeader()||node->getLoopHeader())) {
        out << "  Loop: Depth=" << node->getLoopDepth();
        if (node->isLoopHeader()) 
            out << ", hdr, hdr= ";
        else
            out << ", !hdr, hdr=";
        printNodeName(node->getLoopHeader());
    }

	if (bb!=NULL && irManager->getCodeStartAddr()!=NULL){
		out<<", code="<<(void*)bb->getCodeStartAddr();
	}

	if (bb!=NULL){
		if (irManager->hasLivenessInfo()){

			StlVector<LiveSet *> liveSets(mm);
			StlVector<uint32> regUsages(mm);
			LiveSet * lsAll=new (mm) LiveSet(mm, irManager->getOpndCount());

			LiveSet * lsCurrent=new (mm) LiveSet(mm, irManager->getOpndCount());
			irManager->getLiveAtExit(node, *lsCurrent);
			lsAll->copyFrom(*lsCurrent);
			LiveSet * ls=new (mm) LiveSet(mm, irManager->getOpndCount());
			ls->copyFrom(*lsCurrent);
			liveSets.push_back(ls);

			uint32 regUsage=0, regUsageAll=0;
			irManager->getRegUsageAtExit(node, OpndKind_GPReg, regUsage);
			regUsageAll|=regUsage;
			regUsages.push_back(regUsage);

			const Insts& insts=bb->getInsts();
			for (Inst * inst = insts.getLast(); inst != NULL; inst = insts.getPrev(inst)) {
				irManager->updateLiveness(inst, *lsCurrent);
				lsAll->unionWith(*lsCurrent);
				ls=new (mm) LiveSet(mm, irManager->getOpndCount());
				ls->copyFrom(*lsCurrent);
				liveSets.push_back(ls);

				irManager->updateRegUsage(inst, OpndKind_GPReg, regUsage);
				regUsageAll|=regUsage;
				regUsages.push_back(regUsage);
			}

			assert(irManager->getLiveAtEntry(node)->isEqual(*lsCurrent));

			StlVector<Opnd *> opndsAll(mm);

			for (uint32 i=0, n=irManager->getOpndCount(); i<n; i++){
				Opnd * opnd=irManager->getOpnd(i);
				if (lsAll->isLive(opnd))
					opndsAll.push_back(opnd);
			}
			char * regStrings[IRMaxRegKinds]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* VSH: TODO - reg kinds are masks now, not indexes !			
*/

			out << "\\l|{\\" << ::std::endl;

			out<<"Operand Ids:\\l\\"<<::std::endl;

			uint32 regKindCount=0;
			for (uint32 i=0; i<IRMaxRegKinds; i++){
				if (regStrings[i]!=NULL){
					regKindCount++;
					out<<getOpndKindString((OpndKind)i) << "\\l\\" << ::std::endl;
				}
			}

			for (Inst * inst = insts.getFirst(); inst != NULL; inst = insts.getNext(inst)) {
				printInst(inst); out<<"\\l\\"<<::std::endl;
			}

			out << "|\\" << ::std::endl;
			for (uint32 i=0, n=opndsAll.size(); i<n; i++){
				out<<opndsAll[i]->getFirstId()<<'_';
			}
			out << "\\l\\" << ::std::endl;

			for (uint32 i=0; i<IRMaxRegKinds; i++){
				if (regStrings[i]!=NULL)
					out << regStrings[i] << "\\l\\" << ::std::endl;
			}

			uint32 idx=liveSets.size()-1;

			for (Inst * inst = insts.getFirst(); inst != NULL; inst = insts.getNext(inst), idx--) {
				printLivenessForInst(opndsAll, liveSets[idx], liveSets[idx-1]); // output at entry
				out << "\\l\\" << ::std::endl;
			}

			if (regUsageAll!=0){
				out << "|\\" << ::std::endl;

				out << "01234567" << "\\l\\" << ::std::endl;
				for (uint32 i=1; i<regKindCount; i++)
					out << "\\l\\" << ::std::endl;

				for (int32 i=(int32)regUsages.size()-1; i>=0; i--) {
					uint32 regUsage=regUsages[i];
					for (uint32 m=1; m!=0x100; m<<=1)
						out << (m&regUsage?'.':'_');
					out << "\\l\\" << ::std::endl;
				}
			}

			out << "}\\" << ::std::endl;
		
		}else{
			out<<"liveness info is outdated";
		}
	}
	if (bb!=NULL) out << "}";
	out << "\"";
    if (node->hasKind(Node::Kind_DispatchNode))
        out << ",shape=diamond,color=blue";
    else if (node->hasKind(Node::Kind_UnwindNode))
        out << ",shape=diamond,color=red";
    else if (node->hasKind(Node::Kind_ExitNode))
        out << ",shape=ellipse,color=green";
    out << "]" << ::std::endl;
}

//_________________________________________________________________________________________________
void IRLivenessDotPrinter::printLivenessForInst(const StlVector<Opnd*> opnds, const LiveSet * ls0, const LiveSet * ls1)
{
	::std::ostream& out=getStream();
	for (uint32 i=0, n=opnds.size(); i<n; i++){
		Opnd * opnd=opnds[i];
		bool isLiveBefore=ls0!=NULL && ls0->isLive(opnd);
		bool isLiveAfter=ls1!=NULL && ls1->isLive(opnd);
		if (isLiveAfter && isLiveBefore)
			out<<'I';
		else if (isLiveAfter)
			out<<'.';
		else if (isLiveBefore)
			out<<'\'';
		else
			out<<'_';
		for (uint32 j=0, l=getOpndNameLength(opnd)-1; j<l; j++) 
			out<<'_';
	}
}



//_________________________________________________________________________________________________
void dumpIR(
			const IRManager * irManager,
			uint32 stageId,
			const char * readablePrefix,
			const char * readableStageName,
			const char * stageTagName,
			const char * subKind1, 
			const char * subKind2,
			uint32 instFilter, 
			uint32 opndFlavor,
			uint32 opndRolesFilter
			)
{
	Log::out() << "-------------------------------------------------------------" << ::std::endl;

	char title[128];
	strcpy(title, readablePrefix);
	strcat(title, readableStageName);

	char subKind[128];
	assert(subKind1!=NULL);
	strcpy(subKind, subKind1);
	if (subKind2!=NULL && subKind2[0]!=0){
		strcat(subKind, ".");
		strcat(subKind, subKind2);
	}

	Log::printIRDumpBegin(stageId, readableStageName, subKind);
	if (subKind2!=0){
		if (stricmp(subKind2, "opnds")==0){
			IROpndPrinter(irManager, title)
				.setInstFilter(instFilter)
				.setStream(Log::out()).print();
		}else if (stricmp(subKind2, "inst_constraints")==0){
			IRInstConstraintPrinter(irManager, title)
				.setInstFilter(instFilter)
				.setStream(Log::out()).print();
		}else if (stricmp(subKind2, "liveness")==0){
			IRLivenessPrinter(irManager, title)
				.setInstFilter(instFilter)
				.setStream(Log::out()).print();
		}
	}else{
		IRPrinter(irManager, title)
			.setInstFilter(instFilter)
			.setStream(Log::out()).print();
	}
	Log::printIRDumpEnd(stageId, readableStageName, subKind);
}

//_________________________________________________________________________________________________
void printDot(
			const IRManager * irManager,
			uint32 stageId,
			const char * readablePrefix,
			const char * readableStageName,
			const char * stageTagName,
			const char * subKind1, 
			const char * subKind2,
			uint32 instFilter, 
			uint32 opndFlavor,
			uint32 opndRolesFilter
			)
{
	char title[128];
	strcpy(title, readablePrefix);
	strcat(title, readableStageName);

	char subKind[256]; 
	assert(subKind1!=NULL);
	sprintf(subKind, "%s.%d.%s", stageTagName, (int)stageId, subKind1);
	if (subKind2!=NULL && subKind2[0]!=0){
		strcat(subKind, ".");
		strcat(subKind, subKind2);
	}

	if (subKind2!=0){
		if (stricmp(subKind2, "liveness")==0){
			IRLivenessDotPrinter(irManager, title)
				.setInstFilter(instFilter)
				.createDotFileName(Log::getDotFileDirName(), subKind).print();
		}
	}else{
		IRDotPrinter(irManager, title)
			.setInstFilter(instFilter)
			.createDotFileName(Log::getDotFileDirName(), subKind).print();
	}
}

//_________________________________________________________________________________________________
void printRuntimeArgs(::std::ostream& os, uint32 opndCount, CallingConvention::OpndInfo * infos, JitFrameContext * context)
{
	MemoryManager mm(0x1000, "printRuntimeOpndInternalHelper");
	DrlVMTypeManager tm(mm); tm.init();
	os<<opndCount<<" args: ";
	for (uint32 i=0; i<opndCount; i++){
		CallingConvention::OpndInfo & info=infos[i];
		uint32 cb=0;
		uint8 arg[4*sizeof(uint32)]; 
		for (uint32 j=0; j<info.slotCount; j++){
			if (getRegKind(info.slots[j])==OpndKind_Mem){
				*(uint32*)(arg+cb)=((uint32*)context->esp)[getRegIndex(info.slots[j])];
				cb+=sizeof(uint32);
			}else{
				assert((getRegKind(info.slots[j])&OpndKind_Reg)!=0);
				assert(0); 			        
			}	
		}
		if (i>0)
			os<<", ";
		printRuntimeOpnd(os, tm, (Type::Tag)info.typeTag, (const void*)arg);
	}
}

//_________________________________________________________________________________________________
void printRuntimeOpnd(::std::ostream& os, TypeManager & tm, Type::Tag typeTag, const void * p)
{
	if (Type::isObject(typeTag)){
		printRuntimeObjectOpnd(os, tm, *(const void**)p);
	}else{
		tm.getPrimitiveType(typeTag)->print(os); 
		os<<" ";
		switch (typeTag){
	        case Type::Int8:
				os<<*(int8*)p;
				break;
			case Type::UInt8:
				os<<*(uint8*)p;
				break;
			case Type::Boolean:
				os<<(*(int8*)p?true:false);
				break;
			case Type::Int16:   
				os<<*(int16*)p;
				break;
			case Type::UInt16:
				os<<*(uint16*)p;
				break;
			case Type::Char:
				os<<*(uint16*)p<<*(char*)p;
				break;
			case Type::Int32:
				os<<*(int32*)p;
				break;
			case Type::UInt32:
				os<<*(uint32*)p;
				break;
			case Type::Int64:   
				os<<*(int64*)p;
				break;
			case Type::UInt64:
				os<<*(uint64*)p;
				break;
			case Type::Double:
			case Type::Float:
				os<<*(double*)p;
				break;
			case Type::Single:
				os<<*(float*)p;
				break;
			default:
				assert(0);
				break;
		}
	}
}

//_________________________________________________________________________________________________
void printRuntimeObjectOpnd(::std::ostream& os, TypeManager & tm, const void * p)
{
//	check for valid range for object pointer
	if (p==NULL){
		os<<"Ref Null";
		return;
	}
	os<<"Ref "<<p;
	if (p<(const void*)0x10000||p>(const void*)0x20000000){ 
		os<<"(INVALID PTR)";
		return;
	}
// check for valid alignment
	if ((((uint32)p)&0x3)!=0){
		os<<"(INVALID PTR)";
		return;
	}
	uint32 vtableOffset=tm.getVTableOffset();

	//	assume that vtable pointer in object head is allocation handle 
	void * allocationHandle=*(void**)(((uint8*)p)+vtableOffset);
		
	if (allocationHandle<(void*)0x10000||allocationHandle>(void*)0x20000000||(((uint32)allocationHandle)&0x3)!=0){
		os<<"(INVALID VTABLE PTR: "<<allocationHandle<<")";
		return;
	}

	ObjectType * type=tm.getObjectTypeFromAllocationHandle(allocationHandle);
	if (type==NULL){
		os<<"(UNRECOGNIZED VTABLE PTR: "<<allocationHandle<<")";
		return;
	}
	os<<"(";
	printRuntimeObjectContent(os, tm, type, p);
	os<<":"<<type->getName();
	os<<")";
}

//_________________________________________________________________________________________________
void printRuntimeObjectContent_Array(::std::ostream& os, TypeManager & tm, Type * type, const void * p)
{
	ArrayType * arrayType=(ArrayType *)type;
	uint32 lengthOffset=arrayType->getArrayLengthOffset();
	uint32 length=*(uint32*)(((uint8*)p)+lengthOffset);
	os<<"{"<<length<<" elems: ";
	if (length>0){
		uint32 elemOffset=arrayType->getArrayElemOffset();
		printRuntimeOpnd(os, tm, arrayType->getElementType()->tag, (const void*)(((uint8*)p)+elemOffset));
		if (length>1)
			os<<", ...";
	}
	os<<"}";
}

//_________________________________________________________________________________________________
void printRuntimeObjectContent_String(::std::ostream& os, TypeManager & tm, Type * type, const void * p)
{
#ifndef KNOWN_STRING_FORMAT
	os<<"\"...\"";
	return;
#else
	uint32 stringLengthOffset=8;
	uint32 stringOffsetOffset=12;
	uint32 stringBufferOffset=16;
	uint32 bufferLengthOffset=8;
	uint32 bufferElemsOffset=12;
	
	uint8 * string=(uint8*)p;

	uint32	stringLength=*(uint32*)(string+stringLengthOffset);
	uint32	stringOffset=*(uint32*)(string+stringOffsetOffset);
	uint8 * buffer=*(uint8**)(string+stringBufferOffset);

	if (buffer==NULL){
		if (stringLength==0)
			os<<"\"\"";
		else
			os<<"INVALID STRING";
		return;
	}

	uint32 bufferLength=*(uint32*)(buffer+bufferLengthOffset);

	uint16 * bufferElems=(uint16*)(buffer+bufferElemsOffset);

	if (stringOffset>bufferLength || stringLength>bufferLength || stringLength>bufferLength-stringOffset){
		os<<"INVALID STRING";
		return;
	}

	os<<"\"";
	for (uint32 i=stringOffset, n=stringOffset+stringLength; i<n; i++)
		os<<(char)bufferElems[i];
	os<<"\"";
#endif
}

//_________________________________________________________________________________________________
void printRuntimeObjectContent(::std::ostream& os, TypeManager & tm, Type * type, const void * p)
{
	if (type->isArray()){
		os<<" ";
		printRuntimeObjectContent_Array(os, tm, type, p);		
	}else if (type->isSystemString()){
		os<<" ";
		printRuntimeObjectContent_String(os, tm, type, p);		
	}
}

//_________________________________________________________________________________________________
void __stdcall printRuntimeOpndInternalHelper(const void * p)
{
	MemoryManager mm(0x1000, "printRuntimeOpndInternalHelper");
	DrlVMTypeManager tm(mm); tm.init();
	printRuntimeObjectOpnd(::std::cout, tm, p);
}


}}; //namespace Ia32

