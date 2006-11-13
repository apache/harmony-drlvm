/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
 *
 */

#include "IpfDce.h"
#include "IpfIrPrinter.h"
#include "IpfOpndManager.h"
#include "IpfLiveAnalyzer.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Dce
//========================================================================================//

void Dce::eliminate() {
    
    IPF_LOG << endl;
    NodeVector &nodes = cfg.search(SEARCH_POST_ORDER);        // get postordered node list
    for(uint16 i=0; i<nodes.size(); i++) {                    // iterate through nodes postorder

        currLiveSet.clear();                                  // clear live set
        nodes[i]->mergeOutLiveSets(currLiveSet);              // put in the live set merged live sets of successors
    
        if(nodes[i]->getNodeKind() != NODE_BB) continue;      // node does not contain insts - ignore
    
        InstVector   &insts    = ((BbNode *)nodes[i])->getInsts();
        InstIterator currInst  = insts.end()-1;
        InstIterator firstInst = insts.begin()-1;
        
        for(; currInst>firstInst;) {
    
            if (isInstDead(*currInst)) {
                InstIterator inst = currInst--;
                IPF_LOG << "    node" << nodes[i]->getId();
                IPF_LOG << " dead code - " << IrPrinter::toString(*inst) << endl;
                insts.erase(inst);                       // remove instruction
                continue;
            }
            
            LiveAnalyzer::updateLiveSet(currLiveSet, *currInst);
            currInst--;
        }
    }
}

//----------------------------------------------------------------------------------------//
// Check if instruction can be removed from inst vector. 
// Do not remove instruction having "side effects" (like "call")

bool Dce::isInstDead(Inst *inst) {
    
    if (Encoder::isBranchCallInst(inst))   return false; // "call" inst is never dead
    if (inst->getInstCode() == INST_ALLOC) return false; // "alloc" inst is never dead

    uint16 numDst = inst->getNumDst();
    if (numDst == 0) return false;                     // if there is no dst opnds - ignore

    OpndVector& opnds = inst->getOpnds();              // get inst opnds
    
    // If no one dst opnd is in Live Set - inst is dead
    for (uint16 i=1; i<numDst+1; i++) {
        RegOpnd *opnd = (RegOpnd *)opnds[i];
        if (currLiveSet.count(opnd) > 0) return false;
    }

    return true;
}

} // IPF
} // Jitrino
