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
 * @version $Revision: 1.14.24.4 $
 *
 */

#ifndef _CONTROLFLOWGRAPH_
#define _CONTROLFLOWGRAPH_

#include <assert.h>
#include <iostream>

#include "Stl.h"

#include "open/types.h"

namespace Jitrino {

const uint32 UINT32_MAX = (uint32) -1;

class ControlFlowGraph
{
public:
    ControlFlowGraph(MemoryManager& mm) : _entry(NULL), _exit(NULL), 
        _return(NULL), _unwind(NULL),
        _nodes(mm), _nodeIDGenerator(0), _edgeIDGenerator(0), _nodeCount(0),
        _traversalNumber(0), _lastModifiedTraversalNumber(0),  
        _lastOrderingTraversalNumber(0),
        _lastEdgeRemovalTraversalNumber(0) { }
    
    class Edge;
    /**
     * A doubly-linked list of edges.
     **/ 
    typedef StlList<Edge*> EdgeList;

    class Node;
    /**
     * A doubly-linked list of Nodes.
     **/
    typedef StlList<Node*> NodeList;
    
    /**
     * Base class for CFG Edges.  An Edge represents control flow between two nodes: both explicit (branches
     * and fall-throughs) and implicit (exceptions).  A CFG may contain more than one edge between the two given nodes.
     **/
    class Edge {
        friend class ControlFlowGraph;
        friend class Node;
    public:
        enum Kind {
            True,           // A taken branch BN->BN
            False,          // Fall-through after not-taken branch BN->BN
            Unconditional,  // Jump or fall-through with no branch BN->BN
            Exception,      // Exception throw (to DispatchNode)   *->DN
            Catch,          // Exception catch (from DispatchNode to handler)  DN->BN
            Switch,         // A taken switch  BN->BN  
            Exit,           // Any edge to the exit node.
            Unknown
        };



		// Get the unique edge ID.  This is unique for all edges in the containing flow graph.
        // It is constant for the lifetime of this edge.
        uint32 getId() const { return _id; }

        virtual Kind getKind() = 0; 
        
        Node* getSourceNode() { return _source; }
        Node* getTargetNode() { return _target; }
        Node* getNode(bool isForward) { return isForward ? _target : _source; }
        
    protected:
        Edge(Node* source, Node* target) : _id(UINT32_MAX), _source(source), _target(target) {}
        Edge() : _id(UINT32_MAX), _source(NULL), _target(NULL) {}

        virtual ~Edge() {}        
    private:
        void setId(uint32 num) { _id = num; }
        void setSourceNode(Node* source) { _source=source; }
        void setTargetNode(Node* target) { _target=target; }

        uint32 _id;
        Node* _source;
        Node* _target;
    };

    /**
     * Base class for CFG nodes.  A node may be either a block or dispatch 
     * node.  A block node represents a basic block and contains a sequence of 
     * instructions with a single entry point at the beginning and a single
     * exit at the end.  A block node is explicit; the JIT 
     * will eventually generate instructions corresponding to this node.
     * A dispatch node is used to model implicit control flow for throwing and catching 
     * exceptions.  A dispatch node is implicit; the JIT does not generate code.
     * Instead, the virtual machine performs the actual dispatch operation.
     **/
    class Node {
        friend class ControlFlowGraph;
    public:
        enum Kind {
            Block,      // Basic block
            Dispatch,   // Exception Dispatch
            Exit        // Exit
        };

        // Get the unique node ID.  This is unique for all nodes in the containing flow graph.
        // It is constant for the lifetime of this node.  This provides a sparse id between 0
        // and the graph's getMaxNodeId().
        uint32 getId() const { return _id; }
        
        // Get the depth first numbering of this node computed during the last pass.
        // Use this number as a dense id during a short phase.
        uint32 getDfNum() const { return _dfNumber; }
        uint32 getPreNum() const { return _preNumber; }
        uint32 getPostNum() const { return _postNumber; }

        // Get the traversal number of this node.  This is a monotonically increasing value, 
        // and always less than or equal to the traversal number of the graph itself.  
        // The traversal number is used mark a node visited during a particular traversal.
        uint32 getTraversalNum() const { return _traversalNumber; }
        void setTraversalNum(uint32 num) { _traversalNumber = num; }

