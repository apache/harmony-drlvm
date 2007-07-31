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

#ifndef _CLASSIC_ABCD_SOLVER_H
#define _CLASSIC_ABCD_SOLVER_H

#include <assert.h>
#include <iostream>
#include <climits>

#include "open/types.h"
#include "Stl.h"

namespace Jitrino {

class IOpnd {
public:
    IOpnd(uint32 id, bool is_phi = false, bool is_constant = false) :
        _id(id), _phi(is_phi), _const(is_constant), 
        _unconstrained(false), _value(0)
    {}

    IOpnd() { assert(0); }
    virtual ~IOpnd() {};

    void setPhi(bool s = true) { _phi = s; }
    bool isPhi() const { return _phi; }

    void setIsConstant(bool s = true) { _const = s; }
    bool isConstant() const { return _const; }

    void setConstant(int32 val) { setIsConstant(true); _value = val; }
    int32 getConstant() const { assert(isConstant()); return _value; }

    void setUnconstrained(bool unc) { _unconstrained = unc; }
    bool isUnconstrained() { return _unconstrained; }

    void setID(uint32 id) { _id = id; }
    uint32 getID() const { return _id; }

    virtual void printName(std::ostream& os) const;
    void printFullName(std::ostream& os) const;
private:
    uint32 _id;
    bool   _phi, _const, _unconstrained;
    int32  _value;
};

class BoundState {
public:
    BoundState() : _upper(true) {}
    BoundState(bool upper) : _upper(upper) {}

    void setUpper(bool upper = true) { _upper = upper; }

    bool isUpper() const { return _upper; }
private:

    bool _upper;
};

class HasBoundState {
public:
    HasBoundState(const BoundState& bs) : _bound_state(bs) {}

    const BoundState& getBoundState() { return _bound_state; }

    bool isUpper() const { return _bound_state.isUpper(); }
private:
    const BoundState& _bound_state;
};

class IneqEdge {
public:
    IneqEdge(IOpnd* src, IOpnd* dst, int32 len) :
        _src(src), _dst(dst), _length(len)
    {}
    IOpnd* getSrc() const { return _src; }
    IOpnd* getDst() const { return _dst; }
    int32 getLength() const { return _length; }
    void setLength(int32 len) { _length = len; }
private:
    IOpnd *_src, *_dst;
    int32 _length;
};

class InequalityGraph {
    typedef StlMap<uint32, StlList<IneqEdge*> > OpndEdgeMap;
public:
    typedef StlList<IneqEdge*> EdgeList;
    InequalityGraph(MemoryManager& mem_mgr) : 
        _mem_mgr(mem_mgr), 
        _id_to_opnd_map(mem_mgr),
        _edges(mem_mgr),
        _opnd_to_inedges_map(mem_mgr), 
        _opnd_to_outedges_map(mem_mgr),
        _emptyList(mem_mgr)
    {}

    void addEdge(IOpnd* from, IOpnd* to, int32 distance);

    void addEdge(uint32 id_from, uint32 id_to, int32 distance);

    void addOpnd(IOpnd* opnd);

    const EdgeList& getInEdges(IOpnd* opnd) const;

    const EdgeList& getOutEdges(IOpnd* opnd) const;

    void printDotFile(std::ostream& os) const;

    bool isEmpty() const { return _id_to_opnd_map.empty(); }

    IOpnd* findOpnd(uint32 id) const;

    MemoryManager& getMemoryManager() { return _mem_mgr; }
private:
    friend class InequalityOpndIterator;
    friend class InequalityGraphPrinter;
    typedef StlMap<uint32, IOpnd*> IdToOpndMap;

    static bool has_other_opnd_with_same_id(IdToOpndMap& map, IOpnd* opnd);

    void printDotHeader(std::ostream& os) const;
    void printDotBody(std::ostream& os) const;
    void printDotEnd(std::ostream& os) const;

    void addEdgeToIdMap (OpndEdgeMap& mp, uint32 id, IneqEdge* edge);

    MemoryManager& _mem_mgr;
    IdToOpndMap _id_to_opnd_map;
    EdgeList _edges;

    OpndEdgeMap _opnd_to_inedges_map, _opnd_to_outedges_map;
    EdgeList _emptyList;
};

class Bound : public HasBoundState {
public:
    Bound(int32 bnd, const BoundState& bs) : HasBoundState(bs), _bound(bnd) {}

    // bound - int32 -> Bound
    Bound(Bound* bound, int32 val, const BoundState& bs);

    void printFullName(std::ostream& os);

    static bool leq(Bound* bound1, Bound* bound2);

    static bool eq(Bound* bound1, Bound* bound2);

    static bool leq_int32(Bound* bound1, int32 value);

    static bool int32_leq(int32 value, Bound* bound1);

    // returns (dst_val - src_val <= bound)
    static bool const_distance_leq(int32 src_val, int32 dst_val, Bound* bound);

private:
    friend class BoundAllocator;
    int32 _bound;
};

class TrueReducedFalseChart;

class BoundAllocator {
public:
    BoundAllocator(MemoryManager& mem_mgr) : _mem_mgr(mem_mgr) {}

