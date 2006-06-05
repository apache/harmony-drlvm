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
 * @version $Revision: 1.20.8.2.4.3 $
 */

#ifndef _IA32_IRMANAGER_H_
#define _IA32_IRMANAGER_H_

#include "open/types.h"
#include "Stl.h"
#include "MemoryManager.h"
#include "Type.h"
#include "CodeGenIntfc.h"
#include "Ia32Inst.h"
#include "Ia32CFG.h"
#include "BitSet.h"
#include "Timer.h"
#include "XTimer.h"
#include "Log.h"
#include "../../shared/PlatformDependant.h"
#include "CGSupport.h"
#include "CompilationContext.h"

namespace Jitrino
{
namespace Ia32{

const char * newString(MemoryManager& mm, const char * str, uint32 length=EmptyUint32);

//========================================================================================
// STL aux classes (need to be moved somewhere)
//========================================================================================
struct ConstCharStringLess
{	
	bool operator()(const char * l, const char * r) const
	{ return (l==0?(r==0?0:-1):r==0?1:strcmp(l,r))<0; }
};


//========================================================================================
// class IRManager
//========================================================================================
/**
	class IRManager is used to create elements of the LIR, to change the LIR's structure,
	and to access various information which is considered worth to be tracked LIR-wide
	
*/
class IRManager: public CFG
{
public:

	typedef StlMap<const char *, void *, ConstCharStringLess>	ConstCharStringToVoidPtrMap;

	struct InternalHelperInfo
	{
		const void * pfn;
		const CallingConvention * callingConvention;
		InternalHelperInfo(const void * _pfn=NULL, const CallingConvention * _callingConvention=NULL)
			:pfn(_pfn), callingConvention(_callingConvention){}
	};

	typedef StlMap<const char *, InternalHelperInfo, ConstCharStringLess>	InternalHelperInfos;

	//---------------------------------------------------------------------------------------
	/**	Creates IRManager */
	
	IRManager(MemoryManager& memManager, TypeManager& tm, MethodDesc& md, 
		CompilationInterface& compIface);

	//-------------------------------------------------------------------------------------

	/** Returns the type manager used for LIR */
	TypeManager& getTypeManager()const{ return typeManager; }

	/** Returns the MethodDesc for the method represented by this IR */
	MethodDesc& getMethodDesc()const{ return methodDesc; }

	/** Returns the CompilationInterface for this compilation session */
	CompilationInterface& getCompilationInterface()const{ return compilationInterface; }
    
    CompilationContext* getCompilationContext()const{ return compilationInterface.getCompilationContext();}
    CGFlags* getCGFlags() const {return getCompilationContext()->getIa32CGFlags();}

	//-----------------------------------------------------------------------------------------------
	void setInfo(const char * key, void * info)
	{ infoMap[newInternalString(key)]=info; }

	const void * getInfo(const char * key)
	{ ConstCharStringToVoidPtrMap::const_iterator it=infoMap.find(key); return it!=infoMap.end()?it->second:NULL; }

	//-------------------------------------------------------------------------------------
	struct AliasRelation
	{
		Opnd * outerOpnd;
		uint32 offset;
		AliasRelation(Opnd * oo =0, uint32 offs=0)
			:outerOpnd(oo), offset(offs){}
	};

	//-------------------------------------------------------------------------------------
	/** Creates a new unassigned operand (virtual register) of type ta*/
	Opnd * newOpnd(Type * type);
	/** Creates a new unassigned operand (virtual register) of type ta, with additional constraint c */
	Opnd * newOpnd(Type * type, Constraint c);

	/** Creates a new operand assigned with immediate value */
	Opnd * newImmOpnd(Type * type, int64 immediate);

	/** Creates a new immediate operand associated with 
	runtime info of the specified kind */
	Opnd * newImmOpnd(Type * type, Opnd::RuntimeInfo::Kind kind, void * arg0=0, void * arg1=0, void * arg2=0, void * arg3=0);

	ConstantAreaItem *	newConstantAreaItem(float f);
	ConstantAreaItem *	newConstantAreaItem(double d);
	ConstantAreaItem *	newSwitchTableConstantAreaItem(uint32 numTargets);
	ConstantAreaItem *	newInternalStringConstantAreaItem(const char * str);
	ConstantAreaItem *	newBinaryConstantAreaItem(uint32 size, const void * pv);