        bool isBlockNode() const { return _kind==Block; }
        bool isDispatchNode() const { return _kind==Dispatch; }
        bool isExitNode() const { return _kind==Exit; }
        Kind getKind() const { return _kind; }

        const EdgeList& getInEdges() const { return _inEdges; }
        uint32 getInDegree() const { return (uint32) _inEdges.size(); }
        // For dispatch nodes, edges will be in control flow order (e.g., first handler to match, second, etc.). 
        const EdgeList& getOutEdges() const { return _outEdges; }
        uint32 getOutDegree() const { return (uint32) _outEdges.size(); }
        const EdgeList& getEdges(bool isForward) const { return isForward ? _outEdges : _inEdges; }

        // Find the edge corresponding to a particular source or target.  Return NULL if
        // no such edge is found.
        Edge* findSource(Node* source);
        Edge* findTarget(Node* target);

        // Return true if this node contains no instructions
        virtual bool isEmpty() const = 0;

        // Get unique edge that matches this kind.  Return NULL if no such edge exists.
        Edge* getTrueEdge() { return getOutEdge(Edge::True); }
        Edge* getFalseEdge() { return getOutEdge(Edge::False); }
        Edge* getUnconditionalEdge() { return getOutEdge(Edge::Unconditional); }
        Edge* getExceptionEdge() { return getOutEdge(Edge::Exception); }

        class Annotator {
        public:
	    virtual ~Annotator() {}
            virtual void annotateNode(::std::ostream& os, Node* node) = 0;
        };

        virtual void print(::std::ostream& os, Annotator* annotator=NULL);
        virtual void printLabel(::std::ostream& os);
        virtual void printInsts(::std::ostream& os, uint32 indent=0) = 0;

        class ChainedAnnotator : public Annotator {
        public:
            ChainedAnnotator(Annotator* a1=NULL, Annotator* a2=NULL, Annotator* a3=NULL) {
                add(a1);
                add(a2);
                add(a3);
            }
	    virtual ~ChainedAnnotator() {};
            virtual void annotateNode(::std::ostream& os, Node* node) {
                ::std::list<Annotator*>::iterator i;
                for(i = _annotators.begin(); i != _annotators.end(); ++i) {
                    (*i)->annotateNode(os, node);
                    os << "  ";
                }
            }

            void add(Annotator* annotator) {
                if(annotator != NULL)
                    _annotators.push_back(annotator);
            }

            void clear() {
                _annotators.clear();
            }
        private:
            ::std::list<Annotator*> _annotators; 
        };

    protected:
        // Constructs a new node.  If isBlock, the new node is a BlockNode, otherwise
        // it is a dispatch node.
        Node(MemoryManager& mm, Kind kind=Block) : 
          _id(UINT32_MAX), _dfNumber(UINT32_MAX), _preNumber(UINT32_MAX), _postNumber(UINT32_MAX), _traversalNumber(0), _kind(kind),
          _inEdges(mm), _outEdges(mm) {}

        virtual ~Node() {}
    private:
        void setId(uint32 num) { _id = num; }
        void setDFNum(uint32 num) { _dfNumber = num; }
        void setPreNum(uint32 num) { _preNumber = num; }
        void setPostNum(uint32 num) { _postNumber = num; }

        Edge* getOutEdge(Edge::Kind kind);

        uint32 _id;
        uint32 _dfNumber;
        uint32 _preNumber;
        uint32 _postNumber;
        uint32 _traversalNumber;
        Kind _kind;

        // Only blocks ending in switch statements have > 2 out edges.
        // Using a doubly linked list is pretty heavy weight for this.
        EdgeList _inEdges;
        EdgeList _outEdges;
    };

    // Get unique entry node.  This node dominates every other node in the graph.
    Node* getEntry() { return _entry; }
    void setEntry(Node* node) { _entry = node; }
    
    // Get unique exit node.  This node post-dominates every other node in the graph.
    Node* getReturn() { return _return; }
    void setReturn(Node* node) { _return = node; }

    // Get unique exit node.  This node post-dominates every other node in the graph.
    Node* getUnwind() { return _unwind; }
    void setUnwind(Node* node) { _unwind = node; }

    // Get unique exit node.  This node post-dominates every other node in the graph.
    Node* getExit() { return _exit; }
    void setExit(Node* node) { _exit = node; }

