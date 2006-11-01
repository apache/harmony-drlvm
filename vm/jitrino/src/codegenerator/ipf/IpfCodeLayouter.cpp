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

#include "IpfCodeLayouter.h"
#include "IpfIrPrinter.h"
#include "IpfOpndManager.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Compare two edges by prob value
//========================================================================================//

bool greaterEdge(Edge *e1, Edge *e2) { return e1->getProb() > e2->getProb(); }
 
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
    checkUnwind();

    IPF_LOG << endl << "  Make Chains" << endl;
    makeChains();
    layoutNodes();
    
    IPF_LOG << endl << "  Set Branch Targets" << endl;
    setBranchTargets();
    
    IPF_STAT << endl << "STAT_NUM_NODES " << cfg.search(SEARCH_POST_ORDER).size() << endl;
}

//----------------------------------------------------------------------------------------//

void CodeLayouter::mergeNodes() {
    
    NodeVector& nodeVector = cfg.search(SEARCH_POST_ORDER);
    NodeList    nodes(nodeVector.begin(), nodeVector.end());

    // try to merge current node with its successor
    for (NodeListIterator it=nodes.begin(); it!=nodes.end(); it++) {
        
        Node *node = *it;
        if (node->getNodeKind() != NODE_BB) continue; // node is not BB - ignore
        if (node == cfg.getEnterNode())     continue; // do not merge enter node
        
        BbNode *pred = (BbNode *)node;                // let's name current node "pred"
        BbNode *succ = getSucc(pred);                 // check if it has mergable successor
        if (succ == NULL)              continue;      // current node does not have mergable successor
        if (succ == cfg.getExitNode()) continue;      // do not merge exit node
        if (checkSucc(succ) == false)  continue;      // succ can not be merged with pred
        
        merge(pred, succ);                            // merge pred and succ nodes
        nodes.remove(succ);                           // remove succ node from current nodes list
    }
    cfg.search(SEARCH_UNDEF_ORDER);                   // we could remove some nodes - old search is broken
}

//----------------------------------------------------------------------------------------//
// if node has only one succ (not taking in account unwind) - return the succ

BbNode* CodeLayouter::getSucc(BbNode *node) {
    
    Node       *succ     = NULL;
    EdgeVector &outEdges = node->getOutEdges();

    for (uint16 i=0; i<outEdges.size(); i++) {
        Node *target = outEdges[i]->getTarget();            // get successor of current node
        if (target->getNodeKind() == NODE_UNWIND) continue; // if it is unwind node - ignore
        if (succ != NULL) return NULL;                      // there is more than one succ - node can not be merged
        succ = target;                                      // it is first succ
    }
    
    if (succ == NULL)                   return NULL;        // there is no successor
    if (succ->getNodeKind() != NODE_BB) return NULL;        // if succ is not BB - node can not be merged
    return (BbNode *)succ;
}

//----------------------------------------------------------------------------------------//
// check if succ can be merged with pred (has only one pred)

bool CodeLayouter::checkSucc(BbNode *node) {

    EdgeVector &inEdges = node->getInEdges();   // get succ in edges
    if (inEdges.size() > 1) return false;       // succ has more than one pred - it can not be merged
    return true;
}
    
//----------------------------------------------------------------------------------------//
// merge two nodes

void CodeLayouter::merge(BbNode *pred, BbNode *succ) {
    
    // copy succ's insts in pred node
    InstVector &predInsts = pred->getInsts();
    InstVector &succInsts = succ->getInsts();
    predInsts.insert(predInsts.end(), succInsts.begin(), succInsts.end());
    
    // remove pred out edges
    EdgeVector predOutEdges = pred->getOutEdges();    // make copy of pred out edges vector
    for (uint16 i=0; i<predOutEdges.size(); i++) {    // iterate edges
        predOutEdges[i]->remove();                    // remove edge
    }
    
    // redirect succ's out edges on pred
    EdgeVector succOutEdges = succ->getOutEdges();    // make copy of succ out edges vector
    for (uint16 i=0; i<succOutEdges.size(); i++) {    // iterate edges
        succOutEdges[i]->changeSource(pred);          // redirect edge
    }
    
    IPF_LOG << "    node" << left << setw(3) << pred->getId() << " merged with node" << succ->getId() << endl;

    if (LOG_ON) {
        if (succ->getInEdges().size()  != 0) IPF_ERR << " size " << succ->getInEdges().size() << endl;
        if (succ->getOutEdges().size() != 0) IPF_ERR << " size " << succ->getOutEdges().size() << endl;
    }
}
        
