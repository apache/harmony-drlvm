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
 * @version $Revision: 1.18.12.2.4.3 $
 */

#ifndef _IA32_INST_H_
#define _IA32_INST_H_

#include "open/types.h"
#include "Stl.h"
#include "MemoryManager.h"
#include "Type.h"
#include "CodeGenIntfc.h"
#include "MemoryAttribute.h"


#include "Ia32Encoder.h"
#include "Ia32CallingConvention.h"
namespace Jitrino
{
namespace Ia32{

//=========================================================================================================
class CodeEmitter;
class CfgNode;
class BasicBlock;
class IRManager;
class Opnd;
class Inst;

//=========================================================================================================
//		class Opnd
//=========================================================================================================
/** 
class Opnd represents an operand in the LIR.

Instructions contain pointers to Opnd instances for their operands.
For example the following LIR pseudo-code:
	I0: mov t1, t0
	I1: add t1, t3

instructions I0 and I1 will contain pointers (at index 0) to the same Opnd instance with id==1

Operands behavior is managed by an array of 3 constraints: 
		ConstraintKind_Initial,
		ConstraintKind_Calculated,
		ConstraintKind_Location,

When an operand is created it is assigned ConstraintKind_Initial for the rest of its live
At opnd creation time ConstraintKind_Calculated is also set to ConstraintKind_Initial
When the operand is assigned to a specific physical location (Imm, Reg, Mem), its Location constraint
is set appropriately (representing the assigned location). 

The Calculated constraint is calculated during special passes 
taking into account all instruction irregularities
The Initial constraint always contains the Calculated constraint and 
the Calculated constraint contains the Location one. 

*/

class Opnd: public CG_OpndHandle
{
public:

	/** enum Scope defines properties of the set of definitions of an operand  */
	enum DefScope{
		DefScope_Null=0,
		/** The operand has single definition */
		DefScope_Temporary,
		/** The operand has multiple defs all withing one basic block (occures after convertion into 2-operand form) */
		DefScope_SemiTemporary,
		/** The operand has merging defs */
		DefScope_Variable,
	};

	/** enum ConstraintKind is used to indicate a particular constraint of an operand 
	Used in getConstraint()
	*/
	enum ConstraintKind{
		/** The outer-most constraint assigned during Opnd creation basing on Opnd size*/
		ConstraintKind_Size=-1,
		/** An additional constraint assigned during Opnd creation */
		ConstraintKind_Initial=0,
		/** A constraint calculated during special pass from instruction properties */
		ConstraintKind_Calculated=1,
		/** A constraint defining assigned physical location of an operand */
		ConstraintKind_Location=2,
		/** The current constraint. If an operand is not assigned with physical storage,
		getConstraint will return the Calculated constraint, 
		otherwise, it will return the Location constraint
		*/
		ConstraintKind_Current
	};

	//-------------------------------------------------------------------------
	/** class RuntimeInfo contains information allowing CG to determine operand value from the current runtime information
	Initially added to support AOT compiler the class is used to annotate operands with runtime info
	*/
	class RuntimeInfo
	{
	public:
		enum Kind{
			Kind_Null=0,
			/** The value of the operand is [0]->ObjectType::getAllocationHandle() */
			Kind_AllocationHandle,		
			/**	The value of the operand is [0]->NamedType::getRuntimeIdentifier() */
			Kind_TypeRuntimeId,		
			/**	The value of the operand is [0]->NamedType::getRuntimeIdentifier() */
			Kind_MethodRuntimeId,		
			/** The value of the operand is [1], but the information can be used to serialize/deserizalize 
			this value: [0] - Type * - the containing class, [1] - string token */
			Kind_StringDescription,	
			/** The value of the operand is [0]->ObjectType::getObjectSize() */
			Kind_Size,					
			/** The value of the operand is compilationInterface->getRuntimeHelperAddress([0]) */
			Kind_HelperAddress,
			/** The value of the operand is irManager.getInternalHelperInfo((const char*)[0]).pfn */
			Kind_InternalHelperAddress,
			/** The value of the operand is [0]->FieldDesc::getAddress() */
			Kind_StaticFieldAddress,
			/** The value of the operand is [0]->FieldDesc::getOffset() */
			Kind_FieldOffset,
			/** The value of the operand is compilationInterface.getVTableOffset(), zero args */
			Kind_VTableAddrOffset,
			/** The value of the operand is [0]->ObjectType::getVTable() */
			Kind_VTableConstantAddr,
			/** The value of the operand is [0]->MethodDesc::getOffset() */
			Kind_MethodVtableSlotOffset,
			/** The value of the operand is [0]->MethodDesc::getIndirectAddress() */
			Kind_MethodIndirectAddr,
			/** The value of the operand is *[0]->MethodDesc::getIndirectAddress() */
			Kind_MethodDirectAddr,
	
			/** The value of the operand is address of constant pool item  ((ConstantPoolItem*)[0])->getAddress() */
			Kind_ConstantAreaItem=0x80,

			/** more ... */
		};

