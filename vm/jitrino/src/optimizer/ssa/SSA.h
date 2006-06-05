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
 * @version $Revision: 1.11.16.4 $
 *
 */

#ifndef _SSA_H_
#define _SSA_H_

#include "MemoryManager.h"
#include "Stack.h"
#include "HashTable.h"
#include "Dominator.h"
#include "optpass.h"

namespace Jitrino {

class DomFrontier;
class SparseOpndMap;
struct OptimizerFlags;

DEFINE_OPTPASS(SSAPass)
DEFINE_OPTPASS(DeSSAPass)
DEFINE_OPTPASS(SplitSSAPass)
DEFINE_OPTPASS(FixupVarsPass)

//
// There is a VarDefSites for each var that records all CFG nodes in which 
// the var is defined. Phi node also defines var. The two bit vectors are
// used to avoid adding/recording CFG nodes twice.
// 
class VarDefSites {
public:
    VarDefSites(MemoryManager& m, uint32 max) 
    : stack(m), alreadyRecorded(m, max), insertedPhi(m, max) {}
    void addDefSite(CFGNode* node) {
        // avoid pushing the same node twice onto the stack
        if (alreadyRecorded.getBit(node->getDfNum()))
            return;
        alreadyRecorded.setBit(node->getDfNum(),true);
        stack.push(node);
    }
    CFGNode* removeDefSite() {return stack.pop();}
    void insertPhiSite(CFGNode* node) {
        insertedPhi.setBit(node->getDfNum(), true);
        addDefSite(node);
    }
    bool beenInsertedPhi(CFGNode* node) {
        return insertedPhi.getBit(node->getDfNum());
    }
    bool isDefSite(CFGNode* node) {
        return alreadyRecorded.getBit(node->getDfNum());
    }
private:
    Stack<CFGNode> stack;
    BitSet         alreadyRecorded; // def sites (blocks) been recorded
    BitSet         insertedPhi;     // blocks that have been inserted phi 
};

//
// A hash table that keeps the mapping: var --> VarDefSites
//
class DefSites {
public:
    DefSites(MemoryManager& m, uint32 n) : table(m,32), mm(m), numNodes(n) {}
    void addVarDefSite(VarOpnd* var, CFGNode* node) {
        if (var == NULL) return;

        VarDefSites* varSites = table.lookup(var);
        if (varSites == NULL) {
            varSites = new (mm) VarDefSites(mm, numNodes);
            table.insert(var,varSites);
        }
        varSites->addDefSite(node);
    }
    PtrHashTable<VarDefSites>* getVarDefSites() {return &table;}
private:
     PtrHashTable<VarDefSites> table;
     MemoryManager&  mm;
     uint32        numNodes;
};

class RenameStack;

class SSABuilder {
public:
    SSABuilder(OpndManager& om, InstFactory& factory, DomFrontier& df, FlowGraph* f, OptimizerFlags& optFlags) 
    : instFactory(factory), frontier(df), opndManager(om), fg(f), createPhi(false), optimizerFlags(optFlags) {}
    bool convertSSA(MethodDesc& methodDesc);
    bool fixupSSA(MethodDesc& methodDesc, bool useBetterAlg);
    bool fixupVars(FlowGraph* fg, MethodDesc& methodDesc);
    static void deconvertSSA(FlowGraph* fg,OpndManager& opndManager);
    static void splitSsaWebs(FlowGraph* fg,OpndManager& opndManager);
private:
    void findDefSites(DefSites& allDefSites);
    void insertPhi(DefSites& allDefSites);
    void createPhiInst(VarOpnd* var, CFGNode* insertedLoc);
    void addPhiSrc(PhiInst* i, SsaVarOpnd* src);
    void renameNode(RenameStack *rs, DominatorNode* dt,
                    const StlVectorSet<VarOpnd *> *whatVars);
    void clearPhiSrcs(CFGNode *, const StlVectorSet<VarOpnd *> *whatVars);
    void clearPhiSrcs2(CFGNode *, 
                       const StlVectorSet<VarOpnd *> *whatVars,
                       StlVector<VarOpnd *> *changedVars,
                       const StlVectorSet<Opnd *> *removedVars,
                       StlVector<CFGNode *> &scratchNodeList);
    bool checkForTrivialPhis(CFGNode *, 
                             StlVector<VarOpnd *> &changedVars);
    void checkForTrivialPhis2(CFGNode *node, 
                              const StlVectorSet<VarOpnd *> *lookatVars,
                              StlVector<VarOpnd *> *changedVars,
                              StlVector<Opnd *> *removedVars);

    InstFactory& instFactory;
    DomFrontier& frontier;
    OpndManager& opndManager;
    FlowGraph*   fg;
    bool         createPhi;
    OptimizerFlags& optimizerFlags;

    friend class ClearPhiSrcsWalker;
    friend class CheckForTrivialPhisWalker;
    friend class ClearPhiSrcsWalker2;
    friend class CheckForTrivialPhisWalker2;
    friend class SsaRenameWalker;
};

} //namespace Jitrino 

#endif // _SSA_H_
