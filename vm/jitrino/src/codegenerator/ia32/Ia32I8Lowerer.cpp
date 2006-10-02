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
 * @author Nikolay A. Sidelnikov
 * @version $Revision: 1.12.6.2.4.3 $
 */

#include "Ia32IRManager.h"
#include "Ia32Inst.h"

namespace Jitrino
{
namespace Ia32 {

class I8Lowerer : public SessionAction {
    void runImpl();
protected:
    void processOpnds(Inst * inst, StlMap<Opnd *,Opnd **>& pairs);
    void prepareNewOpnds(Opnd * longOpnd, StlMap<Opnd *,Opnd **>& pairs, Opnd*& newOp1, Opnd*& newOp2);
    bool isI8Type(Type * t){ return t->tag==Type::Int64 || t->tag==Type::UInt64; }
    uint32 getNeedInfo () const     {return 0;}
    virtual uint32 getSideEffects()const {return foundI8Opnds;}

    void buildShiftSubGraph(Inst * inst, Opnd * src1_1, Opnd * src1_2, Opnd * src2, Opnd * dst_1, Opnd * dst_2, Mnemonic mnem, Mnemonic opMnem);
    void buildComplexSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst = NULL);
    void buildSetSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst = NULL);
    void buildJumpSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst = NULL);

    uint32 foundI8Opnds;
};

static ActionFactory <I8Lowerer> _i8l("i8l");

//_______________________________________________________________________________________________________________
// I8 operation internal helpers
int64 __stdcall imul64(const int64 src1, const int64 src2)  stdcall__;
int64 __stdcall imul64(const int64 src1, const int64 src2)
{   return src1*src2;   }

int64 __stdcall idiv64(const int64 src1, const int64 src2)  stdcall__;
int64 __stdcall idiv64(const int64 src1, const int64 src2)
{   return src1/src2;   }

//_______________________________________________________________________________________________________________
void I8Lowerer::runImpl() 
{

// I8 operation internal helpers
    irManager->registerInternalHelperInfo("imul64", IRManager::InternalHelperInfo((void*)&imul64,&CallingConvention_STDCALL));
    irManager->registerInternalHelperInfo("idiv64", IRManager::InternalHelperInfo((void*)&idiv64,&CallingConvention_STDCALL));

    StlMap<Opnd *,Opnd **> pairs(irManager->getMemoryManager());
    StlVector<Inst *> i8Insts(irManager->getMemoryManager());

    ControlFlowGraph* fg = irManager->getFlowGraph();
    const Nodes* postOrder = &fg->getNodesPostOrder();
    for (Nodes::const_reverse_iterator it = postOrder->rbegin(), end = postOrder->rend(); it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()) {
            for (Inst* inst = (Inst*)node->getFirstInst(); inst!=NULL; inst = inst->getNextInst()) {
                if (inst->hasKind(Inst::Kind_I8PseudoInst) || (inst->getMnemonic()==Mnemonic_CALL 
                        || inst->getMnemonic()==Mnemonic_RET ||inst->hasKind(Inst::Kind_EntryPointPseudoInst)) 
                        || inst->hasKind(Inst::Kind_AliasPseudoInst)){
                    i8Insts.push_back(inst);
                    foundI8Opnds = ~(uint32)0;
                }
            }
        }
    }

    for(StlVector<Inst *>::iterator it = i8Insts.begin(); it != i8Insts.end(); it++) {
        processOpnds(*it, pairs);
    }
    
    fg->purgeEmptyNodes();

    postOrder = &fg->getNodesPostOrder();
    for (Nodes::const_reverse_iterator it = postOrder->rbegin(), end = postOrder->rend(); it!=end; ++it) {
        Node * node= *it;
        if (node->isBlockNode()) {
            Inst *  cdq = NULL;
            for (Inst* inst = (Inst*)node->getFirstInst(),*nextInst=NULL; inst!=NULL; inst = nextInst) {
                nextInst = inst->getNextInst();
                uint32  defCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def);
                if(inst->getMnemonic() == Mnemonic_CDQ) {
                    if (inst->getNextInst()!=NULL && inst->getNextInst()->getMnemonic() == Mnemonic_IDIV) {
                        continue;
                    }
                    cdq = inst;
                } else if ( cdq && inst->getMnemonic() == Mnemonic_AND &&
                    inst->getOpnd(defCount+1)->isPlacedIn(OpndKind_Imm) && 
                    inst->getOpnd(defCount+1)->getImmValue() == 0 &&
                    cdq->getOpnd(0)==inst->getOpnd(defCount)) {
                        Inst * tmpInst = irManager->newCopyPseudoInst(Mnemonic_MOV,inst->getOpnd(0), irManager->newImmOpnd(inst->getOpnd(0)->getType(),0));
                        tmpInst->insertAfter(inst);
                        inst->unlink();
                        inst = tmpInst;
                        cdq->unlink();
                        cdq = NULL;
                } else if ( inst->getMnemonic() == Mnemonic_AND &&
                            inst->getOpnd(defCount+1)->isPlacedIn(OpndKind_Imm) && 
                            inst->getOpnd(defCount+1)->getImmValue() == 0xFFFFFFFF) {
                    Inst * tmpInst = irManager->newCopyPseudoInst(Mnemonic_MOV,inst->getOpnd(0), inst->getOpnd(defCount));
                    tmpInst->insertAfter(inst);
                    inst->unlink();
                    inst = tmpInst;
                }
            }
        }
    }
}

