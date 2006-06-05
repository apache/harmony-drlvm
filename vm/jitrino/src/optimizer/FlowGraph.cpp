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
 * @version $Revision: 1.21.12.2.4.4 $
 *
 */

#include <stdlib.h>
#include <string.h>
#include "Opcode.h"
#include "Type.h"
#include "FlowGraph.h"
#include "Dominator.h"
#include "Stack.h"
#include "IRBuilder.h"
#include "Log.h"
#include "Timer.h"
#include "Profiler.h"
#include "escapeanalyzer.h"
#include "TranslatorIntfc.h"
#include "CGSupport.h"

namespace Jitrino {

CFGEdge::Kind 
CFGEdge::getEdgeType() {
    CFGEdge::Kind kind = CFGEdge::Unknown;
    //
    // determine kind
    //
    CFGNode* node = getSourceNode();
    CFGNode* succ = getTargetNode();
    if (succ->isDispatchNode()) 
        kind = CFGEdge::Exception;
    else if(succ->isExitNode())
        kind = CFGEdge::Exit;
    else if (node->isDispatchNode())
        kind = CFGEdge::Catch;
    else { // true, false or unconditional
        Inst* last = node->getLastInst();
        assert(last != NULL);
        if (last->getOpcode() == Op_Branch) {
            BranchInst* br = (BranchInst*)last;
            // cfgnode's first inst is a label inst
            kind = (br->getTargetLabel() == succ->getFirstInst())?
                   CFGEdge::True : CFGEdge::False;
        }
        else if (last->isSwitch()) {
            SwitchInst* sw = (SwitchInst *)last;
            kind = (sw->getDefaultTarget() == succ->getFirstInst())?
                   CFGEdge::False : CFGEdge::Switch;
        }
        else 
            kind = CFGEdge::Unconditional;  // jump
    }
    return kind;
}


//
// assign depth first numbering starting from the prolog node
//

uint32  FlowGraph::assignDepthFirstNum() {
    dfnMap.clear();
    getNodesPreOrder(dfnMap);
    return (uint32) dfnMap.size();
}

//
// orders the nodes on a breadth-first manner
//
void   FlowGraph::orderDepthFirst() {
    MemoryManager mm((getMaxNodeId() + 1) * sizeof(CFGNode*), "FlowGraph::orderDepthFirst");
    CFGNodeDeque workSet(mm);
    ControlFlowGraph::getNodesPreOrder((ControlFlowGraph::NodeList&) workSet);

    CFGNodeDeque& nodes = (CFGNodeDeque&) getNodes();
    nodes.clear();
    nodes.splice(nodes.begin(), workSet);
}

//
// as the name says, create a basic block, and put it in the node list !
//

CFGNode*
FlowGraph::createBlockNode()
{
    return createBlock(instFactory.makeLabel());
}

CFGNode*
FlowGraph::createCatchNode(uint32 order, Type* exceptionType)
{
    return createBlock(instFactory.makeCatchLabel(order, exceptionType));
}

CFGNode* FlowGraph::createBlock(LabelInst* label) {
    CFGNode* bb = new (memManager) CFGNode(memManager, label);
    label->setCFGNode(bb);
    addNode(bb);  // append bb into the node list
    return bb;
}

CFGNode*
FlowGraph::createDispatchNode()
{
    LabelInst* label = instFactory.makeLabel();
    CFGNode* dn = new (memManager) CFGNode(memManager, label, CFGNode::Dispatch);
    label->setCFGNode(dn);
    addNode(dn);
    return dn;
}

CFGNode*
FlowGraph::createExitNode()
{
    LabelInst* label = instFactory.makeLabel();
    CFGNode* dn = new (memManager) CFGNode(memManager, label, CFGNode::Exit);
    label->setCFGNode(dn);
    addNode(dn);
    return dn;
}

// same as before, but put the new node after a known node


CFGNode* FlowGraph::createBlockAfter(CFGNode *node) {
    return createBlockAfter(instFactory.makeLabel(),node);
}

CFGNode* FlowGraph::createBlockAfter(LabelInst *label, CFGNode *node) {
    CFGNode* bb = new (memManager) CFGNode(memManager, label);
    label->setCFGNode(bb);
    label->setState(((LabelInst*)node->getFirstInst())->getState());

    CFGNodeDeque& nodes = (CFGNodeDeque&) getNodes();
    CFGNodeDeque::iterator i;
    if (node == NULL) {
        i = nodes.begin();
    } else {
        i = ::std::find(nodes.begin(), nodes.end(), node);
        assert(i != nodes.end());
        ++i;
    }
    
    addNode((ControlFlowGraph::NodeList::iterator&) i, bb);

    return bb;
}


CFGNode* FlowGraph::duplicateBlockAfter(LabelInst *label, CFGNode *node, CFGNode *source) {
    assert(!source->isExitNode() && (source != getUnwind()) && (source != getReturn()));
    CFGNode* bb = new (memManager) CFGNode(memManager, label, source->getKind());
    bb->setFlags(source->getFlags());

    // the rest is the same as createBlockAfter !!!

    label->setCFGNode(bb);

    CFGNodeDeque& nodes = (CFGNodeDeque&) getNodes();
    CFGNodeDeque::iterator i;
    if (node == NULL) {
        i = nodes.end();
    } else {
        label->setState(((LabelInst*)node->getFirstInst())->getState());
        i = ::std::find(nodes.begin(), nodes.end(), node);
        assert(i != nodes.end());
        ++i;
    }

    addNode((ControlFlowGraph::NodeList::iterator&) i, bb);

    return bb;
}

void 
FlowGraph::_moveInstructions(Node* _fromNode, Node* _toNode, bool prepend) 
{
    CFGNode* fromNode = (CFGNode*) _fromNode;
    CFGNode* toNode = (CFGNode*) _toNode;

    Inst* to = prepend ? toNode->getFirstInst() : toNode->getLastInst();
    Inst* from = fromNode->getFirstInst();

    for (Inst *inst = from->next(); inst != from; ) {
        Inst *next = inst->next();
        inst->unlink();
        inst->insertAfter(to);
        to = inst;
        assert(prepend || to == toNode->getLastInst());
        inst = next;
    }
}

void
FlowGraph::_removeBranchInstruction(ControlFlowNode* _source)
{
    CFGNode* source = (CFGNode*) _source;

    // Remove branch instruction
    Inst* last =  source->getLastInst();
    assert(last != NULL);
    if (last->getOpcode() == Op_Branch || last->isSwitch()) {
        last->unlink();
    }
}

void
FlowGraph::_updateBranchInstruction(ControlFlowNode* _source, ControlFlowNode* _oldTarget, ControlFlowNode* _newTarget)
{
    CFGNode* source = (CFGNode*) _source;
    CFGNode* oldTarget = (CFGNode*) _oldTarget; 
    CFGNode* newTarget = (CFGNode*) _newTarget; 
    assert(oldTarget->getKind() == newTarget->getKind());
        
    LabelInst*  oldLabel = oldTarget->getLabel();
    LabelInst*  newLabel = newTarget->getLabel();

    // Update source nodes branch instruction
    Inst* last =  source->getLastInst();
    assert(last != NULL);
    if (last->getOpcode() == Op_Branch) {
        BranchInst* br = (BranchInst*)last;
        if(br->getTargetLabel() == oldLabel) {
            assert(newTarget->isBlockNode());
            br->replaceTargetLabel(newLabel);
        }
    }
    else if (last->isSwitch()) {
        SwitchInst* sw = (SwitchInst *)last;
        if (sw->getDefaultTarget() == oldLabel) {
            assert(newTarget->isBlockNode());
            sw->replaceDefaultTargetLabel(newLabel);
        }
        uint32 n = sw->getNumTargets();
        for (uint32 i = 0; i < n; i++) {
            if (sw->getTarget(i) == oldLabel) {
                assert(newTarget->isBlockNode());
                sw->replaceTargetLabel(i, newLabel);
            }
        }
    }    
}


// Splice a new empty block on the CFG edge.
CFGNode* FlowGraph::spliceBlockOnEdge(CFGEdge* edge)
{
    CFGNode*    source = edge->getSourceNode();
    double edgeProb = edge->getEdgeProb();
    double edgeFreq = source->getFreq()*edgeProb;

    // Split the edge
    CFGNode*    split = (CFGNode*) splitEdge(edge);
    split->setFreq(edgeFreq);

    // Set the incoming edge probability
    assert(split->getInDegree() == 1);
    split->getInEdges().front()->setEdgeProb(edgeProb);

    // Set the outgoing edge probability
    assert(split->getOutDegree() == 1);
    split->getOutEdges().front()->setEdgeProb(1.0);

    return split;
}


CFGEdge*     
FlowGraph::addEdge(CFGNode* source, CFGNode* target) 
{ 
    CFGEdge* edge = (CFGEdge*) source->findTarget(target);
    if(edge == NULL) {
		edge = new (memManager) CFGEdge; 
		ControlFlowGraph::addEdge(source, target, edge); 
	}
    return edge; 
}

CFGEdge*     
FlowGraph::replaceEdgeTarget(ControlFlowEdge* _edge, ControlFlowNode* newTarget) 
{
    CFGEdge* edge = (CFGEdge*) _edge;
    double prob = edge->getEdgeProb();
    CFGEdge* newEdge = (CFGEdge*) ControlFlowGraph::replaceEdgeTarget(edge, newTarget);
    newEdge->setEdgeProb(prob);
    return newEdge;
}


void         
FlowGraph::foldBranch(CFGNode* block, BranchInst* br, bool isTaken)
{
    assert(br == block->getLastInst());
    assert(block->getOutDegree() == 2);
    if(isTaken)
        removeEdge(block->getFalseEdge());
    else
        removeEdge(block->getTrueEdge());
    br->unlink();
}

void         
FlowGraph::foldSwitch(CFGNode* block, SwitchInst* sw, uint32 index)
{
    assert(sw == block->getLastInst());
    LabelInst* target;
    if(index < sw->getNumTargets())
        target = sw->getTarget(index);
    else
        target = sw->getDefaultTarget();

    CFGEdgeDeque::const_iterator i;
    for(i = block->getOutEdges().begin(); i != block->getOutEdges().end();) {
        CFGEdge* edge = *i;
        ++i;
        if(edge->getTargetNode()->getLabel() != target)
            removeEdge(edge);
    }
    assert(block->getOutDegree() == 1);
    sw->unlink();
}

void         
FlowGraph::eliminateCheck(CFGNode* block, Inst* check, bool alwaysThrows)
{
    assert(check == block->getLastInst() && 
           check->getOperation().canThrow());

    if (alwaysThrows) {
#ifndef NDEBUG
        Inst *prevInst = check->prev();
        assert(prevInst->isThrow());
#endif
        ControlFlowGraph::Edge* edge = block->getUnconditionalEdge();
        assert(edge != NULL);
        removeEdge(edge);
    } else {
        ControlFlowGraph::Edge* edge = block->getExceptionEdge();
        assert(edge != NULL);
        removeEdge(edge);
    }

    check->unlink();
}

//
// prints a readable version of the node information
//
void CFGNode::printLabel(::std::ostream& cout) {
    LabelInst *first = (LabelInst*)getFirstInst();
    if (isBasicBlock()) {
        if(getInDegree() == 0)
            cout << "ENTRY";
        else if(getOutDegree() == 1 && getOutEdges().front()->getTargetNode()->isExitNode())
            cout << "RETURN";
        else
            first->printId(cout);
    } else if (isDispatchNode()) {
        if(getOutDegree() == 1 && getOutEdges().front()->getTargetNode()->isExitNode())
            cout << "UNWIND";
        else
            cout << "D" << (int32)first->getLabelId();
    } else {
		cout << "EXIT";
	}
}

//
// print instructions of a basic block
//

void CFGNode::printInsts(::std::ostream& cout, uint32 indent) {
    assert(insts != NULL);
    ::std::string indentstr(indent, ' ');
    Inst* inst = insts;
    do {
        cout << indentstr.c_str();
        inst->print(cout); 
        cout << ::std::endl;
        inst = inst->next();
    } while (inst != insts);

    // The IR does not contain explicit GOTOs, so for IR printing purposes
    // we should print a GOTO if required, extracted from the CFG.
    inst = getLastInst();
    if (!inst->isReturn() && !inst->isLeave() &&
        !inst->isThrow()  &&
        !inst->isEndFinally() && !inst->isRet()) {
        CFGNode *target = NULL;
        if (inst->isConditionalBranch()) {
            target = ((BranchInst*)inst)->getTakenEdge(FALSE)->getTargetNode();
        } else {
            CFGEdgeDeque::const_iterator
                j = getOutEdges().begin(),
                jend = getOutEdges().end();
            for (; j != jend; ++j) {
                CFGEdge* edge = *j;
                CFGNode *node = edge->getTargetNode();
                if (!node->isDispatchNode()) {
                    target = node;
                    break;
                }
            }
        }
        if (target != NULL) {
            cout << indentstr.c_str() << "GOTO ";
            target->printLabel(cout);
            cout << ::std::endl;
        }
    }
}


//
// Print the profile information of a basic block
//
void     
CFGNode::printProfile(::std::ostream& cout)
{
    char    str[16];

    sprintf(str, "%u", getId()); 
    cout    << ::std::endl
            << "// Node " << str << " : " << " Freq " << freq;
    
    if ( getOutEdges().size() > 0 ) {
        cout << ", Target Taken Probability : ";
        CFGEdgeDeque::const_iterator j = getOutEdges().begin();
        CFGEdgeDeque::const_iterator jend = getOutEdges().end();
        for (; j != jend; ++j) {
            CFGEdge* edge = *j;
            CFGNode *node = edge->getTargetNode();
            sprintf(str, "%u", node->getId()); 
            cout << " N" << str << " (" << edge->getEdgeProb() << ")";
        }
    }
    cout << ::std::endl;
}


void FlowGraph::printInsts(::std::ostream& cout, MethodDesc& methodDesc, CFGNode::Annotator* annotator) {
    const char* methodName = methodDesc.getName();
    cout    << ::std::endl
            << "--------  irDump: "
            << methodDesc.getParentType()->getName()
            << "::" << methodName << "  --------" << ::std::endl;
    print(cout, annotator);
}


/******* collection of utilities to duplicate basic blocks, inline, etc... ***/


// Renumber all labels in byte code order after inlining.  
void FlowGraph::renumberLabels() {
    uint32 labelId = 0x7fffffff;
    CFGNodeDeque::const_iterator i;
    for(i = getNodes().begin(); i != getNodes().end(); ++i) {
        CFGNode* node = *i;
        LabelInst *label = (LabelInst*)node->getFirstInst();
        uint32 n = label->getLabelId();
        if (n < labelId)
            labelId = n;
    }
    
    for(i = getNodes().begin(); i != getNodes().end(); ++i) {
        CFGNode* node = *i;
        LabelInst *label = (LabelInst*)node->getFirstInst();
        label->setLabelId(labelId++);
    }
}

void FlowGraph::cleanupPhase() {
	static Timer * cleanupPhaseTimer=NULL;
	PhaseTimer tm(cleanupPhaseTimer, "ptra::fg::cleanupPhase");
    purgeUnreachableNodes();
    inlineJSRs();
    renumberLabels();

	{
	static Timer * cleanupPhaseInternalTimer=NULL;
	PhaseTimer tm(cleanupPhaseInternalTimer, "ptra::fg::cleanupPhase::in");
    // Cleanup optimizations
    CFGNodeDeque::const_iterator niter;
    niter = getNodes().begin();
    while(niter != getNodes().end()) {
        CFGNode* node = *niter;
        ++niter;
        if (node->isDispatchNode() || node == getReturn() || node == getExit()) {
            continue;
        }
        Inst *last = node->getLastInst();
        if (last->getOpcode() == Op_TauMonitorExit) {
            // Check for monitor exit loop.  
            breakMonitorExitLoop(node, last);
        }
        else if (last->isBranch()) {
            if (last->isLeave()) {
				// Inline Finally - only necessary after translation
                inlineFinally(node);
                renumberLabels();
            } else if (last->isConditionalBranch()) { // loop peeling for conditional branches
                CFGNode *target  = ((BranchInst*)last)->getTargetLabel()->getCFGNode();
                if (target->getLabelId() > node->getLabelId()) {
                    LabelInst *targetLabel = ((BranchInst*)last)->getTargetLabel();
                    CFGNode *fthru = NULL, *taken = NULL;
                    const CFGEdgeDeque& outEdges = node->getOutEdges();
                    CFGEdgeDeque::const_iterator eIter;
                    for(eIter = outEdges.begin(); eIter != outEdges.end(); ++eIter) {
                        CFGEdge* edge = *eIter;
                        CFGNode * tar = edge->getTargetNode();
                        if (!tar->isDispatchNode()) {
                            if (tar->getFirstInst() == targetLabel) {
                                taken = tar;
                            } else {
                                fthru = tar; 
                            }
                        }
                    }
                    Inst *takenLdConst = taken->getFirstInst()->next();
                    Inst *fthruLdConst = fthru->getFirstInst()->next();
                    Inst *takenStVar   = takenLdConst->next();
                    Inst *fthruStVar   = fthruLdConst->next();
                    if (taken->hasOnlyOneSuccEdge()                             &&
                        taken->hasOnlyOnePredEdge()                             &&
                        fthru->hasOnlyOneSuccEdge()                             &&
                        fthru->hasOnlyOnePredEdge()                             &&
                        (taken->getOutEdges().front()->getTargetNode() ==
                         fthru->getOutEdges().front()->getTargetNode())         &&
                        takenLdConst->getOpcode()       == Op_LdConstant        &&
                        fthruLdConst->getOpcode()       == Op_LdConstant        &&
                        takenLdConst->getType()         == Type::Int32          &&
                        fthruLdConst->getType()         == Type::Int32          &&
                        takenStVar->getOpcode()         == Op_StVar             &&
                        fthruStVar->getOpcode()         == Op_StVar             &&
                        takenStVar->getDst()            == fthruStVar->getDst() &&
                        takenStVar->next()->getOpcode() == Op_Label             &&
                        fthruStVar->next()->getOpcode() == Op_Label) {

                        int takenint32 = ((ConstInst*)takenLdConst)->getValue().i4;
                        int fthruint32 = ((ConstInst*)fthruLdConst)->getValue().i4;
                        CFGNode *meet = taken->getOutEdges().front()->getTargetNode();
                        // find the ldVar for the variable, if any
                        Inst *meetLdVar = meet->getFirstInst()->next();
                        while (meetLdVar->getOpcode()==Op_LdVar) {
                            if (meetLdVar->getSrc(0) == takenStVar->getDst())
                                break;
                            meetLdVar = meetLdVar->next();
                        }
                        if ((((takenint32==0) && (fthruint32==1)) ||
                             ((takenint32==1) && (fthruint32==0))) &&
                            meetLdVar->getOpcode()==Op_LdVar &&
                            last->getNumSrcOperands()==2) {
                            // change the instruction to reflect the compare instruction
                            removeEdge(node,taken);
                            removeEdge(node,fthru);
                            addEdge(node,meet);
                            Opnd* dst = opndManager.createSsaTmpOpnd(meetLdVar->getDst()->getType());
                            BranchInst *lastBranch = (BranchInst*)last;
                            if (takenint32==0) 
                                lastBranch->swapTargets(NULL);
                            Inst *cmp = instFactory.makeCmp(
                                  lastBranch->getComparisonModifier(),
                                  lastBranch->getType(), dst, 
                                  lastBranch->getSrc(0),
                                  lastBranch->getSrc(1));
                            cmp->insertBefore(lastBranch);
                            Inst* newStVar = instFactory.makeStVar(meetLdVar->getSrc(0)->asVarOpnd(),
                                                                   dst);
                            newStVar->insertBefore(lastBranch);
                            lastBranch->unlink();
                        }
                    }
                }
            }
        }
        // remove trivial basic blocks
        if (node->hasOnlyOnePredEdge()) {
            CFGNode *pred = node->getInEdges().front()->getSourceNode();
            if (!pred->isDispatchNode() &&  !pred->getLastInst()->isSwitch() &&
                (pred->hasOnlyOneSuccEdge() || 
                                            (node->isEmpty() && node->hasOnlyOneSuccEdge()))) {                

                // don't merge if the node has an exception edge 
                CFGEdge *edge = NULL;
                CFGEdgeDeque::const_iterator eiter;
                for(eiter = node->getOutEdges().begin(); eiter != node->getOutEdges().end(); ++eiter) {
                    edge = *eiter;
                    if ((edge->getTargetNode())->isDispatchNode()) {
                        break;
                    }
                }
                // If the node has an exception edge, then merging is potentially illegal, so
                // skip.
                if (edge == NULL) {
                    if (Log::cat_opt()->isDebugEnabled()) {
                        Log::out()<<" MERGE ";pred->printLabel(Log::out());node->printLabel(Log::out());Log::out()<<"\n";
                        Log::out().flush();
                    }
                    BranchInst *branch = NULL;
                    if (!pred->hasOnlyOneSuccEdge() && node->isEmpty()) {
                        Inst* lastPred = pred->getLastInst();
                        CFGNode* nodeSucc = node->getOutEdges().front()->getTargetNode();
                        if (lastPred->isBranch()) {
                            branch = (BranchInst*)lastPred;
                            if (branch->getTargetLabel() == node->getFirstInst()) {
                                branch->replaceTargetLabel((LabelInst*)nodeSucc->getFirstInst());
                            }
                        }
                    }
                    mergeWithSingleSuccessor(pred,node);


                    // remove useless branches
                    if (branch != NULL) {
                        CFGNode *target = NULL;
                        for(eiter=pred->getOutEdges().begin(); eiter!=pred->getOutEdges().end();) {
                            CFGEdge* edge = *eiter;
                            ++eiter;
                            CFGNode *succ = edge->getTargetNode();
                            if (! succ->isDispatchNode()) {
                                if (target == succ) {
                                    removeEdge(pred,succ);
                                    branch->unlink();
                                    break;
                                }
                                target = succ;
                            }
                        }
                    }
                }
            }
        }
    }
	}
    //
    // a quick cleanup of unreachable and empty basic blocks
    //
    purgeUnreachableNodes();
    purgeEmptyNodes(false);
}

void 
FlowGraph::breakMonitorExitLoop(CFGNode* node, Inst* monExit) {

    // Look for the following type of loop where a handler with a 
    // monitorexit handles itself.  
    // D1:
    // 
    // B1:
    //   t1 = catch
    //   ...
    //
    // B2:
    //   monitorExit t2 ---> D1
    assert(node->getLastInst() == monExit);
    assert(monExit->getOpcode() == Op_TauMonitorExit);

    CFGNode* dispatch = (CFGNode*) node->getExceptionEdge()->getTargetNode();
    CFGNode* catchBlock = node;
    ::std::vector<CFGEdge*> exceptionEdges;
    exceptionEdges.push_back((CFGEdge*) node->getExceptionEdge());

    // Look for a catchBlock that dominates the node.  Assume no control flow.
    while(!catchBlock->getFirstInst()->isCatchLabel() && catchBlock->getInEdges().size() == 1) {
        catchBlock = catchBlock->getInEdges().front()->getSourceNode();
        CFGEdge* exceptionEdge = (CFGEdge*) catchBlock->getExceptionEdge();
        if(exceptionEdge == NULL || exceptionEdge->getTargetNode() != dispatch)
            return;
        exceptionEdges.push_back(exceptionEdge);
    }

    if(catchBlock->getFirstInst()->isCatchLabel()) {
        // We are in a catch.
        if(dispatch->findTarget(catchBlock)) {
            // We have a cycle!
            CFGNode* newDispatch = (CFGNode*) dispatch->getExceptionEdge()->getTargetNode();
            assert(newDispatch != NULL);

            ::std::vector<CFGEdge*>::iterator i;
            for(i = exceptionEdges.begin(); i != exceptionEdges.end(); ++i) {
                CFGEdge* exceptionEdge = *i;
                assert(exceptionEdge->getTargetNode() == dispatch);
                replaceEdgeTarget(exceptionEdge, newDispatch);
            }
        }
    }
}

void
FlowGraph::spliceFlowGraphInline(CFGNode* callNode, Inst* call, FlowGraph& inlineFG) {
    assert(callNode->getLastInst() == call);
    CFGNode* nextNode = (CFGNode*) callNode->getUnconditionalEdge()->getTargetNode();
    CFGNode* dispatchNode = (CFGNode*) callNode->getExceptionEdge()->getTargetNode();

    // Unlink call.
    call->unlink();
    removeEdge(callNode->getUnconditionalEdge());
    removeEdge(callNode->getExceptionEdge());

    // Splice inlineFG into this CFG.

    // Need to renumber the edge id from the inlined CFG so that the edge
    //  profiler can work properly.

    // Add nodes to CFG
	CFGNode* entryNode = inlineFG.getEntry();
	CFGNode* returnNode = inlineFG.getReturn();
	CFGNode* unwindNode = inlineFG.getUnwind();
	CFGNode* exitNode = inlineFG.getExit();
	
	bool returnSeen = false;
	bool unwindSeen = false;

    const CFGNodeDeque& nodes = inlineFG.getNodes();
    CFGNodeDeque::const_iterator i;
    for(i = nodes.begin(); i != nodes.end(); ++i) {
        if(*i != exitNode) {
            addNode(*i);
            const CFGEdgeDeque& oEdges = (*i)->getOutEdges();
            CFGEdgeDeque::const_iterator e;
            for(e = oEdges.begin(); e != oEdges.end(); ++e) {
                setNewEdgeId(*e);
            }
		}
		if(*i == returnNode)
			returnSeen = true;
		if(*i == unwindNode)
			unwindSeen = true;
    }

    // Connect nodes.
    addEdge(callNode, entryNode)->setEdgeProb(1);
	if(returnSeen) { 
		removeEdge(returnNode, exitNode);
		addEdge(returnNode, nextNode)->setEdgeProb(1);
	}
	if(unwindSeen) {
		removeEdge(unwindNode, exitNode);
		addEdge(unwindNode, dispatchNode)->setEdgeProb(1);
	}
}


//
// the following utility will inline a very small basic block, for the time being targeted
// only at 'finally' basic blocks.
//
CFGNode * FlowGraph::duplicateCFGNode(CFGNode *source, 
                                      CFGNode *before,
                                      OpndRenameTable *renameTable) {
    CFGNode *newblock;
    Inst *first = source->getFirstInst();
    if(Log::cat_opt()->isDebugEnabled()) {
        first->print(Log::out()); Log::out() << ::std::endl;
    }
    LabelInst* l = (LabelInst*)instFactory.clone(first,opndManager,renameTable);
    newblock = duplicateBlockAfter(l,before,source);

    // go throw the 'entry' instructions and duplicate them (renaming whenever possible)
    for (Inst *inst = first->next(); inst != first; inst = inst->next()) {
        if(Log::cat_opt()->isDebugEnabled()) {
            Log::out() << "DUPLICATE ";
            Log::out().flush();
            inst->print(Log::out()); Log::out() << "\n";
            Log::out().flush();
        }

        Inst* newInst = instFactory.clone(inst,opndManager,renameTable);
        newblock->append(newInst);
        if(Log::cat_opt()->isDebugEnabled()) {
            Log::out() << "          ";
            Log::out() << (int32)inst->getNumSrcOperands();
            Log::out() << " " << (int32)inst->getOpcode() << " ";
            newInst->print(Log::out());
            Log::out() << "\n";
            Log::out().flush();
        }
    }
    return newblock;
}

void FlowGraph::renameOperandsInNode(CFGNode *node, OpndRenameTable *renameTable) {
   Inst *first = node->getFirstInst();
    for (Inst *inst = first->next(); inst != first; inst = inst->next()) {
        uint32 n = inst->getNumSrcOperands();
        for(uint32 i = 0; i < n; ++i) {
            Opnd* src = inst->getSrc(i);
            Opnd* newsrc = renameTable->getMapping(src);
            if(newsrc != NULL)
                inst->setSrc(i, newsrc);
        }
    }
}

// this utility is used to merge with a single successor node, thus eliminating the successor basic block

void FlowGraph::mergeWithSingleSuccessor(CFGNode *pred, CFGNode *succ) {
    mergeNodes(pred,succ);
}


// this utility splits a node at a particular instruction, leaving the instruction in the
// same basic block, and the successor instructions in the newly created node.
// returns the next node after the split instruction

CFGNode *FlowGraph::splitNodeAtInstruction(Inst *inst) {
    CFGNode *nextNode;
    CFGNode *node = inst->getNode();
    if (inst == node->getLastInst()) {
        assert(node->hasOnlyOneSuccEdge());
        nextNode = (CFGNode*) splitNode(node, true);
    } else {
        nextNode = (CFGNode*) splitNode(node, true);
        // now move the instructions
        for (Inst *ins = inst->next(); ins != node->getFirstInst(); ) {
             Inst *nextins = ins->next();
             nextNode->append(ins);
             ins = nextins;
        } 
    }
    nextNode->setFreq(node->getFreq());
    ((CFGEdge*) node->findTarget(nextNode))->setEdgeProb(1.0);
    return nextNode;
}

// Create a new block node in between the return node and its predecessors.

CFGNode *FlowGraph::splitReturnNode() {
    return (CFGNode*) splitNode(getReturn(), false);
}


void FlowGraph::_fixBranchTargets(Inst *newInst, CFGNodeRenameTable *nodeRenameTable) {
    // fix targets of instructions
    switch(newInst->getOpcode()) {
    case Op_Branch:
    case Op_JSR:
    case Op_Jump:
        {
        BranchInst *newBranch = (BranchInst*)newInst;
        CFGNode *tar = newBranch->getTargetLabel()->getCFGNode();
        if(nodeRenameTable->getMapping(tar) != NULL) {
            LabelInst *label = (LabelInst*)nodeRenameTable->getMapping(tar)->getFirstInst();
            newBranch->replaceTargetLabel(label);
        }
        }
        break;
    case Op_Switch:
        assert(0);
    default:
        break;
    }
}


// Finally inlining

bool FlowGraph::_inlineFinally(CFGNode *from, CFGNode *to, CFGNode *retTarget, void *info,
                            CFGNodeRenameTable *nodeRenameTable,
                            OpndRenameTable *opndRenameTable) {
    // if the node has been visited, return
    if (nodeRenameTable->getMapping(from) != NULL) return TRUE;

    // if the node is epilogue, add and edge to it
    if (to == getReturn() || to->getInfo() != info) {
       addEdge(from,to);
       return true;
    }
    CFGNode *newto = nodeRenameTable->getMapping(to);
    if (newto != NULL) {
        addEdge(from,newto);
        return true;
    }
    // start following the control flow graph, duplicating basic blocks as needed
    newto = duplicateCFGNode(to, to, opndRenameTable);
    addEdge(from,newto);
    nodeRenameTable->setMapping(to,newto);
    if (to->getLastInst()->isEndFinally()) {
        // remove the duplicate 'end finally'
        Inst *endInst = newto->getLastInst();
        endInst->unlink();
        // add edge from the new node to the target
        addEdge(newto,retTarget);
    } else {
        Log::cat_opt()->debug << "LONG FINALLY\n";

        const CFGEdgeDeque& outEdges = to->getOutEdges();
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
            CFGEdge* edge = *eiter;
            _inlineFinally(newto,edge->getTargetNode(),retTarget,info,
                           nodeRenameTable,opndRenameTable);
        }
        _fixBranchTargets(newto->getLastInst(),nodeRenameTable);
    }
    return true;
}


