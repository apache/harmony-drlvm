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
 * @author Intel, Mikhail Y. Fursov
 */

#include "Ia32Inst.h"
#include "Ia32IRManager.h"


//#define ENABLE_GC_RT_CHECKS

namespace Jitrino {
namespace  Ia32 {

class APIMagicsHandlerSession: public SessionAction {
    void runImpl();
    uint32 getNeedInfo()const{ return 0; }
    uint32 getSideEffects()const{ return 0; }
    bool isIRDumpEnabled(){ return true; }
};

static ActionFactory<APIMagicsHandlerSession> _api_magics("api_magic");

static Opnd* getCallDst(CallInst* callInst) {
    Inst::Opnds defs(callInst, Inst::OpndRole_InstLevel | Inst::OpndRole_Def | Inst::OpndRole_Explicit);
    uint32 idx = defs.begin();
    return callInst->getOpnd(idx);
}
static Opnd* getCallSrc(CallInst* callInst, uint32 n) {
    Inst::Opnds uses(callInst, Inst::OpndRole_InstLevel | Inst::OpndRole_Use | Inst::OpndRole_Explicit);
    uint32 idx  = uses.begin(); //the first use is call addr
    for (uint32 i=0; i<=n; i++) {
        idx = uses.next(idx);
    }
    return  callInst->getOpnd(idx);
}

class APIMagicHandler {
public:
    APIMagicHandler(IRManager* _irm, CallInst* _inst, MethodDesc* _md)   : irm(_irm), callInst(_inst), md(_md) {
        cfg = irm->getFlowGraph();
    }
    virtual ~APIMagicHandler(){};

    virtual void run()=0;
protected:

    void    convertIntToInt(Opnd* dst, Opnd* src, Node* node);
    Opnd*   addElemIndexWithLEA(Opnd* array, Opnd* index, Node* node);

    IRManager* irm;
    CallInst* callInst;
    MethodDesc*  md;
    ControlFlowGraph* cfg;
};

#define DECLARE_HELPER_INLINER(name)\
class name : public APIMagicHandler {\
public:\
    name (IRManager* irm, CallInst* inst, MethodDesc* md)\
    : APIMagicHandler(irm, inst, md){}\
    \
    virtual void run();\
};\

DECLARE_HELPER_INLINER(Integer_numberOfLeadingZeros_Handler_x_I_x_I);
DECLARE_HELPER_INLINER(Integer_numberOfTrailingZeros_Handler_x_I_x_I);
DECLARE_HELPER_INLINER(Long_numberOfLeadingZeros_Handler_x_J_x_I);
DECLARE_HELPER_INLINER(Long_numberOfTrailingZeros_Handler_x_J_x_I);
DECLARE_HELPER_INLINER(String_compareTo_Handler_x_String_x_I);
DECLARE_HELPER_INLINER(String_regionMatches_Handler_x_I_x_String_x_I_x_I_x_Z);

void APIMagicsHandlerSession::runImpl() {
    CompilationContext* cc = getCompilationContext();
    MemoryManager tmpMM("Inline API methods");
    //finding all api magic calls
    IRManager* irm = cc->getLIRManager();
    ControlFlowGraph* fg = irm->getFlowGraph();
    StlVector<APIMagicHandler*> handlers(tmpMM);
    const Nodes& nodes = fg->getNodesPostOrder();//process checking only reachable nodes.
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode()) {
            for (Inst* inst = (Inst*)node->getFirstInst(); inst!=NULL; inst = inst->getNextInst()) {
                if (!inst->hasKind(Inst::Kind_CallInst)) {
                    continue;
                }
                if ( ((CallInst*)inst)->isDirect() ) {
                    CallInst* callInst = (CallInst*)inst;
                    Opnd * targetOpnd=callInst->getOpnd(callInst->getTargetOpndIndex());
                    assert(targetOpnd->isPlacedIn(OpndKind_Imm));
                    Opnd::RuntimeInfo * ri=targetOpnd->getRuntimeInfo();
                    if( !ri ) { 
                        continue; 
                    };
                    if( ri->getKind() == Opnd::RuntimeInfo::Kind_MethodDirectAddr ){
#ifndef _EM64T_
                        MethodDesc * md = (MethodDesc*)ri->getValue(0);
                        const char* className = md->getParentType()->getName();
                        const char* methodName = md->getName();
                        const char* signature = md->getSignatureString();
                        if (!strcmp(className, "java/lang/Integer")) {
                            if (!strcmp(methodName, "numberOfLeadingZeros") && !strcmp(signature, "(I)I")) {
                                handlers.push_back(new (tmpMM) Integer_numberOfLeadingZeros_Handler_x_I_x_I(irm, callInst, md));
                            } else if (!strcmp(methodName, "numberOfTrailingZeros") && !strcmp(signature, "(I)I")) {
                                handlers.push_back(new (tmpMM) Integer_numberOfTrailingZeros_Handler_x_I_x_I(irm, callInst, md));
                            }
                        } else if (!strcmp(className, "java/lang/Long")) {
                            if (!strcmp(methodName, "numberOfLeadingZeros") && !strcmp(signature, "(J)I")) {
                                handlers.push_back(new (tmpMM) Long_numberOfLeadingZeros_Handler_x_J_x_I(irm, callInst, md));
                            } else if (!strcmp(methodName, "numberOfTrailingZeros") && !strcmp(signature, "(J)I")) {
                                handlers.push_back(new (tmpMM) Long_numberOfTrailingZeros_Handler_x_J_x_I(irm, callInst, md));
                            }
                        }
#endif
                    } else if( ri->getKind() == Opnd::RuntimeInfo::Kind_InternalHelperAddress ) {
                        if( strcmp((char*)ri->getValue(0),"String_compareTo")==0 ) {
                            if(getBoolArg("String_compareTo_as_magic", true))
                                handlers.push_back(new (tmpMM) String_compareTo_Handler_x_String_x_I(irm, callInst, NULL));
                        } else if( strcmp((char*)ri->getValue(0),"String_regionMatches")==0 ) {
                            if(getBoolArg("String_regionMatches_as_magic", true))
                                handlers.push_back(new (tmpMM) String_regionMatches_Handler_x_I_x_String_x_I_x_I_x_Z(irm, callInst, NULL));
                        }
                    }
                }
            }
        }
    }

