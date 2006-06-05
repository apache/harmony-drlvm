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
 * @author Intel, Vyacheslav P. Shakin, Mikhail Y. Fursov
 * @version $Revision: 1.18.20.4 $
 */

#ifndef _IA32_CFG_H_
#define _IA32_CFG_H_

#include "open/types.h"
#include "MemoryManager.h"
#include "VMInterface.h"
#include "Type.h"
#include "Stl.h"
#include "Ia32Inst.h"
#include "BitSet.h"
namespace Jitrino
{
namespace Ia32{

    //=========================================================================================================
    // Forward declarations
    //=========================================================================================================
    class Node;
    class DispatchNode;
    class BasicBlock;
    class Inst;

    typedef double  ProbValue;
    typedef double  ExecCntValue;
    typedef StlVector<Node*> Nodes;
    typedef StlList<Node*> NodeList;
    

//deprecated
#define UnknownProbValue (-1.0)
#define UnknownExecCnt (-1.0)

#define EdgeProbValue_Unknown (-1.0)
#define ExecCountValue_Unknown (-1.0)
#define EdgeProbValue_Exception 0.01


    //========================================================================================
    // class LiveSet
    //========================================================================================
    /** class LiveSet represents a bit-set describing operand liveness at particular point

    Generally LiveSet can be implemented as any data structure emulating the bit-set pattern, 
    not necessarily the literal bit-set

    Currently it extends the BitSet helper class and adds the isLive convenience method

    */
    class LiveSet: public BitSet {
    public:
        /** initializes the bit-set to contain numOperands bits */
        LiveSet(MemoryManager& mm, uint32 numOperands)
            :BitSet(mm, numOperands){}

        LiveSet(MemoryManager& mm, const LiveSet& setToCopy)
            :BitSet(mm, setToCopy){}

        LiveSet(MemoryManager& mm)
            :BitSet(mm,0){}

        /** returns true if the opnd's id is set in the bit set (the operand is live) */
        bool isLive(const Opnd * opnd)const{ return getBit(opnd->getId()); }
    };

    //=========================================================================================================
    // class Edge: CFG edge 
    //=========================================================================================================
    /** class Edge represents an edge in a control flow graph */
    class Edge {

    public:

        /** enum Kind represents dynamic type info of Edge and descendants.
        This enumeration is hierarchical and is used in getKind and hasKind Edge methods
        */
        enum Kind{
            Kind_Edge=0xffffffff,
            Kind_CatchEdge=0xf,
        };

        /** returns the kind of the edge representing its class */
        Kind            getKind()const {return kind;}
        /** returns true if the edge is of kind (class) k or its subclass */
        bool            hasKind(Kind k)const {return (kind&k)==kind;}
        /** returns the node the edge is connected to at the edge's point defined by dir */
        Node *          getNode(Direction dir)const {return nodes[dir];}


        /**    Returns true if both head and tail of an edge are basic blocks */
        bool            isBlockEdge()const;
        /**  Returns true if an edge is a back edge */
        bool            isBackEdge()const;
        /**  Returns true if an edge is a loop exit edge */
        bool            isLoopExit()const;

        /**  Returns true if an edge is a fall-through edge */
        bool            isFallThroughEdge()const;

        /**  Returns true if an edge is a direct branch edge */
        bool            isDirectBranchEdge()const;

        /** returns the probability value associated with the edge */
        ProbValue       getProbability()const { return probability; }
        /** Associates a probability value with the edge */
        void            setProbability(ProbValue prob) { probability = prob; }

        /** returns the BranchInst this edge corresponds to.
        The tail node of the edge must be BasicBlock and the edge must be set as the block's direct branch.
        */
        BranchInst *    getBranch()const;

        //---------------------------------------------------------------------------------------------
    protected: 
        Edge(Node *from, Node *to, ProbValue prob = UnknownProbValue)
            : kind(Kind_Edge), probability(prob) 
        { assert(from!=NULL && to!=NULL); nodes[Direction_Backward]=from;  nodes[Direction_Forward]=to; }

