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
 * @version $Revision: 1.10.14.1.4.3 $
 */

#ifndef _IA32_COMPLEXADDRFORMLOADER_H_
#define _IA32_COMPLEXADDRFORMLOADER_H_

#include "Ia32CodeGenerator.h"
#include "Ia32CFG.h"
#include "Ia32IRManager.h"
#include "Ia32Inst.h"
#include "open/types.h"
#include "Stl.h"
#include "MemoryManager.h"
#include "Type.h"
namespace Jitrino
{
namespace Ia32{

struct SubOpndsTable {
	Opnd * baseOp;
	Opnd * indexOp;
	Opnd * scaleOp;
	Opnd * dispOp;
	Opnd * baseCand1;
	Opnd * baseCand2;
	Opnd * suspOp;

	SubOpndsTable(Opnd * s, Opnd * disp) : baseOp(NULL), indexOp(NULL), scaleOp(NULL), dispOp(disp), baseCand1(NULL), baseCand2(NULL), suspOp(s) {}
};

BEGIN_DECLARE_IRTRANSFORMER(ComplexAddrFormLoader, "cafl", "Complex Address Form Loader")
	IRTRANSFORMER_CONSTRUCTOR(ComplexAddrFormLoader)
	void runImpl();
protected:
	//fill complex address form
	bool findAddressComputation(Opnd * memOp);
	bool checkIsScale(Inst * inst);
	void walkThroughOpnds(SubOpndsTable& table);
private:
	uint32 refCountThreshold;
END_DECLARE_IRTRANSFORMER(ComplexAddrFormLoader)

} //end namespace Ia32
}
#endif