		/** Constructs a RuntimeInfo instance of RuntimeInfo::Type t and initialize it with given values */
		RuntimeInfo(RuntimeInfo::Kind k, void * value0, void * value1=0, void * value2=0, void * value3=0, uint32 addOffset=0)
			:kind(k), additionalOffset(addOffset)
			{ value[0]=value0; value[1]=value1; value[2]=value2; value[3]=value3; }

		/** Returns the the value at index i */
		void * getValue(uint32 i)const{ assert(i<sizeof(value)/sizeof(value[0])); return value[i]; }

		uint32 getAdditionalOffset()const{ return additionalOffset; }

		/** Returns the type of the info (not to be confused with ::Type)*/
		RuntimeInfo::Kind getKind()const { return kind; }
	private:
		RuntimeInfo::Kind kind;
		void * value[4];
		uint32 additionalOffset;
	};

public:

	/** returns the ID of the operand */
	uint32		getId()const{ return id; }

	/** returns the ID of the operand assigned at its creation*/
	uint32		getFirstId()const{ return firstId; }

	/** returns the type of the operand */
	Type * getType()const{ return type; }

	/** returns the constraint of the specified kind sk */
	Constraint Opnd::getConstraint(ConstraintKind ck)const
	{
		if (ck==ConstraintKind_Current)
			return constraints[ConstraintKind_Location].isNull()?constraints[ConstraintKind_Calculated]:constraints[ConstraintKind_Location];
		return ck==ConstraintKind_Size?Constraint(OpndKind_Any, constraints[ConstraintKind_Initial].getSize()):constraints[ck];
	}

	Constraint Opnd::getConstraint(ConstraintKind ck, OpndSize size)const
	{	Constraint c=getConstraint(ck); return size==OpndSize_Any?c:c.getAliasConstraint(size); }

    /** returns true if the operand CAN BE assigned to a location defined by constraint
	*/
	bool canBePlacedIn(Constraint c)const
		{ return !(getConstraint(ConstraintKind_Calculated, c.getSize())&c).isNull(); }
	inline bool canBePlacedIn(OpndKind opndKind)const
	{ return ((uint32)opndKind & constraints[ConstraintKind_Calculated].getKind()) != 0; }

	/** returns the physical register assigned to the operand */
	RegName	getRegName()const{ return isPlacedIn(OpndKind_Reg)?regName:RegName_Null; }

	/** returns the immediate value assigned to the operand */
	int64				getImmValue()const{ return isPlacedIn(OpndKind_Imm)?immValue:0; }

	/** returns a sub-operand of a memory operand or NULL 	*/
	Opnd *	getMemOpndSubOpnd(MemOpndSubOpndKind so)const
	{ assert(memOpndKind != MemOpndKind_Null); return memOpndSubOpnds[so]; }
	Opnd * const * getMemOpndSubOpnds()const { return memOpndSubOpnds; }

	/** returns the memory kind of the operand 
	if it has been assigned to a memory location or Null otherwise */
	MemOpndKind getMemOpndKind()const{ return memOpndKind; }

	void setMemOpndKind(MemOpndKind k){ memOpndKind=k; }

	/** returns true if the operand IS assigned to a location defined by constraint
	constraint can be either explicitly created or
	implicitly created from OpndKind or RegName values
	*/
	inline bool isPlacedIn(Constraint c)const
	{ Constraint cl=getConstraint(ConstraintKind_Location, c.getSize()); return !cl.isNull() && c.contains(cl); }

	inline bool isPlacedIn(OpndKind opndKind)const
	{ return ( (uint32)opndKind & constraints[ConstraintKind_Location].getKind() ) != 0; }

	bool hasAssignedPhysicalLocation()const
	{ return !constraints[ConstraintKind_Location].isNull(); }

	OpndSize getSize()const
	{ OpndSize sz=constraints[ConstraintKind_Initial].getSize(); assert(sz!=OpndSize_Null && sz!=OpndSize_Any); return sz; }

	/** returns true if the operand can be allocated on register kind described by constraint c */
	bool isAllocationCandidate(Constraint c)const
	{ return canBePlacedIn((OpndKind)c.getKind()) && constraints[ConstraintKind_Location].isNull(); }

	/** returns the RuntimeInfo associated with the operand or NULL */
	RuntimeInfo * getRuntimeInfo()const 
	{ return runtimeInfo; }
	/** associates a RuntimeInfo with the operand */
	void setRuntimeInfo(RuntimeInfo * ri)
	{ assert(isPlacedIn(OpndKind_Imm)); runtimeInfo=ri; }

	/** assigns immediate value to the operand */
	void assignImmValue(int64 v);
	/** assigns physical register to the operand */
	void assignRegName(RegName r);
	/** assigns a memory location to the operand */
	void assignMemLocation(MemOpndKind k, Opnd * _base, Opnd * _index=0, Opnd * _scale=0, Opnd * _displacement=0);

	/** Changes sub-operands of a memory opnd (should be already assigned to memory location) */ 
	void setMemOpndSubOpnd(MemOpndSubOpndKind so, Opnd * opnd);

	/** returns sub-operand constraint for the sub-operand defined by so
	according to the Current operand constraints
	*/
	Constraint getMemOpndSubOpndConstraint(MemOpndSubOpndKind so)
	{ return Encoder::getMemOpndSubOpndConstraint(Constraint(), so); }

