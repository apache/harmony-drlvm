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
 * @version $Revision: 1.19.8.4.4.3 $
 */

#define IRTRANSFORMER_REGISTRATION_ON 1

#include <fstream>
#include "Stl.h"
#include "Ia32CodeGenerator.h"
#include "Ia32CodeSelector.h"
#include "Log.h"
#include "Ia32IRManager.h"
#include "Ia32Printer.h"

#include "Ia32DCE.h"
#include "Ia32RCE.h"
#include "Ia32ComplexAddrFormLoader.h"
#include "Ia32CodeLayout.h"
#include "Ia32ConstraintsResolver.h"
#include "Ia32RegAlloc2.h"
#include "Ia32SpillGen.h"
#include "Ia32CopyExpansion.h"
#include "Ia32CodeEmitter.h"
#include "Ia32StackLayout.h"
#include "Ia32GCMap.h"
#include "Ia32I8Lowerer.h"
#include "Ia32InternalTrace.h"
#include "Ia32GCSafePoints.h"
#include "Ia32RCE.h"
#include "CGSupport.h"
#include "Ia32InternalProfiler.h"
#include "Ia32BBPolling.h"

namespace Jitrino
{
namespace Ia32{

//___________________________________________________________________________________________________
void _cdecl die(uint32 retCode, const char * message, ...)
{
	::std::cerr<<"---------- die called (ret code = "<<retCode<<") --------------------------------------"<<::std::endl;
	if (message!=NULL){
		va_list args;
		va_start(args, message);
		char str[0x10000];
		vsprintf(str, message, args);
		::std::cerr<<str<<::std::endl;
	}
	exit(retCode);
}

//___________________________________________________________________________________________________
BEGIN_DECLARE_IRTRANSFORMER(CodeSelector, "selector", "Code selector")
	IRTRANSFORMER_CONSTRUCTOR(CodeSelector)
	void runImpl() 
	{
		::Jitrino::MethodCodeSelector * methodCodeSelector=(::Jitrino::MethodCodeSelector *)irManager.getInfo("methodCodeSelector");
		assert(methodCodeSelector);
		MemoryManager  codeSelectorMemManager(1024, "CodeGenerator::selectCode.codeSelectorMemManager");
		MethodCodeSelector    codeSelector(irManager.getCompilationInterface(),
			irManager.getMemoryManager(), codeSelectorMemManager, irManager);

		printIRDumpBegin("hir2lir.log");
		methodCodeSelector->selectCode(codeSelector);
		printIRDumpEnd("hir2lir.log");
	}
	uint32 getNeedInfo()const{	return 0;	}
END_DECLARE_IRTRANSFORMER(CodeSelector)

//___________________________________________________________________________________________________
BEGIN_DECLARE_IRTRANSFORMER(InstructionFormTranslator, "native", "Translation to native form")
	IRTRANSFORMER_CONSTRUCTOR(InstructionFormTranslator)
	void runImpl(){ irManager.translateToNativeForm(); }
	uint32 getNeedInfo()const{ return 0; }
	uint32 getSideEffects()const{ return 0; }
END_DECLARE_IRTRANSFORMER(InstructionFormTranslator) 

//___________________________________________________________________________________________________
BEGIN_DECLARE_IRTRANSFORMER(UserRequestedDie, "die", "User requested run time termination")
	IRTRANSFORMER_CONSTRUCTOR(UserRequestedDie)
	void runImpl(){ die(10, parameters); }
	uint32 getNeedInfo()const{ return 0; }
	uint32 getSideEffects()const{ return 0; }
	bool isIRDumpEnabled(){ return false; }
END_DECLARE_IRTRANSFORMER(UserRequestedDie) 

//___________________________________________________________________________________________________
BEGIN_DECLARE_IRTRANSFORMER(UserRequestedReturnFalse, "false", "User requested rejection of code generation (return false)")
	IRTRANSFORMER_CONSTRUCTOR(UserRequestedReturnFalse)
	void runImpl(){  }
	uint32 getNeedInfo()const{ return 0; }
	uint32 getSideEffects()const{ return 0; }
	bool isIRDumpEnabled(){ return false; }
END_DECLARE_IRTRANSFORMER(UserRequestedReturnFalse) 
	
//___________________________________________________________________________________________________
BEGIN_DECLARE_IRTRANSFORMER(UserRequestedBreakPoint, "break", "User requested break point at beginning of each method")
	IRTRANSFORMER_CONSTRUCTOR(UserRequestedBreakPoint)
	void runImpl(){ 
		irManager.getPrologNode()->prependInsts(irManager.newInst(Mnemonic_INT3));
	}
	uint32 getNeedInfo()const{ return 0; }
	uint32 getSideEffects()const{ return 0; }
	bool isIRDumpEnabled(){ return false; }
END_DECLARE_IRTRANSFORMER(UserRequestedBreakPoint) 

//___________________________________________________________________________________________________

BEGIN_DECLARE_IRTRANSFORMER(EarlyPropagation, "early_prop", "Early Propagation")
	IRTRANSFORMER_CONSTRUCTOR(EarlyPropagation)