//----------------------------------------------------------------------------------------//
// if unwind node does not have predecessors (they could be removed during merging) - it can 
// be removed

void CodeLayouter::checkUnwind() {
    
    // find unwind node (it must be predecessor of exit node)
    Node       *unwind  = NULL;
    Node       *exit    = cfg.getExitNode();
    EdgeVector &inEdges = exit->getInEdges();            // get exit node in edges
    for (uint16 i=0; i<inEdges.size(); i++) {            // iterate them
        unwind = inEdges[i]->getSource();                // get edge source
        if (unwind->getNodeKind() == NODE_UNWIND) break; // if the source is unwind node - we have found it
    }
    
    // check if unwind can be removed
    if (unwind == NULL)                  return;         // there is no unwind node
    if (unwind->getInEdges().size() > 0) return;         // unwind node is alive - nothind to do
    
    // remove useless unwind
    unwind->remove();
    IPF_LOG << endl << "    unwind node removed" << endl;
}

//----------------------------------------------------------------------------------------//

void CodeLayouter::makeChains() {

    // make edge list
    EdgeVector edges;
    NodeVector &nodes = cfg.search(SEARCH_POST_ORDER);               // get nodes vector
    for (uint16 i=0; i<nodes.size(); i++) {                          // iterate throgh it
        EdgeVector &outEdges = nodes[i]->getOutEdges();              // get out edges of current node
        edges.insert(edges.end(), outEdges.begin(), outEdges.end()); // add them in edges list
    }
    
    // sort edge list by prob
    sort(edges.begin(), edges.end(), greaterEdge);
    
    // make chain list
    for (uint16 i=0; i<edges.size(); i++) inChainList(edges[i]);
}

//----------------------------------------------------------------------------------------//
// if there is chain that can be connected with the edge - connect, else - create new chain

void CodeLayouter::inChainList(Edge *edge) {
    
    Node  *sourceNode  = edge->getSource();
    Node  *targetNode  = edge->getTarget();
    Chain *targetChain = NULL;
    Chain *sourceChain = NULL;

    // try to find chains to add current edge in
    for (ChainListIterator it=chains.begin(); it!=chains.end(); it++) {
        if ((*it)->front() == targetNode) targetChain = *it;
        if ((*it)->back()  == sourceNode) sourceChain = *it;
    }
    
    if (targetChain!=NULL && sourceChain!=NULL) {               // edge connects two existing chains
        if (targetChain == sourceChain) return;                 // do not merge chain with itself
        sourceChain->splice(sourceChain->end(), *targetChain);  // merge chains in source chain
        chains.remove(targetChain);                             // erase target chain
        return;
    }
    
    if (sourceChain != NULL) {                                  // sourceChain ending with edge source
        pushBack(sourceChain, targetNode);                      // push target back in sourceChain
        return;
    }
    
    if (targetChain != NULL) {                                  // targetChain starting with edge target 
        pushFront(targetChain, sourceNode);                     // push source front in targetChain
        return;
    }

    // there is no chain that can be merged with the edge 
    Chain *newChain = new Chain();                              // create new chain
    pushBack(newChain, sourceNode);                             // push source back in new chain
    pushBack(newChain, targetNode);                             // push target back in new chain
    if (newChain->size() > 0) chains.push_back(newChain);       // insert new chain in chain list
}