	bool replaceMemOpndSubOpnd(Opnd * opndOld, Opnd * opndNew);
	bool replaceMemOpndSubOpnds(Opnd * const * opndMap);

	DefScope getDefScope()const{ return defScope; }
	
	uint32 getRefCount()const{ return refCount; }

	void setCalculatedConstraint(Constraint c);

	Inst * getDefiningInst()const{ return definingInst; }

	bool isSubjectForLivenessAnalysis()const
	{
		return (memOpndKind&(MemOpndKind_StackManualLayout|MemOpndKind_ConstantArea|MemOpndKind_Heap))==0 && !isPlacedIn(OpndKind_Imm);
	}

        RegName getBaseReg() const     { return baseReg; }
        void    setBaseReg(RegName br) { baseReg = br; }

protected:
	void addRefCount(uint32& index, uint32 blockExecCount);

	void setDefiningInst(Inst * inst);

#ifdef _DEBUG
	void checkConstraints();
#else
	void checkConstraints(){}
#endif
private:

	//-------------------------------------------------------------------------
	Opnd::Opnd(uint32 _id, Type * t, Constraint c)
		:id(_id), firstId(_id), type(t), memOpndKind(MemOpndKind_Null), baseReg(RegName_Null)
		{ constraints[ConstraintKind_Initial]=constraints[ConstraintKind_Calculated]=c; }

	//-------------------------------------------------------------------------
	uint32			id;
	uint32			firstId;
	Type	 *		type;
	MemOpndKind		memOpndKind;
	Constraint		constraints[ConstraintKind_Current];

	DefScope		defScope;
	Inst *			definingInst;
	uint32			refCount;
        RegName                 baseReg;

	union{
		RegName		regName;
		struct{
			int64		immValue;
			RuntimeInfo	*	runtimeInfo;
		};
		Opnd *		memOpndSubOpnds[MemOpndSubOpndKind_Count];
	};

	//-------------------------------------------------------------------------
	friend class IRManager;
	friend class Inst;
};

typedef Opnd *		POpnd;
typedef StlVector<Opnd*> OpndVector;

//=========================================================================================================
//   class Inst
//=========================================================================================================
/**

class Inst represents an instruction of the LIR.

Each instruction contains an array of its explicit operands (pointers to Opnd instances).

Each instruction contains a pointer to the basic block it is attached to.

Each instruction has an ID unique in the method it belongs to.

Each instruction can be assigned a sequential index using IRManager::indexInsts() to order instruction
in a particular order.

Inst provides Opnds collection allowing it to iterate over all the operands the instruction 
uses or defines explicitly or implicitly.

Inst also provides an interface to operand constraints (weak and strong constraints). 
Initially operand constraints are defined for each occurence of an operand in an instruction,
but can be calculated for operands (using all their occurences) in a separate pass

*/

class Inst: protected Dlink
{

public:

	//---------------------------------------------------------------
	/** enum Kind represents dynamic type info of Inst and descendants.
	This enumeration is hierarchical and is used in getKind and hasKind Inst methods.
	*/
	enum Kind
	{
		Kind_Inst=0x7fffffff,
		Kind_PseudoInst=0x7f000000,
			Kind_EntryPointPseudoInst=0x40000000,
			Kind_AliasPseudoInst=0x20000000,
			Kind_CatchPseudoInst=0x10000000,
			Kind_CopyPseudoInst=0x01000000,
			Kind_I8PseudoInst=0x02000000,
            Kind_GCInfoPseudoInst=0x04000000,
			Kind_SystemExceptionCheckPseudoInst=0x08000000,
		Kind_ControlTransferInst=0x0000fff0,
			Kind_BranchInst=0x000003f0,
			Kind_SwitchInst=0x00000300,
			Kind_InterProceduralControlTransferInst=0x0000fc00,
				Kind_CallInst=0x00000f000,
				Kind_RetInst=0x000000c00,
	};

	/** misc properties of an instruction */
	enum Properties{
		/** The operation of the instruction is commutative regarding its uses */
		Properties_Symmetric=0x10,
		Properties_Conditional=0x20,
	};

	/** enum OpndRole defines the role of an operand in an instruction 
	The structure of the enumeration is filter-like allowing to combine its values with '|'
	*/
	enum OpndRole
	{
		OpndRole_Null=0,
		/** Instruction uses this operand */
		OpndRole_Use=0x1,
		/** Instruction defines this operand */
		OpndRole_Def=0x2,
		/** both uses and defs */
		OpndRole_UseDef=OpndRole_Use|OpndRole_Def,
		/** Roles set by Encoder */
		OpndRole_FromEncoder=OpndRole_UseDef,

		/** Explicit operand, must be explicitly provided into newInstruction 
			Such operands can be replaced in instructions and can be virtual registers
		*/
		OpndRole_Explicit=0x10,

		/** Auxilary operand, 
			These operands can also be replaced in instructions and can be virtual registers
			Examples are:
			return values, arguments of a call instruction, and so on
		*/ 
		OpndRole_Auxilary=0x20,

