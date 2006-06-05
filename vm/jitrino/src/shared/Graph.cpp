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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.9.22.4 $
 *
 */

#include <stdlib.h>
#include "Type.h"
#include "Graph.h"
#include "Stack.h"

#include <string.h>

namespace Jitrino {

//
// count number of nodes
//

uint32 Graph::countNodes() {
    uint32 numNodes = 0;
    for (Node* n = head.next(); n != &head; n = n->next())
        numNodes++;
    return numNodes;
}

//
// assign depth first numbering starting from an initial node and propagating forward
//

void   Graph::assignDfNum(Node* node, uint32&  dfNum, StlVector<Node*>& dfnMap) {
    if (node->getTraversalNum() >= traversalNum)
        return;
    node->setTraversalNum((uint16)traversalNum);

    // set node's depth first number
    dfnMap.push_back(node);
    node->setDfNum(dfNum++);
    assert(dfNum == dfnMap.size());

    // traverse each successor
    for (Edge* e = node->getSuccEdges(); e != NULL; e = e->getNextSuccEdge()) 
        assignDfNum(e->getSuccNode(), dfNum, dfnMap);
}



//
// add <pred,succ> edge into pred and succ nodes
// the user needs to supply the edge to insert
//

void Graph::addEdge(Node *predNode, Node* succNode, Edge *edge) {
#ifdef _DEBUGx
    ::std::cout << "add edge<";
    predNode->print(cout);cout << ",";
    succNode->print(cout);cout << ">" << ::std::endl;
#endif
    assert(!predNode->hasEdge(succNode));
    edge->setNextSuccEdge(predNode->getSuccEdges());
    predNode->setSuccEdges(edge);
    edge->setSuccNode(succNode);

    edge->setNextPredEdge(succNode->getPredEdges());
    succNode->setPredEdges(edge);
    edge->setPredNode(predNode);

    setBeenModified(true);
}

//
// same as before, but the user does not need to supply the edge to insert, it is
// created automatically
//
void Graph::addEdge(Node *predNode, Node* succNode) {
    Edge *edge = new (memManager)Edge();
    addEdge(predNode, succNode, edge);
}

Edge * Graph::deleteEdgeToSuccessor(Node* predNode, Node* succNode) {
    Edge *edge;
    Edge *prev = NULL;
    for (edge = predNode->getSuccEdges(); edge != NULL; edge = edge->getNextSuccEdge()) {
        if (edge->getSuccNode() == succNode)
            break;
        prev = edge;
    }
    assert(edge != NULL);
    if (prev != NULL)
        prev->setNextSuccEdge(edge->getNextSuccEdge());
    else
        predNode->setSuccEdges(edge->getNextSuccEdge());
    return edge;
}

void Graph::deleteEdgeToPredecessor(Node *succNode, Edge *edge) {
    Edge *e;
    Edge *prev = NULL;
    for (e = succNode->getPredEdges(); e != NULL; e = e->getNextPredEdge()) {
        if (e == edge)
            break;
        prev = e;
    }
    if (prev != NULL)
       prev->setNextPredEdge(edge->getNextPredEdge());
    else
       succNode->setPredEdges(edge->getNextPredEdge());
}

//
// remove edges between pred and succ nodes, returns the edge
//
Edge * Graph::deleteEdge(Node* predNode, Node* succNode) {
    Edge *edge  = deleteEdgeToSuccessor(predNode,succNode);
    deleteEdgeToPredecessor(succNode,edge);
    return edge;
}

//
// move edges from succ node to predecessor node
//
void Graph::mergeNodeWithSuccessor(Node* predNode, Node* succNode) {
    if (succNode->hasOnlyOnePredEdge()) {
        // delete the edge between predecessor and successor
        deleteEdgeToSuccessor(predNode,succNode);
        Edge *edge;
        for (edge = succNode->getSuccEdges(); edge != NULL; edge = edge->getNextSuccEdge()) {
            edge->setPredNode(predNode);
        }
        Edge *prev = NULL;
        for (edge = predNode->getSuccEdges(); edge != NULL; edge = edge->getNextSuccEdge()) {
            prev = edge;
        }
        if (prev != NULL) {
            prev->setNextSuccEdge(succNode->getSuccEdges());
        } else
            predNode->setSuccEdges(succNode->getSuccEdges());
        succNode->unlink();
    } else { // more general case, less efficient
        Edge *edge = deleteEdge(predNode,succNode);
        Edge *first = succNode->getSuccEdges();
        addEdge(predNode,first->getSuccNode(),edge);
        for (edge = first->getNextSuccEdge(); edge != NULL; edge = edge->getNextSuccEdge()) {
            addEdge(predNode,edge->getSuccNode());
        }
    }
}


// 
// move successor edges from node 'from' to a newly created node 'to'

void Graph::moveSuccEdgesToNewNode(Node* from, Node *to) {
    assert(to->getSuccEdges()==NULL);
    for (Edge *edge = from->getSuccEdges(); edge != NULL; edge = edge->getNextSuccEdge()) {
        edge->setPredNode(to);
    }
    to->setSuccEdges(from->getSuccEdges());
    from->setSuccEdges(NULL);
}


//
// return true, if there exists an edge between predNode and succNode
// only used for assertion
//
bool Node::hasEdge(Node* succNode) {
    for (Edge* e = succs; e != NULL; e = e->getNextSuccEdge())
        if (e->getSuccNode() == succNode)
            return true;
    return false;
}

//
// generic print node id (only prints the depth-first number)
//

void Node::print(::std::ostream& cout) {
    cout << "Node" << (int32)dfnum << ::std::endl;
}


/*********** support for DOT files ******************/

void Graph::printDotBody() {
    ::std::ostream& out = *os;
    Node *head = getNodeList();
    Node *node;
    for (node = head->next(); node != head; node = node->next())
        node->printDotNode(out);
    for (node = head->next(); node != head; node = node->next())
        for (Edge *edge = node->getSuccEdges(); edge != NULL; edge = edge->getNextSuccEdge())
            edge->printDotEdge(out);
}


void Node::printDotNode(::std::ostream& out) {
    print(out);
    out << " [label=\"";
    print(out);
    out << "\"]" << ::std::endl;
}

void Edge::printDotEdge(::std::ostream& out) {
    predNode->print(out);
    out << " -> ";
    succNode->print(out);
    out << ";" << ::std::endl;
}



} //namespace Jitrino 
