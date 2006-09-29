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

#include "BitSet.h"
#include "IpfCodeLayouter.h"
#include "IpfIrPrinter.h"
#include "IpfOpndManager.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Compare two edges by prob value
//========================================================================================//

bool greaterEdge (Edge *e1, Edge *e2) { return e1->getProb() > e2->getProb(); }
 
//========================================================================================//
// CodeLayouter
//========================================================================================//

CodeLayouter::CodeLayouter(Cfg &cfg_) : 
    mm(cfg_.getMM()),
    cfg(cfg_) {
}

//----------------------------------------------------------------------------------------//

void CodeLayouter::layout() {

    IPF_LOG << endl << "  Merge Nodes" << endl;
    mergeNodes();

    IPF_LOG << endl << "  Make Chains" << endl;
    makeChains();
    
    IPF_LOG << endl << "  Fix Branches" << endl;
    fixBranches();
    
    IPF_STAT << endl << "STAT_NUM_NODES " << cfg.search(SEARCH_POST_ORDER).size() << endl;
}

//----------------------------------------------------------------------------------------//

void CodeLayouter::mergeNodes() {
    
    NodeVector& nodes      = cfg.search(SEARCH_DIRECT_ORDER);
    bool        cfgChanged = false;

    for (uint i=0; i<nodes.size(); i++) {  // iterate through nodes
        Node* node = nodes[i];

        if (LOG_ON) {
            IPF_LOG << "    node" << setw(2) << left << node->getId() << flush;
            node->printEdges(LOG_OUT);
        }
        
        // don't merge with enter node
        if (cfg.getEnterNode() == nodes[i]) {
            IPF_LOG << " - ignore (don't merge with enter node)" << endl;
            continue;
        }
        
        // check node has only one successor (not taking in account unwind node)
        EdgeVector& outEdges = node->getOutEdges();
        if (outEdges.size() != 1) {
            IPF_LOG << " - ignore (has more than one successor)" << endl;
            continue;
        }
        
        // check if node is basic block 
        if (node->getNodeKind() != NODE_BB) { 
            IPF_LOG << " - ignore (is not basic block)" << endl;
            continue;
        }
        
        // don't merge with enter node
        if (cfg.getEnterNode() == outEdges[0]->getTarget()) {
            IPF_LOG << " - ignore (don't merge with enter node)" << endl;
            continue;
        }
        
        // don't merge with enter node
        if (outEdges[0]->getTarget()->getNodeKind() != NODE_BB) {
            IPF_LOG << " - ignore (target is not basic block)" << endl;
            continue;
        }
        
        cfgChanged |= mergeNode((BbNode*)node, outEdges[0]);
    }
    
    if (cfgChanged) {
        cfg.search(SEARCH_UNDEF_ORDER); // we have removed some nodes - old search is broken
    }
}
    
//----------------------------------------------------------------------------------------//

bool CodeLayouter::mergeNode(BbNode *pred, Edge* pred2succ) {    
    BbNode*  succ = (BbNode*)pred2succ->getTarget(); 
    IPF_LOG << " - try to merge with node" << setw(2) << left;
    IPF_LOG << succ->getId() << flush;    
    if (succ->getNodeKind() != NODE_BB) { // if succ is NODE_DISPATCH - ignore
        IPF_LOG << " - succ is not NODE_BB - ignore" << endl;
        return false;
    }

    InstVector &predInsts = pred->getInsts();
    if (predInsts.size() != 0) {  // checks applicaple to non-empty blocks:
        // do not merge non-empty node with exit node
         if (succ == cfg.getExitNode()) {
            IPF_LOG << " - merge canceled (succ is Exit Node)" << endl;
             return false;
        }
        // if Succ has predecessors other than pred - return
        if (succ->getInEdges().size()!=1) {
            IPF_LOG << " - succInEdges.size()!=1" << endl;
            return false;
        }
        // insert Succ instructions after Pred instructions
        InstVector &succInsts = succ->getInsts();
        for (uint i=0, succsize = succInsts.size(); i<succsize; i++) {
            predInsts.push_back(succInsts[i]);
        }
        // move combined instruction set to Succ
        succInsts.swap(predInsts);
    }  // end if non-empty block
    
     // remove Pred's out-edge
       pred2succ->disconnect();
    // reconnect Pred's in-edges to Succ 
    EdgeVector &predInEdges = pred->getInEdges();
    while (predInEdges.size()>0) {
        predInEdges[predInEdges.size()-1]->connect(succ);
    }
    // if Pred is enter node - set Succ as new enter node
    if (pred == cfg.getEnterNode()) {
        cfg.setEnterNode(succ);
        IPF_LOG << " - new enter";
    }
    
    IPF_LOG << " - merge successful" << endl;
    return true;
}

