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
* @author Intel, Nikolay A. Sidelnikov
* @version $Revision: 1.20.8.1.4.4 $
*
*/

#include "escapeanalyzer.h"
#include "Log.h"
#include "Inst.h"
#include "Dominator.h"
#include "globalopndanalyzer.h"
#include "optimizer.h"
#include "FlowGraph.h"
#include "LoopTree.h"

#include <algorithm>

namespace Jitrino {

    struct LoopEdges 
    {
        Edge * outEdge;
        Edge * inEdge;
        Edge * backEdge;

        LoopEdges() : outEdge(NULL), inEdge(NULL), backEdge(NULL) {} ;
    };

DEFINE_SESSION_ACTION(FastArrayFillPass, fastArrayFill, "Fast Array Filling")

void
FastArrayFillPass::_run(IRManager& irManager) 
{
    LoopTree * info = irManager.getLoopTree();
    if (!info->isValid()) {
        info->rebuild(false);
    }
    if (!info->hasLoops())  {
        return;
    }

    MemoryManager tmm(1024,"FastArrayInitPass::insertFastArrayInit");
    Edges loopEdges(tmm);
    StlMap<Node *, LoopEdges> loopInfo(tmm);
        
    const Nodes& nodes = irManager.getFlowGraph().getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (!info->isLoopHeader(node)) {
            continue;
        }
        //compute number of nodes of the loop
        Nodes loopNodes = info->getLoopNode(node,false)->getNodesInLoop();
        unsigned sz = loopNodes.size();
        if (sz!=3) {
            continue;
        }

        if (info->isBackEdge(node->getInEdges().front())) {
            loopInfo[node].backEdge = node->getInEdges().front();
            loopInfo[node].inEdge = node->getInEdges().back();
        } else {
            loopInfo[node].inEdge = node->getInEdges().front();
            loopInfo[node].backEdge = node->getInEdges().back();
        }
        loopInfo[node].outEdge = info->isLoopExit(node->getOutEdges().front())? node->getOutEdges().front() : node->getOutEdges().back();
    }