	Opnd * newFPConstantMemOpnd(float f);
	Opnd * newFPConstantMemOpnd(double f);
	Opnd * newSwitchTableConstantMemOpnd(uint32 numTargets, Opnd * index);

	Opnd * newInternalStringConstantImmOpnd(const char * str);
	Opnd * newBinaryConstantImmOpnd(uint32 size, const void * pv);

	/** Creates a new operand assigned to a physical register */
	Opnd * newRegOpnd(Type * type, RegName reg);

	/** Creates a new operand assigned to a memory location of kind k */
	Opnd * newMemOpnd(Type * type, MemOpndKind k, Opnd * base, Opnd * index=0, Opnd * scale=0, Opnd * displacement=0, RegName baseReg=RegName_Null);
	/** Shortcut: Creates a new operand assigned to a memory location of kind Heap */
	Opnd * newMemOpnd(Type * type, Opnd * base, Opnd * index=0, Opnd * scale=0, Opnd * displacement=0, RegName baseReg=RegName_Null);
	/** Shortcut: Creates a new operand assigned to a memory location of kind k */
	Opnd * newMemOpnd(Type * type, MemOpndKind k, Opnd * base, int32 displacement, RegName baseReg=RegName_Null);

	Opnd * newMemOpndAutoKind(Type * type, MemOpndKind k, Opnd * opnd0, Opnd * opnd1=0, Opnd * opnd2=0);
	Opnd * newMemOpndAutoKind(Type * type, Opnd * opnd0, Opnd * opnd1=0, Opnd * opnd2=0)
	{ return newMemOpndAutoKind(type, MemOpndKind_Heap, opnd0, opnd1, opnd2); }

	//-------------------------------------------------------------------------------------
	/** Creates a new Native Inst defined by mnemonic with up to 8 operands */
	Inst * newInst(Mnemonic mnemonic, Opnd * opnd0=0, Opnd * opnd1=0, Opnd * opnd2=0);

	Inst * newInst(Mnemonic mnemonic, 
		Opnd * opnd0, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, 
		Opnd * opnd4, Opnd * opnd5=0, Opnd * opnd6=0, Opnd * opnd7=0
		);

	/** Creates a new Extended Inst defined by mnemonic with up to 8 operands */
	Inst * newInstEx(Mnemonic mnemonic, uint32 defCount, Opnd * opnd0=0, Opnd * opnd1=0, Opnd * opnd2=0);

	Inst * newInstEx(Mnemonic mnemonic, uint32 defCount, 
		Opnd * opnd0, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, 
		Opnd * opnd4, Opnd * opnd5=0, Opnd * opnd6=0, Opnd * opnd7=0
		);

	/** Creates a new branch instruction
		The source of the targetMemOpnd can be defined by its RuntimInfo

		if the targetOpnd is null, implicit immediate operand is created and initialized to 0, 
		meaning that the branch is direct. Branch address will be updated during code emission
	*/
	BranchInst * newBranchInst(Mnemonic mnemonic, Opnd * targetOpnd=0); 

	SwitchInst * newSwitchInst(uint32 numTargets, Opnd * index);

	/** Creates a CallInst instance. */
	CallInst *	newCallInst(Opnd * targetOpnd, const CallingConvention * cc, 
		uint32 numArgs, Opnd ** args, Opnd * retOpnd, InlineInfo* ii = NULL);

	/** A specialization of the newCallInst to create a VM Helper CallInst. */
	CallInst *	newRuntimeHelperCallInst(CompilationInterface::RuntimeHelperId helperId, 
		uint32 numArgs, Opnd ** args, Opnd * retOpnd);
	/** A specialization of the newCallInst to create an internal helper CallInst. */
	CallInst *	newInternalRuntimeHelperCallInst(const char * internalHelperID, uint32 numArgs, Opnd ** args, Opnd * retOpnd);

	void registerInternalHelperInfo(const char * internalHelperID, const InternalHelperInfo& info);
	const InternalHelperInfo *	getInternalHelperInfo(const char * internalHelperID)const
	{ InternalHelperInfos::const_iterator it=internalHelperInfos.find(internalHelperID); return it!=internalHelperInfos.end()?&it->second:NULL; }

	const char * newInternalString(const char * originalString)const
	{ return newString(memoryManager, originalString); }

