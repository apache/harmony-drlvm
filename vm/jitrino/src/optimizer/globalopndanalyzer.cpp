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
 * @version $Revision: 1.11.24.4 $
 *
 */

#include "globalopndanalyzer.h"
#include "irmanager.h"
#include "FlowGraph.h"
#include "Dominator.h"
#include "Inst.h"
#include "Loop.h"
#include "BitSet.h"
#include "Log.h"

namespace Jitrino {

DEFINE_OPTPASS_IMPL(GlobalOperandAnalysisPass, markglobals, "Basic Block based Global Operand Analysis")

void
GlobalOperandAnalysisPass::_run(IRManager& irm) {
    computeDominators(irm);
    computeLoops(irm);
    AdvancedGlobalOpndAnalyzer globalOpndAnalyzer(irm, *irm.getLoopTree());
    globalOpndAnalyzer.doAnalysis();
}

//
//  Get list of nodes in postorder
//
void
GlobalOpndAnalyzer::getNodesInPostorder() {
    nodes.reserve(flowGraph.getMaxNodeId());
    flowGraph.getNodesPostOrder(nodes);
}

//
//  Reset global bits 
//
void
GlobalOpndAnalyzer::resetGlobalBits() {
    ::std::vector<CFGNode*>::iterator niter;
    for (niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        Inst *headInst = node->getFirstInst();
        for (Inst* inst = headInst->next();inst!=headInst;inst=inst->next()) {
            //
            // SsaVarOpnds and VarOpnds are always global; we care only about
            // SsaTmpOpnds.
            //
            Opnd* dst = inst->getDst();
            if (dst->isSsaVarOpnd() || dst->isVarOpnd()) {
                dst->setIsGlobal(true);
                continue;
            }
            dst->setIsGlobal(false);
        }
    }
}

//
//  Mark temporaries whose live range spans basic block boundary as globals
//
void 
GlobalOpndAnalyzer::markGlobals() {
    //
    // Now walk instructions in postorder, marking sources that have not been
    // visited as global.
    //
    uint32 numInsts = irManager.getInstFactory().getNumInsts();
    MemoryManager memManager(numInsts/8+4,"GlobalOpndAnalyzer::doAnalysis()");
    BitSet markedInstSet(memManager,numInsts);
    ::std::vector<CFGNode*>::iterator niter;
    for (niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        Inst* headInst = node->getFirstInst();
        for (Inst* inst = headInst->next();inst!=headInst;inst=inst->next()) {
            // mark as visited
            markedInstSet.setBit(inst->getId(),true);
            // check sources to see if any span globally
            for (uint32 i=0; i<inst->getNumSrcOperands(); i++) {
                Opnd* src = inst->getSrc(i);
                //
                // we care only about srcs that are temporary
                //
                if (src->isSsaTmpOpnd() == false)
                    continue;
                //
                // if the instruction defining this source has not been marked,
                // then the opnd must be global because of the post order walk
                //
                if (markedInstSet.getBit(src->getInst()->getId()) == false) {
                    src->setIsGlobal(true);
                    if(Log::cat_opt()->isDebugEnabled())  {
                        Log::out() << "XXX - GlobalOpnd:";src->getInst()->print(Log::out()); Log::out() << ::std::endl;
                    }
                }
            }
        }
    }
}

void
GlobalOpndAnalyzer::doAnalysis() {
    getNodesInPostorder();
    resetGlobalBits();
    // mark temporaries whose live spans basic block boundary as globals
    markGlobals();   
}

bool
AdvancedGlobalOpndAnalyzer::cfgContainsLoops() {
    ::std::vector<CFGNode*>::iterator niter;
    for (niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        if (loopInfo.isLoopHeader(node))
            return true;
    }
    return false;
}


//
//  Information about a global operand
//
struct AdvancedGlobalOpndAnalyzer::OpndInfo {
    OpndInfo(uint32 h, uint32 ts) : header(h), timeStamp(ts), isGlobal(false) {
    }
    uint32    header;       // dfNum of a header where node is used.
    uint32    timeStamp;
    bool      isGlobal;
};

//
//  Hash table with the global operand information
//
class AdvancedGlobalOpndAnalyzer::OpndTable : public HashTable<Opnd,OpndInfo> {
public:
    OpndTable(MemoryManager& mm, uint32 size) : 
        HashTable<Opnd,OpndInfo>(mm,size), memManager(mm), lastHeaderTimeStamp(0) {
    }
    void clear() {
        lastHeaderTimeStamp = 0;
        removeAll();
    }
    void recordUse(Opnd * use, uint32 loopHeader, uint32 timeStamp) {
        OpndInfo * info = lookup(use);
        if (info == NULL)
            insertOpnd(use, loopHeader, timeStamp);
        else {
            //
            //  If previous use had different loop header, temp is global
            //
            if (info->header != loopHeader)
                info->isGlobal = true;
        }
    }
    void recordDef(Opnd * def, uint32 loopHeader, uint32 timeStamp) {
        OpndInfo * info = lookup(def);
        //
        //  Operand may be not in the table if it's definition is inside
        //  the loop and use is outside the loop, or if it's dead. 
        //  Insert it into a table
        //
        if (info == NULL) 
            insertOpnd(def,loopHeader,timeStamp);
        else {
            //
            //  If loop header of the definition is not equal loop header of
            //  the use, operand is global
            //
            if (info->header != loopHeader)
                info->isGlobal = true;
            //
            //  If we saw a loop header between use and definition operand 
            //  may be global.
            //
            else if (info->timeStamp < lastHeaderTimeStamp)
                info->isGlobal = true;
        }
    }
    void recordLoopHeader(uint32 timeStamp) {
        lastHeaderTimeStamp = timeStamp;
    }
private:
    virtual bool keyEquals(Opnd* key1,Opnd* key2) const {
        return key1 == key2;
    }
    virtual uint32 getKeyHashCode(Opnd* key) const {
        return key->getId();
    }
    void insertOpnd(Opnd * opnd, uint32  header, uint32 timeStamp) {
        insert(opnd, new(memManager) OpndInfo(header,timeStamp));
    }
    MemoryManager& memManager;
    uint32  lastHeaderTimeStamp;
};
    
void
AdvancedGlobalOpndAnalyzer::markManagedPointerBases() {
    //
    //  Walk nodes in postorder
    //
    ::std::vector<CFGNode*>::iterator niter;
    for (niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        //
        //  Walk instructions in reverse order
        //
        Inst* headInst = node->getFirstInst();
        for (Inst* inst = headInst->prev(); inst!=headInst; inst=inst->prev()) {       
            Opcode opcode = inst->getOpcode();
            // If the managed pointer generated by an instruction is global, its source 
            // should also be marked as global.  (Though, technically only if the managed
            // pointer is live across a GC safe point.  We don't make this further
            // distinction.)
            switch(opcode) {
            case Op_LdVarAddr:
                break;
            case Op_LdLockAddr:
            case Op_LdFieldAddr: 
            case Op_LdElemAddr:
            case Op_LdArrayBaseAddr:
            case Op_AddScaledIndex:
		{
                Opnd* src = inst->getSrc(0);
                Opnd* dst = inst->getDst();
                if(!src->isGlobal() && dst->isGlobal()) {
                    assert(dst->getType()->isManagedPtr());
                    src->setIsGlobal(true);
                }
		};
                break;
	    default:
		break;
            }
        }
    }
}
    
void
AdvancedGlobalOpndAnalyzer::unmarkFalseGlobals() {
    MemoryManager localMemManager(1024,
       "RefinedGlobalOpndAnalyzer::doAnalysis.localMemManager");
    opndTable = new (localMemManager) OpndTable(localMemManager, 16);
    uint32 timeStamp = 0;
    //
    //  Walk nodes in postorder
    //
    ::std::vector<CFGNode*>::iterator niter;
    for (niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        timeStamp++;    
        //
        //  Figure out the current loop header
        //
        uint32 dfNum = node->getDfNum();
        uint32 currHeader = loopInfo.isLoopHeader(node) ? dfNum
                          : loopInfo.hasContainingLoopHeader(node) ? loopInfo.getContainingLoopHeader(node)->getDfNum()
                          : (uint32)-1;  
        //
        //  Walk instructions in reverse order
        //
        Inst* headInst = node->getFirstInst();
        for (Inst* inst = headInst->prev(); inst!=headInst; inst=inst->prev()) {
            //
            // Analyze sources 
            //
            for (uint32 i=0; i<inst->getNumSrcOperands(); i++) {
                Opnd* src = inst->getSrc(i);
                //
                // We care only about global tmps
                //
                if (src->isSsaTmpOpnd() == true && src->isGlobal() == true)
                    opndTable->recordUse(src, currHeader,timeStamp);
            }
            //
            //  Analyze destination
            //
            Opnd * dst = inst->getDst();
            //
            //  We care only about global temps
            //
            if (dst->isSsaTmpOpnd() == true && dst->isGlobal() == true) 
                opndTable->recordDef(dst,currHeader, timeStamp);

        }
        //
        //  If this node is a loop header record it in operand table
        //
        if (loopInfo.isLoopHeader(node))
            opndTable->recordLoopHeader(timeStamp);
    }
    //
    //  Mark global operands that we found to be actually local as local
    //
    HashTableIter<Opnd, OpndInfo> tableIter(opndTable);
    Opnd *     opnd;
    OpndInfo * info;
    while (tableIter.getNextElem(opnd,info)) {
        if (info->isGlobal == false) {
            opnd->setIsGlobal(false);
            if(Log::cat_opt()->isDebugEnabled())  {
                Log::out() << "XXX - Not a Global Opnd:";
                opnd->print(Log::out()); 
                Log::out() << ::std::endl;
            }
        }
    }
}


void
AdvancedGlobalOpndAnalyzer::markGlobals() {
    //
    //  If CFG does not contain any loops there are no global temporaries
    //
    if (!cfgContainsLoops())
        return;
    //
    //  Mark temporaries whose live spans basic block as globals
    //
    GlobalOpndAnalyzer::markGlobals();
    //
    //  Unmark global temporaries whose live range does not span loop boundary
    //
    unmarkFalseGlobals();

    //
    //  Mark references that generate global managed pointers as global also. 
    //
    markManagedPointerBases();
}



} //namespace Jitrino 