bool FlowGraph::inlineFinally(CFGNode *block) {
    BranchInst* leaveInst  = (BranchInst*)block->getLastInst();
    CFGNode *   retTarget  = leaveInst->getTargetLabel()->getCFGNode();
    // record input operand
    CFGNode *   entryFinally = NULL;
    const CFGEdgeDeque& outEdges = block->getOutEdges();
    CFGEdgeDeque::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        CFGEdge* edge = *eiter;
        CFGNode *node = edge->getTargetNode();
        if (node != retTarget && !node->isDispatchNode()) {
            entryFinally = node;
            break;
        }
    }
    if (Log::cat_opt()->isDebugEnabled()) {
        Log::out() << "INLINE FINALLY\n";
        entryFinally->printLabel(Log::out());
        retTarget->printLabel(Log::out());
        Log::out() << ::std::endl;
        Log::out().flush();
    }
    if (entryFinally->hasOnlyOnePredEdge()) {
        setTraversalNum(getTraversalNum()+1);
        removeEdge(block,retTarget);
        _fixFinally(entryFinally,retTarget);
    } else {
        MemoryManager inlineManager(0,"FlowGraph::inlineFinally.inlineManager");
        // prepare the hashtable for the operand rename translation
        OpndRenameTable    *opndRenameTable =
              new (inlineManager) OpndRenameTable(inlineManager,10);
        CFGNodeRenameTable *nodeRenameTable =
             new (inlineManager) CFGNodeRenameTable(inlineManager,10);
        removeEdge(block,retTarget);
        removeEdge(block,entryFinally);
        _inlineFinally(block,entryFinally,retTarget,entryFinally->getInfo(),
                   nodeRenameTable,opndRenameTable );
    }
    leaveInst->unlink();
    return TRUE;
}


