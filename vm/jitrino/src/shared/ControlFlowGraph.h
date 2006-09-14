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
* @author Intel, Mikhail Y. Fursov
* @version $Revision$
*
*/

#ifndef _CONTROLFLOWGRAPH_
#define _CONTROLFLOWGRAPH_

#include <assert.h>
#include <iostream>

#include "Dlink.h"
#include "Stl.h"

#include "open/types.h"

namespace Jitrino {

class Edge;
class Node;
class CFGInst;
class ControlFlowGraph;
class Type;
class DominatorTree;
class LoopTree;

typedef StlVector<Edge*> Edges;
typedef StlVector<Node*> Nodes;

class Edge {
    friend class ControlFlowGraph;
    friend class ControlFlowGraphFactory;
public:
    enum Kind {                  // shortcut: max1: only one edge of this type/node. maxN: multiple edges/node
        Kind_Dispatch = 1,       // All *->DN edges.  
        Kind_Unconditional = 2,  // Jump or fall-through with no branch BN->BN, *->EXIT (max1)
        Kind_False = 4,          // Fall-through after not-taken branch BN->BN, default target of switches (max1)
        Kind_True = 8,           // A taken branch BN->BN, switch edges (except default) (maxN)
        Kind_Catch = 16          // All DN->BN edges  (maxN)
    };

    virtual ~Edge() {}


    // Get the unique edge ID.  This is unique for all edges in the containing flow graph.
    // It is constant for the lifetime of this edge.
    uint32 getId() const {return id;}

    virtual Kind getKind() const;
    bool isDispatchEdge() const {return getKind() == Edge::Kind_Dispatch;}
    bool isUnconditionalEdge() const {return getKind() == Edge::Kind_Unconditional;}
    bool isFalseEdge() const {return getKind() == Edge::Kind_False;}
    bool isTrueEdge() const {return getKind() == Edge::Kind_True;}
    bool isCatchEdge() const {return getKind() == Edge::Kind_Catch;}

    Node* getSourceNode() const {return source;}
    Node* getTargetNode() const {return target;}
    Node* getNode(bool isForward) const {return isForward ? target : source;}


    //profile info
    double getEdgeProb() const {return prob;}
    void setEdgeProb(double val) {prob = val;}

protected:
    Edge() : id (MAX_UINT32), source(NULL), target(NULL), prob(0) {}
    
    void setId(uint32 _id) {id = _id;}

    uint32 id;
    Node *source;
    Node *target;
    double prob;
};

class CFGInst : protected Dlink {
friend class Node;
friend class Edge;
friend class ControlFlowGraph;
public:
    virtual ~CFGInst(){}

    Node* getNode() const {return node;}
    CFGInst* next() const; 
    CFGInst* prev() const;

    // unlinks inst from node
    void unlink() {node = NULL; Dlink::unlink();}
    
    // inserts 'this' before 'inst'. Assign node to 'this' 
    // if 'this' inst has next inst, it's also inserted
    void insertBefore(CFGInst* inst);
    
    // inserts 'this' after 'inst'. Assign node to 'this' 
    // if 'this' inst has next inst, it's also inserted
    void insertAfter(CFGInst* inst);

    virtual bool isHeaderCriticalInst() const {return isLabel();}
    virtual bool isLabel() const {return false;}

protected:
    // called from CFG to detect BB->BB block edges
    virtual Edge::Kind getEdgeKind(const Edge* edge) const = 0;
    // called from CFG when edge target is replaced
    virtual void updateControlTransferInst(Node* oldTarget, Node* newTarget) {}
    // called from CFG when 2 blocks are merging and one of  the branches is redundant.
    virtual void removeRedundantBranch(){};

protected:
    CFGInst() : node(NULL) {}
    Node* node;
};

class Node {
    friend class ControlFlowGraph;
    friend class ControlFlowGraphFactory;
    friend class CFGInst;
public:
    enum Kind {
        Kind_Block,      // Basic block
        Kind_Dispatch,   // Exception Dispatch
        Kind_Exit        // Exit
    };

    virtual ~Node() {}

    Kind getKind() const {return kind;}

    // Get the unique node ID.  This is unique for all nodes in the containing flow graph.
    // It is constant for the lifetime of this node.  This provides a sparse id between 0
    // and the graph's getMaxNodeId().
    uint32 getId() const {return id;}

