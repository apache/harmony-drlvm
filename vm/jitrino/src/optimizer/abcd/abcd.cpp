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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.20.24.4 $
 *
 */

#include "irmanager.h"
#include "Dominator.h"
#include "constantfolder.h"
#include "abcd.h"
#include "abcdbounds.h"
#include "abcdsolver.h"
#include "opndmap.h"

#include "Stl.h"
#include "Log.h"
#include "open/types.h"
#include "Inst.h"
#include "walkers.h"
#include "PMFAction.h"

#include <assert.h>
#include <iostream>
#include <algorithm>

namespace Jitrino {

//Array Bounds Check Elimination
class ABCDPass: public SessionAction {
public:
    void run();    
};

ActionFactory<ABCDPass> _abcd("abcd");

void
ABCDPass::run() {
    IRManager& irm = *getCompilationContext()->getHIRManager();
    OptPass::splitCriticalEdges(irm);
    OptPass::computeDominators(irm);
    Abcd abcd(irm, irm.getNestedMemoryManager(), *irm.getDominatorTree());
    abcd.runPass();
}

void 
Abcd::readFlags(Action* argSource, AbcdFlags* flags ) {
    IAction::HPipeline p = NULL; //default pipeline for argSource
    flags->partial = argSource->getBoolArg(p, "abcd.partial", false);
    flags->dryRun = argSource->getBoolArg(p, "abcd.dry_run", false);
    flags->useAliases = argSource->getBoolArg(p, "abcd.use_aliases", true);
    flags->useConv = argSource->getBoolArg(p, "abcd.use_conv", true);
    flags->remConv = argSource->getBoolArg(p, "abcd.rem_conv", false);
    flags->useShr = argSource->getBoolArg(p, "abcd.use_shr", true);
    flags->unmaskShifts = argSource->getBoolArg(p, "abcd.unmask_shifts", true);
    flags->remBr = argSource->getBoolArg(p, "abcd.rem_br", true);
    flags->remCmp = argSource->getBoolArg(p, "abcd.rem_cmp", true);
    flags->remOneBound = argSource->getBoolArg(p, "abcd.rem_one_bound", true);
    flags->remOverflow = argSource->getBoolArg(p, "abcd.rem_overflow", false);
    flags->checkOverflow = argSource->getBoolArg(p, "abcd.check_overflow", false);
    flags->useReasons = argSource->getBoolArg(p, "abcd.use_reasons", false);
}

void Abcd::showFlags(std::ostream& os) {
    os << "  abcd flags:"<<std::endl;
    os << "    abcd.partial[={on|OFF}]        - try to eliminate partial ABCs" << std::endl;
    os << "    abcd.dry_run[={ON|off}]        - don't really eliminate checks" << std::endl;
    os << "    abcd.use_aliases[={ON|off}]    - try to use ld/stvar induced aliases" << std::endl;
    os << "    abcd.use_conv[={ON|off}]       - make use of conv info" << std::endl;
    os << "    abcd.rem_conv[={ON|off}]       - remove conv ops in abcd" << std::endl;
    os << "    abcd.use_shr={ON|off}]         - use shr info" << std::endl;
    os << "    abcd.unmask_shifts={ON|off}]   - try to unmask shifts" << std::endl;
    os << "    abcd.rem_br[={ON|off}]         - try to remove branches in abcd" << std::endl;
    os << "    abcd.rem_cmp[={ON|off}]        - try to fold cmps in abcd" << std::endl;
    os << "    abcd.rem_one_bound[={ON|off}]  - eliminate even just upper or lower bound check" << std::endl;
    os << "    abcd.rem_overflow[={on|OFF}]   - try to mark overflow impossible in 32-bit int add/sub";
    os << "    abcd.check_overflow[={on|OFF}] - try to mark overflow impossible in checkbounds";
    os << "    abcd.use_reasons[={ON|off}]    - build more precise taus for eliminated instructions";
}

Abcd::Abcd(IRManager &irManager0,  MemoryManager& memManager, DominatorTree& dom0)
: irManager(irManager0), 
mm(memManager),
ineqGraph(0),
dominators(dom0),
piMap(0),
nextPiOpndId(0),
solver(0),
canEliminate(memManager),
canEliminateUB(memManager),
canEliminateLB(memManager),
tauUnsafe(0),
tauSafe(0),
blockTauPoint(0),
lastTauPointBlock(0),
blockTauEdge(0),
lastTauEdgeBlock(0),
flags(*irManager.getOptimizerFlags().abcdFlags)
{
};

void Abcd::runPass()
{
    if (Log::isEnabled() || Log::isEnabled()) {
        Log::out() << "IR before ABCD pass" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
        FlowGraph::printDotFile(irManager.getFlowGraph(), irManager.getMethodDesc(), "beforeabcd");
        dominators.printDotFile(irManager.getMethodDesc(), "beforeabcd.dom");
    }

    insertPiNodes(); // add a pi node after each test on a variable
    renamePiVariables(); // rename all uses to use the inserted Pi variables

    // WARNING: Pi var live ranges may overlap the original 
    // var live ranges here

    if (Log::isEnabled()) {
        Log::out() << "IR after Pi insertion" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
        FlowGraph::printDotFile(irManager.getFlowGraph(), irManager.getMethodDesc(), "withpi");
    }

    removeRedundantBoundsChecks();
    if (Log::isEnabled() || Log::isEnabled()) {
        Log::out() << "IR after removeRedundantBoundsChecks" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
    }

    removePiNodes();
    if (Log::isEnabled()) {
        Log::out() << "IR after ABCD pass" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
    }
}

// a DomWalker, to be applied pre-order
class InsertPiWalker {
    Abcd *thePass;
public:
    void applyToDominatorNode(DominatorNode *domNode) { thePass->insertPiNodes(domNode->getNode()); };
    void enterScope() {}; // is called before a node and its children are processed
    void exitScope() {}; // is called after node and children are processed
    InsertPiWalker(Abcd *thePass0) : thePass(thePass0) {};
};


// Inserts Pi nodes.
// WARNING: Pi var live ranges may overlap the original var live ranges
// since we don't bother to add Phi nodes and rename subsequent uses of var.
void Abcd::insertPiNodes()
{
    // Add a Pi node on each branch after 
    // a test which tells something about a variable.
    // For now, don't bother with Exception edges.

    InsertPiWalker insertPiWalker(this);
    DomTreeWalk<true, InsertPiWalker>(dominators, insertPiWalker, mm); // pre-order
}

PiOpnd *Abcd::getNewDestOpnd(Node *block,
                             Opnd *org)
{
    PiOpnd *tmpOp = irManager.getOpndManager().createPiOpnd(org);
    return tmpOp;
}

// a DomWalker, to be applied forwards/preorder
class RenamePiWalker {
    Abcd *thePass;
    MemoryManager &localMemManager;
    SparseOpndMap* &piMap;
    int sizeEstimate;
public:
    void applyToDominatorNode(DominatorNode *domNode) { thePass->renamePiVariables(domNode->getNode()); };