        void setKind(Kind k){ kind=k; }

        //---------------------------------------------------------------------------------------------
    protected:
        Kind            kind;
        Node *          nodes[2];
        Dlink           links[2];
        ProbValue       probability; // Probability this edge is taken once control reaches tail.

        //---------------------------------------------------------------------------------------------
        friend class Edges;
        friend class CFG;
        friend class Node;
    };


    //=========================================================================================================
    // class Edges: edge collection
    //=========================================================================================================
    /** class Edge is a collection of edges. 

    Emulates NULL-terminated iteration.

    Each CFG node contain two Edges collection (In and Out)
    */
    class Edges: private Dlink
    {
    public:
        /** Constructs either Direction_In or Direction_Out collection of edges 

        The Direction value is used for internal needs and represent the corresponding Dlink element in an edge
        */
        Edges(Direction dir=Direction_Backward): direction(dir), count(0) {}
        /** returns the first edge in the collection or NULL if the collection is empty */
        Edge *  getFirst()const {return toEdge(Dlink::getNext());}
        /** returns the last edge in the collection or NULL if the collection is empty */
        Edge *  getLast()const  {return toEdge(Dlink::getPrev());}
        /** returns the edge next to the specified edge or NULL if there are no such edges in the collection */
        Edge *  getNext(const Edge * edge)const     {return toEdge(edge->links[direction].getNext());}
        /** returns the edge previous to the specified edge or NULL if there are no such edges in the collection */
        Edge *  getPrev(const Edge * edge)const     {return toEdge(edge->links[direction].getPrev());}
        /** returns true if the collection is empty */
        bool    isEmpty()const{ bool empty = Dlink::getNext()==this; assert(!empty||count==0); return empty; }
        /** returns the Direction value of the collection set in its constructor */
        uint32  getDirection()const {return direction;}
        /** returns the number of edges in the collection */
        uint32  getCount()const     {return count;}
        /** returns true if the collection contains the edge */
        bool has(const Edge * edge)const;
    private:
        Edges(const Edges& r){ assert(0); } // one should write "const Edges& es=..." instead of "Edges es=..."
        Edges& operator=(const Edges& r){ assert(0); return *this; }

        void append(Edge* edge){ edge->links[direction].insertBefore(this); count++; }
        void prepend(Edge* edge){ edge->links[direction].insertAfter(this); count++; }
        void del(Edge* edge){ edge->links[direction].unlink(); count--; }
        Edge * toEdge(Dlink * link)const
        { return link==this?0:(Edge*)((char*)link-offsetof(Edge, links[direction])); }

        Direction   direction;
        uint32      count;

        friend class Node;
    };


    //=========================================================================================================
    //  edge with the exception information
    //=========================================================================================================
    /** class CatchEdge is specialization of Edge representing an edge 
    from a dispatch node to a handler block 
    */
    class CatchEdge : public Edge 
    {

    public:
        /** Returns the caught exception type associated with the edge */
        Type *  getType()const {return type;}
        /** Returns the priority of the edges during exception handling 

        If the same exception can be handled by several
        exception handlers, the handler with the highest priority handles it.
        The smaller the priority number the hight is the priority.  
        */
        uint32  getPriority()const {return priority;}

        //---------------------------------------------------------------------------------------------
    protected: 
        CatchEdge(DispatchNode * from, BasicBlock * to, Type *ty, uint32 prior,
            ProbValue prob = UnknownProbValue)
            : Edge((Node*)from, (Node*)to, prob), type(ty), priority(prior) { setKind(Kind_CatchEdge); }

            //---------------------------------------------------------------------------------------------
    protected:
        /** The type of a caught exception */
        Type *      type;    

        /**  Priority of the edge. If the same exception can be handled by several
        exception handlers, the handler with the highest priority handles it.
        The smaller the priority number the hight is the priority.
        */
        uint32      priority;

        //---------------------------------------------------------------------------------------------
        friend class CFG;
        friend class Node;
    };


