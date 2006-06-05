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
 * @version $Revision: 1.1.12.3.4.3 $
 */

#include "Ia32BBPolling.h"
#include "DrlVMInterface.h"

namespace Jitrino
{
namespace Ia32{

const uint32 gcFlagOffsetOffset = flagTLSSuspendRequestOffset();

Opnd*
BBPolling::getOrCreateTLSBaseReg(Edge* e)
{
    // TLS base reg is one per each nested loop
    Node* loopHeader = toppestLoopHeader[loopHeaderOfEdge[e]->getId()];
    assert(loopHeader);

    const uint32 id = loopHeader->getId();

    Opnd* tlsBaseReg = tlsBaseRegForLoopHeader[id];
    if ( tlsBaseReg ) { // it is already created for this loop
        return tlsBaseReg;
    } else {
        Type* typeInt32 = irManager.getTypeManager().getPrimitiveType(Type::Int32);
        tlsBaseReg = irManager.newOpnd(typeInt32, Constraint(RegName_EAX)|
                                                             RegName_EBX |
                                                             RegName_ECX |
                                                             RegName_EDX |
                                                             RegName_EBP |
                                                             RegName_ESI |
                                                             RegName_EDI);
        // Basic Block for flag address calculating. (To be inserted before the loopHeaders)
        BasicBlock * bbpFlagAddrBlock = irManager.newBasicBlock();
#ifdef PLATFORM_POSIX
         // TLS base can be obtained by calling get_thread_ptr()  (from vm_threads.h)
         Opnd * target=irManager.newImmOpnd( irManager.getTypeManager().getIntPtrType(),
                                             Opnd::RuntimeInfo::Kind_HelperAddress,
                                            (void*)CompilationInterface::Helper_GetSuspReqFlag
                                           );
         bbpFlagAddrBlock->appendInsts(irManager.newCallInst(target, &CallingConvention_STDCALL, 0, NULL, tlsBaseReg));
#else // PLATFORM_POSIX
        // TLS base can be obtained from [fs:0x14]
        Opnd* tlsBase = irManager.newMemOpnd(typeInt32, MemOpndKind_Any, NULL, 0x14, RegName_FS);
        if (version == 4 || version == 6) {
            Opnd * offset = irManager.newImmOpnd(typeInt32, gcFlagOffsetOffset);
            bbpFlagAddrBlock->appendInsts(irManager.newInstEx(Mnemonic_ADD, 1, tlsBaseReg, tlsBase, offset));
        } else {
            bbpFlagAddrBlock->appendInsts(irManager.newInst(Mnemonic_MOV, tlsBaseReg, tlsBase));
        }
#endif // PLATFORM_POSIX

        // inserting bbpFlagAddrBlock before the given loopHeader
        uint32 startIndex = otherStartNdx[id];

        for (uint32 otherIdx = startIndex; ; otherIdx++) {
            if (otherIdx == otherEdges.size())
                break;
            Edge* other = otherEdges[otherIdx];
            if (other->getNode(Direction_Head) != loopHeader)
                break;
            
            irManager.retargetEdge(Direction_Head, other, bbpFlagAddrBlock);
        }
        
        assert(loopHeader->hasKind(Node::Kind_BasicBlock));
        irManager.newFallThroughEdge(bbpFlagAddrBlock,(BasicBlock*)loopHeader,1);

        tlsBaseRegForLoopHeader[id] = tlsBaseReg;
        return tlsBaseReg;
    }
}

Node*
BBPolling::getBBPSubCFGController(uint32 targetId, uint32 dispatchId)
{
    const BBPControllersMap::iterator iter = bbpCFGControllerForNode.find(::std::make_pair(targetId,dispatchId));
    if (iter == bbpCFGControllerForNode.end()) {
        return NULL;
    } else {
        return (*iter).second;
    }
}

void
BBPolling::setBBPSubCFGController(uint32 targetId, uint32 dispatchId, Node* node)
{
    assert(node);
    assert(!bbpCFGControllerForNode.has(::std::make_pair(targetId,dispatchId)));
    bbpCFGControllerForNode[::std::make_pair(targetId,dispatchId)] = node;
}

// This is a debug hack. It does not work with gcc (warning when casting to int64 below)
#if 0
static void callerStarter()
{
   ::std::cout << "Caller started!" << ::std::endl;
//    DebugBreak();
}

static void controllerStarter()
{
    ::std::cout << "Controller started!" << ::std::endl;
//    DebugBreak();
}
#endif

CFG*
BBPolling::createBBPSubCFG(IRManager& irManager, Opnd* tlsBaseReg)
{
    Type* typeInt32 = irManager.getTypeManager().getPrimitiveType(Type::Int32);
    // CFG for inlining
    CFG* bbpCFG = new(irManager.getMemoryManager()) CFG(irManager.getMemoryManager());
    BasicBlock * bbpBBController=bbpCFG->getPrologNode();
    BasicBlock * bbpBBHelpCaller=bbpCFG->newBasicBlock();
    ExitNode * bbpExit=bbpCFG->getExitNode();

    // Controller node

// This is a debug hack. It does not work with gcc (warning when casting to int64)
#if 0
    Opnd * target = irManager.newImmOpnd(   irManager.getTypeManager().getIntPtrType(),
                                            (int64)controllerStarter);
    Inst* dbgCall = irManager.newCallInst(target, &CallingConvention_STDCALL, 0, NULL, NULL, NULL);
    bbpBBController->appendInsts(dbgCall);
#endif

#ifdef PLATFORM_POSIX
    Opnd* gcFlag = irManager.newMemOpnd(typeInt32, MemOpndKind_Any, tlsBaseReg, 0);
#else
    Opnd* gcFlag = NULL;
    if (version == 4 || version == 6) {
        gcFlag = irManager.newMemOpnd(typeInt32, MemOpndKind_Any, tlsBaseReg, 0);
    } else {
        gcFlag = irManager.newMemOpnd(typeInt32, MemOpndKind_Any, tlsBaseReg, gcFlagOffsetOffset);
    }
#endif
    Opnd* zero = irManager.newImmOpnd(typeInt32, 0);
    bbpBBController->appendInsts(irManager.newInst(Mnemonic_CMP, gcFlag, zero));
    bbpBBController->appendInsts(irManager.newBranchInst(Mnemonic_JNZ));

    bbpCFG->newDirectBranchEdge(bbpBBController,bbpBBHelpCaller, 0.001);
    bbpCFG->newEdge(bbpBBController, bbpExit, 0.999);

    // Helper Caller node

// This is a debug hack. It does not work with gcc (warning when casting to int64)
#if 0
    target = irManager.newImmOpnd(  irManager.getTypeManager().getIntPtrType(),
                                    (int64)callerStarter);
    dbgCall = irManager.newCallInst(target, &CallingConvention_STDCALL, 0, NULL, NULL, NULL);
    bbpBBHelpCaller->appendInsts(dbgCall);
#endif

    bbpBBHelpCaller->appendInsts(irManager.newRuntimeHelperCallInst(
        CompilationInterface::Helper_EnableThreadSuspension, 0, NULL, NULL)
        );
    bbpCFG->newEdge(bbpBBHelpCaller, bbpExit, 0.999);
    bbpCFG->newUnwindNode();
    bbpCFG->newEdge(bbpBBHelpCaller, bbpCFG->getUnwindNode(), 0.001);

    return bbpCFG;
}

bool
BBPolling::isThreadInterruptablePoint(const Inst* inst) {
    if ( inst->getMnemonic() == Mnemonic_CALL ) {
        Opnd::RuntimeInfo* ri = inst->getOpnd(((ControlTransferInst*)inst)->getTargetOpndIndex())->getRuntimeInfo();
        if (!ri) {
            return false;
        } else {
            if ( ri->getKind() == Opnd::RuntimeInfo::Kind_HelperAddress ) {
                return true;
            }
            if ( ri->getKind() == Opnd::RuntimeInfo::Kind_MethodDirectAddr &&
                 ((MethodDesc*)ri->getValue(0))->isNative() )
            {
                return true;
            }
        }
    }
    return false;
}

bool
BBPolling::hasNativeInterruptablePoints(const Node* node) {
    if (node->hasKind(Node::Kind_DispatchNode))
        return true;
    if (node->hasKind(Node::Kind_BasicBlock)) {
        const Insts& insts = ((BasicBlock*)node)->getInsts();
        // If current BB has an inst that is ThreadInterruptablePoint itself
        // the hasThreadInterruptablePoint becomes true for it
        for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)) {
            if (BBPolling::isThreadInterruptablePoint(inst)) {
                return true;
            }
        }
    }
    return false;
}

