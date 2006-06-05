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
 * @version $Revision: 1.15.12.2.4.4 $
 *
 */

#ifndef _FLOWGRAPH_
#define _FLOWGRAPH_

#include "Stl.h"
#include "BitSet.h"
#include "List.h"
#include "Inst.h"
#include "ControlFlowGraph.h"
#include "PrintDotFile.h"
#include "TranslatorIntfc.h"
#include <iomanip>
#include "Log.h"

namespace Jitrino {

class CFGNode;
class CFGEdge;
class FlowGraph;
class DominatorTree;
class DominatorNode;
class LoopTree;
class IRManager;
class TypeFailStream;

typedef StlList<CFGEdge*>   CFGEdgeDeque;
typedef StlList<CFGNode*>   CFGNodeDeque;

typedef StlMultiMap<Inst*, Inst*> JsrEntryInstToRetInstMap;
typedef std::pair<JsrEntryInstToRetInstMap::const_iterator, 
        JsrEntryInstToRetInstMap::const_iterator> JsrEntryCIterRange;

class CFGVector : public StlVector<CFGNode*> {
public:
    CFGVector(MemoryManager& mm) : StlVector<CFGNode*>(mm) {}
    CFGVector(MemoryManager& mm,uint32 size) : StlVector<CFGNode*>(mm) {
        reserve(size);
    }
    typedef StlVector<CFGNode*>::const_iterator ConstIterator;
    typedef StlVector<CFGNode*>::reverse_iterator ReverseIterator;
};

//
// Control Flow Graph Edge: same as Edge, with an enumeration !!!
//

class CFGEdge: public ControlFlowEdge {
    friend class FlowGraph;
public:
    CFGNode* getSourceNode() { return (CFGNode*) ControlFlowEdge::getSourceNode(); }
    CFGNode* getTargetNode() { return (CFGNode*) ControlFlowEdge::getTargetNode(); }

    CFGEdge::Kind getEdgeType();
    Kind    getKind() { return getEdgeType(); }

    int     getEdgeFlags() const    {return edgeFlags;}
    void    setEdgeFlags(int flags) {edgeFlags = flags;}

    double  getEdgeProb() { return prob; }
    void    setEdgeProb(double p) { prob = p; }

    void printDotCFGEdge(::std::ostream& os); // dot file support

protected:
    friend class CFGNode;

private:
    CFGEdge(ControlFlowNode* source, ControlFlowNode* target) : ControlFlowEdge(source, target), prob(0.0) {}
    CFGEdge() : ControlFlowEdge(), prob(0.0) { }
    int     edgeFlags;
    double  prob;       // the taken probability of the edge WRT source node
};


//
// Control Flow Graph node
//

class CFGNode : public ControlFlowNode {

public:
    CFGNode(MemoryManager& mm, LabelInst* label, Kind kind=Block)
    : ControlFlowNode(mm, kind), flags(0), info(NULL), insts(label), freq(0.0) { }
    void*    getInfo()                      {return info;}
    void     setInfo(void* r)               {info = r;}
    Inst*    getFirstInst()                 {return insts;}
    LabelInst* getLabel()                   {return (LabelInst*) insts; }
    uint32   getLabelId()                   {return ((LabelInst*)insts)->getLabelId(); }
    Inst*    getLastInst()                  {assert(insts != NULL); return insts->prev(); }
    bool     isEmpty() const                {return insts->next() == insts; }
    bool     isBasicBlock()                 {return isBlockNode(); }
    bool     isCatchBlock()                 {return getLabel()->isCatchLabel();}
    int      getFlags()                     {return flags; }
    void     setFlags(int f)                {flags = f;    }
    void     append(Inst* inst) {  // append inst to the end of block
        assert(insts != NULL);
        inst->unlink();
        inst->insertBefore(insts);
    }
    void     appendBeforeBranch(Inst* inst) { // append, but before any branch
        assert(insts != NULL);
        inst->unlink();
        Inst *lastInst = getLastInst();
        if (lastInst->getOperation().mustEndBlock()) {
            assert(!inst->getOperation().mustEndBlock());
            inst->insertBefore(lastInst);
        } else {
            inst->insertAfter(lastInst);
        }
    }
    void     prepend(Inst* inst) { // insert inst to the beginning of the block
        assert(insts != NULL);
        inst->unlink();
        inst->insertAfter(insts);
    }
    void     prependAfterCriticalInst(Inst* inst) { 
        // Insert inst to the beginning of block after any critical instruction
        // Critical instructions are those that are implicitly assumed to be
        // the first in a block. They include: Op_Catch.
        assert(insts != NULL);
        inst->unlink();
        Inst* realInst = insts->next();
        if (realInst->getOpcode() == Op_Catch) {
            inst->insertAfter(realInst); 
        } else
            inst->insertAfter(insts);
    }

