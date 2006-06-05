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
 * @version $Revision: 1.9.12.1.4.4 $
 */

#include "Ia32CodeLayoutBottomUp.h"
#include "Ia32IRManager.h"
#include "Log.h"
namespace Jitrino
{
namespace Ia32 {
/**
* Class to perform bottom-up block layout. 
* The layout algorithm is similar to the one described in the paper
* "Profile Guided Code Positioning" by Hansen & Pettis. 
*/

BottomUpLayout::BottomUpLayout(IRManager* irm) : 
Linearizer(irm), 
mm(40*irm->getMaxNodeId(), "Ia32::bottomUpLayout"), 
firstInChain(mm,irManager->getMaxNodeId(), false),
lastInChain(mm,irManager->getMaxNodeId(), false),
prevInLayoutBySuccessorId(mm, irManager->getMaxNodeId(), NULL)
{
}


struct edge_comparator {
    bool operator() (const Edge* e1, const Edge* e2) const { //true -> e1 is first
        ExecCntValue v1 = getEdgeExecCount(e1);
        ExecCntValue v2 = getEdgeExecCount(e2);
        return   v1 > v2 ? true : (v1 < v2 ? false: e1 < e2);
    }
    static ExecCntValue getEdgeExecCount(const Edge* e) { 
        return e->getNode(Direction_Tail)->getExecCnt() * e->getProbability();
    }

};

void BottomUpLayout::linearizeCfgImpl() {
    irManager->updateExecCounts();
#ifdef _DEBUG
    ensureProfileIsValid();
#endif
    StlVector<Edge*> sortedEdges(mm);
    const Nodes& nodes = irManager->getNodes();
    sortedEdges.reserve(nodes.size() * 3);
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node* node = *it;
        const Edges& edges = node->getEdges(Direction_Out);
        for (Edge *e = edges.getFirst(); e; e = edges.getNext(e)) {
            sortedEdges.push_back(e);
        }
    }
    std::sort(sortedEdges.begin(), sortedEdges.end(), edge_comparator());
    for(StlVector<Edge*>::const_iterator it = sortedEdges.begin(), itEnd = sortedEdges.end(); it!=itEnd; it++) {
        Edge* edge  = *it;
        layoutEdge(edge);
    }
    //create one-block-chains from blocks that was not laid out
    //these blocks are dispatch successors or are blocks connected by in/out edges to already laid out chains
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            BasicBlock* block = (BasicBlock*)node;
            if (block->getLayoutSucc() == NULL && !lastInChain[block->getId()]) {
                firstInChain[block->getId()] = true;
                lastInChain[block->getId()] = true;
            }
        }
    }
    combineChains();
    fixBranches();
}

void  BottomUpLayout::layoutEdge(Edge *edge) {
    Node* tailNode = edge->getNode(Direction_Tail);
    Node* headNode = edge->getNode(Direction_Head);
    if (!headNode->hasKind(Node::Kind_BasicBlock) || !tailNode->hasKind(Node::Kind_BasicBlock)) {
        return;
    }
    if (headNode == irManager->getPrologNode()) { //prolog node should be first in layout
        return;
    }
    if (tailNode == headNode) {
        return; //backedge to self
    }
    BasicBlock* tailBlock = (BasicBlock*)tailNode;
    if (tailBlock->getLayoutSucc()!=NULL) {
        return;  //tailBlock is layout predecessor for another successor
    }
    BasicBlock* headBlock = (BasicBlock*)headNode;
    if (prevInLayoutBySuccessorId[headBlock->getId()]!=NULL) {
        return; // head was already laid out (in other chain)
    }
    if (lastInChain[tailBlock->getId()] && firstInChain[headBlock->getId()]) {
        BasicBlock* tailOfHeadChain = headBlock;
        while (tailOfHeadChain->getLayoutSucc()!=NULL) {
            tailOfHeadChain = tailOfHeadChain->getLayoutSucc();
        }   
        if (tailOfHeadChain == tailBlock) {
            return; // layout must be acyclic
        }
    }

    tailBlock->setLayoutSucc(headBlock);
    prevInLayoutBySuccessorId[headBlock->getId()] = tailBlock;

    BasicBlock* tailPred = prevInLayoutBySuccessorId[tailBlock->getId()];
    if (tailPred) {
        assert(lastInChain[tailBlock->getId()]);
        lastInChain[tailBlock->getId()] = false;
    } else {
        firstInChain[tailBlock->getId()] = true;
    }// here we have valid first
    firstInChain[headBlock->getId()] = false;

    BasicBlock* newLast = headBlock;
    while (newLast->getLayoutSucc()!=NULL) {
        newLast = newLast->getLayoutSucc();
    }
    lastInChain[newLast->getId()] = true;
}