void I8Lowerer::processOpnds(Inst * inst, StlMap<Opnd *,Opnd **>& pairs)
{
    Opnd * newOp1 = NULL, *newOp2 = NULL;
    Opnd * dst_1 = NULL, * dst_2 = NULL, * src1_1 = NULL, * src1_2 = NULL, * src2_1 = NULL, * src2_2 = NULL;
    Mnemonic mn = inst->getMnemonic();
    if (mn==Mnemonic_CALL || mn==Mnemonic_RET ||inst->hasKind(Inst::Kind_EntryPointPseudoInst) 
            || inst->hasKind(Inst::Kind_AliasPseudoInst)) {
        for(uint32 i = 0; i < inst->getOpndCount(); i++) {
            Opnd * opnd = inst->getOpnd(i);
            if (!isI8Type(opnd->getType()))
                continue;
            uint32 roles = inst->getOpndRoles(i);
            if (inst->hasKind(Inst::Kind_AliasPseudoInst)) {
                if (roles & Inst::OpndRole_Use) {
                    prepareNewOpnds(opnd,pairs,newOp1,newOp2);
                    inst->setOpnd(i, newOp1);
                    inst->insertOpnd(i+1, newOp2, roles);
                    inst->setConstraint(i, Constraint(OpndKind_Mem, OpndSize_32));
                    inst->setConstraint(i+1, Constraint(OpndKind_Mem, OpndSize_32));
                    i++;
                }
            } else {
                prepareNewOpnds(opnd,pairs,newOp1,newOp2);
            inst->setOpnd(i++, newOp1);
            inst->insertOpnd(i, newOp2, roles);
        }
        }
    } else if (inst->hasKind(Inst::Kind_I8PseudoInst)){
        uint32  defCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def),
                useCount = inst->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Use);
        Opnd * dst = defCount > 0 ? inst->getOpnd(0) : NULL;
        Opnd * src1 = useCount> 0 ? inst->getOpnd(defCount): NULL;
        Opnd * src2 = useCount> 1 ? inst->getOpnd(defCount+1): NULL;

        if (mn!=Mnemonic_IDIV && mn!=Mnemonic_IMUL) {
            if (dst)
                prepareNewOpnds(dst,pairs,dst_1,dst_2);
            if (src1)
                prepareNewOpnds(src1,pairs,src1_1,src1_2);
            if (src2)
                prepareNewOpnds(src2,pairs,src2_1,src2_2);
        }

        switch(mn) {
            case Mnemonic_ADD   :
                assert(dst_1 && src1_1 && src2_1);
                assert(dst_2 && src1_2 && src2_2);
                irManager->newInstEx(Mnemonic_ADD, 1, dst_1, src1_1, src2_1)->insertBefore(inst);
                irManager->newInstEx(Mnemonic_ADC, 1, dst_2, src1_2, src2_2)->insertBefore(inst);
                inst->unlink();
                break;
            case Mnemonic_SUB   :
                assert(dst_1 && src1_1 && src2_1);
                assert(dst_2 && src1_2 && src2_2);
                irManager->newInstEx(Mnemonic_SUB, 1, dst_1, src1_1, src2_1)->insertBefore(inst);
                irManager->newInstEx(Mnemonic_SBB, 1, dst_2, src1_2, src2_2)->insertBefore(inst);
                inst->unlink();
                break;
            case Mnemonic_AND   :
            case Mnemonic_OR    :
            case Mnemonic_XOR   :
                assert(dst_1 && src1_1 && src2_1);
                assert(dst_2 && src1_2 && src2_2);
            case Mnemonic_NOT   :
                assert(dst_1 && src1_1);
                assert(dst_2 && src1_2);
                irManager->newInstEx(mn, 1, dst_1, src1_1, src2_1)->insertBefore(inst);
                irManager->newInstEx(mn, 1, dst_2, src1_2, src2_2)->insertBefore(inst);
                inst->unlink();
                break;
            case Mnemonic_MOV   :
                assert(dst_1 && src1_1);
                irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, src1_1)->insertBefore(inst);
                if (dst_2 && src1_2) {
                    irManager->newCopyPseudoInst(Mnemonic_MOV, dst_2, src1_2)->insertBefore(inst);
                }
                inst->unlink();
                break;
            case Mnemonic_MOVSX :
            case Mnemonic_MOVZX :
                assert(dst_1 && dst_2 && src1_1);
                assert(!src1_2);
                if (src1_1->getSize()<OpndSize_32){
                    irManager->newInstEx(mn, 1, dst_1, src1_1)->insertBefore(inst);
                }else{
                    assert(src1_1->getSize()==OpndSize_32);
                    irManager->newInstEx(Mnemonic_MOV, 1, dst_1, src1_1)->insertBefore(inst);
                }
                if (mn==Mnemonic_MOVSX){
                    irManager->newInstEx(Mnemonic_CDQ, 1, dst_2, dst_1)->insertBefore(inst);
                    inst->unlink();
                } else {
                    Opnd* zero = irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), 0);
                    irManager->newInstEx(Mnemonic_MOV, 1, dst_2, zero)->insertBefore(inst);
                    inst->unlink();
                }
                break;
            case Mnemonic_PUSH  :
                assert(src1_1);
                assert(src1_2);
                irManager->newInstEx(Mnemonic_PUSH, 0, src1_2)->insertBefore(inst);
                irManager->newInstEx(Mnemonic_PUSH, 0, src1_1)->insertBefore(inst);
                inst->unlink();
                break;
            case Mnemonic_POP   :
                assert(dst_1);
                assert(dst_2);
                irManager->newInstEx(Mnemonic_POP, 1, dst_1)->insertBefore(inst);
                irManager->newInstEx(Mnemonic_POP, 1, dst_2)->insertBefore(inst);
                inst->unlink();
                break;
            case Mnemonic_SHL   :
            {
                assert(dst && src1 && src2);
                buildShiftSubGraph(inst, src1_2, src1_1, src2, dst_2, dst_1, mn, Mnemonic_SHR);
                inst->unlink();
                break;
            }
            case Mnemonic_SHR   :
            case Mnemonic_SAR   :
            {
                assert(dst && src1 && src2);
                buildShiftSubGraph(inst, src1_1, src1_2, src2, dst_1, dst_2, mn, Mnemonic_SHL);
                inst->unlink();
                break;
            }
            case Mnemonic_CMP   :
            {
                assert(src1 && src2);
                Inst * condInst = inst->getNextInst();
                Mnemonic mnem = getBaseConditionMnemonic(condInst->getMnemonic());
                if (condInst->getMnemonic() == Mnemonic_MOV) {
                    condInst = condInst->getNextInst();
                    mnem = getBaseConditionMnemonic(condInst->getMnemonic());
                }
                if (mnem != Mnemonic_NULL) {
                            if(condInst->hasKind(Inst::Kind_BranchInst)) {
                                buildJumpSubGraph(inst,src1_1,src1_2,src2_1,src2_2,condInst);
                            } else {
                                buildSetSubGraph(inst,src1_1,src1_2,src2_1,src2_2,condInst);
                            }
                } else {
                    buildComplexSubGraph(inst,src1_1,src1_2,src2_1,src2_2);
                }
                inst->unlink();
                break;
            }
            case Mnemonic_IMUL  :
            {
                assert(dst && src1 && src2);
                Opnd * args[2]={ src1, src2 };
                CallInst * callInst = irManager->newInternalRuntimeHelperCallInst("imul64", 2, args, dst);
                callInst->insertBefore(inst);
                processOpnds(callInst, pairs);
                inst->unlink();
                break;
            }
            case Mnemonic_IDIV  :
            {
                assert(dst && src1 && src2);
                Opnd * args[2]={ src1, src2 };
                CallInst * callInst = irManager->newInternalRuntimeHelperCallInst("idiv64", 2, args, dst);
                callInst->insertBefore(inst);
                processOpnds(callInst, pairs);
                inst->unlink();
                break;
            }
            default :   
                assert(0);
        }//end switch by mnemonics
    }
}