	struct OpndInfo
	{
		uint32 defCount;
		Inst * sourceInst;
		uint32 sourceOpndId;
		uint32 sourceOpndDefCountAtCopy;
		OpndInfo()
			:defCount(0), sourceInst(NULL), sourceOpndId(EmptyUint32), sourceOpndDefCountAtCopy(0){}
	};
	
	void runImpl()
	{ 
		irManager.updateLoopInfo();
		uint32 opndCount=irManager.getOpndCount();

		MemoryManager mm(0x100 + sizeof(OpndInfo) * opndCount + sizeof(Opnd*) * opndCount, "early_prop");
		OpndInfo * opndInfos = new(mm) OpndInfo[opndCount];
		Node * currentLoopHeader = NULL;

		bool anyInstHandled=false;

		for (CFG::NodeIterator it(irManager, CFG::OrderType_Topological); it!=NULL; ++it){
			Node * node=it;
			if (!node->hasKind(Node::Kind_BasicBlock)) 
				continue;
			const Insts& insts = ((BasicBlock*)node)->getInsts();
			Node * loopHeader = node->isLoopHeader() ? node : node->getLoopHeader();
			if (currentLoopHeader != loopHeader){
				currentLoopHeader = loopHeader;
				for (uint32 i = 0; i < opndCount; ++i)
					if (opndInfos[i].sourceOpndId != EmptyUint32)
						opndInfos[i].defCount++;
			}

			for (Inst * inst = insts.getFirst(); inst != NULL; inst=insts.getNext(inst)){
				bool assignedOpndPropagated = false;
				Inst::Opnds opnds(inst, Inst::OpndRole_All);
				for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
					Opnd * opnd=inst->getOpnd(it);
					uint32 roles=inst->getOpndRoles(it);
					uint32 opndId = opnd->getId();
					OpndInfo& opndInfo = opndInfos[opndId];

					if (roles & Inst::OpndRole_Def){
						++opndInfo.defCount;
					}else if (roles & Inst::OpndRole_Use){
						if (opndInfo.sourceOpndId != EmptyUint32){
							if (opndInfo.sourceOpndDefCountAtCopy < opndInfos[opndInfo.sourceOpndId].defCount)
								opndInfo.sourceOpndId = EmptyUint32;
							else{
								Opnd * srcOpnd = irManager.getOpnd(opndInfo.sourceOpndId);
								Constraint co = srcOpnd->getConstraint(Opnd::ConstraintKind_Location);
	                            if (co.getKind() == OpndKind_Mem){
	                            	if ((roles & Inst::OpndRole_Explicit) == 0 ||
	                            		inst->hasKind(Inst::Kind_PseudoInst) || irManager.isGCSafePoint(inst) ||
	                            		opndInfo.sourceInst != insts.getPrev(inst) || assignedOpndPropagated ||
                                        (inst->getConstraint(Inst::ConstraintKind_Weak, it, co.getSize())&co).isNull()
	                            	)
										opndInfo.sourceOpndId = EmptyUint32;
                                    assignedOpndPropagated = true;
	                            }
							}
						}
					}
					if (opndInfo.defCount > 1){
						opndInfo.sourceOpndId = EmptyUint32;
					}
				}
				bool isCopy = inst->getMnemonic() == Mnemonic_MOV ||(
						(inst->getMnemonic() == Mnemonic_ADD || inst->getMnemonic() == Mnemonic_SUB) && 
						inst->getOpnd(3)->isPlacedIn(OpndKind_Imm) && inst->getOpnd(3)->getImmValue()==0
						&& inst->getOpnd(3)->getRuntimeInfo()==NULL
					);

				if (isCopy){ // CopyPseudoInst or mov
					Opnd * defOpnd = inst->getOpnd(0);
					Opnd * srcOpnd = inst->getOpnd(1);
					uint32 defOpndId = defOpnd->getId();
					OpndInfo * opndInfo = opndInfos + defOpndId;
					bool instHandled=false;
					if (opndInfo->defCount == 1 && ! srcOpnd->isPlacedIn(OpndKind_Reg)){
						if (!defOpnd->hasAssignedPhysicalLocation()){
							opndInfo->sourceInst = inst;
							opndInfo->sourceOpndId = srcOpnd->getId();
							instHandled=true;
						}
					}
					if (instHandled){
						if (opndInfos[opndInfo->sourceOpndId].sourceOpndId != EmptyUint32)
							opndInfo->sourceOpndId = opndInfos[opndInfo->sourceOpndId].sourceOpndId;
						opndInfo->sourceOpndDefCountAtCopy = opndInfos[opndInfo->sourceOpndId].defCount;
						anyInstHandled=true;
					}
				}
			}
		}

