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
 * @author Sergey L. Ivashin
 * @version $Revision: 1.14.20.3 $
 */

#if !defined(__IA32SPILLGEN_H_INCLUDED__)
#define __IA32SPILLGEN_H_INCLUDED__

#include "Ia32IRManager.h"
#include "Stl.h"


namespace Jitrino
{

namespace Ia32
{

BEGIN_DECLARE_IRTRANSFORMER(SpillGen, "spillgen", "Spill code generator")

	SpillGen (IRManager&, const char * params);
	~SpillGen ();

	uint32 getNeedInfo () const		{return NeedInfo_LivenessInfo;}
	uint32 getSideEffects () const	{return SideEffect_InvalidatesLivenessInfo;}

	void runImpl();
	bool verify(bool force=false);

//	Inter-blocks data
//	-----------------

	typedef uint32 RegMask;
	typedef StlVector<Opnd*> Opnds;

	MemoryManager mm;			// this is private MemoryManager, not irm.getMemoryManager()

	// Table of all available registers sets.
	// Each set (GPReg, FPReg, XMMReg and so on) is represented by the corresponding 
	// Constraint object.
	struct Registers : public StlVector<Constraint>
	{
		Registers (MemoryManager& mm)		:StlVector<Constraint>(mm) {}

		void  parse (const char*);

		// clear mask fields in all elements of the table
		void clearMasks ();

		void merge (const Registers&);

		// register the new constraint (register) in the table.
		// if table doesn't contain constraint of the specified kind, it will be ignored
		// (add = false) or new table entry for the constraint will be created (add = true).
		int merge (const Constraint&, bool add = false);

		// returns table index of the constraint of the specified kind or -1
		int getIndex (const Constraint&) const;

		// returns pointer to the table element of the specified kind
		Constraint* contains (const Constraint&);

		int indexes[IRMaxRegKinds];
	};
	Registers registers;

	size_t  opndcount;			// initial count of operands
	size_t  emitted;			// extra instructions count
	size_t  fails;				// if != 0, then SpillGen should be aborted

	// mapping operand -> its memory location
	// every operand that was splitted to memory is registered in this table
	typedef ::std::pair<const Opnd*, Opnd*> TwoOpnds;
	typedef StlVector<TwoOpnds> MemOpnds;
	MemOpnds memopnds;


//	Block-specific data
//	-------------------


	// extension of 'Inst' structure with some specific data
	struct  Instx;
	typedef StlVector<Instx> Instxs;
	Instxs  instxs;

	// created for all problem (needed to be allocated) operands	
	struct  Op;
	struct  Opline;
	typedef StlVector<Opline> Oplines;
	Oplines actives;
	StlVector<Opline*> actsmap;

	struct  Evict;
	typedef StlVector<Evict> Evicts;

	// current context 
	BasicBlock* bblock;		// the basic block being processed
	LiveSet* lives_start,	// liveness info for start and end of the block
		   * lives_exit;	//
	LiveSet* lives_catch;	// 0 or mask of operands live at the corresponding dispatch node

	bool   evicts_known;


//	Methods
//	-------

   size_t pass0 ();
   size_t pass1 ();
 RegMask  lookPreffered (Opline&);
	bool  tryRegister (Opline&, Constraint, RegMask);
	bool  tryMemory   (Opline&, Constraint);
	bool  tryEvict    (Opline&, Constraint);
	bool  tryRepair   (Opline&, Constraint);
	bool  simplify	  (Inst*, Opnd*);
  RegMask usedRegs  (const Instx*, int idx, bool isuse) const;
  RegMask callRegs  (const Instx*, int idx) const;
	int   update	  (const Inst*, const Opnd*, Constraint&) const;
	void  assignReg   (Opline&, Instx* begx, Instx* endx, RegName);
	void  assignMem   (Opline&, Instx* begx, Instx* endx);
	void  saveOpnd    (Opline&, Instx*, Opnd*);
	void  saveOpnd    (Opline&, Instx*, Opnd*, RegMask, int, bool);
	void  loadOpnd    (Opline&, Instx*, Opnd*);
	void  loadOpndMem (Opline&);
	Opnd* opndMem     (Opline&);
	Opnd* opndMem     (const Opnd*);
	Opnd* opndReg     (const Opnd*, RegName) const;
	void  emitPushPop (bool before, Inst*, Opnd* opnd, bool push);
	Inst* emitMove    (bool before, Inst*, Opnd* dst, Opnd* src);
	RegName findFree  (RegMask usable, int idx, Instx* begx);
	RegName findEvict (RegMask usable, int idx, Instx* begx, Instx*& endx);
    void    setupEvicts ();
	Evict*  pickEvict (Evicts&);
	bool    isEvict   (const Opnd*,  const Instx*) const;
	void    killEvict (const Opnd*, Instx*) const;
	RegMask getRegMaskConstr (const Opnd*, Constraint) const;

END_DECLARE_IRTRANSFORMER(SpillGen)


} //namespace Ia32
} //namespace Jitrino

#endif
