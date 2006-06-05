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
 * @author George A. Timoshenko
 * @version $Revision: 1.1.12.4.4.3 $
 */

#ifndef _IA32_BBPOLLING_H_
#define _IA32_BBPOLLING_H_

#include "Ia32IRManager.h"

namespace Jitrino
{
namespace Ia32{

const uint32 BBPollingMaxVersion = 6;

//========================================================================================
// class BBPolling
//========================================================================================
/**
    class BBPolling implements utilities for back branch polling pass 

*/
class BBPolling
{
typedef ::std::pair<uint32,uint32> targetIdDispatchIdPair;
typedef StlMap<targetIdDispatchIdPair,Node*> BBPControllersMap;
public:

    BBPolling(IRManager& ir,uint32 ver) :
        irManager(ir),
        version(ver),
        hasThreadInterruptablePoint(irManager.getMemoryManager(), irManager.getMaxNodeId()+1, false),
        hasNativeInterruptablePoint(irManager.getMemoryManager(), irManager.getMaxNodeId()+1, false),
        isOnThreadInterruptablePath(irManager.getMemoryManager(), irManager.getMaxNodeId()+1, false),
        tlsBaseRegForLoopHeader(irManager.getMemoryManager(), irManager.getMaxNodeId()+1, NULL),
        bbpCFGControllerForNode(irManager.getMemoryManager()),
        toppestLoopHeader(irManager.getMemoryManager(), irManager.getMaxNodeId()+1, NULL),
        otherStartNdx(irManager.getMemoryManager(), irManager.getMaxNodeId()+1, 0),
        loopHeaders(irManager.getMemoryManager()),
        otherEdges(irManager.getMemoryManager()),
        eligibleEdges(irManager.getMemoryManager()),
#ifdef _DEBUG
        interruptablePoints(0),
        pollingPoints(0),
#endif
        loopHeaderOfEdge(irManager.getMemoryManager())
    {
        calculateInitialInterruptability(version == 5 || version == 6);

        if (version == 2 || version == 3)
            calculateInterruptablePathes();
        // no more calculations here, just collect the edges!
        switch (version) {
            case 1: collectEligibleEdges(); break;
            case 2: collectEligibleEdges2(); break;
            case 3: collectEligibleEdgesRecursive(); break;
            case 4: collectEligibleEdges(); break;
            case 5: collectEligibleEdges(); break;
            case 6: collectEligibleEdges(); break;
            default: assert(0);
        }
        if (Log::cat_cg()->isDebugEnabled()) {
            dumpEligibleEdges();
        }
#ifdef _DEBUG
        if (version == 2 || version == 3)
            verify();
#endif
    }

    uint32  numberOfAffectedEdges()     { return eligibleEdges.size(); }
    Edge*   getAffectedEdge(uint32 i)   { assert(i < eligibleEdges.size()); return eligibleEdges[i]; }

    Opnd*   getOrCreateTLSBaseReg(Edge* e);

    static bool isThreadInterruptablePoint(const Inst* inst);

    static bool hasNativeInterruptablePoints(const Node* node);
    
    bool    hasAllThreadInterruptablePredecessors(const Node* node);

    Node*   getBBPSubCFGController(uint32 targetId, uint32 dispatchId);
    void    setBBPSubCFGController(uint32 targetId, uint32 dispatchId, Node* node);
    CFG*    createBBPSubCFG(IRManager& ir, Opnd* tlsBaseReg);

private:

    bool    isInterruptable(const BasicBlock* b) {
        return hasThreadInterruptablePoint[b->getId()];
    }
    // select all loopHeaders
    // identify all nodes which hasNativeInterruptablePoints
    // mark all successors of such nodes as hasThreadInterruptablePoint
    void    calculateInitialInterruptability(bool doPassingDown);
    // identify the nodes which are on the way from a loopHeader to a node which hasThreadInterruptablePoints
    void    calculateInterruptablePathes();
    bool    isOnInterruptablePath(Node* node);

    // collect the edges for substition by subCFG that checks the flag and call the helper if the flag.
    // These edges are those from a Node which isOnInterruptablePath to it's succ_node which is not.
    void    collectEligibleEdges();     // all backedges
    void    collectEligibleEdges2();    // all pairs [isOnThreadInterruptablePath]->[!isOnThreadInterruptablePath]
    void    collectEligibleEdgesRecursive(); // recursive selecting of 2
    void    collectEdgesDeeper(Node* node);

    void    dumpEligibleEdges();

#ifdef _DEBUG
    bool    isEligible(Edge* e);
    void    verify();
    void    verifyDeeper(Node* node, Node* enwind, Node* exit);
#endif // _DEBUG

    IRManager&  irManager;