		/** Operands which can be set or replaced directly in an instruction */
		OpndRole_Changeable=OpndRole_Explicit|OpndRole_Auxilary,

		/** Implicit operand, must not be explicitly provided into newInstruction
			These operands are assigned with location from the moment of creation,
			cannot be replaced in instructions, and fully defined by the semantics of an instruction.
			Examples are:
			EFLAG, and affected registers of a call instructions

		*/
		OpndRole_Implicit=0x40,

		/** Operands contained directly in the instruction 
		(as oppsed to sub-operands of the InstLevel operands) 
		
		InstLevel operands can be read directly from the instruction using
		common indexing space
		*/
		OpndRole_InstLevel=OpndRole_Changeable|OpndRole_Implicit,

		/** Sub-operand of an instruction memory operand (base, index, scale, displacement) */
		OpndRole_MemOpndSubOpnd=0x80,
		/** sub-operands of the InstLevel operands */
		OpndRole_OpndLevel=OpndRole_MemOpndSubOpnd,

		OpndRole_ForIterator=OpndRole_InstLevel|OpndRole_OpndLevel,

		OpndRole_All=0xff,
		OpndRole_AllDefs=OpndRole_ForIterator|OpndRole_Def,
		OpndRole_AllUses=OpndRole_ForIterator|OpndRole_Use,
	};

	/** enum Form represents the form of an instruction 
	Inst can be in either Extended ("3-address") form or Native ("2-address") form 
	*/
	enum Form
	{
		/** Instructions's operands are in the "2-address" native form, e.g. add t0, t1 */
		Form_Native,
		/** Instructions's operands are in the "3-address" extended form, e.g. t2=add t0, t1 */
		Form_Extended,
	};

	/** Constraint kind for an operand of an instruction */
	enum ConstraintKind
	{
		ConstraintKind_Weak=0, 
		ConstraintKind_Strong=1,
		ConstraintKind_Current=2
	};

	//---------------------------------------------------------------
public:
	/** returns the kind of the instruction representing its class */
	Kind getKind()const{ return kind; }
	/** returns true if the instruction is of kind (class) k or its subclass */
	bool hasKind(Kind k)const{ return (kind&k)==kind; }

	/** returns the id of the instruction */
	uint32		getId()const{ return id; }

	/** returns the current form of the instruction */
	Form		getForm()const{ return (Form)form; }

	/** returns the stack depth of the instruction */
	uint32		getStackDepth() const { return stackDepth; }

	/** returns the mnemonic of the instruction */
	Mnemonic	getMnemonic()const{ return mnemonic; }

	/** Returns opcode group description associated with this instruction */
	const Encoder::OpcodeGroupDescription * getOpcodeGroupDescription()const
	{ assert(opcodeGroupDescription); return opcodeGroupDescription; }


	/** Shortcut: returns the properties of the instruction (bit-mask of Properties) */
	uint32 getProperties()const
	{ return properties; }

	/** returns the sequential index of the instruction after ordering via IRManager::indexInsts */
	uint32		getIndex()const{ return index; }

	/** returns the basic block this instruction is appended to or NULL */
	BasicBlock * getBasicBlock()const{ return basicBlock; }

	/** Direct access to InstLevel operands
	returns the number of InstLevel operands containing directly in the instruction 
	of the given role combination
	*/
	uint32		getOpndCount()const{ return opndCount; }
	uint32		getOpndCount(uint32 roles)const
	{ return 
		roles == (OpndRole_InstLevel|OpndRole_UseDef) ? opndCount:
		roles == (OpndRole_InstLevel|OpndRole_Def) ? defOpndCount:
		roles == (OpndRole_InstLevel|OpndRole_Use) && (Form)form == Form_Extended ? opndCount - defOpndCount:
		countOpnds(roles);
	}

	/** Direct access to InstLevel operands including OpndLevel.
	The indexing space is common for all operands.
	Inst::Opnds::iterator can also be used as index in this method.
	*/
	Opnd * getOpnd(uint32 index)const
	{	
		Opnd * const * opnds = getOpnds();
		if (index < opndCount)
			return opnds[index];
		uint32 diffIndex = index - opndCount;
		Opnd * instOpnd = *(Opnd * const *)(((Byte*)opnds) + (diffIndex & ~3));
		return instOpnd->getMemOpndSubOpnd((MemOpndSubOpndKind)(diffIndex & 3));
	}

	uint32 getInstLevelIndex(uint32 fullIndex)const
	{ return (fullIndex - opndCount) & ~3; }

	/** returns a mask describing operand roles (|-ed from OpndRole values) 
	for an InstLevel operand at idx 
	the indexing space is common for all operands including OpndLevel
	*/
	uint32  getOpndRoles(uint32 index) const
	{ return index < opndCount ? getOpndRoles()[index] : OpndRole_OpndLevel|OpndRole_Use;	}

	Opnd * const * getOpnds()const				{ return opnds; }
	const uint32 * getOpndRoles()const			
	{ uint32 aoc = allocatedOpndCount; return (const uint32*)(opnds + aoc); }
	const Constraint * getConstraints()const	
	{ uint32 aoc = allocatedOpndCount; return (const Constraint*)((const uint32*)(opnds + aoc) + aoc); }

