/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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

#include "Opcode.h"
#include "Type.h"
#include "FlowGraph.h"
#include "Dominator.h"
#include "Stack.h"
#include "IRBuilder.h"
#include "irmanager.h"
#include "Log.h"
#include "XTimer.h"
#include "StaticProfiler.h"
#include "escapeanalyzer.h"
#include "deadcodeeliminator.h"
#include "TranslatorIntfc.h"
#include "CGSupport.h"
#include "LoopTree.h"

#include <stdlib.h>
#include <string.h>

namespace Jitrino {

class NodeRenameTable : public HashTable<Node,Node> {
public:
    typedef HashTableIter<Node, Node> Iter;

    NodeRenameTable(MemoryManager& mm,uint32 size) :
                    HashTable<Node,Node>(mm,size) {}
    Node *getMapping(Node *node) {
        return (Node*)lookup(node);
    }
    void     setMapping(Node *node, Node *to) {
        insert(node,to);
    }
    
protected:
    virtual bool keyEquals(Node* key1,Node* key2) const {
        return key1 == key2;
    }
    virtual uint32 getKeyHashCode(Node* key) const {
        // return hash of address bits
        return ((uint32)(((POINTER_SIZE_INT)key) >> sizeof(void*)));
    }
};


void         
FlowGraph::foldBranch(ControlFlowGraph& fg, BranchInst* br, bool isTaken)
{
	Node* block = br->getNode();
	assert(block->getOutDegree() == 2);
	
    fg.removeEdge(block->getOutEdge(isTaken ? Edge::Kind_False : Edge::Kind_True));
    br->unlink();
}

void         
FlowGraph::foldSwitch(ControlFlowGraph& fg, SwitchInst* sw, uint32 index)
{
    Node* block = sw->getNode();
    assert(sw == block->getLastInst());
    LabelInst* target;
    if(index < sw->getNumTargets())
        target = sw->getTarget(index);
    else
        target = sw->getDefaultTarget();

    while (!block->getOutEdges().empty()) {
        fg.removeEdge(block->getOutEdges().back());
    }
    sw->unlink();
    fg.addEdge(block, target->getNode(), 1.0);
}


void         
FlowGraph::eliminateCheck(ControlFlowGraph& fg, Node* block, Inst* check, bool alwaysThrows)
{
    assert(check == (Inst*)block->getLastInst() &&  check->getOperation().canThrow());
    Edge* edge = NULL;
    if (alwaysThrows) {
#ifndef NDEBUG
        Inst *prevInst = check->getPrevInst();
        assert(prevInst->isThrow());
#endif
        edge = block->getUnconditionalEdge();
    } else {
        edge = block->getExceptionEdge();
    }
    assert(edge != NULL);
    fg.removeEdge(edge);
    check->unlink();
}


Node* 
FlowGraph::tailDuplicate(IRManager& irm, Node* pred, Node* tail, DefUseBuilder& defUses) {
    MemoryManager mm("FlowGraph::tailDuplicate.mm");
    ControlFlowGraph& fg = irm.getFlowGraph();

    // Set region containing only node.
    StlBitVector region(mm, fg.getMaxNodeId()*2);
    region.setBit(tail->getId());

    // Make copy.
    Node* copy = duplicateRegion(irm, tail, region, defUses);

    // Specialize for pred.
    Edge* edge = fg.replaceEdgeTarget(pred->findTargetEdge(tail), copy);
    copy->setExecCount(pred->getExecCount()*edge->getEdgeProb());
    if(copy->getExecCount() < tail->getExecCount())  {
        tail->setExecCount(tail->getExecCount() - copy->getExecCount());
    } else {
        tail->setExecCount(0);
    }
    return copy;
}

static Node* duplicateNode(IRManager& irm, Node *source, Node *before, OpndRenameTable *renameTable) {
    Node *newblock;
    LabelInst *first = (LabelInst*)source->getFirstInst();
    if(Log::isEnabled()) {
        first->print(Log::out()); Log::out() << std::endl;
    }
    InstFactory& instFactory = irm.getInstFactory();
    OpndManager& opndManager = irm.getOpndManager();
    ControlFlowGraph& fg = irm.getFlowGraph();
    LabelInst* l = (LabelInst*)instFactory.clone(first,opndManager,renameTable);
    newblock = fg.createNode(source->getKind(), l);
    l->setState(first->getState());

    // go throw the 'entry' instructions and duplicate them (renaming whenever possible)
    for (Inst *inst = first->getNextInst(); inst != NULL; inst = inst->getNextInst()) {
        if(Log::isEnabled()) {
            Log::out() << "DUPLICATE ";
            Log::out().flush();
            inst->print(Log::out()); Log::out() << "\n";
            Log::out().flush();
        }

        Inst* newInst = instFactory.clone(inst,opndManager,renameTable);
        newblock->appendInst(newInst);
        if(Log::isEnabled()) {
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


static Node* duplicateNode(IRManager& irm, Node *node, StlBitVector* nodesInRegion, 
                           DefUseBuilder* defUses, OpndRenameTable* opndRenameTable) 
{
    if(Log::isEnabled()) {
        Log::out() << "DUPLICATE NODE " << std::endl;
        FlowGraph::print(Log::out(), node);
    }

    InstFactory& instFactory = irm.getInstFactory();
    OpndManager& opndManager = irm.getOpndManager();
    ControlFlowGraph& fg = irm.getFlowGraph();

    Node *newNode;
    LabelInst *first = (LabelInst*)node->getFirstInst();
    LabelInst* l = (LabelInst*)instFactory.clone(first,opndManager,opndRenameTable);
    newNode  = fg.createNode(node->getKind(), l);
    l->setState(first->getState());

    // Copy edges
    const Edges& outEdges = node->getOutEdges();
    Edges::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        Edge* edge = *eiter;
        Node* succ = edge->getTargetNode();
        Edge* newEdge = fg.addEdge(newNode, succ);
        newEdge->setEdgeProb(edge->getEdgeProb());
    }    

    Node* stBlock = NULL;

    // go throw the 'entry' instructions and duplicate them (renaming whenever possible)
    for (Inst *inst = first->getNextInst(); inst != NULL; inst = inst->getNextInst()) {
        if(Log::isEnabled()) {
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
                if(Log::isEnabled()) {
                    Log::out() << "Examine use: ";
                    useInst->print(Log::out());
                    Log::out() << std::endl;
                }
                Node* useNode = useInst->getNode();
                assert(useNode != NULL);
                if(!nodesInRegion->getBit(useNode->getId())) {
                    //
                    // Need to promote dst.
                    //
                    if(Log::isEnabled()) {
                        Log::out() << "Patch use: ";
                        useInst->print(Log::out());
                        Log::out() << std::endl;
                    }
                    Inst* patchdef = NULL;
                    if(ci != NULL) {
                        MemoryManager mm("FlowGraph::duplicateNode.mm");
                        OpndRenameTable table(mm,1);
                        patchdef = instFactory.clone(ci, opndManager, &table)->asConstInst();
                        assert(patchdef != NULL);
                    } else {
                        if(var == NULL) {
                            var = opndManager.createVarOpnd(dst->getType(), false);
                            Inst* stVar = instFactory.makeStVar(var, dst);
                            if(inst->getOperation().canThrow()) {
                                stBlock = fg.createBlockNode(instFactory.makeLabel());
                                stBlock->appendInst(stVar);
                                Node* succ =  node->getUnconditionalEdge()->getTargetNode();
                                Edge* succEdge = node->findTargetEdge(succ);
                                stBlock->setExecCount(node->getExecCount()*succEdge->getEdgeProb());
                                fg.replaceEdgeTarget(succEdge, stBlock);
                                fg.replaceEdgeTarget(newNode->findTargetEdge(succ), stBlock);
                                fg.addEdge(stBlock, succ)->setEdgeProb(1.0);
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
        newNode->appendInst(newInst);
        defUses->addUses(newInst);
    }
    if(Log::isEnabled()) {
        FlowGraph::print(Log::out(), newNode);
        Log::out() << "---------------" << std::endl;
    }
    return newNode;
}



static Node* _duplicateRegion(IRManager& irm, Node* node, Node* entry, StlBitVector& nodesInRegion, DefUseBuilder* defUses, NodeRenameTable* nodeRenameTable, OpndRenameTable* opndRenameTable) {
    assert(nodesInRegion.getBit(node->getId()));
    Node* newNode = nodeRenameTable->getMapping(node);
    if(newNode != NULL)
        return newNode;

    newNode = duplicateNode(irm, node, &nodesInRegion, defUses, opndRenameTable);
    nodeRenameTable->setMapping(node, newNode);

    ControlFlowGraph& fg = irm.getFlowGraph();
    const Edges& outEdges = node->getOutEdges();
    Edges::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        Edge* edge = *eiter;
        Node* succ = edge->getTargetNode();
        if(succ != entry && nodesInRegion.getBit(succ->getId())) {
            Node* newSucc = _duplicateRegion(irm, succ, entry, nodesInRegion, defUses, nodeRenameTable, opndRenameTable);
            assert(newSucc != NULL);
            fg.replaceEdgeTarget(newNode->findTargetEdge(succ), newSucc);
        }
    }    

    return newNode;
}


static Node* _duplicateRegion(IRManager& irm, Node* entry, StlBitVector& nodesInRegion, DefUseBuilder& defUses, NodeRenameTable& nodeRenameTable, OpndRenameTable& opndRenameTable, double newEntryFreq) {
    Node* newEntry = _duplicateRegion(irm, entry, entry, nodesInRegion, &defUses, &nodeRenameTable, &opndRenameTable);
    if(newEntryFreq == 0) {
        return newEntry;
    }
    double scale = newEntryFreq / entry->getExecCount();
    assert(scale >=0 && scale <= 1);
    NodeRenameTable::Iter iter(&nodeRenameTable);
    Node* oldNode = NULL;
    Node* newNode = NULL;
    while(iter.getNextElem(oldNode, newNode)) {
        newNode->setExecCount(oldNode->getExecCount()*scale);
        oldNode->setExecCount(oldNode->getExecCount()*(1-scale));
    }
    return newEntry;
}


Node* FlowGraph::duplicateRegion(IRManager& irm, Node* entry, StlBitVector& nodesInRegion, DefUseBuilder& defUses, double newEntryFreq) {
    MemoryManager dupMemManager("FlowGraph::duplicateRegion.dupMemManager");
    // prepare the hashtable for the operand rename translation
    OpndRenameTable    *opndRenameTable = new (dupMemManager) OpndRenameTable(dupMemManager,10);
    NodeRenameTable *nodeRenameTable = new (dupMemManager) NodeRenameTable(dupMemManager,10);
    return _duplicateRegion(irm, entry, nodesInRegion, defUses, *nodeRenameTable, *opndRenameTable, newEntryFreq);
}

void FlowGraph::renameOperandsInNode(Node *node, OpndRenameTable *renameTable) {
    Inst *first = (Inst*)node->getFirstInst();
    for (Inst *inst = first->getNextInst(); inst != NULL; inst = inst->getNextInst()) {
        uint32 n = inst->getNumSrcOperands();
        for(uint32 i = 0; i < n; ++i) {
            Opnd* src = inst->getSrc(i);
            Opnd* newsrc = renameTable->getMapping(src);
            if(newsrc != NULL)
                inst->setSrc(i, newsrc);
        }
    }
}




static void breakMonitorExitLoop(ControlFlowGraph& fg, Node* node, Inst* monExit) {

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

    Node* dispatch =  node->getExceptionEdge()->getTargetNode();
    Node* catchBlock = node;
    std::vector<Edge*> exceptionEdges;
    exceptionEdges.push_back(node->getExceptionEdge());

    // Look for a catchBlock that dominates the node.  Assume no control flow.
    while(!((Inst*)catchBlock->getFirstInst())->isCatchLabel() && catchBlock->getInEdges().size() == 1) {
        catchBlock = catchBlock->getInEdges().front()->getSourceNode();
        Edge* exceptionEdge = catchBlock->getExceptionEdge();
        if(exceptionEdge == NULL || exceptionEdge->getTargetNode() != dispatch)
            return;
        exceptionEdges.push_back(exceptionEdge);
    }

    if(((Inst*)catchBlock->getFirstInst())->isCatchLabel()) {
        // We are in a catch.
        if(dispatch->findTargetEdge(catchBlock)) {
            // We have a cycle!
            Node* newDispatch =  dispatch->getExceptionEdge()->getTargetNode();
            assert(newDispatch != NULL);

            std::vector<Edge*>::iterator i;
            for(i = exceptionEdges.begin(); i != exceptionEdges.end(); ++i) {
                Edge* exceptionEdge = *i;
                assert(exceptionEdge->getTargetNode() == dispatch);
                fg.replaceEdgeTarget(exceptionEdge, newDispatch);
            }
        }
    }
}



//
// used to find 'saveret' that starts the node
//     'labelinst' and 'stvar' are skipped
//
static Inst* findSaveRet(Node* node) {
    assert(node);
    Inst* first = (Inst*)node->getFirstInst();
    assert(first->getOpcode() == Op_Label);
    Inst* i;
    for ( i = first->getNextInst(); i != NULL; i = i->getNextInst() ) {
        Opcode opc = i->getOpcode();
        if ( opc == Op_SaveRet ) {
            return i;
        }else if ( opc != Op_LdVar ) {
            return NULL;
        }
    }
    return NULL;
}

void static findNodesInRegion(Node* entry, Node* end, StlBitVector& nodesInRegion) {
    if(Log::isEnabled()) {
        Log::out() << "Find nodes in region (entry = ";
        FlowGraph::printLabel(Log::out(), entry);
        Log::out() << ", exit = ";
        FlowGraph::printLabel(Log::out(), end);
        Log::out() << ")" << std::endl;
    }

    // Return if visited and mark.
    if(nodesInRegion.setBit(end->getId()))
        return;

    if(end != entry) {
        const Edges& inEdges = end->getInEdges();
        // Must not be method entry.
        assert(!inEdges.empty());
        Edges::const_iterator eiter;
        for(eiter = inEdges.begin(); eiter != inEdges.end(); ++eiter) {
            Edge* edge = *eiter;
            findNodesInRegion(entry, edge->getSourceNode(), nodesInRegion);
        }
    }
    assert(nodesInRegion.getBit(entry->getId()));
}


static bool inlineJSR(IRManager* irManager, Node *block, DefUseBuilder& defUses, JsrEntryInstToRetInstMap& entryMap) {
    // Inline the JSR call at the end of block.
    Inst* last = (Inst*)block->getLastInst();
    assert(last->isJSR());
    BranchInst* jsrInst   = (BranchInst*) last;

    //
    // Process entry of JSR.
    //
    Node* entryJSR  = jsrInst->getTargetLabel()->getNode();
    Inst *saveReturn = findSaveRet(entryJSR);
    assert(saveReturn);
    //
    // if JSR 'returns' via at least one RET
    // then
    //     'stvar' follows 'saveret' and stores return info into the local var
    //     retVar (the stored operand) is non-NULL
    // else
    //     no 'stvar' can follow 'saveret' (because it is unused)
    //     retVar is NULL
    Inst *stVar = saveReturn->getNextInst();
    Opnd *retVar = NULL;
    if ( stVar && 
         stVar->isStVar() && 
         (stVar->getSrc(0) == saveReturn->getDst()) ) {
        retVar = stVar->getDst();
    }

    //
    // Find return node for this invocation.
    //
    Node* retTarget = NULL;
    const Edges& outEdges = block->getOutEdges();
    Edges::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        Edge* edge = *eiter;
        Node *node = edge->getTargetNode(); 
        if (node != entryJSR && !node->isDispatchNode()) {
            retTarget = node;
            break;
        }
    }

    if(Log::isEnabled()) {
        Log::out() << "INLINE JSR into ";
        FlowGraph::printLabel(Log::out(), block);
        Log::out() << ":" << std::endl;
        FlowGraph::print(Log::out(), entryJSR);
    }

    ControlFlowGraph& fg = irManager->getFlowGraph();

    //
    // entryMap is a mapping [stVar -> ret inst]
    //
    // In some cases retNode exists, but is unreachable
    // Let's check this
    // if retNodeIsUnrichable==true
    // then JSR never returns.  Convert to jmp
    bool retNodeIsUnreachable = true;
    if (entryMap.find(stVar) != entryMap.end()) {
        JsrEntryCIterRange jsr_range = entryMap.equal_range(stVar);
        JsrEntryInstToRetInstMap::const_iterator i;
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            assert(i->first->getNode() == entryJSR);
            Node* retNode = i->second->getNode();
            UNUSED Inst* ret = (Inst*)retNode->getLastInst();
            assert(ret->isRet());
            assert(ret->getSrc(0) == retVar);

            const Edges& inEdges = retNode->getInEdges();
            if (!inEdges.empty()) {
                retNodeIsUnreachable = false;
                break;
            }
        }
    }

    if(retNodeIsUnreachable == true || retVar == NULL || (entryMap.find(stVar) == entryMap.end())) {
        //
        // JSR never returns.  Convert to jmp.
        //
        saveReturn->unlink();
        if(retVar != NULL)
            stVar->unlink();

        const Edges& inEdges = entryJSR->getInEdges();
        for(eiter = inEdges.begin(); eiter != inEdges.end();) {
            Edge* edge = *eiter;
            ++eiter;
            Node *node = edge->getSourceNode();
            Inst* last = (Inst*)node->getLastInst();
            if (last->isJSR()) {
	           last->unlink();
            }
            if (node->getOutDegree() == 2) {
	            Node* t1 = node->getOutEdges().front()->getTargetNode();
	            Node* t2 = node->getOutEdges().back()->getTargetNode();
	            if(t1 == entryJSR) {
	                fg.removeEdge(node, t2);
	            } else {
	                assert(t2 == entryJSR);
	                fg.removeEdge(node, t1);
	            }
            }
        }
    } else if (entryJSR->hasOnlyOnePredEdge()) {
        // 
        // Inline the JSR in place - no duplication needed.
        //
        fg.removeEdge(block,retTarget);
        jsrInst->unlink();

        JsrEntryCIterRange jsr_range = entryMap.equal_range(stVar);
        JsrEntryInstToRetInstMap::const_iterator i;
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            assert(i->first->getNode() == entryJSR);
            Node* retNode = i->second->getNode();
            Inst* ret = (Inst*)retNode->getLastInst();
            assert(ret->isRet());
            assert(ret->getSrc(0) == retVar);
            ret->unlink();
            fg.addEdge(retNode, retTarget);
        }
    } else {
        //
        // Duplicate and inline the JSR.
        //
        MemoryManager inlineManager("FlowGraph::inlineJSR.inlineManager"); 

        // Find the nodes in the JSR.
        StlBitVector nodesInJSR(inlineManager, fg.getMaxNodeId());

        JsrEntryCIterRange jsr_range = entryMap.equal_range(stVar);
        JsrEntryInstToRetInstMap::const_iterator i;
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            assert((*i).first->getNode() == entryJSR);
            findNodesInRegion(entryJSR, (*i).second->getNode(), nodesInJSR);
        }

        // prepare the hash tables for rename translation
        OpndRenameTable    *opndRenameTable = new (inlineManager) OpndRenameTable(inlineManager,10);
        NodeRenameTable *nodeRenameTable = new (inlineManager) NodeRenameTable(inlineManager,10);

        Node* newEntry = _duplicateRegion(*irManager, entryJSR, nodesInJSR, defUses, *nodeRenameTable, *opndRenameTable, 0);

        // update the BCMap
        if(irManager->getCompilationInterface().isBCMapInfoRequired()) {
            MethodDesc* meth = irManager->getCompilationInterface().getMethodToCompile();
            void *bc2HIRmapHandler = getContainerHandler(bcOffset2HIRHandlerName, meth);

            // update the BCMap for each copied node
            NodeRenameTable::Iter nodeIter(nodeRenameTable);
            Node* oldNode = NULL;
            Node* newNode = NULL;
            StlBitVector regionNodes(inlineManager);
            while(nodeIter.getNextElem(oldNode, newNode)) {
                Inst* oldLast = (Inst*)oldNode->getLastInst();
                Inst* newLast = (Inst*)newNode->getLastInst();
                if (oldLast->asMethodCallInst() || oldLast->asCallInst()) {
                    assert(newLast->asMethodCallInst() || newLast->asCallInst());
                    uint16 bcOffset = getBCMappingEntry(bc2HIRmapHandler, oldLast->getId());
                    assert((bcOffset != 0) && (bcOffset != ILLEGAL_BC_MAPPING_VALUE));
                    setBCMappingEntry(bc2HIRmapHandler, newLast->getId(), bcOffset);
                }

            }
        }

        fg.removeEdge(block,retTarget);
        fg.removeEdge(block,entryJSR);
        jsrInst->unlink();
        fg.addEdge(block, newEntry);

        //
        // Add edge from inline ret locations to return target.
        //
        for(i = jsr_range.first; i != jsr_range.second; ++i) {
            Node* inlinedRetNode = nodeRenameTable->getMapping((*i).second->getNode());
            Inst* ret = (Inst*)inlinedRetNode->getLastInst();
            assert(ret->isRet());
            assert(ret->getSrc(0) == retVar);
            ret->unlink();
            fg.addEdge(inlinedRetNode, retTarget);
        }

        //
        // delete the first two instructions of the inlined node
        // 
        assert(entryJSR != newEntry);
        saveReturn = findSaveRet(newEntry);
        assert(saveReturn);
        stVar = saveReturn->getNextInst();
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


static void inlineJSRs(IRManager* irManager) {
    MemoryManager jsrMemoryManager("FlowGraph::inlineJSRs.jsrMemoryManager");
    static CountTime inlineJSRTimer("ptra::fg::inlineJSRs");
    AutoTimer tm(inlineJSRTimer);

    JsrEntryInstToRetInstMap* jsr_entry_map = irManager->getJsrEntryMap();
    assert(jsr_entry_map); // must be set by translator

    if(Log::isEnabled()) {
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
        static CountTime findJSRTimer("ptra::fg::inlineJSRs::defuse");
        AutoTimer tm(findJSRTimer);
        defUses.initialize(irManager->getFlowGraph());
    }

    ControlFlowGraph& fg = irManager->getFlowGraph();
    const Nodes& nodes = fg.getNodes();
    //Nodes nodes(jsrMemoryManager);
    //fg.getNodesPostOrder(nodes);
    //std::reverse(nodes.begin(), nodes.end());
    //WARN: new nodes created during the iteration 
    //we use the fact that new nodes added to the end of the collection here.
    for (uint32 idx = 0; idx < nodes.size(); ++idx) {
        Node* node = nodes[idx];
        Inst* last = (Inst*)node->getLastInst();
        if(last->isJSR()) {
            inlineJSR(irManager, node, defUses, *jsr_entry_map);
        }
    }
#ifdef _DEBUG
    const Nodes& nnodes = fg.getNodes();
    for (uint32 idx = 0; idx < nnodes.size(); ++idx) {
        Node* node = nnodes[idx];
        Inst* last = (Inst*)node->getLastInst();
        assert(!last->isJSR());
    }
#endif
}

// Finally inlining
static void _fixFinally(ControlFlowGraph& fg, Node *node, Node *retTarget) {
    // if the node has been visited, return
    if (node->getTraversalNum() >= fg.getTraversalNum()) {
        return;
    }
    node->setTraversalNum(fg.getTraversalNum());
    Inst *last = (Inst*)node->getLastInst();
    if (last->isEndFinally()) {
        // remove the 'ret' instruction
        last->unlink();
        // add edge from the new node to the target
        fg.addEdge(node,retTarget);
    } else {
        const Edges& outEdges = node->getOutEdges();
        Edges::const_iterator eiter;
        for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
            Edge* edge = *eiter;
            _fixFinally(fg, edge->getTargetNode(),retTarget);
        }
    }
}


static void _fixBranchTargets(Inst *newInst, NodeRenameTable *nodeRenameTable) {
    // fix targets of instructions
    switch(newInst->getOpcode()) {
    case Op_Branch:
    case Op_JSR:
    case Op_Jump:
        {
            BranchInst *newBranch = (BranchInst*)newInst;
            Node *tar = newBranch->getTargetLabel()->getNode();
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
static bool _inlineFinally(IRManager& irm, Node *from, Node *to, Node *retTarget,
                            NodeRenameTable *nodeRenameTable,
                            OpndRenameTable *opndRenameTable) {
    // if the node has been visited, return
    if (nodeRenameTable->getMapping(from) != NULL) return true;

    ControlFlowGraph& fg = irm.getFlowGraph();
    // if the node is epilogue, add and edge to it
    if (to->isExitNode()) {
       fg.addEdge(from,to);
       return true;
    }
    Node *newto = nodeRenameTable->getMapping(to);
    if (newto != NULL) {
        fg.addEdge(from,newto);
        return true;
    }
    // start following the control flow graph, duplicating basic blocks as needed
    newto = duplicateNode(irm, to, to, opndRenameTable);
    fg.addEdge(from,newto);
    nodeRenameTable->setMapping(to,newto);
    if (((Inst*)to->getLastInst())->isEndFinally()) {
        // remove the duplicate 'end finally'
        Inst *endInst =(Inst*)newto->getLastInst();
        endInst->unlink();
        // add edge from the new node to the target
        fg.addEdge(newto,retTarget);
    } else {
        Log::out() << "LONG FINALLY\n";

        const Edges& outEdges = to->getOutEdges();
        Edges::const_iterator eiter;
        for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
            Edge* edge = *eiter;
            _inlineFinally(irm, newto,edge->getTargetNode(),retTarget,nodeRenameTable,opndRenameTable);
        }
        _fixBranchTargets((Inst*)newto->getLastInst(),nodeRenameTable);
    }
    return true;
}


static bool inlineFinally(IRManager& irm, Node *block) {
    ControlFlowGraph& fg = irm.getFlowGraph();
    BranchInst* leaveInst  = (BranchInst*)block->getLastInst();
    Node *   retTarget  = leaveInst->getTargetLabel()->getNode();
    // record input operand
    Node *   entryFinally = NULL;
    const Edges& outEdges = block->getOutEdges();
    Edges::const_iterator eiter;
    for(eiter = outEdges.begin(); eiter != outEdges.end(); ++eiter) {
        Edge* edge = *eiter;
        Node *node = edge->getTargetNode();
        if (node != retTarget && !node->isDispatchNode()) {
            entryFinally = node;
            break;
        }
    }
    if (Log::isEnabled()) {
        Log::out() << "INLINE FINALLY\n";
        FlowGraph::printLabel(Log::out(), entryFinally);
        FlowGraph::printLabel(Log::out(), retTarget);
        Log::out() << std::endl;
        Log::out().flush();
    }
    if (entryFinally->hasOnlyOnePredEdge()) {
        fg.setTraversalNum(fg.getTraversalNum()+1);
        fg.removeEdge(block,retTarget);
        _fixFinally(fg, entryFinally,retTarget);
    } else {
        MemoryManager inlineManager("FlowGraph::inlineFinally.inlineManager");
        // prepare the hashtable for the operand rename translation
        OpndRenameTable    *opndRenameTable =
            new (inlineManager) OpndRenameTable(inlineManager,10);
        NodeRenameTable *nodeRenameTable =new (inlineManager) NodeRenameTable(inlineManager,10);
        fg.removeEdge(block,retTarget);
        fg.removeEdge(block,entryFinally);
        _inlineFinally(irm, block,entryFinally,retTarget, nodeRenameTable,opndRenameTable );
    }
    leaveInst->unlink();
    return true;
}


void FlowGraph::doTranslatorCleanupPhase(IRManager& irm) {
    ControlFlowGraph& fg = irm.getFlowGraph();
    InstFactory& instFactory = irm.getInstFactory();
    OpndManager& opndManager = irm.getOpndManager();

    static CountTime cleanupPhaseTimer("ptra::fg::cleanupPhase");
    AutoTimer tm(cleanupPhaseTimer);
    fg.purgeUnreachableNodes();

    inlineJSRs(&irm);
    fg.purgeUnreachableNodes();
    
    {
        static CountTime cleanupPhaseInternalTimer("ptra::fg::cleanupPhase::in");
        AutoTimer tm(cleanupPhaseInternalTimer);
        // Cleanup optimizations
        for (Nodes::const_iterator niter = fg.getNodes().begin(), end = fg.getNodes().end(); niter!=end; ++niter) {
            Node* node = *niter;
            if (node->isDispatchNode() || node == fg.getReturnNode() || node == fg.getExitNode()) {
                continue;
            }
            Inst *last = (Inst*)node->getLastInst();
            if (last->getOpcode() == Op_TauMonitorExit) {
                // Check for monitor exit loop.  
                breakMonitorExitLoop(fg, node, last);
            }
            else if (last->isBranch()) {
                if (last->isLeave()) {
                    // Inline Finally - only necessary after translation
                    inlineFinally(irm, node);
                } else if (last->isConditionalBranch()) { // loop peeling for conditional branches
                    Node *target  = ((BranchInst*)last)->getTargetLabel()->getNode();
                    if (((LabelInst*)target->getFirstInst())->getLabelId() > ((LabelInst*)node->getFirstInst())->getLabelId()) 
                    {
                        LabelInst *targetLabel = ((BranchInst*)last)->getTargetLabel();
                        Node *fthru = NULL, *taken = NULL;
                        const Edges& outEdges = node->getOutEdges();
                        Edges::const_iterator eIter;
                        for(eIter = outEdges.begin(); eIter != outEdges.end(); ++eIter) {
                            Edge* edge = *eIter;
                            Node * tar = edge->getTargetNode();
                            if (!tar->isDispatchNode()) {
                                if (tar->getFirstInst() == targetLabel) {
                                    taken = tar;
                                } else {
                                    fthru = tar; 
                                }
                            }
                        }
                        Inst *takenLdConst = (Inst*)taken->getSecondInst();
                        Inst *fthruLdConst = (Inst*)fthru->getSecondInst();
                        Inst *takenStVar   = (takenLdConst!=NULL) ? takenLdConst->getNextInst() : NULL;
                        Inst *fthruStVar   = (fthruLdConst!=NULL) ? fthruLdConst->getNextInst() : NULL;
                        if (takenStVar!=NULL && fthruStVar!=NULL                    &&
                            taken->hasOnlyOneSuccEdge()                             &&
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
                            takenStVar->getNextInst()       == taken->getLastInst() &&
                            fthruStVar->getNextInst()       == fthru->getLastInst())
                        {

                                int takenint32 = ((ConstInst*)takenLdConst)->getValue().i4;
                                int fthruint32 = ((ConstInst*)fthruLdConst)->getValue().i4;
                                Node *meet = taken->getOutEdges().front()->getTargetNode();
                                // find the ldVar for the variable, if any
                                Inst *meetLdVar = (Inst*)meet->getSecondInst();
                                while (meetLdVar->getOpcode()==Op_LdVar) {
                                    if (meetLdVar->getSrc(0) == takenStVar->getDst())
                                        break;
                                    meetLdVar = meetLdVar->getNextInst();
                                }
                                if ((((takenint32==0) && (fthruint32==1)) ||
                                    ((takenint32==1) && (fthruint32==0))) &&
                                    meetLdVar->getOpcode()==Op_LdVar &&
                                    last->getNumSrcOperands()==2) {
                                        // change the instruction to reflect the compare instruction
                                        fg.removeEdge(node,taken);
                                        fg.removeEdge(node,fthru);
                                        fg.addEdge(node,meet);
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
                                        Inst* newStVar = instFactory.makeStVar(meetLdVar->getSrc(0)->asVarOpnd(),dst);
                                        newStVar->insertBefore(lastBranch);
                                        lastBranch->unlink();
                                    }
                            }
                    }
                }
            }
            // remove trivial basic blocks
            if (node->hasOnlyOnePredEdge()) {
                Node *pred = node->getInEdges().front()->getSourceNode();
                if (!pred->isDispatchNode() &&  !((Inst*)pred->getLastInst())->isSwitch() &&
                    (pred->hasOnlyOneSuccEdge() ||  (node->isEmpty() && node->hasOnlyOneSuccEdge()))) 
                {                

                    // don't merge if the node has an exception edge 
                    Edge *edge = NULL;
                    Edges::const_iterator eiter;
                    for(eiter = node->getOutEdges().begin(); eiter != node->getOutEdges().end(); ++eiter) {
                        edge = *eiter;
                        if ((edge->getTargetNode())->isDispatchNode()) {
                            break;
                        }
                    }
                    // If the node has an exception edge, then merging is potentially illegal, so
                    // skip.
                    if (edge == NULL) {
                        if (Log::isEnabled()) {
                            Log::out()<<" MERGE ";FlowGraph::printLabel(Log::out(), pred);FlowGraph::printLabel(Log::out(), node);Log::out()<<"\n";
                            Log::out().flush();
                        }
                        BranchInst *branch = NULL;
                        if (!pred->hasOnlyOneSuccEdge() && node->isEmpty()) {
                            Inst* lastPred = (Inst*)pred->getLastInst();
                            Node* nodeSucc = node->getOutEdges().front()->getTargetNode();
                            if (lastPred->isBranch()) {
                                branch = (BranchInst*)lastPred;
                                if (branch->getTargetLabel() == node->getFirstInst()) {
                                    branch->replaceTargetLabel((LabelInst*)nodeSucc->getFirstInst());
                                }
                            }
                        }
                        fg.mergeBlocks(pred,node);

                        // remove useless branches
                        if (branch != NULL) {
                            Node *target = NULL;
                            for(eiter=pred->getOutEdges().begin(); eiter!=pred->getOutEdges().end();) {
                                Edge* edge = *eiter;
                                ++eiter;
                                Node *succ = edge->getTargetNode();
                                if (! succ->isDispatchNode()) {
                                    if (target == succ) {
                                        fg.removeEdge(pred,succ);
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
    // Remove extra PseudoThrow insts
    DeadCodeEliminator dce(irm);
    dce.removeExtraPseudoThrow();

    //
    // a quick cleanup of unreachable and empty basic blocks
    //
    fg.purgeUnreachableNodes();
    fg.purgeEmptyNodes(false);
}



void FlowGraph::printHIR(std::ostream& os, ControlFlowGraph& fg, MethodDesc& methodDesc) {
    const char* methodName = methodDesc.getName();
    os << std::endl << "--------  irDump: " << methodDesc.getParentType()->getName() << "::" << methodName << "  --------" << std::endl;

    MemoryManager mm("ControlFlowGraph::print.mm");
    Nodes nodes(mm);
    fg.getNodesPostOrder(nodes);
    Nodes::reverse_iterator iter = nodes.rbegin(), end = nodes.rend();
    for(; iter != end; iter++) {
        Node* node = *iter;
        print(os, node);
        os << std::endl;
    }
}

void FlowGraph::print(std::ostream& os, Node* node) {
    os << "Block ";
    printLabel(os, node);
    os << ":  ";
    os << std::endl;

    // Print predecessors
    os << "  Predecessors:";
    const Edges& inEdges = node->getInEdges();
    for (Edges::const_iterator ite = inEdges.begin(), ende = inEdges.end(); ite!=ende; ++ite) {
        Edge* edge = *ite;
        os << " ";
        printLabel(os, edge->getSourceNode());
    }
    os << std::endl;

    // Print successors
    os << "  Successors:";
    const Edges& outEdges = node->getOutEdges();
    for (Edges::const_iterator ite = outEdges.begin(), ende = outEdges.end(); ite!=ende; ++ite) {
        Edge* edge = *ite;
        os << " ";
        printLabel(os, edge->getTargetNode());
    }
    os << std::endl;

    // Print instructions
    printInsts(os, node, 2);

}

void FlowGraph::printLabel(std::ostream& cout, Node* node) {
    Inst *first = (Inst*)node->getFirstInst();
    assert(first->isLabel());
    if (node->isBlockNode()) {
        if(node->getInDegree() == 0) {
            cout << "ENTRY_";
            ((LabelInst*)first)->printId(cout);
        } else if(node->getOutDegree() == 1 && node->getOutEdges().front()->getTargetNode()->isExitNode()) {
            cout << "RETURN";
        } else {
            ((LabelInst*)first)->printId(cout);
        }
    } else if (node->isDispatchNode()) {
        if(node->getOutDegree() == 1 && node->getOutEdges().front()->getTargetNode()->isExitNode())
            cout << "UNWIND";
        else
            cout << "D" << (int32)((LabelInst*)first)->getLabelId();
    } else {
        cout << "EXIT";
    }
}

void FlowGraph::printInsts(std::ostream& cout, Node* node, uint32 indent){
    std::string indentstr(indent, ' ');
    Inst* inst = (Inst*)node->getFirstInst();
    while (inst!=NULL) {
        cout << indentstr.c_str();
        inst->print(cout); 
        cout << std::endl;
        inst = inst->getNextInst();
    }

    // The IR does not contain explicit GOTOs, so for IR printing purposes
    // we should print a GOTO if required, extracted from the CFG.
    inst = (Inst*)node->getLastInst();
    if (!inst->isReturn() && !inst->isLeave() &&
        !inst->isThrow()  &&
        !inst->isEndFinally() && !inst->isRet()) {
            Node *target = NULL;
            if (inst->isConditionalBranch()) {
                target = ((BranchInst*)inst)->getTakenEdge(false)->getTargetNode();
            } else {
                Edges::const_iterator j = node->getOutEdges().begin(), jend = node->getOutEdges().end();
                for (; j != jend; ++j) {
                    Edge* edge = *j;
                    Node *node = edge->getTargetNode();
                    if (!node->isDispatchNode()) {
                        target = node;
                        break;
                    }
                }
            }
            if (target != NULL) {
                cout << indentstr.c_str() << "GOTO ";
                printLabel(cout, target);
                cout << std::endl;
            }
        }
}

class HIRDotPrinter : public PrintDotFile {
public:
    HIRDotPrinter(ControlFlowGraph& _fg) : fg(_fg), loopTree(NULL), dom(NULL){}
    void printDotBody();
    void printDotNode(Node* node);
    void printDotEdge(Edge* edge);
private:
    ControlFlowGraph& fg;
    LoopTree* loopTree;
    DominatorTree* dom;
};

void FlowGraph::printDotFile(ControlFlowGraph& fg, MethodDesc& methodDesc,const char *suffix) {
    HIRDotPrinter printer(fg);
    printer.printDotFile(methodDesc, suffix);
}

void HIRDotPrinter::printDotBody() {
    const Nodes& nodes = fg.getNodes();
    dom = fg.getDominatorTree();
    if (dom != NULL && !dom->isValid()) {
        dom = NULL;
    }
    loopTree = fg.getLoopTree();
    if (loopTree!=NULL && !loopTree->isValid()) {
        loopTree = NULL;
    }
    for(Nodes::const_iterator niter = nodes.begin(); niter != nodes.end(); ++niter) {
        Node* node = *niter;
        printDotNode(node);
    }
    for(Nodes::const_iterator niter = nodes.begin(); niter != nodes.end(); ++niter) {
        Node* node = *niter;
        const Edges& edges = node->getOutEdges();
        for(Edges::const_iterator eiter = edges.begin(); eiter != edges.end(); ++eiter) {
            Edge* edge = *eiter;
            printDotEdge(edge);
        }
    }
}

void HIRDotPrinter::printDotNode(Node* node) {
    std::ostream& out = *os;
    FlowGraph::printLabel(out, node);
    out << " [label=\"";
    if (!node->isEmpty()) {
        out << "{";
    }
    ((Inst*)node->getFirstInst())->print(out);
    out << "tn: " << (int) node->getTraversalNum() << " pre:" << (int)node->getPreNum() << " post:" << (int)node->getPostNum() << " ";
    out << "id: " << (int) node->getId() << " ";
    if(fg.hasEdgeProfile()) {
        out << " execCount:" << node->getExecCount() << " ";
    }
    Node* idom = dom==NULL ? NULL: dom->getIdom(node);
    if (idom!=NULL) {
        out << "idom("; FlowGraph::printLabel(out, idom); out << ") ";
    }
    if (loopTree!=NULL) {
        Node* loopHeader = loopTree->getLoopHeader(node);
        bool isLoopHeader = loopTree->isLoopHeader(node);
        out << "loop(";
        if (!isLoopHeader) {
            out<<"!";
        }
        out<<"hdr";
        if (loopHeader!=NULL) {
            out<<" head:";FlowGraph::printLabel(out, loopHeader); 
        }
        out << ") ";
    }

    if (!node->isEmpty()) {
        out << "\\l|\\" << std::endl;
        Inst* first = (Inst*)node->getFirstInst();
        for (Inst* i = first->getNextInst(); i != NULL; i=i->getNextInst()) { // skip label inst
            i->print(out); 
            out << "\\l\\" << std::endl;
        }
        out << "}";
    }
    out << "\"";
    if (!node->isExitNode() && fg.hasEdgeProfile()) {
        double freq = node->getExecCount();
        double methodFreq = fg.getEntryNode()->getExecCount();
        if(freq > methodFreq*10) {
            out << ",color=red";
        } else if(freq > 0 && freq >= 0.85*methodFreq) {
            out << ",color=orange";
        }
    }
    if (node->isDispatchNode()) {
        out << ",shape=diamond,color=blue";
    } else if (node->isExitNode()) {
        out << ",shape=ellipse,color=green";
    } 
    
    out << "]" << std::endl;
}

void HIRDotPrinter::printDotEdge(Edge* edge) {
    std::ostream& out = *os;
    Node *from = edge->getSourceNode();
    Node *to   = edge->getTargetNode();
    FlowGraph::printLabel(out, from);
    out << " -> ";
    FlowGraph::printLabel(out, to);
    out<<" [";
    out << "taillabel=\"" ;
        if (loopTree) {
        if (loopTree->isBackEdge(edge)) {
            out<<"(backedge)";
        } 
        if (loopTree->isLoopExit(edge)) {
            out<<"(loopexit)";
        }
    }
    if (fg.hasEdgeProfile()) {
        out<<"p:" << edge->getEdgeProb();
    }
    out <<"\"";
    if (to->isDispatchNode()) {
        out << ",style=dotted,color=blue";
    }
    out << "];" << std::endl;
}

} //namespace Jitrino 