    // the following utilities are useful in some tests
    bool    hasOnlyOneSuccEdge()        { return getOutEdges().size() == 1; }
    bool    hasOnlyOnePredEdge()        { return getInEdges().size() == 1; }
    bool    hasTwoOrMoreSuccEdges()     { return getOutEdges().size() > 1; }
    bool    hasTwoOrMorePredEdges()     { return getInEdges().size() > 1; }

    const CFGEdgeDeque& getInEdges() { return (const CFGEdgeDeque&) ControlFlowNode::getInEdges(); }
    const CFGEdgeDeque& getOutEdges() { return (const CFGEdgeDeque&) ControlFlowNode::getOutEdges(); }

    double  getFreq() { return freq; }
    void    setFreq(double f) { freq = f; }

    virtual void printLabel(::std::ostream& cout);
    void     printInsts(::std::ostream& cout, uint32 indent=0);    // print the instructions in a basic block
    void     printProfile(::std::ostream& cout);  // print the profile of a basic block
    void     printDotCFGNode(::std::ostream& cout, DominatorTree *dom, double freq = 0, IRManager *irm = NULL); // dot file support

    class ProfileAnnotator : public ControlFlowNode::Annotator {
    public:
        void annotateNode(::std::ostream& os, ControlFlowNode* _node) {
            CFGNode* node = (CFGNode*) _node;
            os << "[Freq=" << ::std::setprecision(4) << node->getFreq();
            if (node->getOutEdges().size() > 0 ) {
                os << ", Target Prob:";
                CFGEdgeDeque::const_iterator j = node->getOutEdges().begin();
                CFGEdgeDeque::const_iterator jend = node->getOutEdges().end();
                for (; j != jend; ++j) {
                    CFGEdge* edge = *j;
                    CFGNode* target = edge->getTargetNode();
                    os << " ";
                    target->printLabel(os);
                    os << "=" << edge->getEdgeProb();
                }
            }
            os << ", id=" << (unsigned int) node->getId() << "]";
        }
    };
protected:
    friend class FlowGraph;

    //
    // scratch flags for any purpose (some of them)
    int    flags;
    //
    // scratch field for any purpose
    //
    void*  info;
    //
    // instruction list: the first inst is a label inst
    //
    Inst*   insts;
    //
    // The execution frequency as obtained from an execution profile
    //
    double  freq;
};

class CFGNodeRenameTable;
class Opnd;
class DefUseBuilder;

typedef StlMultiMap<CFGNode*, CFGNode*> CFG2CFGNodeMap;

class FlowGraph : public ControlFlowGraph, public PrintDotFile {
public:
    FlowGraph(MemoryManager& mm, InstFactory& instFactory, OpndManager& opndManager)
        : ControlFlowGraph(mm), prolog(0), epilog(0),
          memManager(mm), opndManager(opndManager), instFactory(instFactory), 
          dfnMap(mm), edgeProfile(false), irManager(NULL) {}

    CFGNode*     getEntry()               {return (CFGNode*) ControlFlowGraph::getEntry() ;}
    CFGNode*     getReturn()              {return (CFGNode*) ControlFlowGraph::getReturn() ;}
    CFGNode*     getUnwind()              {return (CFGNode*) ControlFlowGraph::getUnwind();}
    CFGNode*     getExit()                {return (CFGNode*) ControlFlowGraph::getExit() ;}

    bool         hasEdgeProfile()      {return  edgeProfile; }
    void         setEdgeProfile(bool f)  { edgeProfile = f; }