	/** returns a constraint for an InstLevel operand at idx 
	the indexing space is common for all operands including OpndLevel
	*/
	Constraint getConstraint(ConstraintKind ck, uint32 idx, OpndSize size=OpndSize_Null)const;

	bool isLiveRangeStart(uint32 idx)const
	{ return (getOpndRoles(idx) & Inst::OpndRole_Use) != 0 && getOpnd(idx)->isSubjectForLivenessAnalysis(); }

	bool isLiveRangeEnd(uint32 idx)const
	{ return (getOpndRoles(idx) & Inst::OpndRole_UseDef) == Inst::OpndRole_Def && (getProperties() & Inst::Properties_Conditional)==0; }

	/** Direct access to operands
	Sets operand containing directly in the instruction at idx 
	the indexing space is common for all InstLevel operands
	*/
	void setOpnd(uint32 idx, Opnd * opnd);

	void insertOpnd(uint32 idx, Opnd * opnd, uint32 opndRoles);

	void setConstraint(uint32 idx, Constraint c)
	{ assert( (getOpndRoles()[idx] & OpndRole_Explicit) == 0 ); getConstraints()[idx] = c; }

	/** replaces all occurences of opndOld with roles matching opndRoleMask
	to opndNew */
	bool replaceOpnd(Opnd * opndOld, Opnd * opndNew, uint32 opndRoleMask=OpndRole_All);

	/** replaces all occurences of operands with roles matching opndRoleMask
		which has an entry in opndMap to the operand from that entry

		The opndMap map is organized as an array indexed by from-operand ID which contains to-operands.
		The number of entries in the array must be no less than the value returned by IRManager::getOpndCount()
	*/
	bool replaceOpnds(Opnd * const * opndMap, uint32 opndRoleMask=OpndRole_All);

	/** returns true if the instruction has side effect not described by its operands */
	virtual bool hasSideEffect()const
	{ return false; }

	/** emits (encodes) the instruction into stream */
	uint8 * emit(uint8* stream)const;

	void initFindInfo(Encoder::FindInfo& fi, Opnd::ConstraintKind opndConstraintKind)const;

	/** Shortcut to get the next instruction in an Inst list */
	Inst * getNext()const{ return (Inst*)_next; }
	/** Shortcut to get the prev instruction in an Inst list */
	Inst * getPrev()const{ return (Inst*)_prev; }
	
	/** swaps inst's operands at idx0 and idx1  */
	void swapOperands(uint32 idx0, uint32 idx1);

	/** changes instruction form to native
	and makes all necessary changes in instruction operands
	*/	
	void makeNative(IRManager * irManager);

	/** changes condition for a conditional instruction (SETcc, MOVcc, Jcc)
	Conditional instruction has Properies_Conditional
	*/
	void changeInstCondition(ConditionMnemonic cc, IRManager * irManager);

	/* Reverses condition of a conditional inst and updates its opcode group appropriately */
	virtual void reverse(IRManager * irManager);

	/* Returns true if the condition of a conditional inst can be reverted */
	virtual bool canReverse()const
	{ return getProperties()&&Properties_Conditional; }

	/** sets the offset of native code for this instruction */
	void			setCodeOffset(uint32 offset) {codeOffset = offset;}
	/** returns the offset of native code for this instruction */
	uint32			getCodeOffset()const	{	return codeOffset;	}
	/** sets the size of native code for this instruction */
	void			setCodeSize(uint32 size) {codeSize = size;}
	/** returns the size of native code for this instruction */
	uint32			getCodeSize()const	{	return codeSize;	}

	/** returns the pointer to the native code for this instruction */
	void *			getCodeStartAddr()const;

	class Opnds;
protected:
	//---------------------------------------------------------------

	static inline void* operator new(size_t sz, MemoryManager& mm, uint32 opndCount) 
	{	
		Inst * p = (Inst*)mm.alloc(sz + Inst::getOpndChunkSize(opndCount)); 
		p->allocatedOpndCount = opndCount; p->opnds = (Opnd**)((uint8*)p + sz);
		return p;
	}
	static inline void operator delete(void * p, MemoryManager& mm, uint32 opndCount) {}
	static inline void operator delete(void * p) {}

	Inst(Mnemonic m, uint32 _id, Form f)
		:kind(Kind_Inst), id(_id), mnemonic(m), 
		form(f), reservedFlags(0), 
		opcodeGroupDescription(0), basicBlock(0), defOpndCount(0), opndCount(0)
	{}
	virtual ~Inst(){};

	static uint32 getOpndChunkSize(uint32 opndCount){ return opndCount * (sizeof(Opnd*) + sizeof(uint32) * 2); }


	Opnd ** getOpnds()				{ return opnds; }
	uint32 * getOpndRoles()			
	{ uint32 aoc = allocatedOpndCount; return (uint32*)(opnds + aoc); }
	Constraint * getConstraints()	
	{ uint32 aoc = allocatedOpndCount; return (Constraint*)((const uint32*)(opnds + aoc) + aoc); }
	static uint32 getExplicitOpndIndexFromOpndRoles(uint32 roles)
	{ return roles>>16; }


	uint32 countOpnds(uint32 roles)const;