    // Get the depth first numbering of this node computed during the last pass.
    // Use this number as a dense id during a short phase.
    uint32 getDfNum() const {return dfNumber;}
    uint32 getPreNum() const {return preNumber;}
    uint32 getPostNum() const {return postNumber;}

    
    bool isBlockNode() const {return getKind() == Node::Kind_Block;}
    bool isDispatchNode() const {return getKind() == Node::Kind_Dispatch;}
    bool isExitNode() const {return getKind() == Node::Kind_Exit;}
    bool isCatchBlock() const {return getInDegree() >=1 && getInEdges().front()->isCatchEdge();}

    //edges
    const Edges& getInEdges() const {return inEdges;}
    const Edges& getOutEdges() const {return outEdges;}
    const Edges& getEdges(bool isForward) const {return isForward ? outEdges : inEdges;}
    
    // Get the first edge that matches this kind.  
    // Return NULL if no such edge exists,
    Edge* getOutEdge(Edge::Kind edgeKind) const;
    Edge* getUnconditionalEdge() const {return getOutEdge(Edge::Kind_Unconditional);}
    Edge* getFalseEdge() const {return getOutEdge(Edge::Kind_False);}
    Edge* getTrueEdge() const {return getOutEdge(Edge::Kind_True);}
    Edge* getExceptionEdge() const {return getOutEdge(Edge::Kind_Dispatch);}
    bool  isConnectedTo(bool isForward, Node* node) const {return findEdgeTo(isForward, node)!=NULL;}
    Edge*  findEdgeTo(bool isForward, Node* node) const;
    
    // return target of the first edge with specified kind found.
    // return NULL if no edge found with specified kind
    Node* getEdgeTarget(Edge::Kind edgeKind) const {Edge* edge = getOutEdge(edgeKind); return edge == NULL ? NULL : edge->getTargetNode();}
    Node* getUnconditionalEdgeTarget() const {return getEdgeTarget(Edge::Kind_Unconditional);}
    Node* getFalseEdgeTarget() const  {return getEdgeTarget(Edge::Kind_False);}
    Node* getTrueEdgeTarget() const  {return getEdgeTarget(Edge::Kind_True);}
    Node* getExceptionEdgeTarget() const  {return getEdgeTarget(Edge::Kind_Dispatch);}
    
    // Find the edge corresponding to a particular source or target.  Return NULL if
    // no such edge is found.
    Edge* findEdge(bool isForward, const Node* node) const;
    Edge* findSourceEdge(const Node* source) const {return findEdge(false, source);}
    Edge* findTargetEdge(const Node* target) const {return findEdge(true, target);}
    
    uint32 getOutDegree() const {return getOutEdges().size();}
    uint32 getInDegree() const {return getInEdges().size();}

    bool hasOnlyOneSuccEdge() const {return getOutDegree() == 1;}
    bool hasOnlyOnePredEdge() const {return getInDegree() == 1;}
    bool hasTwoOrMoreSuccEdges() const {return getOutDegree() >= 2;}
    bool hasTwoOrMorePredEdges() const {return getInDegree() >= 2;}


    
    // insts 

    // appends inst to the end of node list.
    // if inst is a list of insts (has next insts) -> the whole list is appended
    void appendInst(CFGInst* newInst, CFGInst* instBefore = NULL);

    // appends inst to the beginning of node list. Checks "block header critical inst" property
    // if inst is a list of insts (has next insts) -> the whole list is prepended
    void prependInst(CFGInst* newInst, CFGInst* instAfter = NULL);
    
    // counts number of inst in node. Complexity ~ num of insts
    uint32 getInstCount(bool ignoreLabels = true) const;
    bool isEmpty(bool ignoreLabels = true) const {CFGInst* inst = getFirstInst(); return inst == NULL || (ignoreLabels && inst->isLabel() && inst->next() == NULL);}
    CFGInst* getFirstInst() const {return instsHead->getNext() != instsHead ? (CFGInst*)instsHead->getNext() : NULL;}
    CFGInst* getLastInst() const {return instsHead->getPrev() != instsHead ? (CFGInst*)instsHead->getPrev() : NULL;}

    //profile info
    double getExecCount() const {return execCount;}
    void setExecCount(double val) {execCount = val;}

    //mod count
    uint32 getTraversalNum() const {return traversalNumber;}
    void setTraversalNum(uint32 num) {traversalNumber = num;}

