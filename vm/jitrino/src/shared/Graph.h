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
 * @version $Revision: 1.8.24.4 $
 *
 */

//
// Graph.h
//
// Generic graph with nodes and bidirectional edges to be used for multiple purposes,
// for example: - Control Flow Graph construction,
//              - Call Graph
//              - Dependence analysis graphs
//              - Class Hierarchy Analysis,
//              - etc...
// 
#ifndef _GRAPH_
#define _GRAPH_

#include "Stl.h"
#include "Dlink.h"
#include "PrintDotFile.h"

namespace Jitrino {

//
// Generic Bidirectional Edge 
//  You can use it directly, or subclass if you require more information.
//

class Node;

class Edge {

public:
        Edge() : predNode(NULL), succNode(NULL), nextPredEdge(NULL), nextSuccEdge(NULL) {}
    Node*    getPredNode()            {return predNode; }
    Node*    getSuccNode()            {return succNode; }
    Edge*    getNextPredEdge()        {return nextPredEdge; }
    Edge*    getNextSuccEdge()        {return nextSuccEdge; }
    int      getEdgeFlags()           {return edgeFlags; }

    // these routines are not meant for general purpose use
    void     setNextPredEdge(Edge* e) {nextPredEdge    = e;    }
    void     setNextSuccEdge(Edge* e) {nextSuccEdge    = e;    }
    void     setPredNode(Node *n)     {predNode    = n;    }
    void     setSuccNode(Node *n)     {succNode    = n;    }
    void     setEdgeFlags(int flags)  {edgeFlags     = flags;}
    virtual  void printDotEdge(::std::ostream& os); // for DOT files
protected:
    Node*    predNode; Node * succNode;
    Edge*    nextPredEdge; Edge * nextSuccEdge;
    int      edgeFlags; // generic edge flags to be used by different algorithms
};




// invalid depth first number
#define INVALID_DFN  ((uint32)-1)

//
// Generic Node  -)
//  You can use it directly, or subclass if you require more information.
//

class Node : public Dlink {

   public:
       Node():  traversalNum(0), dfnum(INVALID_DFN), succs(NULL), preds(NULL) {}
    //
    //  true if there is any edge to a successor node.
    bool    hasEdge(Node* succNode);

    // utilities to perform node numbering
    uint16  getTraversalNum()           {return traversalNum;}
    void    setTraversalNum(uint16 num) {traversalNum = num;}
    uint32  getDfNum()                  {return dfnum;}
    void    setDfNum(uint32 num)        {dfnum = num;}

    // return the start list of predecessor or successor edges
    Edge*   getSuccEdges()              {return succs;}
    Edge*   getPredEdges()              {return preds;}

    // the following inherited from DLink, next and previous node in a linked list
    Node*   next()                      {return (Node*)_next;}
    Node*   prev()                      {return (Node*)_prev;}

    // the following utilities are useful in some tests
    bool    hasOnlyOneSuccEdge()        { return succs != NULL && succs->getNextSuccEdge() == NULL; }
    bool    hasOnlyOnePredEdge()        { return preds != NULL && preds->getNextPredEdge() == NULL; }
    bool    hasTwoOrMoreSuccEdges()     { return succs != NULL && succs->getNextSuccEdge() != NULL; }
    bool    hasTwoOrMorePredEdges()     { return preds != NULL && preds->getNextPredEdge() != NULL; }
    void    setSuccEdges(Edge* list)    {succs = list;}
    void    setPredEdges(Edge* list)    {preds = list;}

    virtual void print(::std::ostream& cout);
    virtual void printDotNode(::std::ostream& cout); // for Dot files

   private:
    friend    class Graph;            // associated graph to which this node belongs to

protected:
    //
    // indicate if the node has been visited or not
    //
    uint16  traversalNum;
    uint32  dfnum;  // depth first number
    //
    // linked list of successors and predecessors
    //
    Edge* succs;
    Edge* preds;
};


//
// Generic Graph. Not too much information so far, add as you need.
//

class Graph: public PrintDotFile {

public:
    Graph(MemoryManager& mm) :
        memManager(mm), traversalNum(0) {}

	MemoryManager& getMemoryManager()	{ return memManager; }
    uint32  getTraversalNum()            {return traversalNum;}
    void    setTraversalNum(uint32 num)  {traversalNum = num;}
    void    setBeenModified(bool mod)    {beenModified = mod;}
    bool    getBeenModified()            {return beenModified;}
    //
    // modify flow graph routines
    //
    void    addEdge(Node* pred, Node* succ);
    Edge *  deleteEdge(Node* pred, Node* succ);
    void    addEdge(Node* pred, Node* succ, Edge *edge);

    // more elaborate modify routines
    void    mergeNodeWithSuccessor(Node *pred, Node *succ);
    void    moveSuccEdgesToNewNode(Node *from, Node *to);

    //
    // insert node in the node list
    //
    void    insertNode(Node* node)       {node->insertBefore(&head);}
    //
    // get node list
    //
    Node*   getNodeList()                {return &head;}
    //
    //  assign depth first numbering starting from a node
    //
    void    assignDfNum(Node* n, uint32& dfNum, StlVector<Node*>& dfnMap);
    // 
    //  count nodes in the graph
    //
    uint32    countNodes();    
    //
    /* for DOT files support */
    virtual void printDotBody();
 
protected:
    // The following node provided to keep a list of the nodes in the graph
    Node        head;

    MemoryManager& memManager;
    uint32      traversalNum;
    bool        beenModified;  // graph has changed
private:
    Edge *  deleteEdgeToSuccessor(Node* pred, Node* succ);
    void    deleteEdgeToPredecessor(Node* succ, Edge *edge);
};


} //namespace Jitrino 

#endif // _GRAPH_
