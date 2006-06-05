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
 * @author Intel, George A. Timoshenko
 * @version $Revision: 1.17.8.2.4.4 $
 *
 */

#include <stdlib.h>
#include <string.h>
#include "Log.h"
#include "Opcode.h"
#include "Type.h"
#include "FlowGraph.h"
#include "Stack.h"
#include "IRBuilder.h"
#include "ExceptionInfo.h"
#include "JavaFlowGraphBuilder.h"
#include "TranslatorIntfc.h"
#include "irmanager.h"

namespace Jitrino {


CFGNode* JavaFlowGraphBuilder::genBlock(LabelInst* label) {
    if (currentBlock == NULL) {
        currentBlock = fg->createBlock(label);
        assert(fg->getEntry()==NULL);
        fg->setEntry(currentBlock);
    } else {
        currentBlock = fg->createBlock(label);
    }
    return currentBlock;
}

CFGNode* JavaFlowGraphBuilder::genBlockAfterCurrent(LabelInst *label) {
    // hidden exception info
    void *exceptionInfo = ((LabelInst*)currentBlock->getFirstInst())->getState();
    currentBlock = fg->createBlockAfter(label,currentBlock);
    label->setState(exceptionInfo);
    return currentBlock;
}

CFGNode *JavaFlowGraphBuilder::createDispatchNode() {
    return fg->createDispatchNode();
}


void JavaFlowGraphBuilder::edgeForFallthrough(CFGNode* block) {
    //
    // find fall through basic block; skip handler nodes
    //
    const CFGNodeDeque& nodes = fg->getNodes();
    CFGNodeDeque::const_iterator niter = ::std::find(nodes.begin(), nodes.end(), block);
    assert(niter != nodes.end());
    TranslatorFlags& translatorFlags = *irBuilder.getIRManager().getCompilationContext()->getTranslatorFlags();
    for(++niter; niter != nodes.end(); ++niter) {
        if (translatorFlags.newCatchHandling) {
            CFGNode* node = *niter;
            if(node->isBasicBlock() && !node->getLabel()->isCatchLabel())
                break;
        } else {
            if((*niter)->isBasicBlock())
                break;
        }
    }
    assert(niter != nodes.end());
    CFGNode* fallthrough = *niter;
    fg->addEdge(block,fallthrough);
}

CFGNode * JavaFlowGraphBuilder::edgesForBlock(CFGNode* block) {
    //
    // find if this block has any region that could catch the exception
    //
    CFGNode *dispatch = NULL;
    ExceptionInfo *exceptionInfo = (CatchBlock*)((LabelInst*)block->getFirstInst())->getState();
    if (exceptionInfo != NULL) {
        dispatch = exceptionInfo->getLabelInst()->getCFGNode();
    }else{
        dispatch = fg->getUnwind();
    }
    assert(dispatch->isDispatchNode());

    //
    // split the block so that 
    //      each potentially-exceptional instruction ends a block
    //
    Inst* first = block->getFirstInst();
    Inst* last = block->getLastInst();
    Inst* lastExceptionalInstSeen = NULL;
    for (Inst* inst = first->next(); inst != first; inst = inst->next()) {
        if (lastExceptionalInstSeen != NULL) {
            // start a new basic block
            CFGNode *newblock = fg->createBlockAfter(block);
            Inst *ins = lastExceptionalInstSeen->next();
            ins->moveTailTo(first, newblock->getFirstInst());

            // now fix up the CFG, duplicating edges
            if (!lastExceptionalInstSeen->isThrow())
                fg->addEdge(block,newblock);
            //
            // add an edge to handler entry node
            //
            assert(!block->findTarget(dispatch));
            fg->addEdge(block,dispatch);
            block = newblock;
            first = block->getFirstInst();
            lastExceptionalInstSeen = NULL;
        } 
        if (inst->getOperation().canThrow()) {
            lastExceptionalInstSeen = inst;
        }
        inst->setNode(block);
    }

    // 
    // examine the last instruction and create appropriate CFG edges
    //
    switch(last->getOpcode()) {
    case Op_Jump:
        {
        fg->addEdge(block,((BranchInst*)last)->getTargetLabel()->getCFGNode());
        last->unlink();
        }
        break;
    case Op_Branch:
    case Op_JSR:
        fg->addEdge(block,((BranchInst*)last)->getTargetLabel()->getCFGNode());
        edgeForFallthrough(block);
        break;
    case Op_Throw:
    case Op_ThrowLazy:
    case Op_ThrowSystemException:
    case Op_ThrowLinkingException:
        // throw/rethrow creates an edge to a handler that catches the exception
        assert(dispatch != NULL);
        assert(lastExceptionalInstSeen == last);
        break;
    case Op_Return:
        fg->addEdge(block,fg->getReturn());
        break;
    case Op_Ret:
        break; // do  not do anything
    case Op_Switch:
        {
            SwitchInst *sw = (SwitchInst*)last;
            uint32 num = sw->getNumTargets();
            for (uint32 i = 0; i < num; i++) {
                CFGNode* target = sw->getTarget(i)->getCFGNode();
                // two switch values may go to the same block
                if (!block->findTarget(target))
                    fg->addEdge(block,target);
            }
            CFGNode* target = sw->getDefaultTarget()->getCFGNode();
            if (!block->findTarget(target))
                fg->addEdge(block,target);
        }
        break;
    default:
        if (block != fg->getReturn()) { // a fallthrough edge is needed
           // if the basic block does not have any outgoing edge, add one fall through edge
           if (block->getOutEdges().empty())
               edgeForFallthrough(block);
        }
    }
    //
    // add an edge to handler entry node
    //
    if (lastExceptionalInstSeen != NULL &&  !block->findTarget(dispatch))
        fg->addEdge(block,dispatch);
    return block;
}

void JavaFlowGraphBuilder::edgesForHandler(CFGNode* entry) {
    CatchBlock *catchBlock = (CatchBlock*)((LabelInst*)entry->getFirstInst())->getState();
    if(entry == fg->getUnwind())
        // No local handlers for unwind.
        return;
    //
    // create edges between handler entry and each catch
    //
    CatchHandler * handlers = catchBlock->getHandlers();
    for (;handlers != NULL; handlers = handlers->getNextHandler())  {
        fg->addEdge(entry,handlers->getLabelInst()->getCFGNode());
    }
    // edges for uncaught exception
    ExceptionInfo *nextBlock = NULL;
    for (nextBlock = catchBlock->getNextExceptionInfoAtOffset();
         nextBlock != NULL; nextBlock = nextBlock->getNextExceptionInfoAtOffset()) {
        if (nextBlock->isCatchBlock()) {
            CFGNode *next = nextBlock->getLabelInst()->getCFGNode();
            fg->addEdge(entry,next);
            break;
        }
    }
    if(nextBlock == NULL) {
        fg->addEdge(entry,fg->getUnwind());
    }
}

void JavaFlowGraphBuilder::createCFGEdges() {
    const CFGNodeDeque& nodes = fg->getNodes();
    CFGNodeDeque::const_iterator niter;
    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        if (node->isBasicBlock())
            node = edgesForBlock(node);
        else if (node->isDispatchNode())
            edgesForHandler(node);
        else
            assert(node == fg->getExit());
    }
}