    //=========================================================================================================
    //  node
    //=========================================================================================================
    /** class Node is a base class for all nodes in the CFG.*/
    class Node {

        //---------------------------------------------------------------------------------------------
    public:
        /** enum Kind represents dynamic type info of Node and descendants.
        This enumeration is hierarchical and is used in getKind and hasKind Node methods
        */

        enum Kind
        {
            //      Kind_Node=0xffffffff, must not exist
            Kind_BasicBlock=0xffff,
            Kind_DispatchNode=0xf0000,
            Kind_UnwindNode=0x10000000,
            Kind_ExitNode=0x20000000
        };

        //---------------------------------------------------------------------------------------------
        /** Returns the ID of the node */
        uint32              getId()const {return id;}
        /** Returns the kind of the node representing its class */
        Kind                getKind()const{ return kind; }
        /** Returns true if the node is of kind (class) k or its subclass */
        bool                hasKind(Kind k)const{ return (kind&k)==kind; }

        /** returns the CFG this node is in */
        CFG *               getCFG()const{ return cfg; }

        
        /** Returns the ExecCntValue DPGO value */
        ExecCntValue        getExecCnt()const    { return execCnt; }
        /** Sets the ExecCntValue DPGO value */
        void                setExecCnt(ExecCntValue ec) { execCnt = ec; }

        /** Returns either Direction_In or Direction_Out collection of edges */
        const Edges&        getEdges(Direction dir)const
        { return edges[dir]; }

        /**  Returns true if this node is connected to a node of the specified kind. */
        bool                isConnectedTo(Direction dir, Kind k)const
        { return getEdge(dir, k)!=NULL; }
        /**  Returns true if this node is connected to the specified node. */
        bool                isConnectedTo(Direction dir, Node * node)const
        { return getEdge(dir, node)!=NULL; }

        /**  Returns the edge leading to the first node of the specified kind */
        Edge *          getEdge(Direction dir, Kind k)const;
        /**  Returns the edge leading to the specified node */
        Edge *          getEdge(Direction dir, Node * node)const;

        /**  Returns the first node of the specified kind connected with this node */
        Node *          getNode(Direction dir, Kind k)const
        {   Edge * e=getEdge(dir, k); return e!=NULL?e->getNode(dir):NULL; }

        /** Returns true if the node is a loop header.
        Loop info must be set before calling this method (using setLoopInfo).
        */
        bool                isLoopHeader() const;
        /** Returns true the loop header node for the node (if the node is withing a loop).
        Loop info must be set before calling this method (using setLoopInfo).
        */
        Node *              getLoopHeader() const;
        /** Returns the depth of the loop this node is in. */
        uint32              getLoopDepth()const;
        /** Returns true if the node is within a loop. */
        bool                isWithinLoop(const Node * loopHeader)const;
        /** Sets loop info for the node.
        The loop info is set during CFG lowering (CfgCodeSelector)
        */

        void                setLoopInfo(bool _isLoopHeader, Node * header)
        { nodeIsHeader = _isLoopHeader; loopHeader = header; }
        /** Returns true if loop info has been set for the node */
        bool                hasLoopInfo() const;
        /** Returns the persistent id of the node (HIR CFG node id resulting to this node) */
        uint32              getPersistentId()const {return persistentId; }
        /** Sets the persistent id of the node (HIR CFG node id resulting to this node) */
        void                setPersistentId(uint32 persId) { persistentId = persId; }

        virtual void        verify();

        //---------------------------------------------------------------------------------------------
    protected: 

        Node(CFG * cfg, uint32 id, ExecCntValue cnt = UnknownExecCnt);

        //---------------------------------------------------------------------------------------------
        void addEdge(Direction dir, Edge * edge);
        virtual void delEdge(Direction dir, Edge * edge);

        LiveSet *           getLiveAtEntry()const {return liveAtEntry;}
 
        void setKind(Kind k){ kind=k; }

        //---------------------------------------------------------------------------------------------
    protected:
        Kind                kind;
        uint32              id;
        Edges               edges[2];

