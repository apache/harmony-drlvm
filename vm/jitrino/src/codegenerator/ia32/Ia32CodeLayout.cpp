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
 * @version $Revision: 1.8.20.4 $
 */

#include "Ia32CodeLayout.h"
#include "Ia32IRManager.h"
#include "Log.h"
#include "Ia32Printer.h"
#include "Ia32CodeLayoutTopDown.h"
#include "Ia32CodeLayoutBottomUp.h"
namespace Jitrino
{
namespace Ia32 {

#define ACCEPTABLE_DOUBLE_PRECISION_LOSS  0.000000000001 

Linearizer::Linearizer(IRManager* irMgr) : irManager(irMgr) {
}



/**  Fix branches to work with the code layout */
void Linearizer::fixBranches() {
    const Nodes& nodes = irManager->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            fixBranches((BasicBlock*)node);
        }
    }
}

void Linearizer::fixBranches(BasicBlock* block) { 
    BasicBlock* succBlock = block->getLayoutSucc();
    // If block ends with branch to its layout successor reverse
    // the branch to fallthrough to the layout successor
    if (!block->isEmpty()) {
        Inst * lastInst = block->getInsts().getLast();
        if (lastInst->hasKind(Inst::Kind_BranchInst)) {
            BranchInst* bInst =  ((BranchInst *)lastInst);
            if (bInst->isDirect()) {
                BasicBlock* target =bInst->getDirectBranchTarget();
                if (target == succBlock) {
                    reverseBranchIfPossible(block);
                }
            }
        }
    }

    //  If block's fallthrough successor is not its layout successor
    //  add new block with a jump
    Edge * fallEdge = block->getFallThroughEdge();
    if (fallEdge != NULL && fallEdge->getNode(Direction_Head) != succBlock) {
        BasicBlock * jmpBlk = addJumpBlock(fallEdge);
        //  Put jump block after the block in code layout
        jmpBlk->setLayoutSucc(succBlock);
        block->setLayoutSucc(jmpBlk);
    }
}



/**  Reverse branch predicate. 
     We assume that branch is the last instruction in the node.
  */
bool Linearizer::reverseBranchIfPossible(BasicBlock * bb) {
    //  Last instruction of the basic block should be predicated branch:
    //      (p1) br  N
    //  Find p2 in (p1,p2) or (p2,p1) produced by the compare.
    assert(!bb->isEmpty() && bb->getInsts().getLast()->hasKind(Inst::Kind_BranchInst));
    BranchInst * branch = (BranchInst *)bb->getInsts().getLast();
    assert(branch->isDirect());
    Edge* edge = bb->getDirectBranchEdge();
    if (canEdgeBeMadeToFallThrough(edge)) {
        branch->reverse(irManager);
    }
    return true;
}


 /**  Add block containing jump instruction to the fallthrough successor
  *  after this block
  */
BasicBlock * Linearizer::addJumpBlock(Edge * fallEdge) {
    //  Create basic block for jump inst and add this block to the CFG
    Node* block = fallEdge->getNode(Direction_Tail);
    Node* targetNode= fallEdge->getNode(Direction_Head);
    assert(targetNode->hasKind(Node::Kind_BasicBlock));
    bool loopInfoIsValidOnStart = irManager->isLoopInfoValid();
    Node* loopHeader = loopInfoIsValidOnStart ?  (targetNode->isLoopHeader()? targetNode : targetNode->getLoopHeader()):NULL;//cache in advance
    bool livenessInfoIsValidOnStart = irManager->hasLivenessInfo();
    BasicBlock* targetBlock = (BasicBlock*)targetNode;
    BasicBlock* jumpBlock = irManager->newBasicBlock(fallEdge->getProbability() * block->getExecCnt());
    jumpBlock->setPersistentId(targetBlock->getPersistentId());

    //  Add jump instruction to the created block
    BranchInst * jumpInst = irManager->newBranchInst(Mnemonic_JMP);
    jumpBlock->appendInsts(jumpInst);
    irManager->newDirectBranchEdge(jumpBlock, targetBlock, 1.0);
    
    //  Retarget head of fallthrough edge to the jumpBlock
    irManager->retargetEdge(Direction_Head, fallEdge, jumpBlock);
    
    // now fix liveness and loop infos
    if (livenessInfoIsValidOnStart) {        //fixing liveness info
        assert(jumpInst->getOpndCount() == 1); 
        uint32 nOpnds = irManager->getOpndCount();
#ifdef _DEBUG
        Opnd* opnd = jumpInst->getOpnd(0);
        assert(opnd->getId() == nOpnds - 1);
#endif
        const Nodes& nodes = irManager->getNodes();
        for (Nodes::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; it++) {
            Node* node = *it;
            LiveSet* liveSet = irManager->getLiveAtEntry(node);
            assert(node == jumpBlock || liveSet->getSetSize() == nOpnds - 1);
            liveSet->resize(nOpnds); //fills new opnd info with 0
        }
        LiveSet* jumpBlockLiveness = irManager->getLiveAtEntry(jumpBlock);
        irManager->getLiveAtExit(jumpBlock, *jumpBlockLiveness); // get liveness from child nodes
        irManager->updateLiveness(jumpInst, *irManager->getLiveAtEntry(jumpBlock)); // and add local info to it
        assert(irManager->ensureLivenessInfoIsValid());
    }

    if (loopInfoIsValidOnStart) {    //fixing loop info if needed
        jumpBlock->setLoopInfo(false, loopHeader);
        irManager->forceLoopInfoIsValid();
    }
    return jumpBlock;
}