struct chains_comparator{
    const StlVector<BasicBlock*>& prevInLayoutBySuccessorId;
    const BasicBlock* prolog;
    chains_comparator(const StlVector<BasicBlock*>& _prevInLayoutBySuccessorId, BasicBlock* _prolog) 
        : prevInLayoutBySuccessorId(_prevInLayoutBySuccessorId), prolog(_prolog) 
    {};
    
    
    bool operator() (const BasicBlock* c1, const BasicBlock* c2) const { 
        if (c1 == prolog) {
            return true;
        }
        if (c2 == prolog) {
            return false;
        }
        ExecCntValue fromC1ToC2 = calcEdgesWeight(c1, c2);
        ExecCntValue fromC2ToC1 = calcEdgesWeight(c2, c1);
        if (fromC1ToC2 > fromC2ToC1) {
            return true; //c1 is first in topological order
        } else if (fromC1ToC2 < fromC2ToC1) {
            return false; //c2 is first in topological order
        }
        return c1 > c2; //any stable order..
    }

    ExecCntValue calcEdgesWeight(const BasicBlock* c1, const BasicBlock* c2) const {
        ExecCntValue d = 0.0;
        //distance is sum of exec count of c1 blocks out edges c1 to c2;
        for (const BasicBlock* b = c1; b!=NULL; b = b->getLayoutSucc()) {
            const Edges& outEdges = b->getEdges(Direction_Out);
            for (Edge* e = outEdges.getFirst(); e!=NULL; e = outEdges.getNext(e)) {
                Node* node = e->getNode(Direction_Head);
                if (node != b->getLayoutSucc() && node->hasKind(Node::Kind_BasicBlock)) {
                    const BasicBlock* targetBlock = (BasicBlock*)node;
                    //look if node is in c2 chain
                    const BasicBlock* targetChain = findChain(targetBlock);
                    if (targetChain == c2) {
                        ExecCntValue dd = b->getExecCnt() * e->getProbability();
                        d+=dd;
                    }
                }
            }
        }
        return d;
    }

    const BasicBlock* findChain(const BasicBlock* bb) const  {
        const BasicBlock* prev  = bb;
        while ((prev = prevInLayoutBySuccessorId[bb->getId()])!=NULL) {
            bb = prev;
        }
        return bb;
    }
};



void BottomUpLayout::combineChains() {
    StlVector<BasicBlock*> chains(mm);
    const Nodes& nodes = irManager->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node* node = *it;
        if (firstInChain[node->getId()]) {
            assert(node->hasKind(Node::Kind_BasicBlock));
            chains.push_back((BasicBlock*)node);
        }
    }
    std::sort(chains.begin(), chains.end(), chains_comparator(prevInLayoutBySuccessorId, irManager->getPrologNode()));
    assert(*chains.begin() ==irManager->getPrologNode());

    assert(*chains.begin() == irManager->getPrologNode());
    for (uint32 i = 0, n = chains.size()-1; i<n;i++) {
        BasicBlock* firstChain = chains[i];
        BasicBlock* secondChain= chains[i+1];
        BasicBlock* lastInFirst = firstChain;
        while (lastInFirst->getLayoutSucc()!=NULL) {
            lastInFirst = lastInFirst->getLayoutSucc();
        }
        lastInFirst->setLayoutSucc(secondChain);
    }
}

}} //namespace