    void enterScope() { 
        if (!piMap) piMap = new (localMemManager) SparseOpndMap(sizeEstimate,
                                                                localMemManager, 1, 4, 7);
        piMap->enter_scope(); };
    void exitScope() { piMap->exit_scope(); };
    RenamePiWalker(Abcd *thePass0,
                   MemoryManager &localMM,
                   SparseOpndMap* &piMap0,
                   int sizeEstimate0) 
        : thePass(thePass0), localMemManager(localMM), piMap(piMap0),
          sizeEstimate(sizeEstimate0)
    {
    };
};

// Renames variables for which we have Pi nodes.
void Abcd::renamePiVariables()
{
    MethodDesc &methodDesc= irManager.getMethodDesc();
    uint32 byteCodeSize = methodDesc.getByteCodeSize();
    MemoryManager localMemManager("Abcd::renamePiNodes");

    RenamePiWalker theWalker(this, localMemManager, piMap, byteCodeSize);
    DomTreeWalk<true, RenamePiWalker>(dominators, theWalker,
                                      localMemManager);
}


void Abcd::insertPiNodeForOpnd(Node *block,
                               Opnd *org,
                               const PiCondition &cond,
                               Opnd *tauOpnd)
{
    if (ConstantFolder::isConstant(org)) {
        if (Log::isEnabled()) {
            Log::out() << "Skipping Pi Node for opnd ";
            org->print(Log::out());
            Log::out() << " under condition ";
            cond.print(Log::out());
            Log::out() << " since it is constant" << std::endl;
        }
    } else {
        
        PiOpnd *piOpnd = irManager.getOpndManager().createPiOpnd(org);
        Inst *headInst = (Inst*)block->getFirstInst();
        PiCondition *condPtr = new (irManager.getMemoryManager()) PiCondition(cond);
        if (tauOpnd == 0)
            tauOpnd = getBlockTauEdge(block);
        Inst *newInst = irManager.getInstFactory().makeTauPi(piOpnd, org, tauOpnd, condPtr);
        Inst *place = headInst->getNextInst();
        while (place != NULL) {
            Opcode opc = place->getOpcode();
            if ((opc != Op_Phi) && (opc != Op_TauPoint) && (opc != Op_TauEdge))
                break;
            place = place->getNextInst();
        }
        if (Log::isEnabled()) {
            Log::out() << "Inserting Pi Node for opnd ";
            org->print(Log::out());
            Log::out() << " under condition ";
            cond.print(Log::out());
            if (place!=NULL) {
                Log::out() << " just before inst ";
                place->print(Log::out());
            }
            Log::out() << std::endl;
        }
        if (place != NULL) {
            newInst->insertBefore(place);
        } else {
            block->appendInst(newInst);
        }
    }
}

void Abcd::insertPiNodeForOpndAndAliases(Node *block,
                                         Opnd *org,
                                         const PiCondition &cond,
                                         Opnd *tauOpnd)
{
    if (flags.useAliases) {
        if (Log::isEnabled()) {
            Log::out() << "Inserting Pi Node for opnd ";
            org->print(Log::out());
            Log::out() << " and its aliases";
            Log::out() << " under condition ";
            cond.print(Log::out());
            Log::out() << std::endl;
        }            
        AbcdAliases aliases(mm);
        // check for aliases
        insertPiNodeForOpnd(block, org, cond, tauOpnd);
        if (getAliases(org, &aliases, 0)) {
            if (Log::isEnabled()) {
                Log::out() << "Has aliases ";
                AbcdAliasesSet::iterator iter = aliases.theSet.begin();
                AbcdAliasesSet::iterator end = aliases.theSet.end();
                for ( ; iter != end; iter++) {
                    PiBound alias = *iter;
                    alias.print(Log::out());
                    Log::out() << " ";
                }
                Log::out() << std::endl;
            }
            AbcdAliasesSet::iterator iter = aliases.theSet.begin();
            AbcdAliasesSet::iterator end = aliases.theSet.end();
            const PiBound &lb = cond.getLb();
            const PiBound &ub = cond.getUb();
            for ( ; iter != end; iter++) {
                PiBound alias = *iter; 
                PiBound inverted = alias.invert(org); // org - c
                // plug-in lb and ub into inverted, yields bounds:
                //   [ lb - c, ub - c ]
                PiCondition renamedCondition(PiBound(inverted, org, lb),
                                             PiBound(inverted, org, ub));
                insertPiNodeForOpnd(block, alias.getVar().the_var, 
                                    renamedCondition, tauOpnd);
            }
        }
    } else {
        insertPiNodeForOpnd(block, org, cond, tauOpnd);
    }
}

static ComparisonModifier
negateComparison(ComparisonModifier mod)
{
    switch (mod) {
    case Cmp_GT: return Cmp_GTE;
    case Cmp_GT_Un: return Cmp_GTE_Un;
    case Cmp_GTE: return Cmp_GT;
    case Cmp_GTE_Un: return Cmp_GT_Un;
    case Cmp_EQ: return Cmp_EQ;
    case Cmp_NE_Un: return Cmp_NE_Un;
    default:
        assert(0); return mod;
    }
}

static const char *
printableComparison(ComparisonModifier mod)
{
    switch (mod) {
    case Cmp_GT: return "Cmp_GT";
    case Cmp_GT_Un: return "Cmp_GT_Un";
    case Cmp_GTE: return "Cmp_GTE";
    case Cmp_GTE_Un: return "Cmp_GTE_Un";
    case Cmp_EQ: return "Cmp_EQ";
    case Cmp_NE_Un: return "Cmp_NE_Un";
    default:
        assert(0); return "";
    }
}

static Type::Tag
unsignType(Type::Tag typetag)
{
    switch (typetag) {
    case Type::IntPtr: return Type::UIntPtr;
    case Type::Int8: return Type::UInt8;
    case Type::Int16: return Type::UInt16;
    case Type::Int32: return Type::UInt32;
    case Type::Int64: return Type::UInt64;
    default:
        assert(0); return typetag;
    }
}

void Abcd::insertPiNodesForComparison(Node *block,
                                      ComparisonModifier mod,
                                      const PiCondition &bounds,
                                      Opnd *op,
                                      bool swap_operands,
                                      bool negate_comparison)
{
    if (Log::isEnabled()) {
        Log::out() << "insertPiNodesForComparison(..., ";
        Log::out() << printableComparison(mod);
        Log::out() << ", ";
        bounds.print(Log::out());
        Log::out() << ", ";
        op->print(Log::out());
        Log::out() << ", ";
        Log::out() << (swap_operands ? "true" : "false");
        Log::out() << (negate_comparison ? "true" : "false");
        Log::out() << std::endl;
    }

    PiCondition bounds0 = bounds;
    // add a Pi node for immediate value.
    if (negate_comparison) {
        mod = negateComparison(mod);
        swap_operands = !swap_operands;
        if (Log::isEnabled()) {
            Log::out() << "insertPiNodesForComparison: negating comparison to " ;
            Log::out() << printableComparison(mod);
            Log::out() << std::endl;
        }
    }
    switch (mod) {
    case Cmp_EQ:
        if (negate_comparison)
            insertPiNodeForOpndAndAliases(block, op, bounds0);
        else {
            if (Log::isEnabled()) {
                Log::out() << "insertPiNodesForComparison: cannot represent ! Cmp_EQ" << std::endl;
            }
        }
        // we can't represent the other case
        break;
    case Cmp_NE_Un:
        if (!negate_comparison)
            insertPiNodeForOpndAndAliases(block, op, bounds0);
        else {
            if (Log::isEnabled()) {
                Log::out() << "insertPiNodesForComparison: cannot represent Cmp_NE_Un" << std::endl;
            }
        }
        // we can't represent the other case
        break;
    case Cmp_GT_Un:
        if (swap_operands) { // op > bounds, only a lower bound on op
            Type::Tag optag = op->getType()->tag;
            if (!Type::isUnsignedInteger(optag)) {
                // 1 is a lower bound on int op
                PiCondition oneBounds(PiBound(optag, (int64)1), 
                                      PiBound(optag, (int64)1));
                PiCondition oneLowerBound(oneBounds.only_lower_bound());
                insertPiNodeForOpndAndAliases(block, op, oneLowerBound);
            } else {
                // we can be more precise for an unsigned op
                bounds0 = bounds0.cast(unsignType(bounds0.getType()));
                PiCondition bounds1a(bounds0.only_lower_bound());
                PiCondition bounds1(bounds1a.add((int64)1));
                if (! bounds1.getLb().isUnknown())
                    insertPiNodeForOpndAndAliases(block, op, bounds1);
                else {
                    if (Log::isEnabled()) {
                        Log::out() << "insertPiNodesForComparison(1): bounds1 LB is Unknown;\n\tbounds is ";
                        bounds.print(Log::out());
                        Log::out() << "\n\tbounds0 is ";
                        bounds0.print(Log::out());
                        Log::out() << "\n\tbounds1a is ";
                        bounds1a.print(Log::out());
                        Log::out() << "\n\tbounds1 is ";
                        bounds1.print(Log::out());
                        Log::out() << std::endl;
                    }
                }
            }
        } else { // bounds > op, only an upper bound on op
            Type::Tag optag = op->getType()->tag;
            if (Type::isUnsignedInteger(optag)) {
                // for an unsigned upper bound, we're ok
                bounds0 = bounds0.cast(unsignType(bounds0.getType()));
                PiCondition bounds1(bounds0.only_upper_bound().add((int64)-1));
                if (! bounds1.getUb().isUnknown())
                    insertPiNodeForOpndAndAliases(block, op, bounds1);
                else {
                    if (Log::isEnabled()) {
                        Log::out() << "insertPiNodesForComparison(2): bounds1 LB is Unknown;\n\tbounds is ";
                        bounds.print(Log::out());
                        Log::out() << "\n\tbounds0 is ";
                        bounds0.print(Log::out());
                        Log::out() << "\n\tbounds1 is ";
                        bounds1.print(Log::out());
                        Log::out() << std::endl;
                    }
                }
            } else {
                // otherwise, we know nothing unless bound is a small constant
                PiCondition bounds1(bounds0.only_upper_bound().add((int64)-1));
                if (bounds0.getUb().isConstant()) {
                    int64 ubConst = bounds1.getUb().getConst();
                    if (((optag == Type::Int32) &&
                         ((ubConst&0xffffffff) <= 0x7ffffff) && 
                         ((ubConst&0xffffffff) >= 0)) ||
                        ((optag == Type::Int64) &&
                         ((ubConst <= 0x7ffffff) && 
                          (ubConst >= 0)))) {
                        insertPiNodeForOpndAndAliases(block, op, bounds1);
                    } else {
                        if (Log::isEnabled()) {
                            Log::out() << "insertPiNodesForComparison(2): bounds1 LB is Unknown;\n\tbounds is ";
                            bounds.print(Log::out());
                            Log::out() << "\n\tbounds0 is ";
                            bounds0.print(Log::out());
                            Log::out() << "\n\tbounds1 is ";
                            bounds1.print(Log::out());
                            Log::out() << std::endl;
                        }
                    }
                }
            }
        }
        break;
    case Cmp_GT:
        if (swap_operands) { // op > bounds, only a lower bound on op
            PiCondition bounds1a(bounds0.only_lower_bound());
            PiCondition bounds1(bounds1a.add((int64)1));
            if (! bounds1.getLb().isUnknown())
                insertPiNodeForOpndAndAliases(block, op, bounds1);
            else {
                if (Log::isEnabled()) {
                    Log::out() << "insertPiNodesForComparison(1): bounds1 LB is Unknown;\n\tbounds is ";
                    bounds.print(Log::out());
                    Log::out() << "\n\tbounds0 is ";
                    bounds0.print(Log::out());
                    Log::out() << "\n\tbounds1a is ";
                    bounds1a.print(Log::out());
                    Log::out() << "\n\tbounds1 is ";
                    bounds1.print(Log::out());
                    Log::out() << std::endl;
                }
            }
        } else { // bounds > op, only an upper bound on op
            PiCondition bounds1(bounds0.only_upper_bound().add((int64)-1));
            if (! bounds1.getUb().isUnknown())
                insertPiNodeForOpndAndAliases(block, op, bounds1);
            else {
                if (Log::isEnabled()) {
                    Log::out() << "insertPiNodesForComparison(2): bounds1 LB is Unknown;\n\tbounds is ";
                    bounds.print(Log::out());
                    Log::out() << "\n\tbounds0 is ";
                    bounds0.print(Log::out());
                    Log::out() << "\n\tbounds1 is ";
                    bounds1.print(Log::out());
                    Log::out() << std::endl;
                }
            }
        }
        break;
    case Cmp_GTE_Un:
        if (swap_operands) { // op >= bounds, only lower bound on op
            Type::Tag optag = op->getType()->tag;
            if (!Type::isUnsignedInteger(optag)) {
                // 0 is a lower bound on an int op
                PiCondition zeroBounds(PiBound(optag, (int64)0), 
                                       PiBound(optag, (int64)0));
                PiCondition zeroLowerBound(zeroBounds.only_lower_bound());
                insertPiNodeForOpndAndAliases(block, op, zeroLowerBound);
            } else {
                // we can be more precise for an unsigned op lb
                bounds0 = bounds0.cast(unsignType(bounds0.getType()));
                if (! bounds0.getLb().isUnknown()) {
                    insertPiNodeForOpndAndAliases(block, op, 
                                                  bounds0.only_lower_bound());
                } else {
                    if (Log::isEnabled()) {
                        Log::out() << "insertPiNodesForComparison(3): bounds0 LB is Unknown;\n\tbounds is ";
                        bounds.print(Log::out());
                        Log::out() << "\n\tbounds0 is ";
                        bounds0.print(Log::out());
                        Log::out() << std::endl;
                    }
                }
            }
        } else { // bounds >= op, only upper bound on op
            Type::Tag optag = op->getType()->tag;
            if (Type::isUnsignedInteger(optag)) {
                // unsigned ub on unsigned op 
                bounds0 = bounds0.cast(unsignType(bounds0.getType()));
                if (! bounds0.getUb().isUnknown())
                    insertPiNodeForOpndAndAliases(block, op, 
                                                  bounds0.only_upper_bound());
                else {
                    if (Log::isEnabled()) {
                        Log::out() << "insertPiNodesForComparison(4): bounds0 UB is Unknown;\n\tbounds is ";
                        bounds.print(Log::out());
                        Log::out() << "\n\tbounds0 is ";
                        bounds0.print(Log::out());
                        Log::out() << std::endl;
                    }
                }
            } else {
                // otherwise, we know nothing unless bound is a small constant
                if (bounds0.getUb().isConstant()) {
                    int64 ubConst = bounds0.getUb().getConst();
                    if (((optag == Type::Int32) &&
                         ((ubConst&0xffffffff) <= 0x7ffffff) && 
                         ((ubConst&0xffffffff) >= 0)) ||
                        ((optag == Type::Int64) &&
                         ((ubConst <= 0x7ffffff) && 
                          (ubConst >= 0)))) {
                        insertPiNodeForOpndAndAliases(block, op, bounds0);
                    } else {
                        if (Log::isEnabled()) {
                            Log::out() << "insertPiNodesForComparison(2): bounds0 LB is Unknown;\n\tbounds is ";
                            bounds.print(Log::out());
                            Log::out() << "\n\tbounds0 is ";
                            bounds0.print(Log::out());
                            Log::out() << std::endl;
                        }
                    }
                }
            }
        }
        break;
    case Cmp_GTE:
        if (swap_operands) { // op >= bounds, only lower bound on op
            if (! bounds0.getLb().isUnknown()) {
                insertPiNodeForOpndAndAliases(block, op, 
                                              bounds0.only_lower_bound());
            } else {
                if (Log::isEnabled()) {
                    Log::out() << "insertPiNodesForComparison(3): bounds0 LB is Unknown;\n\tbounds is ";
                    bounds.print(Log::out());
                    Log::out() << "\n\tbounds0 is ";
                    bounds0.print(Log::out());
                    Log::out() << std::endl;
                }
            }
        } else { // bounds >= op, only upper bound on op
            if (! bounds0.getUb().isUnknown())
                insertPiNodeForOpndAndAliases(block, op, 
                                              bounds0.only_upper_bound());
            else {
                if (Log::isEnabled()) {
                    Log::out() << "insertPiNodesForComparison(4): bounds0 UB is Unknown;\n\tbounds is ";
                    bounds.print(Log::out());
                    Log::out() << "\n\tbounds0 is ";
                    bounds0.print(Log::out());
                    Log::out() << std::endl;
                }
            }
        }
        break;
    case Cmp_Zero:
    case Cmp_NonZero:
    case Cmp_Mask:
        assert(0);
        break;
    default:
    assert(false);
    break;
    }
}

// Insert Pi Nodes for any variables occurring in the branch test
//
// Since we're examining the test anyway, let's figure out the conditions
// here, too, so we don't have to duplicate any code.  Note that this 
// condition may already be in terms of Pi variables from the predecessor 
// block, since 
//   -- predecessor dominates this block
//   -- we are traversing blocks in a dominator-tree preorder
// so we must have already visited the predecessor.
//
// We also must add the new Pi variable to our map.
// 
void Abcd::insertPiNodesForBranch(Node *block, BranchInst *branchi, 
                                  Edge::Kind kind) // True or False only
{
    Type::Tag instTypeTag = branchi->getType();
    if (!Type::isInteger(instTypeTag))
        return; 
    ComparisonModifier mod = branchi->getComparisonModifier();
    if (branchi->getNumSrcOperands() == 1) {
        Opnd *op0 = branchi->getSrc(0);
        PiCondition zeroBounds(PiBound(instTypeTag, (int64)0), 
                               PiBound(instTypeTag, (int64)0));
        switch (mod) {
        case Cmp_Zero:
            insertPiNodesForComparison(block,
                                       Cmp_EQ,
                                       zeroBounds,
                                       op0,
                                       false,
                                       (kind == Edge::Kind_False)); // negate if false edge
            break;
        case Cmp_NonZero:
            insertPiNodesForComparison(block,
                                       Cmp_EQ, // use EQ
                                       zeroBounds,
                                       op0,
                                       false, 
                                       (kind == Edge::Kind_True)); // but negate if true edge
            break;
    default:
        break;
        }
    } else {
        Opnd *op0 = branchi->getSrc(0);
        Opnd *op1 = branchi->getSrc(1);
        assert(branchi->getNumSrcOperands() == 2);
        PiCondition bounds0(op0->getType()->tag, op0);
        PiCondition bounds1(op1->getType()->tag, op1);
        if (!bounds0.isUnknown()) {
            insertPiNodesForComparison(block,
                                       mod,
                                       bounds0,
                                       op1,
                                       false,
                                       (kind == Edge::Kind_False)); // negate for false edge
        }
        if (!bounds1.isUnknown()) {
            insertPiNodesForComparison(block,
                                       mod,
                                       bounds1,
                                       op0,
                                       true,
                                       (kind == Edge::Kind_False)); // negate for false edge
        }                                       
    }
}

SsaTmpOpnd* Abcd::getBlockTauPoint(Node *block) {
    if ((lastTauPointBlock == block) && blockTauPoint) return blockTauPoint;
    Inst *firstInst = (Inst*)block->getFirstInst();
    Inst *inst = (Inst*)firstInst->getNextInst();
    for (; inst != NULL; inst = inst->getNextInst()) {
        if (inst->getOpcode() == Op_TauPoint) {
            blockTauPoint = inst->getDst()->asSsaTmpOpnd();
            assert(blockTauPoint);
            lastTauPointBlock = block;
            return blockTauPoint;
        }
    }
    for (inst = firstInst->getNextInst(); inst != NULL; inst = inst->getNextInst()) {
        if (inst->getOpcode() != Op_Phi) {
            break; // insert before inst.
        }
    }
    // no non-phis, insert before inst;
    TypeManager &tm = irManager.getTypeManager();
    SsaTmpOpnd *tauOpnd = irManager.getOpndManager().createSsaTmpOpnd(tm.getTauType());
    Inst* tauPoint = irManager.getInstFactory().makeTauPoint(tauOpnd);
    if(Log::isEnabled()) {
        Log::out() << "Inserting tauPoint inst ";
        tauPoint->print(Log::out());
        if (inst!=NULL) {
            Log::out() << " before inst ";
            inst->print(Log::out());
        } 
        Log::out() << std::endl;
    }
    if (inst!=NULL) {
        tauPoint->insertBefore(inst);
    } else {
        block->appendInst(tauPoint);
    }
    blockTauPoint = tauOpnd;
    lastTauPointBlock = block;
    return tauOpnd;
}

SsaTmpOpnd* Abcd::getBlockTauEdge(Node *block) {
    if ((lastTauEdgeBlock == block) && blockTauEdge) return blockTauEdge;
    Inst *firstInst = (Inst*)block->getFirstInst();
    Inst *inst = firstInst->getNextInst();
    for (; inst != NULL; inst = inst->getNextInst()) {
        if (inst->getOpcode() == Op_TauEdge) {
            blockTauEdge = inst->getDst()->asSsaTmpOpnd();
            assert(blockTauEdge);
            lastTauEdgeBlock = block;
            return blockTauEdge;
        }
    }
    for (inst = firstInst->getNextInst(); inst != NULL; inst = inst->getNextInst()) {
        if ((inst->getOpcode() != Op_Phi) && (inst->getOpcode() != Op_TauPoint)) {
            break; // insert before inst.
        }
    }
    // no non-phis, insert before inst;
    TypeManager &tm = irManager.getTypeManager();
    SsaTmpOpnd *tauOpnd = irManager.getOpndManager().createSsaTmpOpnd(tm.getTauType());
    Inst* tauEdge = irManager.getInstFactory().makeTauEdge(tauOpnd);
    if(Log::isEnabled()) {
        Log::out() << "Inserting tauEdge inst ";
        tauEdge->print(Log::out());
        if (inst!=NULL) {
            Log::out() << " before inst ";
            inst->print(Log::out()); 
        }
        Log::out() << std::endl;
    }
    if (inst != NULL) {
        tauEdge->insertBefore(inst);
    }  else {
        block->appendInst(tauEdge);
    }
    blockTauEdge = tauOpnd;
    lastTauEdgeBlock = block;
    return tauOpnd;
}

SsaTmpOpnd* Abcd::getTauUnsafe() {
    if (!tauUnsafe) {
        Node *head = irManager.getFlowGraph().getEntryNode();
        Inst *entryLabel = (Inst*)head->getFirstInst();
        // first search for one already there
        Inst *inst = entryLabel->getNextInst();
        while (inst != NULL) {
            if (inst->getOpcode() == Op_TauUnsafe) {
                tauUnsafe = inst->getDst()->asSsaTmpOpnd();
                assert(tauUnsafe);
                return tauUnsafe;
            }
            inst = inst->getNextInst();
        }
        // need to insert one
        TypeManager &tm = irManager.getTypeManager();
        SsaTmpOpnd *tauOpnd = irManager.getOpndManager().createSsaTmpOpnd(tm.getTauType());
        Inst *tauUnsafeInst = irManager.getInstFactory().makeTauUnsafe(tauOpnd);
        // place after label and Phi instructions
        inst = entryLabel->getNextInst();
        while (inst != NULL) {
            Opcode opc = inst->getOpcode();
            if ((opc != Op_Phi) && (opc != Op_TauPi) && (opc != Op_TauPoint)) {
                break;
            }
            inst = inst->getNextInst();
        }
        if(Log::isEnabled()) {
            Log::out() << "Inserting tauUnsafe inst ";
            tauUnsafeInst->print(Log::out());
            if (inst!=NULL) {
                Log::out() << " before inst ";
                inst->print(Log::out());
            }
            Log::out() << std::endl;
        }
        if (inst!=NULL) {
            tauUnsafeInst->insertBefore(inst);
        } else {
            head->appendInst(tauUnsafeInst);
        }
        tauUnsafe = tauOpnd;
    }
    return tauUnsafe;
};

SsaTmpOpnd* Abcd::getTauSafe() {
    if (!tauSafe) {
        Node *head = irManager.getFlowGraph().getEntryNode();
        Inst *entryLabel = (Inst*)head->getFirstInst();
        // first search for one already there
        Inst *inst = entryLabel->getNextInst();
        while (inst != NULL) {
            if (inst->getOpcode() == Op_TauSafe) {
                tauSafe = inst->getDst()->asSsaTmpOpnd();
                assert(tauSafe);
                return tauSafe;
            }
            inst = inst->getNextInst();
        }
        // need to insert one
        TypeManager &tm = irManager.getTypeManager();
        SsaTmpOpnd *tauOpnd = irManager.getOpndManager().createSsaTmpOpnd(tm.getTauType());
        Inst *tauSafeInst = irManager.getInstFactory().makeTauSafe(tauOpnd);
        // place after label and Phi instructions
        inst = entryLabel->getNextInst();
        while (inst != NULL) {
            Opcode opc = inst->getOpcode();
            if ((opc != Op_Phi) && (opc != Op_TauPi) && (opc != Op_TauPoint)) {
                break;
            }
            inst = inst->getNextInst();
        }
        if(Log::isEnabled()) {
            Log::out() << "Inserting tauSafe inst ";
            tauSafeInst->print(Log::out());
            if (inst!=NULL) {
                Log::out() << " before inst ";
                inst->print(Log::out());
            }
            Log::out() << std::endl;
        }
        if (inst!=NULL) {
            tauSafeInst->insertBefore(inst);
        } else {
            head->appendInst(tauSafeInst);
        }
        tauSafe = tauOpnd;
    }
    return tauSafe;
};

void Abcd::insertPiNodesForUnexceptionalPEI(Node *block, Inst *lasti) 
{
    switch (lasti->getOpcode()) {
    case Op_TauCheckBounds:
        {
            // the number of newarray elements must be >= 0.
            assert(lasti->getNumSrcOperands() == 2);
            Opnd *idxOp = lasti->getSrc(1);
            Opnd *boundsOp = lasti->getSrc(0);

            if (Log::isEnabled()) {
                Log::out() << "Adding info about CheckBounds instruction ";
                lasti->print(Log::out());
                Log::out() << std::endl;
            }
            Type::Tag typetag = idxOp->getType()->tag;
            PiBound lb(typetag, int64(0));
            PiBound ub(typetag, 1, VarBound(boundsOp),int64(-1));
            PiCondition bounds0(lb, ub);
            Opnd *tauOpnd = lasti->getDst(); // use the checkbounds tau
            insertPiNodeForOpndAndAliases(block, idxOp, bounds0, tauOpnd);

            PiBound idxBound(typetag, 1, VarBound(idxOp), int64(1));
            PiCondition bounds1(idxBound, PiBound(typetag, false));
            insertPiNodeForOpndAndAliases(block, boundsOp, bounds1, tauOpnd);
        }
        break;
    case Op_NewArray:
        {
            // the number of newarray elements must be in [0, MAXINT32]
            assert(lasti->getNumSrcOperands() == 1);
            Opnd *numElemOpnd = lasti->getSrc(0);
            if (Log::isEnabled()) {
                Log::out() << "Adding info about NewArray instruction ";
                lasti->print(Log::out());
                Log::out() << std::endl;
            }
            Opnd *tauOpnd = getBlockTauEdge(block); // need to use a TauEdge
            PiCondition bounds0(PiBound(numElemOpnd->getType()->tag, int64(0)),
                                PiBound(numElemOpnd->getType()->tag, int64(0x7fffffff)));
            insertPiNodeForOpndAndAliases(block, numElemOpnd, bounds0, tauOpnd);
        }
        break;
    case Op_NewMultiArray:
        {
            // the number of newarray dimensions must be >= 1.
            uint32 numOpnds = lasti->getNumSrcOperands();
            assert(numOpnds >= 1);
            StlSet<Opnd *> done(mm);
            if (Log::isEnabled()) {
                Log::out() << "Adding info about NewMultiArray instruction ";
                lasti->print(Log::out());
                Log::out() << std::endl;
            }
            Opnd *tauOpnd = 0;
            // the number of newarray elements must be in [0, MAXINT32]
            for (uint32 opndNum = 0; opndNum < numOpnds; opndNum++) {
                Opnd *thisOpnd = lasti->getSrc(opndNum);
                if (!done.has(thisOpnd)) {
                    done.insert(thisOpnd);
                    PiCondition bounds0(PiBound(thisOpnd->getType()->tag, int64(0)),
                                        PiBound(thisOpnd->getType()->tag, int64(0x7fffffff)));
                    if (!tauOpnd) tauOpnd = getBlockTauEdge(block); // must use a tauEdge
                    insertPiNodeForOpndAndAliases(block, thisOpnd, bounds0, tauOpnd);
                }
            }
        }
        break;
    default:
        break;
    }    
}

// Add a Pi node in the node if it is after
// a test which tells something about a variable.
// For now, don't bother with Exception edges.
void Abcd::insertPiNodes(Node *block)
{
    Edge *domEdge = 0;

    // see if there is a predecessor block idom such that 
    //  (1) idom dominates this one
    //  (2) this block dominates all other predecessors
    //  (3) idom has multiple out-edges
    //  (4) idom has only 1 edge to this node
    
    // (1a) if a predecessor dominates it must be idom
    Node *idom = dominators.getIdom(block);
    
    // (3) must exist and have multiple out-edges
    if ((idom == NULL) || (idom->hasOnlyOneSuccEdge())) {
        return;
    }
    
    if (Log::isEnabled()) {
        Log::out() << "Checking block " << (int)block->getId() << " with idom "
                   << (int) idom->getId() << std::endl;
    }

    if (block->hasOnlyOnePredEdge()) {
        // must be from idom -- (1b)
        // satisfies (2) trivially
        domEdge = *(block->getInEdges().begin());
    } else { 
        // check (1b) and (2)
        const Edges &inedges = block->getInEdges();
        typedef Edges::const_iterator EdgeIter;
        EdgeIter eLast = inedges.end();
        for (EdgeIter eIter = inedges.begin(); eIter != eLast; eIter++) {
            Edge *inEdge = *eIter;
            Node *predBlock = inEdge->getSourceNode();
            if (predBlock == idom) {
                // (1b) found idom
                if (domEdge) {
                    // failed (4): idom found on more than one incoming edge
                    return;
                }
                domEdge = inEdge;
            } else if (! dominators.dominates(block, predBlock)) {
                // failed (2)
                return;
            }
        }
    }

    if (domEdge) { 
        Edge *inEdge = domEdge;
        Node *predBlock = idom;
        if (Log::isEnabled()) {
            Log::out() << "Checking branch for " << (int)block->getId() << " with idom "
                       << (int) idom->getId() << std::endl;
        }
        if (!predBlock->hasOnlyOneSuccEdge()) {
            Edge::Kind kind = inEdge->getKind();
            switch (kind) {
            case Edge::Kind_True:
            case Edge::Kind_False:
                {
                    Inst* branchi1 = (Inst*)predBlock->getLastInst();
                    assert(branchi1 != NULL);
                    BranchInst* branchi = branchi1->asBranchInst();
                    if (branchi && branchi->isConditionalBranch()) {
                        insertPiNodesForBranch(block, branchi, kind);
                    } else {
                        return;
                    }
                }
                break;
                
            case Edge::Kind_Dispatch: 
                return;

            case Edge::Kind_Unconditional:
                // Previous block must have a PEI
                // since it had multiple out-edges.
                // This is the unexceptional condition.
                { 
                    Inst* lasti = (Inst*)predBlock->getLastInst();
                    assert(lasti != NULL);
                    insertPiNodesForUnexceptionalPEI(block, lasti);
                }
                // We could look for a bounds check in predecessor.
                
                // But: since now all useful PEIs have explicit results,
                // they imply a Pi-like action.
                break;
                
            case Edge::Kind_Catch:
                break;
        default:
        break;
            };
        }
    }
}

void Abcd::renamePiVariables(Node *block) 
{
    // For each variable use in the block, check for a Pi version in
    // the piTable.  Since we are visiting in preorder over dominator
    // tree dominator order, any found version will dominate this node.

    // we defer adding any new mappings for the Pi instructions here until
    // we are past the Pi instructions
    
    // first process any pi nodes, just the RHSs
    Inst* headInst = (Inst*)block->getFirstInst();
    for (int phase=0; phase < 2; ++phase) {
        // phase 0: remap just Pi node source operands
        // phase 1: add Pi remappings, remap source operands of other instructions
    
        for (Inst* inst = headInst->getNextInst(); inst != NULL; inst = inst->getNextInst()) {
            
            if (inst->getOpcode() == Op_TauPi) {
                if (phase == 1) {
                    // add any Pi node destination to the map.
                    
                    Opnd *dstOpnd = inst->getDst();
                    assert(dstOpnd->isPiOpnd());
                    Opnd *orgOpnd = dstOpnd->asPiOpnd()->getOrg();
                    if (flags.useAliases) {
                        if (orgOpnd->isSsaVarOpnd()) {
                            orgOpnd = orgOpnd->asSsaVarOpnd()->getVar();
                        }
                    }
                    piMap->insert(orgOpnd, dstOpnd);
                    if (Log::isEnabled()) {
                        Log::out() << "adding remap for Pi of ";
                        orgOpnd->print(Log::out());
                        Log::out() << " to ";
                        inst->getDst()->print(Log::out());
                        Log::out() << std::endl;
                    }
                    
                    continue; // don't remap Pi sources;
                }
            } else {
                if (phase == 0) {
                    // no more Pi instructions, we're done with phase 0.
                    break; 
                }
            }
            
            // now process source operands
            uint32 numOpnds = inst->getNumSrcOperands();
            for (uint32 i=0; i<numOpnds; i++) {
                Opnd *opnd = inst->getSrc(i);
                if (opnd->isPiOpnd())
                    opnd = opnd->asPiOpnd()->getOrg();
                Opnd *foundOpnd = piMap->lookup(opnd);
                if (foundOpnd) {
                    inst->setSrc(i,foundOpnd);
                }
            }

            if (inst->getOpcode() == Op_TauPi) {
                // for a Pi, remap variables appearing in the condition as well
                if (Log::isEnabled()) {
                    Log::out() << "remapping condition in ";
                    inst->print(Log::out());
                    Log::out() << std::endl;
                }
                TauPiInst *thePiInst = inst->asTauPiInst();
                assert(thePiInst);
                PiCondition *cond = thePiInst->cond;
                if (Log::isEnabled()) {
                    Log::out() << "  original condition is ";
                    cond->print(Log::out());
                    Log::out() << std::endl;
                }
                Opnd *lbRemap = cond->getLb().getVar().the_var;
                if (lbRemap) {
                    if (Log::isEnabled()) {
                        Log::out() << "  has lbRemap=";
                        lbRemap->print(Log::out());
                        Log::out() << std::endl;
                    }
                    if (lbRemap->isPiOpnd())
                        lbRemap = lbRemap->asPiOpnd()->getOrg();
                    Opnd *lbRemapTo = piMap->lookup(lbRemap);
                    if (lbRemapTo) {
                        if (Log::isEnabled()) {
                            Log::out() << "adding remap of lbRemap=";
                            lbRemap->print(Log::out());
                            Log::out() << " to lbRemapTo=";
                            lbRemapTo->print(Log::out());
                            Log::out() << " to condition ";
                            cond->print(Log::out());
                        }
                        PiCondition remapped(*cond, lbRemap, lbRemapTo);
                        if (Log::isEnabled()) {
                            Log::out() << " YIELDS1 ";
                            remapped.print(Log::out());
                        }
                        *cond = remapped;
                        if (Log::isEnabled()) {
                            Log::out() << " YIELDS ";
                            cond->print(Log::out());
                            Log::out() << std::endl;
                        }
                    }
                }
                Opnd *ubRemap = cond->getUb().getVar().the_var;
                if (ubRemap && (lbRemap != ubRemap)) {
                    if (Log::isEnabled()) {
                        Log::out() << "  has ubRemap=";
                        ubRemap->print(Log::out());
                        Log::out() << std::endl;
                    }
                    if (ubRemap->isPiOpnd())
                        ubRemap = ubRemap->asPiOpnd()->getOrg();
                    Opnd *ubRemapTo = piMap->lookup(ubRemap);
                    if (ubRemapTo) {
                        if (Log::isEnabled()) {
                            Log::out() << "adding remap of ubRemap=";
                            ubRemap->print(Log::out());
                            Log::out() << " to ubRemapTo=";
                            ubRemapTo->print(Log::out());
                            Log::out() << " to condition ";
                            cond->print(Log::out());
                        }
                        PiCondition remapped(*cond, ubRemap, ubRemapTo);
                        if (Log::isEnabled()) {
                            Log::out() << " YIELDS1 ";
                            remapped.print(Log::out());
                        }
                        *cond = remapped;
                        if (Log::isEnabled()) {
                            Log::out() << " YIELDS ";
                            cond->print(Log::out());
                            Log::out() << std::endl;
                        }
                    }
                }
            }
        }

    }        
}
 
// an InstWalker
class AbcdSolverWalker {
    AbcdSolver *theSolver;
public:
    AbcdSolverWalker(AbcdSolver *solver) : theSolver(solver) {};
    void applyToInst(Inst *i) {
        theSolver->tryToEliminate(i);
    }
};

void Abcd::removeRedundantBoundsChecks()
{

    assert(!solver);
    solver = new (mm) AbcdSolver(this);
    AbcdSolverWalker solveInst(solver);  // to apply to each 
    typedef Inst2NodeWalker<true, AbcdSolverWalker> AbcdSolverNodeWalker;
    AbcdSolverNodeWalker solveNode(solveInst);
    NodeWalk<AbcdSolverNodeWalker>(irManager.getFlowGraph(), solveNode);

    solver = 0;
}

// a ScopedDomNodeInstWalker, forward/preorder
class RemovePiWalker {
    Abcd *thePass;
    Node *block;
public:
    void startNode(DominatorNode *domNode) { block = domNode->getNode(); };
    void applyToInst(Inst *i) { thePass->removePiNodes(block, i); };
    void finishNode(DominatorNode *domNode) { };

