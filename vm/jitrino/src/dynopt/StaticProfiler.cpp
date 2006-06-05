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
 * @version $Revision: 1.19.20.5 $
 *
 */

#include "StaticProfiler.h"
#include "FlowGraph.h"
#include "Dominator.h"
#include "Loop.h"
#include "constantfolder.h"


#ifdef OLD_FREQ_ALG
#include "Profiler.h"
#endif


/** see article:  "Static Branch Frequency and Program Profile Analysis / Wu, Laurus, IEEE/ACM 1994" */

namespace Jitrino{

DEFINE_OPTPASS_IMPL(StaticProfilerPass, statprof, "Static Profiler")


struct StaticProfilerContext {
    FlowGraph* fg;
    CFGNode* node;
    CFGEdge* edge1; //trueEdge
    CFGEdge* edge2; //falseEdge
    DominatorTree* domTree;
    DominatorTree* postDomTree;
    LoopTree* loopTree;
};

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define ABS(a) (((a) > (0)) ? (a) : -(a))

//probabilities of taken branch
#define PROB_HEURISTIC_FAIL         0.50
#define PROB_ALL_EXCEPTIONS         0.001
#define PROB_UNCONDITIONAL          (1 - PROB_ALL_EXCEPTIONS)
#define PROB_BACKEDGE               0.88
#define PROB_OPCODE_HEURISTIC       0.84
#define PROB_CALL_HEURISTIC         0.78
#define PROB_LOOP_HEADER_HEURISTIC  0.75
#define PROB_RET_HEURISTIC          0.72
#define PROB_DEVIRT_GUARD_HEURISTIC 0.62
#define PROB_REFERENCE              0.60
#define PROB_STORE_HEURISTIC        0.55

#define ACCEPTABLE_DOUBLE_PRECISION_LOSS  0.000000001 

#define DEFAULT_ENTRY_NODE_FREQUENCY 10000

/** returns edge1 probability 
    Explicit parameters used here help to avoid code duplication in impl functions
*/
typedef double(*HeuristicFn) (const StaticProfilerContext*);

static double callHeuristic(const StaticProfilerContext*);
static double returnHeuristic(const StaticProfilerContext*);
static double loopHeaderHeuristic(const StaticProfilerContext*);
static double opcodeHeuristic(const StaticProfilerContext*);
static double storeHeuristic(const StaticProfilerContext*);
static double referenceHeuristic(const StaticProfilerContext*);
static double devirtGuardHeuristic(const StaticProfilerContext*);

static HeuristicFn heuristics[] = {
    loopHeaderHeuristic,
    callHeuristic, 
    returnHeuristic, 
    opcodeHeuristic,
    storeHeuristic,
    referenceHeuristic,
    devirtGuardHeuristic,
    NULL
};

static void estimateNode(StaticProfilerContext* c);
static void countFrequenciesFromProbs(FlowGraph* fg, LoopTree* lt);

static inline CFGNode* findLoopHeader(LoopTree* lt, CFGNode* node, bool outerLoop = false) {
    CFGNode* header = NULL;
    if (!outerLoop && lt->isLoopHeader(node)) {
        header = node;
    } else if (lt->hasContainingLoopHeader(node)) {
        header = lt->getContainingLoopHeader(node);
    }
    return header;
}

/** looks for unconditional path to inner loop header.
 */
static bool hasUncondPathToInnerLoopHeader(CFGNode* parentLoopHeader, LoopTree* loopTree,  CFGEdge* edge) {
    CFGNode* node = edge->getTargetNode();
    if (!node->isBasicBlock() || node == parentLoopHeader) {
        return false;
    }

    if (loopTree->isLoopHeader(node)) {
        return true;
    }
    CFGEdge* uncondEdge = (CFGEdge*)node->getUnconditionalEdge();
    if (uncondEdge==NULL) {
        return false;
    }
    return hasUncondPathToInnerLoopHeader(parentLoopHeader, loopTree, uncondEdge);
}

/** returns TRUE if
 *  1) edge is back-edge
 *  2) if lookForDirectPath == TRUE returns isBackedge(edge->targetNode->targetUncondOutEdge ) 
 *    if targetUncondOutEdge is the only unconditional edge of targetNode
 */
static inline bool isBackedge(LoopTree* loopTree, CFGEdge* e, bool lookForDirectPath) {
    CFGNode* loopHeader = findLoopHeader(loopTree, e->getSourceNode());
    if (loopHeader == NULL) {
        return false;
    }
    CFGNode* targetNode = e->getTargetNode();
    if (targetNode == loopHeader) {
        return true;
    }
    if (lookForDirectPath) {
        CFGEdge* targetUncondOutEdge = (CFGEdge*)targetNode->getUnconditionalEdge();
        if (targetUncondOutEdge !=NULL) {
            return isBackedge(loopTree, targetUncondOutEdge, lookForDirectPath);
        }
    }
    return false;
}

static bool isLoopExit(LoopTree* loopTree, CFGEdge* e) {
    CFGNode* loopHeader = findLoopHeader(loopTree, e->getSourceNode());
    return loopHeader!= NULL && !loopTree->loopContains(loopHeader ,e->getTargetNode());
}

static bool isLoopHeuristicAcceptable(StaticProfilerContext* c) {
    bool edge1IsLoopExit = isLoopExit(c->loopTree, c->edge1);
    bool edge2IsLoopExit = isLoopExit(c->loopTree, c->edge2);
    assert (!(edge1IsLoopExit && edge2IsLoopExit)); //both edges could not be loop exits at the same time
    return edge1IsLoopExit || edge2IsLoopExit;
}

void StaticProfilerPass::_run(IRManager& irm) {
    computeDominatorsAndLoops(irm);
    
    StaticProfilerContext c;
    c.fg = &irm.getFlowGraph();
    c.loopTree = irm.getLoopTree();
    c.domTree = irm.getDominatorTree();
    c.postDomTree = DominatorBuilder().computePostdominators(irm.getNestedMemoryManager(), c.fg);
    
    const CFGNodeDeque& nodes = c.fg->getNodes();
    for (CFGNodeDeque::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        CFGNode* node = *it;
        c.node = node;
        c.edge1 = NULL;
        c.edge2 = NULL;
        estimateNode(&c);
    }
    countFrequenciesFromProbs(c.fg, c.loopTree);
    c.fg->setEdgeProfile(true);
}

void estimateNode(StaticProfilerContext* c) {
    const CFGEdgeDeque& edges =  c->node->getOutEdges();
    if (edges.empty()) {
        return;
    } else if (edges.size() == 1) {
        (*edges.begin())->setEdgeProb(1.0);
        return;
    }
    CFGEdge* falseEdge = (CFGEdge*)c->node->getFalseEdge();
    CFGEdge* trueEdge = (CFGEdge*)c->node->getTrueEdge();
    double probLeft = 1.0;
    uint32 edgesLeft = edges.size();
    if (falseEdge == NULL || trueEdge == NULL) { // can't apply general heuristics.
        CFGEdge* uncondEdge = (CFGEdge*)c->node->getUnconditionalEdge();
        if (uncondEdge) {
            uncondEdge->setEdgeProb(PROB_UNCONDITIONAL);
            probLeft-=PROB_UNCONDITIONAL;
            edgesLeft--;
        }
    } else {
        assert(falseEdge->getTargetNode()!=trueEdge->getTargetNode());

        double prob = 0.5; // trueEdge prob

        // separate back-edge heuristic from others heuristics (the way it's done in article)
        c->edge1 = trueEdge;
        c->edge2 = falseEdge;
        if (isLoopHeuristicAcceptable(c)) {
            prob = isLoopExit(c->loopTree, trueEdge) ? 1 - PROB_BACKEDGE : PROB_BACKEDGE;
        } else {
            // from this point we can apply general heuristics
            for (HeuristicFn* fn = heuristics; *fn!=NULL; fn++) {
                double dprob = (*fn)(c);
                if (dprob!=PROB_HEURISTIC_FAIL) {
                    prob = prob * dprob / (prob*dprob + (1-prob)*(1-dprob));
                }
            }
        }

        // all other edges (exception, catch..) have lower probability
        double othersProb = edges.size() == 2 ? 0.0 : PROB_ALL_EXCEPTIONS;
        trueEdge->setEdgeProb(MAX(PROB_ALL_EXCEPTIONS, prob - othersProb/2));
        falseEdge->setEdgeProb(MAX(PROB_ALL_EXCEPTIONS, 1 - prob - othersProb/2));
        probLeft = othersProb;
        edgesLeft-=2;
    }
    
    if (edgesLeft != 0) {
        double probPerEdge = probLeft / edgesLeft;
        for (CFGEdgeDeque::const_iterator it = edges.begin(), itEnd = edges.end(); it!=itEnd; it++) {
            CFGEdge* e = *it;
            if (e->getEdgeProb()==0.0) {
                e->setEdgeProb(probPerEdge);
            }
        }
    } 
    
#ifdef _DEBUG //debug check
    double probe = 0;
    for (CFGEdgeDeque::const_iterator it = edges.begin(), itEnd = edges.end(); it!=itEnd; it++) {
        CFGEdge* e = *it;
        double dprob = e->getEdgeProb();
        assert (dprob!=0.0);
        probe+=dprob;
    }
    assert(ABS(probe - 1.0) < ACCEPTABLE_DOUBLE_PRECISION_LOSS);
#endif
}


/** looks for Inst with opcode in specified range (both bounds are included) 
  * in specified node and it's unconditional successors.
  */
static inline Inst* findInst(CFGNode* node, Opcode opcodeFrom, Opcode opcodeTo) {
    for (Inst *i = node->getFirstInst(), *last = node->getLastInst(); ; i = i->next()) {
        Opcode opcode = i->getOpcode();
        if (opcode>=opcodeFrom && opcode<=opcodeTo) {
            return i;
        }
        if (i == last) {
            break;
        }
    }
    
    CFGEdge* uncondEdge = (CFGEdge*)node->getUnconditionalEdge(); 
    if (uncondEdge!=NULL && uncondEdge->getTargetNode()->getInEdges().size() == 1) {
        return findInst(uncondEdge->getTargetNode(), opcodeFrom, opcodeTo);
    }
    return NULL;
}

static inline Inst* findInst(CFGNode* node, Opcode opcode) {
    return findInst(node, opcode, opcode);
}


/** Call heuristic (CH). 
 *  Predict a successor that contains
 *  a call and does not post-dominate will not be taken.
 */
static double callHeuristic(const StaticProfilerContext* c) {
    CFGNode* node1 = c->edge1->getTargetNode();
    CFGNode* node2 = c->edge2->getTargetNode();
    bool node1HasCall = findInst(node1, Op_DirectCall, Op_IntrinsicCall)!=NULL;
    bool node2HasCall = findInst(node2, Op_DirectCall, Op_IntrinsicCall)!=NULL;
    
    if (!node1HasCall && !node2HasCall) {
        return PROB_HEURISTIC_FAIL;
    }

    // at least one node has call from this point
    bool node1Post = false;
    bool node2Post = false;
    
    if (node1HasCall) {
        node1Post = c->postDomTree->dominates(node1, c->node);
        if (!node1Post && !node2HasCall) {
            return 1 - PROB_CALL_HEURISTIC;
        }
    }
    if (node2HasCall) {
        node2Post = c->postDomTree->dominates(node2, c->node);
        if (!node2Post && !node1HasCall) {
            return PROB_CALL_HEURISTIC;
        }
    }
    
    if (node1HasCall != node2HasCall) {
        return PROB_HEURISTIC_FAIL;
    }
    
    // both nodes have call from this point
    return node1Post==node2Post ? PROB_HEURISTIC_FAIL: (!node1Post ? 1- PROB_CALL_HEURISTIC: PROB_CALL_HEURISTIC);
    
}


/** Return heuristic (RH). 
 *  Predict a successor that contains a return will not be taken.
 */
static double returnHeuristic(const StaticProfilerContext* c) {
    bool node1HasRet= findInst(c->edge1->getTargetNode(), Op_Return)!=NULL;
    bool node2HasRet= findInst(c->edge2->getTargetNode(), Op_Return)!=NULL;
    return node1HasRet == node2HasRet ? PROB_HEURISTIC_FAIL: (node1HasRet ? 1 - PROB_RET_HEURISTIC: PROB_RET_HEURISTIC);
}


/**  Loop header heuristic (LHH)
 *   Predict a successor that is a loop header or loop pre-header
 *   and does not post-dominate will be taken
 */
static double loopHeaderHeuristic(const StaticProfilerContext* c) {
    CFGNode* parentLoopHeader = findLoopHeader(c->loopTree, c->node, true);
    
    bool edge1Accepted = hasUncondPathToInnerLoopHeader(parentLoopHeader, c->loopTree, c->edge1) 
        && !c->postDomTree->dominates(c->edge1->getTargetNode(), c->node);
    
    bool edge2Accepted = hasUncondPathToInnerLoopHeader(parentLoopHeader, c->loopTree, c->edge2) 
        && !c->postDomTree->dominates(c->edge2->getTargetNode(), c->node);
    
    if (edge1Accepted == edge2Accepted) {
        return PROB_HEURISTIC_FAIL;
    }
    return edge1Accepted ? PROB_LOOP_HEADER_HEURISTIC : 1 - PROB_LOOP_HEADER_HEURISTIC;
}


/** Opcode heuristic (OH)
 *  Predict that a comparison of an integer for less than zero,
 *  less than or equal to zero or equal to a constant, will fail.
 */
static double opcodeHeuristic(const StaticProfilerContext* c) {
    Inst* inst = c->node->getLastInst();
    if (!(inst->getOpcode() == Op_Branch && inst->getSrc(0)->getType()->isInteger())) {
        return PROB_HEURISTIC_FAIL;
    }

    assert (inst->getNumSrcOperands() == 2);
    
    Opnd* src0 = inst->getSrc(0);
    Opnd* src1 = inst->getSrc(1);

    
    ConstInst::ConstValue v0, v1;
    bool src0Const = ConstantFolder::isConstant(src0->getInst(), v0);
    bool src1Const = ConstantFolder::isConstant(src1->getInst(), v1);

    if (!src0Const && !src1Const) {
        return PROB_HEURISTIC_FAIL;
    }

    Type::Tag tag = src0->getType()->tag;

    enum CMP_STATUS { CMP_UNDEF, CMP_OK, CMP_FAIL } rc = CMP_UNDEF;

    if (src0Const && src1Const) {
        rc = v0.dword1 == v1.dword1 && v0.dword2 == v1.dword2 ? CMP_OK : CMP_FAIL;
    } else { //!(src0Const && src1Const)
        ConstInst::ConstValue val =  src0Const ? v0: v1;
        bool constValLEzero = false;
        switch(tag) {
            case Type::IntPtr:  // ptr-sized integer
#ifdef POINTER64
                constValLEzero = val.i8 <= 0;
#else
                constValLEzero = val.i4 <= 0;
#endif
                break;
            case Type::Int8:  constValLEzero = ((int8)val.i4) <= 0; break;
            case Type::Int16: constValLEzero = ((int16)val.i4) <= 0; break;
            case Type::Int32: constValLEzero = val.i4 <= 0; break;
            case Type::Int64: constValLEzero = val.i8 <= 0; break;
            
            case Type::UIntPtr: //ConstValue is union
            case Type::UInt8: 
            case Type::UInt16:
            case Type::UInt32:
            case Type::UInt64: constValLEzero = ((uint64)val.i8) == 0; break;
            default: assert(false);
        }

        ComparisonModifier cmpOp = inst->getComparisonModifier();
        switch (cmpOp) {
            case Cmp_EQ:     rc = CMP_FAIL; break;//comparison with const fails
            case Cmp_NE_Un:  rc = CMP_OK; break;
            case Cmp_GT:
            case Cmp_GT_Un:
            case Cmp_GTE:
            case Cmp_GTE_Un:
            if (constValLEzero) { //negative or zero branch is not taken
                rc = src0Const ? CMP_FAIL : CMP_OK;
            }
            break;
            case Cmp_Zero: rc = CMP_FAIL; break;//compare with const fails
            case Cmp_NonZero:  rc = CMP_OK;  break;//not equal to const -> OK
            default: break; //to avoid warning  that this enum value is not handled
        }
    }
    if (rc == CMP_UNDEF) {
        return PROB_HEURISTIC_FAIL;
    } else if (rc == CMP_OK) {
        return PROB_OPCODE_HEURISTIC;
    } 
    return 1 - PROB_OPCODE_HEURISTIC;
}


/** Store heuristic (SH). 
*  Predict a successor that contains a store instruction 
*  and does not post dominate will not be taken
*/
static double storeHeuristic(const StaticProfilerContext* c) {
    CFGNode* node1 = c->edge1->getTargetNode();
    CFGNode* node2 = c->edge2->getTargetNode();
    bool node1Accepted = findInst(node1, Op_TauStInd)!=NULL;
    bool node2Accepted = findInst(node2, Op_TauStInd)!=NULL;
    if (!node1Accepted && !node2Accepted) {
        return PROB_HEURISTIC_FAIL;
    }
    node1Accepted == node1Accepted && !c->postDomTree->dominates(node1, c->node);
    node2Accepted == node2Accepted && !c->postDomTree->dominates(node2, c->node);

    if (!node1Accepted && !node2Accepted) {
        return PROB_HEURISTIC_FAIL;
    }
    return node1Accepted ? 1 - PROB_STORE_HEURISTIC: PROB_STORE_HEURISTIC;
}

/** Reference heuristic (RH)
 *  Same as Pointer heuristic (PH) in article
 *  Predict that a comparison of object reference
 *  against null or two references will fail
 */
static double referenceHeuristic(const StaticProfilerContext *c) {
    Inst* inst = c->node->getLastInst();
    if (inst->getOpcode() != Op_Branch || !inst->getSrc(0)->getType()->isObject()) {
        return PROB_HEURISTIC_FAIL;
    }
    enum CMP_STATUS { CMP_UNDEF, CMP_OK, CMP_FAIL } rc = CMP_UNDEF;
    ComparisonModifier cmpOp = inst->getComparisonModifier();
    switch (cmpOp) {
        case Cmp_EQ: rc = CMP_FAIL; break;
        case Cmp_NE_Un: rc = CMP_OK; break;
        case Cmp_Zero: rc = CMP_FAIL; break;
        case Cmp_NonZero: rc = CMP_OK; break;
        default: assert(false);
    }
    assert(rc!=CMP_UNDEF);
    return rc == CMP_OK ? PROB_REFERENCE : 1 - PROB_REFERENCE;
}

/** De-virtualization guard heuristic (DGH) 
  * Predict that comparison of VTablePtr will succeed
  */
static double devirtGuardHeuristic(const StaticProfilerContext *c) {
    Inst* inst = c->node->getLastInst();
    if (inst->getOpcode() != Op_Branch || !inst->getSrc(0)->getType()->isVTablePtr()) {
        return PROB_HEURISTIC_FAIL;
    }
    ComparisonModifier cmpOp = inst->getComparisonModifier();
    assert(cmpOp == Cmp_EQ || cmpOp == Cmp_NE_Un);
    return cmpOp == Cmp_EQ ? PROB_DEVIRT_GUARD_HEURISTIC : 1 - PROB_DEVIRT_GUARD_HEURISTIC;
}



/************************************************************************/
/************************************************************************/
/*  FREQUENCY CALCULATION                                               */
/************************************************************************/
/************************************************************************/

class CountFreqsContext {
public:
    FlowGraph* fg;
    MemoryManager& mm;
    LoopTree* loopTree;
    bool useCyclicFreqs;
    double* cyclicFreqs;
    bool* visitInfo;
    StlVector<CFGEdge*> exitEdges;

