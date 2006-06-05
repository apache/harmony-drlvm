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
 * @version $Revision: 1.6.22.3 $
 */

#ifndef _IA32_COPYEXPANSION_H_
#define _IA32_COPYEXPANSION_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{



//========================================================================================
// class CopyExpansion
//========================================================================================
/**
	class CopyExpansion translated CopyPseudoInsts to corresponding copying sequences
*/
BEGIN_DECLARE_IRTRANSFORMER(CopyExpansion, "copy", "Copy pseudo inst expansion")
	IRTRANSFORMER_CONSTRUCTOR(CopyExpansion)
	void runImpl();
	void restoreRegUsage(BasicBlock * bb, Inst * toInst, uint32& regUsageMask);
	uint32 getNeedInfo()const{ return NeedInfo_LivenessInfo; }
	uint32 getSideEffects()const{ return 0; }
END_DECLARE_IRTRANSFORMER(CopyExpansion)

}}; // namespace Ia32

#endif