//----------------------------------------------------------------------------------------//

void CodeLayouter::makeChains() {

    uint maxNodeId=cfg.getMaxNodeId();
    BbNode **availNodes=new(mm) BbNode*[maxNodeId];
    for(uint i=0; i<maxNodeId; i++) {
        availNodes[i] = NULL;
    }
    
    NodeVector& nodes = cfg.search(SEARCH_DIRECT_ORDER);   // actually, order does not matter
    for(uint i=0; i<nodes.size(); i++) {
        Node* node=nodes[i];
        if (node->getNodeKind() != NODE_BB) continue; // ignore not BB nodes
        availNodes[node->getId()] = (BbNode*)node;
    }
    
    BbNode *currNode = (BbNode*) cfg.getEnterNode();  // start from the Enter Node
    uint availPos=0; // where to look for next available node
    IPF_LOG << "    new chain:";
    for (;;) {
        availNodes[currNode->getId()] = NULL;  // mark curr node consumed
        IPF_LOG << " node" << currNode->getId() << "->";
        
        BbNode* nextNode=NULL;

        // find out edge with max prob 
        double currProb            = -10.0;
        EdgeVector& outEdges = currNode->getOutEdges();
        for(uint32 j=0; j<outEdges.size(); j++) {
            BbNode* candidate = (BbNode*) outEdges[j]->getTarget();
            if (availNodes[candidate->getId()] == NULL) continue; // ignore consumed nodes
            if (currProb < outEdges[j]->getProb()) {          // we have found edge with prob greater then curr
                currProb        = outEdges[j]->getProb();    // reset curr max prob 
                nextNode         = candidate;                  // curr cand to be placed in chain
            }
        }
        if (nextNode==NULL) {
            IPF_LOG << endl;  // current chain finished
            // try to start next chain
            for (; availPos<maxNodeId; availPos++) {
                nextNode = availNodes[availPos];
                if (nextNode!=NULL) {
                    break;
                }
            }
            if (nextNode==NULL) {
                return;  // no more nodes available
            }
            IPF_LOG << "    new chain:";
        }
        currNode->setLayoutSucc(nextNode);
        currNode=nextNode;
    }
}
//----------------------------------------------------------------------------------------//

void CodeLayouter::fixBranches() {
    
    BbNode *node = (BbNode*) cfg.getEnterNode();

    for(; node != NULL; node = node->getLayoutSucc()) {

        IPF_LOG << "    node" << setw(2) << left << node->getId();
        InstVector& insts = node->getInsts();

        // check if last inst is conditional branch
        if(insts.size() != 0) {
            CompVector& compList = insts.back()->getComps();
            if(compList.size()>0 && compList[0] == CMPLT_BTYPE_COND) {
                IPF_LOG << " fix conditional branch" << endl;
                fixConditionalBranch(node);
                continue;
            }
        }
        
        // check if last inst is ret
        if(insts.size() != 0) {
            CompVector& compList = insts.back()->getComps();
            if(compList.size()>0 && compList[0] == CMPLT_BTYPE_RET) {
                IPF_LOG << " last inst is \"ret\"" << endl;
                continue;
            }
        }
        
        // check if last inst is switch
        if(insts.size() !=0) {
            if(insts.back()->getInstCode() == INST_SWITCH) {
                IPF_LOG << " fix switch" << endl;
                fixSwitch(node);
                continue;
            }
        }

        // thus, it is unconditional branch
        IPF_LOG << " fix unconditional branch" << endl;
        fixUnconditionalBranch(node);
    }
    cfg.search(SEARCH_UNDEF_ORDER); // it is possible that we have removed some nodes - old search is broken
}


//----------------------------------------------------------------------------------------//
// (p0)   cmp4.ge     p8, p0 = r32, r2
// (p8)   br          unknown target
//
// Sets branch target accordingly 