    void enterScope() { };
    void exitScope() { };
    RemovePiWalker(Abcd *thePass0)
        : thePass(thePass0), block(0) // forward
    {
    };
};


void Abcd::removePiNodes()
{
    RemovePiWalker removePiWalker(this);

    typedef ScopedDomNodeInst2DomWalker<true, RemovePiWalker>
        RemovePiDomWalker;
    RemovePiDomWalker removePiDomWalker(removePiWalker);

    DomTreeWalk<true, RemovePiDomWalker>(dominators, removePiDomWalker,
                                         mm);
}

void Abcd::removePiNodes(Node *block, Inst *inst)
{
    AbcdReasons *why;
    if (inst->getOpcode() == Op_TauPi) {
        inst->unlink();
    } else if ((!flags.dryRun) &&
               (inst->getOpcode() == Op_TauCheckBounds) &&
               isMarkedToEliminate(inst, why)) {
        Opnd* srcOpnd = (flags.useReasons
                         ? getReasonTau(why, inst)
                         : getBlockTauPoint(block));
        Opnd* dstOpnd = inst->getDst();
        inst->setDst(OpndManager::getNullOpnd());
        Inst* copy = irManager.getInstFactory().makeCopy(dstOpnd,srcOpnd);
        copy->insertBefore(inst);
        FlowGraph::eliminateCheck(irManager.getFlowGraph(),block, inst, false);
    } else {
        uint32 numOpnds = inst->getNumSrcOperands();
        for (uint32 i=0; i<numOpnds; i++) {
            Opnd *opnd0 = inst->getSrc(i);
            Opnd *opnd = opnd0;
            Opnd *newOpnd = opnd0;
            while (newOpnd) {
                opnd = newOpnd;
                newOpnd = 0;
                if (opnd->isPiOpnd()) {
                    // it's a Pi operand, dereference directly
                    PiOpnd *piOpnd = opnd->asPiOpnd();
                    newOpnd = piOpnd->getOrg();
                }
            }
            if (flags.remConv &&
                (!flags.dryRun) &&
                (opnd->getInst()->getOpcode() == Op_Conv)) {
                Opnd *srcOpnd = opnd->getInst()->getSrc(0);
                bool deref = isMarkedToEliminate(opnd->getInst(), why);
                while (deref) {
                    deref = false;
                    if (srcOpnd->getType()->tag ==
                        opnd->getType()->tag) {
                        opnd = srcOpnd;
                        Inst *newInst = opnd->getInst();
                        if ((newInst->getOpcode() == Op_Conv) &&
                            isMarkedToEliminate(newInst, why)) {

                            deref = true;
                        }
                    }
                }
            }
            if (flags.unmaskShifts &&
                (!flags.dryRun) &&
                ((opnd->getInst()->getOpcode() == Op_Shr) ||
                 (opnd->getInst()->getOpcode() == Op_Shl))) {
                Inst *the_inst = opnd->getInst();

                if (isMarkedToEliminate(the_inst, why)) {
                    // don't eliminate, just clear shift-mask

                    the_inst->setShiftMaskModifier(ShiftMask_None);
                }
            }                    

            if (opnd0 != opnd) {
                inst->setSrc(i,opnd);
            };
        }

        if (flags.remOneBound &&
            (inst->getOpcode() == Op_TauCheckBounds)) {
            if (isMarkedToEliminateLB(inst, why)) {
                Opnd *dstTau = inst->getDst();
                inst->setDst(OpndManager::getNullOpnd());
                Opnd *a = inst->getSrc(1); // index
                Opnd *b = inst->getSrc(0); // array length
                // don't bother with tauAnd, chkbound is immobile
                Inst *new_check 
                    = irManager.getInstFactory().makeTauCheckUpperBound(dstTau, a, b);
                if (inst->getOverflowModifier() == Overflow_None) {
                    new_check->setOverflowModifier(Overflow_None);
                }
                if (Log::isEnabled()) {
                    Log::out() << " inserting ";
                    new_check->print(Log::out());
                    Log::out() << " in place of ";
                    inst->print(Log::out());
                    Log::out() << std::endl;
                }
                new_check->insertBefore(inst);
                inst->unlink();

            } else if (isMarkedToEliminateUB(inst, why)) {
                Opnd *dstTau = inst->getDst();
                inst->setDst(OpndManager::getNullOpnd());
                Opnd *b = inst->getSrc(1); // index
                
                // build a constant 0 operand a
                Type *idxType = b->getType();
                ConstInst::ConstValue constZero;
                constZero.i8 = 0;
                OpndManager &opndManager = irManager.getOpndManager();
                SsaTmpOpnd *a = opndManager.createSsaTmpOpnd(idxType);
                InstFactory &instFactory = irManager.getInstFactory();
                Inst *ldcInst = instFactory.makeLdConst(a, constZero);
                // don't bother with tauAnd, chk is immobile
                Inst *new_check 
                    = instFactory.makeTauCheckLowerBound(dstTau, a, b);
                if (inst->getOverflowModifier() == Overflow_None) {
                    new_check->setOverflowModifier(Overflow_None);
                }
                if (Log::isEnabled()) {
                    Log::out() << " inserting ";
                    ldcInst->print(Log::out());
                    Log::out() << ";";
                    new_check->print(Log::out());
                    Log::out() << "; in place of ";
                    inst->print(Log::out());
                    Log::out() << std::endl;
                }
                new_check->insertBefore(inst);
                ldcInst->insertBefore(new_check);
                inst->unlink();
            }
        }
    }
}

void Abcd::markInstToEliminate(Inst *i)
{
    if (Log::isEnabled()) {
        Log::out() << "Can eliminate instruction "; i->print(Log::out());
        Log::out() << std::endl;
    }
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminate.begin();
    itertype finish = canEliminate.end();
    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        canEliminate.insert(lb, InstReasonPair(i, 0));
    }
}