#define WHITE 0  // node has not been touched yet
#define BLACK 1  // node can reach exit node

//
// Traverse all the nodes in a reverse DFS manner, and mark all the ndoes
// that are reachable from exit node with BLACK.
//
void JavaFlowGraphBuilder::reverseDFS( StlVector<int8>& state,
                                       CFGNode* targetNode,
                                       uint32* nodesCounter )
{
    const CFGEdgeDeque& in_edges = targetNode->getInEdges();

    assert( state[targetNode->getId()] == WHITE );
    state[targetNode->getId()] = BLACK;

    *nodesCounter += 1;

    for( CFGEdgeDeque::const_iterator in_iter = in_edges.begin(); in_iter != in_edges.end(); in_iter++ ){
        CFGNode* srcNode = (*in_iter)->getSourceNode();

        if( state[srcNode->getId()] != BLACK ){
            reverseDFS( state, srcNode, nodesCounter );
        }
    }

    return;
}

//
// Traverse all the nodes in a forward DFS manner, and add dispatches
// for while(true){;} loops, if they exist.
//
void JavaFlowGraphBuilder::forwardDFS( StlVector<int8>& state,
                                       CFGNode* srcNode )
{
    // Flip the state, so the same node will not be visited more than twice.
    state[srcNode->getId()] = ~state[srcNode->getId()];

    const CFGEdgeDeque& out_edges = srcNode->getOutEdges();

    for( CFGEdgeDeque::const_iterator out_iter = out_edges.begin(); out_iter != out_edges.end(); out_iter++ ){

        CFGNode* targetNode = (*out_iter)->getTargetNode();
        const int32 targetState = state[targetNode->getId()];

        if( targetState == BLACK ||
            targetState == WHITE ){
            // Keep searching ...
            forwardDFS( state, targetNode );

        } else if( targetState == ~WHITE &&
                   srcNode->hasOnlyOneSuccEdge() ){
            // Find an infinite loop that will never reach exit node.
            assert( state[srcNode->getId()] == ~WHITE );
            irBuilder.genLabel( (LabelInst*)targetNode->getFirstInst() );
            Opnd* args[] = { irBuilder.genTauSafe(), irBuilder.genTauSafe() };
            Type* returnType = irBuilder.getTypeManager().getVoidType();
  
            irBuilder.genJitHelperCall( PseudoCanThrow,
                                        returnType,
                                        sizeof(args) / sizeof(args[0]),
                                        args );

            ExceptionInfo* exceptionInfo =
                (CatchBlock*)((LabelInst*)targetNode->getFirstInst())->getState();
            CFGNode* dispatch = NULL;

            if( exceptionInfo != NULL) {
                dispatch = exceptionInfo->getLabelInst()->getCFGNode();
            } else {
                dispatch = fg->getUnwind();
            }

            assert( !targetNode->findTarget(dispatch) );
            fg->addEdge( targetNode, dispatch );
            state[targetNode->getId()] = ~BLACK;  // Now it can reach exit node.

            if( Log::cat_fe()->isIREnabled() ){
                MethodDesc &methodDesc = irBuilder.getIRManager().getMethodDesc();
                Log::out() << "PRINTING LOG: After resolveWhileTrue" << ::std::endl;
                targetNode->print( Log::out() );
                fg->printInsts( Log::out(), methodDesc );
            }
        }
    }

    // Don't visit ~WHITE node again.
    state[srcNode->getId()] = ~BLACK;

    return;
}