bool
BBPolling::hasAllThreadInterruptablePredecessors(const Node* node) {
    if (node->hasKind(Node::Kind_DispatchNode))
        return true;
    const Edges& edges = node->getEdges(Direction_In);
    // All the predecessors must be processed earlier!
    for (Edge* e = edges.getFirst(); e!=NULL; e = edges.getNext(e)) {
        if (!hasThreadInterruptablePoint[e->getNode(Direction_Tail)->getId()] &&
            !e->isLoopExit() )
        {
            return false;
        }
    }
    return true;
}

void
BBPolling::calculateInitialInterruptability(bool doPassingDown)
{
    const CFG::OrderType order = doPassingDown ? CFG::OrderType_Topological : CFG::OrderType_Arbitrary;

    for (CFG::NodeIterator it(irManager, order); it!=NULL; ++it) {
        Node* node = it;
        const uint32 id = node->getId();

        if (node->isLoopHeader() && node->hasKind(Node::Kind_BasicBlock)) {
            loopHeaders.push_back(node);
            // here we calculate loopHeaders with depth == 0, and otherEdges for them
            // to create only one TLS base loader per each nested loop
            Node* loopHeader = node;
            while (loopHeader->getLoopDepth() > 0) {
                loopHeader = loopHeader->getLoopHeader();
            }
            if ( !toppestLoopHeader[id] ) {
                toppestLoopHeader[id] = loopHeader;
                // here we need to remember all otheredges and their start index for particular loopHeader
                otherStartNdx[loopHeader->getId()] = otherEdges.size();
                const Edges& edges = loopHeader->getEdges(Direction_In);
                for (Edge* e = edges.getFirst(); e!=NULL; e = edges.getNext(e)) {
                    if ( !e->isBackEdge() ) {
                        otherEdges.push_back(e);
                    }
                }
            }
        }

        // If current BB has an inst that is ThreadInterruptablePoint itself
        // or it is a Dispatch Node
        // the hasThreadInterruptablePoint becomes true for it
        hasNativeInterruptablePoint[id] = BBPolling::hasNativeInterruptablePoints(node);
        if (hasNativeInterruptablePoint[id]) {
            hasThreadInterruptablePoint[id] = true;
            if (Log::cat_cg()->isDebugEnabled())
                Log::cat_cg()->out() << "        hasNativeInterruptablePoint["<<id<<"] == true" << ::std::endl;
        }

        if (doPassingDown) {
            // if all the predecessors hasThreadInterruptablePoint or incoming edge is a loopExit
            // mark the node as it hasThreadInterruptablePoint (if it is not a loop header)
            if (!hasThreadInterruptablePoint[id] && !node->isLoopHeader() ) {
                if ( hasThreadInterruptablePoint[id] = hasAllThreadInterruptablePredecessors(node) &&
                     Log::cat_cg()->isDebugEnabled() )
                {
                    Log::cat_cg()->out() << "        (inherited) hasThreadInterruptablePoint["<<id<<"] == true" << ::std::endl;
                }
            }
        }
    }
} // calculateInitialInterruptability