void I8Lowerer::buildShiftSubGraph(Inst * inst, Opnd * src1_1, Opnd * src1_2, Opnd * src2, Opnd * dst_1, Opnd * dst_2, Mnemonic mnem, Mnemonic opMnem) {
    Opnd * dst1_1 = irManager->newOpnd(dst_2->getType()), 
            * dst1_2 = irManager->newOpnd(dst_2->getType()), 
            * tmpSrc2 = irManager->newOpnd(src2->getType());

    if(src2->isPlacedIn(OpndKind_Imm)) {
        int64 immVal = src2->getImmValue();
        immVal &= 63;
        if (immVal == 32) {
            irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, src1_2)->insertBefore(inst);
            if (mnem != Mnemonic_SAR) {
                irManager->newCopyPseudoInst(Mnemonic_MOV, dst_2, irManager->newImmOpnd(src1_1->getType(), 0))->insertBefore(inst);
            } else {
                irManager->newInstEx(mnem, 1, dst_2, src1_2, irManager->newImmOpnd(src2->getType(), 31))->insertBefore(inst);
            }
        } else if(immVal > 31) {
            if (mnem != Mnemonic_SAR) {
                irManager->newCopyPseudoInst(Mnemonic_MOV, dst_2, irManager->newImmOpnd(src1_1->getType(), 0))->insertBefore(inst);
            } else {
                irManager->newInstEx(mnem, 1, dst_2, src1_2, irManager->newImmOpnd(src2->getType(), 31))->insertBefore(inst);
            }
            irManager->newInstEx(mnem, 1, dst_1, src1_2, src2)->insertBefore(inst);
        } else if (immVal == 0) {
            irManager->newCopyPseudoInst(Mnemonic_MOV, dst_2, src1_2)->insertBefore(inst);
            irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, src1_1)->insertBefore(inst);
        } else {
            irManager->newInstEx(mnem == Mnemonic_SAR?Mnemonic_SHR:mnem, 1, dst1_1, src1_1, src2)->insertBefore(inst);
            irManager->newInstEx(opMnem, 1, dst1_2, src1_2, irManager->newImmOpnd(src2->getType(), 32-immVal))->insertBefore(inst);
            irManager->newInstEx(Mnemonic_OR, 1, dst_1, dst1_2, dst1_1)->insertBefore(inst);
            irManager->newInstEx(mnem, 1, dst_2, src1_2, src2)->insertBefore(inst);
        }
    } else {
        ControlFlowGraph* subCFG = irManager->createSubCFG(true, false);

        Node* bbMain = subCFG->getEntryNode(),
                    * lNode = subCFG->createBlockNode(),
                    * gNode =  subCFG->createBlockNode(),
                    * nullNode =  subCFG->createBlockNode(),
                    * cmpNullNode = subCFG->createBlockNode();

        bbMain->appendInst(irManager->newInstEx(Mnemonic_AND, 1, src2, src2, irManager->newImmOpnd(src2->getType(), 63)));
        bbMain->appendInst(irManager->newInst(Mnemonic_CMP, src2, irManager->newImmOpnd(src2->getType(), 31)));
        bbMain->appendInst(irManager->newBranchInst(getMnemonic(Mnemonic_Jcc, ConditionMnemonic_LE), cmpNullNode, gNode));

        cmpNullNode->appendInst(irManager->newInst(Mnemonic_CMP, src2, irManager->newImmOpnd(src2->getType(), 0)));
        cmpNullNode->appendInst(irManager->newBranchInst(getMnemonic(Mnemonic_Jcc, ConditionMnemonic_E), nullNode, lNode));

        nullNode->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, src1_1));
        nullNode->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_2, src1_2));

        if (mnem == Mnemonic_SAR)
            gNode->appendInst(irManager->newInstEx(mnem, 1, dst_2, src1_2, irManager->newImmOpnd(src2->getType(), 31)));
        else
            gNode->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_2, irManager->newImmOpnd(dst_2->getType(), 0)));
        gNode->appendInst(irManager->newInstEx(mnem == Mnemonic_SAR?Mnemonic_SHR:mnem, 1, dst_1, src1_2, src2));

        lNode->appendInst(irManager->newInstEx(mnem == Mnemonic_SAR?Mnemonic_SHR:mnem, 1, dst1_1, src1_1, src2));
        lNode->appendInst(irManager->newInstEx(Mnemonic_SUB, 1, tmpSrc2, irManager->newImmOpnd(src2->getType(), 32), src2));
        lNode->appendInst(irManager->newInstEx(opMnem, 1, dst1_2, src1_2, tmpSrc2));
        lNode->appendInst(irManager->newInstEx(Mnemonic_OR, 1, dst_1, dst1_2, dst1_1));
        lNode->appendInst(irManager->newInstEx(mnem, 1, dst_2, src1_2, src2));
        
        Node* sinkNode =  subCFG->getReturnNode();
        
        subCFG->addEdge(bbMain, cmpNullNode, 0.5);
        subCFG->addEdge(bbMain, gNode, 0.5);
        subCFG->addEdge(cmpNullNode, nullNode, 0.5);
        subCFG->addEdge(cmpNullNode, lNode, 0.5);
        subCFG->addEdge(gNode, sinkNode, 1);
        subCFG->addEdge(lNode, sinkNode, 1);
        subCFG->addEdge(nullNode, sinkNode, 1);
        
        irManager->getFlowGraph()->spliceFlowGraphInline(inst, *subCFG);
    }
}