// Returns true if edge can be converted to a fall-through edge (i.e. an edge
// not requiring a branch) assuming the edge's head block is laid out after the tail block. 
bool Linearizer::canEdgeBeMadeToFallThrough(Edge *edge) {
    BranchInst *br = edge->getBranch();
    if (br == NULL) {
        return TRUE;
    } 
    return br->canReverse();
}

bool Linearizer::isBlockLayoutDone() {
    bool lastBlockFound = FALSE;
    const Nodes& nodes = irManager->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node* node = *it;
        if (!node->hasKind(Node::Kind_BasicBlock)) {
            continue;
        }
        BasicBlock* block  = (BasicBlock*)node;
        if (block->getLayoutSucc()==NULL) {
           if (lastBlockFound) {
               return FALSE; //two blocks without layout successor found
           }
           lastBlockFound = TRUE;
        }
    }
    return lastBlockFound;
}

void Linearizer::linearizeCfg() {
    assert(!irManager->isLaidOut());
#ifdef _DEBUG
    bool livenessIsOkOnStart = irManager->hasLivenessInfo();
#endif
    
    linearizeCfgImpl();
    
    assert(isBlockLayoutDone());
    irManager->setIsLaidOut();
#ifdef _DEBUG
    assert(hasValidLayout(irManager));
    assert(hasValidFallthroughEdges(irManager));
    if (livenessIsOkOnStart) {
        assert(irManager->ensureLivenessInfoIsValid());
    }
#endif
}


void Linearizer::doLayout(LinearizerType t, IRManager* irManager) {
    if (t == TOPDOWN) {
        TopDownLayout linearizer(irManager);
        linearizer.linearizeCfg();
    } else if (t == BOTTOM_UP) {
        BottomUpLayout linearizer(irManager);
        linearizer.linearizeCfg();
    } else {
        assert (t == TOPOLOGICAL);
        TopologicalLayout linearizer(irManager);
        linearizer.linearizeCfg();
    }
}

bool Linearizer::hasValidLayout(IRManager* irm) {
    uint32 maxNodes = irm->getMaxNodeId();
    StlVector<uint32> numVisits(irm->getMemoryManager(), maxNodes); 
    StlVector<bool> isBB(irm->getMemoryManager(), maxNodes);
    uint32 i;

    for (i = 0; i < maxNodes; i++) {
        numVisits[i] = 0;
        isBB[i] = FALSE;
    }
    //    Find basic blocks
    bool foundExit = FALSE;
    const Nodes& nodes = irm->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node *node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            isBB[node->getId()] = TRUE;
            BasicBlock* layoutSucc =((BasicBlock*)node)->getLayoutSucc();
            if (layoutSucc==NULL )  {
                if (foundExit) {
                    if (Log::cat_cg()->isWarnEnabled()) {
                        Log::out() << "two blocks without layout successor\n";
                    }
                    return FALSE;
                } else {
                    foundExit = TRUE;
                }
            } else {
                uint32 id = layoutSucc->getId();
                numVisits[id]++;
                if (numVisits[id] > 1) {
                    return FALSE;
                }
            }
        }
    }

    
    //    Check that every basic block has been visited once
    bool foundEntry = FALSE;
    for (i = 0; i < maxNodes; i++) {
        uint32 correctNumVisits = isBB[i] ? 1 : 0;
        if (numVisits[i] != correctNumVisits) {
            if (isBB[i] && numVisits[i] == 0 && !foundEntry) {
                foundEntry = TRUE;
            } else {
                if (Log::cat_cg()->isWarnEnabled()) {
                    Log::out() << "numVisits[" << (int) i << "] = ";
                    Log::out() << (int) numVisits[i];
                    Log::out() << " expected " << (int) correctNumVisits << ::std::endl;
                }
                return FALSE;
            }
        }
    }
    return TRUE;
}