// Finally inlining

void FlowGraph::_fixFinally(CFGNode *node, CFGNode *retTarget) {
    // if the node has been visited, return
    if (node->getTraversalNum() >= getTraversalNum()) return;
    node->setTraversalNum(getTraversalNum());
    Inst *last = node->getLastInst();
    if (last->isEndFinally()) {
        // remove the 'ret' instruction
        last->unlink();
        // add edge from the new node to the target
        addEdge(node,retTarget);
    } else {
        const CFGEdgeDeque& outEdges = node->getOutEdges();
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
            CFGEdge* edge = *eiter;
            _fixFinally(edge->getTargetNode(),retTarget);
        }
    }
}

void 
FlowGraph::findNodesInRegion(CFGNode* entry, CFGNode* end, StlBitVector& nodesInRegion) {
    if(Log::cat_opt()->isDebugEnabled()) {
        Log::out() << "Find nodes in region (entry = ";
        entry->printLabel(Log::out());
        Log::out() << ", exit = ";
        end->printLabel(Log::out());
        Log::out() << ")" << ::std::endl;
    }

    // Return if visited and mark.
    if(nodesInRegion.setBit(end->getId()))
        return;

    if(end != entry) {
        const CFGEdgeDeque& inEdges = end->getInEdges();
        // Must not be method entry.
        assert(!inEdges.empty());
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = inEdges.begin(); eiter != inEdges.end(); ++eiter) {
            CFGEdge* edge = *eiter;
            findNodesInRegion(entry, edge->getSourceNode(), nodesInRegion);
        }
    }
    
    assert(nodesInRegion.getBit(entry->getId()));
}