	void fixOpndsForOpcodeGroup(IRManager * irManager);
	void assignOpcodeGroup(IRManager * irManager);

	void setStackDepth(const uint32 sd) { stackDepth = sd; }

	void verify()
#ifndef _DEBUG
	{}
#else
	;
#endif

	//---------------------------------------------------------------
	Kind										kind;
	uint32										id;
	Mnemonic									mnemonic;

	uint32										form:1;
	uint32										reservedFlags:7;
	uint32										codeSize:8;
	uint32										properties:8;
	uint32										reservedFlags2:8;

	const Encoder::OpcodeGroupDescription	*	opcodeGroupDescription;
	BasicBlock *								basicBlock;

	uint32										index;
	uint32										codeOffset;

	uint32										allocatedOpndCount:16;
	uint32										defOpndCount:16;
	uint32										opndCount:16;
	uint32										stackDepth:16;

	Opnd **										opnds;

	//---------------------------------------------------------------
	friend class	Insts;
	friend class	IRManager;
	friend class	Encoder;
	friend class	BasicBlock;

	friend void appendToInstList(Inst *& head, Inst * listToAppend);
};


//=========================================================================================================
//   Inst virtual operand collection for iteration
//=========================================================================================================

class Inst::Opnds
{
public:
	typedef uint32 iterator;
	inline Opnds(const Inst * inst, uint32 r)
	{
		opnds = inst->getOpnds();

		if (r & Inst::OpndRole_InstLevel) {
			startIndex = 0;
			if (r & Inst::OpndRole_Use){
				endIndex = instEndIndex = inst->opndCount; 
				if (r & Inst::OpndRole_OpndLevel)
					endIndex += endIndex<<2;
			}else if (r & Inst::OpndRole_Def) 
				endIndex = instEndIndex = inst->defOpndCount;
			else
				endIndex = instEndIndex = 0;
		
			if ((r & Inst::OpndRole_InstLevel) != Inst::OpndRole_InstLevel ||
				(r & Inst::OpndRole_UseDef) == Inst::OpndRole_Use 
			){
				roles = inst->getOpndRoles();
				rolesToCheck = r;
				startIndex = next(startIndex - 1);
			}else roles = NULL;
		}else{
			instEndIndex = inst->opndCount;
			endIndex = instEndIndex + (instEndIndex<<2);
			startIndex = skipNulls(instEndIndex);
		}
	}

	inline iterator begin()const{	return startIndex;	}
	inline iterator end()const{	return endIndex;	}

	inline iterator next(iterator index)const
	{	
		++index; 
		if (index < instEndIndex){
			if (roles == NULL)
				return index;
			uint32 r = rolesToCheck;
			do { 
				uint32 ri = roles[index] & r;
				if ( (ri & Inst::OpndRole_ForIterator) && (ri & Inst::OpndRole_FromEncoder) ) return index; 
			}while (++index < instEndIndex);
		}
		return skipNulls(index);
	}

	uint32 skipNulls(uint32 index)const
	{
		while (index < endIndex){
			uint32 diffIndex = index - instEndIndex;
			Opnd * instOpnd = *(Opnd * const *)(((Byte*)opnds) + (diffIndex & ~3));
			uint32 subIndex = diffIndex & 3;
			if (subIndex == 0 && instOpnd->getMemOpndKind()==MemOpndKind_Null)
				index += 4;
			else if (instOpnd->getMemOpndSubOpnd((MemOpndSubOpndKind)(subIndex))==NULL) index++;
			else break;
		}
		return index;
	}

	uint32 fill( Opnd ** opnds )const;

	Opnd * getOpnd(uint32 index)const
	{ 
		if (index < instEndIndex)
			return opnds[index];
		else {
			uint32 diffIndex = index - instEndIndex;
			Opnd * instOpnd = *(Opnd * const *)(((Byte*)opnds) + (diffIndex & ~3));
			return instOpnd->getMemOpndSubOpnd((MemOpndSubOpndKind)(diffIndex & 3));
		}
	}

	uint32 startIndex, endIndex, instEndIndex;
	Opnd * const * opnds; const uint32 * roles;
	uint32 rolesToCheck;

};


//=========================================================================================================
//   class AliasPseudoInst
//=========================================================================================================
/**
	Class AliasPseudoInst represents ...
*/
class AliasPseudoInst: public Inst
{
protected:
    AliasPseudoInst(int id)  
		: Inst(Mnemonic_NULL, id, Inst::Form_Extended), offset(EmptyUint32)
	{kind=Kind_AliasPseudoInst;}

	uint32 offset;

	friend class	IRManager;
};

//=========================================================================================================
//   class CatchPseudoInst
//=========================================================================================================
/**
	Class CatchPseudoInst represents ...
*/
class CatchPseudoInst: public Inst
{
protected:
    CatchPseudoInst(int id)  
		: Inst(Mnemonic_NULL, id, Inst::Form_Extended)
	{kind=Kind_CatchPseudoInst;}

