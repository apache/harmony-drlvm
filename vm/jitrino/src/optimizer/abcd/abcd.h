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
#include "insertpi.h"
#include "abcd/AbcdFlags.h"

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

class Abcd {
public:    
    static void readFlags(Action* argSource, AbcdFlags* flags);
    static void showFlags(std::ostream& os);
    
    Abcd(IRManager &irManager0, MemoryManager& memManager, DominatorTree& dom0);
    
    ~Abcd() {}

    void runPass();

    bool getAliases(Opnd *theOpnd, AbcdAliases *,
                    int64 addend);  // adds them to aliases list, adding addend

    static bool isConvOpnd(const Opnd *opnd);
    static bool convPassesSource(const Opnd *opnd);
    static Opnd *getConvSource(const Opnd *opnd);
    static bool typeIncludes(Type::Tag type1, Type::Tag type2);
    static bool hasTypeBounds(Type::Tag srcTag, int64 &lb, int64 &ub);
    static bool isCheckableType(Type::Tag type1);
    static bool hasCheckableType(const Opnd *opnd);
private:
    Opnd *getConstantOpnd(Opnd *opnd); // dereferencing through Pis, 0 if not constant.

    void removeRedundantBoundsChecks();
    void removePiEliminateChecksOnInst(Node *block, Inst *inst);

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
    SsaTmpOpnd *getTauUnsafe();
    SsaTmpOpnd *getTauSafe();
    SsaTmpOpnd *makeReasonPhi(Opnd *derefVar, StlVector<AbcdReasons *> &reasons,
                              StlVector<Opnd *> &derefVarVersions);
    SsaTmpOpnd* getReasonTau(AbcdReasons *reason, Inst *useSite);

    void removePiEliminateChecks();

    IRManager& irManager;
    MemoryManager &mm;
    DominatorTree& dominators;
    AbcdSolver *solver;
    StlVector<InstReasonPair> canEliminate; // sorted by Inst
    StlVector<InstReasonPair> canEliminateUB; // keep sorted
    StlVector<InstReasonPair> canEliminateLB; // keep sorted

    SsaTmpOpnd *tauUnsafe;
    SsaTmpOpnd *tauSafe;
    
    AbcdFlags& flags;
    InsertPi insertPi;

    SsaTmpOpnd* blockTauPoint;
    Node* lastTauPointBlock;

    friend class AbcdSolver;
    friend class RemovePiEliminateChecksWalker;
};

} //namespace Jitrino 

#endif // _ABCD_H
