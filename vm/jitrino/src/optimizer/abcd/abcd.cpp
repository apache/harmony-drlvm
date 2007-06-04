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
DEFINE_SESSION_ACTION(ABCDPass, abcd, "ABCD: eliminating Array Bounds Check on Demand");

void
ABCDPass::_run(IRManager &irm) {
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

Abcd::Abcd(IRManager &irManager0,  MemoryManager& memManager, DominatorTree& dom0) : 
    irManager(irManager0), 
    mm(memManager),
    dominators(dom0),
    solver(0),
    canEliminate(memManager),
    canEliminateUB(memManager),
    canEliminateLB(memManager),
    tauUnsafe(0),
    tauSafe(0),
    flags(*irManager.getOptimizerFlags().abcdFlags),
    insertPi(memManager, dom0, irManager0, flags.useAliases),
    blockTauPoint(0),
    lastTauPointBlock(0)
{}

void Abcd::runPass()
{
    if ( Log::isEnabled() ) {
        Log::out() << "IR before ABCD pass" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
        FlowGraph::printDotFile(irManager.getFlowGraph(), irManager.getMethodDesc(), "beforeabcd");
        dominators.printDotFile(irManager.getMethodDesc(), "beforeabcd.dom");
    }

    insertPi.insertPi();

    removeRedundantBoundsChecks();
    if ( Log::isEnabled() ) {
        Log::out() << "IR after removeRedundantBoundsChecks" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
    }

    removePiEliminateChecks();
    if ( Log::isEnabled() ) {
        Log::out() << "IR after ABCD pass" << std::endl;
        FlowGraph::printHIR(Log::out(), irManager.getFlowGraph(), irManager.getMethodDesc());
    }
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

// return an opnd to be used just before useSite, possibly building
// a tauAnd instruction to use.
SsaTmpOpnd* Abcd::getReasonTau(AbcdReasons *reason, Inst *useSite)
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

// a ScopedDomNodeInstWalker, forward/preorder
class RemovePiEliminateChecksWalker {
public:
    RemovePiEliminateChecksWalker(Abcd* abcd) : _abcd(abcd), block(0) // forward
    {}

    void startNode(DominatorNode *domNode) { block = domNode->getNode(); };
    void applyToInst(Inst *i) { _abcd->removePiEliminateChecksOnInst(block, i); };
    void finishNode(DominatorNode *domNode) {}

    void enterScope() {}
    void exitScope() {}
private:
    Abcd* _abcd;
    Node* block;
};
//------------------------------------------------------------------------------

void Abcd::removePiEliminateChecks()
{
    RemovePiEliminateChecksWalker removePiWalker(this);
    typedef ScopedDomNodeInst2DomWalker<true, RemovePiEliminateChecksWalker> 
        RemovePiDomWalker;
    RemovePiDomWalker removePiDomWalker(removePiWalker);

    DomTreeWalk<true, RemovePiDomWalker>(dominators, removePiDomWalker, mm);
}

void Abcd::removePiEliminateChecksOnInst(Node *block, Inst *inst)
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
            }
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

SsaTmpOpnd* Abcd::getBlockTauPoint(Node *block) 
{
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

} //namespace Jitrino 