	virtual bool hasSideEffect()const{ return true; }
	friend class	IRManager;
};

//=========================================================================================================
//   class GCInfoPseudoInst
//=========================================================================================================
/**
Class GCInfoPseudoInst adds uses of managed pointers bases to CFG
All opnds of this inst are bases that must be live in a place in CFG this inst is located.
staticMPtrs offsets contains resolved static offsets of managed pointers
*/

class GCInfoPseudoInst: public Inst {
    friend class	IRManager;

protected:
    GCInfoPseudoInst(IRManager * irm, int id);
    virtual bool hasSideEffect()const{ return true; }

public:   
    StlVector<int32> offsets;
    const char* desc;
};

//=========================================================================================================
//   class SystemExceptionCheckInst
//=========================================================================================================

class SystemExceptionCheckPseudoInst: public Inst
{
public:
	CompilationInterface::SystemExceptionId getExceptionId()const{ return exceptionId; }
    bool checksThisOfInlinedMethod() const { return checksThis; }
protected:
    SystemExceptionCheckPseudoInst(CompilationInterface::SystemExceptionId eid, int id, bool chkThis)
		: Inst(Mnemonic_CALL, id, Inst::Form_Extended), exceptionId(eid), checksThis(chkThis)
	{kind=Kind_SystemExceptionCheckPseudoInst;}

	CompilationInterface::SystemExceptionId exceptionId;

	virtual bool hasSideEffect()const{ return false; }
    bool checksThis;
	friend class	IRManager;
};


//=========================================================================================================
//   class ControlTransferInst
//=========================================================================================================
/** class ControlTransferInst is a base class for all intructions which trasfers control:
branches, calls, rets
*/
class ControlTransferInst: public Inst
{

public:
	/** Sub-type: returns true if the instruction is a direct ConstrolTransferInst instance 
	(direct branch or direct call), which means that its operand is immediate */
	virtual bool isDirect()const
	{ return getOpndCount()>0 && getOpnd(getTargetOpndIndex())->isPlacedIn(OpndKind_Imm); }

	uint32 getTargetOpndIndex()const{ return getOpndCount(OpndRole_InstLevel|OpndRole_Def); }

	virtual bool hasSideEffect()const { return true; }
protected:
    ControlTransferInst(Mnemonic mnemonic,  int id)  
		: Inst(mnemonic, id, Form_Native){kind=Kind_ControlTransferInst;}

	friend class	IRManager;
};

//=========================================================================================================
//   class BranchInst
//=========================================================================================================
/** class BranchInst is used for all branch instructions: Jcc, JMP 
*/
class BranchInst: public ControlTransferInst
{
public:

	/** Returns the basic block this branch transfers control to
	Works only for direct branches
	*/
	BasicBlock * getDirectBranchTarget()const;

	/* Reverses direct branch condition and updates target&fallthrough edges
       does not affect layout.
     */
    void reverse(IRManager * irManager);

	/* Returns true if the direct branch can be reverted, i.e. it is direct and is conditional */
	bool canReverse()const;

protected:
    BranchInst(Mnemonic mnemonic,  int id)  : ControlTransferInst(mnemonic, id)
		{kind=Kind_BranchInst; }

private:
	friend class	IRManager;

};

//=========================================================================================================
//   class SwitchInst
//=========================================================================================================
/** class SwitchInst is used for all branch instructions: Jcc, JMP 
*/
class SwitchInst: public BranchInst
{
public:

	/** Returns the basic block for index i */
	BasicBlock * getTarget(uint32 i)const;

	/** Sets the basic block for index i */
	void setTarget(uint32 i, BasicBlock * bb);

    void replaceTarget(BasicBlock * bbFrom, BasicBlock * bbTo);

protected:
    SwitchInst(Mnemonic mnemonic,  int id)  : BranchInst(mnemonic, id)
		{kind=Kind_SwitchInst; }

private:
	friend class	IRManager;

};

//=========================================================================================================
//   class CallingConventionClient
//=========================================================================================================
class CallingConventionClient
{
public:
	struct StackOpndInfo
	{
		uint32  opndIndex;
		uint32	offset;
		bool operator<(const StackOpndInfo& r)const{ return offset < r.offset; }
	};

	CallingConventionClient(MemoryManager& mm, const CallingConvention * cc)
		:callingConvention(cc), defInfos(mm), useInfos(mm), defStackOpndInfos(mm), useStackOpndInfos(mm), defArgStackDepth(0), useArgStackDepth(0){}

	const StlVector<CallingConvention::OpndInfo> & getInfos(Inst::OpndRole role)const
	{ return (role & Inst::OpndRole_UseDef)==Inst::OpndRole_Def?defInfos:useInfos; }
	const StlVector<StackOpndInfo> & getStackOpndInfos(Inst::OpndRole role)const
	{ return (role & Inst::OpndRole_UseDef)==Inst::OpndRole_Def?defStackOpndInfos:useStackOpndInfos; }

	void pushInfo(Inst::OpndRole role, Type::Tag typeTag)
	{
		CallingConvention::OpndInfo info;
		info.typeTag=(uint32)typeTag; info.slotCount=0;
		StlVector<CallingConvention::OpndInfo> & infos = getInfos(role);
		infos.push_back(info);
	}

