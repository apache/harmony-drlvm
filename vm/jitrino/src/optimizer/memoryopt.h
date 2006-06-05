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
 * @version $Revision: 1.12.24.4 $
 *
 */

#ifndef _MEMORY_OPT_H
#define _MEMORY_OPT_H

#include <iostream>
#include "open/types.h"
#include "Opcode.h"
#include "FlowGraph.h"
#include "Stl.h"
#include <utility>
#include "optpass.h"

namespace Jitrino {

class IRManager;
class MemoryManager;
class InequalityGraph;
class DominatorNode;
class Dominator;
class DomFrontier;
class JitrinoParameterTable;
class CFGNode;
class Opnd;
class CSEHashTable;
class Type;
class LoopTree;
class AliasManager;
class InstMemBehavior;
class AliasAnalyzer;
class AliasDefSites;
class AliasRep;
class VarDefSites;
class MemPhiSites;
class AliasRenameMap;
class MemUseMap;

DEFINE_OPTPASS(MemoryValueNumberingPass)

class MemoryOpt {
    IRManager& irManager;
    FlowGraph& fg;
    MemoryManager &mm;
    DominatorTree& dominators;
    DomFrontier &df;
    LoopTree *loopTree;
    AliasManager *aliasManager;
    typedef StlHashMap<Inst *, InstMemBehavior *> Inst2MemBehavior;
    Inst2MemBehavior *instDoesWhat;
    AliasDefSites *aliasDefSites;
    MemPhiSites *memPhiSites;
    AliasRenameMap *renameMap;

    typedef StlVectorSet<Inst *> UsesSet;
    typedef StlHashMap<Inst *, UsesSet *> Def2UsesMap;
    typedef StlVectorSet<Inst *> DefsSet;
    typedef StlHashMap<Inst *, DefsSet *> Use2DefsMap;
    Use2DefsMap memUseDefs;
    Def2UsesMap memDefUses;


    InstMemBehavior *getOrCreateInstEffect(Inst *i);
    InstMemBehavior *getInstEffect(Inst *i); // 0 if none
public:
    enum Model { 
        Model_Strict,    // mark all memory operations unmovable, 
                         // unCSEable
        Model_ReadsKill, // each read acts like it is immediately
                         // followed by a write for the purposes of
                         // computing dependences below
        Model_CseFinal,  // allows CSE of all final fields
        Model_Default
    };
    enum Synch { 
        Synch_Fence,     // monitorEnter/monitorExit acts like a 
                         // read/write of ALL
        Synch_Moveable,  // allow load/store movement into lock regions
                         // to facilitate fence merging in case of
                         // eliminable lock
        Synch_OnlyLock,  // escape analysis can just remove locks,
                         // versus turning them into fence operations
        Synch_OneThread, // turn all lock-type operations into
                         // unsynchronized operations
        Synch_Default    
    };
    struct Flags {
        enum Model model;      
        enum Synch synch;
        bool debug;
        bool verbose;
        bool redstore;
        bool syncopt;
    };
private:
    static Flags *defaultFlags;
    Flags flags;
public:    
    static void readDefaultFlagsFromCommandLine(const JitrinoParameterTable *params);
    static void showFlagsFromCommandLine();

    MemoryOpt(IRManager &irManager0, 
              MemoryManager& memManager,
              DominatorTree& dom0,
              DomFrontier& df0,
              LoopTree *loopTree,
              AliasAnalyzer *aa0);

    ~MemoryOpt();

    void runPass();

    bool hasSameReachingDefs(Inst *i1, Inst *i2);
    bool hasDefReachesUse(Inst *def, Inst *use);
    // update maps with transitive closure of i1, then remove i1 from def-use

    void eliminateRedStores();
    void doSyncOpt();

private:
    // renaming phase for SSA construction

    void initMemoryOperations();
    void insertMemPhi();
    void createUsesMap();

