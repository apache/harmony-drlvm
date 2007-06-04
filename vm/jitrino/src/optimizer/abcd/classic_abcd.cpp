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

#include "irmanager.h"
#include "Dominator.h"
#include "classic_abcd.h"
#include "classic_abcd_solver.h"
#include "opndmap.h"

#include "Stl.h"
#include "Log.h"
#include "open/types.h"
#include "Inst.h"
#include "walkers.h"
#include "PMFAction.h"
#include "constantfolder.h"

#include <assert.h>
#include <iostream>
#include <algorithm>

namespace Jitrino {

// eliminating Array Bounds Check on Demand
DEFINE_SESSION_ACTION(CLASSIC_ABCDPass, classic_abcd, "Classic ABCD: eliminating Array Bounds Check on Demand");

void
CLASSIC_ABCDPass::_run(IRManager &irm) {
    OptPass::splitCriticalEdges(irm);
    OptPass::computeDominators(irm);
    ClassicAbcd classic_abcd(this, irm, irm.getNestedMemoryManager(), 
                             *irm.getDominatorTree());
    classic_abcd.runPass();
}
//------------------------------------------------------------------------------

class IOpndProxy : public IOpnd
{
public:
    IOpndProxy(Opnd* opnd);

    IOpndProxy(int32 c, uint32 id);

    virtual void printName(std::ostream& os) const
    {
        if ( _opnd ) {
            _opnd->print(os);
        }else{
            os << "_c" << getID() << "(const=" << getConstant() << ")";
        }
    }

    Opnd* getOrg() const { assert(_opnd); return _opnd; }

    static uint32 getProxyIdByOpnd(Opnd* opnd);
private:
    Opnd* _opnd;

    /* ids of PiOpnd, SsaOpnd, VarOpnd may alias their IDs,
     * encoding all in one ID with unaliasing
     */
    static const uint64 min_var_opnd = 0;
    static const uint64 min_ssa_opnd = MAX_UINT32 / 4;
    static const uint64 min_pi_opnd = (min_ssa_opnd) * 2;
    static const uint64 min_const_opnd = (min_ssa_opnd) * 3;
};
//------------------------------------------------------------------------------

bool inInt32(int64 c) {
    return (int64)(int32)c == c;
}

bool inInt32Type(Type t) {
    return (t.tag == Type::Int8) || 
         (t.tag == Type::Int16) || 
         (t.tag == Type::Int32);
}

IOpndProxy::IOpndProxy(Opnd* opnd) : 
    IOpnd(0/* id */, 
          opnd->getInst()->isPhi() /* is_phi */, 
          ConstantFolder::isConstant(opnd) /* is_constant */),
    _opnd(opnd)
{
    setID(getProxyIdByOpnd(_opnd));
    if ( isConstant() ) {
        ConstInst* c_inst = _opnd->getInst()->asConstInst();
        assert(c_inst);
        int64 value = c_inst->getValue().i8;
        if ( inInt32Type(c_inst->getType()) ) {
            value = c_inst->getValue().i4;
        }else if ( c_inst->getType() != Type::Int64 ) {
            setUnconstrained(true);
            return;
        }
        if ( inInt32(value) ) {
            setConstant((int32)value);
        }else{
            setUnconstrained(true);
        }
    }
}
//------------------------------------------------------------------------------

IOpndProxy::IOpndProxy(int32 c, uint32 id) : 
    IOpnd(0, false /* is_phi */, true /* is_constant */),
    _opnd(NULL)
{
    setID(min_const_opnd + id);
    setConstant(c);
}

uint32 IOpndProxy::getProxyIdByOpnd(Opnd* opnd)
{
    uint32 id = opnd->getId();
    if ( opnd->isVarOpnd() ) {
        id += min_var_opnd;
    }else if ( opnd->isPiOpnd() ) { 
        // note: PiOpnd inherits from SsaOpnd, check PiOpnd first
        id += min_pi_opnd;
    }else if ( opnd->isSsaOpnd() ) {
        id += min_ssa_opnd;
    }else {
        assert(0);
    }
    return id;
}
//------------------------------------------------------------------------------

class BuildInequalityGraphWalker {
public:
    BuildInequalityGraphWalker(InequalityGraph* igraph, bool isLower) :
       _igraph(igraph), _isLower(isLower), _const_id_counter(1 /*reserve 0 for solver*/)
    {}