        CFG *               cfg;

        ExecCntValue        execCnt;        // Estimate for # times this node is entered
        uint32              persistentId;   // persistent id

        //  Loop information
        bool                nodeIsHeader;   // is this node a loop header
        Node *              loopHeader;     // header of the containing loop, NULL if none.
        
        LiveSet *           liveAtEntry;    // operands live at node entry

        //---------------------------------------------------------------------------------------------
        friend class CFG;
        friend class IRManager;
        friend class Edge;
    };

    //=========================================================================================================
    //    Exception dispatch node
    //=========================================================================================================
    /** class DispatchNode is a node collecting control flow for handled exceptions */
    class DispatchNode : public Node 
    {
    public:

        /** Auxiliary method, returns true if the catch edges are sorted */
        bool catchEdgesAreSorted()const;
        void sortCatchEdges();

        //---------------------------------------------------------------------------------------------
    protected: 
        DispatchNode(CFG * cfg, uint32 id, ExecCntValue cnt = UnknownExecCnt) 
            :Node(cfg, id, cnt) { setKind(Kind_DispatchNode); }

    private:

        //---------------------------------------------------------------------------------------------
        friend class CFG;
        friend class Edge;
        friend class Node;
    };

    //=========================================================================================================
    //    Unwind node
    //=========================================================================================================
    /** class DispatchNode is the only node in a CFG collecting control flow for unhandled exceptions */
    class UnwindNode : public Node 
    {
    protected: 
        UnwindNode(CFG * cfg, uint32 id, ExecCntValue cnt = UnknownExecCnt) :Node(cfg, id, cnt) 
        {setKind(Kind_UnwindNode); }
        //---------------------------------------------------------------------------------------------
        friend class CFG;
    };

    //=========================================================================================================
    //    Exit node
    //=========================================================================================================
    /** class DispatchNode is the exit node of a CFG */
    class ExitNode : public Node 
    {
    protected: 
        ExitNode(CFG * cfg, uint32 id, ExecCntValue cnt = UnknownExecCnt) :Node(cfg, id, cnt) 
        {setKind(Kind_ExitNode); }
        //---------------------------------------------------------------------------------------------
        friend class CFG;
    };


    //=========================================================================================================
    // class Insts: inst double linked list
    //=========================================================================================================
    /** Collection of instructions. Emulates NULL-terminated iteration.
    Is used in basic blocks.
    */
    class Insts: private Dlink
    {
    public:
        /** constructs an empty Inst collection */
        Insts():count(0) {}


        /** returns the first instruction in the collection or NULL if the collection is empty */
        Inst * getFirst()const  {return toInst(Dlink::getNext());}
        /** returns the last instruction in the collection or NULL if the collection is empty */
        Inst * getLast()const   {return toInst(Dlink::getPrev());}
        /** returns the instruction next to the specified instruction or 
        NULL if there are no such instruction in the collection */
        Inst * getNext(const Inst * inst)const  {return toInst(((Dlink*)inst)->getNext());}
        /** returns the instruction previous to the specified instruction or 
        NULL if there are no such edges in the collection */
        Inst * getPrev(const Inst * inst)const  {return toInst(((Dlink*)inst)->getPrev());}

        /** returns the first inst if dir is Direction_Forward or the 
        last inst if dir is Direction_Backward*/
        Inst * getFirst(Direction dir)const 
        { return dir==Direction_Forward?getFirst():getLast(); }

        /** returns the next inst if dir is Direction_Forward or the 
        last inst if dir is Direction_Backward*/
        Inst * getNext(Direction dir, const Inst * inst)const 
        { return dir==Direction_Forward?getNext(inst):getPrev(inst); }

        /** returns true if the collection is empty */
        bool isEmpty()const{ bool empty = Dlink::getNext()==this; assert(!empty||count==0); return empty; }
        /** returns the number of instructions in the collection */
        uint32  getCount()const     {return count;}

