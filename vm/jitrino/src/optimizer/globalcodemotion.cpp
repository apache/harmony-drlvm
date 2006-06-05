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
 * @version $Revision: 1.22.16.4 $
 *
 */

#include <assert.h>
#include <iostream>
#include <algorithm>
#include "Stl.h"
#include "Log.h"
#include "PropertyTable.h"
#include "open/types.h"
#include "Inst.h"
#include "irmanager.h"
#include "FlowGraph.h"
#include "Dominator.h"
#include "Loop.h"
#include "globalcodemotion.h"
#include "CSEHash.h"
#include "Opcode.h"
#include "constantfolder.h"
#include "walkers.h"
#include "optimizer.h"
#include "hashvaluenumberer.h"
#include "aliasanalyzer.h"
#include "memoryopt.h"

namespace Jitrino {

DEFINE_OPTPASS_IMPL(GlobalCodeMotionPass, gcm, "Global Code Motion")

void
GlobalCodeMotionPass::_run(IRManager& irm) {
    splitCriticalEdges(irm);
    computeDominatorsAndLoops(irm);
    MemoryManager& memoryManager = irm.getNestedMemoryManager();
    DominatorTree* dominatorTree = irm.getDominatorTree();
    LoopTree* loopTree = irm.getLoopTree();
    assert(dominatorTree->isValid() && loopTree->isValid());
    GlobalCodeMotion gcm(irm, memoryManager, *dominatorTree, loopTree);
    gcm.runPass();
}

DEFINE_OPTPASS_IMPL(GlobalValueNumberingPass, gvn, "Global Value Numbering")

void
GlobalValueNumberingPass::_run(IRManager& irm) {
    computeDominatorsAndLoops(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    assert(dominatorTree && dominatorTree->isValid());
    LoopTree* loopTree = irm.getLoopTree();
    assert(loopTree && loopTree->isValid());
    MemoryManager& memoryManager = irm.getNestedMemoryManager();
    FlowGraph& flowGraph = irm.getFlowGraph();

    DomFrontier frontier(memoryManager,*dominatorTree,&flowGraph);
    TypeAliasAnalyzer aliasAnalyzer;
    MemoryOpt mopt(irm, memoryManager, *dominatorTree,
        frontier, loopTree, &aliasAnalyzer);
    mopt.runPass();
    HashValueNumberer valueNumberer(irm, *dominatorTree);
    valueNumberer.doGlobalValueNumbering(&mopt);
}


GlobalCodeMotion::Flags *GlobalCodeMotion::defaultFlags = 0;

GlobalCodeMotion::GlobalCodeMotion(IRManager &irManager0, 
                                   MemoryManager& memManager,
                                   DominatorTree& dom0,
                                   LoopTree *loopTree0)
    : irManager(irManager0), 
      fg(irManager0.getFlowGraph()),
      methodDesc(irManager0.getMethodDesc()),
      mm(memManager),
      dominators(dom0),
      loopTree(loopTree0),
      flags(*defaultFlags),
      earliest(memManager),
      latest(memManager),
      visited(memManager),
      uses(memManager)
{
    assert(defaultFlags);
}

GlobalCodeMotion::~GlobalCodeMotion()
{
}

void 
GlobalCodeMotion::readDefaultFlagsFromCommandLine(const JitrinoParameterTable *params)
{
    if (!defaultFlags)
        defaultFlags = new Flags;
    defaultFlags->dry_run = params->lookupBool("opt::gcm::dry_run", false);
    defaultFlags->sink_stvars = params->lookupBool("opt::gcm::sink_stvars", false);
    defaultFlags->min_cut = params->lookupBool("opt::gcm::min_cut", false);
    defaultFlags->sink_constants = params->lookupBool("opt::gcm::sink_constants", true);
}

void GlobalCodeMotion::showFlagsFromCommandLine()
{
    Log::out() << "    opt::gcm::dry_run[={on|OFF}] = don't really move anything" << ::std::endl;
    Log::out() << "    opt::gcm::sink_stvars[={on|OFF}] = raise ldvar, sink stvar to Phi nodes" 
               << ::std::endl;
    Log::out() << "    opt::gcm::min_cut[={on|OFF}] = duplicate placement on mincut between def and uses" << ::std::endl;
    Log::out() << "    opt::gcm::sink_constants[={ON|off}] = sink constants after placement" << ::std::endl;
}

void GlobalCodeMotion::runPass()
{
    if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gcm()->isIR2Enabled()) {
        Log::out() << "IR before GCM pass" << ::std::endl;
        fg.printInsts(Log::out(), methodDesc);
        fg.printDotFile(methodDesc, "beforegcm", &dominators);
        dominators.printDotFile(methodDesc, "beforegcm.dom");
        loopTree->printDotFile(methodDesc, "beforegcm.loop");
    }

    // schedule instructions early
    scheduleAllEarly();
    // now earliest[i] = domNode for earliest placement.

    clearVisited();

    scheduleAllLate();

    // sink loads of constants, since IPF back-end doesn't re-materialize
    if (flags.sink_constants)
        sinkAllConstants();
    // patch up SSA form in case live ranges overlap now
    // for now, we don't move Phis, Ldvars, or Stvars, so Ssa form is unaffected
    // fixSsaForm();

    if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
        Log::out() << "IR after GCM pass" << ::std::endl;
        fg.printInsts(Log::out(), methodDesc);
        fg.printDotFile(methodDesc, "aftergcm", &dominators);
        dominators.printDotFile(methodDesc, "aftergcm.dom");
    }
}

