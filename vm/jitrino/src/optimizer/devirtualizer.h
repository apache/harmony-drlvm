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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.8.16.1.4.4 $
 *
 */

#ifndef _DEVIRTUALIZER_H_
#define _DEVIRTUALIZER_H_

#include "optpass.h"
#include "irmanager.h"

namespace Jitrino {

DEFINE_OPTPASS(GuardedDevirtualizationPass)
DEFINE_OPTPASS(GuardRemovalPass)

class Devirtualizer {
public:
    Devirtualizer(IRManager& irm);

    void guardCallsInRegion(IRManager& irm, DominatorTree* tree);

    void unguardCallsInRegion(IRManager& irm, uint32 safetyLevel=0);

private:
    void guardCallsInBlock(IRManager& irm, CFGNode* node);
    void genGuardedDirectCall(IRManager& irm, CFGNode* node, Inst* call, MethodDesc* methodDesc, Opnd *tauNullChecked, Opnd *tauTypesChecked, uint32 argOffset);
    bool isGuardableVirtualCall(Inst* inst, MethodInst*& methodInst, Opnd*& base,
                                Opnd* & tauNullChecked, Opnd*&tauTypesChecked,
                                uint32 &argOffset);
    bool doGuard(IRManager& irm, CFGNode* node, MethodDesc& methodDesc);
    bool isPreexisting(Opnd* obj);

    bool _hasProfileInfo;
    bool _doProfileOnlyGuardedDevirtualization;
    bool _doAggressiveGuardedDevirtualization;
    bool _skipColdTargets;
    bool _devirtUseCHA;
    bool _devirtSkipExceptionPath;

    TypeManager& _typeManager;
    InstFactory& _instFactory;
    OpndManager& _opndManager;
};

} //namespace Jitrino 

#endif //_DEVIRTUALIZER_H_