    CountFreqsContext(MemoryManager& _mm, FlowGraph* _fg, LoopTree* _lt)  :
        fg(_fg), mm(_mm), loopTree(_lt), useCyclicFreqs(false), exitEdges(mm)
    {
        uint32 n = fg->getMaxNodeId();
        cyclicFreqs = new (mm) double[n];
        visitInfo = new (mm) bool[n];
        for (uint32 i=0; i< n; i++) {
            cyclicFreqs[i]=1;
        }
    }

    inline void resetVisitInfo() {
        memset(visitInfo, false, fg->getMaxNodeId());
    }
};

static void checkFrequencies(FlowGraph* fg);
static void countLinearFrequency(const CountFreqsContext* c, CFGNode* node);
static void estimateCyclicFrequencies(CountFreqsContext* c, LoopNode* loopHead);
static void findLoopExits(CountFreqsContext* c, LoopNode* loopHead);

void countFrequenciesFromProbs(FlowGraph* fg, LoopTree* lt) {
    MemoryManager& mm = fg->getIRManager()->getMemoryManager();
#ifdef OLD_FREQ_ALG
    fg->getEntry()->setFreq(DEFAULT_ENTRY_NODE_FREQUENCY);
    EdgeProfile p(mm, fg->getIRManager()->getMethodDesc(), true);
    p.smoothProfile(*fg);
#else
    CountFreqsContext c(mm, fg, lt);
    c.resetVisitInfo();
    countLinearFrequency(&c, fg->getEntry());
    LoopNode* topLevelLoop = (LoopNode*)lt->getRoot();
    if (topLevelLoop != NULL && topLevelLoop->getChild()!=NULL) { //if there are loops in method
        c.useCyclicFreqs = true;
        for (LoopNode* loopHead = topLevelLoop->getChild(); loopHead!=NULL; loopHead = loopHead->getSiblings()) {
            estimateCyclicFrequencies(&c, loopHead);
        }
        c.resetVisitInfo();
        countLinearFrequency(&c, fg->getEntry());
    }
#endif   
    checkFrequencies(fg);
}

static void checkFrequencies(FlowGraph* fg) {
#ifdef _DEBUG
    const CFGNodeDeque& nodes = fg->getNodes();
    for (CFGNodeDeque::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; it++) {
        CFGNode* node = *it;
        assert(node->getFreq() > 0);
        const CFGEdgeDeque& inEdges = node->getInEdges();
        if (inEdges.empty()) {
            assert(node == fg->getEntry());
            continue;
        }
        double freq = 0.0;
        for(CFGEdgeDeque::const_iterator it = inEdges.begin(), itEnd = inEdges.end(); it!=itEnd; it++) {
            CFGEdge* edge = *it;
            CFGNode* fromNode = edge->getSourceNode();
            double fromFreq = fromNode->getFreq();
            assert(fromFreq > 0);
            freq += fromFreq * edge->getEdgeProb();
        }
        assert(ABS(node->getFreq()- freq)/freq  < ACCEPTABLE_DOUBLE_PRECISION_LOSS);
    }
    CFGNode* exitNode = fg->getExit();
    double exitFreq = exitNode->getFreq();
    assert(ABS(exitFreq - DEFAULT_ENTRY_NODE_FREQUENCY) < ACCEPTABLE_DOUBLE_PRECISION_LOSS * DEFAULT_ENTRY_NODE_FREQUENCY);
#endif
}


static void countLinearFrequency(const CountFreqsContext* c, CFGNode* node) {
    const CFGEdgeDeque& inEdges = node->getInEdges();
    if (inEdges.empty()) { //node is entry
        node->setFreq(DEFAULT_ENTRY_NODE_FREQUENCY);
    } else {
        double freq = 0.0;
        LoopNode* loopHead = c->loopTree->isLoopHeader(node) ? c->loopTree->getLoopNode(node): NULL;
        for(CFGEdgeDeque::const_iterator it = inEdges.begin(), itEnd = inEdges.end(); it!=itEnd; it++) {
            CFGEdge* edge = *it;
            CFGNode* fromNode = edge->getSourceNode();
            if (loopHead!=NULL && loopHead->inLoop(fromNode)) { //backedge
                continue; //only linear freq estimation
            }
            if (c->visitInfo[fromNode->getId()] == false) {
                return;
            }
            double fromFreq = fromNode->getFreq();
            freq += fromFreq * edge->getEdgeProb();
        }
        if (c->useCyclicFreqs && c->loopTree->isLoopHeader(node)) {
            freq *= c->cyclicFreqs[node->getId()];
        }
        node->setFreq(freq);
    }
    c->visitInfo[node->getId()] = true;
    CFGNode* outerLoopHead = findLoopHeader(c->loopTree, node, true);
    const CFGEdgeDeque& outEdges = node->getOutEdges();
    for(CFGEdgeDeque::const_iterator it = outEdges.begin(), itEnd = outEdges.end(); it!=itEnd; it++) {
        CFGEdge* edge = *it;
        CFGNode* toNode = edge->getTargetNode();
        if (toNode == node || toNode == outerLoopHead) { //backedge
            continue;//do only linear freq estimation
        }
        if (c->visitInfo[toNode->getId()] == false ) {
            countLinearFrequency(c, toNode);
        }
    }
}    

static void estimateCyclicFrequencies(CountFreqsContext* c, LoopNode* loopHead) {
    //process all child loops first
    bool hasChildLoop = loopHead->getChild()!=NULL;
    if (hasChildLoop) {
        for (LoopNode* childHead = loopHead->getChild(); childHead!=NULL; childHead = childHead->getSiblings()) {
            estimateCyclicFrequencies(c, childHead);
        }
    }
    findLoopExits(c, loopHead);
    if (hasChildLoop) {
        c->resetVisitInfo();
        countLinearFrequency(c, c->fg->getEntry());
    }
    CFGNode* cfgLoopHead = loopHead->getHeader();
    double inFlow = cfgLoopHead->getFreq(); //node has linear freq here
    double exitsFlow = 0;
    //sum all exits flow
    for (StlVector<CFGEdge*>::const_iterator it = c->exitEdges.begin(), end = c->exitEdges.end(); it!=end; ++it) {
        CFGEdge* edge = *it;
        CFGNode* fromNode = edge->getSourceNode();
        exitsFlow += edge->getEdgeProb() * fromNode->getFreq();
    }
    // if loop will make multiple iteration exitsFlow becomes equals to inFlow
    double loopCycles = inFlow / exitsFlow;
    assert(loopCycles > 1);
    c->cyclicFreqs[cfgLoopHead->getId()] = loopCycles;
}

static void findLoopExits(CountFreqsContext* c, LoopNode* loopHead) {
    c->exitEdges.clear();
    const CFGNodeDeque& nodes = c->fg->getNodes();
    for (CFGNodeDeque::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        CFGNode* node = *it;
        if (!loopHead->inLoop(node)) {
            continue;
        }
        const CFGEdgeDeque& outEdges = node->getOutEdges();
        for(CFGEdgeDeque::const_iterator eit = outEdges.begin(), eend = outEdges.end(); eit!=eend; ++eit) {
            CFGEdge* edge = *eit;
            CFGNode* targetNode = edge->getTargetNode();
            if (!loopHead->inLoop(targetNode)) {
                c->exitEdges.push_back(edge);
            }
        }
    }
}

}