// EARLY SCHEDULING

// a DomNodeInstWalker, forwards/preorder
class GcmScheduleEarlyWalker {
    GlobalCodeMotion *thePass;
    CFGNode *block;
    DominatorNode *domNode;
public:
    void startNode(DominatorNode *domNode0) { 
        domNode = domNode0;
        block = domNode->getNode();
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Begin early scheduling of block ";
            block->print(Log::out());
            Log::out() << ::std::endl;
        }    
    };
    void applyToInst(Inst *i) { thePass->scheduleEarly(domNode, i);  };
    void finishNode(DominatorNode *domNode) { 
        CFGNode *block1 = domNode->getNode();
        if( !(block == block1) ) assert(0);
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Done early scheduling of block ";
            block->print(Log::out());
            Log::out() << ::std::endl;
        }    
    };
    
    GcmScheduleEarlyWalker(GlobalCodeMotion *thePass0)
        : thePass(thePass0), block(0), domNode(0)
    {
    };
};

void GlobalCodeMotion::scheduleAllEarly()
{
    GcmScheduleEarlyWalker scheduleEarlyWalker(this);

    // adapt the DomNodeInstWalker to a forwards DomWalker
    typedef DomNodeInst2DomWalker<true, GcmScheduleEarlyWalker> EarlyDomInstWalker;
    EarlyDomInstWalker domInstWalker(scheduleEarlyWalker);
    
    // do the walk, pre-order
    DomTreeWalk<true, EarlyDomInstWalker>(dominators, domInstWalker, mm);
}

// since it will usually visit in the right order, we used dominator tree order
// BUT: since GVN may leave instructions out of place, we can't count on
// operands having been visited before the instruction.
// THOUGH: we guarantee the head of each cycle (when placed legally) will
// be a Phi function, and thus pinned.  This means that for non-Phi instrs,
// we can just place all operands first, then this instruction.
// For Phi instructions, we must just leave them in place (pinned) and leave
// the operands for later visiting.