	void finalizeInfos(Inst::OpndRole role, CallingConvention::ArgKind argKind);
	void layoutAuxilaryOpnds(Inst::OpndRole role, OpndKind kindForStackArgs);

	const CallingConvention *	getCallingConvention()const
	{ assert(callingConvention!=NULL); return callingConvention; }

	uint32 getArgStackDepth(Inst::OpndRole role)const
	{ return (role & Inst::OpndRole_UseDef)==Inst::OpndRole_Def?defArgStackDepth:useArgStackDepth; }

	void setOwnerInst(Inst * oi){ ownerInst = oi; }
protected:
	StlVector<CallingConvention::OpndInfo> & getInfos(Inst::OpndRole role)
	{ return (role & Inst::OpndRole_UseDef)==Inst::OpndRole_Def?defInfos:useInfos; }
	StlVector<StackOpndInfo> & getStackOpndInfos(Inst::OpndRole role)
	{ return (role & Inst::OpndRole_UseDef)==Inst::OpndRole_Def?defStackOpndInfos:useStackOpndInfos; }

	const CallingConvention *	callingConvention;

	Inst *										ownerInst;

	StlVector<CallingConvention::OpndInfo>		defInfos;
	StlVector<CallingConvention::OpndInfo>		useInfos;
	StlVector<StackOpndInfo>					defStackOpndInfos;
	StlVector<StackOpndInfo>					useStackOpndInfos;
	
	uint32 defArgStackDepth, useArgStackDepth;

};

//=========================================================================================================
//   class EntryPointPseudoInst
//=========================================================================================================
/**
	Class EntryPointPseudoInst represents an entry point for an instruction 
	and is used as definition point for all incoming arguments
*/
class EntryPointPseudoInst: public Inst
{
public:
	Opnd * getDefArg(uint32 i)const;

	uint32 getArgStackDepth()const
	{ return callingConventionClient.getArgStackDepth(Inst::OpndRole_Def); }

	CallingConventionClient& getCallingConventionClient(){ return callingConventionClient; }
	const CallingConventionClient& getCallingConventionClient()const { return callingConventionClient; }

	virtual bool hasSideEffect()const { return true; }
	//--------------------------------------------------------------------
protected:
	CallingConventionClient callingConventionClient;

    EntryPointPseudoInst(IRManager * irm, int id, const CallingConvention * cc);
	friend class	IRManager;
};


//=========================================================================================================
//   class CallInst
//=========================================================================================================
/** class BranchInst is used for all calls instructions: CALL
*/
class CallInst: public ControlTransferInst
{
public:

	uint32 getArgStackDepth()const
	{ return callingConventionClient.getArgStackDepth(Inst::OpndRole_Use); }

	CallingConventionClient& getCallingConventionClient(){ return callingConventionClient; }
	const CallingConventionClient& getCallingConventionClient()const { return callingConventionClient; }

	Constraint getCalleeSaveRegs(OpndKind regKind=OpndKind_GPReg)const 
	{
		return Constraint(regKind, Constraint::getDefaultSize(regKind), callingConventionClient.getCallingConvention()->getCalleeSavedRegs(regKind));
	}

	Constraint getCallerSaveRegs(OpndKind regKind=OpndKind_GPReg)const;

	InlineInfo * getInlineInfo() const { return inlineInfo; }

protected:
	CallingConventionClient callingConventionClient;

	CallInst(IRManager * irm, int id, const CallingConvention * cc, InlineInfo* ii);

	//--------------------------------------------------------------------
	friend class	IRManager;
private:
	InlineInfo * inlineInfo;
};

//=========================================================================================================
//   class RetInst
//=========================================================================================================
/** class RetInst is used for ret instructions: RET, RET N
*/
class RetInst: public ControlTransferInst
{
public:
    RetInst(IRManager * irm, int id);
	friend class	IRManager;

	CallingConventionClient& getCallingConventionClient(){ return callingConventionClient; }
	const CallingConventionClient& getCallingConventionClient()const { return callingConventionClient; }

protected:
	CallingConventionClient callingConventionClient;

};


}}; // namespace Ia32


//=========================================================================================================
//		class ConstantAreaItem
//=========================================================================================================
/**  class ConstantAreaItem

*/
class ConstantAreaItem
{
public:
	enum Kind{
		Kind_ConstantAreaItem=0xffffffff,
		Kind_ValueConstantAreaItem=0xff,
		Kind_FPSingleConstantAreaItem=0x1,
		Kind_FPDoubleConstantAreaItem=0x2,
		Kind_InternalStringConstantAreaItem=0x4,
		Kind_BinaryConstantAreaItem=0x80,
		Kind_SwitchTableConstantAreaItem=0x100,
	};

	ConstantAreaItem(Kind k, uint32 s, const void * v)
		:kind(k), size(s), value(v), address(NULL){}

	Kind				getKind()const{ return kind; }
	bool				hasKind(Kind k)const{ return (kind&k)==kind; }

	uint32				getSize()const{ return size; }
	void const *	getValue()const{ return value; }

	void *				getAddress()const { return address; }
	void				setAddress(void * addr) { address=addr; }
protected:
	const Kind			kind;
	const uint32		size;
	void const *	value;
	void *	 			address;
};


#endif
