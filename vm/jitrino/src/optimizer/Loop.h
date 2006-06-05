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
 * @version $Revision: 1.10.22.4 $
 *
 */

#ifndef _LOOP_H_
#define _LOOP_H_

#include "BitSet.h"
#include "Stl.h"
#include "Tree.h"
#include "irmanager.h"
#include "PropertyTable.h"
#include "optpass.h"

namespace Jitrino {

DEFINE_OPTPASS(LoopPeelingPass)

class LoopNode : public TreeNode {
public:
    LoopNode* getChild()    {return (LoopNode*)child;}
    LoopNode* getSiblings() {return (LoopNode*)siblings;}
    LoopNode* getParent()   {return (LoopNode*)parent;}
    
    bool  inLoop(CFGNode* node)   {return nodesInLoop2->getBit(node->getDfNum());}

    virtual void  print(::std::ostream& cout) {
        if (parent == NULL)
            cout << "root";
        else
            header->printLabel(cout);
    }

    void  printNodesInLoop(::std::ostream& cout) {
        if (nodesInLoop2 == NULL)
            return;
        for (uint32 i = 0; i < nodesInLoop2->size(); i++)
            if (nodesInLoop2->getBit(i))
                cout << (int)i << " ";
    }

    /**** for Dot files *****/
    virtual void printDotNodesInTreeNode(::std::ostream& os) {
        os << " | {nodes: ";
        printNodesInLoop(os);
        os << "}";
    }

    CFGNode* getHeader() const { return header; }
private:
    friend class LoopTree;
    friend class LoopBuilder;

    LoopNode(CFGNode* hd, StlBitVector* bv) 
    : nodesInLoop2(bv), header(hd) {}

    void  markNode(CFGNode* node) {nodesInLoop2->setBit(node->getDfNum(),true);}
    const StlBitVector* getNodesInLoop() const { return nodesInLoop2; }

    StlBitVector* nodesInLoop2;
    CFGNode* header;  // loop entry point
};

class LoopTree : public Tree, public CFGNode::Annotator
{
public:
    LoopTree(MemoryManager& mm, FlowGraph* f) :
      mm(mm), flowgraph(f), size((uint32) f->getNodeCount()),
      traversalNum(0), loopNodeMap(mm),
      containingHeaderMap(mm) {
          loopNodeMap.reserve(size);
          containingHeaderMap.reserve(size);
    }

    
    uint32 getNodeCount() const { return size; }

    bool isLoopHeader(CFGNode* node) const { return (loopNodeMap[node->getDfNum()] != NULL); }
    bool hasContainingLoopHeader(CFGNode* node) const { return (containingHeaderMap[node->getDfNum()] != NULL); }
    CFGNode* getContainingLoopHeader(CFGNode* node) const { return containingHeaderMap[node->getDfNum()]->getHeader(); }
    LoopNode* getLoopNode(CFGNode* node) const { return loopNodeMap[node->getDfNum()]; }

    uint32 getLoopDepth(CFGNode* node) const {
        uint32 base = isLoopHeader(node) ? 1 : 0;
        return hasContainingLoopHeader(node) ? (base + getLoopDepth(getContainingLoopHeader(node)))
            : base;
    }

    bool loopContains(CFGNode* header, CFGNode *node) const { 
        LoopNode *loopNode = loopNodeMap[header->getDfNum()];
        assert(loopNode);
        return (loopNode->inLoop(node)); 
    };

    void process(LoopNode* r) {
		root = r;
        // Clear data structures
        loopNodeMap.clear();
        loopNodeMap.insert(loopNodeMap.begin(), size, NULL);
        containingHeaderMap.clear();
        containingHeaderMap.insert(containingHeaderMap.begin(), size, NULL);
        if(r != NULL)
            visit(r->getChild());
        traversalNum = flowgraph->getTraversalNum();
    }

    // True if the graph was not modified since the dominator was computed.
    bool isValid() { 
        return traversalNum > flowgraph->getModificationTraversalNum(); 
    }
    uint32 getTraversalNum() { return traversalNum; }

    void annotateNode(::std::ostream& os, ControlFlowNode* node) {
        if(isValid()) {
            CFGNode* n = (CFGNode*) node;
            bool isHeader = isLoopHeader(n);
            CFGNode* myHeader = hasContainingLoopHeader(n) ? getContainingLoopHeader(n) : NULL;
            uint32 depth = getLoopDepth(n);
            os << "[Loop Depth=" << (int) depth;
            if(isHeader)
                os << ", IsNewHeader";
            if(myHeader != NULL) {
                os << ", LoopHeader=";
                myHeader->printLabel(os);
            }
            os << "]";
        }
    }
    
private:
    class NodeVisitor {
    public:
        NodeVisitor(LoopNode* node, StlVector<LoopNode*>& headerMap) : headerNode(node), headerMap(headerMap) {}