        static void connectInstLists(Inst * inst1, Inst * inst2) 
        {   inst1->_next = inst2;  inst2->_prev = inst1;   }
    private:
        Insts(const Insts& r){ assert(0); } // one should write "const Insts& is=..." instead of "Insts is=..."
        Insts& operator=(const Insts& r){ assert(0); return *this; }

        void append(Inst* inst, Inst * after=0 )
        { if (after==0) after=(Inst*)Dlink::getPrev(); inst->insertAfter(after); count++; }
        void prepend(Inst* inst, Inst * before=0 )
        {  if (before==0) before=(Inst*)Dlink::getNext(); inst->insertBefore(before); count++; }
        void del(Inst* inst){ inst->unlink(); count--; }

        Inst * toInst(Dlink * link)const
        { return link==this?0:(Inst*)link; }

        uint32      count;

        friend class BasicBlock;
    };

    //=========================================================================================================
    //  Basic block
    //=========================================================================================================
    /** class BasicBlock represents basic blocks of a CFG: nodes which can contain intructions */
    class BasicBlock : public Node {

    public:
        /** Returns the Insts collection with instructions of this basic block */
        const Insts&    getInsts()const {return insts;}
        /** Returns true if the basic block contains no instruction */
        bool            isEmpty()const { return insts.isEmpty(); }

        /** Appends instList to the basic after instruction after 
        if after is provided it must be bound to this basic block
        */
        void appendInsts(Inst * instList, Inst * after=0);

        /** Prepends instList to the basic block bb bevore instruction before 
        if before is provided it must be bound to this basic block
        */
        void prependInsts(Inst * instList, Inst * before=0);

        /** Removes the inst from the basic block */
        void removeInst(Inst * inst);

        /** Returns the fall-through outgoing edge of this basic block */
        Edge *          getFallThroughEdge()const{ return fallThroughEdge; }
        /** Returns the outgoing edge of this basic block associated with a direct branch 
        The direct branch must be the last instruction in the block
        */
        Edge *          getDirectBranchEdge()const{ return directBranchEdge; }

        /** Marks the specified edge as a fall-through edge */
        void makeEdgeFallThrough(Edge * edge);
        /** Marks the specified edge as a direct branch edge */
        void makeEdgeDirectBranch(Edge * edge);

        /** recalculates the probability of the fall-through edge 
        as 1 - (sum for other edges) */
        void recalculateFallThroughEdgeProbability();

        BasicBlock * getIndirectBranchPredecessor()const;

        bool    canBeRemoved()const
        { return getIndirectBranchPredecessor() == NULL; }

        /** Returns the basic block which is the layout successor of this one 
        The returned value must be set using setLayoutSucc by a code layout algorithm
        */
        BasicBlock *        getLayoutSucc()const {return layoutSucc;}

        /** Sets the basic block which is the layout successor of this one  */
        void                setLayoutSucc(BasicBlock *bb);

        /** sets the offset of native code for this basic block */
        void            setCodeOffset(uint32 offset) {codeOffset = offset;}
        /** returns the offset of native code for this basic block */
        uint32          getCodeOffset()const    {   return codeOffset;  }
        /** sets the size of native code for this basic block */
        void            setCodeSize(uint32 size) {codeSize = size;}
        /** returns the size of native code for this basic block */
        uint32          getCodeSize()const  {   return codeSize;    }

        /** returns the pointer to the native code for this basic block */
        void * getCodeStartAddr()const;

        void fixBasicBlockEndInstructions();

        void    verify();
    //---------------------------------------------------------------------------------------------
    protected: 
        BasicBlock::BasicBlock(CFG * cfg, uint32 id, ExecCntValue cnt) 
            : Node(cfg, id, cnt), layoutSucc(NULL), codeOffset(0), codeSize(0), fallThroughEdge(0), directBranchEdge(0)
        {setKind(Kind_BasicBlock); }

        //  Clone block's skeleton, i.e. create new basic block of the same type for the same cfg
        virtual BasicBlock *  cloneSkeleton(CFG * cfg, uint32 nIn, uint32 nOut);

        virtual void delEdge(Direction dir, Edge * edge);