void Abcd::markInstToEliminateAndWhy(Inst *i, AbcdReasons *why)
{
    if (Log::isEnabled()) {
        Log::out() << "Can eliminate instruction "; i->print(Log::out());
        Log::out() << std::endl;
    }
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminate.begin();
    itertype finish = canEliminate.end();
    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        canEliminate.insert(lb, InstReasonPair(i, why));
    }
    AbcdReasons *why2;
    bool ism2e = isMarkedToEliminate(i, why2);
    if( !(ism2e && (why == why2)) ) assert(0);
}

void Abcd::markCheckToEliminate(Inst *i)
{
    markInstToEliminate(i);
}

void Abcd::markCheckToEliminateAndWhy(Inst *i, AbcdReasons *why)
{
    markInstToEliminateAndWhy(i, why);
}

bool Abcd::isMarkedToEliminate(Inst *i, AbcdReasons *&why)
{
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminate.begin();
    itertype finish = canEliminate.end();

    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        return false;
    }
    why = lb->second;
    return true;
}

void Abcd::markInstToEliminateLB(Inst *i)
{
    if (Log::isEnabled()) {
        Log::out() << "Can eliminate LB check in instruction "; i->print(Log::out());
        Log::out() << std::endl;
    }
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminateLB.begin();
    itertype finish = canEliminateLB.end();
    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        canEliminateLB.insert(lb, InstReasonPair(i, 0));
    }
}