void CodeLayouter::fixConditionalBranch(BbNode *node) {
    
    InstVector& insts      = node->getInsts();
    Inst*       branchInst = insts.back();

    Edge *branchEdge  = node->getOutEdge(EDGE_BRANCH);
    Edge *throughEdge = node->getOutEdge(EDGE_THROUGH);

    IPF_LOG << "        old branch target is node" << branchEdge->getTarget()->getId() << endl;
    
    // check if branch edge target coinsides with layout successor
    if (branchEdge->getTarget() == node->getLayoutSucc()) {
        // swap fall through and branch edges            
        throughEdge->setEdgeKind(EDGE_BRANCH);
        branchEdge->setEdgeKind(EDGE_THROUGH);
        Edge *tmpEdge=throughEdge;
        throughEdge=branchEdge; branchEdge=tmpEdge;
        
        // swap predicate registers of the "cmp" instruction
        Inst* cmpInst = *(insts.end() - 2);         // get "cmp" inst
        Opnd* p1 = cmpInst->getOpnd(POS_CMP_P1);    // get p1 opnd
        Opnd* p2 = cmpInst->getOpnd(POS_CMP_P2);    // get p2 opnd
        cmpInst->setOpnd(POS_CMP_P1, p2);           // set p2 on p1's position
        cmpInst->setOpnd(POS_CMP_P2, p1);           // set p1 on p2's position
    }

    // Set target for branch instruction
    NodeRef *targetOpnd = (NodeRef*)branchInst->getOpnd(POS_BR_TARGET);
    targetOpnd->setNode((BbNode*)branchEdge->getTarget());

    IPF_LOG << "        new branch target is node" << branchEdge->getTarget()->getId() << endl;
    
    // If through edge target coinsides with layout successor - do nothing
    BbNode* target=(BbNode*) throughEdge->getTarget();
    if (target == node->getLayoutSucc()) {
        IPF_LOG << "        through edge coinsides with layout successor" << endl;
        return;
    }
    IPF_LOG << "        through edge points to" << target->getId() << endl;

    // add intermediate basic block for unconditional branch
    BbNode *intNode = new(mm) BbNode(cfg.getNextNodeId(), 0xFFFF);  //-1?TBD
    IPF_LOG << "        generate intermediate node" << intNode->getId() << endl;
    intNode->setLayoutSucc(node->getLayoutSucc());
    node->setLayoutSucc(intNode);
//    nodes.push_back(intNode);
    throughEdge->connect(intNode);
     Edge *intEdge = new(mm) Edge(intNode, target, throughEdge->getProb(), EDGE_BRANCH);
    cfg.addEdge(intEdge);
    cfg.search(SEARCH_UNDEF_ORDER); // old search is broken
 
     // Add branch instruction
    OpndManager*  opndManager = cfg.getOpndManager();
    Opnd*    p0         = opndManager->getP0();
    NodeRef* targetNodeRef = opndManager->newNodeRef(target);
    intNode->addInst(new(mm) Inst(INST_BR, p0, targetNodeRef));  
}
    
//----------------------------------------------------------------------------------------//

void CodeLayouter::fixSwitch(BbNode* node) {
    
    Inst* lastInst = node->getInsts().back();

    // Find edge corresponding to layout successor and mark it fall through
    Edge *throughEdge = node->getOutEdge((Node*) node->getLayoutSucc());
    throughEdge->setEdgeKind(EDGE_THROUGH);
    
    Opnd           *troughTargetImm   =                   lastInst->getOpnd(POS_SWITCH_THROUGH);
    ConstantRef    *constantRef       = (ConstantRef*)    lastInst->getOpnd(POS_SWITCH_TABLE);
    SwitchConstant *switchConstant    = (SwitchConstant*) constantRef->getConstant();
    
    // Find out which switch choice corresponds to fall through edge
    uint16 throughChoice = switchConstant->getChoice(throughEdge);

    // Set imm representing fall through choice
    troughTargetImm->setValue(throughChoice);
    
    // We do not need switch opnds in switch instruction any more. Remove them
    lastInst->removeLastOpnd();
    lastInst->removeLastOpnd();
    lastInst->removeLastOpnd();
    lastInst->setInstCode(INST_BR);
}
    
//----------------------------------------------------------------------------------------//

void CodeLayouter::fixUnconditionalBranch(BbNode *node) {
    
    Edge* throughEdge = node->getOutEdge(EDGE_THROUGH);
    if(throughEdge == NULL) { // there is no through edge - nothing to do
        IPF_LOG << "        there is no through edge" << endl;
        return; 
    }

    // If through edge target coinsides with layout successor - do nothing
    BbNode* target=(BbNode*) throughEdge->getTarget();
    if (target == (Node*) node->getLayoutSucc()) {
        IPF_LOG << "        through edge coinsides with layout successor" << endl;
        return;
    }

    // Add branch to through edge target
    Opnd*    p0         = cfg.getOpndManager()->getP0();
    NodeRef* targetNode = cfg.getOpndManager()->newNodeRef(target);
    node->addInst(new(mm) Inst(INST_BR, p0, targetNode));
    
    throughEdge->setEdgeKind(EDGE_BRANCH);
}
    
} // IPF
} // Jitrino