CFGNode* 
FlowGraph::tailDuplicate(CFGNode* pred, CFGNode* tail, DefUseBuilder& defUses) {
        MemoryManager mm(0,"FlowGraph::tailDuplicate.mm");

        // Set region containing only node.
        StlBitVector region(mm, getMaxNodeId()*2);
        region.setBit(tail->getId());

        // Make copy.
        CFGNode* copy = duplicateRegion(tail, region, defUses);

        // Specialize for pred.
        CFGEdge* edge = (CFGEdge*) replaceEdgeTarget(pred->findTarget(tail), copy);
		copy->setFreq(pred->getFreq()*edge->getEdgeProb());
		if(copy->getFreq() < tail->getFreq()) 
			tail->setFreq(tail->getFreq() - copy->getFreq());
		else
			tail->setFreq(0);
        return copy;
}

CFGNode* 
FlowGraph::duplicateCFGNode(CFGNode *node, StlBitVector* nodesInRegion, DefUseBuilder* defUses, OpndRenameTable* opndRenameTable) {
    if(Log::cat_opt()->isDebugEnabled()) {
        Log::out() << "DUPLICATE NODE " << ::std::endl;
        node->print(Log::out());
    }

    CFGNode *newNode;
    Inst *first = node->getFirstInst();
    LabelInst* l = (LabelInst*)instFactory.clone(first,opndManager,opndRenameTable);
    newNode = duplicateBlockAfter(l,NULL,node);

    // Copy edges
    const CFGEdgeDeque& outEdges = node->getOutEdges();
    CFGEdgeDeque::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        CFGEdge* edge = *eiter;
        CFGNode* succ = edge->getTargetNode();
        CFGEdge* newEdge = addEdge(newNode, succ);
        newEdge->setEdgeProb(edge->getEdgeProb());
    }    

    CFGNode* stBlock = NULL;

    // go throw the 'entry' instructions and duplicate them (renaming whenever possible)
    for (Inst *inst = first->next(); inst != first; inst = inst->next()) {
        if(Log::cat_opt()->isDebugEnabled()) {
            Log::out() << "DUPLICATE ";
            Log::out().flush();
            inst->print(Log::out()); Log::out() << "\n";
            Log::out().flush();
        }

        Opnd* dst = inst->getDst();
        if(dst->isSsaOpnd()) {
            assert(!dst->isSsaVarOpnd());

            //
            // Determine if dst should be promoted to a var.
            //
            ConstInst* ci = inst->asConstInst();
            VarOpnd* var = NULL;
            DefUseLink* duLink = defUses->getDefUseLinks(inst);
            for(; duLink != NULL; duLink = duLink->getNext()) {
                Inst* useInst = duLink->getUseInst();
                    if(Log::cat_opt()->isDebugEnabled()) {
                        Log::out() << "Examine use: ";
                        useInst->print(Log::out());
                        Log::out() << ::std::endl;
                    }
                CFGNode* useNode = useInst->getNode();
                assert(useNode != NULL);
                if(!nodesInRegion->getBit(useNode->getId())) {
                    //
                    // Need to promote dst.
                    //
                    if(Log::cat_opt()->isDebugEnabled()) {
                        Log::out() << "Patch use: ";
                        useInst->print(Log::out());
                        Log::out() << ::std::endl;
                    }
                    Inst* patchdef = NULL;
                    if(ci != NULL) {
                        MemoryManager mm(0,"FlowGraph::duplicateCFGNode.mm");
                        OpndRenameTable table(mm,1);
                        patchdef = instFactory.clone(ci, opndManager, &table)->asConstInst();
                        assert(patchdef != NULL);
                    } else {
                        if(var == NULL) {
                            var = opndManager.createVarOpnd(dst->getType(), false);
                            Inst* stVar = instFactory.makeStVar(var, dst);
                            if(inst->getOperation().canThrow()) {
                                stBlock = createBlockNode();
                                stBlock->append(stVar);
                                CFGNode* succ = (CFGNode*) node->getUnconditionalEdge()->getTargetNode();
                                CFGEdge* succEdge = (CFGEdge*) node->findTarget(succ);
                                stBlock->setFreq(node->getFreq()*succEdge->getEdgeProb());
                                replaceEdgeTarget(succEdge, stBlock);
                                replaceEdgeTarget(newNode->findTarget(succ), stBlock);
                                addEdge(stBlock, succ)->setEdgeProb(1.0);
                                defUses->addUses(stVar);
                                nodesInRegion->resize(stBlock->getId()+1);
                                nodesInRegion->setBit(stBlock->getId());
                            } else {
                                stVar->insertAfter(inst);
                                defUses->addUses(stVar);
                            }
                        }

                        Opnd* newUse = opndManager.createSsaTmpOpnd(dst->getType());
                        patchdef = instFactory.makeLdVar(newUse, var);
                    }

                    //
                    // Patch useInst to load from var.
                    //
                    uint32 srcNum = duLink->getSrcIndex();
                    patchdef->insertBefore(useInst);
                    defUses->addUses(patchdef);
                    useInst->setSrc(srcNum, patchdef->getDst());

                    //
                    // Update def-use chains.
                    //
                    defUses->removeDefUse(inst, useInst, srcNum);
                    defUses->addDefUse(patchdef, useInst, srcNum);
                }
            }
        }

        // Create clone inst, add to new node, and update defuse links.
        Inst* newInst = instFactory.clone(inst,opndManager,opndRenameTable);
        newNode->append(newInst);
        defUses->addUses(newInst);
    }
    if(Log::cat_opt()->isDebugEnabled()) {
        newNode->print(Log::out());
        Log::out() << "---------------" << ::std::endl;
    }
    return newNode;
}