    void createMemPhiInst(const AliasRep &, CFGNode *n);
    void insertPhiFor(const AliasRep &theRep, VarDefSites* defSites,
                      StlList<VarDefSites *> &ancestorSites);

    friend class MemoryOptInitWalker;
    friend class MemoryRenameWalker;
    friend class MemoryDebugWalker;
    friend class MemoryRedStoreWalker;
    friend class MemorySyncOptWalker;

    // methods to note memory effects of an instruction
    void effectAnyGlobal(CFGNode *n, Inst *i); // R/W anything that escapes or is global
    void effectWriteVtable(CFGNode *n, Inst *i, Opnd *opnd);
    void effectReadVtable(CFGNode *n, Inst *i, Opnd *opnd);
    void effectReadMethodPtr(CFGNode *n, Inst *i, Opnd *obj, MethodDesc *desc);
    void effectReadMethodPtr(CFGNode *n, Inst *i, MethodDesc *desc);
    void effectReadFunPtr(CFGNode *n, Inst *i, Opnd *funptr);

    void effectInit(CFGNode *n, Inst *i); // initial state, everything defined
    void effectExit(CFGNode *n, Inst *i);
    void effectEntry(CFGNode *n, Inst *i); // just globals overwritten
    void effectRead(CFGNode *n, Inst *i, Opnd *addr);
    void effectWrite(CFGNode *n, Inst *i, Opnd *addr);

    void effectReadClassVtable(CFGNode *n, Inst *i, NamedType *t);
    void effectWriteArrayLength(CFGNode *n, Inst *i, Opnd *opnd);
    void effectReadArrayLength(CFGNode *n, Inst *i, Opnd *opnd);
    void effectReadArrayElements(CFGNode *n, Inst *i, Opnd *arrayop, 
                                 Opnd *offsetop, Opnd *length);
    void effectWriteArrayElements(CFGNode *n, Inst *i, Opnd *arrayop,
                                  Opnd *offsetop, Opnd *length);

    // creates an object/array, returned in opnd:
    //   writes array length, etc.
    void effectNew(CFGNode *n, Inst *i, Opnd *dstop); 

    // make sure object's vtable are visible to others before publishing this
    void effectReleaseObject(CFGNode *n, Inst *i, Opnd *obj);

    // make sure object's vtable is available to all
    void effectInitType(CFGNode *n, Inst *i, NamedType *type);

    // mark end of initializer when finalizers should stay constant:
    void effectFinishObject(CFGNode *n, Inst *i, Opnd *obj);
    void effectFinishType(CFGNode *n, Inst *i, NamedType *type);

    // can commute with any ops, but not be added/removed:
    void effectIncCounter(CFGNode *n, Inst *i);

    // object may be 0 if lock has been removed
    void effectMonitorEnter(CFGNode *n, Inst *i, Opnd *object);
    void effectMonitorExit(CFGNode *n, Inst *i, Opnd *object);
    // just increments the lock on object, no acq/rel
    void effectIncRecCount(CFGNode *n, Inst *i, Opnd *object);

    // lock type methods
    void effectTypeMonitorEnter(CFGNode *n, Inst *i, Type *type);
    void effectTypeMonitorExit(CFGNode *n, Inst *i, Type *type);

private:
    // implementation
    void addDefToInstruction(CFGNode *n, Inst *i, const AliasRep &thisMem);
    void addUseToInstruction(CFGNode *n, Inst *i, const AliasRep &thisMem);
    void addReleaseToInstruction(CFGNode *n, Inst *i);
    void addAcquireToInstruction(CFGNode *n, Inst *i);

    void addMemUseDef(Inst *use, Inst *def);
    void addMemUseDefs(Inst *use, DefsSet &defs);
public:
    void remMemInst(Inst *inst); // notify MemoryOpt that we're removing this inst
    void replaceMemInst(Inst *oldI, Inst *newI); // substitute dep info
};

} //namespace Jitrino 

#endif // _MEMORY_OPT_H