	AliasPseudoInst * newAliasPseudoInst(Opnd * targetOpnd, Opnd * sourceOpnd, uint32 offset);
	AliasPseudoInst * newAliasPseudoInst(Opnd * targetOpnd, uint32 sourceOpndCount, Opnd ** sourceOpnds);

	CatchPseudoInst * newCatchPseudoInst(Opnd * exception);

	Inst * newI8PseudoInst(Mnemonic mnemonic, uint32 defCount,
			Opnd * opnd0, Opnd * opnd1=0, Opnd * opnd2=0, Opnd * opnd3=0
		);

    GCInfoPseudoInst* newGCInfoPseudoInst(const StlVector<Opnd*>& basesAndMptr);

	SystemExceptionCheckPseudoInst* newSystemExceptionCheckPseudoInst(CompilationInterface::SystemExceptionId exceptionId, Opnd * opnd0, Opnd * opnd1=0, bool checksThisForInlinedMethod=false);

	Inst * newCopyPseudoInst(Mnemonic mn, Opnd * opnd0, Opnd * opnd1=NULL);

	/** Creates the ret instruction 
	using the primary calling convention associated with this method in IRManager's constructor
	*/
	RetInst * newRetInst(Opnd * retOpnd);

	/** Creates an EntryPointPseudoInst pseudo instruction representing the entry point of a method.
	*/
	EntryPointPseudoInst * newEntryPointPseudoInst(const CallingConvention * cc);

 	Inst * newCopySequence(Mnemonic mn, Opnd * opnd0, Opnd * opnd1=NULL, uint32 regUsageMask=(uint32)~0);

	//---------------------------------------------------------------------------------------
	void setHasCalls(){ hasCalls=true; }
	bool getHasCalls()const{ return hasCalls; }
	void setHasNonExceptionCalls(){ hasNonExceptionCalls=true; }
	bool getHasNonExceptionCalls()const{ return hasNonExceptionCalls; }
	//-----------------------------------------------------------------------------------------------
	const CallingConvention * getCallingConvention(CompilationInterface::RuntimeHelperId helperId)const;

	const CallingConvention * getCallingConvention(MethodDesc * methodDesc)const;

	const CallingConvention * getDefaultManagedCallingConvention() const { return &CallingConvention_DRL; }

	EntryPointPseudoInst * getEntryPointInst()const { return entryPointInst; }

	const CallingConvention * getCallingConvention()const { assert(NULL!=entryPointInst); return getEntryPointInst()->getCallingConventionClient().getCallingConvention(); }

	Opnd * defArg(Type * type, uint32 position);

	void applyCallingConventions();

	uint32 getMaxInstId() { return instId; }

	//-----------------------------------------------------------------------------------------------
	/** Updates operands resolving their RuntimeInfo */
    void resolveRuntimeInfo();
    void resolveRuntimeInfo(Opnd* opnd) const;


	/** AOT support: serializes inst into stream */
	Byte * serialize(Byte * stream, Inst * inst);
	/** AOT support: deserializes inst from stream */
	Byte * deserialize(Byte * stream, Inst *& inst);

	//-----------------------------------------------------------------------------------------------
	/** calculate liveness information */
	void calculateLivenessInfo();

	void fixLivenessInfo( uint32 * map = NULL );

    bool hasLivenessInfo()const
	{ 
		if (!_hasLivenessInfo)
			return false;
		LiveSet * ls = getPrologNode()->getLiveAtEntry(); 
		return ls != NULL && ls->getSetSize()==getOpndCount();
	}

    /** calculate liveness information if needed*/
    void updateLivenessInfo() { if (!hasLivenessInfo()) calculateLivenessInfo();}

	void invalidateLivenessInfo(){ _hasLivenessInfo=false; }


	/** returns the live set for node entry */
	LiveSet * getLiveAtEntry(const Node * node)const
	{ return node->getLiveAtEntry(); }
	/** initializes ls with liveness info at node exit */
	void getLiveAtExit(const Node * node, LiveSet & ls)const;
	/** use this function to update ls in a single bakrward pass through a node when liveness info is calculated */
	void updateLiveness(const Inst * inst, LiveSet & ls)const;

	/** returns the reg usage information for node entry 
	as uint32containing bit mask for used registers */
	uint32 getRegUsageAtEntry(const Node * node, OpndKind regKind)const
	{	return getRegUsageFromLiveSet(getLiveAtEntry(node), regKind);	}

