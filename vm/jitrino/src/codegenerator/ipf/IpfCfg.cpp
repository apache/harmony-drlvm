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

#include "IpfCfg.h"
#include "IpfOpndManager.h"
#include "IpfIrPrinter.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// Edge
//========================================================================================//

Edge::Edge(Node *source_, Node *target_, double prob_, EdgeKind edgeKind_) : 
    edgeKind(edgeKind_),
    source(source_), 
    target(target_), 
    prob(prob_) {
}

//----------------------------------------------------------------------------------------//

void Edge::remove() {
    
    source->removeEdge(this);
    target->removeEdge(this);
}

//----------------------------------------------------------------------------------------//

void Edge::insert() {

    source->addEdge(this);
    target->addEdge(this);
}
    
//----------------------------------------------------------------------------------------//

void Edge::changeSource(Node *source_) {
    
    source->removeEdge(this);
    source = source_;
    source->addEdge(this);
}

//----------------------------------------------------------------------------------------//

void Edge::changeTarget(Node *target_) {
    
    target->removeEdge(this);
    target = target_;
    target->addEdge(this);
}

//----------------------------------------------------------------------------------------//

bool Edge::isBackEdge() {

    if(source->getLoopHeader() == target) return true;
    return false;
}

//---------------------------------------------------------------------------//
void    Edge::connect(Node *target_) {
    if (target!=NULL) {
//        target->removeEdge(this);
        target->removeInEdge(this);
    }
    target=target_;
//    target_->addEdge(this);
    target_->addInEdge(this);
}
    
//----------------------------------------------------------------------------//
void    Edge::disconnect() {
    if (target!=NULL) {
//        target->removeEdge(this);
        target->removeInEdge(this);
        target=NULL;
    }
}

//========================================================================================//
// ExceptionEdge
//========================================================================================//

ExceptionEdge::ExceptionEdge(Node   *source_, 
                             Node   *target_, 
                             double  prob_, 
                             Type   *exceptionType_, 
                             uint32  priority_) :
    Edge(source_, target_, prob_, EDGE_EXCEPTION), 
    exceptionType(exceptionType_),
    priority(priority_) {
}

//========================================================================================//
// Node
//========================================================================================//

Node::Node(uint32 id_, NodeKind kind_)  {
    id         = id_;
    nodeKind   = kind_;
    loopHeader = NULL;
}

//----------------------------------------------------------------------------------------//

void Node::addEdge(Edge *edge) {

    if (edge->getSource() == this) outEdges.push_back(edge);
    if (edge->getTarget() == this) inEdges.push_back(edge);
}

//----------------------------------------------------------------------------------------//

void Node::removeEdge(Edge *edge) {

    if (edge->getSource() == this) { 
        EdgeIterator it = find(outEdges.begin(), outEdges.end(), edge); 
        if (it != outEdges.end()) outEdges.erase(it); 
    }
    
    if (edge->getTarget() == this) { 
        EdgeIterator it = find(inEdges.begin(), inEdges.end(), edge); 
        if (it != inEdges.end()) inEdges.erase(it); 
    }
}

//----------------------------------------------------------------------------------------//

Edge *Node::getOutEdge(EdgeKind edgeKind) {
    
    for(uint16 i=0; i<outEdges.size(); i++) {
        if(outEdges[i]->getEdgeKind() == edgeKind) return outEdges[i];
    }
    return NULL;
}

//----------------------------------------------------------------------------------------//

Edge *Node::getInEdge(EdgeKind edgeKind) {
    
    for(uint16 i=0; i<inEdges.size(); i++) {
        if(inEdges[i]->getEdgeKind() == edgeKind) return inEdges[i];
    }
    return NULL;
}

//----------------------------------------------------------------------------------------//

Edge *Node::getOutEdge(Node *targetNode) {

    for(uint16 i=0; i<outEdges.size(); i++) {
        if(outEdges[i]->getTarget() == targetNode) return outEdges[i];
    }
    return NULL;
}

//----------------------------------------------------------------------------------------//

Edge *Node::getInEdge(Node *sourceNode) {

    for(uint16 i=0; i<inEdges.size(); i++) {
        if(inEdges[i]->getSource() == sourceNode) return inEdges[i];
    }
    return NULL;
}

//----------------------------------------------------------------------------------------//

Node *Node::getDispatchNode() {
    
    Edge *dispatchEdge = getOutEdge(EDGE_DISPATCH);
    if(dispatchEdge == NULL) return NULL;
    return dispatchEdge->getTarget();
}
    
//----------------------------------------------------------------------------------------//

void Node::mergeOutLiveSets(RegOpndSet &resultSet) {
    
    for(uint16 i=0; i<outEdges.size(); i++) {
        RegOpndSet &outLiveSet = outEdges[i]->getTarget()->getLiveSet();
        resultSet.insert(outLiveSet.begin(), outLiveSet.end());
    }
}