//
// Introduce a dispatch edge from an infinite loop, if exists, to the
// unwind node, so that the graph is connected from entry to exit.
//
void JavaFlowGraphBuilder::resolveWhileTrue()
{
    MemoryManager mm( fg->getMaxNodeId(), "ControlFlowGraph::resolveWhileTrue" );

    StlVector<int8> state( mm, fg->getMaxNodeId() );

    //
    // Pass 1: Identify all the nodes that are reachable from exit node.
    //
    uint32 nodesAffected = 0;
    reverseDFS( state, fg->getExit(), &nodesAffected );

    //
    // Pass 2: Add dispatches for while(true){;} loops, if they exist.
    //
    if( nodesAffected < fg->getNodes().size() ){
        forwardDFS( state, fg->getEntry() );
    }

    return;
}




//
// construct flow graph
//
void JavaFlowGraphBuilder::build() {
    //
    // create epilog, unwind, and exit
    //
    fg->setReturn(fg->createBlockNode());
    fg->setUnwind(fg->createDispatchNode());
    fg->setExit(fg->createExitNode());
    fg->addEdge(fg->getReturn(), fg->getExit());
    fg->addEdge(fg->getUnwind(), fg->getExit());
    //
    // second phase: construct edges
    //
    createCFGEdges();
    //
    // third phase: add dispatches for infinite loop(s)
    //
    resolveWhileTrue();

    eliminateUnnestedLoopsOnDispatch();
}


