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
 * @version $Revision: 1.8.20.3 $
 */

#ifndef _IA32_I8_LOWERER_H_
#define _IA32_I8_LOWERER_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{

BEGIN_DECLARE_IRTRANSFORMER(I8Lowerer, "i8l", "8-byte instructions lowerer")
	I8Lowerer(IRManager& irm, const char * params=0): IRTransformer(irm, params),foundI8Opnds(0) {}
	void runImpl();
protected:
	void processOpnds(Inst * inst, StlMap<Opnd *,Opnd **>& pairs);
	void prepareNewOpnds(Opnd * longOpnd, StlMap<Opnd *,Opnd **>& pairs, Opnd*& newOp1, Opnd*& newOp2);
	bool isI8Type(Type * t){ return t->tag==Type::Int64 || t->tag==Type::UInt64; }
	uint32 getNeedInfo () const		{return 0;}
	virtual uint32 getSideEffects()const {return foundI8Opnds;}

	void buildComplexSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst = NULL);
	void buildSetSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst = NULL);
	void buildJumpSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst = NULL);
	void buildSimpleSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2);

	uint32 foundI8Opnds;

END_DECLARE_IRTRANSFORMER(I8Lowerer)

}} //namespace Ia32
#endif //_IA32_I8_LOWERER_H_