CFGNode* 
FlowGraph::duplicateRegion(CFGNode* node, CFGNode* entry, StlBitVector& nodesInRegion, DefUseBuilder* defUses, CFGNodeRenameTable* nodeRenameTable, OpndRenameTable* opndRenameTable) {
    assert(nodesInRegion.getBit(node->getId()));
    CFGNode* newNode = nodeRenameTable->getMapping(node);
    if(newNode != NULL)
        return newNode;

    newNode = duplicateCFGNode(node, &nodesInRegion, defUses, opndRenameTable);
    nodeRenameTable->setMapping(node, newNode);

    const CFGEdgeDeque& outEdges = node->getOutEdges();
    CFGEdgeDeque::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        CFGEdge* edge = *eiter;
        CFGNode* succ = edge->getTargetNode();
        if(succ != entry && nodesInRegion.getBit(succ->getId())) {
            CFGNode* newSucc = duplicateRegion(succ, entry, nodesInRegion, defUses, nodeRenameTable, opndRenameTable);
            assert(newSucc != NULL);
            replaceEdgeTarget(newNode->findTarget(succ), newSucc);
        }
    }    

    return newNode;
}


CFGNode* 
FlowGraph::duplicateRegion(CFGNode* entry, StlBitVector& nodesInRegion, DefUseBuilder& defUses, CFGNodeRenameTable& nodeRenameTable, OpndRenameTable& opndRenameTable, double newEntryFreq) {
    CFGNode* newEntry = duplicateRegion(entry, entry, nodesInRegion, &defUses, &nodeRenameTable, &opndRenameTable);
    if(newEntryFreq == 0)
        return newEntry;
    double scale = newEntryFreq / entry->getFreq();
    assert(scale >=0 && scale <= 1);
    CFGNodeRenameTable::Iter iter(&nodeRenameTable);
    CFGNode* oldNode = NULL;
    CFGNode* newNode = NULL;
    while(iter.getNextElem(oldNode, newNode)) {
        newNode->setFreq(oldNode->getFreq()*scale);
        oldNode->setFreq(oldNode->getFreq()*(1-scale));
    }
    return newEntry;
}
    