void Abcd::markInstToEliminateLBAndWhy(Inst *i, AbcdReasons *why)
{
    if (Log::isEnabled()) {
        Log::out() << "Can eliminate LB check in instruction "; i->print(Log::out());
        Log::out() << " for why=" << ::std::hex << (void *) why << ::std::dec << "=";
        why->print(Log::out());
        Log::out() << std::endl;
    }
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminateLB.begin();
    itertype finish = canEliminateLB.end();
    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        canEliminateLB.insert(lb, InstReasonPair(i, why));
    }
    AbcdReasons *why2;
    bool ism2eLB = isMarkedToEliminateLB(i, why2);
    if( !(ism2eLB && (why == why2)) ) assert(0);
}

void Abcd::markCheckToEliminateLB(Inst *i)
{
    markInstToEliminateLB(i);
}

void Abcd::markCheckToEliminateLBAndWhy(Inst *i, AbcdReasons *why)
{
    markInstToEliminateLBAndWhy(i, why);
}

bool Abcd::isMarkedToEliminateLB(Inst *i, AbcdReasons *&why)
{
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminateLB.begin();
    itertype finish = canEliminateLB.end();

    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        return false;
    }
    why = lb->second;
    return true;
}

void Abcd::markInstToEliminateUB(Inst *i)
{
    if (Log::isEnabled()) {
        Log::out() << "Can eliminate UB of instruction "; i->print(Log::out());
        Log::out() << std::endl;
    }
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminateUB.begin();
    itertype finish = canEliminateUB.end();
    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        canEliminateUB.insert(lb, InstReasonPair(i, 0));
    }
}