// domNode should be 0 or i's DominatorNode, if we happen to know it.
void GlobalCodeMotion::scheduleEarly(DominatorNode *domNode, Inst *i)
{
    if (i->isLabel()) return;
    if (alreadyVisited(i)) {
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Inst is already scheduled early: ";
            i->print(Log::out());
            Log::out() << ::std::endl;
        }
        return;
    } else {
        markAsVisited(i);

        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Beginning early scheduling of inst ";
            i->print(Log::out());
            Log::out() << ::std::endl;
        }

        if (isPinned(i)) {
            // leave it here
            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "Inst is pinned" << ::std::endl;
            }

            // We place operands before placing instruction, to get ordering right
            // in the block; in normal code, operands should appear first and 
            // have been already placed, but after GVN we don't have that guarantee.
            if (i->getOpcode() != Op_Phi) { // phi operands must also be pinned, skip them.
                if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                    Log::out() << "Inst is not a Phi, processing operands" << ::std::endl;
                }
                // for others, record uses in this instruction
                uint32 numSrcs = i->getNumSrcOperands();
                for (uint32 srcNum=0; srcNum < numSrcs; ++srcNum) {
                    Opnd *srcOpnd = i->getSrc(srcNum);
                    Inst *srcInst = srcOpnd->getInst();
                    scheduleEarly(0, srcInst);
                    if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                        Log::out() << "Recording that ";
                        srcInst->print(Log::out());
                        Log::out() << " is used by ";
                        i->print(Log::out());
                        Log::out() << ::std::endl;
                    }
                    uses[srcInst].insert(i);
                }
            }

            // now place instruction
            if (domNode) {
                earliest[i] = domNode;
            } else {
                CFGNode *cfgNode = i->getNode();
                DominatorNode *domNode1 = dominators.getDominatorNode(cfgNode);
                earliest[i] = domNode1;
            }

            if (i->getOperation().canThrow()) {
                // as a special case, mark it as being in the normal successor node;
                // this will cause dependent instructions to be placed correctly.
                CFGNode *peiNode = earliest[i]->getNode();

                const CFGEdgeDeque &outedges = peiNode->getOutEdges();
                typedef CFGEdgeDeque::const_iterator EdgeIter;
                EdgeIter eLast = outedges.end();
                DominatorNode *newDom = 0;
                for (EdgeIter eIter = outedges.begin(); eIter != eLast; eIter++) {
                    CFGEdge *outEdge = *eIter;
                    CFGEdge::Kind kind = outEdge->getEdgeType();
                    if (kind != CFGEdge::Exception) {
                        CFGNode *succBlock = outEdge->getTargetNode();
                        newDom = dominators.getDominatorNode(succBlock);
                        break;
                    }
                }        
                if (newDom) {
                    earliest[i] = newDom;
                }
            }
            
        } else {
            // figure out earliest placement
            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "Inst is not pinned:";
                i->print(Log::out()); 
                Log::out()<< ::std::endl;
            }
            DominatorNode *currentEarliest = dominators.getDominatorRoot();
            uint32 numSrcs = i->getNumSrcOperands();
            for (uint32 srcNum=0; srcNum < numSrcs; ++srcNum) {
                Opnd *srcOpnd = i->getSrc(srcNum);
                Inst *srcInst = srcOpnd->getInst();
                if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                    Log::out() << "Inst ";
                    i->print(Log::out());
                    Log::out() << " uses ";
                    srcInst->print(Log::out());
                    Log::out()<< ::std::endl;
                }
                scheduleEarly(0, srcInst);
                DominatorNode *srcInstEarliest = earliest[srcInst];
                if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                    Log::out() << "Inst ";
                    i->print(Log::out());
                    Log::out() << " uses ";
                    srcInst->print(Log::out());
                    Log::out() << ", which had early placement in ";
                    srcInstEarliest->print(Log::out());
                    Log::out() << ::std::endl;
                }
                // Note that both currentEarliest and srcInstEarliest dominate this instruction.
                // Thus one of them must dominate the other.  The dominated one is the earliest place
                // we can put this.
                if (dominators.isAncestor(currentEarliest, srcInstEarliest)) {
                    // must move currentEarliest down to have srcOpnd available
                    currentEarliest = srcInstEarliest;
                }
                if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                    Log::out() << "Recording that ";
                    srcInst->print(Log::out());
                    Log::out() << " is used by ";
                    i->print(Log::out());
                    Log::out() << ::std::endl;
                }
                uses[srcInst].insert(i); // record the use while we're iterating.
            }
            earliest[i] = currentEarliest;
        }

        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Done earliest placement of ";
            i->print(Log::out());
            Log::out() << " is in block ";
            earliest[i]->print(Log::out());            
            Log::out() << ::std::endl;
        }
        
    }
}