    //running all handlers
    for (StlVector<APIMagicHandler*>::const_iterator it = handlers.begin(), end = handlers.end(); it!=end; ++it) {
        APIMagicHandler* handler = *it;
        handler->run();
    }
    if (handlers.size() > 0) {
        irm->invalidateLivenessInfo();
    }
}


void Integer_numberOfLeadingZeros_Handler_x_I_x_I::run() {
    //mov r2,-1
    //bsr r1,arg
    //cmovz r1,r2
    //return 31 - r1;
    Type * i32Type =irm->getTypeFromTag(Type::Int32);
    Opnd* r1 = irm->newOpnd(i32Type);
    Opnd* r2 = irm->newOpnd(i32Type);
    Opnd* arg = getCallSrc(callInst, 0);
    Opnd* res = getCallDst(callInst);

    
    irm->newCopyPseudoInst(Mnemonic_MOV, r2, irm->newImmOpnd(i32Type, -1))->insertBefore(callInst);
    irm->newInstEx(Mnemonic_BSR, 1, r1, arg)->insertBefore(callInst);
    irm->newInstEx(Mnemonic_CMOVZ, 1, r1, r1, r2)->insertBefore(callInst);
    irm->newInstEx(Mnemonic_SUB, 1, res, irm->newImmOpnd(i32Type, 31), r1)->insertBefore(callInst);

    callInst->unlink();
}

void Integer_numberOfTrailingZeros_Handler_x_I_x_I::run() {
    //mov r2,32
    //bsf r1,arg
    //cmovz r1,r2
    //return r1
    Type * i32Type =irm->getTypeFromTag(Type::Int32);
    Opnd* r1 = irm->newOpnd(i32Type);
    Opnd* r2 = irm->newOpnd(i32Type);
    Opnd* arg = getCallSrc(callInst, 0);
    Opnd* res = getCallDst(callInst);

    irm->newCopyPseudoInst(Mnemonic_MOV, r2, irm->newImmOpnd(i32Type, 32))->insertBefore(callInst);
    irm->newInstEx(Mnemonic_BSF, 1, r1, arg)->insertBefore(callInst);
    irm->newInstEx(Mnemonic_CMOVZ, 1, r1, r1, r2)->insertBefore(callInst);
    irm->newCopyPseudoInst(Mnemonic_MOV, res, r1)->insertBefore(callInst);

    callInst->unlink();
}