CFGNode*
FlowGraph::duplicateRegion(CFGNode* entry, StlBitVector& nodesInRegion, DefUseBuilder& defUses, double newEntryFreq) {
    MemoryManager dupMemManager(1024, "FlowGraph::duplicateRegion.dupMemManager");
    // prepare the hashtable for the operand rename translation
    OpndRenameTable    *opndRenameTable =
        new (dupMemManager) OpndRenameTable(dupMemManager,10);
    CFGNodeRenameTable *nodeRenameTable =
        new (dupMemManager) CFGNodeRenameTable(dupMemManager,10);

    return duplicateRegion(entry, nodesInRegion, defUses, *nodeRenameTable, *opndRenameTable, newEntryFreq);
}

void
FlowGraph::inlineJSRs() 
{
    MemoryManager jsrMemoryManager(0, "FlowGraph::inlineJSRs.jsrMemoryManager");
	static Timer * inlineJSRTimer=NULL;
	PhaseTimer tm(inlineJSRTimer, "ptra::fg::inlineJSRs");

    JsrEntryInstToRetInstMap* jsr_entry_map = irManager->getJsrEntryMap();
    assert(jsr_entry_map); // must be set by translator

    if(Log::cat_opt()->isDebugEnabled()) {
        Log::out() << "JSR entry map:";
        JsrEntryInstToRetInstMap::const_iterator jsr_entry_it, jsr_entry_end;
        for ( jsr_entry_it = jsr_entry_map->begin(), 
              jsr_entry_end = jsr_entry_map->end(); 
              jsr_entry_it != jsr_entry_end; ++jsr_entry_it) {
            Log::out() << "--> [entry->ret]: ";
            jsr_entry_it->first->print(Log::out());
            Log::out() << " || ";
            jsr_entry_it->second->print(Log::out());
            Log::out() << std::endl;
        }
    }

    DefUseBuilder defUses(jsrMemoryManager);
	{
	static Timer * findJSRTimer=NULL;
	PhaseTimer tm(findJSRTimer, "ptra::fg::inlineJSRs::defuse");
    defUses.initialize(*this);
	}

    CFGNodeDeque::const_iterator niter2;
    niter2 = getNodes().begin();
    while(niter2 != getNodes().end()) {
        CFGNode* node = *niter2;
        ++niter2;
        Inst* last = node->getLastInst();
        if(last->isJSR()) {
            inlineJSR(node, defUses, *jsr_entry_map);
        }
    }
}


