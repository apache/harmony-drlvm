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
 * @version $Revision: 1.20.8.1.4.4 $
 *
 */

#include <algorithm>
#include "Log.h"
#include "FlowGraph.h"
#include "Inst.h"
#include "Dominator.h"
#include "Loop.h"
#include "globalopndanalyzer.h"
#include "optimizer.h"
#include "escapeanalyzer.h"

namespace Jitrino {

DEFINE_OPTPASS_IMPL(LoopPeelingPass, peel, "Loop Peeling")

void
LoopPeelingPass::_run(IRManager& irm) {
    computeDominators(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    assert(dominatorTree->isValid());
    LoopBuilder lb(irm.getNestedMemoryManager(),
                   irm, *dominatorTree, irm.getFlowGraph().hasEdgeProfile());
    LoopTree* loopTree = lb.computeAndNormalizeLoops(true);
    if (lb.needSsaFixup()) {
        fixupSsa(irm);
    }
    irm.setLoopTree(loopTree);
    smoothProfile(irm);
}

LoopBuilder::LoopBuilder(MemoryManager& mm, IRManager& irm, 
                         DominatorTree& d, bool useProfile) 
    : loopMemManager(mm), dom(d), irManager(irm), 
      instFactory(irm.getInstFactory()), fg(irm.getFlowGraph()), info(NULL), root(NULL), 
      useProfile(useProfile), needsSsaFixup(false) 
{
    JitrinoParameterTable& params = irManager.getParameterTable();
    flags.hoist_loads = params.lookupBool("opt::loop::hoist_loads", false);
    flags.invert = params.lookupBool("opt::loop::invert", false);
    flags.peel = params.lookupBool("opt::loop::peel", true);
    flags.insideout_peeling = params.lookupBool("opt::loop::insideout_peeling", false);
    flags.old_static_peeling = params.lookupBool("opt::loop::old_static_peeling", false);
    flags.aggressive_peeling = params.lookupBool("opt::loop::aggressive_peeling", true);
    flags.peel_upto_branch = params.lookupBool("opt::loop::peel_upto_branch", true);
    flags.peeling_threshold = params.lookupUint("opt::loop::peeling_threshold", 2);
    flags.fullpeel = params.lookupBool("opt::loop::fullpeel", false);
    flags.fullpeel_max_inst = params.lookupUint("opt::loop::fullpeel_max_inst", 40);
    flags.unroll = params.lookupBool("opt::loop::unroll", false);
    flags.unroll_count = params.lookupUint("opt::loop::unroll_count", 4);
    flags.unroll_threshold = params.lookupUint("opt::loop::unroll_threshold", 10);
    flags.eliminate_critical_back_edge = params.lookupBool("opt::loop::eliminate_critical_back_edge", false);
    flags.peel_upto_branch_no_instanceof = params.lookupBool("opt::loop::peel_upto_branch::no_instanceof", true);
}

void LoopBuilder::showFlagsFromCommandLine()
{
    Log::out() << "    opt::loop::hoist_loads[={on|OFF}] = peel assuming load hoisting" << ::std::endl;
    Log::out() << "    opt::loop::invert[={on|OFF}] = try to invert loops to reduce branches" << ::std::endl;
    Log::out() << "    opt::loop::peel[={ON|off}] = do peeling" << ::std::endl;
    Log::out() << "    opt::loop::insideout_peeling[={on|OFF}] = ?" << ::std::endl;
    Log::out() << "    opt::loop::aggressive_peeling[={ON|off}] = be aggressive about peeling" << ::std::endl;
    Log::out() << "    opt::loop::peel_upto_branch[={on|OFF}] = peel only up to a branch" << ::std::endl;
    Log::out() << "    opt::loop::old_static_peeling[={on|OFF}] = use old-style peeling for static runs" << ::std::endl;
    Log::out() << "    opt::loop::peeling_threshold[=int] = ? (default 2)" << ::std::endl;
    Log::out() << "    opt::loop::unroll[={on|OFF}] = ?" << ::std::endl;
    Log::out() << "    opt::loop::unroll_count[=int] = ? (default 4)" << ::std::endl;
    Log::out() << "    opt::loop::unroll_threshold[=int] = ? (default 10)" << ::std::endl;
    Log::out() << "    opt::loop::eliminate_critical_back_edge[={on|OFF}] = ?" << ::std::endl;
    Log::out() << "    opt::loop::peel_upto_branch::no_instanceof[={on|OFF}] = with peel_upto_branch, peel only up to a branch or instanceof" << ::std::endl;
}

//
// If loop A is contained in loop B, then B's header must dominate 
// A's header.  We sort loop edges based on headers' dfn so as to
// construct loops from the outermost to the innermost
//
void LoopBuilder::formLoopHierarchy(StlVector<CFGEdge*>& edges,
                                      uint32 numLoops) {
    if (numLoops == 0)
        return;

    // new blocks are created, recompute df num
    uint32 numBlocks = fg.getNodeCount();

    // create a root loop containing all loops within the method
    createLoop(NULL,NULL,numBlocks);

    fg.getNodeCount();

    // create loops in df num order
    for (uint32 j = 0; j < numLoops; j++) 
        createLoop(edges[j]->getTargetNode(), edges[j]->getSourceNode(), numBlocks);

}

template <class IDFun>
class LoopMarker {
public:
    static uint32 markNodesOfLoop(StlBitVector& nodesInLoop, CFGNode* header, CFGNode* tail);
private:
    static uint32 backwardMarkNode(StlBitVector& nodesInLoop, CFGNode* node, StlBitVector& visited);
    static uint32 countInsts(CFGNode* node);
};

template <class IDFun>
uint32 LoopMarker<IDFun>::countInsts(CFGNode* node) {
    uint32 count = 0;
    Inst* first = node->getFirstInst();
    for(Inst* inst = first->next(); inst != first; inst = inst->next())
        ++count;
    return count;
}

template <class IDFun>
uint32 LoopMarker<IDFun>::backwardMarkNode(StlBitVector& nodesInLoop, 
                                CFGNode* node, 
                                StlBitVector& visited) {
    static IDFun getId;
    if(visited.setBit(node->getId()))
        return 0;

    uint32 count = countInsts(node);
    nodesInLoop.setBit(getId(node));

    const CFGEdgeDeque& inEdges = node->getInEdges();
    CFGEdgeDeque::const_iterator eiter;
    for(eiter = inEdges.begin(); eiter != inEdges.end(); ++eiter) {
        CFGEdge* e = *eiter;
        count += backwardMarkNode(nodesInLoop, e->getSourceNode(), visited);
    }
    return count;
}

// 
// traverse graph backward starting from tail until header is reached
//
template <class IDFun>
uint32 LoopMarker<IDFun>::markNodesOfLoop(StlBitVector& nodesInLoop,
                               CFGNode* header,
                               CFGNode* tail) {
    static IDFun getId;
    uint32 maxSize = ::std::max(getId(header), getId(tail));
    MemoryManager tmm(maxSize >> 3, "LoopBuilder::markNodesOfLoop.tmm");
    StlBitVector visited(tmm, maxSize);

    // mark header is visited
    uint32 count = countInsts(header);
    nodesInLoop.setBit(getId(header));
    visited.setBit(header->getId());

    // starting backward traversal
    return count + backwardMarkNode(nodesInLoop, tail, visited);
}

uint32 LoopBuilder::markNodesOfLoop(StlBitVector& nodesInLoop,
                               CFGNode* header,
                               CFGNode* tail) {
    return LoopMarker<IdentifyByDFN>::markNodesOfLoop(nodesInLoop, header, tail);
}


//
// the current loop contains the header
// we traverse descendants to find who contains the header and return the 
// descendant loop
//
LoopNode* LoopBuilder::findEnclosingLoop(LoopNode* loop, CFGNode* header) {
    for (LoopNode* l = loop->getChild(); l != NULL; l = l->getSiblings())
        if (l->inLoop(header))
            return findEnclosingLoop(l, header);

    // if no descendant contains the header then return loop
    return loop;
}

void LoopBuilder::createLoop(CFGNode* header, 
                          CFGNode* tail,
                          uint32   numBlocks) {
    if (header == NULL)  // root loop
        root = new (loopMemManager) LoopNode(NULL, NULL); 
    else {
        // walk up the hierarchy to find which loop contains it
        StlBitVector* nodesInLoop = new (loopMemManager) StlBitVector(loopMemManager,numBlocks);
        LoopNode* loop = new (loopMemManager) LoopNode(header, nodesInLoop);
        findEnclosingLoop((LoopNode*) root, header)->addChild(loop);
        
        // make blocks within the loop using backward depth first
        // search starting from tail
        markNodesOfLoop(*nodesInLoop, header, tail);
    }
}

// numLoopEdges loops share the same header
// create a new block that sinks all loop edges (the edges are no longer
// loop edges).  create a new loop edge <block,header>.
// return the newly created block.
//
CFGEdge* LoopBuilder::coalesceEdges(StlVector<CFGEdge*>& edges,
                                    uint32 numEdges) {
    CFGNode* header = edges.back()->getTargetNode();

    // create a new block and re-target all loop edges to the block
    CFGNode* block = fg.createBlock(instFactory.makeLabel());
    
    for (uint32 nE=numEdges; nE != 0; nE--) {
        // remove edge <pred,header>
        CFGEdge* e = edges.back();
        edges.pop_back();
        assert(e->getTargetNode() == header);  // must share the same header

        fg.replaceEdgeTarget(e, block);
        block->setFreq(block->getFreq()+e->getSourceNode()->getFreq()*e->getEdgeProb());
    }
    if (numEdges > 1) {
        // copy any phi nodes from successor
        Inst* phi = header->getFirstInst()->next();
        for (; phi->isPhi(); phi = phi->next()) {
            Opnd *orgDst = phi->getDst();
            SsaVarOpnd *ssaOrgDst = orgDst->asSsaVarOpnd();
            assert(ssaOrgDst);
            VarOpnd *theVar = ssaOrgDst->getVar();
            assert(theVar);
            SsaVarOpnd* newDst = irManager.getOpndManager().createSsaVarOpnd(theVar);
            Inst *newInst = instFactory.makePhi(newDst, 0, 0);
            PhiInst *newPhiInst = newInst->asPhiInst();
            assert(newPhiInst);
            uint32 n = phi->getNumSrcOperands();
            for (uint32 i=0; i<n; i++) {
                instFactory.appendSrc(newPhiInst, phi->getSrc(i));
            }
            PhiInst *phiInst = phi->asPhiInst();
            assert(phiInst);
            instFactory.appendSrc(phiInst, newDst);
            block->prependAfterCriticalInst(newInst);
            needsSsaFixup = true;
        }
    }
    // create factored edge <block,header>
    CFGEdge* edge = fg.addEdge(block,header);
    edge->setEdgeProb(1.0);
    return edge;
}

//
// check each header to see if multiple loops share the head
// if found, coalesce those loop edges.
// return number of found loops 
uint32 LoopBuilder::findLoopEdges(MemoryManager&     mm,
                               StlVector<CFGNode*>&  headers,      // in
                               StlVector<CFGEdge*>& loopEdges) {  // out
    uint32 numLoops = 0;
    while (!headers.empty()) {
        MemoryManager tmm(16*sizeof(CFGEdge*)*2,"LoopBuilder::findLoopEdges.tmm");
        StlVector<CFGEdge*> entryEdges(tmm);
        
        CFGNode* head = headers.back();
        headers.pop_back();

        uint32 numLoopEdges = 0;
        uint32 numEntryEdges = 0;
        // if n dominates its predecessor, then the edge is an loop back edge
        const CFGEdgeDeque& inEdges = head->getInEdges();
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = inEdges.begin(); eiter != inEdges.end(); ++eiter) {
            CFGNode* pred = (*eiter)->getSourceNode();
            if (dom.dominates(head, pred)) {
                loopEdges.push_back(*eiter);
                numLoopEdges++;
            } else {
                entryEdges.push_back(*eiter);
                numEntryEdges++;
            }
        }
        assert(numLoopEdges != 0 && numEntryEdges != 0); 
        numLoops++;

        if (head->isBasicBlock()) {
            // Create unique preheader if 
            // a) more than one entry edge or
            // b) the one entry edge is a critical edge 
            if ((numEntryEdges > 1) || (entryEdges.front()->getSourceNode()->getOutDegree() > 1)) {
                coalesceEdges(entryEdges, numEntryEdges);
            }

            // Create unique tail if
            // a) more than one back edge or
            // b) the one back edge is a critical edge
            if ((numLoopEdges > 1) || (flags.eliminate_critical_back_edge && (loopEdges.front()->getSourceNode()->getOutDegree() > 1))) {
                CFGEdge* backEdge = coalesceEdges(loopEdges, numLoopEdges);
                loopEdges.push_back(backEdge);
            }
        }
    }
    return numLoops;
}

// 
// we find loop headers and then find loop edges because coalesceLoops 
// introduces new blocks and edges into the flow graph.  dom does not
// dominator information for the newly created blocks.
//
void LoopBuilder::findLoopHeaders(StlVector<CFGNode*>& headers) { // out
    const CFGNodeDeque& nodes = fg.getNodes();
    CFGNodeDeque::const_iterator niter;
    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* n = *niter;
        // if n dominates its predecessor, then the edge is an loop back edge
        const CFGEdgeDeque& edges = n->getInEdges();
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = edges.begin(); eiter != edges.end(); ++eiter) {
            CFGNode* pred = (*eiter)->getSourceNode();
            if (pred->isBlockNode() && dom.dominates(n, pred)) {
                headers.push_back(n);
                break;
            }
        }
    }
}

