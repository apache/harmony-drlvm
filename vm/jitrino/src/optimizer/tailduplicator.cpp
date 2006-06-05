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
 * @version $Revision: 1.11.20.4 $
 *
 */

#include "Log.h"
#include "tailduplicator.h"
#include "irmanager.h"
#include "Dominator.h"
#include "Loop.h"
#include "escapeanalyzer.h"

namespace Jitrino {

DEFINE_OPTPASS_IMPL(RedundantBranchMergingPass, taildup, "Redundant Branch Merging/Factoring")

void
RedundantBranchMergingPass::_run(IRManager& irm) {
    computeDominators(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    assert(dominatorTree->isValid());
    TailDuplicator tm(irm, dominatorTree);
    tm.doTailDuplication();
}

DEFINE_OPTPASS_IMPL(HotPathSplittingPass, hotpath, "Profile Guided Hot Path Splitting")

void
HotPathSplittingPass::_run(IRManager& irm) {
    computeDominatorsAndLoops(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    LoopTree* loopTree = irm.getLoopTree();
    assert(dominatorTree->isValid() && loopTree->isValid());
    TailDuplicator tm(irm, dominatorTree);
    tm.doProfileGuidedTailDuplication(loopTree);
}

bool
TailDuplicator::isMatchingBranch(BranchInst* br1, BranchInst* br2) {
    if(br1->getComparisonModifier() != br2->getComparisonModifier())
        return false;
    if(br1->getNumSrcOperands() == 2 && br2->getNumSrcOperands() == 2) {
        if(br1->getSrc(0) == br2->getSrc(0) && br1->getSrc(1) == br2->getSrc(1))
            return true;
    }
    return false;
}

void
TailDuplicator::process(DefUseBuilder& defUses, DominatorNode* dnode) {
    // Process children.
    DominatorNode* child;
    for(child = dnode->getChild(); child != NULL; child = child->getSiblings()) {
        process(defUses, child);
    }

    CFGNode* node = dnode->getNode();
    BranchInst* branch = node->getLastInst()->asBranchInst();
    if(branch == NULL)
        return;

    // Tail duplicate redundant branches in dominated nodes.
    for(child = dnode->getChild(); child != NULL; child = child->getSiblings()) {
        CFGNode* cnode = child->getNode();
        BranchInst* branch2 = cnode->getLastInst()->asBranchInst();
        if(branch2 != NULL && node->findTarget(cnode) == NULL && isMatchingBranch(branch, branch2))
            tailDuplicate(defUses, node, cnode);
    }
}

void
TailDuplicator::tailDuplicate(DefUseBuilder& defUses, CFGNode* idom, CFGNode* tail) 
{
    if(tail->getInDegree() != 2)
        return;
    CFGNode* t1 = (CFGNode*) idom->getTrueEdge()->getTargetNode();
    CFGNode* f1 = (CFGNode*) idom->getFalseEdge()->getTargetNode();
    
    CFGNode* p1 = (CFGNode*) tail->getInEdges().front()->getSourceNode();
    CFGNode* p2 = (CFGNode*) tail->getInEdges().back()->getSourceNode();

    CFGNode* t2;

    if(_dtree->dominates(t1, p1) && _dtree->dominates(f1, p2)) {
        t2 = p1;
    } else if(_dtree->dominates(t1, p2) && _dtree->dominates(f1, p1)) {
        t2 = p2;
    } else {
        return;
    }

    if(Log::cat_opt_td()->isDebugEnabled()) {
        Log::out() << "Tail duplicate ";
        tail->printLabel(Log::out());
        Log::out() << ::std::endl;
    }

    FlowGraph& fg = _irm.getFlowGraph();
    CFGNode* copy = fg.tailDuplicate(t2, tail, defUses);
    fg.foldBranch(copy, copy->getLastInst()->asBranchInst(), true);
    fg.foldBranch(tail, tail->getLastInst()->asBranchInst(), false);
}

void
TailDuplicator::doTailDuplication() {
    MemoryManager mm(0, "TailDuplicator::doTailDuplication.mm");
    FlowGraph& fg = _irm.getFlowGraph();

    DefUseBuilder defUses(mm);
    defUses.initialize(fg);

    DominatorNode* dnode = _dtree->getDominatorRoot();

    process(defUses, dnode);
}

void
TailDuplicator::profileGuidedTailDuplicate(LoopTree* ltree, DefUseBuilder& defUses, CFGNode* node) {
    FlowGraph& fg = _irm.getFlowGraph();
    
    if(node->getInDegree() < 2)
        // Nothing to duplicate
        return;

    double heatThreshold = _irm.getHeatThreshold();
    
    double nodeCount = node->getFreq();
    Log::cat_opt_td()->debug << "Try nodeCount = " << nodeCount << ::std::endl;
    if(nodeCount < 0.90 * heatThreshold)
        return;
    
    const CFGEdgeDeque& inEdges = node->getInEdges();

    CFGNode* hotPred = NULL;
    CFGEdge* hotEdge = NULL;
    CFGEdgeDeque::const_iterator j;
    for(j = inEdges.begin(); j != inEdges.end(); ++j) {
        CFGEdge* edge = *j;
        CFGNode* pred = edge->getSourceNode();
        if(!pred->isBasicBlock()) {
            hotPred = NULL;
            break;
        }
        if(pred->getFreq() > 0.90 * nodeCount) {
            hotPred = pred;
            hotEdge = edge;
        }
    }

    if(hotPred != NULL) {
        Log::cat_opt_td()->debug << "Duplicate " << ::std::endl;
        double hotProb = hotEdge->getEdgeProb();
        double hotFreq = hotPred->getFreq() * hotProb;
        if(Log::cat_opt_td()->isDebugEnabled()) {
            Log::out() << "Hot Pred = ";
            hotPred->printLabel(Log::out());
            Log::out() << ::std::endl;
            Log::out() << "HotPredProb = " << hotProb << ::std::endl;
            Log::out() << "HotPredFreq = " << hotPred->getFreq() << ::std::endl;
            Log::out() << "HotFreq = " << hotFreq << ::std::endl;
        }
        if(hotFreq > 1.10 * nodeCount)
            assert(0);
        if(hotFreq > nodeCount)
            hotFreq = nodeCount;
        else if(hotFreq < 0.75 * nodeCount)
            return;
        CFGNode* newNode = fg.tailDuplicate(hotPred, node, defUses);
        if(Log::cat_opt_td()->isDebugEnabled()) {
            Log::out() << "New node = ";
            newNode->printLabel(Log::out());
            Log::out() << ::std::endl;
        }
        newNode->setFreq(hotFreq);
        CFGEdge* stBlockEdge = (CFGEdge*) newNode->getUnconditionalEdge();
        if(stBlockEdge != NULL) {
            CFGNode* stBlock = (CFGNode*) stBlockEdge->getTargetNode();
            if(stBlock->getId() > newNode->getId())
                stBlock->setFreq(newNode->getFreq());
        }
        node->setFreq(nodeCount-hotFreq);
        ((CFGEdge*) hotPred->findTarget(newNode))->setEdgeProb(hotProb);

        // Test if node is source of back edge.
        CFGNode* loopHeader = NULL;
        const CFGEdgeDeque& outEdges = node->getOutEdges();
        for(j = outEdges.begin(); j != outEdges.end(); ++j) {
            CFGEdge* edge = *j;
            CFGNode* succ = edge->getTargetNode();
            if(succ->getDfNum() != UINT32_MAX && ltree->isLoopHeader(succ) && succ->getDfNum() < node->getDfNum()) {
                loopHeader = succ;
            }
        }
        
        if(loopHeader != NULL) {
            // Create new loop header:
            CFGNode* newHeader = fg.createBlockNode();

            const CFGEdgeDeque& loopInEdges = loopHeader->getInEdges();
            for(j = loopInEdges.begin(); j != loopInEdges.end();) {
                CFGEdge* edge = *j;
                ++j;
                CFGNode* pred = edge->getSourceNode();
                if(pred != newNode) {
                    fg.replaceEdgeTarget(edge, newHeader);
                    newHeader->setFreq(newHeader->getFreq()+pred->getFreq()*edge->getEdgeProb());
                }
            }
            fg.addEdge(newHeader, loopHeader);            
        }
    }
}

void 
TailDuplicator::doProfileGuidedTailDuplication(LoopTree* ltree) {
    MemoryManager mm(0, "TailDuplicator::doProfileGuidedTailDuplication.mm");
    FlowGraph& fg = _irm.getFlowGraph();
    if(!fg.hasEdgeProfile())
        return;

    DefUseBuilder defUses(mm);
    defUses.initialize(fg);

    StlVector<CFGNode*> nodes(mm);
    fg.getNodesPostOrder(nodes);
    StlVector<CFGNode*>::reverse_iterator i;
    for(i = nodes.rbegin(); i != nodes.rend(); ++i) {
        CFGNode* node = *i;
        if(Log::cat_opt_td()->isDebugEnabled()) {
            Log::out() << "Consider ";
            node->printLabel(Log::out());
            Log::out() << ::std::endl;
        }
        if(!node->isBlockNode() || node == fg.getReturn())
            continue;

        if(ltree->isLoopHeader(node))
            continue;

        profileGuidedTailDuplicate(ltree, defUses, node);
    }
}


} //namespace Jitrino 