void I8Lowerer::buildJumpSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst) {
    Node * bb=inst->getNode();
    
    ControlFlowGraph* subCFG = irManager->createSubCFG(true, false);
    Node* bbHMain = subCFG->getEntryNode();
    Node* bbLMain = subCFG->createBlockNode();
    Node* sinkNode =  subCFG->getReturnNode();
    
    bbHMain->appendInst(irManager->newInst(Mnemonic_CMP, src1_2, src2_2));
    bbHMain->appendInst(irManager->newBranchInst(Mnemonic_JNZ, sinkNode, bbLMain));
    
    bbLMain->appendInst(irManager->newInst(Mnemonic_CMP, src1_1, src2_1));
    
    
    Node* bbFT = bb->getFalseEdgeTarget();
    Node* bbDB = bb->getTrueEdgeTarget();
    
    Mnemonic mnem = Mnemonic_NULL;
    switch(condInst->getMnemonic()) {
        case Mnemonic_JL : mnem = Mnemonic_JB; break;
        case Mnemonic_JLE : mnem = Mnemonic_JBE; break;
        case Mnemonic_JG : mnem = Mnemonic_JA; break;
        case Mnemonic_JGE : mnem = Mnemonic_JAE; break;
        default: mnem = condInst->getMnemonic(); break;
    }
    bbLMain->appendInst(irManager->newBranchInst(mnem, bbDB, bbFT));
    
    subCFG->addEdge(bbHMain, sinkNode, 0.1);
    subCFG->addEdge(bbHMain, bbLMain, 0.9);
    subCFG->addEdge(bbLMain, bbFT, 0.1);
    subCFG->addEdge(bbLMain, bbDB, 0.9);
        
    irManager->getFlowGraph()->spliceFlowGraphInline(inst, *subCFG);
}