// LATE SCHEDULING

// a domNodeInstWalker, backwards/postorder
class GcmScheduleLateWalker {
    GlobalCodeMotion *thePass;
    CFGNode *block;
    DominatorNode *domNode;
public:
    void startNode(DominatorNode *domNode0) { 
        domNode = domNode0;
        block = domNode->getNode();
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Begin late scheduling of block ";
            block->print(Log::out());
            Log::out() << ::std::endl;
        }    
    };
    void applyToInst(Inst *i) { thePass->scheduleLate(domNode, 0, i); };
    void finishNode(DominatorNode *domNode) { 
        CFGNode *block1 = domNode->getNode();
        if( !(block == block1) ) assert(0);;
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Done late scheduling of block ";
            block->print(Log::out());
            Log::out() << ::std::endl;
        }    
    };
    GcmScheduleLateWalker(GlobalCodeMotion *thePass0)
        : thePass(thePass0), block(0), domNode(0)
    {
    };
};

void GlobalCodeMotion::scheduleAllLate()
{
    GcmScheduleLateWalker scheduleLateWalker(this);

    // adapt the DomNodeInstWalker to a forwards DomWalker
    typedef DomNodeInst2DomWalker<false, GcmScheduleLateWalker> LateDomInstWalker;
    LateDomInstWalker domInstWalker(scheduleLateWalker);

    DomTreeWalk<false, LateDomInstWalker>(dominators, domInstWalker, mm);
}

// Note that we are visiting instructions in a block in reverse order.
// It is safe to modify anything below the current instruction.
// as long as no using* instruction precedes this instruction in the same block, we
// are safe in recursively placing use.

