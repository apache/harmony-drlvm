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
 * @version $Revision: 1.8.12.1.4.4 $
 */
 
#include "Ia32CFG.h"
#include "Timer.h"

#include "Ia32Printer.h"

namespace Jitrino {
namespace Ia32 {

    
/************************************************************************/
/************************************************************************/
/*  FREQUENCY CALCULATION                                               */
/************************************************************************/
/************************************************************************/
#define PROB_EXCEPTION  0.001
#define PROB_UNKNOWN_BB 0.5
#define PROB_IN_LOOP    0.88
#define ACCEPTABLE_DOUBLE_PRECISION_LOSS  0.000000001 
#define ABS(a) (((a) > (0)) ? (a) : -(a))
#define DEFAULT_ENTRY_NODE_FREQUENCY 10000

class _CountFreqsContext {
public:
    CFG*            cfg;
    MemoryManager&  mm;
    bool            useCyclicFreqs;
    double*         cyclicFreqs;
    bool*           visitInfo;
    StlVector<Edge*>       exitEdges;

    _CountFreqsContext(MemoryManager& _mm, CFG* _cfg)  : cfg(_cfg), mm(_mm), useCyclicFreqs(false), exitEdges(mm) {
        uint32 n = cfg->getMaxNodeId();
        cyclicFreqs = new (mm) double[n];
        visitInfo = new (mm) bool[n];
        for (uint32 i=0; i< n; i++) {
            cyclicFreqs[i]=1;
        }
    }