    Bound* create_inc1(Bound* bound);

    Bound* create_dec1(Bound* bound);

    Bound* create_dec_const(Bound* bound, int32 cnst);

    TrueReducedFalseChart* create_empty_TRFChart();

private:
    friend class MemoizedDistances;

    MemoryManager& getMemoryManager() { return _mem_mgr; }

    Bound* newBound(int32 val, const BoundState& bs);
    MemoryManager& _mem_mgr;
};

enum ProveResult {
    True = 2,
    Reduced = 1,
    False = 0
};

typedef ProveResult (*meet_func_t)(ProveResult, ProveResult);

class TrueReducedFalseChart {
public:
    TrueReducedFalseChart() :
        _max_false(NULL),
        _min_true(NULL),
        _min_reduced(NULL),
        _bound_alloc(NULL)
    {assert(0);}

    TrueReducedFalseChart(BoundAllocator* alloc) :
        _max_false(NULL),
        _min_true(NULL),
        _min_reduced(NULL),
        _bound_alloc(alloc)
    {}

    void addFalse(Bound* f_bound);

    void addReduced(Bound* r_bound);

    void addTrue(Bound* t_bound);

    bool hasBoundResult(Bound* bound) const;

    ProveResult getBoundResult(Bound* bound) const;

    Bound* getMaxFalseBound() { return _max_false; }

    Bound* getMinTrueBound() { return _min_true; }

    Bound* getMinReducedBound() { return _min_reduced; }

    void print(std::ostream& os) const;

private:
    void printBound(Bound* b, std::ostream& os) const;

    void clearRedundantReduced();

    Bound *_max_false, *_min_true, *_min_reduced;
    BoundAllocator* _bound_alloc;
};

class MemoizedDistances {
public:
    MemoizedDistances(BoundAllocator& alloc) : 
        _bound_alloc(alloc),
        _map(_bound_alloc.getMemoryManager()) 
    {}

    void makeEmpty();

    // set [dest - source <= bound] 
    void updateLeqBound(IOpnd* dest, Bound* bound, ProveResult res);

    bool hasLeqBoundResult(IOpnd* dest, Bound* bound) const;

    // returns [dest - source <= bound] 
    //      that is True, Reduced or False
    ProveResult getLeqBoundResult(IOpnd* dest, Bound* bound) const;

    bool minTrueDistanceLeqBound(IOpnd* dest, Bound* bound);

    bool maxFalseDistanceGeqBound(IOpnd* dest, Bound* bound);

    bool minReducedDistanceLeqBound(IOpnd* dest, Bound* bound);

    void print(std::ostream& os) const;

private:
    void initOpnd(IOpnd* op);

    typedef StlMap<IOpnd*, TrueReducedFalseChart> OpndToTRFChart;
    BoundAllocator& _bound_alloc;
    OpndToTRFChart _map;
};

class ActiveOpnds {
typedef StlMap<IOpnd*, Bound*>::const_iterator iter_t;
public:
    ActiveOpnds(MemoryManager& mem_mgr) : _map(mem_mgr) {}

    void makeEmpty() { _map.clear(); }

    bool hasOpnd(IOpnd* opnd) const { return _map.find(opnd) != _map.end(); }

    Bound* getBound(IOpnd* opnd) const;

    void setBound(IOpnd* opnd, Bound* bound) { _map[opnd] = bound; }

    void clearOpnd(IOpnd* opnd) { _map.erase(_map.find(opnd)); }

    void print(std::ostream& os) const;

private:
    StlMap<IOpnd*, Bound*> _map;
};

class ClassicAbcdSolver {
public:
    ClassicAbcdSolver(InequalityGraph& i, MemoryManager& solver_mem_mgr) : 
        _igraph(i), 
        _source_opnd(NULL), 
        _bound_alloc(solver_mem_mgr),
        _mem_distance(_bound_alloc),
        _active(solver_mem_mgr)
    {}

    bool demandProve
        (IOpnd* source, IOpnd* dest, int32 bound_int, bool prove_upper_bound);

private:
    ProveResult prove(IOpnd* dest, Bound* bound, uint32 prn_level);

    void updateMemDistanceWithPredecessors
        (IOpnd* dest, Bound* bound, uint32 prn_level, meet_func_t meet_f);

    class Printer {
    public:
        Printer(uint32 level, std::ostream& os) : _level(level), _os(os) {}

        void prnLevel();

        void prnStr(char* str);

        void prnStrLn(char* str);

    private:
        uint32 _level;
        std::ostream& _os; 
    };

    InequalityGraph& _igraph;
    IOpnd* _source_opnd;
    BoundAllocator _bound_alloc;
    MemoizedDistances _mem_distance;
    ActiveOpnds _active;
};

int classic_abcd_test_main();

} //namespace Jitrino 

#endif /* _CLASSIC_ABCD_SOLVER_H */