void
BBPolling::calculateInterruptablePathes()
{
    assert(loopHeaders.size());
    uint32 maxLoopDepth = irManager.getMaxLoopDepth();

    // process the deepest firstly
    for (uint32 currDepth = maxLoopDepth; currDepth > 0; currDepth--)
    {
        for (uint32 i=0; i < loopHeaders.size(); i++)
        {
            Node* loopHeader = loopHeaders[i];
            const uint32 loopHeaderId = loopHeader->getId();
            if (loopHeader->getLoopDepth() == currDepth - 1)
            {
                if (hasNativeInterruptablePoint[loopHeaderId]) 
                {
                    // fortunately we can skip this loop, because it is already interruptable ( it has respective
                    // instructions and it was calculated earlier in calculateInitialInterruptability() )
                    continue;
                }

                // Process all successors of the loopHeader
                const Edges& edges = loopHeader->getEdges(Direction_Out);
                for (Edge* e = edges.getFirst(); e != NULL; e = edges.getNext(e)) {
                    Node* succ = e->getNode(Direction_Head);
                    if( loopHeader == succ->getLoopHeader() ) {
                        isOnInterruptablePath(succ);
                    } else {
                        // this edge does not go into the loop. Skip it.
                        // OR our loop consist of only one BB (the loopHeader itself). Skip the backedge.
                    }
                }
                // After the loop is processed it gets hasThreadInterruptablePoint mark
                // to prvent the outer loop processing from going inside the current one
                // Also it gets isOnThreadInterruptablePath mark for further selection of eligibleEdges
                hasThreadInterruptablePoint[loopHeaderId] = true;
                isOnThreadInterruptablePath[loopHeaderId] = true;
                if (Log::cat_cg()->isDebugEnabled()) {
                    Log::cat_cg()->out() << "    loopHeader:" << ::std::endl;
                    Log::cat_cg()->out() << "        hasThreadInterruptablePoint["<<loopHeaderId<<"] == true" << ::std::endl;
                    Log::cat_cg()->out() << "        isOnThreadInterruptablePath["<<loopHeaderId<<"] == true" << ::std::endl;
                }
            }
        }
    }
} // calculateInterruptablePathes