void Abcd::markInstToEliminateUBAndWhy(Inst *i, AbcdReasons *why)
{
    if (Log::isEnabled()) {
        Log::out() << "Can eliminate UB of instruction "; i->print(Log::out());
        Log::out() << std::endl;
    }
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminateUB.begin();
    itertype finish = canEliminateUB.end();
    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        canEliminateUB.insert(lb, InstReasonPair(i, why));
    }
    AbcdReasons *why2;
    bool ism2eUB = isMarkedToEliminateUB(i, why2);
    if( !(ism2eUB && (why == why2)) ) assert(0);
}

void Abcd::markCheckToEliminateUB(Inst *i)
{
    markInstToEliminateUB(i);
}

void Abcd::markCheckToEliminateUBAndWhy(Inst *i, AbcdReasons *why)
{
    markInstToEliminateUBAndWhy(i, why);
}

bool Abcd::isMarkedToEliminateUB(Inst *i, AbcdReasons *&why)
{
    typedef StlVector<InstReasonPair>::iterator itertype;
    itertype start = canEliminateUB.begin();
    itertype finish = canEliminateUB.end();

    itertype lb = ::std::lower_bound(start, finish, InstReasonPair(i, 0));
    if ((lb == finish) || (lb->first != i)) {
        return false;
    }
    why = lb->second;
    return true;
}

