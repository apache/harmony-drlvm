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
 * @version $Revision: 1.17.12.2.4.4 $
 */

#include "Log.h"
#include "Ia32CFG.h"
#include "Ia32Inst.h"
namespace Jitrino
{
namespace Ia32{

    //=========================================================================================================
    //                      CFG Edge
    //=========================================================================================================

	//_________________________________________________________________________________________________________
    bool Edge::isBlockEdge()const
	{
		return nodes[Direction_Head]->hasKind(Node::Kind_BasicBlock) &&
			nodes[Direction_Tail]->hasKind(Node::Kind_BasicBlock);
	}

	//_________________________________________________________________________________________________________
    bool Edge::isBackEdge()const
    { 
		Node * head=nodes[Direction_Head];
        return head->isLoopHeader() && getNode(Direction_Tail)->isWithinLoop(head);
    }

	//_________________________________________________________________________________________________________
	bool Edge::isFallThroughEdge()const
	{ 
		return isBlockEdge() && ((BasicBlock*)nodes[Direction_In])->getFallThroughEdge()==this; 
	}
	
	//_________________________________________________________________________________________________________
	bool Edge::isDirectBranchEdge()const
	{ 
		return isBlockEdge() && ((BasicBlock*)nodes[Direction_In])->getDirectBranchEdge()==this; 
	}
	
	
	//_________________________________________________________________________________________________________
    /**  Check if an edge is a loop exit edge */
    bool Edge::isLoopExit() const
    {
		Node * tail=nodes[Direction_Tail];
		Node * tailLoopHeader = tail->isLoopHeader() ? tail : tail->getLoopHeader();
		return tailLoopHeader && !getNode(Direction_Head)->isWithinLoop(tailLoopHeader);
    }

    //_________________________________________________________________________________________________________
    BranchInst * Edge::getBranch()const
    {
        Node * node=getNode(Direction_Tail);
        if (!node->hasKind(Node::Kind_BasicBlock))
            return NULL;
        BasicBlock * bb=(BasicBlock*)node;
        if (bb->getDirectBranchEdge()!=this)
            return NULL;
        Inst * inst=bb->getInsts().getLast();
        assert(inst && inst->hasKind(Inst::Kind_BranchInst) && ((BranchInst*)inst)->isDirect());
        return (BranchInst*)inst;
    }

    //=========================================================================================================
    //                       Edges
    //=========================================================================================================
    //_________________________________________________________________________________________________________
    bool Edges::has(const Edge * edge)const
    {
	Edge * e = NULL;
        for (e=getFirst(); e!=NULL && e!=edge; e=getNext(e));
        return e==edge;
    }

    //=========================================================================================================
    //                       Node 
    //=========================================================================================================

    //_________________________________________________________________________________________________________
    /**  Construct a node */

    Node::Node(CFG * cfg, uint32 _id, ExecCntValue cnt)
        : id(_id), cfg(cfg), 
        execCnt(cnt), persistentId(EmptyUint32)
        ,  nodeIsHeader(false), loopHeader(NULL)
    { 
        edges[Direction_Backward].direction=Direction_Backward; 
        edges[Direction_Forward].direction=Direction_Forward; 
        liveAtEntry = new (cfg->getMemoryManager()) LiveSet(cfg->getMemoryManager(), 0);
        cfg->incModCount();
    }

    //_________________________________________________________________________________________________________
    Edge * Node::getEdge(Direction dir, Kind k)const
    {
        const Edges & es=edges[dir];
    	Edge * e = NULL;
        for (e=es.getFirst(); e!=NULL && !e->getNode(dir)->hasKind(k); e=es.getNext(e));
        return e;
    }

   //_________________________________________________________________________________________________________
    Edge * Node::getEdge(Direction dir, Node * node)const
    {
        const Edges & es=edges[dir];
    	Edge * e = NULL;
        for ( e=es.getFirst(); e!=NULL && e->getNode(dir)!=node; e=es.getNext(e));
        return e;
    }

    //_________________________________________________________________________________________________________
    void Node::addEdge(Direction dir, Edge * edge)
    {
        assert(edge->getNode(dir==Direction_Out?Direction_Tail:Direction_Head)==this);
        assert(edge->hasKind(Edge::Kind_CatchEdge) || !isConnectedTo(dir, edge->getNode(dir))); // such edge already exists
        cfg->incModCount();
        edges[dir].append(edge);
    }

    //_________________________________________________________________________________________________________
    void Node::delEdge(Direction dir, Edge * edge)
    {
        assert(edge->getNode(dir==Direction_Out?Direction_Tail:Direction_Head)==this);
        assert(edges[dir].has(edge));
        cfg->incModCount();
        edges[dir].del(edge);
    }

    //_________________________________________________________________________________________________________
    bool Node::isLoopHeader() const {
       assert(cfg->isLoopInfoValid());
       return nodeIsHeader;
    }

    //_________________________________________________________________________________________________________
    Node* Node::getLoopHeader() const {
       assert(cfg->isLoopInfoValid());
       return loopHeader;
    }
    //_________________________________________________________________________________________________________
    bool Node::hasLoopInfo() const {
         return cfg->isLoopInfoValid();
    }
    //_________________________________________________________________________________________________________
    /**  Compute loop depth */
    uint32 Node::getLoopDepth() const
    {
		uint32 depth =  0;
		for (const Node * h=getLoopHeader(); h != NULL; h = h->getLoopHeader()) depth++;
		return depth;
    }