    const CFGNodeDeque& getNodes() const  { return (CFGNodeDeque&) ControlFlowGraph::getNodes(); }
    void         getNodesPreOrder(StlVector<CFGNode*>& container, bool isForward=true) { ControlFlowGraph::getNodesPreOrder((StlVector<Node*>&) container, isForward); } 
    void         getNodesPreOrder(::std::vector<CFGNode*>& container, bool isForward=true) { ControlFlowGraph::getNodesPreOrder((::std::vector<Node*>&) container, isForward); } 
    void         getNodesPostOrder(StlVector<CFGNode*>& container, bool isForward=true) { ControlFlowGraph::getNodesPostOrder((StlVector<Node*>&) container, isForward); } 
    void         getNodesPostOrder(::std::vector<CFGNode*>& container, bool isForward=true) { ControlFlowGraph::getNodesPostOrder((::std::vector<Node*>&) container, isForward); } 

    CFGNode*     createBlockNode(); 
    CFGNode*     createDispatchNode();
    CFGNode*     createExitNode();
    CFGNode*     createCatchNode(uint32 order, Type* exceptionType); 
    CFGNode*     createBlock(LabelInst *label);
    CFGNode*     createBlockAfter(CFGNode *node);
    CFGNode*     createBlockAfter(LabelInst *label, CFGNode *node);
    CFGNode*     duplicateBlockAfter(LabelInst *label, CFGNode *afterNode, CFGNode *sourceNode);
    CFGNode*     spliceBlockOnEdge(CFGEdge* edge);
    CFGEdge*     addEdge(CFGNode* source, CFGNode* target);
    CFGEdge*     replaceEdgeTarget(Edge* edge, Node* newTarget);


    // Folds the branch at the end of block.  If isTaken, the true edge is
    // converted to an unconditional edge, and the false edge is deleted.
    // If !isTaken, then the false edge is converted, and the true edge
    // is deleted.  In either case, the branch instruction br is removed
    // from block.
    void         foldBranch(CFGNode* block, BranchInst* br, bool isTaken);

    void         foldSwitch(CFGNode* block, SwitchInst* sw, uint32 target);

    // Eliminates the check at the end of block and the associated exception
    // edge.  If (alwaysThrows), then eliminates the non-exception edge instead;
    // we should have already inserted throw instruction before the check.
    void         eliminateCheck(CFGNode* block, Inst* check, bool alwaysThrows);

    // Splices flowgraph of inlined method into this flowgraph, replacing the 
    // passed call instruction.  callNode must be the node that contains call.
    void         spliceFlowGraphInline(CFGNode* callNode, Inst* call, FlowGraph& inlineFG);

    // Finds nodes dominated by entry and post-dominated by end inclusive.  Entry must
    // dominate end, but end need not post-dominate entry.  NodesInRegion must be
    // initialized false as it is used to mark visited nodes.
    void findNodesInRegion(CFGNode* entry, CFGNode* end, StlBitVector& nodesInRegion);

    CFGNode* duplicateRegion(CFGNode* entry, StlBitVector& nodesInRegion, DefUseBuilder& defUses, double newEntryFreq=0.0);
    CFGNode* duplicateRegion(CFGNode* entry, StlBitVector& nodesInRegion, DefUseBuilder& defUses, 
        CFGNodeRenameTable& nodeRenameTable, OpndRenameTable& opndRenameTable, double newEntryFreq=0.0);
    CFGNode* tailDuplicate(CFGNode* pred, CFGNode* tail, DefUseBuilder& defUses);

    void         smoothProfileInfo();   
    void         smoothProfile(MethodDesc& methodDesc); // direct-computation version
    bool         isProfileConsistent(char *methodStr, bool warn);


    void         renameOperandsInNode(CFGNode *node, OpndRenameTable *renameTable);
    bool         inlineFinally(CFGNode *bblock);
    void         mergeWithSingleSuccessor(CFGNode *to, CFGNode *from);
    CFGNode*     splitNodeAtInstruction(Inst *inst);
    CFGNode*     splitReturnNode();
    void         renumberLabels();
    void         cleanupPhase();
    uint32       assignDepthFirstNum();
    void         orderDepthFirst();
    void         linkDFS(CFGNode *);
    void         printInsts(::std::ostream& cout, MethodDesc& methodDesc, CFGNode::Annotator* annotator=NULL);
    void         printDotFile(MethodDesc& methodDesc,const char *suffix, DominatorTree *dom);
    virtual void printDotBody();

