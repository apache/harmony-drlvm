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
 * @version $Revision: 1.13.24.4 $
 *
 */

#ifndef _ABCD_H
#define _ABCD_H

#include <iostream>
#include "open/types.h"
#include "Opcode.h"
#include "FlowGraph.h"
#include "optpass.h"

namespace Jitrino {

class IRManager;
class MemoryManager;
class InequalityGraph;
class DominatorNode;
class Dominator;
class SparseOpndMap;
class PiCondition;
class Node;
class Opnd;
class BranchInst;
class AbcdSolver;
class AbcdAliases;
class AbcdReasons;


typedef ::std::pair<Inst *, AbcdReasons *> InstReasonPair;

inline bool operator <(const InstReasonPair &pair1, const InstReasonPair &pair2) {
    return (pair1.first < pair2.first);
}

struct AbcdFlags {
    bool partial;
    bool dryRun;
    bool useAliases;
    bool useConv;
    bool remConv;
    bool useShr;
    bool unmaskShifts;
    bool remBr;
    bool remCmp;
    bool remOneBound;
    bool remOverflow;
    bool checkOverflow;
    bool useReasons;
};

class Abcd {
    IRManager& irManager;
    MemoryManager &mm;
    InequalityGraph *ineqGraph;
    DominatorTree& dominators;
    SparseOpndMap *piMap;
    uint32 nextPiOpndId;
    AbcdSolver *solver;
    StlVector<InstReasonPair> canEliminate; // sorted by Inst
    StlVector<InstReasonPair> canEliminateUB; // keep sorted
    StlVector<InstReasonPair> canEliminateLB; // keep sorted

    SsaTmpOpnd *tauUnsafe;
    SsaTmpOpnd *tauSafe;
    SsaTmpOpnd *blockTauPoint;
    Node *lastTauPointBlock;
    SsaTmpOpnd *blockTauEdge;
    Node *lastTauEdgeBlock;
    
    AbcdFlags& flags;
public:    
    static void readFlags(Action* argSource, AbcdFlags* flags);
    static void showFlags(std::ostream& os);
    
    Abcd(IRManager &irManager0, MemoryManager& memManager, DominatorTree& dom0);
    
    ~Abcd() {
    };

    void runPass();
private:
    void insertPiNodes(); // insert and rename over whole tree;
    void insertPiNodes(DominatorNode *domBlock); // for each dominator
    void insertPiNodes(Node *block); // for each dominator
    void insertPiNodesForUnexceptionalPEI(Node *block, Inst *pei);
    void insertPiNodesForBranch(Node *block, BranchInst *branchi, 
                                Edge::Kind kind);
    void insertPiNodesForComparison(Node *block,
                                    ComparisonModifier mod,
                                    const PiCondition &bounds,
                                    Opnd *op,
                                    bool swap_operands,
                                    bool negate_comparison);
    void insertPiNodeForOpnd(Node *block, Opnd *org, 
                             const PiCondition &cond,
                             Opnd *tauOpnd = 0);
    // checks for aliases of opnd, inserts them.
    void insertPiNodeForOpndAndAliases(Node *block, Opnd *org, 
                                       const PiCondition &cond,
                                       Opnd *tauOpnd = 0);
    PiOpnd *getNewDestOpnd(Node *block, Opnd *org);

    Opnd *getConstantOpnd(Opnd *opnd); // dereferencing through Pis, 0 if not constant.
    void renamePiVariables();
    void renamePiVariables(Node *block);
    void renamePiVariables(DominatorNode *block);

    void removePiNodes();
    void removePiNodes(Node *block, Inst *i);

    void updateSsaForm();
    void buildInequalityGraph();
    void removeRedundantBoundsChecks();

    void markCheckToEliminate(Inst *); // used by solver to mark eliminable branches
    void markInstToEliminate(Inst *); // used by solver to mark other eliminable instructions
    void markCheckToEliminateAndWhy(Inst *, AbcdReasons *);
    void markInstToEliminateAndWhy(Inst *, AbcdReasons *);
    bool isMarkedToEliminate(Inst *, AbcdReasons *&why); // test whether was marked

    void markCheckToEliminateLB(Inst *); // if just LB check can be eliminated 
    void markInstToEliminateLB(Inst *); // 
    void markCheckToEliminateLBAndWhy(Inst *, AbcdReasons *);
    void markInstToEliminateLBAndWhy(Inst *, AbcdReasons *);
    bool isMarkedToEliminateLB(Inst *, AbcdReasons *&why);

    void markCheckToEliminateUB(Inst *); // if just UB check can be eliminated 
    void markInstToEliminateUB(Inst *); // 
    void markCheckToEliminateUBAndWhy(Inst *, AbcdReasons *);
    void markInstToEliminateUBAndWhy(Inst *, AbcdReasons *);
    bool isMarkedToEliminateUB(Inst *, AbcdReasons *&why);

    SsaTmpOpnd *getBlockTauPoint(Node *block);
    SsaTmpOpnd *getBlockTauEdge(Node *block);
    SsaTmpOpnd *getTauUnsafe();
    SsaTmpOpnd *getTauSafe();
    SsaTmpOpnd *getReasonTau(AbcdReasons *reason,
                             Inst *useSite);
    SsaTmpOpnd *makeReasonPhi(Opnd *derefVar, StlVector<AbcdReasons *> &reasons,
                              StlVector<Opnd *> &derefVarVersions);

    friend class AbcdSolver;
    friend class InsertPiWalker;
    friend class RenamePiWalker;
    friend class RemovePiWalker;
    friend struct AliasCheckingFun;
    void checkForAliases();
public:
    bool getAliases(Opnd *theOpnd, AbcdAliases *,
                    int64 addend);  // adds them to aliases list, adding addend

    static bool isConvOpnd(const Opnd *opnd);
    static bool convPassesSource(const Opnd *opnd);
    static Opnd *getConvSource(const Opnd *opnd);
    static bool typeIncludes(Type::Tag type1, Type::Tag type2);
    static bool hasTypeBounds(Type::Tag srcTag, int64 &lb, int64 &ub);
    static bool isCheckableType(Type::Tag type1);
    static bool hasCheckableType(const Opnd *opnd);
};

} //namespace Jitrino 

#endif // _ABCD_H