    //_________________________________________________________________________________________________________
    /**  Check if a node is contained (directly or nested) in a loop headed by loopHeader. */
    bool Node::isWithinLoop(const Node * loopHeader) const
    {
        const Node * h=this;
		for (; h != NULL && h != loopHeader; h = h->getLoopHeader());
		return h == loopHeader;
    }

    //_________________________________________________________________________________________________
	void Node::verify()
	{
		const Edges& inEdges=getEdges(Direction_In);
		const Edges& outEdges=getEdges(Direction_Out);
		for (Edge * edge=inEdges.getFirst(); edge!=NULL; edge=inEdges.getNext(edge))
			assert(edge->getNode(Direction_Tail)->isConnectedTo(Direction_Out, this));
		for (Edge * edge=outEdges.getFirst(); edge!=NULL; edge=outEdges.getNext(edge))
			assert(edge->getNode(Direction_Head)->isConnectedTo(Direction_In, this));
	}

    //=========================================================================================================
    //                     Exception Dispath Node
    //=========================================================================================================

    //_________________________________________________________________________________________________________
    /**    Check if catch edges are sorted */

    bool DispatchNode::catchEdgesAreSorted() const {
        const Edges& oEdges = getEdges(Direction_Out);
        uint32 idx = 0;
        for (Edge *edge = oEdges.getFirst() ;edge!=NULL; edge = oEdges.getNext(edge), idx++) {
            if (edge->hasKind(Edge::Kind_CatchEdge)  && ((CatchEdge*)edge)->getPriority()!=idx) {
                return false;
            }
        }
        return true;
    }


    //_________________________________________________________________________________________________________
    /**    Sort catch edges according to their priority */

    void DispatchNode::sortCatchEdges() {
        Edges& oEdges = edges[Direction_Out];
        uint32 numCatchEdges = 0;
        for (Edge *edge = oEdges.getFirst(); edge!=NULL; edge = oEdges.getNext(edge)) {
            if (edge->hasKind(Edge::Kind_CatchEdge)) {
               numCatchEdges++;
            }
        }
        if (numCatchEdges == 0) {
            return;
        }
        uint32 count = oEdges.getCount();
        StlVector<Edge*> sortedEdges(getCFG()->getMemoryManager(), count);
        uint32 nonCatchIdx = numCatchEdges;
        for (Edge *edge = oEdges.getFirst(), *nextEdge = NULL; edge!=NULL; edge = nextEdge) {
            nextEdge = oEdges.getNext(edge);                    
            delEdge(Direction_Out, edge);
            if (edge->hasKind(Edge::Kind_CatchEdge)) {
                uint32 idx = ((CatchEdge*)edge)->getPriority();
                assert(idx < numCatchEdges  && sortedEdges[idx] == NULL);
                sortedEdges[idx] = edge;
            } else { // non-catch edges moved to the end of the list
                assert(sortedEdges[nonCatchIdx] == NULL);
                sortedEdges[nonCatchIdx] = edge;
                nonCatchIdx++;
            }
        }
        for (uint32 i = 0; i<count; i++) {
            assert(sortedEdges[i]!=NULL);
            addEdge(Direction_Out, sortedEdges[i]);
        }
        assert(catchEdgesAreSorted());
    }

    
    //=========================================================================================================
    //                      Basic Block
    //=========================================================================================================

    //_________________________________________________________________________________________________________
    void BasicBlock::makeEdgeFallThrough(Edge * edge)
    {
        assert(edges[Direction_Head].has(edge));
        assert(edge->getNode(Direction_Head)->hasKind(Node::Kind_BasicBlock));
        if (directBranchEdge==edge)
            directBranchEdge=fallThroughEdge;
        fallThroughEdge=edge;
        assert(layoutSucc==NULL||layoutSucc==fallThroughEdge->getNode(Direction_Head));
    }

    //_________________________________________________________________________________________________________
    void BasicBlock::makeEdgeDirectBranch(Edge * edge)
    {
        assert(edges[Direction_Head].has(edge));
        assert(edge->getNode(Direction_Head)->hasKind(Node::Kind_BasicBlock));
        assert(!isEmpty());
        assert(getInsts().getLast()->hasKind(Inst::Kind_BranchInst) && ((BranchInst*)getInsts().getLast())->isDirect());
        if (fallThroughEdge==edge){
            fallThroughEdge=directBranchEdge;
            assert(fallThroughEdge==NULL||layoutSucc==NULL||layoutSucc==fallThroughEdge->getNode(Direction_Head));
        }
        directBranchEdge=edge;
    }

    //_________________________________________________________________________________________________________
	void BasicBlock::recalculateFallThroughEdgeProbability()
	{
		Edge * fallEdge=getFallThroughEdge();
		const Edges& outEdges=getEdges(Direction_Out);
		ProbValue p=1.0;
		for (Edge * edge=outEdges.getFirst(); edge!=NULL; edge=outEdges.getNext(edge)){
            if (edge!=fallEdge) 
 				p-=edge->getProbability();
		}
		if (p<=0.0) p=0.00001;
		fallEdge->setProbability(p);
	}

    //_________________________________________________________________________________________________________
    void BasicBlock::delEdge(Direction dir, Edge * edge)
    {
    	if (dir == Direction_Out){
	        if (directBranchEdge==edge)
	            directBranchEdge=0;
	        if (fallThroughEdge==edge)
	            fallThroughEdge=0;
	    }
        Node::delEdge(dir, edge);
    }

    //_________________________________________________________________________________________________________
    BasicBlock *  BasicBlock::cloneSkeleton(CFG * cfg, uint32 nIn, uint32 nOut)
    {
        return 0;
    }