void Long_numberOfLeadingZeros_Handler_x_J_x_I::run() {
#ifdef _EM64T_
    return;
#else
//  bsr r1,hi
//  jz high_part_is_zero 
//high_part_is_not_zero:
//  return 31-r1
//high_part_is_zero:
//  mov r2,-1
//  bsr r1,lw
//  cmovz r1, r2
//  return 63 - r1;

    
    Type * i32Type =irm->getTypeFromTag(Type::Int32);
    Opnd* r1 = irm->newOpnd(i32Type);
    Opnd* r2 = irm->newOpnd(i32Type);
    Opnd* lwOpnd = getCallSrc(callInst, 0);
    Opnd* hiOpnd = getCallSrc(callInst, 1);
    Opnd* res = getCallDst(callInst);
    
    if (callInst!=callInst->getNode()->getLastInst()) {
        cfg->splitNodeAtInstruction(callInst, true, true, NULL);
    }
    Node* node = callInst->getNode();
    Node* nextNode = node->getUnconditionalEdgeTarget();
    assert(nextNode!=NULL);
    cfg->removeEdge(node->getUnconditionalEdge());
    callInst->unlink();

    Node* hiZeroNode = cfg->createBlockNode();
    Node* hiNotZeroNode = cfg->createBlockNode();
    
    //node
    node->appendInst(irm->newInstEx(Mnemonic_BSR, 1, r1, hiOpnd));
    node->appendInst(irm->newBranchInst(Mnemonic_JZ, hiZeroNode, hiNotZeroNode));
    
    
    //high_part_is_not_zero
    hiNotZeroNode->appendInst(irm->newInstEx(Mnemonic_SUB, 1, res, irm->newImmOpnd(i32Type, 31), r1));
    
    //high_part_is_zero
    hiZeroNode->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, r2, irm->newImmOpnd(i32Type, -1)));
    hiZeroNode->appendInst(irm->newInstEx(Mnemonic_BSR, 1, r1, lwOpnd));
    hiZeroNode->appendInst(irm->newInstEx(Mnemonic_CMOVZ, 1, r1, r1, r2));
    hiZeroNode->appendInst(irm->newInstEx(Mnemonic_SUB, 1, res, irm->newImmOpnd(i32Type, 63), r1));


    cfg->addEdge(node, hiZeroNode, 0.3);
    cfg->addEdge(node, hiNotZeroNode, 0.7);
    cfg->addEdge(hiZeroNode, nextNode);
    cfg->addEdge(hiNotZeroNode, nextNode);

#endif
}

void Long_numberOfTrailingZeros_Handler_x_J_x_I::run() {
#ifdef _EM64T_
    return;
#else

//    bsf r1,lw
//    jz low_part_is_zero 
//low_part_is_not_zero:
//    return r1;
//low_part_is_zero:
//    bsf r1,hi
//    jz zero
//not_zero;
//    return 32 + r1;
//zero:
//    return 64;

    Type * i32Type =irm->getTypeFromTag(Type::Int32);
    Opnd* r1 = irm->newOpnd(i32Type);
    Opnd* lwOpnd = getCallSrc(callInst, 0);
    Opnd* hiOpnd = getCallSrc(callInst, 1);
    Opnd* res = getCallDst(callInst);

    if (callInst!=callInst->getNode()->getLastInst()) {
        cfg->splitNodeAtInstruction(callInst, true, true, NULL);
    }
    Node* node = callInst->getNode();
    Node* nextNode = node->getUnconditionalEdgeTarget();
    assert(nextNode!=NULL);
    cfg->removeEdge(node->getUnconditionalEdge());
    callInst->unlink();

    Node* lowZeroNode = cfg->createBlockNode();
    Node* lowNotZeroNode = cfg->createBlockNode();
    Node* notZeroNode = cfg->createBlockNode();
    Node* zeroNode = cfg->createBlockNode();
    
    //node:
    node->appendInst(irm->newInstEx(Mnemonic_BSF, 1, r1, lwOpnd));
    node->appendInst(irm->newBranchInst(Mnemonic_JZ, lowZeroNode, lowNotZeroNode));

    //low_part_is_not_zero:
    lowNotZeroNode->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, res, r1));

    //low_part_is_zero:
    lowZeroNode->appendInst(irm->newInstEx(Mnemonic_BSF, 1, r1, hiOpnd)); 
    lowZeroNode->appendInst(irm->newBranchInst(Mnemonic_JZ, zeroNode, notZeroNode));    

    //not zero:
    notZeroNode->appendInst(irm->newInstEx(Mnemonic_ADD, 1, res, r1, irm->newImmOpnd(i32Type, 32)));

    //zero:
    zeroNode->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, res, irm->newImmOpnd(i32Type, 64)));

    cfg->addEdge(node, lowNotZeroNode, 0.7);
    cfg->addEdge(node, lowZeroNode, 0.3);
    cfg->addEdge(lowNotZeroNode, nextNode);
    cfg->addEdge(lowZeroNode, zeroNode, 0.1);
    cfg->addEdge(lowZeroNode, notZeroNode, 0.9);
    cfg->addEdge(notZeroNode, nextNode);
    cfg->addEdge(zeroNode, nextNode);