bool noStoreOrSynch(FlowGraph& fg) {
    const CFGNodeDeque& nodes = fg.getNodes();
    CFGNodeDeque::const_iterator niter;
    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* n = *niter;
        Inst* first = n->getFirstInst();
        for(Inst* inst = first->next(); inst != first; inst = inst->next()) {
            if (inst->getOperation().isStoreOrSync()) {
                return false;
            }
        }
    }
    return true;
}

bool LoopBuilder::isVariantOperation(Operation operation) {
    if (operation.isCheck())
        return false;
    if (operation.canThrow()) {
        return true;
    }
    if (!flags.hoist_loads) {
        if (operation.isLoad())
            return true;
    }
    if (operation.isCSEable()) {
        return false;
    }
    return true;
}


bool LoopBuilder::isVariantInst(Inst* inst, StlHashSet<Opnd*>& variantOpnds) {
    Operation operation = inst->getOperation();
    Opcode op = operation.getOpcode();

    bool isfinalload = false;
    OptimizerFlags& optimizerFlags = *irManager.getCompilationContext()->getOptimizerFlags();
    if (optimizerFlags.cse_final) {
        switch (inst->getOpcode()) {
        case Op_TauLdInd: 
            {
                Inst *srcInst = inst->getSrc(0)->getInst();
                if ((srcInst->getOpcode() == Op_LdFieldAddr) ||
                    (srcInst->getOpcode() == Op_LdStaticAddr)) {
                    FieldAccessInst *faInst = srcInst->asFieldAccessInst();
                    FieldDesc *fd = faInst->getFieldDesc();
                    if (fd->isInitOnly()) {

                        // first check for System stream final fields which vary
                        NamedType *td = fd->getParentType();
                        if (strncmp(td->getName(),"java/lang/System",20)==0) {
                            const char *fdname = fd->getName();
                            if ((strncmp(fdname,"in",5)==0) ||
                                (strncmp(fdname,"out",5)==0) ||
                                (strncmp(fdname,"err",5)==0)) {
                                break;
                            }
                        }

                        isfinalload = true;
                    }
                }
            } 
            break;
        case Op_LdStatic:
            {
                FieldAccessInst *inst1 = inst->asFieldAccessInst();
                assert(inst1);
                FieldDesc* fd = inst1->getFieldDesc();
                if (fd->isInitOnly()) {

                    // first check for System stream final fields which vary
                    NamedType *td = fd->getParentType();
                    if (strncmp(td->getName(),"java/lang/System",20)==0) {
                        const char *fdname = fd->getName();
                        if ((strncmp(fdname,"in",5)==0) ||
                            (strncmp(fdname,"out",5)==0) ||
                            (strncmp(fdname,"err",5)==0)) {
                            break;
                        }
                    }

                    isfinalload = true;
                }
            }
            break;
        case Op_TauLdField:
            {
                FieldAccessInst *inst1 = inst->asFieldAccessInst();
                assert(inst1);
                FieldDesc* fd = inst1->getFieldDesc();
                if (fd->isInitOnly()) {

                    // first check for System stream final fields which vary
                    NamedType *td = fd->getParentType();
                    if (strncmp(td->getName(),"java/lang/System",20)==0) {
                        const char *fdname = fd->getName();
                        if ((strncmp(fdname,"in",5)==0) ||
                            (strncmp(fdname,"out",5)==0) ||
                            (strncmp(fdname,"err",5)==0)) {
                            break;
                        }
                    }

                    isfinalload = true;
                }
            }
            break;
        default:
            break;
        };
    }

    // Inst has potential side effect
    if (isVariantOperation(operation) && !isfinalload)
        return true;

    // Is merge point.  Variant depending on incoming path.
    if (op == Op_LdVar || op == Op_Phi)
        return true;

    // Check for variant src.
    uint32 n = inst->getNumSrcOperands();
    for(uint32 i = 0; i < n; ++i) {
        Opnd* src = inst->getSrc(i);
        if (variantOpnds.find(src) != variantOpnds.end())
            return true;
    }

    return false;
}