    //_________________________________________________________________________________________________
    void BasicBlock::appendInsts(Inst * instList, Inst * after)
    {
        if (instList==0)
            return;
        if (after==NULL)
            after=insts.getLast();
        assert(after==NULL||after->getBasicBlock()==this);
        Inst * inst=NULL, * nextInst=instList, * lastInst=instList->getPrev(); 
        do { 
            inst=nextInst;
            nextInst=inst->getNext();
			assert(inst->basicBlock==NULL);
            inst->unlink();
            inst->basicBlock=this; insts.append(inst, after); 
			after=inst;
        } while (inst!=lastInst);
    }

    //_________________________________________________________________________________________________
    void BasicBlock::prependInsts(Inst * instList, Inst * before)
    {
        if (instList==0)
            return;
        if (before==NULL)
            before=insts.getFirst();
        assert(before==NULL||before->getBasicBlock()==this);
        Inst * inst=NULL, * nextInst=instList, * lastInst=instList->getPrev(); 
        do{  
            inst=nextInst;
            nextInst=inst->getNext();
			assert(inst->basicBlock==NULL);
            inst->unlink();
            inst->basicBlock=this; insts.prepend(inst, before); 
        } while (inst!=lastInst);
    }

    //_________________________________________________________________________________________________
    void BasicBlock::removeInst(Inst * inst)
	{
		assert(inst->basicBlock==this);
		inst->basicBlock=NULL;
		insts.del(inst);
	}

    //_________________________________________________________________________________________________
	BasicBlock * BasicBlock::getIndirectBranchPredecessor()const
	{ 
		const Edges & inEdges = getEdges(Direction_In);
		for (Edge * edge=inEdges.getFirst(); edge!=NULL; edge=inEdges.getNext(edge)){
		    Node * node = edge->getNode(Direction_Tail);
		    if (!node->hasKind(Kind_BasicBlock))
		    	continue;
			BasicBlock * bb=(BasicBlock*)node;
			Inst * inst = bb->getInsts().getLast();
			if ( inst != NULL && inst->hasKind(Inst::Kind_BranchInst) && inst->getOpndCount() != 0 && inst->getOpnd(0)->isPlacedIn(OpndKind_Mem) )
			     return bb;
		}
		return NULL;
	}

    //_________________________________________________________________________________________________
    void BasicBlock::setLayoutSucc(BasicBlock *bb)
    {
        assert(bb!=this);
        layoutSucc=bb;
    }	

    //_________________________________________________________________________________________________
	void * BasicBlock::getCodeStartAddr()const
	{ 
		return (uint8*)cfg->getCodeStartAddr()+getCodeOffset(); 
	}

    //_________________________________________________________________________________________________
	void BasicBlock::fixBasicBlockEndInstructions()
	{
		const Insts& insts=getInsts();
		Inst * instMustBeLast=NULL;
		for (Inst * inst=insts.getLast(); inst!=NULL; inst=insts.getPrev(inst)){
			if (inst->hasKind(Inst::Kind_BranchInst)){
				instMustBeLast=inst;
				break;
			}
		}
		if (instMustBeLast){
			removeInst(instMustBeLast);
			appendInsts(instMustBeLast);
		}
	}

    //_________________________________________________________________________________________________
	void BasicBlock::verify()
	{
	#ifdef _DEBUG
		const Edges& outEdges=getEdges(Direction_Out);

		assert(fallThroughEdge==NULL || outEdges.has(fallThroughEdge));
		assert(directBranchEdge==NULL || outEdges.has(directBranchEdge));
		assert(directBranchEdge==NULL || (getInsts().getCount()>0 && getInsts().getLast()->hasKind(Inst::Kind_BranchInst)));
		assert(directBranchEdge == NULL || (getInsts().getLast()->getProperties()&Inst::Properties_Conditional)==0 || fallThroughEdge!=NULL);
	#endif
		Node::verify();
	}

    //=========================================================================================================
    //  CFG
    //=========================================================================================================

    //_________________________________________________________________________________________________
    CFG::CFG(MemoryManager& memManager)
        :memoryManager(memManager),  nodes(memManager), postorderNodesCache(memManager), nodeId(0),
        prologNode(0), exitNode(0), unwindNode(0),
        graphModCount(1), lastLoopInfoVersion(0),lastPostorderVersion(0), _isLaidOut(false),  
        maxLoopDepth(0), codeStartAddr(NULL), 
        traversalInfo(memManager), edgeProfile(false)
    {	
        prologNode=newBasicBlock();
		prologNode->setLoopInfo(false, NULL);
        newExitNode();
    }

    //_________________________________________________________________________________________________
    BasicBlock * CFG::newBasicBlock(ExecCntValue cnt)
    {
        BasicBlock * node=new(memoryManager) BasicBlock(this, nodeId++, cnt);
        addNode(node);
        return node;
    }

    //_________________________________________________________________________________________________
    UnwindNode * CFG::newUnwindNode(ExecCntValue cnt)
    {
        assert(unwindNode == NULL);
        unwindNode=new(memoryManager) UnwindNode(this, nodeId++, cnt);
        addNode(unwindNode);
        newEdge(unwindNode, exitNode, 1.0);
        return unwindNode;
    }

    //_________________________________________________________________________________________________
    ExitNode * CFG::newExitNode(ExecCntValue cnt)
    {
        assert(exitNode == NULL);
        exitNode=new(memoryManager) ExitNode(this, nodeId++, cnt);
        addNode(exitNode);
        return exitNode;
    }

