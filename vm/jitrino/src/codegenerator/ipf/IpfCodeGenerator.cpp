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
 * @author Intel, Konstantin M. Anisimov, Igor V. Chebykin
 * @version $Revision$
 *
 */

#include "Jitrino.h"
#include "IpfCodeGenerator.h"
#include "IpfCodeSelector.h"
#include "IpfCodeLayouter.h"
#include "IpfLiveAnalyzer.h"
#include "IpfDce.h"
#include "IpfRegisterAllocator.h"
#include "IpfSpillGen.h"
#include "IpfIrPrinter.h"
#include "IpfEmitter.h"
#include "IpfPrologEpilogGenerator.h"
#include "IpfRuntimeSupport.h"
#include "IpfCfgVerifier.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// IpfCodeGenerator
//========================================================================================//

CodeGenerator::CodeGenerator(MemoryManager        &memoryManager_, 
                             CompilationInterface &compilationInterface_) 
    : memoryManager(memoryManager_), compilationInterface(compilationInterface_) {

    cfg        = NULL;
    methodDesc = NULL;
}

//----------------------------------------------------------------------------------------//

bool CodeGenerator::genCode(MethodCodeSelector &methodCodeSelector) {

    MemoryManager  mm(0x1000, "IpfCodeGenerator");
    cfg = new(mm) Cfg(mm, compilationInterface);
    char logDirName[] = "tmp_dir_name";
    IrPrinter irPrinter(*cfg, logDirName);
    methodDesc = compilationInterface.getMethodToCompile();

//    bool ipfLogIsOnSaved = ipfLogIsOn;
//    if (isIpfLogoutMethod(methodDesc)) {
//        ipfLogIsOn = true;
//    }
    if(LOG_ON) {
        const char *methodName     = methodDesc->getName();
        const char *methodTypeName = (methodDesc->getParentType()!=NULL
            ? methodDesc->getParentType()->getName()
            : "");
        const char * methodSignature = methodDesc->getSignatureString();

        IPF_LOG << endl << methodTypeName << "." << methodName << methodSignature << endl;
    }

    IPF_LOG << endl << "=========== Stage: Code Selector =============================" << endl;
    IpfMethodCodeSelector ipfMethodCodeSelector(*cfg, compilationInterface);
    methodCodeSelector.selectCode(ipfMethodCodeSelector);

    methodDesc = ipfMethodCodeSelector.getMethodDesc();
    cfg->getOpndManager()->initCompBases((BbNode *)cfg->getEnterNode());
    cfg->getOpndManager()->saveThisArg();
    if(LOG_ON) irPrinter.printCfgDot("/cfg_cs.dot");

    IPF_LOG << endl << "=========== Stage: Code Layouter =============================" << endl;
    CodeLayouter codeLayouter(*cfg);
    codeLayouter.layout();
    if(LOG_ON) {
        irPrinter.printCfgDot("/cfg_cl.dot");
        irPrinter.printLayoutDot("/lot.dot");
    }

    IPF_LOG << endl << "=========== Stage: Liveness analyzis =========================" << endl;
    LiveAnalyzer liveAnalyzer(*cfg);
    liveAnalyzer.makeLiveSets(false);

    IPF_LOG << endl << "=========== Stage: Dead Code Eliminator ======================" << endl;
    Dce dce(*cfg);
    dce.eliminate();

    liveAnalyzer.makeLiveSets(false);
    IPF_LOG << endl << "=========== Stage: Build GC Root Set =========================" << endl;
    RuntimeSupport runtimeSupport(*cfg, compilationInterface);
    runtimeSupport.buildRootSet();

    IPF_LOG << endl << "=========== Stage: Register Allocator ========================" << endl;
    RegisterAllocator registerAllocator(*cfg);
    registerAllocator.allocate();
    if(LOG_ON) irPrinter.printAsm(LOG_OUT);

    IPF_LOG << endl << "=========== Stage: Prolog and Epilog Generator ===============" << endl;
    PrologEpilogGenerator prologEpilogGenerator(*cfg);
    prologEpilogGenerator.genPrologEpilog();
    
    IPF_LOG << endl << "=========== Stage: Spill Generator ===========================" << endl;
    SpillGen spillGen(*cfg);
    spillGen.genSpillCode();
    if(LOG_ON) irPrinter.printAsm(LOG_OUT);

    IPF_LOG << endl << "=========== Stage: Code Emitter ==============================" << endl;
    Emitter emitter(*cfg, compilationInterface);
    bool ret = emitter.emit();

    IPF_LOG << endl << "=========== Stage: Make Runtime Info =========================" << endl;
    runtimeSupport.makeRuntimeInfo();
    
    if(ret) IPF_LOG << endl << "=========== Compilation Successful ===========================" << endl;
    else    IPF_LOG << endl << "=========== Compilation Failed ===============================" << endl;

//    ipfLogIsOn = ipfLogIsOnSaved;
    return ret;
}

} // IPF
} // Jitrino 

