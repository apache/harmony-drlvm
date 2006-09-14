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

#include "IpfRegisterAllocator.h"
#include "IpfIrPrinter.h"
#include "IpfOpndManager.h"
#include "IpfLiveAnalyzer.h"

namespace Jitrino {
namespace IPF {

bool greaterSpillCost(RegOpnd *op1, RegOpnd *op2) { return op1->getSpillCost() > op2->getSpillCost(); }

//========================================================================================//
// RegisterAllocator
//========================================================================================//

RegisterAllocator::RegisterAllocator(Cfg &cfg_) : 
    mm(cfg_.getMM()), 
    cfg(cfg_) {

    opndManager = cfg.getOpndManager();
}

//----------------------------------------------------------------------------------------//

void RegisterAllocator::allocate() {
    
    IPF_LOG << endl << "  Build Interference Matrix" << endl << endl;
    buildInterferenceMatrix();
    makeInterferenceMatrixSymmetric();

    IPF_LOG << endl << "  Remove Preassigned Opnds" << endl;
    removePreassignedOpnds();

    IPF_LOG << endl << "  Assign Locations" << endl;
    assignLocations();
}

//----------------------------------------------------------------------------------------//

void RegisterAllocator::buildInterferenceMatrix() {

    NodeVector &nodes = cfg.search(SEARCH_POST_ORDER);

    for(uint16 i=0; i<nodes.size(); i++) {            // iterate through CFG nodes

        IPF_LOG << "    node" << nodes[i]->getId();
        if(nodes[i]->isBb() == false) {               // ignore non BB nodes
            IPF_LOG << "      node is not BB - ignore" << endl;
            continue;
        }

        liveSet.clear();                              // clear live set
        nodes[i]->mergeOutLiveSets(liveSet);          // put in the live set merged live sets of successors
        IPF_LOG << " live set: " << IrPrinter::toString(liveSet) << endl;

        BbNode       *node       = (BbNode *)nodes[i];
        uint32       execCounter = node->getExecCounter();
        InstIterator currInst    = node->getInsts().end()-1;
        InstIterator firstInst   = node->getInsts().begin()-1;
        
        for(; currInst>firstInst; currInst--) {

            Inst        *inst  = *currInst;
            uint16      numDst = inst->getNumDst();       // number of dst opnds (qp has index 0)
            OpndVector& opnds  = inst->getOpnds();        // get inst's opnds

            LiveAnalyzer::defOpnds(liveSet, inst);        // remove dst opnds from live set
            checkCallSite(inst);                          // if currInst is "call" - all alive opnds cross call site
            for(uint16 i=1; i<=numDst; i++) {             // for each dst opnd
                updateAllocSet(opnds[i], execCounter);    // insert in allocSet and add live set in dep list
            }
            
            LiveAnalyzer::useOpnds(liveSet, inst);        // add src opnds in live set
            updateAllocSet(opnds[0], execCounter);        // insert in allocSet pq opnd and add alive qps in dep list
            for(uint16 i=numDst+1; i<opnds.size(); i++) { // for each src opnd
                updateAllocSet(opnds[i], execCounter);    // insert in allocSet and add live set in dep list
            }

            IPF_LOG << "      " << left << setw(46) << IrPrinter::toString(inst); 
            IPF_LOG << " live set : " << IrPrinter::toString(liveSet) << endl; 
        }
    }
}

//----------------------------------------------------------------------------------------//
// 1. Remove opnd dependency on itself 
// 2. Make dependency matrix symmetric

void RegisterAllocator::makeInterferenceMatrixSymmetric() {

    for(RegOpndSetIterator it1=allocSet.begin(); it1!=allocSet.end(); it1++) {
        RegOpnd *opnd = *it1;
        opnd->getDepOpnds().erase(opnd);
        RegOpndSet &depOpnds = opnd->getDepOpnds();
        for(RegOpndSetIterator it2=depOpnds.begin(); it2!=depOpnds.end(); it2++) {
            (*it2)->insertDepOpnd(opnd);
        }
    }

    if(LOG_ON) { 
        IPF_LOG << endl << "  Opnds dependensies: " << endl; 
        for(RegOpndSetIterator it=allocSet.begin(); it!=allocSet.end(); it++) {
            RegOpnd *opnd = *it;
            IPF_LOG << "      " << setw(4) << left << IrPrinter::toString(opnd) << " depends on: ";
            RegOpndSet &depOpnds = opnd->getDepOpnds();
            IPF_LOG << IrPrinter::toString(depOpnds) << endl;
        }
    }
}
    
//----------------------------------------------------------------------------------------//
// 1. Creates out arg opngs list
// 2. Iterate through opnds having preassigned regs and notify their dep opnds that the reg 
//    has already been used
// 3. Remove opnds having preassigned regs from allocation cands list

void RegisterAllocator::removePreassignedOpnds() {

    for(RegOpndSetIterator it1=allocSet.begin(); it1!=allocSet.end();) {

        RegOpnd *opnd = *it1;
        int32   location = opnd->getLocation();
        
        // if opnd does not have preassigned location - ignore
        if(location == LOCATION_INVALID) { it1++; continue; }

        RegOpndSet& depOpnds = opnd->getDepOpnds();  // get opnds that depend on current one
        for(RegOpndSetIterator it2=depOpnds.begin(); it2!=depOpnds.end(); it2++) {
            (*it2)->markRegBusy(location);
        }
        
        // remove opnd
        IPF_LOG << "      remove " << IrPrinter::toString(opnd) << endl;
        it1++;
        allocSet.erase(opnd);
    }
}

//----------------------------------------------------------------------------------------//
// 1. Sort opnd list by Spill Cost
// 2. Assign locations to all not allocated opnds

void RegisterAllocator::assignLocations() {
    
    RegOpndVector opndVector(allocSet.begin(), allocSet.end());   // create vector of opns to be allocated
    sort(opndVector.begin(), opndVector.end(), greaterSpillCost); // sort them by Spill Cost
    
    for (uint16 i=0; i<opndVector.size(); i++) {
        RegOpnd *opnd = opndVector[i];
        IPF_LOG << "      " << left << setw(5) << IrPrinter::toString(opnd); 
        opndManager->assignLocation(opnd);  // assign location for current opnd
        IPF_LOG << " after assignment " << IrPrinter::toString(opnd) << endl; 
        
        if (opnd->isMem()) continue;        // if opnd assigned on stack - nothing more to do 
        
        RegOpndSet &depOpnds = opnd->getDepOpnds();
        int32      regNum    = opnd->getLocation();
        for (RegOpndSetIterator it=depOpnds.begin(); it!=depOpnds.end(); it++) {
            (*it)->markRegBusy(regNum);     // notify all dep opnds that they can not use this reg
        }
    }
}

//----------------------------------------------------------------------------------------//

void RegisterAllocator::updateAllocSet(Opnd *opnd, uint32 execCounter) {

    if (opnd->isReg()      == false) return;        // imm - it does not need allocation
    if (opnd->isMem()      == true)  return;        // mem stack - it does not need allocation
    if (opnd->isConstant() == true)  return;        // constant - it does not need allocation

    RegOpnd *regOpnd = (RegOpnd *)opnd;

    regOpnd->incSpillCost(execCounter);             // increase opnd spill cost 
    allocSet.insert(regOpnd);                       // isert opnd in list for allocation

    // add current live set in opnd dep list (they must be placed on different regs)
    for (RegOpndSetIterator it=liveSet.begin(); it!=liveSet.end(); it++) {
        regOpnd->insertDepOpnd(*it);
    }
}

//----------------------------------------------------------------------------------------//
// Check if current inst is "call" and mark all opnds in liveSet as crossing call site

void RegisterAllocator::checkCallSite(Inst *inst) {

    if(Encoder::isBranchCallInst(inst) == false) return; // opnd does not crass call site

    IPF_LOG << "      these opnds cross call site: ";
    IPF_LOG << IrPrinter::toString(liveSet) << endl;

    for(RegOpndSetIterator it=liveSet.begin(); it!=liveSet.end(); it++) {
        (*it)->setCrossCallSite(true);
    }
}

} // IPF
} // Jitrino
