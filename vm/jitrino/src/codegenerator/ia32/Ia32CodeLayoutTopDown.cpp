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
 * @version $Revision: 1.11.12.1.4.4 $
 */

#include "Ia32CodeLayoutTopDown.h"
#include "Ia32IRManager.h"
#include "Log.h"
#include "Ia32Printer.h"
namespace Jitrino
{
namespace Ia32 {
/**
* Class to perform top-down block layout. 
* The layout algorithm is similar to the one described in the paper
* "Profile Guided Code Positioning" by Hansen & Pettis. 
*/

TopDownLayout::TopDownLayout(IRManager* irm) 
: Linearizer(irm), 
memManager(40*irm->getMaxNodeId(), "ia32::topdown_layout"),
lastBlk(NULL), 
neighboursBlocks(memManager),
blockInfos(memManager, irm->getMaxNodeId()+1, NULL)
{
    const Nodes& nodes = irManager->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node * node=*it;
        if (node->hasKind(Node::Kind_BasicBlock)){
            TopDownLayoutBlockInfo* info = new (memManager) TopDownLayoutBlockInfo();
            info->block = (BasicBlock*)node;
            blockInfos[node->getId()]= info;
        }
    }    
}

//  Do complete top down code layout
void TopDownLayout::linearizeCfgImpl() {
    irManager->updateExecCounts();
#ifdef _DEBUG
    ensureProfileIsValid();
#endif
    BasicBlock * blk;
    startBlockLayout();
    while ((blk = pickLayoutCandidate()) != NULL) {
        layoutBlock(blk);
    }
    endBlockLayout();
}


// Start top down block layout
void TopDownLayout::startBlockLayout() {
    lastBlk = NULL;

    // Check that nodes have no layout successors set
#ifdef _DEBUG
    for (CFG::NodeIterator it(*irManager, CFG::OrderType_Postorder); it.getNode()!=NULL; ++it){
        Node * node=it.getNode();
        if (node->hasKind(Node::Kind_BasicBlock)){
            assert(((BasicBlock *)node)->getLayoutSucc()==NULL);
        }
    }
#endif
}

// Called at the end of top down layout
void TopDownLayout::endBlockLayout() {

	if (lastBlk) {
        fixBranches(lastBlk);
    }
}



// Layout "blk" after "lastBlk". Do all bookkeeping and branch updates as needed
void TopDownLayout::layoutBlock(BasicBlock *blk) {
    TopDownLayoutBlockInfo* bInfo = blockInfos[blk->getId()];
    assert(!bInfo->isLayouted());
    if (Log::cat_cg()->isDebugEnabled()) {
        Log::out() << "layoutBlock(";
        IRPrinter::printNodeName(Log::out(), blk);
        Log::out() << ")" << ::std::endl;
    }
    // Remove the block from the localityMap if it is there
    if (bInfo->isLayoutNeighbour()) {
        neighboursBlocks.erase(bInfo);
    }
    bInfo->state = TopDownLayoutBlockInfo::LAYOUTED;

    if (lastBlk) {
        lastBlk->setLayoutSucc(blk);
        // Add/fix any branches due to this layout decision.
        fixBranches(lastBlk);
    } else {
        // Check our assumption that the first block laid-out is the entry block.
        assert(blk== irManager->getPrologNode());
    }
    lastBlk = blk;
}



// Pick and return the next block to be laid-out
#define PROB_SIMILAR_FACTOR     0.9

BasicBlock * TopDownLayout::pickLayoutCandidate() {
    if (lastBlk == NULL) {
        // Layout the entry block
        return irManager->getPrologNode();
    }
    // Return most likely successor of lastBlk if it has not already been placed
    // and if branch inversion is either not needed or is possible 
    // (i.e. predicate is available). Given two almost equally likely 
    // successors, pick the one that is not a Join block. This will help 
    // reduce taken branches.
    Edge *bestEdge = NULL;
    const Edges& outEdges=lastBlk->getEdges(Direction_Out);
    for(Edge* edge = outEdges.getFirst(); edge!=NULL; edge = outEdges.getNext(edge)) {
        Node *succ = edge->getNode(Direction_Head);
        if (!succ->hasKind(Node::Kind_BasicBlock)) {
            continue;
        }
        TopDownLayoutBlockInfo* info =  blockInfos[succ->getId()];
        if (info->isLayouted() ) {
            continue;
        }
        if (!edge->isFallThroughEdge() && !canEdgeBeMadeToFallThrough(edge)) {
            continue;
        }
        if (bestEdge == NULL) {
            bestEdge = edge;
        }
        double bestEdgeWeight = bestEdge->getProbability();
        double edgeWeight = edge->getProbability();

        if ( edgeWeight > bestEdgeWeight || (edgeWeight >= bestEdgeWeight * PROB_SIMILAR_FACTOR 
            && edge->getNode(Direction_Head)->getEdges(Direction_In).getCount() == 1 
            &&  bestEdge->getNode(Direction_Head)->getEdges(Direction_In).getCount() > 1)) 
        {
            bestEdge = edge;
        }
    }

    // Before returning or choosing a block from the connectivity map, update 
    // the layoutValue information to successors not chosen or already laid out. 
    if (bestEdge) {
        BasicBlock* headBlock = (BasicBlock*)bestEdge->getNode(Direction_Head);
        processSuccLayoutValue(lastBlk, headBlock);
        return headBlock;
    }

    processSuccLayoutValue(lastBlk, NULL);
    
#if _DEBUG  
    // Find node with the greatest layoutValue in the connectedBlkMap
    // Check assumption that the iterator accesses map elements in ordered fashion
    const TopDownLayoutBlockInfo* prevInfo = NULL;
    for (SortedBlockInfoSet::iterator it = neighboursBlocks.begin(), end = neighboursBlocks.end(); it!=end; ++it) {
        TopDownLayoutBlockInfo *bInfo = *it;
        assert(prevInfo == NULL || TopDownLayoutBlockInfo::less(bInfo, prevInfo));
        prevInfo = bInfo;
    }
#endif
    if (Log::cat_cg()->isDebugEnabled()) {
        printConnectedBlkMap(Log::out());
    }

    if (neighboursBlocks.empty()) {
        return NULL;
    }

    TopDownLayoutBlockInfo* info = *neighboursBlocks.begin();
    BasicBlock * locBlk = info->block;
    if (Log::cat_cg()->isDebugEnabled()) {
        Log::out() << "Picking ";
        IRPrinter::printNodeName(Log::out(), locBlk);
        Log::out() << " " << info->layoutValue<< ::std::endl;
    }
    assert(info->isLayoutNeighbour());
    return locBlk;
}


// Update layoutValue information for all successors of node that have not yet been laid out. 
// If a successor is a dispatch node, recursively process its successors, since 
// dispatch nodes are not being laid out.
void TopDownLayout::processSuccLayoutValue(Node *node,  BasicBlock * layoutSucc) {
    const Edges& outEdges = node->getEdges(Direction_Out);
    for (Edge *edge = outEdges.getFirst();edge; edge = outEdges.getNext(edge)) {
        Node *succ = edge->getNode(Direction_Head);
        if (succ->hasKind(Node::Kind_DispatchNode)) {
            processSuccLayoutValue(succ, layoutSucc);
        } else if (succ->hasKind(Node::Kind_BasicBlock)) {
            TopDownLayoutBlockInfo* succInfo = blockInfos[succ->getId()];
            if (succ != layoutSucc && !succInfo->isLayouted()) {
                if (succInfo->isLayoutNeighbour()) { //remove from sorted map and insert latter to sort again.
                    neighboursBlocks.erase(succInfo);
                } 
                succInfo->layoutValue+=node->getExecCnt() * edge->getProbability();
                succInfo->state = TopDownLayoutBlockInfo::LAYOUT_NEIGHBOUR;
                neighboursBlocks.insert(succInfo);

                if (Log::cat_cg()->isDebugEnabled()) {
                    Log::out() << "Block ";
                    IRPrinter::printNodeName(Log::out(), succInfo->block);
                    Log::out() << " is in neighbors set." << ::std::endl;
                }
            }
        } else  {
            assert(succ->hasKind(Node::Kind_UnwindNode) || succ->hasKind(Node::Kind_ExitNode));
        }
    }
}


//
// Print the contents of the connectivity map
//
void TopDownLayout::printConnectedBlkMap(::std::ostream & os) {
    os << "Neighbors set contents: ";
    for (SortedBlockInfoSet::iterator it = neighboursBlocks.begin(), end = neighboursBlocks.end(); it != end; ++it) {
        TopDownLayoutBlockInfo* info= *it;
        IRPrinter::printNodeName(os, info->block);
        os << " " << info->layoutValue << ::std::endl;
    }
}

}
}