// basei, if non-0, is the instruction where we are current iterating, and domNode is
// its node.  If i is where we are iterating, then domNode is its node and basei is 0.
void GlobalCodeMotion::scheduleLate(DominatorNode *domNode, Inst *basei, Inst *i)
{
    if (i->isLabel()) return;
    if (alreadyVisited(i)) {
        return;
    } else {
        markAsVisited(i);

        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Beginning late scheduling of inst ";
            i->print(Log::out());
            Log::out() << ::std::endl;
        }

        bool pinned = isPinned(i);
        // figure out where to put it, first scheduling users along the way

        // schedule users
        DominatorNode *lca = 0;
        UsesSet &users = uses[i];
        UsesSet::iterator 
            uiter = users.begin(),
            uend = users.end();
        for ( ; uiter != uend; ++uiter) {
            Inst *useri = *uiter;
            
            // schedule use
            scheduleLate(domNode, basei, useri);

            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "Inst ";
                i->print(Log::out());
                Log::out() << " is used by inst ";
                useri->print(Log::out());
                Log::out() << " which is in ";
                latest[useri]->print(Log::out());
                Log::out() << ::std::endl;
            }
            
            DominatorNode *usingBlock = latest[useri];
            lca = pinned ? 0 : leastCommonAncestor(lca, usingBlock);
            while (lca && (lca->getNode()->isDispatchNode())) {
                lca = lca->getParent(); // code can't be placed in dispatch nodes
            }
        }
        // if pinned, and following instruction is not already scheduled,
        // then we are scheduling this pinned instruction as a use of some
        // other instruction.  before scheduling it, we must schedule any
        // following pinned instruction.
        if (pinned) {
            Inst *nextInst = i->next();
            while (!nextInst->isLabel() && !alreadyVisited(nextInst)) {
                if (isPinned(nextInst)) {
                    if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                        Log::out() << "Pinned Inst ";
                        i->print(Log::out());
                        Log::out() << " is followed by unscheduled pinned inst ";
                        nextInst->print(Log::out());
                        Log::out() << ::std::endl;
                    }
                    scheduleLate(domNode, basei, nextInst);
                    // scheduling nextInst should result in scheduling
                    // any following pinned inst, so break out of loop.
                    break; 
                } else {
                    if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                        Log::out() << "Pinned Inst ";
                        i->print(Log::out());
                        Log::out() << " is followed by unscheduled unpinned inst ";
                        nextInst->print(Log::out());
                        Log::out() << ::std::endl;
                    }
                    nextInst = nextInst->next();
                }
            }
        }
        
        // now, unless pinned, LCA is leastCommonAncestor of all uses
        // if LCA==0, then there are no uses; i is dead.
        
        if (pinned) {
            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "Inst is pinned, left in place: ";
                i->print(Log::out());
                Log::out() << ::std::endl;
            }
            latest[i] = dominators.getDominatorNode(i->getNode()); // leave it here.
        } else if (lca == 0) {
            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "Inst has no uses, left in place: ";
                i->print(Log::out());
                Log::out() << ::std::endl;
            }
            latest[i] = dominators.getDominatorNode(i->getNode()); // leave it here.
        } else {
            DominatorNode *best = lca;
            DominatorNode *earliestBlock = earliest[i];
            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "LCA is ";
                lca->print(Log::out());
                Log::out() << ", best is ";
                best->print(Log::out());
                Log::out() << " earliestBlock is ";
                earliestBlock->print(Log::out());
                Log::out() << ::std::endl;
            }

            while (lca != earliestBlock) {
                assert(lca);
                lca = lca->getParent(); // go up dom tree
                assert(lca && lca->getNode());
                while (lca->getNode()->isDispatchNode()) {
                    lca = lca->getParent(); // code can't be placed in dispatch nodes
                    assert(lca && lca->getNode());
                }
                if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                    Log::out() << "LCA is ";
                    lca->print(Log::out());
                    Log::out() << " with depth "
                               << (int) loopTree->getLoopDepth(lca->getNode())
                               << ", best is ";
                    best->print(Log::out());
                    Log::out() << " with depth "
                               << (int) loopTree->getLoopDepth(best->getNode());
                    Log::out() << ::std::endl;
                }
                
                if (loopTree->getLoopDepth(lca->getNode()) < loopTree->getLoopDepth(best->getNode())) {
                    // find deepest block at shallowest nest
                    best = lca;
                    
                    if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                        Log::out() << "LCA replaces best" << ::std::endl;
                    }
                }
            };

            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "Finally, best is ";
                best->print(Log::out());
                Log::out() << ::std::endl;
            }
            latest[i] = best;
        }

        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Placing inst ";
            i->print(Log::out());
            Log::out() << " in block ";
            latest[i]->print(Log::out());            
            Log::out() << ::std::endl;
        }

        // now, to actually move the instruction:

        // we always move instructions at the start of the block.
        // this looks confusing if we are inserting into the same block
        // we are iterating over: we may see the instruction again, but
        // this is safe since the instruction is already marked as placed.

        // we do this even for pinned instructions; they will be visited in
        // reverse order, so they will be replaced in the block starting at
        // the top, so we are ok.

        CFGNode *node = latest[i]->getNode();
        Inst* headInst = node->getFirstInst();
        assert(headInst->isLabel());
        
        i->unlink();
        if (i->getOperation().mustEndBlock()) {
            // but an instruction which must be at the end must stay there.
            // this includes branches.
            if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "  Placing inst before headinst ";
                headInst->print(Log::out());
                Log::out() << ::std::endl;
            }
            i->insertBefore(headInst);
        } else {
			if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
                Log::out() << "  Placing inst after headinst ";
                headInst->print(Log::out());
                Log::out() << ::std::endl;
            }
            i->insertAfter(headInst);
        }
    }
}

DominatorNode *GlobalCodeMotion::leastCommonAncestor(DominatorNode *a, DominatorNode *b)
{
    if (a == 0) return b;
    assert(b != 0);
    while (a != b) {
        if (dominators.isAncestor(a, b)) {
            b = a;
        } else if (dominators.isAncestor(b, a)) {
            a = b;
        } else {
            a = a->getParent();
            b = b->getParent();
        }
    }
    return a;
}