    //check found loops for pattern
    for(StlMap<Node *, LoopEdges>::const_iterator it = loopInfo.begin(); it != loopInfo.end(); ++it) {

        Edge * backEdge = it->second.backEdge;
        Edge * inEdge = it->second.inEdge;
        Edge * outEdge = it->second.outEdge;
    
        Node * startNode = backEdge->getSourceNode();
        Opnd * index = NULL; 
        Opnd * tmpIndex = NULL;
        Opnd * constValue = NULL;
        Opnd * address = NULL;
        Opnd * arrayBase = NULL;
        Opnd * arrayRef = NULL;
        Opnd* inc = NULL;
        Opnd * arrayBound = NULL;
        Opnd * fillBound = NULL;
        Opnd *startIndex;
        Inst * inst = ((Inst *)startNode->getLastInst());
        bool found = false;

        //check StVar
        if (inst->getOpcode() == Op_StVar) {
            index = inst->getDst();
            inc = inst->getSrc(0);
            inst = inst->getPrevInst();
            //check Add
            if (inst->getOpcode() == Op_Add) {
                Opnd* addOp = inst->getSrc(1);
                tmpIndex = inst->getSrc(0);
                //check Add operands
                if (inst->getDst() == inc && 
                    addOp->getInst()->getOpcode() == Op_LdConstant && 
                    ((ConstInst *)addOp->getInst())->getValue().i4 == 1) 
                {
                    inst = inst->getPrevInst();
                    //check StInd
                    if (inst->getOpcode() == Op_TauStInd) {
                        constValue = inst->getSrc(0);
                        address = inst->getSrc(1);
                        inst = inst->getPrevInst();
                        //check AddIndex
                        if (inst->getOpcode() == Op_AddScaledIndex && inst->getDst() == address &&
                            inst->getSrc(1) == tmpIndex) 
                        {
                            arrayBase = inst->getSrc(0);
                            inst = inst->getPrevInst();
                            //check Label aka beginning of BB
                            if (inst->getOpcode() == Op_Label) {
                                found = true;
                            }
                        } 
                    } 
                } 
            } 
        }
        if (!found) {
            continue;
        }
        startNode = startNode->getInEdges().front()->getSourceNode();
        inst = ((Inst *)startNode->getLastInst());

        //check CheckUpperBound
        if (inst->getOpcode() == Op_TauCheckUpperBound && inst->getSrc(0) == tmpIndex && inst->getPrevInst()->getOpcode() == Op_Label) {
            arrayBound = inst->getSrc(1);
        } else {
            continue;
        }

        startNode = startNode->getInEdges().front()->getSourceNode();
        inst = ((Inst *)startNode->getLastInst());
        found = false;
        //check Branch
        if (inst->getOpcode() == Op_Branch && inst->getSrc(0) == tmpIndex) {
            fillBound = inst->getSrc(1);
            inst = inst->getPrevInst();
            //check LdVar and Label
            if (inst->getOpcode() == Op_LdVar && inst->getSrc(0) == index && inst->getDst() == tmpIndex && inst->getPrevInst()->getOpcode() == Op_Label) {
                found = true;
            }
        }
        if (!found) {
            continue;
        }

        startNode = inEdge->getSourceNode();
        inst = ((Inst *)startNode->getLastInst());
        found = false;

        //check StVar
        if (inst->getOpcode() == Op_StVar && inst->getDst() == index) {
            startIndex = inst->getSrc(0);
            inst = inst->getPrevInst();
            //check StInd
            if (inst->getOpcode() == Op_TauStInd && inst->getSrc(0) == constValue && inst->getSrc(1) == arrayBase) {
                inst = inst->getPrevInst();
                //check LdBase
                if (inst->getOpcode() == Op_LdArrayBaseAddr && inst->getDst() == arrayBase && inst->getPrevInst()->getOpcode() == Op_Label) {
                    arrayRef = inst->getSrc(0);
                    found = true;
                }
            }
        }
        if (!found) {
            continue;
        }

        startNode = startNode->getInEdges().front()->getSourceNode();
        inst = ((Inst *)startNode->getLastInst());
        found = false;

        //check CheckUpperBound
        ConstInst * cInst = (ConstInst *)inst->getSrc(0)->getInst();
        if (inst->getOpcode() == Op_TauCheckUpperBound && cInst && cInst->getValue().i4 == 0 ) {
            inst = inst->getPrevInst();
            //check ArrayLength and Label
            if (inst->getOpcode() == Op_TauArrayLen && inst->getSrc(0) == arrayRef && inst->getDst() == arrayBound && inst->getPrevInst()->getOpcode() == Op_Label) {
                found = true;
            }
        }
        if (!found) {
            continue;
        }

        //now we found our pattern

        inEdge = startNode->getInEdges().front();

        //get a new constant
        int val = ((ConstInst*)constValue->getInst())->getValue().i4;
        switch (((Type*)arrayRef->getType()->asArrayType()->getElementType())->tag) {
            case Type::Int8:
            case Type::Boolean:
            case Type::UInt8:
                val |= (val << 8);
                val |= (val << 16);
                break;
            case Type::Int16:
            case Type::UInt16:
            case Type::Char:
                val |= (val << 16);
                break;
            case Type::Int32:
            case Type::UInt32:
            case Type::UIntPtr:
            case Type::IntPtr:
                break;
            default:
                continue;
                break;
        }

        ControlFlowGraph& fg = irManager.getFlowGraph();
        Node * preheader = fg.splitNodeAtInstruction(inst, true, false, irManager.getInstFactory().makeLabel());
        Inst * cmp = irManager.getInstFactory().makeBranch(Cmp_NE_Un, arrayBound->getType()->tag, arrayBound, fillBound, (LabelInst *)preheader->getFirstInst());
        startNode->appendInst(cmp);

        Node * prepNode = fg.createBlockNode(irManager.getInstFactory().makeLabel());
        fg.addEdge(startNode,prepNode);
        
        OpndManager& opndManager = irManager.getOpndManager();

        Opnd * copyOp = opndManager.createArgOpnd(irManager.getTypeManager().getInt32Type());
        Inst * copyInst  = irManager.getInstFactory().makeLdConst(copyOp,val);
        prepNode->appendInst(copyInst);

        Opnd *baseOp = opndManager.createArgOpnd(irManager.getTypeManager().getIntPtrType());
        Inst * ldBaseInst = irManager.getInstFactory().makeLdArrayBaseAddr(arrayRef->getType()->asArrayType()->getElementType(),baseOp, arrayRef);
        prepNode->appendInst(ldBaseInst);

        Opnd* args[4] = {copyOp, arrayRef, arrayBound, baseOp};

        // add jit helper
        Inst* initInst = irManager.getInstFactory().makeJitHelperCall(
            OpndManager::getNullOpnd(), FillArrayWithConst, 4, args);
        prepNode->appendInst(initInst);

        fg.addEdge(prepNode, outEdge->getTargetNode());
                    
    }
}
}