    //_________________________________________________________________________________________________
    DispatchNode * CFG::newDispatchNode(ExecCntValue cnt)
    {
        DispatchNode * node=new(memoryManager) DispatchNode(this, nodeId++, cnt);
        addNode(node);
        return node;
    }

    //_________________________________________________________________________________________________
    Edge * CFG::newEdge(Node *from, Node *to, ProbValue prob)
    {
        Edge * edge;
#ifdef  _DEBUG
        edge = from->getEdge(Direction_Out, to);
        assert(!edge);
        assert(!hasEdgeProfile() || (prob>0.0 && prob<=1.0));
#endif
        edge=new(memoryManager) Edge(from, to, prob);
        from->addEdge(Direction_Out, edge);
        to->addEdge(Direction_In, edge);
        return edge;
    }

    //_________________________________________________________________________________________________
    CatchEdge *	CFG::newCatchEdge(DispatchNode * from, BasicBlock * to, Type *ty, uint32 prior, ProbValue prob)
    {
        Edge * edge;
#ifdef  _DEBUG
        edge = from->getEdge(Direction_Out, to);
        assert(!edge);
        assert(!hasEdgeProfile() || (prob>0.0 && prob<=1.0));
#endif

        edge=new(memoryManager) CatchEdge(from, to, ty, prior, prob);
        from->addEdge(Direction_Out, edge);
        to->addEdge(Direction_In, edge);
        return (CatchEdge *)edge;
    }


    //_________________________________________________________________________________________________
    void CFG::retargetEdge(Direction dir, Edge *edge, Node *newTo)
    {
        Direction nodeDir=dir==Direction_Head?Direction_In:Direction_Out;
        edge->getNode(dir)->delEdge(nodeDir, edge);
        edge->nodes[dir]=newTo;
        newTo->addEdge(nodeDir, edge);
		if (dir==Direction_Head && !newTo->hasKind(Node::Kind_BasicBlock)){
			Node * node = edge->getNode(nodeDir);
			if (node->hasKind(Node::Kind_BasicBlock)){
				BasicBlock * bbFrom=(BasicBlock*)node;
				if (bbFrom->fallThroughEdge==edge)
					bbFrom->fallThroughEdge=NULL;
				assert(!edge->isDirectBranchEdge());
			}
		}
    }

    //_________________________________________________________________________________________________
    void CFG::removeNode(Node * node)
    {
		for (uint32 i=0; i<2; i++){
            const Edges& es=node->getEdges(Direction(i));
			for (Edge * e=es.getFirst(), * ne=NULL; e!=NULL; e=ne){
				ne=es.getNext(e);
                removeEdge(e);
			}
        }
        delNode(node);
    }

    //_________________________________________________________________________________________________
	void CFG::mergeSequentialBlocks() {
		assert(!isLaidOut());
        for (Nodes::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; it++) {
            Node* node = *it;
			tryMergeSequentialBlocks(node);
		}
	}

	//_________________________________________________________________________________________________
	void CFG::tryMergeSequentialBlocks(Node * node)
	{
		if (!node->hasKind(Node::Kind_BasicBlock))
			return;
		BasicBlock * bb=(BasicBlock*)node;
		if (bb==prologNode)
			return;
		if (isEpilog(bb))
			return;
		if (!bb->canBeRemoved())
			return;

		const Edges & outEdges=bb->getEdges(Direction_Out);
        Edge* fallEdge =  bb->getFallThroughEdge();
        if (fallEdge==NULL) {
            return;
        }
		for (Edge * edge=outEdges.getFirst(); edge!=NULL; edge=outEdges.getNext(edge)){
            if (edge!=fallEdge && !edge->getNode(Direction_Head)->hasKind(Node::Kind_UnwindNode)) {
 				return;
            }
		}

		BasicBlock * bbNext=(BasicBlock*)fallEdge->getNode(Direction_Out);
		assert(bbNext->hasKind(Node::Kind_BasicBlock));

		if (bbNext->getEdges(Direction_In).getCount()!=1)
			return;
		assert(bbNext->getEdges(Direction_In).getFirst()->getNode(Direction_Tail)==bb);

		if (bbNext->getNode(Direction_Out, Node::Kind_DispatchNode)!=NULL)
			return;			

		while(!bbNext->isEmpty()){
			Inst * inst=bbNext->getInsts().getFirst();
			bbNext->removeInst(inst);
			bb->appendInsts(inst);
		}

		const Edges & bbNextOutEdges=bbNext->getEdges(Direction_Out);
		for (Edge * edge=bbNextOutEdges.getFirst(), * nextEdge=NULL; edge!=NULL; edge=nextEdge){
			nextEdge=bbNextOutEdges.getNext(edge);
			if (!edge->isFallThroughEdge() && !bb->isConnectedTo(Direction_Out, edge->getNode(Direction_Head))){
				bool isDirectBranch=edge->isDirectBranchEdge();
				retargetEdge(Direction_Tail, edge, bb);
				if (isDirectBranch){
					bb->makeEdgeDirectBranch(edge);
					bb->recalculateFallThroughEdgeProbability();
				}
			}
		}

		purgeEmptyBlock(bbNext);
		tryMergeSequentialBlocks(node); 
	}

    //_________________________________________________________________________________________________
	void CFG::purgeEmptyBlocks() {
		assert(!isLaidOut());
        const Nodes& nodes = getNodesPostorder();
        for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
            Node* node = *it;
            if (node->hasKind(Node::Kind_BasicBlock) && ((BasicBlock*)node)->isEmpty()){
				purgeEmptyBlock((BasicBlock*)node);
			}
		}
	}