// a NodeInstWalker, forwards/preorder
class GcmSinkConstantsWalker {
    GlobalCodeMotion *thePass;
    CFGNode *block;
public:
    void startNode(CFGNode *cfgNode0) { 
        block = cfgNode0;
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Begin sinking constants in block";
            block->print(Log::out());
            Log::out() << ::std::endl;
        }    
    };
    void applyToInst(Inst *i) { thePass->sinkConstants(i); };
    void finishNode(CFGNode *cfgNode0) { 
        if (Log::cat_opt_gcm()->isDebugEnabled() || Log::cat_opt_gc()->isIR2Enabled()) {
            Log::out() << "Done sinking constants in block";
            block->print(Log::out());
            Log::out() << ::std::endl;
        }    
    };
    
    GcmSinkConstantsWalker(GlobalCodeMotion *thePass0)
        : thePass(thePass0), block(0)
    {
    };
};


void GlobalCodeMotion::sinkAllConstants()
{
    GcmSinkConstantsWalker gcmSinkConstantsWalker(this);

    // adapt the NodeInstWalker to a NodeWalker

    typedef NodeInst2NodeWalker<true, GcmSinkConstantsWalker> 
        SinkConstantsNodeWalker;
    SinkConstantsNodeWalker sinkConstantsNodeWalker(gcmSinkConstantsWalker);
    
    // do the walk, postorder
    NodeWalk<SinkConstantsNodeWalker>(fg, sinkConstantsNodeWalker);
}

void GlobalCodeMotion::sinkConstants(Inst *inst)
{
    for (uint32 i = 0; i < inst->getNumSrcOperands(); i++) {
        Opnd *opnd = inst->getSrc(i);
        Inst *opndInst = opnd->getInst();
        if (opndInst->isConst()) {
            ConstInst* cinst = opndInst->asConstInst();
            ConstInst::ConstValue cv = cinst->getValue();
            InstFactory &ifactory = irManager.getInstFactory();
            OpndManager &opndManager = irManager.getOpndManager();
            Opnd *newOp = opndManager.createSsaTmpOpnd(opnd->getType());
            Inst *newInst = ifactory.makeLdConst(newOp, cv);
            newInst->insertBefore(inst);
        }
    }
}

// MISC STUFF

bool GlobalCodeMotion::isPinned(Inst *i)
{
    Opcode opcode = i->getOpcode();
    // be explicit about SSA ops
    if ((opcode == Op_LdVar) || (opcode == Op_StVar) || (opcode == Op_Phi))
        return true;
    if (i->getOperation().isMovable()) {
        return false;
    }
    OptimizerFlags& optimizerFlags = *irManager.getCompilationContext()->getOptimizerFlags(); 
    if (0 && optimizerFlags.cse_final) {
        switch (i->getOpcode()) {
        case Op_TauLdInd: 
            {
                Inst *srcInst = i->getSrc(0)->getInst();
                if ((srcInst->getOpcode() == Op_LdFieldAddr) ||
                    (srcInst->getOpcode() == Op_LdStaticAddr)) {
                    FieldAccessInst *faInst = srcInst->asFieldAccessInst();
                    FieldDesc *fd = faInst->getFieldDesc();
                    if (fd->isInitOnly()) {
                        return false;
                    }
                }
            } 
            break;
        case Op_LdStatic:
            {
                FieldAccessInst *inst = i->asFieldAccessInst();
                assert(inst);
                FieldDesc* fd = inst->getFieldDesc();
                if (fd->isInitOnly())
                    return false;
            }
            break;
        case Op_TauLdField:
            {
                FieldAccessInst *inst = i->asFieldAccessInst();
                assert(inst);
                FieldDesc* fd = inst->getFieldDesc();
                if (fd->isInitOnly())
                    return false;
            }
            break;
        default:
            break;
        };
    }
    return true;
}

void GlobalCodeMotion::markAsVisited(Inst *i)
{
    visited.insert(i);
}

bool GlobalCodeMotion::alreadyVisited(Inst *i)
{
    VisitedSet::const_iterator it = visited.find(i);
    return (it != visited.end());
}

void GlobalCodeMotion::clearVisited()
{
    visited.clear();
}


} //namespace Jitrino 