    CFGInst* getSecondInst() const {return isEmpty() ? NULL : getFirstInst()->next();}
    CFGInst* getLabelInst() const {CFGInst* first = getFirstInst(); assert(first==NULL || first->isLabel()); return first;}

protected:

    Node(MemoryManager& mm, Kind _kind);
    
    void setId(uint32 _id) {id = _id;}
    void insertInst(CFGInst* prev, CFGInst* newInst);

    uint32 id;
    uint32 dfNumber;
    uint32 preNumber;
    uint32 postNumber;
    uint32 traversalNumber;
    Kind   kind;

    Edges inEdges;
    Edges outEdges;
    
    CFGInst* instsHead;
    double execCount;
};

class ControlFlowGraphFactory {
public:
    ControlFlowGraphFactory(){}
    virtual ~ControlFlowGraphFactory(){}
    virtual Node* createNode(MemoryManager& mm, Node::Kind kind);
    virtual Edge* createEdge(MemoryManager& mm, Node::Kind srcKind, Node::Kind dstKind);
};


class ControlFlowGraph {
public:

    ControlFlowGraph(MemoryManager& mm, ControlFlowGraphFactory* factory = NULL);
    virtual ~ControlFlowGraph(){};
    
    // Return a collection of all nodes in the graph.  The order in the list has no semantic
    // value.  Note this also returns unreachable nodes.
    const Nodes& getNodes() const {return nodes;}
    
    // Get unique entry node.  This node dominates every other node in the graph.
    Node* getEntryNode() const {return entryNode;}
    void setEntryNode(Node* e) {assert(e!=NULL); entryNode = e; lastModifiedTraversalNumber = traversalNumber;}
    
    // Get unique exit node.  This node post-dominates every other node in the graph.
    Node* getExitNode() const {return exitNode;}
    void setExitNode(Node* e) {assert(e!=NULL); exitNode = e; lastModifiedTraversalNumber = traversalNumber;}

    // Get unique exit block node.  
    // This node post-dominates every other block node in the graph.
    Node* getReturnNode() const {return returnNode; }
    void setReturnNode(Node* node) {assert(returnNode==NULL); assert(node->isBlockNode()); returnNode = node;}
        
    // Get unique unwind node.  This node post-dominates every other dispatch node in the graph.
    Node* getUnwindNode() const {return unwindNode;}
    void setUnwindNode(Node* node) {assert(unwindNode==NULL); assert(node->isDispatchNode()); unwindNode = node;}

    uint32 getMaxNodeId() const {return nodeIDGenerator;}
    
    Node* createNode(Node::Kind kind, CFGInst* inst = NULL);

    Node* createBlockNode(CFGInst* inst = NULL) {return createNode(Node::Kind_Block, inst);}
    
    Node* createDispatchNode(CFGInst* inst = NULL) {return createNode(Node::Kind_Dispatch, inst);}

    Node* createExitNode(CFGInst* inst = NULL) {return createNode(Node::Kind_Exit, inst);}
        
    //removes node and all it's in/out edges
    void removeNode(Node* node);

    // Return the number of nodes in the graph that are reachable. Recompute if the graph was modified.
    uint32 getNodeCount() { if(!hasValidOrdering()) orderNodes(); return nodeCount; }
    
    // return cached postorder collection
    const Nodes& getNodesPostOrder() {if (!hasValidOrdering()) orderNodes(); return postOrderCache;}
    
    template <class Container>
        void getNodes(Container& container) {
            container.insert(container.begin(), nodes.begin(), nodes.end());
        }

    template <class Container>
    void getNodesPostOrder(Container& container, bool isForward=true) {
        runDFS((Container*) NULL, &container,  isForward);
    }


    template <class Container>
    void getNodesPreOrder(Container& container, bool isForward=true) {
        runDFS(&container, (Container*) NULL, isForward);
    }

    void orderNodes(bool isForward=true) {
        runDFS((Nodes*) NULL, (Nodes*) NULL, isForward);
    }

    //edges:
    //creates new edge from source to target
    Edge* addEdge(Node* source, Node* target, double edgeProb = 1.0);
    uint32 getMaxEdgeId() const {return edgeIDGenerator;}
    void  removeEdge(Edge* edge);
    void  removeEdge(Node* source, Node* target) {removeEdge(source->findTargetEdge(target));}
    
    Edge* replaceEdgeTarget(Edge* edge, Node *newTarget);