void I8Lowerer::buildSetSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst) {

    ControlFlowGraph* subCFG = irManager->createSubCFG(true, false);
    
    Node* bbHMain = subCFG->getEntryNode();
    Node* bbHF = subCFG->createBlockNode();
    Node* bbLMain = subCFG->createBlockNode();
    Node* sinkNode =  subCFG->getReturnNode();

    bbHMain->appendInst(irManager->newInst(Mnemonic_CMP, src1_2, src2_2));
    bbHMain->appendInst(irManager->newBranchInst(Mnemonic_JNZ, bbHF, bbLMain));

    bbLMain->appendInst(irManager->newInst(Mnemonic_CMP, src1_1, src2_1));
    
    irManager->getFlowGraph()->splitNodeAtInstruction(condInst, false, true, NULL);
    
    Mnemonic mnem = getBaseConditionMnemonic(condInst->getMnemonic());

    Inst * nextInst = inst->getNextInst();
    if(nextInst->getMnemonic() == Mnemonic_MOV) {
        bbHF->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, nextInst->getOpnd(0), nextInst->getOpnd(1)));
        bbLMain->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, nextInst->getOpnd(0), nextInst->getOpnd(1)));
        nextInst->unlink();
    } else {
        assert(nextInst == condInst);
    }
    bbHF->appendInst(irManager->newInst(condInst->getMnemonic(), condInst->getOpnd(0), mnem == Mnemonic_CMOVcc? condInst->getOpnd(1): NULL));
    
    ConditionMnemonic condMnem = ConditionMnemonic(condInst->getMnemonic()-mnem);
    switch(condMnem) {
        case ConditionMnemonic_L : condMnem = ConditionMnemonic_B; break;
        case ConditionMnemonic_LE : condMnem = ConditionMnemonic_BE; break;
        case ConditionMnemonic_G : condMnem = ConditionMnemonic_A; break;
        case ConditionMnemonic_GE : condMnem = ConditionMnemonic_AE; break;
        default: break;
    }
    bbLMain->appendInst(irManager->newInst(Mnemonic(mnem+condMnem), condInst->getOpnd(0), mnem == Mnemonic_CMOVcc? condInst->getOpnd(1): NULL));
    
    subCFG->addEdge(bbHMain, bbHF, 0.1);
    subCFG->addEdge(bbHMain, bbLMain, 0.9);
    subCFG->addEdge(bbLMain, sinkNode, 1); 
    subCFG->addEdge(bbHF, sinkNode, 1);

    condInst->unlink();

    irManager->getFlowGraph()->spliceFlowGraphInline(inst, *subCFG);
}