    protected:
        Insts           insts;

        BasicBlock *    layoutSucc; 

        uint32          codeOffset;
        uint32          codeSize;

        Edge *          fallThroughEdge;
        Edge *          directBranchEdge;



        //---------------------------------------------------------------------------------------------
        friend class Edge;
        friend class CFG;
    };


    //=========================================================================================================
    // class CFG
    //=========================================================================================================
    /** class CFG represents a control flow graph */
    class CFG
    {
    public:
        /** enum OrderType specifies order used in CFG traversing by visitors and iterators */
        enum OrderType {
            OrderType_Arbitrary=0,
            OrderType_Layout,
            OrderType_Postorder,
            OrderType_Topological, 
                OrderType_ReversePostorder = OrderType_Topological
        };

        //-------------------------------------------------------------------------------------
        /** Creates a CFG instance which uses memManager to create its elements */
        CFG(MemoryManager& memManager);

        //-------------------------------------------------------------------------------------
        /** Returns the memory manager used for LIR */
        MemoryManager & getMemoryManager()const{ return memoryManager; }

        /** Returns the prolog node of the CFG */
        BasicBlock *    getPrologNode()const{ return prologNode; }
        /** Returns the exit node of the CFG */
        ExitNode *      getExitNode()const { return exitNode; } 
        /** Returns the Unwind node of the CFG */
        UnwindNode *    getUnwindNode()const { return unwindNode; } 

        /** returns the number of nodes in the CFG */
        uint32          getNodeCount()const{ return nodes.size(); }

        /** returns the maximum node id assigned to a node in this CFG */
        uint32          getMaxNodeId()const{ return nodeId; }

        /** returns true if the CFG has been laid out 
        (layout successor attributes of nodes are valid) */
        bool            isLaidOut()const{ return _isLaidOut; }
        void            setIsLaidOut(){ _isLaidOut=true; }

        /** returns true if the specified node is an epilog basic block 
        A node is considered epilog if it is a basic block and is connected to the exit node of the CFG 
        */
        bool            isEpilog(const Node * node)const
        { return node->hasKind(Node::Kind_BasicBlock) && node->isConnectedTo(Direction_Out, exitNode); }

        //-------------------------------------------------------------------------------------
        /** Creates a new unwind node and adds it to the control flow graph
        We create unwind nodes in the same CFG
        */
        UnwindNode *        newUnwindNode(ExecCntValue cnt = UnknownExecCnt);
        /** Creates a new basic lock and adds it to the control flow graph */
        BasicBlock *        newBasicBlock(ExecCntValue cnt = UnknownExecCnt);
        /** Creates a new dispatch node and adds it to the control flow graph */
        DispatchNode *      newDispatchNode(ExecCntValue cnt = UnknownExecCnt);

        /** Creates a new CFG edge and addsit to the control flow graph */
        Edge *              newEdge(Node *from, Node *to, ProbValue prob);

        /** Creates a new CFG edge which correspond to the taken direct branch control flow
        and adds it to the control flow graph */
        Edge *              newDirectBranchEdge(BasicBlock *from, BasicBlock * to, ProbValue prob)
        { Edge * edge = newEdge(from, to, prob); from->makeEdgeDirectBranch(edge); return edge; }

        /** Creates a new CFG edge which correspond to the taken direct branch control flow
        and adds it to the control flow graph */
        Edge *              newFallThroughEdge(BasicBlock *from, BasicBlock * to, ProbValue prob)
        { Edge * edge = newEdge(from, to, prob); from->makeEdgeFallThrough(edge); return edge; }

        /** Creates a new catch edge (thus adding it to the control flow graph) */
        CatchEdge *     newCatchEdge(DispatchNode *from, BasicBlock *to, Type *ty, uint32 prior, ProbValue prob);

        //-------------------------------------------------------------------------------------
        class NodeIterator;

        

         const Nodes& getNodesPostorder() const;
         const Nodes& getNodes() const {return nodes;}