    void startNode(DominatorNode *domNode) {}
    void applyToInst(Inst* i);
    void finishNode(DominatorNode *domNode) {}

    void enterScope() {}
    void exitScope() {}
private:
    void updateDstForInst(Inst* inst);

    // returns true if an edge to const opnd is actually added
    bool addEdgeIfConstOpnd(IOpndProxy* dst, Opnd* const_src, Opnd* src, 
                            bool negate_src);

    void addAllSrcOpndsForPhi(Inst* inst);

    // returns true if the edge is actually added
    bool addDistance(IOpndProxy* dst, IOpndProxy* src, int64 constant, 
                     bool negate);

    // same as addDistance, but swap 'from' and 'to' if 'negate'
    void addDistanceSwapNegate(IOpndProxy* to, IOpndProxy* from, int64 c, 
                               bool negate);

    // add edges to (or from) 'dst' induced by given bounds
    void addPiEdgesForBounds(IOpndProxy* dst, 
                             const PiBound& lb, const PiBound& ub);

    void addPiEdgesWithOneBoundInf
         (IOpndProxy* dst, bool lb_is_inf, const PiBound& non_inf_bound);

    IOpndProxy* findProxy(Opnd* opnd);

    IOpndProxy* addOldOrCreateOpnd(Opnd* opnd);