Opnd *Abcd::getConstantOpnd(Opnd *opnd)
{
    if (ConstantFolder::isConstant(opnd)) {
        return opnd;
    } else {
        return 0;
    }
}

bool Abcd::getAliases(Opnd *opnd, AbcdAliases *aliases, int64 addend)
{
    Inst *inst = opnd->getInst();
    switch (inst->getOpcode()) {
    case Op_TauPi:
        return getAliases(inst->getSrc(0), aliases, addend);

    case Op_Add:
        {
            Opnd *op0 = inst->getSrc(0);
            Opnd *op1 = inst->getSrc(1);
            Opnd *constOpnd0 = getConstantOpnd(op0);
            Opnd *constOpnd1 = getConstantOpnd(op1);
            if ((constOpnd0 || constOpnd1) &&
                (inst->getType() == Type::Int32)) {
                // I assume we've done folding first
                assert(!(constOpnd0 && constOpnd1));
                if (constOpnd1) {
                    // swap the operands;
                    constOpnd0 = constOpnd1;
                    op1 = op0;
                }
                // now constOpnd0 should be constant
                // op1 is the non-constant operand
                
                Inst *inst0 = constOpnd0->getInst();
                assert(inst0);
                ConstInst *cinst0 = inst0->asConstInst();
                assert(cinst0);
                ConstInst::ConstValue cv = cinst0->getValue();
                int32 c = cv.i4;
                int64 sumc = c + addend;
                if (add_overflowed<int64>(sumc, c, addend)) {
                    return false;
                } else {
                    VarBound vb(op1);
                    aliases->theSet.insert(PiBound(inst->getType(), 1, vb, sumc));
                    getAliases(op1, aliases, sumc);
                    return true;
                }
            }
        }
        break;
    case Op_Sub:
        {
            Opnd *constOpnd = getConstantOpnd(inst->getSrc(1));
            if (constOpnd && (inst->getType() == Type::Int32)) {
                Opnd *op0 = inst->getSrc(0);
                Opnd *op1 = constOpnd;
                // now op1 should be constant
                // I assume we've done folding first
                if( !(!getConstantOpnd(op0)) ) assert(0);
                
                Inst *inst1 = op1->getInst();
                assert(inst1);
                ConstInst *cinst1 = inst1->asConstInst();
                assert(cinst1);
                ConstInst::ConstValue cv = cinst1->getValue();
                int64 c = cv.i4;
                int64 negc = -c;
                int64 subres = addend + negc;
                if (neg_overflowed<int64>(negc, c) ||
                    add_overflowed<int64>(subres, addend, negc)) {
                    return false;
                } else {
                    VarBound vb(op1);
                    aliases->theSet.insert(PiBound(inst->getType(), 1, vb, subres));
                    getAliases(op1, aliases, subres);
                    return true;
                }
            }
        }
        break;
    case Op_Copy:
        assert(0); // do copy propagation first
        break;
    case Op_TauCheckZero:
        return false;
    default:
        break;
    }
    return false;
}

template <class inst_fun_type>
struct NodeInst2NodeFun : ::std::unary_function<Node *, void>
{
    inst_fun_type underlying_fun;
    NodeInst2NodeFun(const inst_fun_type &theFun) : underlying_fun(theFun) {};
    void operator()(Node *theNode) {
        Inst *thisinst = (Inst*)theNode->getFirstInst();
        assert(thisinst);
        do {
            underlying_fun(theNode, thisinst);
            thisinst = thisinst->getNextInst();
        } while (thisinst != NULL);
    }
};

struct SsaCheckingFun : public ::std::binary_function<Node *, Inst *, void> {
    void operator()(Node *node, Inst *i) {
        if (i->getOpcode() == Op_Phi) {
            const Edges& edges2 = node->getInEdges();
            uint32 numEdges = (uint32) edges2.size();
            uint32 numOps = i->getNumSrcOperands();
            if( !(numOps == numEdges) ) assert(0);
        }
    }
};