    // adds unconditional edge for BB->BB,BB->E or dispatch edge for *->DN nodes
      
    //profile info
    bool hasEdgeProfile() const {return annotatedWithEdgeProfile;}
    void setEdgeProfile(bool val) {annotatedWithEdgeProfile = val;}
    //check if edge profile is consistent. 
    //Checks only reachable nodes. Recalculates postorder cache if needed.
    bool isEdgeProfileConsistent(bool checkEdgeProbs = true, bool checkExecCounts = true, bool doAssert=false);
    //count node exec count from probs
    void smoothEdgeProfile();


    // The traversal number is analogous to a monotonically increasing timestamp.
    // It is updated anytime an ordering traversal is performed on the CFG.
    // If a modification was performed after an ordering, the ordering is invalid.
    uint32 getTraversalNum() const {return traversalNumber;}
    void setTraversalNum(uint32 newTraversalNum) {traversalNumber = newTraversalNum;}

    // The modification traversal number is the traversal number of the
    // last add/remove of a node/edge in the graph.
    uint32 getModificationTraversalNum() const { return lastModifiedTraversalNumber; }

    // The modification traversal number is the traversal number of the
    // last remove of an edge in the graph.
    uint32 getEdgeRemovalTraversalNum() const { return lastEdgeRemovalTraversalNumber; }

    // The ordering traversal number is the traversal number after the last depth
    // first ordering.  Node pre/postnumbers are valid if
    // getOrderingTraversalNum() > getModificationTraversalNum().
    uint32 getOrderingTraversalNum() const { return lastOrderingTraversalNumber; }

    // Return true if the current ordering is still valid.
    bool hasValidOrdering() const { return getOrderingTraversalNum() > getModificationTraversalNum(); }

    //Memory manager used by graph
    MemoryManager& getMemoryManager() const {return mm;}

    Node* splitReturnNode(CFGInst* headerInst=NULL) {return splitNode(getReturnNode(), false, headerInst);}

    // this utility splits a node at a particular instruction, leaving the instruction in the
    // same node and moving all insts (before inst : splitAfter=false/after inst : splitAfter=true 
    // to the newly created note
    // returns the new node created
    Node* splitNodeAtInstruction(CFGInst *inst, bool splitAfter, bool keepDispatch, CFGInst* headerInst);

    Node* spliceBlockOnEdge(Edge* edge, CFGInst* inst = NULL);

    // Inline inlineFG info this CFG after 'instAfter' (splits inst's node if needed)
    // move all nodes from inlinedFG except exit node to 'this' FG
    // relies on valid 'return' and 'unwind' nodes on inlinedFG
    void spliceFlowGraphInline(CFGInst* instAfter, ControlFlowGraph& inlineFG, bool keepDispatch=true);
    
    // Inline inlineFG info this CFG moving egde head to inlinedFG prolog
    // move all nodes from inlinedFG except exit node to 'this' FG
    // relies on valid 'return' and 'unwind' nodes on inlinedFG
    void spliceFlowGraphInline(Edge* edge, ControlFlowGraph& inlineFG);

    // Remove all critical edges from Graph
    // Does not split exception edges (edges to Dispatch nodes)
    // unless parameter is true.
    void splitCriticalEdges(bool includeExceptionEdges, Nodes* newNodes = NULL);

    
    void moveInstructions(Node* fromNode, Node* toNode, bool prepend);

    
    // Combine nodes from CFG that can be folded together.
    void mergeAdjacentNodes(bool skipEntry = false, bool mergeByDispatch= false);

    void mergeBlocks(Node* source, Node* target, bool keepFirst=true);

    // Remove all empty nodes from CFG.
    void purgeEmptyNodes(bool preserveCriticalEdges = false, bool removeEmptyCatchBlocks = false);

    // Remove all unreachable nodes from CFG,
    void purgeUnreachableNodes() { purgeUnreachableNodes((Nodes*) NULL); }

    // Remove all unreachable nodes from CFG.  Add removed nodes to container.
    template <class Container> 
        void purgeUnreachableNodes(Container& container) { purgeUnreachableNodes(&container);}


    DominatorTree* getDominatorTree() const {return domTree;}
    DominatorTree* getPostDominatorTree() const {return postDomTree;}
    LoopTree* getLoopTree() const {return loopTree;}

