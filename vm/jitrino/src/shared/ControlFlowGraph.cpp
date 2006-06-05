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
 * @version $Revision: 1.9.24.4 $
 *
 */

#include "ControlFlowGraph.h"
#include <algorithm>

namespace Jitrino {

void
ControlFlowGraph::addNode(ControlFlowGraph::Node* node)
{
    addNode(_nodes.end(), node);
}

void
ControlFlowGraph::addNode(ControlFlowGraph::NodeList::iterator pos, ControlFlowGraph::Node* node)
{
    // Set ID
    node->setId(_nodeIDGenerator);
    _nodeIDGenerator++;

    // Add node
    _nodes.insert(pos, node);

    // Reset node traversal number
    node->_traversalNumber = 0;

    // Mark the graph modified.
    _lastModifiedTraversalNumber = _traversalNumber;
}

void
ControlFlowGraph::removeNode(ControlFlowGraph::Node* node)
{
    removeNode(::std::find(_nodes.begin(), _nodes.end(), node));
}

void
ControlFlowGraph::removeNode(ControlFlowGraph::NodeList::iterator pos)
{
    Node* node = *pos;

    // Remove incident edges
    EdgeList::const_iterator eiter;
    for(eiter = node->_inEdges.begin(); eiter != node->_inEdges.end();) 
        removeEdge(*(eiter++));
    for(eiter = node->_outEdges.begin(); eiter != node->_outEdges.end();) 
        removeEdge(*(eiter++));

    // Erase the node itself
    _nodes.erase(pos);



    // Mark the graph modified.
    _lastModifiedTraversalNumber = _traversalNumber;
}

void
ControlFlowGraph::addEdge(ControlFlowGraph::Node* source, ControlFlowGraph::Node* target,
                          ControlFlowGraph::Edge* edge)
{
    // Set ID
    edge->setId(_edgeIDGenerator);
    _edgeIDGenerator++;

    // Out edge for source node
    edge->_source = source;
    source->_outEdges.push_back(edge);
 
    // In edge for target node
    edge->_target = target;
    target->_inEdges.push_back(edge);

    // Mark the graph modified.
    _lastModifiedTraversalNumber = _traversalNumber;
}

void
ControlFlowGraph::removeEdge(ControlFlowGraph::Edge* edge)
{
    // Remove out edge from source node
    Node* source = edge->getSourceNode();
    EdgeList& outEdges = source->_outEdges;
    outEdges.remove(edge);

    // Remove in edge from target node
    Node* target = edge->getTargetNode();
    EdgeList& inEdges = target->_inEdges;
    inEdges.remove(edge);

    // Mark the graph modified.
    _lastModifiedTraversalNumber = _traversalNumber;
    _lastEdgeRemovalTraversalNumber = _traversalNumber;
}

void
ControlFlowGraph::removeEdge(ControlFlowGraph::Node* source, ControlFlowGraph::Node* target)
{
    const EdgeList& edges = source->getOutEdges();
    EdgeList::const_iterator eiter = edges.begin();
    while(eiter != edges.end()) {
        Edge* edge = *eiter;
        ++eiter;
        if(edge->getTargetNode() == target)
            removeEdge(edge);
    }
}

ControlFlowGraph::Edge*
ControlFlowGraph::replaceEdgeTarget(Edge* edge, ControlFlowGraph::Node* newTarget)
{
    Node* source = edge->getSourceNode();
    Node* oldTarget = edge->getTargetNode();

    removeEdge(edge);
    Edge* newEdge = createEdge(source, newTarget);
    _updateBranchInstruction(source, oldTarget, newTarget);
    return newEdge;
}
    
ControlFlowGraph::Edge*
ControlFlowGraph::Node::findSource(ControlFlowGraph::Node* source)
{
    EdgeList::const_iterator
        i = _inEdges.begin(),
        iend = _inEdges.end();
    for(; i != iend; ++i) {
        if((*i)->getSourceNode() == source)
            return *i;
    }
    return NULL;
}

ControlFlowGraph::Edge*
ControlFlowGraph::Node::findTarget(ControlFlowGraph::Node* target)
{
    EdgeList::const_iterator
        i = _outEdges.begin(),
        iend = _outEdges.end();
    for(; i != iend; ++i) {
        if((*i)->getTargetNode() == target)
            return *i;
    }
    return NULL;
}

void 
ControlFlowGraph::resetInEdges(ControlFlowGraph::Node* node)
{
    EdgeList::iterator
        i = node->_inEdges.begin(),
        iend = node->_inEdges.end();
    for(; i != iend;) {
        Edge* edge = *i;
        ++i;
        // The old successor; node is the new successor
        Node* old = edge->getTargetNode();
        if(old != node) {
            Node* pred = edge->getSourceNode();
            // Check if another edge already goes to node.  .
            bool redundant = (pred->findTarget(node) != NULL);
            // Set this edge to point to node.  Note, we do this even if the
            // edge is to be deleted.  removeEdge only works if the edge
            // is well-formed.
            edge->setTargetNode(node);
            // Update branch instruction in source.
            if(pred->isBlockNode()) _updateBranchInstruction(pred, old, node);

            if(redundant) {
                // Edge is redundant - eliminate.
                if(pred->getOutDegree() == 2) {
                    // Pred already has edge to node and succ, and no other successors.  
                    // We can eliminate the branch and the edge to succ.
                    if(pred->isBlockNode()) _removeBranchInstruction(pred);
                }

                removeEdge(edge);
            }
        }
    }
}

void 
ControlFlowGraph::resetOutEdges(ControlFlowGraph::Node* node)
{
    EdgeList::iterator
        i = node->_outEdges.begin(),
        iend = node->_outEdges.end();
    for(; i != iend;) {
        Edge* edge = *i;
        ++i;
        // The old predecessor; node is the new predecessor
        Node* old = edge->getSourceNode();
        if(old != node) {
            Node* succ = edge->getTargetNode();
            // Check if another edge already comes from node.
            bool redundant = (succ->findSource(node) != NULL);
            // Set this edge to point to node.  Note, we do this even if the
            // edge is to be deleted.  removeEdge only works if the edge
            // is well-formed.
            edge->setSourceNode(node);

            if(redundant) {
                // Edge is redundant - eliminate.
                removeEdge(edge);
            }
        }
    }
}

ControlFlowGraph::Edge*
ControlFlowGraph::Node::getOutEdge(ControlFlowGraph::Edge::Kind kind)
{
    EdgeList::const_iterator
        i = _outEdges.begin(),
        iend = _outEdges.end();
    for(; i != iend; ++i) {
        Edge* edge = *i;
        if(edge->getKind() == kind)
            return edge;
    }
    return NULL;
}

void 
ControlFlowGraph::mergeNodes(Node* source, Node* target, bool keepFirst)
{
    assert(source->isBlockNode() && target->isBlockNode());
    assert(target->getInDegree() == 1);
    assert(source->getOutDegree() == 1);
    Edge* edge = *(target->_inEdges.begin());
    assert(edge->getSourceNode() == source);
    removeEdge(edge);

    if(keepFirst) {
        // Merge second block into first
        _moveInstructions(target, source, false);
        moveOutEdges(target, source);
        removeNode(target);
    } else {
        // Merge first block into second
        _moveInstructions(source, target, true);
        moveInEdges(source, target);
        removeNode(source);
    }
}

ControlFlowGraph::Node* 
ControlFlowGraph::splitNode(Node* node, bool newBlockAtEnd)
{
    assert(node->isBlockNode());
    Node* newNode = createBlockNode();

    if(newBlockAtEnd) {
        // move edges
        moveOutEdges(node, newNode);
        createEdge(node, newNode);
    } else { // new block at beginning
        // move edges
        moveInEdges(node, newNode);
        createEdge(newNode, node);
    }
    return newNode;
}

// WARNING: depends on the derived class's _createNode() always
// inserting the created nodes at the end of the node list.
// This is the case for FlowGraph currently, but you never know.
void
ControlFlowGraph::splitCriticalEdges(bool includeExceptionEdges)
{
    // go backwards through list so appending won't hurt us.

    NodeList::reverse_iterator 
        iter = _nodes.rbegin(),
        end = _nodes.rend();
    for (; iter != end; ++iter) {
        Node* target = *iter;
        // exception edges go to a dispatch node.
        // only look at nodes with multiple in edges.
        if (target->isDispatchNode() ? includeExceptionEdges : true) {
        restart:
            if (target->getInDegree() > 1) {
                EdgeList& inEdges = target->_inEdges;
                EdgeList::iterator
                    iterEdge = inEdges.begin(),
                    endEdge = inEdges.end();
                for (; iterEdge != endEdge; ++iterEdge) {
                    Edge* thisEdge = *iterEdge;
                    Node* source = thisEdge->getSourceNode();
                    if ((source->getOutDegree() > 1) &&
                        (includeExceptionEdges ||
                         !source->isDispatchNode())) {
                        // it's a critical edge, split it
                        splitEdge(thisEdge);
                        goto restart;
                    }
                }
            }
        }
    }        
}

ControlFlowGraph::Node* 
ControlFlowGraph::splitEdge(Edge *edge)
{
    Node* source = edge->getSourceNode();
    Node* target = edge->getTargetNode();
    Node* newNode = createBlockNode();

    removeEdge(edge);
    addEdge(source, newNode, edge);
    _updateBranchInstruction(source, target, newNode);
    createEdge(newNode, target);
    return newNode;
}

void         
ControlFlowGraph::purgeEmptyNodes(bool preserveCriticalEdges) {
    NodeList::iterator
        iter = _nodes.begin(),
        end = _nodes.end();
    for(; iter != end;) {
        NodeList::iterator current = iter;
        Node* node = *iter;
        ++iter;
        if(node->isBlockNode() && node->isEmpty()) {
            // Preserve entry and return nodes
            if(node == getEntry() || node == getReturn())
                continue;
            // Preserve catch nodes.  These nodes contain exception type information.
            if(/*(node->getInDegree() == 1) &&*/
               (node->getInEdges().front()->getSourceNode()->isDispatchNode()))
               continue;
            // Node must have one unconditional successor if it is empty.
            assert(node->getOutDegree() == 1);
            Node* succ = node->getOutEdges().front()->getTargetNode();
            assert(succ->isBlockNode());
            if(succ == node)
                // This is an empty infinite loop.
                continue;

            // Preserve this node if it breaks a critical edge if requested.
            bool preserve = false;
            if(preserveCriticalEdges && succ->getInDegree() > 1) {
                // Preserve this block if any predecessors have multiple out edges.
                EdgeList::const_iterator eiter;
                for(eiter = node->getInEdges().begin(); !preserve && eiter != node->getInEdges().end(); ++eiter) {
                    if((*eiter)->getSourceNode()->getOutDegree() > 1)
                        // This node breaks a critical edge.
                        preserve = true;
                }

                if(preserve) {
					if(node->getInDegree() == 1) {
                        Node* pred = node->getInEdges().front()->getSourceNode();
                        assert(pred->getOutDegree() > 1);
                        if(pred->getOutDegree() <= succ->getInDegree()) {
                            preserve = false;
                            for(eiter = pred->getOutEdges().begin(); !preserve && eiter != pred->getOutEdges().end(); ++eiter) {
                                Node* node2 = (*eiter)->getTargetNode();
                                if(node2 != succ && (!node2->isEmpty() || node2->getOutEdges().front()->getTargetNode() != succ))
                                    preserve = true;
                            }
                        }
                    }
                }
            }

            if(!preserve) {
                moveInEdges(node, succ);
                removeNode(current);
            }
        }
    }
}

void         
ControlFlowGraph::mergeAdjacentNodes() {
    NodeList::iterator
        iter = _nodes.begin(),
        end = _nodes.end();
    for(; iter != end;) {
        NodeList::iterator current = iter;
        Node* node = *iter;
        ++iter;
        if(node->getOutDegree() == 1) {
            Node* succ = node->getOutEdges().front()->getTargetNode();
            Node::Kind kind = node->getKind();
            if(kind == succ->getKind()) {
                if(kind == Node::Block) {
                    if(succ != getReturn() && succ->getInDegree() == 1) {
                        // Merge block nodes.
                        if(iter != end && *iter == succ)
                            ++iter;
                        mergeNodes(node, succ);
                    }
                } else if(kind == Node::Dispatch) {
                    // This dispatch has no handlers.
                    // Forward edges to this dispatch node to the successor.
                    _moveInstructions(node, succ, true);
                    moveInEdges(node, succ);
                    removeNode(current);
                }
            }
        }
    }
}

void
ControlFlowGraph::print(::std::ostream& os, ControlFlowGraph::Node::Annotator* annotator)
{
    MemoryManager mm(getMaxNodeId(), "ControlFlowGraph::print.mm");
    StlVector<Node*> nodes(mm);
    getNodesPostOrder(nodes);
    StlVector<Node*>::reverse_iterator 
        iter = nodes.rbegin(),
        end = nodes.rend();

    for(; iter != end; iter++) {
        (*iter)->print(os, annotator);
        os << ::std::endl;
    }
}

void
ControlFlowGraph::moveInEdges(ControlFlowGraph::Node* oldNode, ControlFlowGraph::Node* newNode)
{
    assert(oldNode != newNode);
    assert(oldNode->getKind() == newNode->getKind());
    EdgeList& oldIns = oldNode->_inEdges;
    EdgeList& newIns = newNode->_inEdges;
    newIns.splice(newIns.begin(), oldIns, oldIns.begin(), oldIns.end());
    // Reset edge pointers and update branch instructions.
    resetInEdges(newNode);
    assert(oldNode->getInDegree() == 0);

    // Mark the graph modified.
    _lastModifiedTraversalNumber = _traversalNumber;
}

void
ControlFlowGraph::moveOutEdges(ControlFlowGraph::Node* oldNode, ControlFlowGraph::Node* newNode)
{
    assert(oldNode->getKind() == newNode->getKind());
    EdgeList& oldOuts = oldNode->_outEdges;
    EdgeList& newOuts = newNode->_outEdges;
    newOuts.splice(newOuts.end(), oldOuts, oldOuts.begin(), oldOuts.end());
    resetOutEdges(newNode);
    assert(oldNode->getOutDegree() == 0);

    // Mark the graph modified.
    _lastModifiedTraversalNumber = _traversalNumber;
}

void
ControlFlowGraph::Node::print(::std::ostream& os, ControlFlowGraph::Node::Annotator* annotator)
{
    os << "Block ";
    printLabel(os);
    os << ":  ";
    if(annotator)
        annotator->annotateNode(os, this);
    os << ::std::endl;

    // Print predecessors
    os << "  Predecessors:";
    EdgeList::iterator 
        in = _inEdges.begin(),
        inEnd = _inEdges.end();
    for(; in != inEnd; ++in) {
        os << " ";
        (*in)->getSourceNode()->printLabel(os);
    }
    os << ::std::endl;

    // Print successors
    os << "  Successors:";
    EdgeList::iterator 
        out = _outEdges.begin(),
        outEnd = _outEdges.end();
    for(; out != outEnd; ++out) {
        os << " ";
        (*out)->getTargetNode()->printLabel(os);
    }
    os << ::std::endl;

    // Print instructions
    printInsts(os, 2);
}

void
ControlFlowGraph::Node::printLabel(::std::ostream& os)
{
    os << "#" << (int) getId();
}

} //namespace Jitrino 