void I8Lowerer::buildComplexSubGraph(Inst * inst, Opnd * src1_1,Opnd * src1_2,Opnd * src2_1,Opnd * src2_2, Inst * condInst) {

    Opnd * dst_1 = irManager->newOpnd(irManager->getTypeManager().getInt32Type());
    ControlFlowGraph* subCFG = irManager->createSubCFG(true, false);
    Node* bbHMain = subCFG->getEntryNode();
    Node* bbHF = subCFG->createBlockNode();
    Node* bbHS = subCFG->createBlockNode();

    Node* bbLMain = subCFG->createBlockNode();
    Node* bbLF = subCFG->createBlockNode();
    Node* bbLS = subCFG->createBlockNode();
    Node* sinkNode =  subCFG->getReturnNode();

    bbHMain->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), 0)));
    bbHMain->appendInst(irManager->newInst(Mnemonic_CMP, src1_2, src2_2));
    bbHMain->appendInst(irManager->newBranchInst(Mnemonic_JZ, bbLMain, bbHF));
    
    bbHF->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), -1)));
    bbHF->appendInst(irManager->newBranchInst(Mnemonic_JG, bbHS, sinkNode));
    
    bbHS->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), 1)));
    
    bbLMain->appendInst(irManager->newInst(Mnemonic_CMP, src1_1, src2_1));
    bbLMain->appendInst(irManager->newBranchInst(Mnemonic_JZ, sinkNode, bbLF));

    bbLF->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), -1)));
    bbLF->appendInst(irManager->newBranchInst(Mnemonic_JA, bbLS, sinkNode));
    
    bbLS->appendInst(irManager->newCopyPseudoInst(Mnemonic_MOV, dst_1, irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), 1)));
    
    sinkNode->appendInst(irManager->newInst(Mnemonic_CMP, dst_1, irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), 0)));
    
    
    subCFG->addEdge(bbHMain, bbHF, 0.1);
    subCFG->addEdge(bbHMain, bbLMain, 0.9);
    subCFG->addEdge(bbHF, sinkNode, 0.5);
    subCFG->addEdge(bbHF, bbHS, 0.5);
    subCFG->addEdge(bbHS, sinkNode, 1); 
    subCFG->addEdge(bbLMain, bbLF, 0.1);
    subCFG->addEdge(bbLMain, sinkNode, 0.9);
    subCFG->addEdge(bbLF, sinkNode, 0.5);
    subCFG->addEdge(bbLF, bbLS, 0.5);
    subCFG->addEdge(bbLS, sinkNode, 1);
    
    irManager->getFlowGraph()->spliceFlowGraphInline(inst, *subCFG);
}