#endif
}

void String_compareTo_Handler_x_String_x_I::run() {
    //mov ds:esi, this
    //mov es:edi, src
    //mov ecx, min(this.count, src.count)
    //repne cmpw
    //if ZF == 0 (one of strings is a prefix)
    //  return this.count - src.count
    //else
    //  return [ds:esi-2] - [es:edi-2]

    Node* callInstNode = callInst->getNode();
    Node* nextNode = callInstNode->getUnconditionalEdgeTarget();
    assert(nextNode!=NULL);
    cfg->removeEdge(callInstNode->getUnconditionalEdge());

    // arguments of the call are already prepared by respective HLO pass
    // they are not the strings but 'value' arrays
    Opnd* thisArr = getCallSrc(callInst, 0);
    Opnd* thisIdx = getCallSrc(callInst, 1);
    Opnd* thisLen = getCallSrc(callInst, 2);
    Opnd* trgtArr = getCallSrc(callInst, 3);
    Opnd* trgtIdx = getCallSrc(callInst, 4);
    Opnd* trgtLen = getCallSrc(callInst, 5);
    Opnd* valForCounter = getCallSrc(callInst, 6);
    Opnd* res = getCallDst(callInst);

#ifdef _EM64T_
    RegName counterRegName = RegName_RCX;
    RegName thisAddrRegName = RegName_RSI;
    RegName trgtAddrRegName = RegName_RDI;
    Type*   counterType = irm->getTypeManager().getInt64Type();
#else
    RegName counterRegName = RegName_ECX;
    RegName thisAddrRegName = RegName_ESI;
    RegName trgtAddrRegName = RegName_EDI;
    Type*   counterType = irm->getTypeManager().getInt32Type();
#endif

    Node* counterIsZeroNode = irm->getFlowGraph()->createBlockNode();
    // if counter is zero jump to counterIsZeroNode immediately
    callInstNode->appendInst(irm->newInst(Mnemonic_TEST, valForCounter, valForCounter));
    BranchInst* br = irm->newBranchInst(Mnemonic_JZ, NULL, NULL);
    callInstNode->appendInst(br);
    Node* node = irm->getFlowGraph()->createBlockNode();
    br->setTrueTarget(counterIsZeroNode);
    br->setFalseTarget(node);
    irm->getFlowGraph()->addEdge(counterIsZeroNode, nextNode, 1);
    irm->getFlowGraph()->addEdge(callInstNode, counterIsZeroNode, 0.05);
    irm->getFlowGraph()->addEdge(callInstNode, node, 0.95);

    // prepare counter
    Opnd* counter = irm->newRegOpnd(counterType,counterRegName);
    convertIntToInt(counter, valForCounter, node);

    // prepare this position
    Opnd* thisAddr = addElemIndexWithLEA(thisArr,thisIdx,node);
    Opnd* thisAddrReg = irm->newRegOpnd(thisAddr->getType(),thisAddrRegName);
    node->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, thisAddrReg, thisAddr));

    // prepare trgt position
    Opnd* trgtAddr = addElemIndexWithLEA(trgtArr,trgtIdx,node);
    Opnd* trgtAddrReg = irm->newRegOpnd(trgtAddr->getType(),trgtAddrRegName);
    node->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, trgtAddrReg, trgtAddr));

    Inst* compareInst = irm->newInst(Mnemonic_CMPSW,thisAddrReg,trgtAddrReg,counter);
    compareInst->setPrefix(InstPrefix_REPZ);
    node->appendInst(compareInst);

    // counter is 0 means the same as last comparison leaves zero at ZF
    br = irm->newBranchInst(Mnemonic_JZ, NULL, NULL);
    node->appendInst(br);

    Node* differentStringsNode = irm->getFlowGraph()->createBlockNode();
    br->setTrueTarget(counterIsZeroNode);
    br->setFalseTarget(differentStringsNode);
    irm->getFlowGraph()->addEdge(node, counterIsZeroNode, 0.5);
    irm->getFlowGraph()->addEdge(node, differentStringsNode, 0.5);
    irm->getFlowGraph()->addEdge(differentStringsNode, nextNode, 1);

    // counter is zero
    counterIsZeroNode->appendInst(irm->newInstEx(Mnemonic_SUB, 1, res, thisLen, trgtLen));

    // strings are different
    Opnd* two = irm->newImmOpnd(counterType,2);
    differentStringsNode->appendInst(irm->newInstEx(Mnemonic_SUB, 1, thisAddrReg, thisAddrReg, two));
    differentStringsNode->appendInst(irm->newInstEx(Mnemonic_SUB, 1, trgtAddrReg, trgtAddrReg, two));
    Type* charType = irm->getTypeManager().getCharType();
    Opnd* thisChar = irm->newMemOpnd(charType, thisAddrReg);
    Opnd* trgtChar = irm->newMemOpnd(charType, trgtAddrReg);
    Opnd* dst = irm->newOpnd(charType);
    differentStringsNode->appendInst(irm->newInstEx(Mnemonic_SUB, 1, dst, thisChar, trgtChar));
    differentStringsNode->appendInst(irm->newInstEx(Mnemonic_MOVSX, 1, res, dst));

    callInst->unlink();
}

