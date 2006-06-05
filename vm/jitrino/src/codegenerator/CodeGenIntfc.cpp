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
 * @author Intel, Vyacheslav P. Shakin
 * @version $Revision: 1.16.16.5 $
 *
 */

#include "CodeGenIntfc.h"
#if defined (_IPF_)
	#include "ipf/IpfCodeGenerator.h"
#else
	#include "ia32/Ia32CodeGenerator.h"
#endif

namespace Jitrino {
	
void CodeGenerator::readFlagsFromCommandLine(CompilationContext* cs, bool ia32Cg)
{
#if defined(_IPF_)
	IPF::IpfCodeGenerator::readFlagsFromCommandLine(cs);
#else
	Ia32::CodeGenerator::readFlagsFromCommandLine(cs);
#endif
}

void CodeGenerator::showFlagsFromCommandLine(bool ia32Cg)
{
#if defined (_IPF_)
	IPF::IpfCodeGenerator::showFlagsFromCommandLine();
#else
	Ia32::CodeGenerator::showFlagsFromCommandLine();
#endif
}

}

