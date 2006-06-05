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
class JitrinoParameterTable;
class CFGNode;
class Opnd;
class BranchInst;
class AbcdSolver;
class AbcdAliases;
class AbcdReasons;

DEFINE_OPTPASS(ABCDPass)

typedef ::std::pair<Inst *, AbcdReasons *> InstReasonPair;

inline bool operator <(const InstReasonPair &pair1, const InstReasonPair &pair2) {
    return (pair1.first < pair2.first);
}

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
    CFGNode *lastTauPointBlock;
    SsaTmpOpnd *blockTauEdge;
    CFGNode *lastTauEdgeBlock;
public:
    struct Flags {
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
private:
    static Flags *defaultFlags;
    Flags flags;
public:    
    static void readDefaultFlagsFromCommandLine(const JitrinoParameterTable *params);
    static void showFlagsFromCommandLine();
    
    Abcd(IRManager &irManager0, 
         MemoryManager& memManager,
         DominatorTree& dom0)
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
          flags(*defaultFlags)
    {
    };
    
    ~Abcd() {
    };

    void runPass();
private:
    void insertPiNodes(); // insert and rename over whole tree;
    void insertPiNodes(DominatorNode *domBlock); // for each dominator
    void insertPiNodes(CFGNode *block); // for each dominator
    void insertPiNodesForUnexceptionalPEI(CFGNode *block, Inst *pei);
    void insertPiNodesForBranch(CFGNode *block, BranchInst *branchi, 
                                CFGEdge::Kind kind);
    void insertPiNodesForComparison(CFGNode *block,
                                    ComparisonModifier mod,
                                    const PiCondition &bounds,
                                    Opnd *op,
                                    bool swap_operands,
                                    bool negate_comparison);
    void insertPiNodeForOpnd(CFGNode *block, Opnd *org, 
                             const PiCondition &cond,
                             Opnd *tauOpnd = 0);
    // checks for aliases of opnd, inserts them.
    void insertPiNodeForOpndAndAliases(CFGNode *block, Opnd *org, 
                                       const PiCondition &cond,
                                       Opnd *tauOpnd = 0);
    PiOpnd *getNewDestOpnd(CFGNode *block, Opnd *org);

    Opnd *getConstantOpnd(Opnd *opnd); // dereferencing through Pis, 0 if not constant.
    void renamePiVariables();
    void renamePiVariables(CFGNode *block);
    void renamePiVariables(DominatorNode *block);

    void removePiNodes();
    void removePiNodes(CFGNode *block, Inst *i);

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

    SsaTmpOpnd *getBlockTauPoint(CFGNode *block);
    SsaTmpOpnd *getBlockTauEdge(CFGNode *block);
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