//----------------------------------------------------------------------------------------//

void Node::remove() {
    
    EdgeVector edges;
    
    // remove out edges
    edges = outEdges;                          
    for (uint16 i=0; i<edges.size(); i++) edges[i]->remove();                    
    
    // remove in edges
    edges = inEdges;
    for (uint16 i=0; i<edges.size(); i++) edges[i]->remove();                    
}

//----------------------------------------------------------------------------------------//

void Node::removeInEdge(Edge *edge) {
//    remove(inEdges.begin(), inEdges.end(), edge);
    EdgeVector::iterator last=inEdges.end();
    EdgeVector::iterator pos=find(inEdges.begin(), last, edge);
    if (pos==last) return; // not found
    inEdges.erase(pos);
}

void Node::addInEdge(Edge *edge) {
    inEdges.push_back(edge);
}

void Node::printEdges(ostream& out, EdgeVector& edges, bool head) {
    for (uint i=0; i<edges.size(); i++) {
        Edge *edge=edges[i];
        if (edge==NULL) {
            out << " <edge[" << i << "]=null>";
            continue;
        }
        Node *sibling=head? edge->getTarget(): edge->getSource();
        if (sibling==NULL) {
            out << " null";
            continue;
        }
          out << " " << sibling->getId();
    }
}

void Node::printEdges(ostream& out) {
    out << " ins:";
    printEdges(out, getInEdges(), false);
    out << " outs:";
    printEdges(out, getOutEdges(), true);
}

//========================================================================================//
// BbNode
//========================================================================================//

BbNode::BbNode(uint32 id_, uint32 execCounter_) : 
    Node(id_, NODE_BB),
    layoutSucc(NULL),
    address(0),
    execCounter(execCounter_) {
}

//----------------------------------------------------------------------------------------//

void BbNode::addInst(Inst *inst) { 

    IPF_LOG << "        " << IrPrinter::toString(inst) << endl;
    insts.push_back(inst); 
}

//========================================================================================//
// Cfg
//========================================================================================//

Cfg::Cfg(MemoryManager &mm, CompilationInterface &compilationInterface):
    mm(mm),
    compilationInterface(compilationInterface) {

    maxNodeId      = 0;
    opndManager    = new(mm) OpndManager(mm, compilationInterface);
    enterNode      = NULL;
    exitNode       = NULL;
    lastSearchKind = SEARCH_UNDEF_ORDER;
}

//----------------------------------------------------------------------------------------//

NodeVector& Cfg::search(SearchKind searchKind) { 

    if(lastSearchKind == searchKind) return searchResult;
    lastSearchKind = searchKind;

    NodeSet visitedNodes;
    searchResult.clear();

    switch(searchKind) {
        case SEARCH_DIRECT_ORDER : makeDirectOrdered(exitNode, visitedNodes); break;
        case SEARCH_POST_ORDER   : makePostOrdered(enterNode, visitedNodes);  break;
        case SEARCH_LAYOUT_ORDER : makeLayoutOrdered();                       break;
        case SEARCH_UNDEF_ORDER  :                                            break;
        default                  : IPF_ERR << endl;                           break;
    }
    
    return searchResult; 
}
    
//----------------------------------------------------------------------------------------//
// All predecessors of current node have already been visited

void Cfg::makeDirectOrdered(Node *node, NodeSet &visitedNodes) {

    visitedNodes.insert(node);                       // mark node visited

    EdgeVector& inEdges = node->getInEdges();        // get inEdges
    for(uint32 i=0; i<inEdges.size(); i++) {         // iterate through inEdges
        Node *pred = inEdges[i]->getSource();     // get pred node
        if(visitedNodes.count(pred) == 1) continue;  // if it is already visited - ignore
        makeDirectOrdered(pred, visitedNodes);       // we have found unvisited pred - reenter
    }
    searchResult.push_back(node);                    // all succs have been visited - place node in searchResult vector
}

//----------------------------------------------------------------------------------------//
// All successors of current node have already been visited

void Cfg::makePostOrdered(Node *node, NodeSet &visitedNodes) {

    visitedNodes.insert(node);                        // mark node visited

    EdgeVector& outEdges = node->getOutEdges();       // get outEdges
    for(uint32 i=0; i<outEdges.size(); i++) {         // iterate through outEdges
        Node *succ = outEdges[i]->getTarget();     // get succ node
        if(visitedNodes.count(succ) == 1) continue;   // if it is already visited - ignore
        makePostOrdered(succ, visitedNodes);          // we have found unvisited succ - reenter
    }
    searchResult.push_back(node);                     // all succs have been visited - place node in searchResult vector
}

//----------------------------------------------------------------------------------------//

void Cfg::makeLayoutOrdered() {

    BbNode *node = (BbNode *)getEnterNode();
    while(node != NULL) {
        searchResult.push_back(node);
        node = node->getLayoutSucc();
    }
}

} // IPF
} // Jitrino
