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
 * @version $Revision: 1.8.24.4 $
 *
 */

#include "pidgenerator.h"
#include "irmanager.h"
#include "FlowGraph.h"

namespace Jitrino {

DEFINE_OPTPASS_IMPL(PersistentInstIdGenerationPass, pidgen, "Persistent Instruction Id Generation Pass")

void
PersistentInstIdGenerationPass::_run(IRManager& irm) {
    PersistentInstructionIdGenerator pass;
    pass.runPass(irm);
}

void
PersistentInstructionIdGenerator::runPass(IRManager& irm) {
    MemoryManager mm(0, "PersistentInstructionIdGenerator::runPass");
    
    MethodDesc& methodDesc = irm.getMethodDesc();

    StlVector<CFGNode*> nodes(mm);
    irm.getFlowGraph().getNodesPostOrder(nodes);

    StlVector<CFGNode*>::reverse_iterator i;
    for(i = nodes.rbegin(); i != nodes.rend(); ++i) {
        CFGNode* node = *i;
        Inst* label = node->getFirstInst();
        for(Inst* inst = label->next(); inst != label; inst = inst->next())
            inst->setPersistentInstructionId(PersistentInstructionId(&methodDesc, inst->getId() - irm.getMinimumInstId()));
    }
}

} //namespace Jitrino 