   //_________________________________________________________________________________________________
	void CFG::purgeEmptyBlock(BasicBlock * bb) {
		assert(!isLaidOut());
		assert(bb->getInsts().isEmpty());

        if (!bb->canBeRemoved()) {
        	return;
        }

        const Edges& inEdges=bb->getEdges(Direction_In);
        const Edges& outEdges=bb->getEdges(Direction_Out);

        Edge* outEdge = NULL;
        if (inEdges.getCount() == 1 && inEdges.getFirst()->hasKind(Edge::Kind_CatchEdge)) { 
            //handle catch edges - can't remove them (catch edge contains type info +priority info)
            Edge* inEdge = inEdges.getFirst();
            assert(outEdges.getCount() == 1);
            outEdge = outEdges.getFirst();
            Node* commonCatchHandler = outEdge->getNode(Direction_Head);
            assert(commonCatchHandler->hasKind(Node::Kind_BasicBlock));
            retargetEdge(Direction_Head, inEdge, commonCatchHandler);
            removeEdge(outEdge);
        } else {
            if (outEdges.getFirst()!=outEdges.getLast()) {
                for (Edge* tmp = outEdges.getFirst(); tmp; tmp= outEdges.getNext(tmp)) {
                    if (tmp->isBlockEdge()) {
                        assert(outEdge == NULL);
                        outEdge = tmp;
                    } else {
                        // We don't want to introduce a disconnected cfg here.
                        Node* outNode = tmp->getNode(Direction_Head);
                        if( (outNode->getEdges(Direction_In)).getCount() == 1 ){
                            return;
                        }
                    }
                }
                assert(outEdge!=NULL);
            } else {
                outEdge = outEdges.getFirst();
            }
            if (outEdge!=NULL){
                Node* outNode = outEdge->getNode(Direction_Head);

                // Do not delete an empty while-true loop.
                if( outNode == bb ){
                    return;
                }

                for (Edge * inEdge=inEdges.getFirst(), * nextInEdge=NULL; inEdge!=NULL; inEdge=nextInEdge){
                    nextInEdge=inEdges.getNext(inEdge);
                    Node * inNode=inEdge->getNode(Direction_Tail);
#ifdef _DEBUG
                    inNode->verify();
#endif

                    if (inNode->hasKind(Node::Kind_BasicBlock)){
                        BasicBlock * inBB=(BasicBlock *)inNode;
                        const Insts& inNodeInsts=inBB->getInsts();
                        if (!inNodeInsts.isEmpty()){
                            Inst * lastInst=inNodeInsts.getLast();
                            if (lastInst->hasKind(Inst::Kind_SwitchInst)){
                                assert(outNode->hasKind(Node::Kind_BasicBlock));
                                BasicBlock* outBB = (BasicBlock*)outNode;
                                SwitchInst * switchInst = (SwitchInst*)lastInst;

                                switchInst->replaceTarget(bb, outBB);
                            }
                        }
                    }

                    Edge * in2OutDirectEdge = inNode->getEdge(Direction_Out, outNode);
                    if (in2OutDirectEdge!=NULL) {
                        in2OutDirectEdge->setProbability(in2OutDirectEdge->getProbability() + inEdge->getProbability());
                        if (inNode->hasKind(Node::Kind_BasicBlock)){
                            BasicBlock * inBB=(BasicBlock *)inNode;
                            if (inEdge->isDirectBranchEdge()){
                                BranchInst * branchInst=inEdge->getBranch();
                                if (branchInst)
                                    inBB->removeInst(branchInst);
                            }
                            if (in2OutDirectEdge->isDirectBranchEdge()){
                                BranchInst * branchInst=in2OutDirectEdge->getBranch();
                                if (branchInst)
                                    inBB->removeInst(branchInst);
                                inBB->makeEdgeFallThrough(in2OutDirectEdge);
                            }
                        }
                        removeEdge(inEdge);
#ifdef _DEBUG
                        inNode->verify();
#endif
                    } else {
                        retargetEdge(Direction_Head, inEdge, outNode);
#ifdef _DEBUG
                        inNode->verify();
#endif
                    }
#ifdef _DEBUG
                    inNode->verify();
#endif
                }
            }
#ifdef _DEBUG
            bb->verify();
#endif
        }
        removeNode(bb);
	}

    //_________________________________________________________________________________________________
    void CFG::removeEdge(Edge * edge)
    {
        edge->getNode(Direction_Tail)->delEdge(Direction_Out, edge);
        edge->getNode(Direction_Head)->delEdge(Direction_In, edge);
        edge->nodes[Direction_Tail]=edge->nodes[Direction_Head]=0;
    }

    //_________________________________________________________________________________________________________
    // retarget all edges of src node to dst node
    void CFG::retargetAllEdges(Direction dir, Node* oldNode, Node* newNode) {
        CFG* cfg = oldNode->getCFG(); //todo: make retargetEdge static..
        Direction edgeTearDir = dir == Direction_In ? Direction_Head: Direction_Tail;
        while (!oldNode->getEdges(dir).isEmpty()) {
            Edge* edge = oldNode->getEdges(dir).getFirst();
            bool edgeIsFallThru = edge->isFallThroughEdge();
            if (!edgeIsFallThru && dir == Direction_In && oldNode->hasKind(Node::Kind_ExitNode)) { 
                // special handling when exit node is replaced with BB
                // try to make edge fall-through in this case
                Node* tailNode = edge->getNode(Direction_Tail);
                edgeIsFallThru = tailNode->hasKind(Node::Kind_BasicBlock) && newNode->hasKind(Node::Kind_BasicBlock);
            }
            bool egdeIsDirectTarget = !edgeIsFallThru && edge->isDirectBranchEdge();
            cfg->retargetEdge(edgeTearDir, edge, newNode);
            if (edgeIsFallThru || egdeIsDirectTarget) {
                Node* tailNode = edge->getNode(Direction_Tail);
                assert(tailNode->hasKind(Node::Kind_BasicBlock));
                if (egdeIsDirectTarget) {
                    ((BasicBlock*)tailNode)->makeEdgeDirectBranch(edge);   
                } else {
                    ((BasicBlock*)tailNode)->makeEdgeFallThrough(edge);   
                }
            }
        }
    }        