	/** returns the reg usage information for node exit 
	as uint32 containing bit mask for used registers */
	void getRegUsageAtExit(const Node * node, OpndKind regKind, uint32 & mask)const;

    /** use this function to update reg usage in a single postorder pass when liveness info is calculated */
	void updateRegUsage(const Inst * inst, OpndKind regKind, uint32 & mask)const;

	uint32 getRegUsageFromLiveSet(LiveSet * ls, OpndKind regKind)const;

	/** returns Constraint containing bit mask for all registers used in the method */
	uint32 getTotalRegUsage(OpndKind regKind)const;

	void calculateTotalRegUsage(OpndKind regKind);

	static bool isGCSafePoint(const Inst* inst)  {return inst->getMnemonic() == Mnemonic_CALL;}

	static bool isThreadInterruptablePoint(const Inst* inst) {
        if ( inst->getMnemonic() == Mnemonic_CALL ) {
            Opnd::RuntimeInfo* ri = inst->getOpnd(((ControlTransferInst*)inst)->getTargetOpndIndex())->getRuntimeInfo();
            return ri ? ri->getKind() == Opnd::RuntimeInfo::Kind_HelperAddress : false;
        } else {
            return false;
        }
    }

	//-----------------------------------------------------------------------------------------------
	bool isOnlyPrologSuccessor(BasicBlock * bb) ;

	/** expands SystemExceptionCheckPseudoInst */
	void expandSystemExceptions(uint32 reservedForFlags);

	/** changes all Extended insts to Native form by calling makeInstNative */
	void translateToNativeForm();

	void eliminateSameOpndMoves();

	//-----------------------------------------------------------------------------------------------
	/**this function performs the following things in a single postorder pass:
		1) indexes instruction in topological (reverse post-order)
		2) (re-)calculates operand statistics (ref counts)
		3) If reindex is true it sets new sequential Opnd::id 
		for all operands with refcount>0(used for packOpnds)*/
	uint32 calculateOpndStatistics(bool reindex=false);

	/** this function performs the following:
		1) calls calculateOpndStatistics().
		2) removes operands with zero ref counts from the internal
		array of all operands accessed via IRManager::getOpnd(uint32).
	*/
	void packOpnds();

	/** returns the number of operands in the internal
		array of all operands accessed via IRManager::getOpnd(uint32).
		This number is affected by packOpnds which removes from the array all operands which
		are no longer in the LIR.
	*/
	uint32 getOpndCount()const
	{ return opnds.size(); }

	/** returns an operand from the internal array of all operands by its ID. */
	Opnd * getOpnd(uint32 id)const
	{ 
		assert(id<opnds.size()); return opnds[id]; 
	}

	/** sets the the Calculateable opnd constraints to Initial ones */
	void resetOpndConstraints();
	void addToOpndCalculatedConstraints(Constraint c);

	//-----------------------------------------------------------------------------------------------
	uint32 assignInnerMemOpnd(Opnd * outerOpnd, Opnd* innerOpnds, uint32 offset);
	void assignInnerMemOpnds(Opnd * outerOpnd, Opnd** innerOpnds, uint32 innerOpndCount);
	void layoutAliasPseudoInstOpnds(AliasPseudoInst * inst);
	void getAliasRelations(AliasRelation * relations);
	void layoutAliasOpnds();
	uint32 getLayoutOpndAlignment(Opnd * opnd);
	void finalizeCallSites();
	void removeRedundantStackOperations();

	/** Calculates dislacement from stack entry point
		for every instruction.
	*/
	void calculateStackDepth();
	//-----------------------------------------------------------------------------------------------
	bool verify();
	bool verifyLiveness();
	bool verifyHeapAddressTypes();

	//-----------------------------------------------------------------------------------------------
	/** Assigns all instructions in the CFG sequential indexes ordering them by order 
		The index can be obtained using Inst::getIndex()
	*/
	void indexInsts(OrderType order);

	//-----------------------------------------------------------------------------------------------
	/** space optimization: return a pre-existent operand representing physical register regName */
	Opnd * getRegOpnd(RegName regName);
	bool isPreallocatedRegOpnd(Opnd * opnd);
	//-----------------------------------------------------------------------------------------------
	/** returns the initial constraint for the given type */
	Constraint getInitialConstraint(Type * type)const
	{ return initialConstraints[type->tag]; }