void checkSSA(ControlFlowGraph* flowgraph)
{
    SsaCheckingFun checkInst;
    NodeInst2NodeFun<SsaCheckingFun> checkNode(checkInst);
    Node2FlowgraphFun<NodeInst2NodeFun<SsaCheckingFun> > 
        checkFlowGraph(checkNode);

    checkFlowGraph(*flowgraph);
}

bool
Abcd::isConvOpnd(const Opnd *opnd) {
    Inst *defInst = opnd ? opnd->getInst() : 0;
    return (defInst && (defInst->getOpcode() == Op_Conv));
}

bool
Abcd::convPassesSource(const Opnd *opnd) {
    assert(isConvOpnd(opnd));
    Inst *instr = opnd->getInst();
    Opnd *srci = instr->getSrc(0); // the source operand
    Type::Tag srcType = srci->getType()->tag;
    Type::Tag dstType = opnd->getType()->tag;
    Type::Tag instrType = instr->getType();
    if (typeIncludes(dstType, instrType) &&
        typeIncludes(instrType, srcType)) {
        return true;
    } else {
        return false;
    }
}

Opnd *Abcd::getConvSource(const Opnd *opnd) {
    assert(isConvOpnd(opnd));
    Inst *defInst = opnd->getInst();
    return defInst->getSrc(0);
}

// true if type1 includes all values from type2
bool Abcd::typeIncludes(Type::Tag type1, Type::Tag type2)
{
    if (type1 == type2) return true;
    int64 lb1, lb2, ub1, ub2;
    bool hasBounds1, hasBounds2;
    hasBounds1 = Abcd::hasTypeBounds(type1, lb1, ub1);
    hasBounds2 = Abcd::hasTypeBounds(type2, lb2, ub2);
    if ((!hasBounds1) || (!hasBounds2)) {
        return false;
    } else {
        if ((lb1 <= lb2) && (ub2 <= ub1)) {
            return true;
        } else {
            return false;
        }
    }
}

bool 
Abcd::hasTypeBounds(Type::Tag srcTag, int64 &lb, int64 &ub)
{
    switch (srcTag) {
    case Type::Int8:   lb = -int64(0x80); ub = 0x7f; return true;
    case Type::Int16:  lb = -int64(0x8000); ub = 0x7fff; return true;
    case Type::Int32:  lb = -int64(0x80000000); ub = 0x7fffffff; return true;
    case Type::Int64:  
        lb = __INT64_C(0x8000000000000000);
        ub = __INT64_C(0x7fffffffffffffff); return true;
    case Type::UInt8:  lb = 0; ub = 0x100; return true;
    case Type::UInt16: lb = 0; ub = 0x10000; return true;
    case Type::UInt32: lb = 0; ub = __INT64_C(0x100000000); return true;
    default:
        return false;
    }
}

bool
Abcd::isCheckableType(Type::Tag typetag) {
    if (Type::isInteger(typetag) &&
        (typetag != Type::IntPtr) &&
        (typetag != Type::UIntPtr))
        return true;
    else
        return false;
}

bool
Abcd::hasCheckableType(const Opnd *opnd) {
    Type::Tag typetag = opnd->getType()->tag;
    return Abcd::isCheckableType(typetag);
}

// return an opnd to be used just before useSite, possibly building
// a tauAnd instruction to use.
SsaTmpOpnd *Abcd::getReasonTau(AbcdReasons *reason,
                               Inst *useSite)
{
    InstFactory &instFactory = irManager.getInstFactory();
    OpndManager &opndManager = irManager.getOpndManager();
    TypeManager &typeManager = irManager.getTypeManager();

    uint32 numReasons = (uint32) reason->facts.size();
    assert(numReasons < 100);
    if (numReasons == 1) {
        SsaTmpOpnd *reasonOpnd = *(reason->facts.begin());
        return reasonOpnd;
    } else if (numReasons == 0) {
        SsaTmpOpnd *reasonOpnd = getTauSafe();
        return reasonOpnd;
    } else {
        // need to build a tauAnd
        Opnd **newAndOpnds = new (mm) Opnd*[numReasons];
        StlSet<SsaTmpOpnd*>::iterator iter = reason->facts.begin();
        for (uint32 i = 0; i < numReasons;  ++i, ++iter) {
            newAndOpnds[i] = *iter;
        }
        SsaTmpOpnd *tauDst = opndManager.createSsaTmpOpnd(typeManager.getTauType());
        Inst *tauAndInst = instFactory.makeTauAnd(tauDst, numReasons, newAndOpnds);

        if (Log::isEnabled()) {
            Log::out() << "Inserting tauAndInst=(";
            tauAndInst->print(Log::out());
            Log::out() << ") before useSite= ";
            useSite->print(Log::out());
            Log::out() << ")" << std::endl;
        }

        tauAndInst->insertBefore(useSite);
        return tauDst;
    }
}

// phi reasons together
// derefVar definition site is a Phi which should be used for placement
SsaTmpOpnd *Abcd::makeReasonPhi(Opnd *derefVar, StlVector<AbcdReasons *> &reasons,
                                StlVector<Opnd *> &derefVarVersions)
{
    InstFactory &instFactory = irManager.getInstFactory();
    OpndManager &opndManager = irManager.getOpndManager();
    TypeManager &typeManager = irManager.getTypeManager();

    uint32 n = (uint32) reasons.size();
    assert(derefVarVersions.size() == n);
    Opnd **newPhiOpnds = new (mm) Opnd*[n];

    Type *tauType = typeManager.getTauType();
    VarOpnd *tauBaseVar = opndManager.createVarOpnd(tauType, false);

    SsaVarOpnd *tauPhiDstOpnd = opndManager.createSsaVarOpnd(tauBaseVar);
    SsaTmpOpnd *tauResOpnd = opndManager.createSsaTmpOpnd(typeManager.getTauType());

    for (uint32 i=0; i<n; ++i) {
        AbcdReasons *reason = reasons[i];
        Opnd *derefVarVersion = derefVarVersions[i];
        Inst *derefVarInst = derefVarVersion->getInst();

        Inst *stVarLoc = derefVarInst; // inst to insert stvar before
        if (stVarLoc->getOpcode() != Op_StVar) {
            assert(stVarLoc->getOpcode() == Op_Phi);
            // make sure we place stvar in a legal location
            Inst *nextInst = stVarLoc->getNextInst();
            while ((nextInst->getOpcode() == Op_Phi) && (nextInst->getOpcode() == Op_TauPi)) {
                nextInst = nextInst->getNextInst();
            }
            stVarLoc = nextInst;
        }
        
        newPhiOpnds[i] = opndManager.createSsaVarOpnd(tauBaseVar);
        SsaTmpOpnd *tauToStVar = getReasonTau(reason, stVarLoc);
        SsaVarOpnd *varToStTo = newPhiOpnds[i]->asSsaVarOpnd();
        assert(varToStTo);
        Inst *tauStVarInst = instFactory.makeStVar(varToStTo, tauToStVar);

        if (Log::isEnabled()) {
            Log::out() << "Inserting tauStVarInst=(";
            tauStVarInst->print(Log::out());
            Log::out() << ") after stVarLoc= ";
            stVarLoc->print(Log::out());
            Log::out() << ")" << std::endl;
        }
        tauStVarInst->insertAfter(stVarLoc);
    }

    Inst *tauPhiInst = instFactory.makePhi(tauPhiDstOpnd, n, newPhiOpnds);
    Inst *derefVarInst = derefVar->getInst();
    assert(derefVarInst->getOpcode() == Op_Phi);

    if (Log::isEnabled()) {
        Log::out() << "Inserting tauPhiInst=(";
        tauPhiInst->print(Log::out());
        Log::out() << ") before derefVarInst= ";
        derefVarInst->print(Log::out());
        Log::out() << ")" << std::endl;
    }

    tauPhiInst->insertBefore(derefVarInst);
    Inst *tauLdVarInst = instFactory.makeLdVar(tauResOpnd, tauPhiDstOpnd);
    Inst *ldVarLoc = tauPhiInst->getNextInst();
    while (ldVarLoc!=NULL && (ldVarLoc->getOpcode() == Op_Phi) || (ldVarLoc->getOpcode() == Op_TauPi)) {
        ldVarLoc = ldVarLoc->getNextInst();
    }
    if (Log::isEnabled()) {
        Log::out() << "Inserting tauLdVarInst=(";
        tauLdVarInst->print(Log::out());
        Log::out() << ") before ldVarLoc= ";
        ldVarLoc->print(Log::out());
        Log::out() << ")" << std::endl;
    }
    tauLdVarInst->insertBefore(ldVarLoc);
    return tauResOpnd;
}

} //namespace Jitrino 