    //_________________________________________________________________________________________________________
    void CFG::importNodes(const Nodes& nodesToAdd) {
        for (Nodes::const_iterator it = nodesToAdd.begin(), end = nodesToAdd.end(); it!=end; ++it) {
            Node* newNode = *it;
            newNode->cfg = this;
            newNode->id = nodeId++;
            addNode(newNode);
        }
    }

    //_________________________________________________________________________________________________________
    void CFG::mergeGraphAtEdge(CFG* cfg, Edge* edge, bool takeParentDispatch) {
 
        assert(!edge->hasKind(Edge::Kind_CatchEdge));

        BasicBlock* prolog = cfg->getPrologNode();
        ExitNode*   exit   = cfg->getExitNode();
        UnwindNode* unwind = cfg->getUnwindNode();

        Node* sourceNode = edge->getNode(Direction_Tail);
        Node* targetNode = edge->getNode(Direction_Head);
        assert(sourceNode->getCFG() == this);
        CFG * parentCFG  = this;
        assert(&parentCFG->getMemoryManager() == &cfg->getMemoryManager());
        
        UnwindNode* parentUnwind = parentCFG->getUnwindNode();
        Edge* parentDispatchEdge = sourceNode->getEdge(Direction_Out, Node::Kind_DispatchNode);
        Node* parentBlockDipatchNode = parentDispatchEdge == NULL ? NULL : parentDispatchEdge->getNode(Direction_Head);

        //bind exception nodes.
        if (unwind!=NULL) {
            Node* unwindReplacement = 0;
            if (takeParentDispatch && parentBlockDipatchNode != NULL )
                unwindReplacement = parentBlockDipatchNode;
            else
                unwindReplacement = parentUnwind;
            if (unwindReplacement == NULL) {
                unwindReplacement = parentCFG->newUnwindNode();
            }
            retargetAllEdges(Direction_In, unwind, unwindReplacement);
            Edge* edgeToExit = unwind->getEdge(Direction_Head, exit);
            assert(edgeToExit!=NULL);
            cfg->removeEdge(edgeToExit);
            cfg->delNode(unwind);
        }  

        // cut the edge
        parentCFG->retargetEdge(Direction_Head, edge, prolog);
        parentCFG->retargetAllEdges(Direction_In, exit, targetNode);

        cfg->delNode(exit);

        //set inlined nodes owner to parent
        parentCFG->importNodes(cfg->getNodes());
    }

    //_________________________________________________________________________________________________________
    // WARN: childCFG insts must be created with parentCFG IR Manager!
    void CFG::mergeGraphs(CFG* parentCFG, CFG* childCFG,  Inst* insertLocation, bool insertAfter) {
        //todo: handle persistent ids..
        assert(&parentCFG->getMemoryManager() == &childCFG->getMemoryManager());

        BasicBlock* childProlog = childCFG->getPrologNode();
        ExitNode* childExit = childCFG->getExitNode();
        BasicBlock* parentBlock = insertLocation->getBasicBlock();
        UnwindNode* childUnwind  = childCFG->getUnwindNode();
        UnwindNode* parentUnwind = parentCFG->getUnwindNode();
        Edge* parentDispatchEdge = parentBlock->getEdge(Direction_Out, Node::Kind_DispatchNode);
        Node* parentBlockDipatchNode = parentDispatchEdge == NULL ? NULL : parentDispatchEdge->getNode(Direction_Head);
        
        const Insts& insts = parentBlock->getInsts();


        bool appendToParentBlock = insts.getLast() == insertLocation && insertAfter;
        // can't optimize prepend -> need to keep SwitchInst targets -> leave parent block at the old place, even if it is empty
        if (appendToParentBlock) {
            assert(parentBlock->getDirectBranchEdge() == NULL); //only fall-through edge
            Edge* parentFallEdge = parentBlock->getFallThroughEdge();
            assert (parentFallEdge!=NULL);
            Node* fallTarget = parentFallEdge->getNode(Direction_Head);
            parentCFG->retargetEdge(Direction_Out, parentFallEdge, childProlog);
            retargetAllEdges(Direction_In, childExit, fallTarget);
        } else { //split parent block in 2, bottomBlock contains bottom insts from parentBlock
            BasicBlock* bottomBlock = parentCFG->newBasicBlock();
            Inst* insertAfterInst = insertAfter ? insertLocation: insts.getPrev(insertLocation);
            for (Inst* inst = insts.getLast(); inst!=insertAfterInst; ){
                Inst* instToMove = inst;
                inst = insts.getPrev(inst);
                parentBlock->removeInst(instToMove);
                bottomBlock->prependInsts(instToMove);
            } 
            //now make all out edges of parentBlock to be  out edges of the bottomBlock
            retargetAllEdges(Direction_Out, parentBlock, bottomBlock);

            parentCFG->newFallThroughEdge(parentBlock, childProlog, 1.0);
            retargetAllEdges(Direction_In, childExit, bottomBlock);
            
            if (parentBlockDipatchNode!=NULL) {
                //recreate this edge again, was moved with retargetAllEdges(out,..)..
                parentCFG->newEdge(parentBlock, parentBlockDipatchNode, 0.01); 
            }
        }

        //bind exception nodes.
        if (childUnwind!=NULL) {
            Node* childUnwindReplacement = parentBlockDipatchNode != NULL ? parentBlockDipatchNode : parentUnwind;
            if (childUnwindReplacement == NULL) {
                childUnwindReplacement = parentCFG->newUnwindNode();
            }
            retargetAllEdges(Direction_In, childUnwind, childUnwindReplacement);

            //delete child's unwind & exit (optional op)
            Edge* edgeToExit = childUnwind->getEdge(Direction_Head, childExit);
            assert(edgeToExit!=NULL);
            childCFG->removeEdge(edgeToExit);
            childCFG->delNode(childUnwind);  
        }  else if (parentBlockDipatchNode != NULL){ 
            //no unwind node in child CFG, propagate parentBlock dispatching for every child node
            const Nodes& childNodes = childCFG->getNodes();
            for (Nodes::const_iterator it = childNodes.begin(), end = childNodes.end(); it!=end; ++it) {
                Node* childNode = *it;
                if (childNode->hasKind(Node::Kind_BasicBlock)) {
                    assert(childNode->getEdge(Direction_Out, Node::Kind_DispatchNode) == NULL);
                    parentCFG->newEdge(childNode, parentBlockDipatchNode, 0.01);
                }
            }
        }

        childCFG->delNode(childExit);

        //set child nodes owner to parent
        parentCFG->importNodes(childCFG->getNodes());
    }