bool Linearizer::hasValidFallthroughEdges(IRManager* irm) {
    const Nodes& nodes = irm->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it)  {
        Node *node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            BasicBlock* block = (BasicBlock*)node;
            Edge * fallEdge = block->getFallThroughEdge();
            if (fallEdge != NULL && fallEdge->getNode(Direction_Head) != block->getLayoutSucc()) {
                return FALSE;
            }
        } 
    }
    return TRUE;
}


void Linearizer::ensureProfileIsValid() const {
    const Nodes& nodes = irManager->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        const Edges& inEdges = node->getEdges(Direction_In);

#ifdef _DEBUG
        double myFreq = node->getExecCnt();
        assert(myFreq>0);
#endif
        
        
        if (node!=irManager->getPrologNode()) { // checking in-edges, sum freq=myFreq
            double inFreq = 0;
            for (Edge *e = inEdges.getFirst(); e; e = inEdges.getNext(e)) {
                Node* fromNode = e->getNode(Direction_Tail);
                double edgeProb = e->getProbability();
                double dfreq =  edgeProb * fromNode->getExecCnt();
                inFreq+=dfreq;
            }
            assert ((double(abs(int(myFreq -  inFreq))) / myFreq) < ACCEPTABLE_DOUBLE_PRECISION_LOSS); 
        }

        if (node!=irManager->getExitNode()) { //checking out edges -> sum prob == 100%
            const Edges& outEdges = node->getEdges(Direction_Out);
            double prob = 0.0;
            for (Edge *e = outEdges.getFirst(); e; e = outEdges.getNext(e)) {
                double dprob = e->getProbability();
                prob+=dprob;
            }
            assert ((double(abs(int(1 -  prob)))) < ACCEPTABLE_DOUBLE_PRECISION_LOSS); 
        }
    }
}

/////////////////////////////////////////////////////////////////////
/////////////////// TOPOLOGICAL LAYOUT //////////////////////////////

void TopologicalLayout::linearizeCfgImpl() {
    BasicBlock* prev = NULL;
    CFG::NodeIterator it(*irManager, CFG::OrderType_Topological);
    for (; it.getNode()!=NULL; ++it) {
        Node* node = it.getNode();
        if (!node->hasKind(Node::Kind_BasicBlock)) {
            continue;
        }
        BasicBlock* block = (BasicBlock*)node;
        if (prev!= NULL) {
            prev->setLayoutSucc(block);
        } else {
            assert(block == irManager->getPrologNode());
        }
        prev=block; 
    }
    fixBranches();
}

//////////////////////////////////////////////////////////////////////////
///////////////////IRTransformer impl ////////////////////////////////////
void Layouter::runImpl() {
    const char* params = getParameters();
    Linearizer::LinearizerType type = irManager.hasEdgeProfile() ? Linearizer::BOTTOM_UP : Linearizer::TOPOLOGICAL;
    if (params != NULL) {
        if (!strcmp(params, "bottomup")) {
            type = Linearizer::BOTTOM_UP;
        } else if (!strcmp(params, "topdown")) {
            type = Linearizer::TOPDOWN;
        } else if (!strcmp(params, "mixed")) {
            type = irManager.hasLoops() ? Linearizer::BOTTOM_UP  : type = Linearizer::TOPDOWN;
        } else if (!strcmp(params, "topological")) {
            type = Linearizer::TOPOLOGICAL;
        } else {
            if (Log::cat_cg()->isWarnEnabled()) {
                Log::out() << "Layout: unsupported layout type: '"<<params<<"' using default\n";;
            }
        }
    }
    if (type != Linearizer::TOPOLOGICAL && !irManager.hasEdgeProfile()) {
        type = Linearizer::TOPOLOGICAL;
        if (Log::cat_cg()->isWarnEnabled()) {
            Log::out() << "Layout: not edge profile found: '"<<params<<"' using topological layout\n";;
        }
    }
    Linearizer::doLayout(type, &getIRManager());
}

}}//namespace