bool FlowGraph::inlineJSR(CFGNode *block, DefUseBuilder& defUses, JsrEntryInstToRetInstMap& entryMap) {
    // Inline the JSR call at the end of block.
    Inst* last = block->getLastInst();
    assert(last->isJSR());
    BranchInst* jsrInst   = (BranchInst*) last;

    //
    // Process entry of JSR.
    //
    CFGNode* entryJSR  = jsrInst->getTargetLabel()->getCFGNode();
    Inst *saveReturn = findSaveRet(entryJSR);
    assert(saveReturn);
    // 'stvar' should follow 'saveret' (feature of current IRBuilder)
    Inst *stVar = saveReturn->next();
    // If retVar is NULL, JSR never returns. 
    Opnd *retVar = NULL;
    if(stVar->isStVar() && (stVar->getSrc(0) == saveReturn->getDst()))
        retVar = stVar->getDst();

    //
    // Find return node for this invocation.
    //
    CFGNode* retTarget = NULL;
    const CFGEdgeDeque& outEdges = block->getOutEdges();
    CFGEdgeDeque::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        CFGEdge* edge = *eiter;
        CFGNode *node = edge->getTargetNode(); 
        if (node != entryJSR && !node->isDispatchNode()) {
            retTarget = node;
            break;
        }
    }

    if(Log::cat_opt()->isDebugEnabled()) {
        Log::out() << "INLINE JSR into ";
        block->printLabel(Log::out());
        Log::out() << ":" << ::std::endl;
        entryJSR->print(Log::out());
    }

    //
    // entryMap is a mapping [stVar -> ret inst]
    //
    if(retVar == NULL || (entryMap.find(stVar) == entryMap.end())) {
        //
        // JSR never returns.  Convert to jmp.
        //
        saveReturn->unlink();
        if(retVar != NULL)
            stVar->unlink();

        const CFGEdgeDeque& inEdges = entryJSR->getInEdges();
        for(eiter = inEdges.begin(); eiter != inEdges.end();) {
            CFGEdge* edge = *eiter;
            ++eiter;
            CFGNode *node = edge->getSourceNode();
            Inst* last = node->getLastInst();
            assert(last->isJSR());
            last->unlink();
            assert(node->getOutDegree() == 2);
            CFGNode* t1 = node->getOutEdges().front()->getTargetNode();
            CFGNode* t2 = node->getOutEdges().back()->getTargetNode();
            if(t1 == entryJSR) {
                removeEdge(node, t2);
            }
            else {
                assert(t2 == entryJSR);
                removeEdge(node, t1);
            }
        }
    } else if (entryJSR->hasOnlyOnePredEdge()) {
        // 
        // Inline the JSR in place - no duplication needed.
        //
        removeEdge(block,retTarget);
        jsrInst->unlink();

        JsrEntryCIterRange jsr_range = entryMap.equal_range(stVar);
        JsrEntryInstToRetInstMap::const_iterator i;
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            assert(i->first->getNode() == entryJSR);
            CFGNode* retNode = i->second->getNode();
            Inst* ret = retNode->getLastInst();
            assert(ret->isRet());
            assert(ret->getSrc(0) == retVar);
            ret->unlink();
            addEdge(retNode, retTarget);
        }
    } else {
        //
        // Duplicate and inline the JSR.
        //
        MemoryManager inlineManager(0,"FlowGraph::inlineJSR.inlineManager"); 

        // Find the nodes in the JSR.
        StlBitVector nodesInJSR(inlineManager, getMaxNodeId());

        JsrEntryCIterRange jsr_range = entryMap.equal_range(stVar);
        JsrEntryInstToRetInstMap::const_iterator i;
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            assert((*i).first->getNode() == entryJSR);
            findNodesInRegion(entryJSR, (*i).second->getNode(), nodesInJSR);
        }

        // prepare the hash tables for rename translation
        OpndRenameTable    *opndRenameTable =
              new (inlineManager) OpndRenameTable(inlineManager,10);
        CFGNodeRenameTable *nodeRenameTable =
             new (inlineManager) CFGNodeRenameTable(inlineManager,10);

        CFGNode* newEntry = duplicateRegion(entryJSR, nodesInJSR, defUses, *nodeRenameTable, *opndRenameTable);

        removeEdge(block,retTarget);
        removeEdge(block,entryJSR);
        jsrInst->unlink();
        addEdge(block, newEntry);

        //
        // Add edge from inline ret locations to return target.
        //
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            CFGNode* inlinedRetNode = nodeRenameTable->getMapping((*i).second->getNode());
            Inst* ret = inlinedRetNode->getLastInst();
            assert(ret->isRet());
            assert(ret->getSrc(0) == retVar);
            ret->unlink();
            addEdge(inlinedRetNode, retTarget);
        }

        //
        // delete the first two instructions of the inlined node
        // 
        assert(entryJSR != newEntry);
        saveReturn = findSaveRet(newEntry);
        assert(saveReturn);
        stVar = saveReturn->next();
        assert(stVar->isStVar() && (stVar->getSrc(0) == saveReturn->getDst()));
        retVar = stVar->getDst();

    }
    if(retVar != NULL) {
        ((VarOpnd *)retVar)->setDeadFlag(true); 
        stVar->unlink();
    }
    saveReturn->unlink();
    return TRUE;
}

//
// used to find 'saveret' that starts the node
//     'labelinst' and 'stvar' are skipped
//
Inst*
FlowGraph::findSaveRet(CFGNode* node)
{
    assert(node);
    Inst* first = node->getFirstInst();
    assert(first->getOpcode() == Op_Label);
    Inst* i;
    for ( i = first->next(); i != first; i = i->next() ) {
        Opcode opc = i->getOpcode();
        if ( opc == Op_SaveRet ) {
            return i;
        }else if ( opc != Op_LdVar ) {
            return NULL;
        }
    }
    return NULL;
}

void         
FlowGraph::smoothProfileInfo() {
    if(!hasEdgeProfile())
        return;

    MemoryManager mm(sizeof(Node*)*getMaxNodeId(), "FlowGraph::smoothProfileInfo.mm");
    StlVector<CFGNode*> nodes(mm);
    getNodesPostOrder(nodes);
    StlVector<CFGNode*>::reverse_iterator i;

    //
    // Walk over all nodes in reverse postorder.  For each node, fix outgoing edge
    // probabilities so that they approximately add up to one.
    //
    for(i = nodes.rbegin(); i != nodes.rend(); ++i) {
        CFGNode* node = *i;
        const CFGEdgeDeque& outEdges = node->getOutEdges();
        if(!outEdges.empty()) {
            double total = 0;
            double nblock = 0;
            CFGEdgeDeque::const_iterator i2;
            for(i2 = outEdges.begin(); i2 != outEdges.end(); ++i2) {
                CFGEdge* edge = *i2;
                if(edge->getTargetNode()->isBlockNode())
                    ++nblock;
                total += edge->getEdgeProb();
            }
            double scale = -1.0;
            double value = -1.0;
            if(total < 0.01) {
                value = 1.0 / nblock;
            } else {
                scale = 1 / total;
            }
            assert(scale > 0 || value > 0);
            for(i2 = outEdges.begin(); i2 != outEdges.end(); ++i2) {
                CFGEdge* edge = *i2;
                if(scale >= 0)
                    edge->setEdgeProb(scale*edge->getEdgeProb());
                else if(edge->getTargetNode()->isBlockNode())
                    edge->setEdgeProb(value);
                assert(edge->getEdgeProb() >= 0.0 && edge->getEdgeProb() <= 1.0);
            }
        }
    }

    //
    // Recompute all node frequencies based upon method entry and incoming edge probabilities.
    //
    for(i = nodes.rbegin(); i != nodes.rend(); ++i) {
        CFGNode* node = *i;
        if(node != getEntry())
            node->setFreq(0);
    }

    //
    // Iterate until frequencies converge.
    //
    bool done = false;
    while(!done) {
        done = true;
        for(i = nodes.rbegin(); i != nodes.rend(); ++i) {
            CFGNode* node = *i;
            const CFGEdgeDeque& inEdges = node->getInEdges();
            if(!inEdges.empty()) {
                double oldFreq = node->getFreq();
                double newFreq = 0.0;
                CFGEdgeDeque::const_iterator i2;
                for(i2 = inEdges.begin(); i2 != inEdges.end(); ++i2) {
                    CFGEdge* edge = *i2;
                    CFGNode* pred = edge->getSourceNode();
                    newFreq += pred->getFreq() * edge->getEdgeProb();
                }
                if(oldFreq - newFreq > 0.05*oldFreq || newFreq - oldFreq > 0.05*oldFreq) {
                    done = false;
                    node->setFreq(newFreq);
                }
            }
        }
    }
}