bool
BBPolling::isOnInterruptablePath(Node* node)
{
    const uint32 id = node->getId();
    // the order of these check is essential! (because in case of nested loops
    // if the node is a loopHeader of a nested loop we must return true)
    if( hasThreadInterruptablePoint[id] ) {
        isOnThreadInterruptablePath[id] = true;
        if (Log::cat_cg()->isDebugEnabled())
            Log::cat_cg()->out() << "        isOnThreadInterruptablePath["<<id<<"] == true" << ::std::endl;
        return true;
    } else if ( node->isLoopHeader() ) {
        return false; // loopHeader also breaks the recursion
    }

    bool retValue = false;
    const Edges& edges = node->getEdges(Direction_Out);

    for (Edge* e = edges.getFirst(); e != NULL; e = edges.getNext(e)) {
        Node* succ = e->getNode(Direction_Head);
        
        if( !e->isLoopExit() && isOnInterruptablePath(succ) )
        {
            if (Log::cat_cg()->isDebugEnabled())
                Log::cat_cg()->out() << "        isOnThreadInterruptablePath["<<id<<"] == true" << ::std::endl;
            isOnThreadInterruptablePath[id] = true;
            // Must not break here.
            // All outgoing ways must be passed till the LoopHeader or hasThreadInterruptablePoint
            // The LoopHeader at the edge's head means
            //  - that the edge is a LoopExit
            //  - or it is a header of nested loop which must be procesed earlier
            //    so it hasThreadInterruptablePoint
            retValue = true;
        }
    }
    return retValue;
} // isOnInterruptablePath

void
BBPolling::collectEligibleEdges()
{
    for (uint32 i=0; i < loopHeaders.size(); i++) {
        Node* node = loopHeaders[i];
        const Edges& edges = node->getEdges(Direction_In);
        for (Edge* e = edges.getFirst(); e != NULL; e = edges.getNext(e)) {
            if ( e->isBackEdge() && !e->hasKind(Edge::Kind_CatchEdge) &&
                 !hasThreadInterruptablePoint[e->getNode(Direction_Tail)->getId()]
               )
            {
                eligibleEdges.push_back(e);
                loopHeaderOfEdge[e] = node;
            }
        }
    }

} // collectEligibleEdges