void String_regionMatches_Handler_x_I_x_String_x_I_x_I_x_Z::run() {
    //mov ds:esi, this
    //mov es:edi, src
    //mov ecx, counter
    //repne cmpw
    //if ZF == 0 (one of strings is a prefix)
    //  return this.count - src.count
    //else
    //  return [ds:esi-2] - [es:edi-2]

    Node* node = callInst->getNode();
    Node* nextNode = NULL;

    if(callInst == node->getLastInst()) {
        nextNode = node->getUnconditionalEdgeTarget();
        assert(nextNode!=NULL);
    } else {
        nextNode = irm->getFlowGraph()->splitNodeAtInstruction(callInst, true, true, NULL);
    }
    cfg->removeEdge(node->getUnconditionalEdge());

    // arguments of the call are already prepared by respective HLO pass
    // they are not the strings but 'value' arrays
    Opnd* thisArr = getCallSrc(callInst, 0);
    Opnd* thisIdx = getCallSrc(callInst, 1);
    Opnd* trgtArr = getCallSrc(callInst, 2);
    Opnd* trgtIdx = getCallSrc(callInst, 3);
    Opnd* valForCounter = getCallSrc(callInst, 4);
    Opnd* res = getCallDst(callInst);

#ifdef _EM64T_
    RegName counterRegName = RegName_RCX;
    RegName thisAddrRegName = RegName_RSI;
    RegName trgtAddrRegName = RegName_RDI;
    Type*   counterType = irm->getTypeManager().getInt64Type();
#else
    RegName counterRegName = RegName_ECX;
    RegName thisAddrRegName = RegName_ESI;
    RegName trgtAddrRegName = RegName_EDI;
    Type*   counterType = irm->getTypeManager().getInt32Type();
#endif

    // prepare counter
    Opnd* counter = irm->newRegOpnd(counterType,counterRegName);
    convertIntToInt(counter, valForCounter, node);

    // prepare this position
    Opnd* thisAddr = addElemIndexWithLEA(thisArr,thisIdx,node);
    Opnd* thisAddrReg = irm->newRegOpnd(thisAddr->getType(),thisAddrRegName);
    node->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, thisAddrReg, thisAddr));

    // prepare trgt position
    Opnd* trgtAddr = addElemIndexWithLEA(trgtArr,trgtIdx,node);
    Opnd* trgtAddrReg = irm->newRegOpnd(trgtAddr->getType(),trgtAddrRegName);
    node->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, trgtAddrReg, trgtAddr));

    Inst* compareInst = irm->newInst(Mnemonic_CMPSW,thisAddrReg,trgtAddrReg,counter);
    compareInst->setPrefix(InstPrefix_REPZ);
    node->appendInst(compareInst);

    // counter is 0 means the same as last comparison leaves zero at ZF
    BranchInst* br = irm->newBranchInst(Mnemonic_JZ, NULL, NULL);
    node->appendInst(br);

    Node* sameRegionsNode = irm->getFlowGraph()->createBlockNode();
    Node* diffRegionsNode = irm->getFlowGraph()->createBlockNode();
    br->setTrueTarget(sameRegionsNode);
    br->setFalseTarget(diffRegionsNode);
    irm->getFlowGraph()->addEdge(node, sameRegionsNode, 0.5);
    irm->getFlowGraph()->addEdge(sameRegionsNode, nextNode, 1);
    irm->getFlowGraph()->addEdge(node, diffRegionsNode, 0.5);
    irm->getFlowGraph()->addEdge(diffRegionsNode, nextNode, 1);

    // regions are equal
    Opnd* one = irm->newImmOpnd(res->getType(),1);
    sameRegionsNode->appendInst(irm->newInst(Mnemonic_MOV, res, one));

    // regions are different
    Opnd* zero = irm->newImmOpnd(res->getType(),0);
    diffRegionsNode->appendInst(irm->newInst(Mnemonic_MOV, res, zero));

    callInst->unlink();
}