    // Return a list of all nodes in the graph.  The order in the list has no semantic
    // value.  Note this also returns unreachable nodes.
    const NodeList& getNodes() const { return _nodes; }

    Node* createBlockNode() { return _createNode(Node::Block); }
    Node* createDispatchNode() { return _createNode(Node::Dispatch); }
    Node* createExitNode() { return _createNode(Node::Exit); }
    Edge* createEdge(Node* source, Node* target) { return _createEdge(source, target); }
    
    void removeNode(Node* node);
    void removeNode(NodeList::iterator i);
    void removeEdge(Edge* edge);
    void removeEdge(Node* source, Node* target);

    // Replace old edge target with new edge target and update the source branch
    // instruction if necessary.
    Edge* replaceEdgeTarget(Edge* edge, Node* newTarget);

    // Given an STL container (e.g., vector, List, list), append the nodes of this CFG
    // in Postorder.  Excluding loop back edges, this will be a reverse topological ordering
    // of the nodes.  To get Reverse Postorder (topological ordering), use a reverse_iterator.
    template <class Container>
    void getNodesPostOrder(Container& container, bool isForward=true) {
        _lastOrderingTraversalNumber = ++_traversalNumber;
        _currentPreNumber = _currentPostNumber = 0;
        getNodesDFS((Container*) NULL, &container, isForward ? _entry : _exit, isForward);
        assert(_currentPreNumber == _currentPostNumber);
        _nodeCount = _currentPreNumber;
    }

    // Given an STL container (e.g., vector, List, list), append the nodes of this CFG
    // in Preorder.  Note this is sometimes called DepthFirst order.
    template <class Container>
    void getNodesPreOrder(Container& container, bool isForward=true) {
        _lastOrderingTraversalNumber = ++_traversalNumber;
        _currentPreNumber = _currentPostNumber = 0;
        getNodesDFS(&container, (Container*) NULL, isForward ? _entry : _exit, isForward);
        assert(_currentPreNumber == _currentPostNumber);
        _nodeCount = _currentPreNumber;
    }    

    void orderNodes(bool isForward=true) {
        _lastOrderingTraversalNumber = ++_traversalNumber;
        _currentPreNumber = _currentPostNumber = 0;
        getNodesDFS((NodeList*) NULL, (NodeList*) NULL, _entry, isForward);
        assert(_currentPreNumber == _currentPostNumber);
        _nodeCount = _currentPreNumber;
    }

    // Remove all unreachable nodes from CFG,
    void purgeUnreachableNodes() {
        purgeUnreachableNodes((NodeList*) NULL);
    }

    // Remove all unreachable nodes from CFG.  Add removed nodes to container.
    template <class Container>
    void purgeUnreachableNodes(Container& container) {
        purgeUnreachableNodes(&container);
    }

    // Remove all critical edges from Graph
    // Does not split exception edges (edges to Dispatch nodes)
    // unless parameter is true.
    void splitCriticalEdges(bool includeExceptionEdges);

    // Remove all empty nodes from CFG.
    void purgeEmptyNodes(bool preserveCriticalEdges=true);

    // Combine nodes from CFG that can be folded together.
    void mergeAdjacentNodes();
        
    // The traversal number is analogous to a monotonically increasing timestamp.
    // It is updated anytime an ordering traversal is performed on the CFG.
    // If a modification was performed after an ordering, the ordering is invalid.
    uint32 getTraversalNum() const { return _traversalNumber; }

    // The modification traversal number is the traversal number of the
    // last add/remove of a node/edge in the graph.
    uint32 getModificationTraversalNum() const { return _lastModifiedTraversalNumber; }

    // The modification traversal number is the traversal number of the
    // last remove of an edge in the graph.
    uint32 getEdgeRemovalTraversalNum() const { return _lastEdgeRemovalTraversalNumber; }

    // The ordering traversal number is the traversal number after the last depth
    // first ordering.  Node pre/postnumbers are valid if
    // getOrderingTraversalNum() > getModificationTraversalNum().
    uint32 getOrderingTraversalNum() const { return _lastOrderingTraversalNumber; }
    
    // Return true if the current ordering is still valid.
    bool hasValidOrdering() const { return getOrderingTraversalNum() > getModificationTraversalNum(); }