// Check the profile consistency of the FlowGraph.
// Return true if the profile is consistent. Otherwise, return false.
// If warn is set to TRUE, print out warning message to stderr whenever 
//  inconsistent profile is detected.
bool
FlowGraph::isProfileConsistent(char *methodStr, bool warn)
{
    const CFGNodeDeque& nodes = getNodes();
    CFGEdgeDeque::const_iterator eiter;
    CFGNodeDeque::const_iterator niter;
    CFGNode     *node, *srcNode;
    CFGEdge     *edge;
    double      f, diff;
    bool        consistent = true;

    // Always return TRUE if the FlowGraph doesn't have edge profile.
    if(!hasEdgeProfile())
        return true;

    if (warn) {
        ::std::cerr << "Check profile consistent : " << methodStr << ::std::endl;
    }

    for( niter = nodes.begin(); niter != nodes.end(); ++niter) {
        node = *niter;

        // Check whether the sum of freq from incoming edges equal the node 
        //  freq.
        const CFGEdgeDeque& iEdges = node->getInEdges();
        if (iEdges.size() > 0) {
            f = 0.0;
            for(eiter = iEdges.begin(); eiter != iEdges.end(); ++eiter) {
                edge = *eiter;
                srcNode = edge->getSourceNode();
                f += srcNode->getFreq() * edge->getEdgeProb();
            }

            diff = (f == 0.0 ?  node->getFreq() : (f - node->getFreq())/f);
            if (diff > PROFILE_ERROR_ALLOWED || diff < -PROFILE_ERROR_ALLOWED) {
                consistent = false;
                if (warn) {
                    ::std::cerr << "Warning (isProfileConsistent): NODE " 
                        << (unsigned int) node->getId() << " : Incoming freq ("
                        << f << ") != Node freq (" << node->getFreq() << ")"
                        << ::std::endl;
                }
            }
        } 

    
        // Check whether the sum of outgoing edge probability is 1.0.
        const CFGEdgeDeque& oEdges = node->getOutEdges();
        if (oEdges.size() > 0) {
            f = 0.0;
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                f += edge->getEdgeProb();
            }
        
            diff = (f > 1.0) ? (f - 1.0) : (1.0 - f);
            if (diff > PROB_ERROR_ALLOWED) {
                consistent = false;
                if (warn) {
                    ::std::cerr << "Warning (isProfileConsistent) NODE "
                        << (unsigned int) node->getId() << " : Outgoing prob ("
                        << f << ") != 1.0" << ::std::endl;
                }
            }
        }
    }
    return consistent;
}   // FlowGraph::isProfileConsistent


// Smooth the profile in the FlowGraph by deriving the profile from the edge 
//  taken probability and the method entry frequency.
void
FlowGraph::smoothProfile(MethodDesc& methodDesc)
{
    if( !hasEdgeProfile() )
        return;

    //TODO: use new algorithm in StaticProfiler (works several times faster)!

    MemoryManager   mm(getMaxEdgeId() * sizeof(Node*) * 16, 
                       "FlowGraph::smoothProfileInfo.mm");
    EdgeProfile     edgeProfile(mm, methodDesc, true);
    edgeProfile.smoothProfile(*this);
}   // FlowGraph::smoothProfile



/*********************** print Dot utilities *****************************/

void FlowGraph::printDotFile(MethodDesc& methodDesc,const char *suffix, DominatorTree *tree) {
    assert(tree == NULL || tree == irManager->getDominatorTree());
    PrintDotFile::printDotFile(methodDesc,suffix);
}


void FlowGraph::printDotBody() {
    ::std::ostream& out = *os;
    const CFGNodeDeque& nodes = getNodes();
    CFGNodeDeque::const_iterator niter;
    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        DominatorTree* dom = irManager->getDominatorTree();
        node->printDotCFGNode(out,dom,getEntry()->getFreq(), irManager);
    }
    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        CFGNode* node = *niter;
        const CFGEdgeDeque& edges = node->getOutEdges();
        CFGEdgeDeque::const_iterator eiter;
        for(eiter = edges.begin(); eiter != edges.end(); ++eiter) {
            CFGEdge* edge = *eiter;
            edge->printDotCFGEdge(out);
        }
    }
}


void CFGNode::printDotCFGNode(::std::ostream& out, DominatorTree *dom, double methodFreq, IRManager *irMng) {
    printLabel(out);
    out << " [label=\"";
    if (!isEmpty())
        out << "{";
    getFirstInst()->print(out);
    out << "tn: " << (int) getTraversalNum() << " pre:" << (int)getPreNum() << " post:" << (int)getPostNum() << " ";
    out << "id: " << (int) getId() << " ";
    if(freq > 0)
        out << " freq:" << freq << " ";
    //
    // dump immediate dom information
    // [label="{ (L2(dfn:3) | idom(L5)) | ...  if dumping dom is requested
    // {label="{ L2 (dfn:3) | ...               otherwise
    //
    if (dom != NULL && dom->isValid()) {
        out << "idom("; dom->printIdom(out,this); out << "))";
    }
    if (!isEmpty()) {
        out << "\\l|\\" << ::std::endl;
        Inst* first = getFirstInst();
        VectorHandler* bc2HIRMapHandler = NULL;
        if (irMng) {
            MethodDesc* methDesc = irMng->getCompilationInterface().getMethodToCompile();

            if (irMng->getCompilationInterface().isBCMapInfoRequired()) {
                bc2HIRMapHandler = new(irMng->getMemoryManager()) VectorHandler(bcOffset2HIRHandlerName, methDesc);
                assert(bc2HIRMapHandler);
            }
        }
        for (Inst* i = first->next(); i != first; i=i->next()) { // skip label inst
            i->print(out); 
            if (bc2HIRMapHandler != NULL) {
                uint64 bcOffset = 0;
                //POINTER_SIZE_INT instAddr = (POINTER_SIZE_INT) i;
                uint64 instID = i->getId();
                bcOffset = bc2HIRMapHandler->getVectorEntry(instID);
                if (bcOffset != ILLEGAL_VALUE) out<<" "<<"bcOff:"<< (uint16)bcOffset<<" ";
            }
            out << "\\l\\" << ::std::endl;
        }
        out << "}";
    }
    out << "\"";
    if(freq > methodFreq*10)
        out << ",color=red";
    else if(freq > 0 && freq >= 0.85*methodFreq)
        out << ",color=orange";
    if (isDispatchNode())
        out << ",shape=diamond,color=blue";
    out << "]" << ::std::endl;
}

void CFGEdge::printDotCFGEdge(::std::ostream& out) {
    CFGNode *from = getSourceNode();
    CFGNode *to   = getTargetNode();
    from->printLabel(out);
    out << " -> ";
    to->printLabel(out);
    if (to->isDispatchNode())
        out << " [style=dotted,color=blue]";
    out << ";" << ::std::endl;
}

} //namespace Jitrino 
