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
 * @version $Revision: 1.8.22.3 $
 */
#if !defined(__IA32REGALLOC_H_INCLUDED__)
#define __IA32REGALLOC_H_INCLUDED__

#include "Ia32IRManager.h"
#include "Stl.h"


namespace Jitrino
{
namespace Ia32
{


//========================================================================================
// class Ia32RegAlloc2
//========================================================================================

/**
 *	This class attempts to assign register for any operand (found in LIR) that can be 
 *	allocated in register.
 *
 *	Set of registers available for allocation is specified by input arguments and saved 
 *	in 'constrs' class member. All operands that cannot be allocated in the registers 
 *	available are simply ignored.
 *
 *	So this allocator should be called for each set of the registers available (GPReg, XMM, 
 *	FP) independently.
 *
 *	It is not guaranteed that all operands which can be assigned will be assigned. 
 *	Therefore, the companion class (SpillGen) must be used after this allocator.
 *
 *	The main idea behind the allocator is well-known bin packing algorithm, but this 
 *	implementation is completely independent of any published algorithms.
 */

BEGIN_DECLARE_IRTRANSFORMER(RegAlloc2, "bp_regalloc", "Bin-pack register allocator")

	RegAlloc2 (IRManager&, const char * params);
	~RegAlloc2 ();

	uint32 getNeedInfo () const		{return NeedInfo_LivenessInfo;}
	uint32 getSideEffects () const	{return 0;}

	void runImpl();
	bool verify(bool force=false);

//protected:

	typedef uint32 RegMask;		// used to represent set of registers
	typedef size_t Instnb;		// used to describe instruction number

	struct Span;				// interval of instructions not crossed basic block boundary
	struct Opand;				// extension of the 'Opnd' structure
	struct Register;			// holds all operands assigned to this register

	MemoryManager mm;			// this is private MemoryManager, not irm.getMemoryManager()
	size_t opandcount;			// total count of operands in LIR
	Constraint constrs;			// initial constraints (registers available)

	typedef StlVector<Register*> Registers;
	Registers registers;

	typedef StlVector<Opand*> Opands;
	Opands opandmap;			// mapping Ia32Opnd.id -> Opand*
	size_t candidateCount;

	void buildRegs ();
	void buildOpands ();
	void allocateRegs ();
	Register* findReg (RegMask) const;

END_DECLARE_IRTRANSFORMER(RegAlloc2)


} //namespace Ia32
} //namespace Jitrino

#endif	// ifndef __IA32REGALLOC_H_INCLUDED__