         //_________________________________________________________________________________________________
         template <class Container>
         void getNodes(Container& container, OrderType orderType) const {
             switch(orderType){
                case OrderType_Arbitrary:
                    for (Nodes::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; it++) {
                        Node* node = *it;
                        container.push_back(node);
                    }
                    break;
                case OrderType_Layout:
                    assert(isLaidOut());
                    for (BasicBlock * bb=getPrologNode(); bb!=NULL; bb=bb->getLayoutSucc()) {
                        container.push_back(bb);
                    } 
                    break;
                case OrderType_Postorder: 
                case OrderType_ReversePostorder: //same as topological -> get postorder and reverse..
                {
                    const Nodes& postOrder = getNodesPostorder();
                    if (orderType == OrderType_ReversePostorder) {
                        container.insert(container.end(), postOrder.rbegin(), postOrder.rend());
                    } else {
                        container.insert(container.end(), postOrder.begin(), postOrder.end());
                    }
                }
                break;
             }
         }


        //-------------------------------------------------------------------------------------
        /** Retargets an existed CFG edge to the new target node */
        void    retargetEdge(Direction dir, Edge *edge, Node *newTo);

        //-----------------------------------------------------------------------------------------------
        /** splits the basic block after inst, adds the new basic block to CFG, 
        links it with other blocks appropriately, and returns bb */
        BasicBlock * splitBlockAfter(BasicBlock * bb, Inst * inst);

        /** merges sequential blocks according to some rules (TBD) */
        void mergeSequentialBlocks();

        /** remove empty basic blocks */
        void purgeEmptyBlocks();

        /** remove empty basic block bb */
        void purgeEmptyBlock(BasicBlock * bb);

        //-----------------------------------------------------------------------------------------------
        /** removes node and all connected edges from the CFG */
        void removeNode(Node * node);

        /** removes edge from the CFG */
        void removeEdge(Edge * edge);

        /** manual control for loop info validity, can be removed later*/
        void invalidateLoopInfo() { lastLoopInfoVersion = 0; }

        bool isLoopInfoValid() const {return graphModCount == lastLoopInfoVersion;}

        /** could be used as optimization if algorithms modifies graph and maintains valid loop info by itself. */
        void forceLoopInfoIsValid() {lastLoopInfoVersion = graphModCount; assert(ensureLoopInfoIsValid());} 
            
        /** tmp solution(to discuss) : the problem is that non-friend to CFG class Node 
            needs in API to incModCount on edges modification */
        void incModCount() {graphModCount++;}

        /** recalculates loop info if necessary
            loop info is valid after method call*/
        void updateLoopInfo(bool force = FALSE); 
        
        bool hasLoops() const { 
            return getMaxLoopDepth()!= 0;
        }

        uint32 getMaxLoopDepth() const {
            assert(isLoopInfoValid());
            return maxLoopDepth;
        }

        /** recalculates exec counts, using edge probs, tries to fix edge probs if illegal*/ 
        void updateExecCounts();

        //-------------------------------------------------------------------------------------
        void * getCodeStartAddr(uint32 sectionId=0)const 
        { return codeStartAddr; }

        void    setCodeStartAddr(void * addr, uint32 sectionId=0)
        { codeStartAddr=addr; }

        //-------------------------------------------------------------------------------------
        /** adds nodes to CFG, changes node owner and ids. */
        void importNodes(const Nodes& nodesToAdd);

		/** Inserts childCFG into parent CFG right after or before (insertAfter param) of insertLocation
			Note: Be sure that childCFG's insts are created with the same IRManager as parentCFG's insts.
			Note: childCFG is unusable after this call, its nodes  imported to parentCFG
		*/
	    void mergeGraphs(CFG* parentCFG, CFG* childCFG,  Inst* insertLocation, bool insertAfter);

        /** Inlines CFG into edge's (parent) CFG between edge's tail and head
            Note: Be sure that childCFG's insts are created with the same IRManager as parentCFG's insts.
            Note: inlined CFG is unusable after this call, its nodes  imported to edge's (parent) CFG
        */
        void mergeGraphAtEdge(CFG* cfg, Edge* edge, bool takeParentDispatch=true);
	