void LoopBuilder::hoistHeaderInvariants(CFGNode* preheader, CFGNode* header, StlVector<Inst*>& invariantInsts) {
    assert(preheader->getOutDegree() == 1 && preheader->getOutEdges().front()->getTargetNode() == header);

    StlVector<Inst*>::iterator iiter;
    for(iiter = invariantInsts.begin(); iiter != invariantInsts.end(); ++iiter) {
        Inst* inst = *iiter;
        if (inst == header->getLastInst() && header->getOutDegree() > 1) {
            // Don't hoist control flow instruction.
            break;
        }
        inst->unlink();
        preheader->append(inst);
    }
}


bool LoopBuilder::isInversionCandidate(CFGNode* originalHeader, CFGNode* header, StlBitVector& nodesInLoop, CFGNode*& next, CFGNode*& exit) {
    if(Log::cat_opt_loop()->isDebugEnabled()) {
        Log::out() << "Consider rotating ";
        header->printLabel(Log::out());
        Log::out() << "  df = (" << (int) header->getDfNum() << ")" << ::std::endl;
    }
    assert(nodesInLoop.getBit(header->getId()));

    if (flags.aggressive_peeling) {
        if (header->getOutDegree() == 1) {
            next = header->getOutEdges().front()->getTargetNode();
            if(next->getInDegree() > 1 && next != originalHeader
			) {
                // A pre-header for an inner loop - do not peel.
                Log::cat_opt_loop()->debug << "Stop peeling node at L" << (int) header->getLabelId() << ": preheader for inner loop. " << ::std::endl;
                return false;
            }
            exit = NULL;
            return true;
        }
    }

    if (header->getOutDegree() != 2) {
        Log::cat_opt_loop()->debug << "Stop peeling node at L" << (int) header->getLabelId() << ": no exit. " << ::std::endl;
        return false;
    }

    CFGNode* succ1 = header->getOutEdges().front()->getTargetNode();
    CFGNode* succ2 = header->getOutEdges().back()->getTargetNode();
    
    // Check if either succ is an exit.
    if(nodesInLoop.getBit(succ1->getId())) {
        if(!nodesInLoop.getBit(succ2->getId())) {
            next = succ1;
            exit = succ2;
        } else {
            // Both succ's are in loop, no inversion.
            Log::cat_opt_loop()->debug << "Stop peeling node at L" << (int) header->getLabelId() << ": no exit. " << ::std::endl;
            return false;
        }
    } else {
        // At least one succ of a header must be in the loop.
        assert(nodesInLoop.getBit(succ2->getId()));
        next = succ2;
        exit = succ1;
    }

    if(next->getInDegree() > 1 && next != originalHeader
	) {
        // If next has more than one in edge, it must be the header
        // of an inner loop.  Do not peel its preheader.
        Log::cat_opt_loop()->debug << "Stop peeling node at L" << (int) header->getLabelId() << ": preheader for inner loop. " << ::std::endl;
        return false;
    }
    
    return true;
}