void
BBPolling::collectEligibleEdges2()
{
    for (CFG::NodeIterator it(irManager, CFG::OrderType_Arbitrary); it!=NULL; ++it) {
        Node* node = it;
        const uint32 id = node->getId();
        if ( isOnThreadInterruptablePath[id] && ! hasNativeInterruptablePoint[id] ) {
            const Edges& edges = node->getEdges(Direction_Out);
            for (Edge* e = edges.getFirst(); e != NULL; e = edges.getNext(e)) {
                Node* succ = e->getNode(Direction_Head);
                uint32 succId = succ->getId();
                Node* nodeLH = node->isLoopHeader() ? node : node->getLoopHeader();
                assert(nodeLH);
                if ( succ == node || nodeLH == succ->getLoopHeader() || e->isBackEdge() ) {
                    if( !isOnThreadInterruptablePath[succId] )
                    {
                        eligibleEdges.push_back(e);
                        loopHeaderOfEdge[e] = nodeLH;
                    }
                } else {
                    continue; // the edge leaves our loop
                }
            }
        }
    }

} // collectEligibleEdges2

void
BBPolling::collectEligibleEdgesRecursive()
{
    for (uint32 i=0; i < loopHeaders.size(); i++)
    {
        collectEdgesDeeper(loopHeaders[i]);
    }
    std::sort(eligibleEdges.begin(), eligibleEdges.end());
    StlVector<Edge*>::iterator newEnd = std::unique(eligibleEdges.begin(), eligibleEdges.end());
    eligibleEdges.resize(newEnd - eligibleEdges.begin());

} // collectEligibleEdgesRecursive

void
BBPolling::collectEdgesDeeper(Node* node)
{
    uint32 id = node->getId();
    if (hasNativeInterruptablePoint[id]) {
        return;
    }
    if ( isOnThreadInterruptablePath[id] ) {
        // if the node has an outgoing edge to the node which:
        //  - is not OnThreadInterruptablePath
        //  - OR this ougoing edge points to the node itself
        // add the edge to eligibleEdges if it is not a loopExit
        const Edges& edges = node->getEdges(Direction_Out);
        for (Edge* e = edges.getFirst(); e != NULL; e = edges.getNext(e)) {
            Node* succ = e->getNode(Direction_Head);
            uint32 succId = succ->getId();
            if ( succ == node ) {
                eligibleEdges.push_back(e);
                loopHeaderOfEdge[e] = node;
            }
            Node* nodeLH = node->isLoopHeader() ? node : node->getLoopHeader();
            assert(nodeLH);
            if ( nodeLH == succ->getLoopHeader() ) {
                if(!isOnThreadInterruptablePath[succId])
                {
                    eligibleEdges.push_back(e);
                    loopHeaderOfEdge[e] = nodeLH;
                } else {
                    if (succ->isLoopHeader()) {
                        continue; // there are no eligible edges deeper (nested loop)
                    } else {
                        collectEdgesDeeper(succ);
                    }
                }
            } else if (e->isBackEdge()) {
                // get a backedge and have not met any interruptable points earlier
                eligibleEdges.push_back(e);
                loopHeaderOfEdge[e] = nodeLH;
            } else {
                continue; // the edge leaves our loop
            }
        }
    } else {
        assert(0);
    }
} // collectEdgesDeeper

void
BBPolling::dumpEligibleEdges()
{
    assert(Log::cat_cg()->isDebugEnabled());
    Log::cat_cg()->out() << "    EligibleEdges:" << ::std::endl;
    for (uint32 i = 0; eligibleEdges.size() > i; i++) {
        Edge* e = eligibleEdges[i];
        uint32 srcId  = e->getNode(Direction_Tail)->getId();
        uint32 succId = e->getNode(Direction_Head)->getId();
        Log::cat_cg()->out() << "        eligibleEdge ["<<srcId<<"]-->["<<succId<<"]" << ::std::endl;
    }
    Log::cat_cg()->out() << "    EligibleEdges END! " << ::std::endl;
}

