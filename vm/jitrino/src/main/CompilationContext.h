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

#ifndef _COMPILATION_CONTEXT_H_
#define _COMPILATION_CONTEXT_H_
#include <assert.h>
namespace Jitrino {

class MemoryManager;
class CompilationInterface;
class JitrinoParameterTable;
struct OptimizerFlags;
struct TranslatorFlags;
struct IRBuilderFlags;
class JITModeData;
#ifdef _IPF_
#else 
namespace Ia32{
    struct CGFlags;
}
#endif

class CompilationContext {
public:
    CompilationContext(MemoryManager& mm, CompilationInterface* ci, JITModeData* mode);

    CompilationInterface* getCompilationInterface() const  {return compilationInterface;}
    MemoryManager& getCompilationLevelMemoryManager() const {return mm;}

    JitrinoParameterTable* getThisParameterTable() const { return thisPT;}
    void setThisParameterTable(JitrinoParameterTable* _thisPT) { assert(!thisPT); thisPT = _thisPT;}


    OptimizerFlags* getOptimizerFlags() const {return optFlags;};
    TranslatorFlags* getTranslatorFlags() const {return transFlags;}
    IRBuilderFlags* getIRBuilderFlags() const  {return irbFlags;}

    JITModeData* getCurrentModeData() const {return modeData;}
#ifdef _IPF_
#else
    Ia32::CGFlags* getIa32CGFlags() const {return ia32CGFlags;}
#endif

private:
    MemoryManager&          mm;
    CompilationInterface*   compilationInterface;
    JitrinoParameterTable* thisPT;

    OptimizerFlags*         optFlags;
    TranslatorFlags*        transFlags;
    IRBuilderFlags*         irbFlags;
    JITModeData*            modeData;
#ifdef _IPF_
#else
    Ia32::CGFlags*            ia32CGFlags;
#endif
};

}
#endif