// Perform loop inversion for each recorded loop.
void LoopBuilder::peelLoops(StlVector<CFGEdge*>& loopEdgesIn) {
    // Mark the temporaries that are local to a given basic block
    GlobalOpndAnalyzer globalOpndAnalyzer(irManager);
    globalOpndAnalyzer.doAnalysis();

    // Set def-use chains
    MemoryManager peelmem(1000, "LoopBuilder::peelLoops.peelmem");
    DefUseBuilder defUses(peelmem);
    defUses.initialize(irManager.getFlowGraph());

    OpndManager& opndManager = irManager.getOpndManager();
    InstFactory& instFactory = irManager.getInstFactory();

    // Threshold at which a block is considered hot
    double heatThreshold = irManager.getHeatThreshold();

    StlVector<CFGEdge*> loopEdges(peelmem);
    if(flags.insideout_peeling) {
        StlVector<CFGEdge*>::reverse_iterator i = loopEdgesIn.rbegin(); while (i != loopEdgesIn.rend()) {
            loopEdges.insert(loopEdges.end(), *i);
            i++;
        }
    } else {
        StlVector<CFGEdge*>::iterator i = loopEdgesIn.begin();
        while (i != loopEdgesIn.end()) {
            loopEdges.insert(loopEdges.end(), *i);
            i++;
        }
    }

    StlVector<CFGEdge*>::iterator i;
    for(i = loopEdges.begin(); i != loopEdges.end(); ++i) {
        // The current back-edge
        CFGEdge* backEdge = *i;

        // The initial header
        CFGNode* header = backEdge->getTargetNode();
        assert(header->getInDegree() == 2);
        CFGNode* originalInvertedHeader = header;

        // The initial loop end
        CFGNode* tail = backEdge->getSourceNode();

        // The initial preheader
        CFGNode* preheader = header->getInEdges().front()->getSourceNode();
        if (preheader == tail)
            preheader = header->getInEdges().back()->getSourceNode();

        // Temporary memory for peeling
        uint32 maxSize = fg.getMaxNodeId();
        MemoryManager tmm(maxSize, "LoopBuilder::peelLoops.tmm");
        
        // Compute nodes in loop
        StlBitVector nodesInLoop(tmm, maxSize);
        uint32 loopSize = LoopMarker<IdentifyByID>::markNodesOfLoop(nodesInLoop, header, tail);

        // Operand renaming table for duplicated nodes
        OpndRenameTable* duplicateTable = new (tmm) OpndRenameTable(tmm);

        // Perform loop inversion until header has no exit condition or 
        // until one iteration is peeled.
        CFGNode* next;
        CFGNode* exit;

        if (useProfile || !flags.old_static_peeling) {
            //
            // Only peel hot loops
            //
            if(useProfile) {
                double preheaderFreq = preheader->getFreq();
                if(header->getFreq() < heatThreshold || header->getFreq() < preheaderFreq*flags.peeling_threshold || header->getFreq() == 0)
                    continue;
            }
            
            //
            // Rotate loop one more time if the tail is not a branch (off by default)
            //
            Opcode op1 = tail->getLastInst()->getOpcode();
            Opcode op2 = header->getLastInst()->getOpcode();
            if(flags.invert && (op1 != Op_Branch) && (op1 != Op_PredBranch)
               && (op2 != Op_Branch) && (op2 != Op_PredBranch)) {
                if(isInversionCandidate(originalInvertedHeader, header, nodesInLoop, next, exit)) {
                    preheader = fg.tailDuplicate(preheader, header, defUses); 
                    tail = header;
                    header = next;
                    assert(tail->findTarget(header) != NULL && preheader->findTarget(header) != NULL);
                    if(tail->findTarget(header) == NULL) {
                        tail = (CFGNode*) tail->getUnconditionalEdge()->getTargetNode();
                        assert(tail != NULL && tail->findTarget(header) != NULL);
                    }
                    originalInvertedHeader = header;
                }
            }
            
            //
            // Compute blocks to peel
            //
            CFGNode* newHeader = header;
            CFGNode* newTail = NULL;
            StlBitVector nodesToPeel(tmm, maxSize);
            if(flags.fullpeel) {
                if(loopSize <= flags.fullpeel_max_inst) {
                    nodesToPeel = nodesInLoop;
                    newTail = tail;
                }
            } else {
                while(isInversionCandidate(originalInvertedHeader, newHeader, nodesInLoop, next, exit)) {
                    if(Log::cat_opt_loop()->isDebugEnabled()) {
                        Log::out() << "Peel ";
                        newHeader->printLabel(Log::out());
                        Log::out() << ::std::endl;
                    }
                    newTail = newHeader;
                    nodesToPeel.setBit(newHeader->getId());
                    newHeader = next;
                    if(newHeader == originalInvertedHeader) {
                        Log::cat_opt_loop()->debug << "One iteration peeled" << ::std::endl;
                        break;
                    }
                }
            }
            if(flags.peel) {
                header = newHeader;
                if(flags.peel_upto_branch) {
                    //
                    // Break final header at branch to peel more instructions
                    //
                    if(header != originalInvertedHeader) {
                        Inst* last = header->getLastInst();
                        if(header->getOutDegree() > 1)
                            last = last->prev();
                        if (flags.peel_upto_branch_no_instanceof) {
                            Inst *first = header->getFirstInst();
                            while ((first != last) &&
                                   (first->getOpcode() != Op_TauInstanceOf)) {
                                first = first->next();
                            }
                            last = first;
                        }
                        CFGNode* sinkNode = fg.splitNodeAtInstruction(last);
                        newTail = header;
                        nodesToPeel.resize(sinkNode->getId()+1);
                        nodesToPeel.setBit(header->getId());
                        if(Log::cat_opt_loop()->isDebugEnabled()) {
                            Log::out() << "Sink Node = ";
                            sinkNode->printLabel(Log::out());
                            Log::out() << ", Header = ";
                            header->printLabel(Log::out());
                            Log::out() << ", Original Header = ";
                            originalInvertedHeader->printLabel(Log::out());
                            Log::out() << ::std::endl;
                        }
                        header = sinkNode;
                    }
                }
                
                //
                // Peel the nodes
                //
                if(newTail != NULL) {
                    CFGEdge* entryEdge = (CFGEdge*) preheader->findTarget(originalInvertedHeader);
                    double peeledFreq = preheader->getFreq() * entryEdge->getEdgeProb(); 
                    CFGNode* peeled = fg.duplicateRegion(originalInvertedHeader, nodesToPeel, defUses, peeledFreq);
                    fg.replaceEdgeTarget(entryEdge, peeled);
                    if(newTail->findTarget(header) == NULL) {
                        //
                        // An intervening stVar block was added to promote a temp to a var 
                        // in the duplicated block.  The new tail should be the stVar block.
                        //
                        tail = (CFGNode*) newTail->getUnconditionalEdge()->getTargetNode();
                        assert(tail != NULL && tail->findTarget(header) != NULL);
                    } else {
                        tail = newTail;
                    }
                    assert(header->getInDegree() == 2);
                    preheader = header->getInEdges().front()->getSourceNode();
                    if(preheader == tail)
                        preheader = header->getInEdges().back()->getSourceNode();            
                }            
            }

            if(flags.unroll && newHeader == originalInvertedHeader && newTail != NULL && header->getFreq() >= heatThreshold && (header->getFreq() >= flags.unroll_threshold * preheader->getFreq())) {
                header = newHeader;
                // n is the number of times to unroll the loop
                uint32 n = ::std::min(flags.unroll_count, (uint32) (header->getFreq() / preheader->getFreq()));
                double headerFreq = header->getFreq();
                CFGNode* backTarget = header;
                CFGEdge* backEdge = (CFGEdge*) tail->findTarget(header);
                double noEarlyExitProb = (tail->getFreq() * backEdge->getEdgeProb()) / header->getFreq();
                if(!(noEarlyExitProb > 0 && noEarlyExitProb <= 1)) {
					Log::cat_opt_loop()->fail << "headerFreq=" << headerFreq << ::std::endl;
                    Log::cat_opt_loop()->fail << "tailFreq=" << tail->getFreq() << ::std::endl;
                    Log::cat_opt_loop()->fail << "backProb=" << backEdge->getEdgeProb() << ::std::endl;
                    Log::cat_opt_loop()->fail << "noEarlyExit=" << noEarlyExitProb << ::std::endl;
                    assert(0);
                }

                uint32 k;
                double scale = 1;
                double sum = 1;
                for(k = 0; k < (n-1); ++k) {
                    // Probability of starting the k+1 iteration.
                    scale *= noEarlyExitProb;
                    sum += scale;
                }
                for(k = 0; k < (n-1); ++k) {
                    CFGNode* unrolled = fg.duplicateRegion(header, nodesToPeel, defUses, headerFreq * scale / sum);
                    scale /= noEarlyExitProb;
                    backEdge = (CFGEdge*) tail->findTarget(backTarget);
                    if(backEdge == NULL) {
                        //
                        // An intervening stVar block was added to promote a temp to a var 
                        // in the duplicated block.  The new tail should be the stVar block.
                        //
                        tail = (CFGNode*) tail->getUnconditionalEdge()->getTargetNode();
                        assert(tail != NULL);
                        CFGEdge* backEdge = (CFGEdge*) tail->findTarget(backTarget);
                        if( !(backEdge != NULL) ) assert(0);
                    }
                    fg.replaceEdgeTarget(backEdge, unrolled);
                    backTarget = unrolled;
                }
                assert(header->getInDegree() == 2);
                tail = header->getInEdges().front()->getSourceNode();
                if(preheader == tail)
                    tail = header->getInEdges().back()->getSourceNode();
            }
            if(preheader->getOutDegree() > 1) {
                CFGEdge* edge = (CFGEdge*) preheader->findTarget(header);
                assert(edge != NULL);
                preheader = fg.spliceBlockOnEdge(edge);
            }

        } else {            
            while(isInversionCandidate(header, header, nodesInLoop, next, exit)) {
                Log::cat_opt_loop()->debug << "Consider peeling node at L" << (int) header->getLabelId() << ::std::endl;
                assert(header->isBlockNode());
                assert(header->getInDegree() == 2);            
                assert(header->findSource(preheader) != NULL);
                assert(header->findSource(tail) != NULL);
                
                // Find variant instructions.  Invariant instructions need not be duplicated.
                StlVector<Inst*> variantInsts(tmm);
                StlHashSet<Opnd*> variantOpnds(tmm);
                StlVector<Inst*> invariantInsts(tmm);
                Inst* first = header->getFirstInst();
                Inst* inst;
                for(inst = first->next(); inst != first; inst = inst->next()) {
                    if (isVariantInst(inst, variantOpnds)) {
                        if (Log::cat_opt_loop()->isDebugEnabled()) {
                            Log::out() << "Inst ";
                            inst->print(Log::out());
                            Log::out() << " is variant" << ::std::endl;
                        }
                        variantInsts.push_back(inst);
                        Opnd* dst = inst->getDst();
                        variantOpnds.insert(dst);
                    } else {
                        if (Log::cat_opt_loop()->isDebugEnabled()) {
                            Log::out() << "Inst ";
                            inst->print(Log::out());
                            Log::out() << " is invariant" << ::std::endl;
                        }
                        invariantInsts.push_back(inst);
                    }
                }
                
                // Heuristic #0: If no invariant in header, don't peel.
                if (invariantInsts.empty()) {
                    Log::cat_opt_loop()->debug << "Peeling heuristic #0, no invariant" << ::std::endl;
                    break;
                }
                
                // Heuristic #1: If the last instruction is a variant check, stop peeling.
                Inst* last = header->getLastInst();
                if (last->getOperation().isCheck() && !variantInsts.empty() && last == variantInsts.back() &&
                   !(last->getDst()->isNull() 
                     || (last->getDst()->getType()->tag == Type::Tau))) {
                    hoistHeaderInvariants(preheader, header, invariantInsts);
                    Log::cat_opt_loop()->debug << "Peeling heuristic #1, last inst is variant check" << ::std::endl;
                    break;
                }
                // Heuristic #2: If a defined tmp may be live on exit, don't peel this node.
                // Promoting such tmps to vars is expensive, since we need to find all uses.
                StlVector<Inst*> globalInsts(tmm);
                Opnd* unhandledGlobal = NULL;
                bool hasExit = exit != NULL;
                bool exitIsUnwind = hasExit && (exit == fg.getUnwind());
                bool exitIsNotDominated = !hasExit || dom.hasDomInfo(header) && dom.hasDomInfo(exit)
                    && !dom.dominates(header, exit);
                StlVector<Inst*>::iterator iiter;
                for(iiter = variantInsts.begin(); iiter != variantInsts.end(); ++iiter) {
                    Inst* inst = *iiter;
                    Opnd* dst = inst->getDst();
                    if (dst->isGlobal() && !dst->isVarOpnd() && !dst->isNull()) {
                        // Defined global temp.
                        if(exitIsNotDominated || exitIsUnwind || (inst == last && exit->isDispatchNode())) {
                            globalInsts.push_back(inst);
                        } else {
                            unhandledGlobal = dst;
                            break;
                        }
                    }
                }
                if (unhandledGlobal != NULL) {
                    if (Log::cat_opt_loop()->isDebugEnabled()) {
                        Log::out() << "Stop peeling node at L" << (int) header->getLabelId() << ": unhandled global operand. ";
                        unhandledGlobal->print(Log::out());
                        Log::out() << ::std::endl;
                    }
                    hoistHeaderInvariants(preheader, header, invariantInsts);
                    break;
                }
                
                // Duplicate instructions
                CFGNode* peeled = fg.createBlockNode();
                Log::cat_opt_loop()->debug << "Peeling node L" << (int) header->getLabelId() << " as L" << (int) peeled->getLabelId() << ::std::endl;
                Inst* nextInst;
                // Peel instructions to peeled block.  Leave clones of variant insts.
                for(inst = first->next(), iiter = variantInsts.begin(); inst != first; inst = nextInst) {
                    nextInst = inst->next();
                    if (iiter != variantInsts.end() && inst == *iiter) {
                        // Make clone in place.
                        ++iiter;
                        Inst* clone = instFactory.clone(inst, opndManager, duplicateTable);
                        clone->insertBefore(nextInst);
                        inst->unlink();
                        if (Log::cat_opt_loop()->isDebugEnabled()) {
                            Log::out() << "Peeling: copying ";
                            inst->print(Log::out());
                            Log::out() << " to ";
                            clone->print(Log::out());
                            Log::out() << ::std::endl;
                        }
                    } else {
                        if(inst->getOperation().canThrow())
                            fg.eliminateCheck(header, inst, false);
                        else
                            inst->unlink();
                        if (Log::cat_opt_loop()->isDebugEnabled()) {
                            Log::out() << "Peeling: not copying ";
                            inst->print(Log::out());
                            Log::out() << ::std::endl;
                        }
                    }
                    peeled->append(inst);
                }
                assert(iiter == variantInsts.end());
                
                fg.replaceEdgeTarget(preheader->findTarget(header), peeled);
                if (hasExit)
                    fg.addEdge(peeled, exit);
                fg.addEdge(peeled, next);
                if (!globalInsts.empty()) {
                    OpndRenameTable* patchTable = new (tmm) OpndRenameTable(tmm);
                    
                    // Need to patch global defs.  Create store blocks and merge at next
                    Log::cat_opt_loop()->debug << "Merging global temps in node L" << (int) header->getLabelId() << " and L" << (int) peeled->getLabelId() << ::std::endl;
                    CFGNode* newpreheaderSt = fg.createBlockNode();
                    CFGNode* newtailSt = fg.createBlockNode();
                    fg.replaceEdgeTarget(peeled->findTarget(next), newpreheaderSt);
                    fg.addEdge(newpreheaderSt, next);
                    fg.replaceEdgeTarget(header->findTarget(next), newtailSt);
                    fg.addEdge(newtailSt, next);
                    
                    StlVector<Inst*>::iterator i;
                    for(i = globalInsts.begin(); i != globalInsts.end(); ++i) {
                        Inst* inst = *i;
                        Opnd* dst = inst->getDst();
                        Opnd* dstPreheader = opndManager.createSsaTmpOpnd(dst->getType());
                        Opnd* dstTail = duplicateTable->getMapping(dst);
                        inst->setDst(dstPreheader);
                        VarOpnd* dstVar;
                        
                        if ((inst->getOpcode() == Op_LdVar) && (variantOpnds.find(inst->getSrc(0)) == variantOpnds.end())) {
                            // If dst is generated from a LdVar, reuse that var instead of promoting to a new var.
                            dstVar = inst->getSrc(0)->asVarOpnd();
                            assert(dstVar != NULL);
                        } else {
                            // Create a new var and initialize it the original and duplicated blocks.
                            dstVar = opndManager.createVarOpnd(dst->getType(), false);
                            Inst* stPreheader = instFactory.makeStVar(dstVar, dstPreheader);
                            newpreheaderSt->append(stPreheader);
                            Inst* stTail = instFactory.makeStVar(dstVar, dstTail);
                            newtailSt->append(stTail);
                        }
                        
                        Inst* ldVar = instFactory.makeLdVar(dst, dstVar);
                        next->prepend(ldVar);
                        
                        patchTable->setMapping(dst, dstPreheader);
                    }
                    fg.renameOperandsInNode(peeled, patchTable);
                    
                    // Rotate
                    preheader = newpreheaderSt;
                    tail = newtailSt;
                } else {
                    if (peeled->getOutDegree() > 1) {
                        // Remove critical edge
                        preheader = fg.createBlockNode();
                        fg.replaceEdgeTarget(peeled->findTarget(next), preheader);
                        fg.addEdge(preheader, next);
                    } else {
                        preheader = peeled;
                    }
                    tail = header;
                }
                
                // Prepare to peel next node.
                header = next;
                if (flags.aggressive_peeling) {
                    if (header == originalInvertedHeader) {
                        Log::cat_opt_loop()->debug << "Stop peeling node at L" << (int) header->getLabelId() << ": peeled one iteration" << ::std::endl;
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        assert(header->getInDegree() == 2);
        backEdge = (CFGEdge*) header->findSource(tail);
        assert(backEdge != NULL);
        *i = backEdge;
    }
    loopEdgesIn.clear();

    if(flags.insideout_peeling) {
        StlVector<CFGEdge*>::reverse_iterator i = loopEdges.rbegin();
        while (i != loopEdges.rend()) {
            loopEdgesIn.insert(loopEdgesIn.end(), *i);
            i++;
        }
    } else {
        StlVector<CFGEdge*>::iterator i = loopEdges.begin();
        while (i != loopEdges.end()) {
            loopEdgesIn.insert(loopEdgesIn.end(), *i);
            i++;
        }
    }
}

LoopTree* LoopBuilder::computeAndNormalizeLoops(bool doPeelLoops) {
    // find all loop headers
    MemoryManager tmm(16*sizeof(CFGNode*)*2,"LoopBuilder::computeAndNormalizeLoops.tmm");
    StlVector<CFGNode*> headers(tmm);
    findLoopHeaders(headers);
    assert(dom.isValid());

    StlVector<CFGEdge*> loopEdges(tmm);
    uint32 numLoops = findLoopEdges(tmm, headers, loopEdges);
    //
    // build loop hierarchy
    //
    // sort loop edges based on their depth first number
    ::std::sort(loopEdges.begin(), loopEdges.end(), CompareDFN());

    if (doPeelLoops) {
        peelLoops(loopEdges);
        if (Log::cat_opt_loop()->isDebugEnabled()) 
            fg.printInsts(Log::out(), irManager.getMethodDesc());
    }

    if (!fg.hasValidOrdering())
        fg.orderNodes();
    formLoopHierarchy(loopEdges, numLoops);
    info = new (loopMemManager) LoopTree(loopMemManager, &fg);
    info->process((LoopNode*) root);
    return info;
}


} //namespace Jitrino 