#ifdef _DEBUG

bool
BBPolling::isEligible(Edge* e)
{
    return ::std::find(eligibleEdges.begin(),eligibleEdges.end(),e) == eligibleEdges.end();
} // isEligible

void
BBPolling::verify()
{
    if (Log::cat_cg()->isDebugEnabled())
        Log::cat_cg()->out() << "BBPolling verification started" << ::std::endl;
    interruptablePoints = 0;
    pollingPoints = 0;

    Node* unwind = irManager.getUnwindNode();
    Node* exit = irManager.getExitNode();

    for (uint32 i=0; i < loopHeaders.size(); i++)
    {
        assert(interruptablePoints == 0);
        assert(pollingPoints == 0);

        Node* loopHeader = loopHeaders[i];
        bool nativelyInterruptable = BBPolling::hasNativeInterruptablePoints(loopHeader);

        if(nativelyInterruptable)
            interruptablePoints++;
            
        if (Log::cat_cg()->isDebugEnabled())
            Log::cat_cg()->out() << "   verification for loopHeader id=" << loopHeader->getId() << "STARTED" << ::std::endl;
        verifyDeeper(loopHeader,unwind,exit);

        if (Log::cat_cg()->isDebugEnabled())
            Log::cat_cg()->out() << "   verification for loopHeader id=" << loopHeader->getId() << "FINISHED" << ::std::endl;
        if(nativelyInterruptable)
            interruptablePoints--;
    }

    assert(interruptablePoints == 0);
    assert(pollingPoints == 0);
    if (Log::cat_cg()->isDebugEnabled())
        Log::cat_cg()->out() << "BBPolling verification successfully finished" << ::std::endl;
} // verify

void
BBPolling::verifyDeeper(Node* node, Node* unwind, Node* exit)
{
    const Edges& edges = node->getEdges(Direction_Out);
    if (Log::cat_cg()->isDebugEnabled())
        Log::cat_cg()->out() << "   verification:  NODE id=" << node->getId() << ::std::endl;

    for (Edge* e = edges.getFirst(); e != NULL; e = edges.getNext(e))
    {
        bool eligible = isEligible(e);
        Node* succ = e->getNode(Direction_Head);

        if ( e->isLoopExit() || succ == unwind ) {
            continue;
        }
        if (Log::cat_cg()->isDebugEnabled())
            Log::cat_cg()->out() << "   verification:  succ id=" << succ->getId() << ::std::endl;
        if ( e->isBackEdge() ) {
            if (Log::cat_cg()->isDebugEnabled())
                Log::cat_cg()->out() << "   verification BackEdge ["<<node->getId()<<"]-->["<<succ->getId()<<"]" << ::std::endl;
            if(pollingPoints == 0 && eligible) 
                continue;
            if(pollingPoints == 1 && !eligible)
                continue;
            if(pollingPoints == 0 && interruptablePoints > 0 && !eligible)
                continue;
            assert(0);
        }
        if (succ->isLoopHeader()) { // nested loop
            if (Log::cat_cg()->isDebugEnabled())
                Log::cat_cg()->out() << "   verification NestedLoop" << ::std::endl;
            if(pollingPoints == 0 && !eligible)
                continue;
            assert(0);
        }

        bool nativelyInterruptable = BBPolling::hasNativeInterruptablePoints(succ);
        if (nativelyInterruptable)
            interruptablePoints++;
        if (eligible)
            pollingPoints++;

        verifyDeeper(succ,unwind,exit);

        if (nativelyInterruptable)
            interruptablePoints--;
        if (eligible)
            pollingPoints--;
    }
} // verifyDeeper

#endif // _DEBUG

}}; // namespace Ia32