    InequalityGraph* _igraph;
    bool _isLower;
    uint32 _const_id_counter;
};
//------------------------------------------------------------------------------
 
IOpndProxy* BuildInequalityGraphWalker::findProxy(Opnd* opnd)
{
    assert(_igraph);
    return (IOpndProxy*) _igraph->findOpnd(IOpndProxy::getProxyIdByOpnd(opnd));
}
//------------------------------------------------------------------------------

void BuildInequalityGraphWalker::addAllSrcOpndsForPhi(Inst* inst)
{
    assert(inst->getOpcode() == Op_Phi);
    for (uint32 j = 0; j < inst->getNumSrcOperands(); j++) {
        IOpndProxy* proxy_src = addOldOrCreateOpnd(inst->getSrc(j));
        addDistance(findProxy(inst->getDst()), proxy_src, 0, false /*negate*/);
    }
}
//------------------------------------------------------------------------------

void BuildInequalityGraphWalker::applyToInst(Inst* inst)
{
    assert(inst);

    Type::Tag inst_type = inst->getType();
    if ( !Type::isInteger(inst_type) && inst_type != Type::Boolean &&
         inst_type != Type::Char ) {
        // note: some operations of unsupported type can produce operands of
        // supported (int) types, for example,
        // inst-compare-two-unmanaged-pointers, we need these operands as
        // unconstrained in the graph
        Opnd* dst = inst->getDst();
        if ( dst && !dst->isNull() &&
                (dst->getType()->isInteger() ||
                 dst->getType()->isBoolean() ) ) {
            addOldOrCreateOpnd(dst)->setUnconstrained(true);
        }
        return;
    }
    if ( inst->isUnconditionalBranch() || inst->isConditionalBranch() || 
         inst->isReturn() ) {
        return;
    }
    IOpndProxy* proxy_dst;
    Opcode opc = inst->getOpcode();
    switch ( opc ) {
        case Op_Phi:
        {
            proxy_dst = addOldOrCreateOpnd(inst->getDst());
            addAllSrcOpndsForPhi(inst);
        }
            break;
        case Op_Copy:
        case Op_LdVar:
        case Op_StVar:
        {
            proxy_dst = addOldOrCreateOpnd(inst->getDst());
            addDistance(proxy_dst, findProxy(inst->getSrc(0)), 0, 
                        false /* negate */);
        }
            break;
        case Op_Add:
        {
            proxy_dst = addOldOrCreateOpnd(inst->getDst());
            Opnd* src0 = inst->getSrc(0);
            Opnd* src1 = inst->getSrc(1);
            addEdgeIfConstOpnd(proxy_dst, src0, src1, false /* negate */) 
            || addEdgeIfConstOpnd(proxy_dst, src1, src0, false /* negate */);
        } 
            break;
        case Op_Sub:
        {
            proxy_dst = addOldOrCreateOpnd(inst->getDst());
            addEdgeIfConstOpnd(proxy_dst, inst->getSrc(1), inst->getSrc(0),
                               true /* negate */ );
        } 
            break;
        case Op_TauPi:
        {
            proxy_dst = addOldOrCreateOpnd(inst->getDst());
            IOpndProxy* src0 = findProxy(inst->getSrc(0));
            addDistance(proxy_dst, src0, 0, false /* negate */);
            const PiCondition* condition = inst->asTauPiInst()->getCond();
            addPiEdgesForBounds(proxy_dst, 
                                condition->getLb(), 
                                condition->getUb());
        }
            break;
        case Op_TauArrayLen:
        case Op_LdConstant:
            addOldOrCreateOpnd(inst->getDst());
            break;
        case Op_TauStInd: case Op_TauStElem: case Op_TauStField: 
        case Op_TauStRef: case Op_TauStStatic:
            break;
        default:
            addOldOrCreateOpnd(inst->getDst())->setUnconstrained(true);
            break;
    }
}
//------------------------------------------------------------------------------

// returns true if the edge is actually added
bool BuildInequalityGraphWalker::addDistance
     (IOpndProxy* dst, IOpndProxy* src, int64 constant, bool negate)
{
    assert(dst && src);
    // Note: is this an optimization?  It prevents adding a link from
    // unconstrained operands.  This is always safe, and it shouldn't lose
    // opportunity, but maybe we should discuss it to be sure?
    if ( !src->isUnconstrained() ) {
        if ( !inInt32(constant) ) {
            return false;
        }
        if ( negate ) {
            constant = (-1) * constant;
        }
        _igraph->addEdge(src->getID(), dst->getID(), constant);
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------

void BuildInequalityGraphWalker::addDistanceSwapNegate
     (IOpndProxy* to, IOpndProxy* from, int64 c, bool negate)
{
    addDistance(!negate ? to : from, !negate ? from : to, c, negate);
}
//------------------------------------------------------------------------------

// returns true if an edge to const opnd is actually added
bool BuildInequalityGraphWalker::addEdgeIfConstOpnd
    (IOpndProxy* dst, Opnd* const_src, Opnd* src, bool negate_src)
{
    if ( ConstantFolder::isConstant(const_src) ) {
        IOpnd* from = findProxy(const_src);
        assert(from);
        if ( !from->isUnconstrained() ) {
            return addDistance(dst, findProxy(src), from->getConstant(), 
                               negate_src);
        }
    }
    return false;
}
//------------------------------------------------------------------------------

/*
 * pi (src0 \in [undef,A + c] -) dst
 *      dst <= A + c <-> (dst - A) <= c
 *      edge(from:A, to:dst, c)
 *
 * pi (src0 \in [A + c,undef] -) dst
 *      (A + c) <= dst <-> (A - dst) <= -c
 *      edge(from:dst, to:A, -c)
 */
void BuildInequalityGraphWalker::addPiEdgesForBounds
     (IOpndProxy* dst, const PiBound& lb, const PiBound& ub)
{
    if ( _isLower && !lb.isUndefined() ) {
        addPiEdgesWithOneBoundInf(dst, false, lb);
    }
    else if ( !_isLower && !ub.isUndefined() ) {
        addPiEdgesWithOneBoundInf(dst, true, ub);
    }
}
//------------------------------------------------------------------------------

void BuildInequalityGraphWalker::addPiEdgesWithOneBoundInf
     (IOpndProxy* dst, bool lb_is_inf, const PiBound& non_inf_bound)
{
    if ( non_inf_bound.isVarPlusConst()  ) {
        Opnd* var = non_inf_bound.getVar().the_var;
        addDistanceSwapNegate(dst /* to */, 
                              findProxy(var) /* from */,
                              non_inf_bound.getConst(), 
                              false /* negate */);
    } else if ( non_inf_bound.isConst() ) {
        MemoryManager& mm = _igraph->getMemoryManager();
        IOpndProxy* c_opnd = new (mm) 
            IOpndProxy(non_inf_bound.getConst(), _const_id_counter++);
        _igraph->addOpnd(c_opnd);
        addDistanceSwapNegate(c_opnd /* to */, dst, 0, false /* negate */);
    }
}
//------------------------------------------------------------------------------

IOpndProxy* BuildInequalityGraphWalker::addOldOrCreateOpnd(Opnd* opnd)
{
    IOpndProxy* proxy = findProxy(opnd);
    if ( !proxy ) {
        MemoryManager& mm = _igraph->getMemoryManager();
        proxy = new (mm) IOpndProxy(opnd);
        _igraph->addOpnd(proxy);
        if ( Log::isEnabled() ) {
            Log::out() << "added opnd: ";
            proxy->printFullName(Log::out());
            Log::out() << std::endl;
        }
    }
    return proxy;
}

class InequalityGraphPrinter : public PrintDotFile {
public:
    InequalityGraphPrinter(InequalityGraph& graph) : _graph(graph) {}
    void printDotBody()
    {
        _graph.printDotBody(*os);
    }
private:
    InequalityGraph& _graph;
};
//------------------------------------------------------------------------------

void ClassicAbcd::runPass()
{
    static bool run_once = true;
    if ( run_once && _runTests ) {
        classic_abcd_test_main();
        _runTests = false;
        run_once = false;
    }

    MethodDesc& method_desc  = _irManager.getMethodDesc();
    ControlFlowGraph& cfg    = _irManager.getFlowGraph();
    TypeManager& typeManager = _irManager.getTypeManager();
    OpndManager& opndManager = _irManager.getOpndManager();
    InstFactory& instFactory = _irManager.getInstFactory();

    if ( Log::isEnabled() ) {
        FlowGraph::printDotFile(cfg, method_desc, "before_classic_abcd");
        _domTree.printDotFile(method_desc, "before_classic_abcd.dom");
        Log::out() << "ClassicAbcd pass started" << std::endl;
    }

    StlMap<Inst *, uint32> redundantChecks(_mm);

    {
        MemoryManager ineq_mm("ClassicAbcd::InequalityGraph");

        InsertPi insertPi(ineq_mm, _domTree, _irManager, _useAliases, InsertPi::Upper);
        insertPi.insertPi();

        InequalityGraph igraph(ineq_mm);

        BuildInequalityGraphWalker igraph_walker(&igraph, false /*lower*/);
        typedef ScopedDomNodeInst2DomWalker<true, BuildInequalityGraphWalker>
            IneqBuildDomWalker;
        IneqBuildDomWalker dom_walker(igraph_walker);
        DomTreeWalk<true, IneqBuildDomWalker>(_domTree, dom_walker, ineq_mm);

        if ( Log::isEnabled() ) {
            InequalityGraphPrinter printer(igraph);
            printer.printDotFile(method_desc, "inequality.graph");
        }

        ClassicAbcdSolver solver(igraph, ineq_mm);

        for (Nodes::const_iterator i = cfg.getNodes().begin(); i != cfg.getNodes().end(); ++i) {
            Node *curr_node = *i;

            for (Inst *curr_inst = (Inst*)curr_node->getFirstInst();
                 curr_inst != NULL; curr_inst = curr_inst->getNextInst()) {

                if (curr_inst->getOpcode() == Op_TauCheckBounds) {
                    assert(curr_inst->getNumSrcOperands() == 2);
                    Opnd *idxOp = curr_inst->getSrc(1);
                    Opnd *boundsOp = curr_inst->getSrc(0);

                    if (Log::isEnabled()) {
                        Log::out() << "Trying to eliminate CheckBounds instruction ";
                        curr_inst->print(Log::out());
                        Log::out() << std::endl;
                    }

                    IOpnd *idxIOp = igraph.findOpnd(IOpndProxy::getProxyIdByOpnd(idxOp));
                    IOpnd *boundsIOp = igraph.findOpnd(IOpndProxy::getProxyIdByOpnd(boundsOp));

                    bool upper_res = solver.demandProve(boundsIOp, idxIOp, -1, true /*upper*/);
                    if (upper_res) {
                        redundantChecks[curr_inst] = 0x1 /*upper redundant*/;
                        if (Log::isEnabled()) {
                            Log::out() << "can eliminate upper bound check!\n";
                        }
                    }
                }
            }
        }
        insertPi.removePi();
    }


    {
        MemoryManager ineq_mm("ClassicAbcd::InequalityGraph");

        InsertPi insertPi(ineq_mm, _domTree, _irManager, _useAliases, InsertPi::Lower);
        insertPi.insertPi();

        InequalityGraph igraph(ineq_mm);

        BuildInequalityGraphWalker igraph_walker(&igraph, true /*lower*/);
        typedef ScopedDomNodeInst2DomWalker<true, BuildInequalityGraphWalker>
            IneqBuildDomWalker;
        IneqBuildDomWalker dom_walker(igraph_walker);
        DomTreeWalk<true, IneqBuildDomWalker>(_domTree, dom_walker, ineq_mm);

        IOpndProxy *zeroIOp = new (ineq_mm) IOpndProxy(0, 0 /*using reserved ID*/);
        igraph.addOpnd(zeroIOp);
        if ( Log::isEnabled() ) {
            Log::out() << "added zero opnd for solving lower bound problem: ";
            zeroIOp->printFullName(Log::out());
            Log::out() << std::endl;
        }

        if ( Log::isEnabled() ) {
            InequalityGraphPrinter printer(igraph);
            printer.printDotFile(method_desc, "inequality.graph.inverted");
        }

        ClassicAbcdSolver solver(igraph, ineq_mm);

        for (Nodes::const_iterator i = cfg.getNodes().begin(); i != cfg.getNodes().end(); ++i) {
            Node *curr_node = *i;

            for (Inst *curr_inst = (Inst*)curr_node->getFirstInst();
                 curr_inst != NULL; curr_inst = curr_inst->getNextInst()) {

                if (curr_inst->getOpcode() == Op_TauCheckBounds) {
                    assert(curr_inst->getNumSrcOperands() == 2);
                    Opnd *idxOp = curr_inst->getSrc(1);

                    if (Log::isEnabled()) {
                        Log::out() << "Trying to eliminate CheckBounds instruction ";
                        curr_inst->print(Log::out());
                        Log::out() << std::endl;
                    }

                    IOpnd *idxIOp = igraph.findOpnd(IOpndProxy::getProxyIdByOpnd(idxOp));

                    bool lower_res = solver.demandProve(zeroIOp, idxIOp, 0, false /*lower*/);
                    if (lower_res) {
                        redundantChecks[curr_inst] |= 0x2 /*lower redundant*/;
                        if (Log::isEnabled()) {
                            Log::out() << "can eliminate lower bound check!\n";
                        }
                    }
                }
            }
        }
        insertPi.removePi();
    }

    for(StlMap<Inst *, uint32>::const_iterator i = redundantChecks.begin();
        i != redundantChecks.end(); ++i) {
        Inst *redundant_inst = i->first;
        bool fully_redundant = i->second == 0x3;

        if (fully_redundant) {
            // should we check if another tau has already been placed in
            // this block, and if so reuse it?  Also, should we be using
            // taupoint or tauedge?
            Opnd *tauOp = opndManager.createSsaTmpOpnd(typeManager.getTauType());
            Inst* tau_point = instFactory.makeTauPoint(tauOp);
            tau_point->insertBefore(redundant_inst);
      
            if (Log::isEnabled()) {
                Log::out() << "Inserted taupoint inst ";
                tau_point->print(Log::out());
                Log::out() << " before inst ";
                redundant_inst->print(Log::out());
                Log::out() << std::endl;
            }

            Opnd* dstOp = redundant_inst->getDst();
            redundant_inst->setDst(OpndManager::getNullOpnd());
            Inst* copy = instFactory.makeCopy(dstOp, tauOp);
            copy->insertBefore(redundant_inst);
            FlowGraph::eliminateCheck(cfg, redundant_inst->getNode(), redundant_inst, false);
            
            if (Log::isEnabled()) {
                Log::out() << "Replaced bound check with inst ";
                copy->print(Log::out());
                Log::out() << std::endl;
            }
        }
    }

    Log::out() << "ClassicAbcd pass finished" << std::endl;
}
//------------------------------------------------------------------------------

} //namespace Jitrino 