//----------------------------------------------------------------------------------------//
// push node in chain end

void CodeLayouter::pushBack(Chain *chain, Node *node) {
    if (visitedNodes.count(node) != 0) return; // node has already been inserted in some chain
    visitedNodes.insert(node);                 // mark node as visited
    chain->push_back(node);                    // push node back in the chain
}

//----------------------------------------------------------------------------------------//
// push node in chain begining

void CodeLayouter::pushFront(Chain *chain, Node *node) {
    if (visitedNodes.count(node) != 0) return; // node has already been inserted in some chain
    visitedNodes.insert(node);                 // mark node as visited
    chain->push_front(node);                   // push node front in the chain
}

//----------------------------------------------------------------------------------------//
// set layout successors for BbNodes

void CodeLayouter::layoutNodes() {
    
    // sort chains 
    ChainMap order;
    for (ChainListIterator it=chains.begin(); it!=chains.end(); it++) {
        uint32 weight = calculateChainWeight(*it);  // calculate chain weight
        order.insert( make_pair(weight, *it) );     // insert pair weight->chain in map
    }

    // set layout successors for BbNodes
    BbNode *pred = new(mm) BbNode(0, 0);            // current pred node (init with fake node)
    BbNode *succ = NULL;                            // currend succ node
    for (ChainMapIterator it1=order.begin(); it1!=order.end(); it1++) {
        Chain *chain = it1->second;                 // current chain
        IPF_LOG << "    weight: " << setw(10) << it1->first;
        IPF_LOG << " chain: " << IrPrinter::toString(*(it1->second)) << endl;
        
        for (ChainIterator it2=chain->begin(); it2!=chain->end(); it2++) {
            if ((*it2)->isBb() == false) continue;  // if current node is not BB - it does not need layouting
            succ = (BbNode *)*it2;                  //
            pred->setLayoutSucc(succ);              // set current node as layouted successor of pred
            pred = succ;                            // current pred is current node
        }
    }
}

//----------------------------------------------------------------------------------------//
// chain weight is exec counters summ of all nodes in the chain

uint32 CodeLayouter::calculateChainWeight(Chain *chain) {

    if (chain->front() == cfg.getEnterNode()) return UINT_MAX;  // enter node always goes first
    
    uint32 weight = 0;
    for (ChainIterator it=chain->begin(); it!=chain->end(); it++) {
        if ((*it)->isBb() == false) continue;
        BbNode *node = (BbNode *) *it;
        weight += node->getExecCounter();
    }
    return weight;
}

//----------------------------------------------------------------------------------------//
// branch targets have not been set yet. In this method we iterate through each node and 
// check if it ends with branch (needs branch target)

void CodeLayouter::setBranchTargets() {
    
    BbNode *node = (BbNode*) cfg.getEnterNode();
    for(; node != NULL; node = node->getLayoutSucc()) {

        IPF_LOG << "    node" << left << setw(3) << node->getId();
        InstVector& insts = node->getInsts();

        if(insts.size() != 0) {
            Inst *lastInst = insts.back();

            if (lastInst->isRet()) {
                IPF_LOG << " last inst is \"ret\"" << endl;
                continue;
            }

            if (lastInst->isConditionalBranch() && lastInst->getComps().size() != 0) {
                IPF_LOG << " fix conditional branch:";
                fixConditionalBranch(node);
                continue;
            }

            if(lastInst->getInstCode() == INST_SWITCH) {
                IPF_LOG << " fix switch" << endl;
                fixSwitch(node);
                continue;
            }
        }
        
        // thus, it is unconditional branch
        IPF_LOG << " fix unconditional branch:";
        fixUnconditionalBranch(node);
    }
}


//----------------------------------------------------------------------------------------//
// set conditional branch target 