    // version of BBPolling:
    //  0 - must be discarded in runImpl()
    //  1 - insert bbpCFG at all backedges
    //  2 - path analysis based on searching of pairs [isOnThreadInterruptablePath]->[!isOnThreadInterruptablePath]
    //  3 - recursive version of "2"
    //  4 - "1" + suspension flag addr [TLS base + offset] is calculated before the loop header
    //  5 - like "1" but some backedges are not patched (if all paths through it are interuuptable)
    //  6 - "4" + "5"
    //  7.. illegal
    uint32  version;
    // storage for ThreadInterruptablePoints information
    //     hasThreadInterruptablePoint[Node->getId()] == true means that the Node is a LoopHeader
    //     OR It has at least one instruction that isThreadInterruptablePoint(inst)
    //     OR all predecessors hasThreadInterruptablePoint or incoming edge is a loopExit
    StlVector<bool> hasThreadInterruptablePoint;
    StlVector<bool> hasNativeInterruptablePoint; // only those that hase at least one InterruptablePoint(inst)
    // storage for InterruptablePathes information
    //     isOnThreadInterruptablePath[Node->getId()] == true means that there is a way from the Node
    //     to another a_node which hasThreadInterruptablePoint[a_node->getId()]
    StlVector<bool> isOnThreadInterruptablePath;
    //     tlsBaseRegs pointers. One per each affected loopHeader
    StlVector<Opnd*>    tlsBaseRegForLoopHeader;
    //     pointers to already prepared bbpCFG. bbpCFG is placed "before" a node to collect all eligible edges.
    //     pair.first - targetNode id
    //     pair.second - sourceNode dispatch edge target id
    BBPControllersMap   bbpCFGControllerForNode;
    //     to get the toppest loop header of the given without calling getLoopHeader
    StlVector<Node*>    toppestLoopHeader;
    //     start index in otheredges collection for the toppest loop headers
    StlVector<uint32> otherStartNdx;

    // just a collection of loop headers of the method (Basic blocks only!)
    StlVector<Node*> loopHeaders;
    // edgse which are not a back edge
    StlVector<Edge*> otherEdges;
    // edges for inserting BBPolling subCFG
    StlVector<Edge*> eligibleEdges;

#ifdef _DEBUG
    uint32  interruptablePoints;
    uint32  pollingPoints;
#endif
    StlHashMap<Edge*, Node *> loopHeaderOfEdge;

}; // BBPolling class

//___________________________________________________________________________________________________
BEGIN_DECLARE_IRTRANSFORMER(BBPollingTransformer, "bbp", "Back-branch polling. (Insertion of thread suspend points)")

	BBPollingTransformer(IRManager& irm, const char * params=0): IRTransformer(irm, params),hasSideEffects(false){}

    bool hasSideEffects;

    void runImpl(){
        if(!irManager.hasLoops())
            return;
        if (parameters) {
            version = atoi(std::string(parameters).c_str());
            if(version == 0)
                return;
            if(version > BBPollingMaxVersion) {
                assert(0);
                return;
            }
        } else {
            return;
        }
        if (Log::cat_cg()->isDebugEnabled())
            Log::cat_cg()->out() << "BBPolling transformer version="<< version <<" STARTED" << ::std::endl;
        BBPolling bbp = BBPolling(irManager, version);
        uint32 numOfAffectedEdges = bbp.numberOfAffectedEdges();
        hasSideEffects = numOfAffectedEdges != 0;
        // Foreach eligible backedge create bbpCFG and inline it between the backedge's Tail and it's target
        for (uint32 j = 0; j < numOfAffectedEdges; j++) {
            Edge* edge = bbp.getAffectedEdge(j);

            // get or create and insert before the loopHeade a basic block for calculating TLS base
            Opnd* tlsBaseReg = bbp.getOrCreateTLSBaseReg(edge);

            uint32 originalTargetId = edge->getNode(Direction_Head)->getId();
            Edge*  srcDispatchEdge  = edge->getNode(Direction_Tail)->getEdge(Direction_Out,Node::Kind_DispatchNode);
            uint32 sourceDispatchId = srcDispatchEdge ? srcDispatchEdge->getNode(Direction_Head)->getId() : 0;
            // CFG for inlining
            Node* bbpCFGController = bbp.getBBPSubCFGController(originalTargetId,sourceDispatchId);
            if (bbpCFGController) { // just retarget the edge
                irManager.retargetEdge(Direction_Head, edge, bbpCFGController);
            } else { // we need a new bbpCFG
                CFG* bbpCFG = bbp.createBBPSubCFG(irManager, tlsBaseReg);
            
                // Inlining bbpCFG at edge
                irManager.mergeGraphAtEdge(bbpCFG, edge, true /*take parent's dispatch*/);
                bbp.setBBPSubCFGController(originalTargetId,sourceDispatchId,edge->getNode(Direction_Head));
            }
        }
        if (Log::cat_cg()->isDebugEnabled())
            Log::cat_cg()->out() << "BBPolling transformer FINISHED" << ::std::endl;
    } //runImpl()
    uint32  getNeedInfo()const{ return NeedInfo_LoopInfo; }
    uint32  getSideEffects()const{ return hasSideEffects ? SideEffect_InvalidatesLoopInfo|SideEffect_InvalidatesLivenessInfo : 0; }
    bool    isIRDumpEnabled(){ return true; }
    uint32  version;
END_DECLARE_IRTRANSFORMER(BBPollingTransformer) 


}}; // namespace Ia32

#endif // _IA32_BBPOLLING_H_

