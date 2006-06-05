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
 * @version $Revision: 1.13.14.3.4.4 $
 */

#ifndef _IA32_STACK_INFO_H_
#define _IA32_STACK_INFO_H_

#include "CodeGenIntfc.h"
#include "open/types.h"
#include "jit_export.h"
#include "MemoryManager.h"
#include "VMInterface.h"
#include "Type.h"
#include "Ia32Constraint.h"
#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32 {

	/**
	 *	struct StackDepthInfo performs access to information about call 
	 *	instruction. It contains caller-save and callee save registers masks
	 *	and stack depth for this instruction
	 */
	
	struct StackDepthInfo {
		Constraint callerSaveRegs;
		uint32 calleeSaveRegs;
		uint32 stackDepth;
		uint32 callSize;

		StackDepthInfo(uint32 sd=0) : callerSaveRegs(OpndKind_Imm), calleeSaveRegs(0), stackDepth(sd), callSize(0) {}
		StackDepthInfo(Constraint r, uint32 e, uint32 sd=0, uint32 cz=0) : callerSaveRegs(r), calleeSaveRegs(e), stackDepth(sd), callSize(cz) {}
	};

	struct DepthEntry {
		uint32 eip;
		StackDepthInfo info;
		DepthEntry * next;

		DepthEntry(uint32 ip, StackDepthInfo i, DepthEntry * n) : eip(ip), info(i), next(n) {}
	};

	/**
	 * map DepthMap contains StackDepthInfos indexed by EIP of corresponded call
	 * instruction
	 */
	typedef StlMap<uint32, StackDepthInfo> DepthMap;

//========================================================================================
//	class StackInfo
//========================================================================================
/**
 *	class StackInfo performs stack unwinding and fixing handler context.
 *	It can be saved and restored from memory block
 *
 *	This class ensures that
 *
 *	1)	All fields of a JitFrameContext object contain appropriate information
 *
 *	2)	All useful information from StackInfo object saves and restores 
 *		properly
 *
 *	The principle of the filling fields of a JitFrameContext object is a 
 *	hard-coded enumeration of registers.
 *
 *	The implementation of this class is located in file Ia32StackInfo.cpp
 *
 */
	
class StackInfo {
public:
	StackInfo(MemoryManager& mm) 
        : byteSize(0), hashTableSize(0), frameSize(0),
		itraceMethodExitString(0),
		eipOffset(0),
		icalleeMask(0),icalleeOffset(0),
		fcallee(0),foffset(0),
		acallee(0),aoffset(0),
		localOffset(0), 
		callerSaveRegsConstraint(OpndKind_Imm),
		calleeSaveRegsMask(0),
		stackDepth(-1),
		offsetOfThis(0){ stackDepthInfo = new(mm) DepthMap(mm);}

	StackInfo() 
		: byteSize(0), hashTableSize(0), frameSize(0),
		itraceMethodExitString(0),
		eipOffset(0),
		icalleeMask(0),icalleeOffset(0),
		fcallee(0),foffset(0),
		acallee(0),aoffset(0),
		localOffset(0), 
		stackDepthInfo(NULL),
		callerSaveRegsConstraint(OpndKind_Imm),
		calleeSaveRegsMask(0),
		stackDepth(-1),offsetOfThis(0) {}

	/** writes StackInfo data into memory
	 */
	void write(Byte* output);

	/** reads StackInfo data from MethodDesc
	 */
    void read(MethodDesc* pMethodDesc, uint32 eip, bool isFirst);

	/** Loads JitFrameContext with the next stack frame according to StackInfo
	 */
	void unwind(MethodDesc* pMethodDesc, JitFrameContext* context, bool isFirst) const;

	/**	Changes ESP to point to the start of stack frame
	 */
	void fixHandlerContext(JitFrameContext* context);

	/**	stores into StackInfo information about calls instructions such as 
	 *	eip, stack depth, caller-save and callee-save registers masks
	 *	The algorithm takes one-pass over CFG.
	 */
	void registerInsts(IRManager& irm);

	/** sets method exit string which was created during itrace pass 
		and is alive during runtime
	*/
	void setMethodExitString(IRManager& irm);

	/**	returns register offset for previos stack frame. 
		MUST be called before unwind()
	*/
	uint32 * getRegOffset(const JitFrameContext* context, RegName reg) const;

	/** returns stack depth
	*/
	uint32 getStackDepth() const {
		assert(stackDepth>=0);
		return stackDepth;
    }

    uint32 getFrameSize() const {return frameSize;}

	int getRetEIPOffset() const {return eipOffset;}
    
    uint32 getIntCalleeMask() const {return icalleeMask;}
    
    int getIntCalleeOffset() const {return icalleeOffset;}
    
    uint32 getFPCalleeMask() const {return fcallee;}
    
    int getFPCalleeOffset() const {return foffset;}

    uint32 getApplCalleeMask() const {return acallee;}
    
    int getApplCalleeOffset() const {return aoffset;}

	int getLocalOffset() const {return localOffset;}

    uint32 getOffsetOfThis() const {return offsetOfThis;}

	/** returns byte size of StackInfo data
	*/
	uint32 getByteSize() const;

    static uint32 getByteSize(MethodDesc* md);

    /** read byte size from info block
    *                                                                      
    */
    uint32 readByteSize(const Byte* input) const;

private:
    uint32 byteSize;
	uint32 hashTableSize;
    uint32	frameSize;
	
	const char * itraceMethodExitString;

    int		eipOffset;

    uint32	icalleeMask;
    int		icalleeOffset;

    uint32	fcallee;
    int		foffset;

    uint32	acallee;
    int		aoffset;

	int		localOffset;
	
	DepthMap * stackDepthInfo;

	Constraint callerSaveRegsConstraint;
	uint32 calleeSaveRegsMask;
	int stackDepth;
	uint32 offsetOfThis;

	friend class StackLayouter;
};

//========================================================================================
//	class StackInfoInstRegistrar
//========================================================================================
/**
 *	class StackInfoInstRegistrar performs calling of StackInfo method which
 *	performes saving information about call instructions for stack unwinding
 *
 *	This transformer ensures that it calls the proper method
 *
 *	This transformer must inserted after Code Emitter, because it needs
 *	EIP value for instructions.
 */

BEGIN_DECLARE_IRTRANSFORMER(StackInfoInstRegistrar, "si_insts", "StackInfo inst registrar")
	IRTRANSFORMER_CONSTRUCTOR(StackInfoInstRegistrar)
	void runImpl()
	{ 
		StackInfo * stackInfo = (StackInfo*)irManager.getInfo("stackInfo");
		assert(stackInfo!=NULL);
		stackInfo->registerInsts(irManager);
		stackInfo->setMethodExitString(irManager);
	}
	uint32 getNeedInfo()const{ return 0; }
	uint32 getSideEffects()const{ return 0; }
	bool isIRDumpEnabled(){ return false; }
END_DECLARE_IRTRANSFORMER(StackInfoInstRegistrar) 

}}//namespace
#endif /* _IA32_STACK_INFO_H_ */
