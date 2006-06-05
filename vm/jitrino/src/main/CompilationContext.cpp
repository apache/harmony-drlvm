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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.3.10.2.4.4 $
 */
#include "CompilationContext.h"

#include "optimizer.h"
#include "IRBuilder.h"
#include "MemoryManager.h"

#ifdef _IPF_
#else 
#include "ia32/Ia32CodeGenerator.h"
#endif

namespace Jitrino {

CompilationContext::CompilationContext(MemoryManager& _mm, CompilationInterface* ci, JITModeData* mode) 
: mm(_mm), compilationInterface(ci) 
{
    thisPT = NULL;
    optFlags = new (mm) OptimizerFlags;
    transFlags = new (mm) TranslatorFlags;
    irbFlags  = new (mm) IRBuilderFlags();
    modeData = mode;
#ifdef _IPF_
#else 
    ia32CGFlags = new (mm) Ia32::CGFlags();
#endif
}

}