    // Return the number of nodes in the graph.  Recompute if the graph was modified.
    uint32 getNodeCount() { if(!hasValidOrdering()) orderNodes(); return _nodeCount; }

    void setTraversalNum(uint32 traversalNumber) { _traversalNumber=traversalNumber; }
    uint32  getMaxNodeId() const { return (uint32) _nodeIDGenerator; }
    uint32  getMaxEdgeId() const { return (uint32) _edgeIDGenerator; }
    virtual void print(::std::ostream& os, Node::Annotator* annotator=NULL);

protected:
	virtual ~ControlFlowGraph() {};
    // Link the node into the CFG.  The CFG now owns the node.
    void addNode(Node* node);
    void addNode(NodeList::iterator pos, Node* node);

    // Link the edge into the CFG.
    void addEdge(Node* source, Node* target, Edge* edge);
    void setNewEdgeId(Edge* edge) { edge->setId(_edgeIDGenerator++); }

    // Callbacks implemented by subclasses.
    virtual Node* _createNode(Node::Kind kind) = 0;
    virtual Edge* _createEdge(Node* source, Node* target) = 0;
    virtual void _updateBranchInstruction(Node* source, Node* oldTarget, Node* newTarget) = 0;
    virtual void _removeBranchInstruction(Node* source) = 0;
    virtual void _moveInstructions(Node* fromNode, Node* toNode, bool prepend) = 0;

    // Helper methods for subclasses.  Subclasses will need to manage
    // instructions.
    void mergeNodes(Node* source, Node* target, bool keepFirst=true);
    Node* splitNode(Node* node, bool newBlockAtEnd=true);
    Node* splitEdge(Edge* edge);

private:
    // Helper for getNodesPre/PostOrder above.
    template <class Container>
    void getNodesDFS(Container* preOrderContainer, Container* postOrderContainer, Node* node, bool isForward=true) {
        uint32 marked = _traversalNumber;
        node->setTraversalNum(marked);
        if(isForward)
            node->setDFNum(_currentPreNumber);
        node->setPreNum(_currentPreNumber++);
        if(preOrderContainer != NULL)
            preOrderContainer->push_back(node);
        EdgeList::const_iterator
            i = node->getEdges(isForward).begin(),
            iend = node->getEdges(isForward).end();
        for(; i != iend; i++) {
            Node* succ = (*i)->getNode(isForward);
            if(succ->getTraversalNum() < marked) {
                getNodesDFS(preOrderContainer, postOrderContainer, succ, isForward);
            }
        }
        node->setPostNum(_currentPostNumber++);
        if(postOrderContainer != NULL)
            postOrderContainer->push_back(node);
    }

    // Remove all nodes unreachable from entry.
    template <class Container>
    void purgeUnreachableNodes(Container* container) {
        if(!hasValidOrdering())
            orderNodes();

        NodeList::iterator
            iter = _nodes.begin(),
            end = _nodes.end();
        for(; iter != end;) {
            NodeList::iterator current = iter;
            Node* node = *iter;
            ++iter;
            if(node->_traversalNumber < _traversalNumber) {
                removeNode(current);
                if(container != NULL)
                    container->push_back(node);
                assert(node != _entry);
                if (node == _exit) _exit = NULL;
                if (node == _return) _return = NULL;
                if (node == _unwind) _unwind = NULL;
            }
        }
    }

    void moveInEdges(Node* oldNode, Node* newNode);
    void moveOutEdges(Node* oldNode, Node* newNode);

    void resetInEdges(Node* node);
    void resetOutEdges(Node* node);

    Node* _entry;
    Node* _exit;
    Node* _return;
    Node* _unwind;

    NodeList _nodes;

    uint32 _nodeIDGenerator;
    uint32 _edgeIDGenerator;
    uint32 _nodeCount;
    uint32 _traversalNumber;
    uint32 _lastModifiedTraversalNumber;
    uint32 _lastOrderingTraversalNumber;
    uint32 _lastEdgeRemovalTraversalNumber;
    uint32 _currentPreNumber;
    uint32 _currentPostNumber;
};

typedef ControlFlowGraph::Node ControlFlowNode;
typedef ControlFlowGraph::Edge ControlFlowEdge;

} //namespace Jitrino 

#endif // _CONTROLFLOWGRAPH_