	/** converts Type::Tag into pointer to Type instance */
	Type * getTypeFromTag(Type::Tag)const;

	Type * getManagedPtrType(Type * sourceType);

	/** returns the size of location occupied by an operand of type type */
	static OpndSize getTypeSize(Type::Tag tag);
	static OpndSize getTypeSize(Type * type){ return getTypeSize(type->tag); }

    /** checks loop info, returns FALSE if loop info is not valid,
        should be used in debug assert checks*/
    bool ensureLivenessInfoIsValid();

	/** returns true if regs of kind regKind cannot be assigned anymore to operands
	This means reg allocations is done and the new assignment can conflict with existing ones
	*/
	bool isRegisterSetLocked(OpndKind regKind){ return true; } 
	void lockRegisterSet(OpndKind regKind){ } 

	void setVerificationLevel(uint32 v){ verificationLevel=v; }
	uint32 getVerificationLevel()const{ return verificationLevel; }


	static void selfUnitTest(TypeManager & tm, MethodDesc & md, CompilationInterface & compInterface);

protected:

	void addOpnd(Opnd * opnd);

	void addAliasRelation(AliasRelation * relations, Opnd * opndOuter, Opnd * opndInner, uint32 offset);

 	Inst * newCopySequence(Opnd * targetOpnd, Opnd * sourceOpnd, uint32 regUsageMask=(uint32)~0);
 	Inst * newPushPopSequence(Mnemonic mn, Opnd * opnd, uint32 regUsageMask=(uint32)~0);
 	Inst * newMemMovSequence(Opnd * targetOpnd, Opnd * sourceOpnd, uint32 regUsageMask, bool checkSource=false);

	void initInitialConstraints();
	Constraint createInitialConstraint(Type::Tag t)const;

	//-------------------------------------------------------------------------------------
	TypeManager				&		typeManager;

	MethodDesc				&		methodDesc;
	CompilationInterface	&		compilationInterface;

	uint32							opndId;
	uint32							instId;

	OpndVector						opnds;

	uint32							gpTotalRegUsage;

	EntryPointPseudoInst *			entryPointInst;

	Opnd *							regOpnds[IRMaxRegNames];

	bool							_hasLivenessInfo;

	InternalHelperInfos				internalHelperInfos;

	ConstCharStringToVoidPtrMap		infoMap;

	uint32							verificationLevel;

	bool							hasCalls;
	bool							hasNonExceptionCalls;

	Constraint						initialConstraints[Type::NumTypeTags];
};

#define VERIFY_OUT(s) { if (Log::cat_cg()->isFailEnabled()) Log::out() << s; ::std::cerr << s; }

/** MapHandler is auxilary class to eliminate direct usage of map handlers between HLO and codegenerator */

class MapHandler {
public:
    MapHandler(const char* handlerName, MethodDesc* meth) {
        handler = getContainerHandler(handlerName, meth);
    }
    uint64 getMapEntry(uint64 key) {
        return ::Jitrino::getMapEntry(handler, key);
    }
    void setMapEntry(uint64 key, uint64 value) {
        ::Jitrino::setMapEntry(handler, key, value);
    }
    void removeMapEntry(uint64 key) {
        ::Jitrino::removeMapEntry(handler, key);
    }
private:
    void* handler;
};

class VectorHandler {
public:
    VectorHandler(const char* handlerName, MethodDesc* meth) {
        handler = getContainerHandler(handlerName, meth);
    }
    uint64 getVectorEntry(uint64 key) {
        return ::Jitrino::getVectorEntry(handler, key);
    }
    void setVectorEntry(uint64 key, uint64 value) {
        ::Jitrino::setVectorEntry(handler, key, value);
    }
    void removeVectorEntry(uint64 key) {
        ::Jitrino::removeVectorEntry(handler, key);
    }
private:
    void* handler;
};



//========================================================================================
// class IRTransformer
//========================================================================================
/** 
	class IRTransformer is the base class for all IR transformations external to IRManager:
	optimizations, etc.
*/

class IRTransformer
{
	//=============================================================================
public:
	typedef IRTransformer * (*CreateFunction)(IRManager& irm, const char * params);
	const static uint32 MaxIRTransformers=100;
	
	const static uint32 MaxTagLength=31;