    inline void resetVisitInfo() {
        memset(visitInfo, false, cfg->getMaxNodeId());
    }
};


static void countFrequenciesFromProbs(CFG* fg);
static void fixEdgeProbs(CFG* cfg);
static void checkFrequencies(CFG* fg);
static void countLinearFrequency(const _CountFreqsContext* c, Node* node);
static void estimateCyclicFrequencies(_CountFreqsContext* c, Node* loopHead);
static void findLoopExits(_CountFreqsContext* c, Node* loopHead);


//static Timer *freqCalcTimer=NULL;
void CFG::updateExecCounts() {
    assert(isLoopInfoValid());
    assert(hasEdgeProfile());
    if (!hasEdgeProfile()) {
        return;
    }
    
    //step1: fix edge-probs, try to reuse old probs as much as possible..
    fixEdgeProbs(this);
    
    //step2 : calculate frequencies
    countFrequenciesFromProbs(this);
}

void fixEdgeProbs(CFG* cfg) {
    //fix edge-probs, try to reuse old probs as much as possible..    
    for (Nodes::const_iterator it = cfg->getNodes().begin(), end = cfg->getNodes().end(); it!=end; ++it) {
        Node* node = *it;
        const Edges& edges = node->getEdges(Direction_Out);
        if (edges.isEmpty()) {
            assert(node == cfg->getExitNode());
            continue;
        }
        double sumProb = 0;
        bool foundNotEstimated = false;
        for (Edge* e = edges.getFirst(); e!=NULL; e = edges.getNext(e)) {
            double prob = e->getProbability();
            sumProb+=prob;
            foundNotEstimated = foundNotEstimated || prob <= 0;
        }
        if (sumProb==1 && !foundNotEstimated) {
            continue; //ok, nothing to fix
        }
        //now fix probs..
        double mult = 1;
        if (!foundNotEstimated) {
            mult = 1 / sumProb;        
        } else {
            sumProb = 0;
            bool loopHead = node->isLoopHeader();
            for (Edge* e = edges.getFirst(); e!=NULL; e= edges.getNext(e)) {
                double prob = e->getProbability();
                if (prob <= 0 || loopHead) {
                    Node* target = e->getNode(Direction_Head);
                    if (!target->hasKind(Node::Kind_BasicBlock)) {
                        prob = PROB_EXCEPTION;
                    } else {
                        if (loopHead) { //special processing of loop entrance nodes
                            if (target!=node && target->getLoopHeader()!=node) {
                                prob = 1 - PROB_IN_LOOP;
                            } else {
                                prob = PROB_IN_LOOP;
                            }
                        } else {
                            prob = PROB_UNKNOWN_BB;
                        }
                    }
                    assert(prob > 0);
                    e->setProbability(prob);
                }
                sumProb+=prob;
            }
            mult = 1 / sumProb;
        }
        sumProb = 0;
        for (Edge* e = edges.getFirst(); e!=NULL; e= edges.getNext(e)) {
            double prob = e->getProbability();
            prob = prob * mult;
            assert(prob > 0);
            e->setProbability(prob);
#ifdef _DEBUG
            sumProb+=prob;
#endif              
        }
        assert(ABS(1-sumProb) < ACCEPTABLE_DOUBLE_PRECISION_LOSS);
    }
}

void countFrequenciesFromProbs(CFG* cfg) {
    MemoryManager& mm = cfg->getMemoryManager();
    _CountFreqsContext c(mm, cfg);
    c.resetVisitInfo();
    countLinearFrequency(&c, cfg->getPrologNode());
    if (cfg->getMaxLoopDepth() > 0) { //if there are loops in method
        c.useCyclicFreqs = true;
        for (Nodes::const_iterator it = cfg->getNodes().begin(), end = cfg->getNodes().end(); it!=end; ++it) {
            Node* node = *it;
            if (node->isLoopHeader() && node->getLoopHeader() == NULL) {
                estimateCyclicFrequencies(&c, node);
            }
        }
        c.resetVisitInfo();
        countLinearFrequency(&c, cfg->getPrologNode());
    }
    checkFrequencies(cfg);
}

static void checkFrequencies(CFG* cfg) {
#ifdef _DEBUG
    const Nodes& nodes = cfg->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        assert(node->getExecCnt() > 0);
        const Edges& inEdges = node->getEdges(Direction_In);
        if (inEdges.isEmpty()) {
            assert(node == cfg->getPrologNode());
            continue;
        }
        double freq = 0.0;
        for(Edge* edge  =  inEdges.getFirst(); edge!=NULL; edge = inEdges.getNext(edge)) {
            Node* fromNode = edge->getNode(Direction_Tail);
            double fromFreq = fromNode->getExecCnt();
            assert(fromFreq > 0);
            freq += fromFreq * edge->getProbability();
        }
        assert(ABS(node->getExecCnt() - freq)/freq < ACCEPTABLE_DOUBLE_PRECISION_LOSS);
    }
    Node* exitNode = cfg->getExitNode();
    double exitFreq = exitNode->getExecCnt();
    assert(ABS(exitFreq - DEFAULT_ENTRY_NODE_FREQUENCY) < ACCEPTABLE_DOUBLE_PRECISION_LOSS*DEFAULT_ENTRY_NODE_FREQUENCY);
#endif
}


static void countLinearFrequency(const _CountFreqsContext* c, Node* node) {
    const Edges& inEdges = node->getEdges(Direction_In);
    if (inEdges.isEmpty()) { //node is entry
        if (node->getExecCnt() <= 1) {
            node->setExecCnt(DEFAULT_ENTRY_NODE_FREQUENCY);
        } //else leave profile value
    } else {
        double freq = 0.0;
        for(Edge* edge = inEdges.getFirst(); edge!=NULL; edge = inEdges.getNext(edge)) {
            if (edge->isBackEdge()) {
                continue; //only linear freq estimation    
            }
            Node* fromNode = edge->getNode(Direction_Tail);
            if (c->visitInfo[fromNode->getId()] == false) {
                return;
            }
            double fromFreq = fromNode->getExecCnt();
            freq += fromFreq * edge->getProbability();
        }
        if (c->useCyclicFreqs && node->isLoopHeader()) {
            freq *= c->cyclicFreqs[node->getId()];
        }
        node->setExecCnt(freq);
    }
    c->visitInfo[node->getId()] = true;
    const Edges& outEdges = node->getEdges(Direction_Out);
    for(Edge* edge = outEdges.getFirst(); edge!=NULL; edge = outEdges.getNext(edge)) {
        Node* toNode = edge->getNode(Direction_Head);
        if (c->visitInfo[toNode->getId()] == false ) {
            countLinearFrequency(c, toNode);
        }
    }
}    

static void estimateCyclicFrequencies(_CountFreqsContext* c, Node* loopHead) {
    assert(loopHead->isLoopHeader());
    bool hasChildLoop = 0;
    //process all child loops first
    for (Nodes::const_iterator it = c->cfg->getNodes().begin(), end = c->cfg->getNodes().end(); it!=end; ++it) {
        Node* node = *it;
        if (node->getLoopHeader() == loopHead && node->isLoopHeader()) {
            hasChildLoop = true;
            estimateCyclicFrequencies(c, node);
        }
    }
    findLoopExits(c, loopHead);
    if (hasChildLoop) {
        c->resetVisitInfo();
        countLinearFrequency(c, c->cfg->getPrologNode());
    }
    double inFlow = loopHead->getExecCnt(); //node has linear freq here
    double exitsFlow = 0;
    //sum all exits flow
    for (StlVector<Edge*>::const_iterator it = c->exitEdges.begin(), end = c->exitEdges.end(); it!=end; ++it) {
        Edge* edge = *it;
        Node* fromNode = edge->getNode(Direction_Tail);
        exitsFlow += edge->getProbability() * fromNode->getExecCnt();
    }

    // if loop will do multiple iteration exitsFlow becomes equals to inFlow
    double loopCycles = inFlow / exitsFlow;
    assert(loopCycles > 1);
    c->cyclicFreqs[loopHead->getId()] = loopCycles;
}

static void findLoopExits(_CountFreqsContext* c, Node* loopHead) {
    c->exitEdges.clear();  
    const Nodes& nodes = c->cfg->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (!node->isWithinLoop(loopHead)) {
            continue;
        }
        const Edges& outEdges = node->getEdges(Direction_Out);
        for(Edge* edge = outEdges.getFirst(); edge!=NULL; edge = outEdges.getNext(edge)) {
            Node* targetNode = edge->getNode(Direction_Head);
            if (!targetNode->isWithinLoop(loopHead)) {
                c->exitEdges.push_back(edge);
                continue;
            }

        }
    }
}

}} //namespaces


