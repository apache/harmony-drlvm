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

#include <fstream>
#include "classic_abcd_solver.h"
#include "Log.h"

namespace Jitrino {

void IOpnd::printName(std::ostream& os) const
{
    os << "o" << getID();
}

void IOpnd::printFullName(std::ostream& os) const
{
    printName(os);
    assert((!isPhi()) || (!isConstant()));
    if ( isPhi() ) {
        os << "(phi)";
    }
    if ( isConstant() ) {
        os << "(const=" << getConstant() << ")";
    }
}

//------------------------------------------------------------------------------

bool InequalityGraph::has_other_opnd_with_same_id(IdToOpndMap& map, IOpnd* opnd)
{
    IdToOpndMap::iterator it = map.find(opnd->getID());
    if ( it != map.end() && it->second != opnd ) {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------

const InequalityGraph::EdgeList& InequalityGraph::getInEdges(IOpnd* opnd) const
{ 
    OpndEdgeMap::const_iterator it = _opnd_to_inedges_map.find(opnd->getID());

    if ( it == _opnd_to_inedges_map.end() ) {
        return _emptyList;
    }
    return it->second;
}

const InequalityGraph::EdgeList& InequalityGraph::getOutEdges(IOpnd* opnd) const
{ 
    OpndEdgeMap::const_iterator it = _opnd_to_outedges_map.find(opnd->getID());
    if ( it == _opnd_to_outedges_map.end() ) {
        return _emptyList;
    }
    return it->second;
}

void InequalityGraph::addEdgeToIdMap
     (OpndEdgeMap& mp, uint32 id, IneqEdge* edge)
{
    OpndEdgeMap::iterator it = mp.find(id);
    if ( it == mp.end() ) {
        StlList<IneqEdge*> * new_list = new (_mem_mgr) StlList<IneqEdge*>(_mem_mgr);
        new_list->push_back(edge);
        mp.insert(std::make_pair(id, *new_list));
    }else{
        it->second.push_back(edge);
    }
}

void InequalityGraph::addEdge(IOpnd* from, IOpnd* to, int32 distance)
{
    assert(!has_other_opnd_with_same_id(_id_to_opnd_map, from));
    assert(!has_other_opnd_with_same_id(_id_to_opnd_map, to));

    _id_to_opnd_map[from->getID()] = from;
    _id_to_opnd_map[to->getID()] = to;

    IneqEdge* p_edge = new (_mem_mgr) IneqEdge(from, to, distance);
    _edges.push_back(p_edge);

    addEdgeToIdMap(_opnd_to_outedges_map, from->getID(), p_edge);
    addEdgeToIdMap(_opnd_to_inedges_map, to->getID(), p_edge);
}

void InequalityGraph::addEdge(uint32 id_from, uint32 id_to, int32 distance)
{
    IOpnd *from, *to;
    IdToOpndMap::iterator it;

    it = _id_to_opnd_map.find(id_from);
    assert(it != _id_to_opnd_map.end());
    from = it->second;
    it = _id_to_opnd_map.find(id_to);
    assert(it != _id_to_opnd_map.end());
    to = it->second;

    addEdge(from, to, distance);
}

void InequalityGraph::addOpnd(IOpnd* opnd)
{
    assert(!has_other_opnd_with_same_id(_id_to_opnd_map, opnd));
    _id_to_opnd_map[opnd->getID()] = opnd;
}

void InequalityGraph::printDotHeader(std::ostream& os) const
{
    os << "digraph dotgraph {" << std::endl;
    os << "node [shape=record,fontname=\"Courier\",fontsize=9];" << std::endl;
    os << "label=\"Inequality Graph\";" << std::endl;
}

/*
 * example:
 *
 * digraph dotgraph {
 * node [shape=record,fontname="Courier",fontsize=9];
 * label="Label";
 * ENTRY [label="{ENTRY}"]
 * L1 [label="{L1}"]
 * L2 [label="{L2}"]
 * ENTRY -> L1 [label="55"];
 * L1 -> L2;
 * L2 -> ENTRY;
 * }
 */
void InequalityGraph::printDotFile(std::ostream& os) const
{
    printDotHeader(os);
    printDotBody(os);
    printDotEnd(os);
}

void InequalityGraph::printDotEnd(std::ostream& os) const
{
    os << "}" << std::endl;
}

void InequalityGraph::printDotBody(std::ostream& os) const
{
    IdToOpndMap::const_iterator it = _id_to_opnd_map.begin(), 
        last = _id_to_opnd_map.end();
    for (; it != last; it++ ) {
        IOpnd* opnd = it->second;
        os << "\""; opnd->printName(os); os << "\"";
        os << " [label=\"{"; 
        opnd->printFullName(os);
        os << "}\"];" << std::endl;
    }
    for (it = _id_to_opnd_map.begin(); it != last; it++ ) {
        IOpnd* from_opnd = it->second;
        const EdgeList& out_edges_list = getOutEdges(from_opnd);
        EdgeList::const_iterator out_iter = out_edges_list.begin(), 
            out_last = out_edges_list.end();
        for (; out_iter != out_last; out_iter++) {
            IOpnd* to_opnd = (*out_iter)->getDst();
            os << "\""; from_opnd->printName(os); os << "\"";
            os << " -> ";
            os << "\""; to_opnd->printName(os); os << "\"";
            os << " [label=\"" << (*out_iter)->getLength() << "\"];" 
               << std::endl;
        }
    }
}

IOpnd* InequalityGraph::findOpnd(uint32 id) const
{
    IdToOpndMap::const_iterator it = _id_to_opnd_map.find(id);
    if ( it == _id_to_opnd_map.end() ) {
        return NULL;
    }
    return it->second;
}
//------------------------------------------------------------------------------

TrueReducedFalseChart* BoundAllocator::create_empty_TRFChart()
{
    return new (_mem_mgr) TrueReducedFalseChart(this);
}

Bound* BoundAllocator::newBound(int32 val, const BoundState& bs)
{
    return new (_mem_mgr) Bound(val, bs);
}

Bound* BoundAllocator::create_inc1(Bound* bound)
{
    return newBound(bound->isUpper() ? bound->_bound + 1 : bound->_bound - 1, 
                    bound->getBoundState());
}

Bound* BoundAllocator::create_dec1(Bound* bound)
{
    return newBound(bound->isUpper() ? bound->_bound - 1 :  bound->_bound + 1, 
                    bound->getBoundState());
}

Bound* BoundAllocator::create_dec_const(Bound* bound, int32 cnst)
{
    return newBound(bound->_bound - cnst, bound->getBoundState());
}

//------------------------------------------------------------------------------

void Bound::printFullName(std::ostream& os) 
{ 
    os << "Bound(";
    if ( isUpper() ) {
        os << "upper";
    }else{
        os << "lower";
    }
    os << ", " << _bound << ")"; 
}

bool Bound::leq(Bound* bound1, Bound* bound2) 
{ 
    assert(bound1 && bound2);
    assert(bound1->isUpper() == bound2->isUpper());
    return Bound::int32_leq(bound1->_bound, bound2);
}

bool Bound::eq(Bound* bound1, Bound* bound2)
{
    assert(!bound1 || !bound2 || bound1->isUpper() == bound2->isUpper());
    return bound1 && bound2 && 
        Bound::leq(bound1, bound2) && Bound::leq(bound2, bound1);
}

bool Bound::leq_int32(Bound* bound1, int32 value) 
{ 
    assert(bound1);
    return bound1->isUpper() ? 
        bound1->_bound <= value : 
        bound1->_bound >= value;
}

bool Bound::int32_leq(int32 value, Bound* bound1) 
{ 
    assert(bound1);
    return bound1->isUpper() ? 
        value <= bound1->_bound :
        value >= bound1->_bound;
}

// returns (dst_val - src_val <= bound)
bool Bound::const_distance_leq(int32 src_val, int32 dst_val, Bound* bound)
{
    assert(bound);
    int32 distance = dst_val - src_val;
    assert(distance + src_val == dst_val);
    return Bound::int32_leq(distance, bound);
}

//------------------------------------------------------------------------------

ProveResult meetBest(ProveResult res1, ProveResult res2)
{
    return (ProveResult) std::max(res1, res2);
}

ProveResult meetWorst(ProveResult res1, ProveResult res2)
{
    return (ProveResult) std::min(res1, res2);
}

void print_result(ProveResult r, std::ostream& os)
{
    switch (r) {
        case True : os << "True"; break;
        case Reduced : os << "Reduced"; break;
        case False : os << "False"; break;
    }
}

//------------------------------------------------------------------------------

void TrueReducedFalseChart::addFalse(Bound* f_bound)
{
    if ( !_max_false || Bound::leq(_max_false, f_bound) ) {
        _max_false = f_bound;
    }

    /* make none of 3 bounds equal */
    if ( Bound::eq(_max_false, _min_reduced) ) {
        _min_reduced = _bound_alloc->create_inc1(_min_reduced);
    }
    if ( Bound::eq(_max_false, _min_true) ) {
        _min_true = _bound_alloc->create_inc1(_min_true);
    }
    if ( Bound::eq(_min_reduced, _min_true) ) {
        _min_reduced = NULL;
    }

    clearRedundantReduced();
    assert(!_min_true || Bound::leq(_max_false, _min_true));
    assert(!_min_reduced || Bound::leq(_max_false, _min_reduced));
}

void TrueReducedFalseChart::addReduced(Bound* r_bound)
{
    if ( !_min_reduced || Bound::leq(r_bound, _min_reduced) ) {
        _min_reduced = r_bound;
    }

    /* make none of 3 bounds equal */
    if ( Bound::eq(_min_reduced, _min_true) ) {
        _min_true = _bound_alloc->create_inc1(_min_true);
    }
    if ( Bound::eq(_min_reduced, _max_false) ) {
        _max_false = _bound_alloc->create_dec1(_max_false);
    }

    assert(!_min_true || Bound::leq(_min_reduced, _min_true));
    assert(!_max_false || Bound::leq(_max_false, _min_reduced));
}

void TrueReducedFalseChart::addTrue(Bound* t_bound)
{
    if ( !_min_true || Bound::leq(t_bound, _min_true) ) {
        _min_true = t_bound;
    }

    /* make none of 3 bounds equal */
    if ( Bound::eq(_min_true, _min_reduced) ) {
        _min_reduced = _bound_alloc->create_dec1(_min_reduced);
    }
    if ( Bound::eq(_min_true, _max_false) ) {
        _max_false = _bound_alloc->create_dec1(_max_false);
    }
    if ( Bound::eq(_max_false, _min_reduced) ) {
        _min_reduced = NULL;
    }

    clearRedundantReduced();
    assert(!_min_reduced || Bound::leq(_min_reduced, _min_true));
    assert(!_max_false || !_min_reduced || 
           Bound::leq(_max_false, _min_reduced));
}

bool TrueReducedFalseChart::hasBoundResult(Bound* bound) const
{
    assert(!Bound::eq(_min_true, _max_false));
    assert(!Bound::eq(_min_true, _min_reduced));
    assert(!Bound::eq(_max_false, _min_reduced));

    if ( (_max_false && Bound::leq(bound, _max_false)) ||
         (_min_reduced && Bound::leq(_min_reduced, bound)) ||
         (_min_true && Bound::leq(_min_true, bound)) ) {
        return true;
    }
    return false;
}

ProveResult TrueReducedFalseChart::getBoundResult(Bound* bound) const
{
    assert(hasBoundResult(bound));
    if ( (_max_false && Bound::leq(bound, _max_false)) ) {
        return False;
    }
    if ( _min_true && Bound::leq(_min_true, bound) ) {
        return True;
    }
    if ( _min_reduced && Bound::leq(_min_reduced, bound) ) {
        return Reduced;
    }
    assert(0);
    return False;
}

void TrueReducedFalseChart::print(std::ostream& os) const
{
    os << "maxF=";
    printBound(_max_false, os);
    os << ", minR=";
    printBound(_min_reduced, os);
    os << ", minT=";
    printBound(_min_true, os);
}

void TrueReducedFalseChart::printBound(Bound* b, std::ostream& os) const
{
    if ( !b ) {
        os << "NULL";
    }else{
        b->printFullName(os);
    }
}

void TrueReducedFalseChart::clearRedundantReduced()
{
    if ( _min_true && _min_reduced && Bound::leq(_min_true, _min_reduced) &&
         !Bound::eq(_min_true, _min_reduced) ) {
        _min_reduced = NULL;
    }
    if ( _max_false && _min_reduced && Bound::leq(_min_reduced, _max_false) &&
         !Bound::eq(_min_reduced, _max_false) ) {
        _min_reduced = NULL;
    }
}

//------------------------------------------------------------------------------

void MemoizedDistances::makeEmpty()
{
    _map.clear();
}

void MemoizedDistances::initOpnd(IOpnd* op)
{
    OpndToTRFChart::const_iterator it = _map.find(op);
    if ( it == _map.end() ) {
        _map.insert(std::make_pair(op, *_bound_alloc.create_empty_TRFChart()));
    }
}

void MemoizedDistances::updateLeqBound(IOpnd* dest, Bound* bound, ProveResult res)
{
    initOpnd(dest);
    if ( res == False ) {
        _map[dest].addFalse(bound);
    }else if ( res == True ) {
        _map[dest].addTrue(bound);
    }else{
        _map[dest].addReduced(bound);
    }
}

bool MemoizedDistances::hasLeqBoundResult(IOpnd* dest, Bound* bound) const
{
    OpndToTRFChart::const_iterator it = _map.find(dest);
    if ( it == _map.end() ) {
        return false;
    }
    return (it->second).hasBoundResult(bound);
}

ProveResult MemoizedDistances::getLeqBoundResult(IOpnd* dest, Bound* bound) const
{
    assert(hasLeqBoundResult(dest, bound));
    return (_map.find(dest)->second).getBoundResult(bound);
}

// true iff: (there is a distance to 'dest') && (distance <= bound)
bool MemoizedDistances::minTrueDistanceLeqBound(IOpnd* dest, Bound* bound)
{
    initOpnd(dest);
    Bound* stored_bound = _map[dest].getMinTrueBound();
    return stored_bound && Bound::leq(stored_bound, bound);
}

bool MemoizedDistances::maxFalseDistanceGeqBound(IOpnd* dest, Bound* bound)
{
    initOpnd(dest);
    Bound* stored_bound = _map[dest].getMaxFalseBound();
    return stored_bound && Bound::leq(bound, stored_bound);
}

bool MemoizedDistances::minReducedDistanceLeqBound(IOpnd* dest, Bound* bound)
{
    initOpnd(dest);
    Bound* stored_bound = _map[dest].getMinReducedBound();
    return stored_bound && Bound::leq(stored_bound, bound);
}

void MemoizedDistances::print(std::ostream& os) const
{
    OpndToTRFChart::const_iterator it = _map.begin(), last = _map.end();
    os << "--- begin MemoizedDistances dump ---" << std::endl;
    for (; it != last; it++) {
        it->first->printFullName(os);
        os << " --> ";
        (it->second).print(os);
        os << std::endl;
    }
    os << "---   end MemoizedDistances dump ---" << std::endl;
}

//------------------------------------------------------------------------------

Bound* ActiveOpnds::getBound(IOpnd* opnd) const
{ 
    assert(hasOpnd(opnd)); 
    
    return _map.find(opnd)->second;
}

void ActiveOpnds::print(std::ostream& os) const
{
    os << "--- begin ActiveOpnds dump ---" << std::endl;
    for (iter_t it = _map.begin(), last = _map.end(); it != last; it++) {
        it->first->printFullName(os);
        os << " --> ";
        it->second->printFullName(os);
        os << std::endl;
    }
    os << "---   end ActiveOpnds dump ---" << std::endl;
}

//------------------------------------------------------------------------------

bool ClassicAbcdSolver::demandProve
     (IOpnd* source, IOpnd* dest, int32 bound_int, bool prove_upper_bound)
{
    assert(source && dest);
    _source_opnd = source;
    BoundState bs(prove_upper_bound);
    Bound bound(bound_int, bs);

    _active.makeEmpty();
    _mem_distance.makeEmpty();

    if (Log::isEnabled() ) {
        Log::out() << "demandProve(";
        _source_opnd->printFullName(Log::out());
        Log::out() << ", "; 
        dest->printFullName(Log::out());
        Log::out() << ", ";
        bound.printFullName(Log::out());
        Log::out() << std::endl;
    }

    if ( prove(dest, &bound, 0) == False ) {
        if (Log::isEnabled() ) {
            Log::out() << "demandProve: cannot eliminate check" << std::endl;
        }
        return false;
    }
    if (Log::isEnabled() ) {
        Log::out() << "demandProve: !!!CAN!!! eliminate check" << std::endl;
    }
    return true;
}

void ClassicAbcdSolver::updateMemDistanceWithPredecessors (IOpnd* dest, 
                                                           Bound* bound, 
                                                           uint32 prn_level, 
                                                      meet_func_t meet_f)
{
    const InequalityGraph::EdgeList& in_edges = _igraph.getInEdges(dest);
    assert(!in_edges.empty());
    InequalityGraph::EdgeList::const_iterator in_iter = in_edges.begin();
    InequalityGraph::EdgeList::const_iterator in_last = in_edges.end();
    IneqEdge* in_edge = (*in_iter);
    assert(in_edge->getDst() == dest);
    ProveResult res;
    assert(!_mem_distance.hasLeqBoundResult(dest, bound));
    res = prove(in_edge->getSrc(), 
                _bound_alloc.create_dec_const(bound, 
                                              in_edge->getLength()),
                prn_level + 1);
    in_iter++;
    for (; in_iter != in_last; in_iter++) {
        if(((res >= Reduced)  && (meet_f == meetBest)) ||
           ((res == False) && (meet_f == meetWorst))) {
            // For any x, meetBest(True, x)    == True
            //            meetBest(Reduced, x) >= Reduced  
            //        and meetWorst(False, x)  == False
            if (Log::isEnabled() ) {
                Printer prn(prn_level, Log::out());
                prn.prnStr("skipping remaining preds, proven: ");
                print_result(res, Log::out());
                Log::out() << std::endl;
            }
            break;
        }
        in_edge = (*in_iter);
        assert(in_edge->getDst() == dest);
        IOpnd* pred = in_edge->getSrc();
        res = meet_f(res, 
                     prove(pred, 
                           _bound_alloc.create_dec_const(bound, 
                                                         in_edge->getLength()),
                           prn_level + 1));
    }
    _mem_distance.updateLeqBound(dest, bound, res);
}

//
// prove that distance between '_source_opnd' and 'dest' is <= bound
//
ProveResult ClassicAbcdSolver::prove(IOpnd* dest, Bound* bound, 
        uint32 prn_level)
{
    Printer prn(prn_level, Log::out());
    if ( Log::isEnabled() ) {
        prn.prnStr("prove("); 
        dest->printFullName(Log::out());
        Log::out() << ", "; 
        bound->printFullName(Log::out()); Log::out() << ")" << std::endl;
    }

    // if ( C[dest - _source_opnd <= e] == True    for some e<=bound ) 
    //     return True
    if ( _mem_distance.minTrueDistanceLeqBound(dest, bound) ) {
        prn.prnStrLn("case 3: => True");
        return True;
    }

    // if ( C[dest - _source_opnd <= e] == False   for some e>=bound ) 
    //     return False
    if ( _mem_distance.maxFalseDistanceGeqBound(dest, bound) ) {
        prn.prnStrLn("case 4: => False");
        return False;
    }

    // if ( C[dest - _source_opnd <= e] == Reduced for some e<=bound ) 
    //     return Reduced
    if ( _mem_distance.minReducedDistanceLeqBound(dest, bound) ) {
        prn.prnStrLn("case 5: => Reduced");
        return Reduced;
    }
    
    // traversal reached the _source_opnd vertex
    if ( (dest->getID() == _source_opnd->getID()) && 
          Bound::int32_leq(0, bound) ) {
        prn.prnStrLn("reached source vertex => True");
        return True;
    }

    // all constant operands are implicitly connected
    if ( dest->isConstant() && _source_opnd->isConstant() ) {
        if ( Bound::const_distance_leq(_source_opnd->getConstant(), 
                                       dest->getConstant(), 
                                       bound) ) {
            prn.prnStrLn("reached source vertex (const) => True");
            return True;
        }else {
            prn.prnStrLn("reached source vertex (bad const) => False");
            return False;
        }
    }
    
    // if dest has no predecessor then fail
    if ( _igraph.getInEdges(dest).empty() ) {
        prn.prnStrLn("no predecessors => False");
        return False;
    }

    // a cycle was encountered
    if ( _active.hasOpnd(dest) ) {
        if ( Bound::leq(_active.getBound(dest), bound) ) {
            prn.prnStrLn("harmless cycle => Reduced");
            return Reduced; // a harmless cycle
        }else{
            prn.prnStrLn("amplifying cycle => False");
            return False; // an amplifying cycle
        }
    }

    _active.setBound(dest, bound);
    if ( dest->isPhi() ) {
        prn.prnStrLn("phi => worst");
        updateMemDistanceWithPredecessors(dest, bound, prn_level, meetWorst);
    }else{
        prn.prnStrLn("non_phi => best");
        updateMemDistanceWithPredecessors(dest, bound, prn_level, meetBest);
    }
    _active.clearOpnd(dest);

    ProveResult res = _mem_distance.getLeqBoundResult(dest, bound);
    if (Log::isEnabled() ) {
        prn.prnStr("proven: "); print_result(res, Log::out()); Log::out() << std::endl;
    }
    return res;
}

void ClassicAbcdSolver::Printer::prnLevel()
{
    if (Log::isEnabled()) {
        for (uint32 i = 0; i < _level; i++) {
            _os << "    ";
        }
    }
}

void ClassicAbcdSolver::Printer::prnStr(char* str)
{
    prnLevel();
    if (Log::isEnabled()) {
        _os << str;
    }
}

void ClassicAbcdSolver::Printer::prnStrLn(char* str)
{
    if (Log::isEnabled()) {
        prnStr(str);
        _os << std::endl;
    }
}

//------------------------------------------------------------------------------

/*
 * for(i = 5; i < A.length; i++) {
 *     check(-1 < i);
 *     check(i < A.length);
 *     ...
 * }
 *
 * i0 = 5
 * for:
 * i1 = phi(i0, i2)
 * if (i1 < A.length) {
 *     i3 = pi(i1)
 *     __check(i3 > -1);
 *     __check(i3 < A.length);
 *     ...
 *     i2 = i3 + 1;
 *     goto for
 * }
 * 
 * 0 -(0)-> i0 -(0)-> i1(phi)
 * i1(phi) -(0)-> i3 -(1)-> i2 -(0)-> i1(phi)
 * A.length -(-1)-> i3
 *
 * upper: prove(i3 - A.length <= -1) // trivial :)
 * lower: prove(i3 - (-1) >= 1) // not so trivial
 */
void testSimpleIGraph()
{
    MemoryManager mm("testSimpleIGraph.MemoryManager");
    InequalityGraph g(mm);
    ClassicAbcdSolver solver(g, mm);
    assert(g.isEmpty());

    IOpnd op0(0), op1(1, true /*phi*/), op2(2), op3(3);
    g.addOpnd(&op0);
    g.addOpnd(&op1);
    g.addOpnd(&op2);
    g.addOpnd(&op3);

    IOpnd op_const_5(4, false, true);
    op_const_5.setConstant(5);
    g.addOpnd(&op_const_5);

    IOpnd length(5);

    g.addOpnd(&length);
    g.addEdge(0, 1, 0);
    g.addEdge(1, 3, 0);
    g.addEdge(3, 2, 1);
    g.addEdge(2, 1, 0);
    g.addEdge(5, 3, -1);
    g.addEdge(4, 0, 0);

    assert(solver.demandProve(g.findOpnd(5), g.findOpnd(3), -1, true));
    //logfile << " testSimpleIGraph: OK" << std::endl;

    IOpnd op_m1(6, false /* phi */, true /* constant */);
    op_m1.setConstant(-1);
    g.addOpnd(&op_m1);

    assert(solver.demandProve(&op_m1, g.findOpnd(3), 1, false));
    op_const_5.setConstant(-5);
    assert(!solver.demandProve(&op_m1, g.findOpnd(3), 1, false));
    op_const_5.setConstant(0);
    assert(solver.demandProve(&op_m1, g.findOpnd(3), 1, false));
    op_const_5.setConstant(-1);
    assert(!solver.demandProve(&op_m1, g.findOpnd(3), 1, false));
    op_const_5.setConstant(5);

    //logfile << " lower_testSimpleIGraph: OK" << std::endl;
    //g.printDotFile(std::cout);
}

/*
 * for(i = 0; i < A.length; i++) {
 *     for (j = 0; j < i; j++) {
 *         check(j < A.length);
 *     }
 * }
 * i0 = 0
 * for1:
 * i1 = phi(i0, i3)
 * if (i1 < A.length) {
 *     i2 = pi(i1)
 *     j0 = 0
 *     for2:
 *     j1 = phi(j0, j3)
 *     if (j1 < i2) {
 *         j2 = pi(j1)
 *         __check(j2 < A.length)
 *         j3 = j2 + 1
 *         goto for2
 *     }
 *     i3 = i2 + 1
 *     goto for1
 * }
 * 0 -(0)-> i0 -(0)-> i1(phi) -(0)-> i2 -(1)-> i3 -(0)-> i1 (phi)
 * A.length -(-1)-> i2 
 * 0 -(0)-> j0 -(0)-> j1(phi) -(0)-> j2 -(1)-> j3 -(0)-> j1 (phi)
 * i2 -(-1)-> j2
 *
 * upper: prove(j2 - A.length <= -1)
 * lower: prove(j2 - (-1) >= 1)
 */
void testDoubleCycleGraph()
{
    MemoryManager mm("testDoubleCycleGraph.MemoryManager");
    InequalityGraph g(mm);
    ClassicAbcdSolver solver(g, mm);

    IOpnd i0(0), i1(1, true /*phi*/), i2(2), i3(3), 
          j0(10), j1(11, true /*phi*/), j2(12), j3(13), length(20);
    assert(g.isEmpty());
    g.addOpnd(&i0);
    g.addOpnd(&i1);
    g.addOpnd(&i2);
    g.addOpnd(&i3);
    g.addOpnd(&j0);
    g.addOpnd(&j1);
    g.addOpnd(&j2);
    g.addOpnd(&j3);
    g.addOpnd(&length);

    IOpnd op_const_0(21, false, true /*constant*/);
    op_const_0.setConstant(0);
    g.addOpnd(&op_const_0);

    g.addEdge(21, 0, 0);

    g.addEdge(0, 1, 0);
    g.addEdge(1, 2, 0);
    g.addEdge(2, 3, 1);
    g.addEdge(3, 1, 0);
    g.addEdge(20, 2, -1);
    g.addEdge(21, 10, 0);
    g.addEdge(10, 11, 0);
    g.addEdge(11, 12, 0);
    g.addEdge(12, 13, 1);
    g.addEdge(13, 11, 0);
    g.addEdge(2, 12, -1);

    assert(solver.demandProve(g.findOpnd(20), g.findOpnd(12), -1, true));
    //logfile << " testDoubleCycleGraph: OK" << std::endl;

    IOpnd op_m1(6, false /* phi */, true /* constant */);
    op_m1.setConstant(-1);
    g.addOpnd(&op_m1);

    assert(solver.demandProve(&op_m1, g.findOpnd(12), 1, false));
    //logfile << " lower_testDoubleCycleGraph: OK" << std::endl;
    //g.printDotFile(std::cout);
}

/*
 * limit = A.length
 * st = -1
 * while ( st < limit ) {
 *     st++
 *     limit--
 *     for (j = st; j < limit; j++) {
 *         check(limit >= 0) // should *not* be removable (amplifying)
 *         check(limit - j >= 0)
 *         check(j < A.length)
 *         t = j + 1;
 *         check(t < A.length)
 *     }
 * }
 *
 * limit0 = A.length // 00
 * st0 = -1          // 10
 * while:
 *     limit1 = phi(limit0, limit3)     // 01
 *     st1 = phi(st0, st3)              // 11
 *     if ( st1 < limit1 ) {
 *         st2 = pi(st1)                // 12
 *         limit2 = pi(limit1)          // 02
 *         st3 = st2 + 1                // 13
 *         limit3 = limit2 - 1          // 03
 *         j0 = st3                     // 20
 *     for:
 *         j1 = phi(j0, j4)             // 21
 *         if ( j1 < limit3 ) {
 *             j2 = pi(j1)              // 22
 *             limit4 = pi(limit3)      // 04
 *             __check(j2 < A.length)   //(22 < 55)
 *             __check(limit4 >= 0)
 *             j3 = pi(j2)              // 23
 *             __check(limit2 - j3 >= 0)
 *             t0 = j3 + 1              // 30
 *             __check(t0 < A.length)   //(30 < 55)
 *             t1 = pi(t0)              // 31
 *             j4 = j3 + 1              // 24
 *             goto for
 *         }
 *         goto while
 *     }
 */
void testPaperIGraph()
{
    MemoryManager mm("testPaperIGraph.MemoryManager");
    InequalityGraph g(mm);
    ClassicAbcdSolver solver(g, mm);

    assert(g.isEmpty());
    IOpnd minus_1(66, false /* phi */, true /*constant*/);
    minus_1.setConstant(-1);
    IOpnd length(55); // A.length

    IOpnd limit0(00), st0(10), limit1(01, true /*phi*/), st1(11, true /*phi*/),
          st2(12), limit2(02), st3(13), limit3(03), j0(20), 
          j1(21, true /*phi*/), j2(22), limit4(04), j3(23), t0(30), t1(31),
          j4(24);
 
    g.addOpnd(&limit0); g.addOpnd(&limit1); g.addOpnd(&limit2); 
        g.addOpnd(&limit3); g.addOpnd(&limit4);
    g.addOpnd(&st0); g.addOpnd(&st1); g.addOpnd(&st2); g.addOpnd(&st3);
    g.addOpnd(&j0); g.addOpnd(&j1); g.addOpnd(&j2); g.addOpnd(&j3); 
        g.addOpnd(&j4);
    g.addOpnd(&t0); g.addOpnd(&t1);

    g.addEdge(&minus_1, &st0, 0);
    g.addEdge(&st0, &st1, 0);
    g.addEdge(&st1, &st2, 0);
    g.addEdge(&st2, &st3, 1);
    g.addEdge(&st3, &st1, 0);
    g.addEdge(&st3, &j0, 0);
    g.addEdge(&limit2, &st2, -1);
    g.addEdge(&length, &limit0, 0);
    g.addEdge(&limit0, &limit1, 0);
    g.addEdge(&limit1, &limit2, 0);
    g.addEdge(&limit2, &limit3, -1);
    g.addEdge(&limit3, &limit1, 0);
    g.addEdge(&limit3, &limit4, 0);
    g.addEdge(&limit4, &j2, -1);
    g.addEdge(&j0, &j1, 0);
    g.addEdge(&j1, &j2, 0);
    g.addEdge(&j2, &j3, 0);
    g.addEdge(&j3, &j4, 1);
    g.addEdge(&j4, &j1, 0);
    g.addEdge(&j3, &t0, 1);
    g.addEdge(&length, &j3, -1);
    g.addEdge(&t0, &t1, 0);

    assert(solver.demandProve(&length, &j2, -1, true));
    assert(solver.demandProve(&length, &t0, -1, true));
    //logfile << " testPaperIGraph: OK" << std::endl;

    assert(!solver.demandProve(&minus_1, &limit4, 1, false));

    assert(solver.demandProve(&limit2, &j3, 0, false));
    //logfile << " lower_testPaperIGraph: OK" << std::endl;
    //g.printDotFile(std::cout);
}

void printExampleGraph()
{
    uint32 opnd_id = 0;
    IOpnd op0(opnd_id++), op1(opnd_id++, true), op2(opnd_id++, true, true);

    op2.setConstant(25);
    MemoryManager mm("printExampleGraph.MemoryManager");
    InequalityGraph graph(mm);
    graph.addOpnd(&op0);
    graph.addOpnd(&op1);
    graph.addOpnd(&op2);
    graph.addEdge(0, 1, 3);
    graph.addEdge(1, 2, -3);
    graph.addEdge(2, 0, 1);
    graph.printDotFile(std::cout);
}

void testMemoizedDistances()
{
    MemoryManager mm("testMemoizedDistances.MemoryManager");
    BoundState bs(true);
    InequalityGraph graph(mm);
    BoundAllocator b_alloc(mm);
    MemoizedDistances mem_distances(b_alloc);
    IOpnd op0(0), op1(1), op2(2);
    Bound bnd1(-1, bs), bnd2(5, bs), bnd3(10, bs), bndXX(5, bs);

    assert(!mem_distances.hasLeqBoundResult(&op0, &bnd1));
    mem_distances.updateLeqBound(&op0, &bnd2, Reduced);
    mem_distances.updateLeqBound(&op0, &bnd2, False);
    mem_distances.updateLeqBound(&op0, &bnd2, True);
    assert(mem_distances.getLeqBoundResult(&op0, &bnd2) == True);
}

/*
 * for(i = INT_MAX; i < A.length; i+= 25) {
 *     check(-1 < i);
 * }
 *
 * i0 = INT_MAX
 * for:
 * i1 = phi(i0, i2)
 * if (i1 < A.length) {
 *     i2 = pi(i1)
 *     __check(i2 > -1);
 *     ...
 *     i3 = i2 + 1;
 *     goto for
 * }
 * 
 * INT_MAX -(0)-> i0 -(0)-> i1(phi)
 * i1(phi) -(0)-> i2 -(1)-> i3 -(0)-> i1(phi)
 * A.length -(-1)-> i2
 *
 * lower: prove(i2 - (0) >= 0)
 *
 * -----------------------------------------
 * for(i = 25; i < A.length; i+= INT_MAX) {
 *     check(0 <= i);
 * }
 */
void testOverflow()
{
    MemoryManager mm("testOverflow.MemoryManager");
    InequalityGraph g(mm);
    ClassicAbcdSolver solver(g, mm);

    assert(g.isEmpty());
    IOpnd i0(00), i1(01, true /*phi */), i2(02), i3(03), 
          intmax(20, false /* phi */, true /* constant */), 
          zero(22, false /* phi */, true /* constant */), 
          length(21);
    
    intmax.setConstant(INT_MAX);
    zero.setConstant(0);
    g.addOpnd(&zero);
    g.addOpnd(&i0);
    g.addOpnd(&i1);
    g.addOpnd(&i2);
    g.addOpnd(&i3);
    g.addOpnd(&intmax);
    g.addOpnd(&length);

    g.addEdge(&intmax, &i0, 0);
    g.addEdge(&i0, &i1, 0);
    g.addEdge(&i1, &i2, 0);
    g.addEdge(&i2, &i3, 1);
    g.addEdge(&i3, &i1, 0);
    g.addEdge(&length, &i2, -1);

    assert(!solver.demandProve(&zero, &i2, 0, false));
    // well, array size is too big
    //g.printDotFile(std::cout);

    intmax.setConstant(25);
    (*g.getOutEdges(&i2).begin())->setLength(INT_MAX - 5);
    //g.printDotFile(std::cout);
    assert(!solver.demandProve(&zero, &i2, 0, false));
    //logfile << " testOverflow: OK" << std::endl;
}

//------------------------------------------------------------------------------

int classic_abcd_test_main()
{
    std::cout << "running ABCD self-tests" << std::endl;
    testMemoizedDistances();
    testSimpleIGraph();
    testDoubleCycleGraph();
    testPaperIGraph();

    // OK, testOverflow should fail
    //testOverflow();
    std::cout << "ABCD self-tests PASSED" << std::endl;

    return 0;
}

} //namespace Jitrino 

