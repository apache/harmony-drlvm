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
 * @version $Revision$
 *
 */

#include "IpfLiveAnalyzer.h"
#include "IpfIrPrinter.h"
#include "IpfOpndManager.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// LiveAnalyzer
//========================================================================================//

LiveAnalyzer::LiveAnalyzer(Cfg &cfg_) : cfg(cfg_) { }

//----------------------------------------------------------------------------------------//

void LiveAnalyzer::makeLiveSets(bool verify_) {
    
    IPF_LOG << endl;
    verify = verify_;
    NodeVector &nodes = cfg.search(SEARCH_POST_ORDER);   // get postordered node list

    // clear live sets for all nodes
    if (!verify) {
        for(uint16 i=0; i<nodes.size(); i++) {
            nodes[i]->clearLiveSet();
        }
    }
    
    // make new live sets for all nodes
    bool flag = true;
    while (flag == true) {
        flag = false;
        for(uint16 i=0; i<nodes.size(); i++) {
            if (analyzeNode(nodes[i]) == false) flag = true;
        }
    }
}

//----------------------------------------------------------------------------------------//

bool LiveAnalyzer::analyzeNode(Node *node) {

    // build live set for node
    RegOpndSet currLiveSet;
    RegOpndSet &oldLiveSet = node->getLiveSet();
    node->mergeOutLiveSets(currLiveSet);          // put in the live set merged live sets of successors

    if (LOG_ON) {
        IPF_LOG << "    node" << node->getId() << " successors:";
        EdgeVector &edges = node->getOutEdges();
        for (uint16 i=0; i<edges.size(); i++) {
            Node *succ       = edges[i]->getTarget();
            Node *loopHeader = succ->getLoopHeader();
            IPF_LOG << " " << succ->getId();
            if (loopHeader != NULL) IPF_LOG << "(" << loopHeader->getId() << ")";
        }
        IPF_LOG << " exit live set: " << IrPrinter::toString(currLiveSet) << endl;
    }

    // If node is not BB - currLiveSet is not going to change 
    if(node->getNodeKind() != NODE_BB) {
        IPF_LOG << "      node is not BB - live set is not changed" << endl;
        if (oldLiveSet == currLiveSet) return true;     // if live set has not changed - nothing to do

        node->setLiveSet(currLiveSet);            // Set currLiveSet for the current node
        return false;                                   // and continue
    }

    InstVector   &insts    = ((BbNode *)node)->getInsts();
    InstIterator currInst  = insts.end()-1;
    InstIterator firstInst = insts.begin()-1;
    
    for(; currInst>firstInst; currInst--) {

        updateLiveSet(currLiveSet, *currInst);

        IPF_LOG << "      " << left << setw(46) << IrPrinter::toString(*currInst); 
        IPF_LOG << " live set : " << IrPrinter::toString(currLiveSet) << endl; 
    }
    
    if (oldLiveSet == currLiveSet) return true;     // if live set has not changed - nothing to do
    if (verify) {
        IPF_LOG << "ERROR node" << node->getId() << endl;
        IPF_LOG << " old live set: " << IrPrinter::toString(oldLiveSet) << endl;
        IPF_LOG << " new live set: " << IrPrinter::toString(currLiveSet) << endl;
    }
    node->setLiveSet(currLiveSet);                  // set currLiveSet for the current node
    return false;
}

//----------------------------------------------------------------------------------------//
// Remove dst and insert qp and src opnds in live set. 
// If qp is not "p0" do not remove dst opnd from live set, 
// because if the inst is not executed dst opnd will stay alive

void LiveAnalyzer::updateLiveSet(RegOpndSet &liveSet, Inst *inst) {

    OpndVector &opnds = inst->getOpnds();            // get instruction's opnds
    uint16     numDst = inst->getNumDst();           // number of dst opnds (qp has index 0)
    bool       isP0   = (opnds[0]->getValue() == 0); // is instruction qp is p0

    // remove dst opnds from live set
    for(uint16 i=1; isP0 && i<=numDst; i++) {
        if (opnds[i]->isWritable()) liveSet.erase((RegOpnd *)opnds[i]);
    }
    
    // insert qp opnd in live set
    if (opnds[0]->isWritable()) liveSet.insert((RegOpnd *)opnds[0]);
    
    // insert src opnds in live set
    for(uint16 i=numDst+1; i<opnds.size(); i++) {
        if (opnds[i]->isWritable()) liveSet.insert((RegOpnd *)opnds[i]);     
    } 
}    

//----------------------------------------------------------------------------------------//
// Remove dst opnds from live set. 
// If qp is not "p0" do not remove dst opnd from live set, 
// because if the inst is not executed dst opnd will stay alive

void LiveAnalyzer::defOpnds(RegOpndSet &liveSet, Inst *inst) {

    OpndVector &opnds = inst->getOpnds();            // get instruction's opnds
    uint16     numDst = inst->getNumDst();           // number of dst opnds (qp has index 0)
    bool       isP0   = (opnds[0]->getValue() == 0); // is instruction qp is p0

    // remove dst opnds from live set
    for(uint16 i=1; isP0 && i<=numDst; i++) {
        if (opnds[i]->isWritable()) liveSet.erase((RegOpnd *)opnds[i]);
    }
}    

//----------------------------------------------------------------------------------------//
// Insert qp and src opnds in live set. 

void LiveAnalyzer::useOpnds(RegOpndSet &liveSet, Inst *inst) {

    OpndVector &opnds = inst->getOpnds();                          // get instruction's opnds
    uint16     numDst = inst->getNumDst();                         // number of dst opnds (qp has index 0)

    // insert qp opnd in live set
    if (opnds[0]->isWritable()) liveSet.insert((RegOpnd *)opnds[0]);
    
    // insert src opnds in live set
    for(uint16 i=numDst+1; i<opnds.size(); i++) {
        if (opnds[i]->isWritable()) liveSet.insert((RegOpnd *)opnds[i]);     
    } 
}    

} // IPF
} // Jitrino
