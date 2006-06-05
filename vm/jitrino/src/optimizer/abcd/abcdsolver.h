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
 * @version $Revision: 1.14.24.4 $
 *
 */

#ifndef _ABCD_SOLVER_H
#define _ABCD_SOLVER_H

#include <iostream>
#include <utility>
#include <functional>
#include <string>
#include "open/types.h"
#include "Type.h"
class Opnd;
#include "abcd.h"
#include "abcdbounds.h"

namespace Jitrino {

// Reduced means that there is a "harmless" cycle between the given vars
//         
//         v1 <= v2 + 5 <= op1
//
struct ProofLattice {
    enum ProofLatticeValue { 
        ProvenFalse, // tried to prove it and can't (not actually proven false)
        ProvenTrue,
        ProvenReduced 
    } value;
    ProofLattice() : value(ProvenFalse) {};
    ProofLattice(enum ProofLatticeValue t) : value(t) {};
    ProofLattice(const ProofLattice &other) : value(other.value) {};
    ProofLattice &operator=(enum ProofLatticeValue t) { value = t; return *this; }
    ProofLattice &operator=(const ProofLattice &other) { value = other.value; return *this; }
    void meetwith(ProofLattice &other) {
        if (value != ProvenFalse) {
            if ((other.value == ProvenFalse) ||
                (other.value == ProvenReduced)) {
                value = other.value;
            }
        }
    };
    void joinwith(ProofLattice &other) {
        if (value != ProvenTrue) {
            if ((other.value == ProvenTrue) ||
                (other.value == ProvenReduced)) {
                value = other.value;
            }
        }
    };
    bool operator==(ProofLatticeValue val) { return (value == val); };
    bool operator==(const ProofLattice &other) { 
        return (value == other.value); };
    bool operator!=(ProofLatticeValue val) { return (value != val); };
    bool operator!=(const ProofLattice &other) { 
        return (value != other.value); };
    void print(::std::ostream &outs) {
        switch (value) {
        case ProvenFalse: outs << "FALSE"; break;
        case ProvenTrue: outs << "TRUE"; break;
        case ProvenReduced: outs << "REDUCED"; break;
        }
    }
};

// true if a lower bound constraint
// represents op1 <= op2 + c    if an upper bound constraint
//         or op1 >= op2 + c    if a lower bound constraint
typedef ::std::pair<VarBound, VarBound> VarPair;
#ifdef PLATFORM_POSIX
struct AbcdSolverHash : public __gnu_cxx::hash<VarPair>
#else
    #ifndef __SGI_STL_PORT
        struct AbcdSolverHash : public stdext::hash_compare<VarPair>
    #else
        struct AbcdSolverHash : public ::std::hash_compare<VarPair>
    #endif
#endif
{
    size_t operator() (const VarPair &x) const {
        return ((x.first.hash()+1)* (x.second.hash()+2));
    };
    size_t operator() (const VarBound &x) const {
        return (x.hash());
    };
};
typedef StlMap<VarPair, ConstBound/*, AbcdSolverHash*/> VarPair2ConstBound;
struct MemoizedBounds {
    ConstBound leastTrueUB;     // least c s.t. v1 <= v2 + c is known True
    ConstBound leastReducedUB;  // least c s.t. v1 <= v2 + c is known Reduced
    ConstBound greatestFalseUB; // max c s.t. v1 <= v2 + c is known False
    ConstBound greatestTrueLB;  // max c  s.t. v1 >= v2 + c is known True
    ConstBound greatestReducedLB;  // max c  s.t. v1 >= v2 + c is known Reduced
    ConstBound leastFalseUB;    // least c  s.t. v1 >= v2 + c is known False
    AbcdReasons *leastTrueUBwhy;
    AbcdReasons *greatestTrueLBwhy;
    AbcdReasons *leastReducedUBwhy;
    AbcdReasons *greatestReducedLBwhy;
public:
    MemoizedBounds() :
        leastTrueUB(true), // PlusInfinity
        leastReducedUB(true), // PlusInfinity
        greatestFalseUB(false), // MinusInfinity
        greatestTrueLB(false), // MinusInfinity
        greatestReducedLB(false), // MinusInfinity
        leastFalseUB(true), // PlusInfinity
        leastTrueUBwhy(0),
        greatestTrueLBwhy(0),
        leastReducedUBwhy(0),
        greatestReducedLBwhy(0)
    {};
};
// cache of bounds on (var2-var1)
typedef StlMap<VarPair, MemoizedBounds /*, AbcdSolverHash*/> AbcdMemo;

class AbcdSolver : public ::std::unary_function<Inst *, void> {
public:
    AbcdSolver(Abcd *pass);
    ~AbcdSolver();

private:
    Abcd *thePass;
    // memorized results of queries: 
    AbcdMemo cache;
    VarPair2ConstBound active;
    ::std::string indent;

    struct pushindent {
        AbcdSolver *solver;
        ::std::string oldindent;
        pushindent(AbcdSolver *solvr) : solver(solvr), oldindent(solvr->indent) {
            solvr->indent += "    ";
    }
        ~pushindent() {
            solver->indent = oldindent;
    }
    };
    friend struct pushindent;
public:
    // if (prove_lower_bound),
    // then try to prove that (var2 - var1 >= c)
    // else prove that (var2 - var1 <= c)
    // returns true if can be proven
    bool demandProve(bool prove_lower_bound,
                     const VarBound &var1,
                     const VarBound &var2,
                     int64 c,
                     AbcdReasons *why); // if non-null and proven true, reasons why
    // if (prove_lower_bound),
    // then try to prove that (var2 - var1 >= c)
    // else prove that (var2 - var1 <= c)
    // returns ProvenTrue if can be proven, 
    //    ProvenFalse if couldn't prove it
    // if a cycle which shouldn't affect result, yields ProvenReduced
    //    (external users shouldn't see that result)
    ProofLattice prove(bool prove_lower_bound,
                       const VarBound &var1,
                       const VarBound &var2,
                       ConstBound c,
                       AbcdReasons *why); // if non-null and proven true, reasons why
    // subroutine:
    //   try to prove var2 - var1 <= c
    //   by dereferencing either Var1 (if derefVar1==true) or Var2 (otherwise)
    // sets predsEmpty=true if preds set is empty
    ProofLattice proveForPredecessors(const VarBound &var1,
                                      const VarBound &var2,
                                      ConstBound c,
                                      bool derefVar1,
                                      bool &predsEmpty,
                                      AbcdReasons *why); // if non-null and proven true, reasons why
    
    // try to prove var2 - var1 <= c
    // by considering either var1 or var2, depending on derefVar1
    // if (!checkVar1), var2 should have been fully dereferenced 
    // and checked first.; sets noneApply=true if none apply 
    ProofLattice AbcdSolver::proveForSpecialCases(const VarBound &var1,
                                                  const VarBound &var2,
                                                  ConstBound c,
                                                  bool checkVar1,
                                                  bool &noneApply,
                                                  AbcdReasons *why); // if non-null and proven true, reasons why
    void tryToEliminate(Inst *); // solve for an instruction;
    void tryToFoldBranch(Inst *);
    void tryToFoldCompare(Inst *);
};


} //namespace Jitrino 

#endif // _ABCD_SOLVER_H