		if (anyInstHandled){
			Opnd ** replacements = new(mm) Opnd* [opndCount];
			memset(replacements, 0, sizeof(Opnd*) * opndCount);
			bool hasReplacements = false;
			for (uint32 i = 0; i < opndCount; ++i){
				if (opndInfos[i].sourceOpndId != EmptyUint32){
					Inst * inst = opndInfos[i].sourceInst;
					if (inst !=NULL){
						BasicBlock * bb = inst->getBasicBlock();
						if (bb != NULL)
		               		bb->removeInst(inst);
					}
					if (opndInfos[i].sourceOpndId != i){
						replacements[i] = irManager.getOpnd(opndInfos[i].sourceOpndId);
						hasReplacements = true;
					}
				}
			}

			if (hasReplacements){
				for (CFG::NodeIterator it(irManager, CFG::OrderType_Topological); it!=NULL; ++it){
					Node * node=it;
					if (!node->hasKind(Node::Kind_BasicBlock)) 
						continue;
					const Insts& insts = ((BasicBlock*)node)->getInsts();
					for (Inst * inst = insts.getFirst(); inst != NULL; inst=insts.getNext(inst)){
						inst->replaceOpnds(replacements);
					}	
				}
			}
		}
	}

    uint32 getNeedInfo()const{ return 0; }

END_DECLARE_IRTRANSFORMER(EarlyPropagation) 



//================================================================================
//  class IRTransformerPath

//___________________________________________________________________________________________________
bool CodeGenerator::IRTransformerPath::parse(const char * str)
{
	steps.resize(0);
	error=0;
	while(*str){
		while (*str && *str<=' ') str++;
		const char * p=parseTag(str);
		if (error!=0)
			return false;
		if (p){
			str=p;

			while (*str && *str<=' ') str++;
			p=parseSwitch(str, steps.back());
			if (error!=0)
				return false;
			if (p)
				str=p;

			while (*str && *str<=' ') str++;
			p=parseParams(str, steps.back());
			if (error!=0)
				return false;
			if (p)
				str=p;

		}else
			str++;
	}

	IRManager::ConstCharStringToVoidPtrMap tagCounters(memoryManager);

	for (uint32 i=0; i<steps.size(); i++){
		const char * tag=steps[i].tag;
		IRManager::ConstCharStringToVoidPtrMap::iterator it=tagCounters.find(tag);
		uint32 counter=it==tagCounters.end()?0:(uint32)it->second;
		steps[i].extentionIndex=counter;
		tagCounters[tag]=(void*)(counter+1);

	}

	return true;
}

//___________________________________________________________________________________________________
bool CodeGenerator::IRTransformerPath::parseOption(const char * str, Step& result)
{
	error=0;
	while (*str && *str<=' ') str++;
	const char * p=parseSwitch(str, result, false);
	if (error!=0)
		return false;
	if (p)
		str=p;

	while (*str && *str<=' ') str++;
	p=parseParams(str, result);
	if (error!=0)
		return false;
	return true;
}

//___________________________________________________________________________________________________
const char * CodeGenerator::IRTransformerPath::parseTag(const char * str)
{	
	char tag[IRTransformer::MaxTagLength+1];
	uint32 i=0;
	for (; i<IRTransformer::MaxTagLength && *str && (isalpha(*str) || isdigit(*str) || *str=='_'); i++, str++)
		tag[i]=*str;
	tag[i]=0;
	if (i==0)
		return NULL;
	if (i==IRTransformer::MaxTagLength){
		error=1;
		return NULL;
	}

// not a transformer, may be just general CG parameters
	if (IRTransformer::findIRTransformer(tag)==NULL){ 
		return NULL;
	}
	steps.push_back(newString(memoryManager, tag));
	return str;
}	

//___________________________________________________________________________________________________
const char * CodeGenerator::IRTransformerPath::parseSwitch(const char * str, Step& step, bool startsWithColon)
{
	if (*str==0)
		return NULL;
	if (startsWithColon){
		if (*str!=':')
			return NULL;
		str++;
	}
	char sw[IRTransformer::MaxTagLength+1];
	uint32 i=0;
	for (; i<IRTransformer::MaxTagLength && *str && (isalpha(*str) || isdigit(*str) || *str=='_'); i++, str++)
		sw[i]=*str;
	sw[i]=0;
	if (stricmp(sw, "on")==0)
		step.tagSwitch=Step::Switch_On;
	else if (stricmp(sw, "off")==0)
		step.tagSwitch=Step::Switch_Off;
	return str;
}

//___________________________________________________________________________________________________
const char * CodeGenerator::IRTransformerPath::parseParams(const char * str, Step& step)
{
	if (*str==0 || *str!='{')
		return NULL;
	str++;
	const char * start=str;
	while (*str && *str!='}') str++;
	if (*str!='}')
		return NULL;
	step.parameters=newString(memoryManager, start, str-start);
	return ++str;
}

//___________________________________________________________________________________________________
void CodeGenerator::IRTransformerPath::readFlagsFromCommandLine(const JitrinoParameterTable *params)
{
	IRManager::ConstCharStringToVoidPtrMap tagCounters(memoryManager);
	for (uint32 i=0; i<steps.size(); i++){
		const char * tag=steps[i].tag;
		assert(tag!=NULL);
		char pname[IRTransformer::MaxTagLength+20]="ia32::";
		strcat(pname, tag);
		const char * value=params->lookup(pname);
		if (value==NULL){
			strcat(pname, "-");
			sprintf(pname+strlen(pname), "%d", (int)steps[i].extentionIndex);
			value=params->lookup(pname);
		}
		if (value!=NULL){
			Step step(tag);
			if (!parseOption(value, step))
				die(1, "Cannot parse option %s=%s", pname, value);
			if (steps[i].tagSwitch!=Step::Switch_AlwaysOn && step.tagSwitch!=Step::Switch_AlwaysOn)
				steps[i].tagSwitch=step.tagSwitch;
			if (step.parameters)
				steps[i].parameters=step.parameters;
		}
	}
}

//================================================================================
//  class CodeGenerator
//================================================================================

//___________________________________________________________________________________________________
CodeGenerator::CodeGenerator(MemoryManager &mm, CompilationInterface& compInterface) 
        : methodMemManager(mm), compilationInterface(compInterface), 
			typeManager(compInterface.getTypeManager()), 
			irTransformerPath(mm),
			irManager(mm,typeManager,*compInterface.getMethodToCompile(),compInterface),
			instrMap(NULL)
{
}


//___________________________________________________________________________________________________
//  Command line flag processing

Timer * CodeGenerator::paramsTimer=NULL;

const char * CodeGenerator::defaultPathString="selector,bbp:on{6},gcpoints,cafl:on{4},dce:on{1},i8l,early_prop:on,itrace:off,"
"native,opco:off{1},constraints,dce:on,"
		"bp_regalloc:on{EAX|ECX|EDX|EBX|ESI|EDI|EBP},"
		"bp_regalloc:on{XMM0|XMM1|XMM2|XMM3|XMM4|XMM5|XMM6|XMM7},"
		"spillgen," 
		"layout,copy,rce:on,stack,break:off,iprof:off,emitter,si_insts,gcmap,info";

//___________________________________________________________________________________________________
void CodeGenerator::readFlagsFromCommandLine(const CompilationContext* context)
{
	// just clear this way
    CGFlags& flags = *context->getIa32CGFlags();
    JitrinoParameterTable* params = context->getThisParameterTable();
	flags.dumpdot = params->lookupBool("ia32::dumpdot", false);
    flags.useOptLevel = params->lookupBool("ia32::useOptLevel",true);

#ifdef _DEBUG
	flags.verificationLevel = params->lookupUint("ia32::verify",1);
#else
	flags.verificationLevel = params->lookupUint("ia32::verify",0);
#endif
	
}
 
//___________________________________________________________________________________________________
// Help for  command line flags

void CodeGenerator::showFlagsFromCommandLine() {

	Log::out() << " ia32::dumpdot[={on|OFF}]" << ::std::endl;
}

//___________________________________________________________________________________________________
bool CodeGenerator::runIRTransformer(const char * tag, const char * params)
{
	assert(tag!=NULL);
	if (stricmp(tag, "false")==0)
		return false;
	IRTransformer * tr=IRTransformer::newIRTransformer(tag, irManager, params);
	assert(tr);
	tr->run();
	tr->destroy();
	return true;
}

//___________________________________________________________________________________________________
const char * CodeGenerator::getDefaultIRTransformerPathName()
{
/* VSH: current fast path is not fast, it increases compilation time for Eclipse startup !
*/
    return "default";
}

//___________________________________________________________________________________________________
bool CodeGenerator::createIRTransformerPath()
{
	PhaseTimer tm(paramsTimer, "ia32::params");
	const JitrinoParameterTable * methodParams=irManager.getCompilationContext()->getThisParameterTable();
	const char * pathString=methodParams->lookup("ia32::path");

	if (pathString==NULL)
	    pathString=getDefaultIRTransformerPathName();
			
	if (stricmp(pathString, "default")==0)
		pathString=defaultPathString;
	if (!irTransformerPath.parse(pathString))
		return false;
	irTransformerPath.readFlagsFromCommandLine(methodParams);
	return true;
}

//___________________________________________________________________________________________________
bool CodeGenerator::runIRTransformerPath()
{
	const StlVector<IRTransformerPath::Step> steps=irTransformerPath.getSteps();
	bool seenEmitter=false;
	for (uint32 i=0; i<steps.size(); i++){
		if (steps[i].tagSwitch & IRTransformerPath::Step::Switch_On){
			if (!runIRTransformer(steps[i].tag, steps[i].parameters))
				return false;
			if (stricmp(steps[i].tag, "emitter")==0)
				seenEmitter=true;
		}
	}
	return seenEmitter; // true only if we really produced some code
}

//___________________________________________________________________________________________________
//    Generate IA-32 code
bool CodeGenerator::genCode(::Jitrino::MethodCodeSelector& inputProvider) {
    bool result;
	irManager.setVerificationLevel(irManager.getCGFlags()->verificationLevel);
	irManager.setInfo("methodCodeSelector", &inputProvider);
	if (!createIRTransformerPath())
		die(1, "Cannot parse IRTransformer path");
    // add bc <-> HIR code map handler
    MemoryManager& ir_mmgr = irManager.getMemoryManager();
    StlVector<uint64> *lirMap;
    MethodDesc* meth = irManager.getCompilationInterface().getMethodToCompile();

    if (compilationInterface.isBCMapInfoRequired()) {
        lirMap = new(ir_mmgr) StlVector<uint64> (ir_mmgr, meth->getByteCodeSize() 
                * (ESTIMATED_HIR_SIZE_PER_BYTECODE - 1) * ESTIMATED_LIR_SIZE_PER_HIR + 5 );
        addContainerHandler(lirMap, bcOffset2LIRHandlerName, meth);
    }

    result = runIRTransformerPath();

    if (compilationInterface.isBCMapInfoRequired()) {
        removeContainerHandler(bcOffset2LIRHandlerName, meth);
    }

    return result;
}


}}; // namespace Ia32

