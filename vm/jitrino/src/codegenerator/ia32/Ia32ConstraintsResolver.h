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
 * @version $Revision: 1.8.22.3 $
 */

#ifndef _IA32_CONSTRAINTS_RESOLVER_H_
#define _IA32_CONSTRAINTS_RESOLVER_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{



//========================================================================================
// class Ia32ConstraintsResolver
//========================================================================================
/**
 *	class Ia32ConstraintsResolver performs resolution of operand constraints
 *	and assigns calculated constraints (Opnd::ConstraintKind_Calculated) to operands.
 *	The results calculated constraints of operands determine allowable physical location
 *	for the operand.
 *	
 *	This transformer allows to insert operands into instructions before it 
 *  regardless instruction constraints
 *	ConstraintResolver analyzes weak instruction constraints and splits operands when necessary.
 * 
 *	This transformer ensures that 
 *	1)  All instruction constraints for EntryPoints, CALLs and RETs
 *		are set appropriately (IRManager::applyCallingConventions())
 *	2)	All operands has non-null calculated constraints 
 *  3)	All operands fits into instructions they are used in (in terms of instruction constraints)
 *		For example:
 *			Original code piece:
 *				I38: (AD:s65:double) =CopyPseudoInst (AU:t1:double) 
 *				I32: MULSD .s65.:double,.t2:double 
 *				I33: RET t66(0):int16 (AU:s65:double) 
 *			
 *				RET imposes constraint on s65 requiring to place it into FP0 register 
 *				(its FP0D alias in this particular case)
 *				MULSD imposes constraint on s65 requiring to place it into XMM register 
 *						
 *			After optimization: 
 *				I38: (AD:s65:double) =CopyPseudoInst (AU:t1:double) 
 *				I32: MULSD .s65.:double,.t2:double 
 *				I46: (AD:t75:double) =CopyPseudoInst (AU:s65:double) 
 *				I33: RET t66(20):int16 (AU:t75:double) 
 *			
 *				Thus, ConstraintResolver inserted I46 splitting s65 to s65 and t75
 *				s65 is assigned with Mem|XMM calculated constraint and t75 
 *              is assigned with FP0D calculated calculated constraint
 *
 *  4)	If the live range of an operand crosses a call site and the operand is not redefined 
 *		in the call site, the calculated constraint of the operand is narrowed allowing to 
 *		place it only in the callee-save regs or in memory (stack)
 *		
 *	5)	If the operand (referred as original operand here)
 *		 c0) has already been assigned to some location (its location constraint is not null) 
 *		 c1) or has some Initial constraint with kind!=OpndKind_Any
 *		 c2) or is live at entry of a catch handler 
 *		Then:
 *		 -	necessary operand splitting is performed as close as possible 
 *			to the instruction which caused the splitting and original operand is used before and after
 *			the instruction. 
 *
 *		 - 	The calculated constraint of the original operand is always within the constraints 
 *			described above in c0-c2 (their &)
 *
 *		For example, we had v0 which has initial constraint to place it into EBX|mem
 *		and instructin
 *		shl v1, v0; 	shl requires v0 to be immediate of ecx (cl)
 *		The result will be (operand names are arbitrary):
 *		mov t10, v0
 *		shl v1,	t10;	t10 is assigned ECX calculated constraint
 *		... t10 is NOT used after ...
 *		
 *		
 *	The main principle of the algorithm is anding of weak instruction constraint into
 *	operand calculated constraints and splitting operands to ensure that the calculated constraint
 *	is not null
 *
 *	This transformer must be inserted before register allocator which relies on 
 *  calculated operand constraints. 
 *
 *	The implementation of this transformer is located in ConstraintResolverImpl class 
 *  defined in Ia32ConstraintResolver.cpp. For the description of a particular algorithm 
 *  please refer to Ia32ConstraintResolver.cpp
 *
 *
 */

BEGIN_DECLARE_IRTRANSFORMER(ConstraintsResolver, "constraints", "Constraints resolver")
	IRTRANSFORMER_CONSTRUCTOR(ConstraintsResolver)
	/** runImpl is required override, calls ConstraintsResolverImpl.runImpl */
	void runImpl();
	/** This transformer requires up-to-date liveness info */
	uint32 getNeedInfo()const{ return NeedInfo_LivenessInfo; }
	uint32 getSideEffects()const{ return 0; }
END_DECLARE_IRTRANSFORMER(ConstraintsResolver)

}; // namespace Ia32
}
#endif