    //_________________________________________________________________________________________________
    void CFG::updateLoopInfo(bool force) {
        if (!isLoopInfoValid() && !force) {
            if (Log::cat_cg()->isDebugEnabled()) {
                Log::cat_cg()->out() << "Loop info recalculation.";
            }
            maxLoopDepth = 0;
            MemoryManager& mm = getMemoryManager();//(sizeof(DFSLoopInfo) * getMaxNodeId() * 2, "Ia32::CFG:updateLoopInfo");
            DFSLoopInfo* dfsInfo = new (mm) DFSLoopInfo[getMaxNodeId()];
            NodeList outerLoopHeaders(mm);
            memset(dfsInfo, 0, sizeof(DFSLoopInfo) * getMaxNodeId());
            dfsInfo[prologNode->getId()].dfn = 1;
            bool _hasLoops = doLoopInfoDFS(prologNode, dfsInfo, outerLoopHeaders);
            assert (outerLoopHeaders.empty());
            lastLoopInfoVersion = graphModCount;
            if (_hasLoops) {
                for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
                    Node* node = *it;
                    if (node->isLoopHeader()) {
                        uint32 depth = 1 + node->getLoopDepth();
                        maxLoopDepth = std::max(depth, maxLoopDepth);
                    }
					if (node->execCnt <= 0 && !hasEdgeProfile()){
						uint32 execCnt = 1;
						for (uint32 i = 0, loopDepth = (node->getLoopDepth() + (node->isLoopHeader() ? 1 : 0)); i < loopDepth; ++i)
							execCnt *= 10;
						node->execCnt = execCnt;
					}
				}
            }
        }
    }

    //_________________________________________________________________________________________________
    bool CFG::doLoopInfoDFS(Node* node, DFSLoopInfo* info, NodeList& outerLoopHeaders) const {
		bool hasLoops=false;
        assert(node!=NULL);
        node->loopHeader = NULL;

        node->nodeIsHeader = FALSE;
        DFSLoopInfo& myInfo= info[node->getId()];
        assert(myInfo.dfn != 0);
        assert(myInfo.color == 0); //white
        myInfo.color = 1; //gray
        const Edges& edges=node->getEdges(Direction_Out);
        int nLoopHeadersBefore = outerLoopHeaders.size();
        for (const Edge * e=edges.getFirst(); e!=NULL; e=edges.getNext(e)) {
            Node* targetNode  = e->getNode(Direction_Head);
            DFSLoopInfo& targetInfo = info[targetNode->getId()];
            if (targetInfo.color == 2) { //direct edge or cross-edge -> this edge was accessed by another DFS recursion
                //target node could not be a loop header for current node -> it's black
                Node* loopHeader = targetNode->loopHeader;
                if (loopHeader!=NULL) { //check if target node is in loop -> copy its loop info
                    outerLoopHeaders.push_front(loopHeader);
                }
            } else if (targetInfo.color == 1) { //ok we found loop; targetNode is a loop header
                outerLoopHeaders.push_front(targetNode);
            } else {
                targetInfo.dfn = myInfo.dfn+1;
                hasLoops = doLoopInfoDFS(targetNode, info, outerLoopHeaders) || hasLoops;
            }
        }
        
        // find loop header for current node 
        // remove loop header from list if it equals to the current node
        bool isLoopHeader = FALSE;
        Node* myLoopHead = NULL;
         // optimization: keeping all loop-heads for the dfs-parent node in the single list
        //  drawback : need to distinct loop-heads found while current node DFS from others (other children of dfs-parent)
        int loopsHeadsReached = outerLoopHeaders.size() - nLoopHeadersBefore;
        if (loopsHeadsReached>0) {
            for (NodeList::iterator it = outerLoopHeaders.begin(); --loopsHeadsReached>=0; ++it) {
                Node* headerNode = *it;
                if (headerNode==node) { 
                    isLoopHeader = TRUE;
                    outerLoopHeaders.erase(it);
                } else {
                    if (myLoopHead == NULL) {
                        myLoopHead = headerNode;
                    } else if (myLoopHead!=headerNode){
                        // there were edges to different loops (loop-exit edges, backedges). 
                        // Our loop is the most inner one == head with max dfn
                        int myHeadDFN = info[myLoopHead->getId()].dfn;
                        int newHeadDFN = info[headerNode->getId()].dfn;
                        assert(myHeadDFN!=newHeadDFN);
                        if (myHeadDFN < newHeadDFN) {
                            myLoopHead = headerNode; // inner loop
                        }
                    } else { // myLoopHead == headerNode
                        //this also is possible -> multiple paths save the one loopHead many times (= nPathes)
                        //do nothing
                    }
                }
            }
			assert(myLoopHead == NULL || info[myLoopHead->getId()].dfn  < myInfo.dfn);
        }
        node->setLoopInfo(isLoopHeader, myLoopHead);
        myInfo.color = 2; //black, node and all it's child-nodes are checked
		return hasLoops || isLoopHeader;
    }        
    
    //_________________________________________________________________________________________________
    bool CFG::ensureLoopInfoIsValid() {
        if (!isLoopInfoValid()) {
            return FALSE;
        }
        MemoryManager tmpMM((sizeof(Node*)*4 + 1)*getMaxNodeId(), "Ia32CFG::ensureLoopInfoIsValid");
        Node **nodeLoopHead = new (tmpMM) Node*[getMaxNodeId()];
        bool *nodeIsLoopHead = new (tmpMM) bool[getMaxNodeId()];
        for(Nodes::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; it++) {
            Node* node = *it;
            nodeLoopHead[node->getId()] = node->getLoopHeader();
            nodeIsLoopHead[node->getId()] = node->isLoopHeader();
        }
        updateLoopInfo(TRUE);
        for(Nodes::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; it++) {
            Node* node = *it;
            if (nodeLoopHead[node->getId()] != node->getLoopHeader()) {
                return FALSE;
            } 
            if (nodeIsLoopHead[node->getId()]!=node->isLoopHeader()) {
                return FALSE;
            }
        }
        return TRUE;
    }

    //_________________________________________________________________________________________________
    const Nodes& CFG::getNodesPostorder() const {
        if (lastPostorderVersion != graphModCount) {
            traversalInfo.resize(getMaxNodeId() + 1);
            std::fill(traversalInfo.begin(), traversalInfo.end(), 0);
            Direction edgeDir = Direction_Out;
            Node* startNode = prologNode;
            Node* endNode = exitNode;
            // convention: first and last nodes in container are always prolog & exit
            // here we fix it manually, otherwise loops in CFG may cause that exit node 
            // will not be the first node in container for postorder sort
            traversalInfo[endNode->getId()] = 2;
            postorderNodesCache.clear();
            postorderNodesCache.push_back(endNode);
            getNodesDFS(postorderNodesCache, startNode, edgeDir);
#ifdef _DEBUG   //check that all nodes have the same traversal number
            for (Nodes::const_iterator it = nodes.begin(), itEnd = nodes.end(); it!=itEnd; ++it) {
                Node* tmp = *it;
                assert(traversalInfo[tmp->getId()] == 2);
                tmp->verify();
            }
#endif
            assert(postorderNodesCache.size() == nodes.size());
            lastPostorderVersion = graphModCount;
        }
        return postorderNodesCache;
    }

    
    //_________________________________________________________________________________________________
    void CFG::getNodesDFS(Nodes& container, Node * node, Direction edgeDir) const {
        assert(node!=NULL);
        assert(traversalInfo[node->getId()]==0); //node is white here
        uint32 nodeId = node->getId();
        traversalInfo[nodeId] = 1; //mark node gray
        // for preorder: container.push_back(node);
        const Edges& edges=node->getEdges(edgeDir);
        for (const Edge * e=edges.getLast(); e!=NULL; e=edges.getPrev(e)){
            Node* targetNode  = e->getNode(edgeDir);
            if ( traversalInfo[targetNode->getId()] !=0) {
                //back-edge(gray) if == 1 or cross-edge or direct-edge (black) if ==2
                continue; 
            }
            getNodesDFS(container, targetNode, edgeDir);
            
        }
        traversalInfo[nodeId] = 2;//mark node black
        container.push_back(node);
    }

    //_________________________________________________________________________________________________________
    CFG::NodeIterator::NodeIterator(const CFG& cfg, OrderType orderType)  
    {
        switch(orderType) {
            case OrderType_Arbitrary: 
                nodes = &cfg.getNodes();
                break;
            case OrderType_Postorder:
                nodes = &cfg.getNodesPostorder();
                break;
            case OrderType_ReversePostorder:
                nodes = &cfg.getNodesPostorder();
                break;
            case OrderType_Layout: 
                {
                    Nodes* tmp = new (cfg.getMemoryManager()) Nodes(cfg.getMemoryManager());
                    cfg.getNodes(*tmp, OrderType_Layout);
                    nodes = tmp;
                }
                break;
                                   
            default: assert(0);
        }

        if (orderType!= OrderType_ReversePostorder) {
            currentIdx = 0;
            borderIdx = nodes->size();
            increment = 1;
        } else {
            currentIdx = nodes->size()-1;
            borderIdx = -1;
            increment = -1;
        }
    }

    
    //_________________________________________________________________________________________________________
    void CFG::NodeIterator::rewind() {
        if (increment == 1) {
            currentIdx = 0;
        } else {
            currentIdx = nodes->size()-1;
        }
    }

}}; // namespace Ia32