void CodeLayouter::fixConditionalBranch(BbNode *node) {
    
    InstVector &insts       = node->getInsts();
    Inst       *branchInst  = insts.back();
    Edge       *branchEdge  = node->getOutEdge(EDGE_BRANCH);
    Edge       *throughEdge = node->getOutEdge(EDGE_THROUGH);

    // if branch edge target coinsides with layout successor
    if (branchEdge->getTarget() == node->getLayoutSucc()) {
        // swap fall through and branch edges            
        throughEdge->setEdgeKind(EDGE_BRANCH);
        branchEdge->setEdgeKind(EDGE_THROUGH);
        Edge *tmpEdge = throughEdge;
        throughEdge = branchEdge; 
        branchEdge  = tmpEdge;
        
        // swap predicate registers of the "cmp" instruction
        Inst* cmpInst = *(insts.end() - 2);         // get "cmp" inst (it must stay right before "br")
        Opnd* p1 = cmpInst->getOpnd(POS_CMP_P1);    // get p1 opnd
        Opnd* p2 = cmpInst->getOpnd(POS_CMP_P2);    // get p2 opnd
        cmpInst->setOpnd(POS_CMP_P1, p2);           // set p2 on p1's position
        cmpInst->setOpnd(POS_CMP_P2, p1);           // set p1 on p2's position
        
        IPF_LOG << " branch retargeted,";
    }

    BbNode *branchTargetNode = (BbNode *)branchEdge->getTarget();
    BbNode *fallThroughNode  = (BbNode *)throughEdge->getTarget();
    BbNode *layoutSuccNode   = (BbNode *)node->getLayoutSucc();

    // Set target for branch instruction
    NodeRef *targetOpnd = (NodeRef *)branchInst->getOpnd(POS_BR_TARGET);
    targetOpnd->setNode(branchTargetNode);

    IPF_LOG << " branch target is node" << branchTargetNode->getId() << endl;
    
    // if fall through node coinsides with layout successor - noting more to do
    if (fallThroughNode == layoutSuccNode) return;
    
    // create new node for unconditional branch on through edge target node
    BbNode *branchNode = new(mm) BbNode(cfg.getNextNodeId(), fallThroughNode->getExecCounter());
    branchNode->setLayoutSucc(layoutSuccNode); // layout successor of current node becomes layoute successor of new node
    node->setLayoutSucc(branchNode);           // the new node becomes layout successor of current node

    throughEdge->changeTarget(branchNode);     // retarget trough edge on the new node
    Edge *edge = new(mm) Edge(branchNode, fallThroughNode, throughEdge->getProb(), EDGE_THROUGH);
    edge->insert();                            // new edge connects the new node and fall through node

    IPF_LOG << ", through node generated: node" << branchNode->getId() << endl;
    cfg.search(SEARCH_UNDEF_ORDER);            // old search is broken
}
    
//----------------------------------------------------------------------------------------//

void CodeLayouter::fixSwitch(BbNode* node) {
    
    Inst* lastInst = node->getInsts().back();

    // Find edge corresponding to layout successor and mark it fall through
    Edge *throughEdge = node->getOutEdge(node->getLayoutSucc());
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
    
    // if there is no through edge - do nothing
    Edge *throughEdge = node->getOutEdge(EDGE_THROUGH);
    if(throughEdge == NULL) { 
        IPF_LOG << " there is no through edge - ignore" << endl;
        return; 
    }

    // if through edge target coinsides with layout successor - do nothing
    BbNode *target = (BbNode *)throughEdge->getTarget();
    if (target == node->getLayoutSucc()) {
        IPF_LOG << " through edge coinsides with layout successor - ignore" << endl;
        return;
    }

    // Add branch to through edge target
    Opnd    *p0         = cfg.getOpndManager()->getP0();
    NodeRef *targetNode = cfg.getOpndManager()->newNodeRef(target);
    node->addInst(new(mm) Inst(INST_BR, CMPLT_BTYPE_COND, p0, targetNode));
    
    throughEdge->setEdgeKind(EDGE_BRANCH);
    IPF_LOG << " branch on node" << target->getId() << " added" << endl;
}
    
} // IPF
} // Jitrino