	struct IRTransformerRecord
	{
		const char *	tag;
		CreateFunction	createFunction;

		XTimer xtimer;

		IRTransformerRecord(const char * _tag=NULL, CreateFunction _createFunction=NULL)
			:tag(_tag), createFunction(_createFunction)
		{
		}
	};

	static uint32 registerIRTransformer(const char * tag, CreateFunction pfn);

	static IRTransformerRecord * findIRTransformer(const char * tag);

	static IRTransformer *	newIRTransformer(const char * tag, IRManager& irm, const char * params);

private:

	static IRTransformerRecord table[];
	static uint32 tableLength;

	//=============================================================================
public:
	enum SideEffect
	{
		SideEffect_InvalidatesLivenessInfo=0x1,
		SideEffect_InvalidatesLoopInfo=0x2,
	};

	enum NeedInfo
	{
		NeedInfo_LivenessInfo=0x1,
		NeedInfo_LoopInfo=0x2,
	};

    IRTransformer(IRManager & irm, const char * params=0) 
		: irManager(irm), parameters(params), stageId(0), record(NULL){}
	virtual ~IRTransformer(){};
	virtual void destroy(){};

    void run();
    
    IRManager& getIRManager()const{ return irManager; }

	const char *	getParameters()const {return parameters; }

	/** Implementors override getName() to return the full 
	printable name of the pass. 
	
	Required */
	virtual const char * getName()=0;
	
	/** Implementors override getTagName() to return the short tag
	name of the pass (used in log file names). 
	
	Required */
	virtual const char * getTagName()=0;
	
protected:
	/** Implementors override runImpl() to perform all operations of the pass

	Required */
    virtual void runImpl()=0;

	/** Implementors override getNeedInfo() to indicate what info 
	must be up to date for the pass.

	The returned value is |-ed from the NeedInfo enum

	Optional, defaults to all possible info */
	virtual uint32 getNeedInfo()const;

	/** Implementors override getSideEffects() to indicate the side effect 
	of the pass (currently in terms of common info like liveness, loop, etc.)
	
	The returned value is |-ed from the SideEffect enum

	Optional, defaults to all possible side effects */
	virtual uint32 getSideEffects()const;

	virtual bool verify(bool force=false);

	virtual void debugOutput(const char * subKind);
	
	virtual void dumpIR(const char * subKind1, const char * subKind2=0);
	virtual void printDot(const char * subKind1, const char * subKind2=0);

	void printIRDumpBegin(const char * subKind);
	void printIRDumpEnd(const char * subKind);

	virtual bool isIRDumpEnabled(){ return true; }
protected:

	IRManager &		irManager;
	const char *	parameters;

	uint32		stageId;

private:
	IRTransformerRecord * record;

friend struct IRTimers;

};


//========================================================================================
#define IRTRANSFORMER_CONSTRUCTOR(cls) \
	cls(IRManager& irm, const char * params=0): IRTransformer(irm, params){} \



#define BEGIN_DECLARE_IRTRANSFORMER(cls, tag, name) \
class cls: public IRTransformer \
{ \
public:	\
	static const char * getMyTag(){ return tag; } \
	static cls * create(IRManager & irm, const char * params){ return new cls(irm, params); } \
	void  destroy(){ delete this; } \
	const char * getName(){ return name; } \
	const char * getTagName(){ return getMyTag(); } \




#ifdef IRTRANSFORMER_REGISTRATION_ON

IRTransformer::IRTransformerRecord IRTransformer::table[IRTransformer::MaxIRTransformers]={};
uint32 IRTransformer::tableLength=0;

#define REGISTER_IRTRANSFORMER(cls) \
uint32 _register_##cls = IRTransformer::registerIRTransformer(cls::getMyTag(), (IRTransformer::CreateFunction)&cls::create);

#else

#define REGISTER_IRTRANSFORMER(cls) 

#endif

#define END_DECLARE_IRTRANSFORMER(cls) \
}; \
REGISTER_IRTRANSFORMER(cls)



#define DECLARE_IRTRANSFORMER(cls, tag, name) \
	BEGIN_DECLARE_IRTRANSFORMER(cls, tag, name) \
		IRTRANSFORMER_CONSTRUCTOR(cls) \
		void runImpl(); \
	END_DECLARE_IRTRANSFORMER(cls)



}}; // namespace Ia32

#endif