// this intends for indexes and counters conversion
// ATTENTION !!! Zero Extention is used for this
void  APIMagicHandler::convertIntToInt(Opnd* dst, Opnd* src, Node* node) 
{
    Type* dstType = dst->getType();
    Type* srcType = src->getType();

    // this works only for equal types 
    // or Int32 into Int64 conversion
    assert(srcType == dstType || (srcType == irm->getTypeManager().getInt32Type() &&
                                  dstType == irm->getTypeManager().getInt64Type()));

    if(srcType != dstType) {
        node->appendInst(irm->newInstEx(Mnemonic_MOVZX, 1, dst, src));
    } else {
        node->appendInst(irm->newCopyPseudoInst(Mnemonic_MOV, dst, src));
    }
}

//  Compute address of the array element given 
//  address of the first element and index
//  using 'LEA' instruction

Opnd*  APIMagicHandler::addElemIndexWithLEA(Opnd* array, Opnd* index, Node* node) 
{
    ArrayType * arrayType=((Opnd*)array)->getType()->asArrayType();
    Type * elemType=arrayType->getElementType();
    Type * dstType=irm->getManagedPtrType(elemType);

    TypeManager& typeManager = irm->getTypeManager();
#ifdef _EM64T_
    Type * indexType = typeManager.getInt64Type();
    Type * offType = typeManager.getInt64Type();
#else
    Type * indexType = typeManager.getInt32Type();
    Type * offType = typeManager.getInt32Type();
#endif
        
    uint32 elemSize = 0;
    if (elemType->isReference()
        && Type::isCompressedReference(elemType->tag, irm->getCompilationInterface()) 
        && !elemType->isCompressedReference()) {
        elemSize = 4;
    } else {
        elemSize = getByteSize(irm->getTypeSize(elemType));
    }
    Opnd * elemSizeOpnd  = irm->newImmOpnd(indexType, elemSize);
    
    Opnd * indexOpnd = NULL;
    if ( index->isPlacedIn(OpndKind_Imm) && index->getImmValue() == 0 ) {
            indexOpnd = NULL;
            elemSizeOpnd = NULL;
    } else {
        indexOpnd = irm->newOpnd(indexType);
        convertIntToInt(indexOpnd,index,node);
    } 
    Opnd * arrOffset = irm->newImmOpnd(offType, arrayType->getArrayElemOffset());
    Opnd * addr = irm->newMemOpnd(dstType,(Opnd*)array, indexOpnd, elemSizeOpnd, arrOffset);
    Opnd * dst = irm->newOpnd(dstType);
    node->appendInst(irm->newInstEx(Mnemonic_LEA, 1, dst, addr));
    return dst;
}

}} //namespace