        void setHasEdgeProfile(bool val) { edgeProfile = val;}
        bool hasEdgeProfile() const { return edgeProfile;}

    protected:
		/** retargets all dir-edges of src to be in-edges of dst. No fallthru or direct edge info is lost 
         *  dispatch edges are retargeted too. 
         *  While using this function be careful with catch edges, dispatch nodes and switch targets.
         */
        static void retargetAllEdges(Direction dir, Node* oldNode, Node* newNode);
        
        /** methods do not tracks graph mod-count -> for internal CFG use only!*/
        void            addNode(Node* node){ nodes.push_back(node); incModCount(); }
        void            delNode(Node* node){ nodes.erase(std::find(nodes.begin(), nodes.end(), node)); incModCount(); }

        ExitNode * newExitNode(ExecCntValue cnt=UnknownProbValue);

        void tryMergeSequentialBlocks(Node * node);

        void getNodesDFS(Nodes& c, Node* node, Direction edgeDir) const;
        
        struct DFSLoopInfo {
            int color : 8;
            int dfn : 24;
        };
        /** Updates loopHeader info for all nodes accessible from node.
            Heavily depends on valid 'info' and 'outerLoopHeaders' contents -> should be used only
            to recalculate loop info for a whole CFG, starting from prologue node.
         */
        bool doLoopInfoDFS(Node* node, DFSLoopInfo* info, NodeList& outerLoopHeaders) const;

        /** debug check for loop info, should be used inside asserts
        recalculates loop-info and returns TRUE if OK*/
        bool ensureLoopInfoIsValid();



        //-------------------------------------------------------------------------------------
        MemoryManager           &   memoryManager;

        Nodes                       nodes;
        mutable Nodes               postorderNodesCache;
        uint32                      nodeId;
        BasicBlock              *   prologNode;
        ExitNode                *   exitNode;
        UnwindNode              *   unwindNode;

        
        /** number of graph modification: nodes (add/remove) and edges(add/remove/retarget) */        
        uint32                  graphModCount;
        /** graph mod count value when loop info was calculated */
        uint32                  lastLoopInfoVersion;
        mutable uint32          lastPostorderVersion;
        bool                    _isLaidOut;

        uint32                  maxLoopDepth;

        void *                  codeStartAddr;
        mutable StlVector<int>  traversalInfo;

        bool                    edgeProfile;
   };

    //=========================================================================================================
    // class CFG::NodeIterator
    //=========================================================================================================
    /** 
    class CFG::NodeIterator is an alternative CFG traversal interface
    which is more convenient than visitor

    The NodeIterator is a forward-only iterator

    One creates an iterator using its constructor and calls its ++ operator to gen access to the next node

    Usage pattern:
    for(CFG::NodeIterator it(irManager, CFG::OrderType_Postorder); it!=NULL; it++) {
        Node * node=it.getNode();
    }
    */
    class CFG::NodeIterator {
        const Nodes* nodes;
        int currentIdx, borderIdx, increment;
    public:
        /** gets the current node */
        Node *  getNode()const { return currentIdx != borderIdx ? (*nodes)[currentIdx]: NULL;} 
        /** increments the iterator */
        NodeIterator& operator++(){ if (currentIdx!=borderIdx) currentIdx+=increment; return *this;}
        operator Node*()const{ return getNode(); }
        void rewind();

        /** creates an iterator traversing CFG in the orderType order */
        NodeIterator(const CFG& fg, OrderType orderType=OrderType_Arbitrary);
        ~NodeIterator(){};
    private:
        NodeIterator(const NodeIterator& r)  { assert(0); } // should not copy NodeIterator
        NodeIterator& operator=(const NodeIterator& r){ assert(0); return *this; }
        //----------------------------------------------------------------------------------
        
        friend class CFG;
    };


    //=========================================================================================================

}; //namespace Ia32
}
#endif // _IA32_FLOWGRAPH_H