    void setDominatorTree(DominatorTree* dom) {domTree = dom;}
    void setPostDominatorTree(DominatorTree* dom) {domTree = dom;}
    void setLoopTree(LoopTree* lt) {loopTree= lt;}
    
protected:
    void addNode(Node* node);
    void addEdge(Node* source, Node* target, Edge* edge, double edgeProb);
    void removeNode(Nodes::iterator i, bool erase);

    void setNewEdgeId(Edge* edge) { edge->setId(edgeIDGenerator++); }

    void moveInEdges(Node* oldNode, Node* newNode);
    void moveOutEdges(Node* oldNode, Node* newNode);

    void resetInEdges(Node* node);
    void resetOutEdges(Node* node);

    //low-level helper methods
    // WARN: this method does not keep dispatch on original block if newBlockAtEnd=true!
    // WARN: does not move instructions
    Node* splitNode(Node* node, bool newBlockAtEnd, CFGInst* inst);
    Node* splitEdge(Edge* edge, CFGInst* inst);
    bool ControlFlowGraph::isBlockMergeAllowed(Node* source, Node* target, bool allowMergeDispatch) const;

    // Helper for getNodesPre/PostOrder above.
    template <class Container>
    void runDFS(Container* preOrderContainer, Container* postOrderContainer, bool isForward) {
        Node* startNode;
        if (isForward) {
            lastOrderingTraversalNumber = ++traversalNumber;
            postOrderCache.clear();
            startNode = entryNode;
        } else {
            assert(exitNode!=NULL);
            startNode = exitNode;
        }
        currentPreNumber = currentPostNumber = 0;
        getNodesDFS(preOrderContainer, postOrderContainer, startNode, isForward);
        assert(currentPreNumber == currentPostNumber);
        if (isForward) {
            nodeCount = currentPreNumber;
        }
    }
    // Helper for getNodesPre/PostOrder above.
    template <class Container>
    void getNodesDFS(Container* preOrderContainer, Container* postOrderContainer, Node* node, bool isForward=true) {
        uint32 marked = traversalNumber;
        node->setTraversalNum(marked);
        if(isForward) {
            node->dfNumber = currentPreNumber;
        }
        node->preNumber = currentPreNumber++;
        if(preOrderContainer != NULL) {
            preOrderContainer->push_back(node);
        }
        Edges::const_iterator i = node->getEdges(isForward).begin(), iend = node->getEdges(isForward).end();
        for(; i != iend; i++) {
            Edge* edge = *i;
            Node* succ = edge->getNode(isForward);
            if(succ->getTraversalNum() < marked) {
                getNodesDFS(preOrderContainer, postOrderContainer, succ, isForward);
            }
        }
        node->postNumber = currentPostNumber++;
        if (postOrderContainer != NULL) {
            postOrderContainer->push_back(node);
        }
        if (isForward) {
            postOrderCache.push_back(node);
        }
    }

    // Remove all nodes unreachable from entry.
    template <class Container>
        void purgeUnreachableNodes(Container* container) {
            if(!hasValidOrdering()) {
                orderNodes();
            }

            Nodes::iterator iter = nodes.begin(), end = nodes.end();
            for(; iter != end;) {
                Nodes::iterator current = iter;
                Node* node = *iter;
                ++iter;
                if(node->traversalNumber < traversalNumber) {
                    removeNode(current, false);
                    if(container != NULL) {
                        container->push_back(node);
                    }
                }
            }
            nodes.erase(std::remove(nodes.begin(), nodes.end(), (Node*)NULL), nodes.end());
        }

    MemoryManager& mm;
    ControlFlowGraphFactory* factory;
    Node* entryNode;
    Node* returnNode;
    Node* exitNode;
    Node* unwindNode;
    
    Nodes nodes;
    Nodes postOrderCache;

    uint32 nodeIDGenerator;
    uint32 edgeIDGenerator;
    uint32 nodeCount;
    uint32 traversalNumber;
    uint32 lastModifiedTraversalNumber;
    uint32 lastOrderingTraversalNumber;
    uint32 lastEdgeRemovalTraversalNumber;
    uint32 lastProfileUpdateTraversalNumber;
    uint32 currentPreNumber;
    uint32 currentPostNumber;

    bool annotatedWithEdgeProfile;

    DominatorTree* domTree;
    DominatorTree* postDomTree;
    LoopTree* loopTree;
};


} //namespace Jitrino 

#endif // _CONTROLFLOWGRAPH_