//
//   look for edge patterns like this:
//
//      D C
//      C S
//      S has >1 in-edges
//      D has >1 in-edges
//
//      S  -- start node (sycle header)
//      D  -- dispatch node
//      C  -- catch node
//
//   when found, give a warning (to be resolved later)
//
//   ------------------------------------------------------------------------
//
//   look for edge pattern like this:
//
//      S  ME
//      ME D
//      S  D
//      D  C
//      C  S
//      S has >1 in-edges
//      D has >2 in-edges
//
//      S  -- start node (sycle header)
//      D  -- dispatch node
//      C  -- catch node
//      ME -- node with last instruction: 'monitorexit'
//
//   when found,
//      for each in-node of D (except 1st) do
//         duplicate D -> D2, 
//         retarget the in-node to D2
//         for all nodes (Cx) following D
//           if catch node 
//             duplicate Cx -> Cy
//             add edge 'D2 Cy'
//             add edge 'Cy succ(Cx)' // assertion failure if many succsessors
//           else
//             add edge 'D2 Cx'
//
void JavaFlowGraphBuilder::eliminateUnnestedLoopsOnDispatch()
{
    MemoryManager matched_nodes_mm(3*sizeof(CFGNode*), "unnested_loops_mm");
    CFGNodeDeque matched_dispatches(matched_nodes_mm);
    bool found_goto_into_loop_warning = false;
    const CFGNodeDeque& nodes = fg->getNodes();
    CFGNodeDeque::const_iterator niter;
    for (niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* dispatch = *niter;
        if ( dispatch->isDispatchNode() && dispatch->getInDegree() > 1 ) {
            const CFGEdgeDeque& out_edges =  dispatch->getOutEdges();
            for (CFGEdgeDeque::const_iterator out_iter = out_edges.begin();
                out_iter != out_edges.end();
                out_iter++) {

                CFGNode* catch_node = (*out_iter)->getTargetNode();
                if ( !catch_node->isCatchBlock() ) {
                    continue;
                }
                // TODO: Let catch edges contain only one instruction.
                //       In that case assertions will become meaningful.
                //assert(catch_node->hasOnlyOneSuccEdge());
                //assert(catch_node->hasOnlyOnePredEdge());

                CFGNode* catch_target = (*(catch_node->getOutEdges().begin()))->getTargetNode();
                if ( catch_target->getInDegree() <= 1 ) {
                    continue;
                }
                bool found_monitorexit = false;
                bool dispatch_is_after_target = false;
                bool monitorexit_after_catch_target = false;
                const CFGEdgeDeque& in_edges = dispatch->getInEdges();
                for (CFGEdgeDeque::const_iterator in_iter = in_edges.begin();
                     in_iter != in_edges.end();
                     in_iter++) {
                    CFGNode* pre_dispatch = (*in_iter)->getSourceNode();
                    if ( pre_dispatch == catch_target ) {
                        dispatch_is_after_target = true;
                    }
                    if ( lastInstIsMonitorExit(pre_dispatch) ) {
                        found_monitorexit = true;
                        if ( pre_dispatch->getInDegree() == 1 &&
                             (*pre_dispatch->getInEdges().begin())->getSourceNode() == catch_target ) {
                            monitorexit_after_catch_target = true;
                        }
                    }
                }

                if ( dispatch_is_after_target && 
                     found_monitorexit &&
                     monitorexit_after_catch_target ) {
                    if ( dispatch->getInDegree() == 2 ) {
                        //
                        // simple monitorexit code pattern, no goto into loop here
                        //
                        continue;
                    }else if ( dispatch->getInDegree() > 2 ) {
                        // goto into loop found
                        matched_dispatches.push_back(dispatch);
                        if ( Log::cat_fe()->isDebugEnabled() ) {
                            Log::out() << "goto into loop found, fixing..." << std::endl;
                        }
                        break;
                    }
                }
                if ( !found_goto_into_loop_warning ) {
                    if ( Log::cat_fe()->isDebugEnabled() ) {
                        Log::out() << "warning: maybe goto into loop with exception" 
                                   << std::endl;
                    }
                    found_goto_into_loop_warning = true;
                }
            }
        }
    }
    for ( CFGNodeDeque::const_iterator dispatch_iter = matched_dispatches.begin();
          dispatch_iter != matched_dispatches.end();
          dispatch_iter++ ) {
        
        CFGNode* dispatch = (*dispatch_iter);
        const CFGEdgeDeque& in_edges = dispatch->getInEdges();
        CFGEdgeDeque::const_iterator in_edge_iter = in_edges.begin();
        assert(dispatch->getInDegree() > 1);
        for (in_edge_iter++; in_edge_iter != in_edges.end(); in_edge_iter++) {
           CFGEdge* in_edge = *in_edge_iter;
           CFGNode* dup_dispatch = createDispatchNode();
           fg->replaceEdgeTarget(in_edge, dup_dispatch);
           const CFGEdgeDeque& out_edges = dispatch->getOutEdges();
           for (CFGEdgeDeque::const_iterator out_edge_iter = out_edges.begin(); 
                out_edge_iter != out_edges.end();
                out_edge_iter++) {
               CFGNode* dispatch_target = (*out_edge_iter)->getTargetNode();
               if ( !dispatch_target->isCatchBlock() ) {
                   fg->addEdge(dup_dispatch, dispatch_target);
               }else{
                   CatchLabelInst* catch_label = (CatchLabelInst*)dispatch_target->getLabel();
                   CFGNode* dup_catch = fg->createCatchNode(catch_label->getOrder(), catch_label->getExceptionType());
                   fg->addEdge(dup_dispatch, dup_catch);
                   assert(dispatch_target->getOutDegree() == 1);
                   fg->addEdge(dup_catch, (*dispatch_target->getOutEdges().begin())->getTargetNode());
               }
           }
        }
    }
}

bool JavaFlowGraphBuilder::lastInstIsMonitorExit(CFGNode* node)
{
    Inst* last = node->getLastInst();
    if ( last->getOpcode() == Op_TypeMonitorExit ||
         last->getOpcode() == Op_TauMonitorExit ) {
        return true;
    }
    return false;
}

JavaFlowGraphBuilder::JavaFlowGraphBuilder(MemoryManager& mm, IRBuilder &irB) : 
    memManager(mm), currentBlock(NULL), irBuilder(irB)
{
    fg = &irBuilder.getFlowGraph();
}

} //namespace Jitrino 