        void visit(uint32 elem) {
            if((elem != headerNode->getHeader()->getDfNum()) && (headerMap[elem] == NULL))
                headerMap[elem] = headerNode;
        }

    private:
        LoopNode* headerNode;
        StlVector<LoopNode*>& headerMap;
    };

    void visit(LoopNode* node) {
        if(node == NULL)
            return;

        // Visit children first
        visit(node->getChild());

        // Visit header
        uint32 headerNum = node->getHeader()->getDfNum();
        loopNodeMap[headerNum] = node;
        NodeVisitor visitor(node, containingHeaderMap);
        const StlBitVector* nodesInLoop = node->getNodesInLoop();
        for(uint32 i = 0; i < nodesInLoop->size(); ++i)
            if(nodesInLoop->at(i))
                visitor.visit(i);

        // Visit siblings (remaining children of parent)
        visit(node->getSiblings());
    }


    MemoryManager& mm;
    FlowGraph* flowgraph;
    uint32 size;
    uint32 traversalNum;

    StlVector<LoopNode*> loopNodeMap;
    StlVector<LoopNode*> containingHeaderMap;
};

class LoopBuilder { 
    struct Flags {
        bool hoist_loads;
        bool invert;
        bool peel;
        bool insideout_peeling;
        bool old_static_peeling;
        bool aggressive_peeling;
        bool peel_upto_branch;
        uint32 peeling_threshold; 
        bool fullpeel;
        uint32 fullpeel_max_inst;
        bool unroll;
        uint32 unroll_count;
        uint32 unroll_threshold; 
        bool eliminate_critical_back_edge;
        bool peel_upto_branch_no_instanceof;
    };
private:
    Flags flags;
public:    
    static void showFlagsFromCommandLine();

public:
    LoopBuilder(MemoryManager& mm, IRManager& irm, DominatorTree& d, bool useProfile); 

    LoopTree* computeAndNormalizeLoops(bool doPeelLoops=false);
    bool needSsaFixup() { return needsSsaFixup; };
    void didSsaFixup() { needsSsaFixup = false; };
private:
    //
    // private functions
    //
    class CompareDFN {
    public:
        bool operator() (CFGEdge* h1, CFGEdge* h2) { return h1->getTargetNode()->getDfNum() < h2->getTargetNode()->getDfNum(); }
    };
    class IdentifyByID {
    public:
        uint32 operator() (CFGNode* node) { return node->getId(); }
    };
    class IdentifyByDFN {
    public:
        uint32 operator() (CFGNode* node) { return node->getDfNum(); }
    };

    void        findLoopHeaders(StlVector<CFGNode*>& headers);
    void        peelLoops(StlVector<CFGEdge*>& backEdges);
    void        hoistHeaderInvariants(CFGNode* preheader, CFGNode* header, StlVector<Inst*>& invariantInsts);
    bool        isVariantInst(Inst* inst, StlHashSet<Opnd*>& variantOpnds);
    bool        isVariantOperation(Operation operation);
    bool        isInversionCandidate(CFGNode* currentHeader, CFGNode* proposedHeader, StlBitVector& nodesInLoop, CFGNode*& next, CFGNode*& exit);
    uint32      findLoopEdges(MemoryManager& mm,StlVector<CFGNode*>& headers,StlVector<CFGEdge*>& loopEdges); 
    CFGEdge*    coalesceEdges(StlVector<CFGEdge*>& edges,uint32 numEdges);
    void        formLoopHierarchy(StlVector<CFGEdge*>& loopEdges,uint32 numLoops);
    void        createLoop(CFGNode* header,CFGNode* tail,uint32 numBlocks);
    uint32        markNodesOfLoop(StlBitVector& nodesInLoop,CFGNode* header,CFGNode* tail);
    LoopNode*   findEnclosingLoop(LoopNode* loop, CFGNode* header);


    MemoryManager&  loopMemManager; // for creating loop hierarchy
    DominatorTree&  dom;           // dominator information
    IRManager&      irManager;
    InstFactory&    instFactory;  // create new label inst for blocks
    FlowGraph&      fg;
    LoopTree*       info;
    LoopNode*       root;
    bool            useProfile;
    bool            canHoistLoads;
    bool            needsSsaFixup;
    bool            invert;
};

} //namespace Jitrino 

#endif // _LOOP_H_
