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
 * @author Nikolay A. Sidelnikov
 * @version $Revision: 1.8.26.4 $
 */

#ifndef _IA32_RCE_H_
#define _IA32_RCE_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{

//========================================================================================
// class RCE
//========================================================================================
/**
 *	class RCE performs removing comparisons following instructions which
 *	affected flags in the same way as CMP. In some cases instructions can be
 *	reordered for resolving comparison as available for removing
 *
 *	The algorithm takes one-pass over CFG.
 *
 *	This transformer ensures that
 *
 *	1)	All conditional instructions get the same EFLAGS value as before 
 *		transformation
 *
 *	2)	All reordered instructions do the same effects as before transformation
 *
 *	For example:
 *	
 *	Original code piece:
 *		I29: t50.:int32 (ID:v15(EFLGS):uint32) =AND .t28:int32,t49(1):int32 
 *		I30: (AD:v1:int32) =CopyPseudoInst (AU:t48:int32) 
 *		I31: (AD:v2:int32) =CopyPseudoInst (AU:t25:int32) 
 *		I32: (AD:v3:int8[]) =CopyPseudoInst (AU:t38:int8[]) 
 *		I33: (ID:v15(EFLGS):uint32) =CMP .t50:int32,t51(0):int32 
 *		I34: JNZ BB_12 t52(0):intptr (IU:v15(EFLGS):uint32) 
 *
 *	After optimization:
 *		I29: t50:int32 (ID:v15(EFLGS):uint32) =AND .t28:int32,t49(1):int32 
 *		I30: (AD:v1:int32) =CopyPseudoInst (AU:t48:int32) 
 *		I31: (AD:v2:int32) =CopyPseudoInst (AU:t25:int32) 
 *		I32: (AD:v3:int8[]) =CopyPseudoInst (AU:t38:int8[]) 
 *		I34: JNZ BB_12 t52(0):intptr (IU:v15(EFLGS):uint32) 
 *
 *	The implementation of this transformer is located in Ia32RCE.cpp
 *
 */
	
BEGIN_DECLARE_IRTRANSFORMER(RCE, "rce", "Redundant comparison elimination")
	IRTRANSFORMER_CONSTRUCTOR(RCE)
	void runImpl();
protected:

	//	check is flags using by conditional instruction affected by instruction
	bool isUsingFlagsAffected(Inst * inst, Inst * condInst);

	//	check instruction inst for possibility of removing
	bool isSuitableToRemove(Inst * inst, Inst * condInst, Inst * cmpInst, Opnd * cmpOp);
	
END_DECLARE_IRTRANSFORMER(RCE)
}} //end namespace Ia32

#endif