void I8Lowerer::prepareNewOpnds(Opnd * longOpnd, StlMap<Opnd *,Opnd **>& pairs, Opnd*& newOp1, Opnd*& newOp2)
{
    if (!isI8Type(longOpnd->getType())){
        newOp1=longOpnd;
        newOp2=NULL;
        return;
    }

    if(pairs.find(longOpnd)!=pairs.end()) {
        newOp1 = pairs[longOpnd][0];
        newOp2 = pairs[longOpnd][1];
    } else {
        if(longOpnd->isPlacedIn(OpndKind_Memory)) {
            newOp1 = irManager->newOpnd(irManager->getTypeManager().getUInt32Type());
            newOp2 = irManager->newOpnd(irManager->getTypeManager().getInt32Type());
            Opnd * opnds[2] = { newOp1, newOp2 };
            irManager->assignInnerMemOpnds(longOpnd, opnds, 2);
        } else if(longOpnd->isPlacedIn(OpndKind_Imm)){
            newOp1 = irManager->newImmOpnd(irManager->getTypeManager().getUInt32Type(), (uint32)longOpnd->getImmValue());
            newOp2 = irManager->newImmOpnd(irManager->getTypeManager().getInt32Type(), (int32)(longOpnd->getImmValue()>>32));
        } else {
            newOp1 = irManager->newOpnd(irManager->getTypeManager().getUInt32Type());
            newOp2 = irManager->newOpnd(irManager->getTypeManager().getInt32Type());
        }
        pairs[longOpnd] = new(irManager->getMemoryManager()) Opnd*[2];
        pairs[longOpnd][0]=newOp1;
        pairs[longOpnd][1]=newOp2;
    }
}

}}