    const StlVector<CFGNode*>& getDFNMap() const { return dfnMap; }

    CFGNode*    getNext(CFGNode* node) {
        CFGNodeDeque::const_iterator i = ::std::find(getNodes().begin(), getNodes().end(), node);
        assert(i != getNodes().end());
        ++i;
        if(i == getNodes().end())
            return NULL;
        else
            return *i;
    }
    CFGNode*    getPrev(CFGNode* node) {
        CFGNodeDeque::const_iterator i = ::std::find(getNodes().begin(), getNodes().end(), node);
        assert(i != getNodes().end());
        if(i == getNodes().begin())
            return NULL;
        else
            return *(--i);
    }

    bool    isModified() {return getModificationTraversalNum() >= getOrderingTraversalNum();}

    IRManager* getIRManager() { return irManager; }
    void setIRManager(IRManager* irm) { irManager = irm; }
protected:
    // Node factory hook for base class. 
    ControlFlowNode* _createNode(ControlFlowNode::Kind kind) { return (kind == CFGNode::Block  ? createBlockNode() : 
         (kind == CFGNode::Dispatch ? createDispatchNode() : createExitNode())); 
    }
    ControlFlowEdge* _createEdge(ControlFlowNode* source, ControlFlowNode* target) { return addEdge((CFGNode*) source, (CFGNode*) target); }

    void _updateBranchInstruction(ControlFlowNode* source, ControlFlowNode* oldTarget, ControlFlowNode* newTarget);
    void _removeBranchInstruction(ControlFlowNode* source);
    void _moveInstructions(Node* fromNode, Node* toNode, bool prepend);

private:
    CFGNode*     duplicateRegion(CFGNode* node, CFGNode* entry, StlBitVector& nodesInRegion, DefUseBuilder* defUses, 
                                 CFGNodeRenameTable* nodeRenameTable, OpndRenameTable* opndRenameTable);

    CFGNode*     duplicateCFGNode(CFGNode* node, StlBitVector* nodesInRegion, 
                                  DefUseBuilder* defUses, OpndRenameTable* opndRenameTable);
    CFGNode*     duplicateCFGNode(CFGNode *source, CFGNode *after, OpndRenameTable *renameTable);
    void         breakMonitorExitLoop(CFGNode* node, Inst* monExit);
    void         inlineJSRs();
    bool         inlineJSR(CFGNode *bblock, DefUseBuilder& defUses, JsrEntryInstToRetInstMap& entryMap);
    Inst*        findSaveRet(CFGNode* node);

    bool         _inlineFinally(CFGNode *to, CFGNode *from, CFGNode *target, void *info,
                               CFGNodeRenameTable *nodeRenameTable, 
                               OpndRenameTable *opndRenameTable);
    void         _fixFinally   (CFGNode *node, CFGNode *retTarget);
    void         _fixBranchTargets(Inst *newInst, CFGNodeRenameTable *nodeRenameTable);
    bool         validateDefinitions(DominatorNode* dnode, StlHashSet<SsaOpnd*>& defined);

    CFGNode*     prolog;
    CFGNode*     epilog;
    MemoryManager& memManager;
    OpndManager&   opndManager;
    InstFactory&   instFactory;
    StlVector<CFGNode*> dfnMap;
    bool        edgeProfile;     // set if the flow graph is annotated with edgeProfile.
    IRManager*  irManager;          // back pointer to IRManager
};


class CFGNodeRenameTable : public HashTable<CFGNode,CFGNode> {
public:
    typedef HashTableIter<CFGNode, CFGNode> Iter;

    CFGNodeRenameTable(MemoryManager& mm,uint32 size) :
                    HashTable<CFGNode,CFGNode>(mm,size) {}
    CFGNode *getMapping(CFGNode *node) {
        return (CFGNode*)lookup(node);
    }
    void     setMapping(CFGNode *node, CFGNode *to) {
        insert(node,to);
    }
    
protected:
    virtual bool keyEquals(CFGNode* key1,CFGNode* key2) const {
        return key1 == key2;
    }
    virtual uint32 getKeyHashCode(CFGNode* key) const {
        // return hash of address bits
        return ((uint32)(((POINTER_SIZE_INT)key) >> sizeof(void*)));
    }
private:
};

} //namespace Jitrino 

#endif // _FLOWGRAPH_
