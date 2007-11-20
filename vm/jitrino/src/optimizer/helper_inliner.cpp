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

#include "PMFAction.h"
#include "optpass.h"
#include "inliner.h"
#include "LoopTree.h"
#include "Dominator.h"
#include "StlPriorityQueue.h"

namespace Jitrino {

class HelperConfig {
public:
    HelperConfig(VM_RT_SUPPORT id) : helperId(id), doInlining(true), hotnessPercentToInline(0) {}
    VM_RT_SUPPORT helperId;
    bool          doInlining;
    uint32        hotnessPercentToInline;
};    

struct HelperInlinerFlags {
    HelperInlinerFlags(MemoryManager& mm) 
        : inlinerPipelineName(NULL), opcodeToHelperMapping(mm), helperConfigs(mm){}

    const char* inlinerPipelineName;

    StlMap<Opcode, VM_RT_SUPPORT> opcodeToHelperMapping;
    StlMap<VM_RT_SUPPORT, HelperConfig*> helperConfigs;
};

class HelperInlinerAction: public Action {
public:
    HelperInlinerAction() : flags(Jitrino::getGlobalMM()) {}
    void init();
    void registerHelper(Opcode opcode, VM_RT_SUPPORT helperId);
    HelperInlinerFlags& getFlags() {return flags;}
protected:
    HelperInlinerFlags flags;
};

DEFINE_SESSION_ACTION_WITH_ACTION(HelperInlinerSession, HelperInlinerAction, inline_helpers, "VM helpers inlining");

void HelperInlinerAction::init() {
    flags.inlinerPipelineName = getStringArg("pipeline", "inliner_pipeline");
    
    registerHelper(Op_NewObj, VM_RT_NEW_RESOLVED_USING_VTABLE_AND_SIZE);
    registerHelper(Op_NewArray, VM_RT_NEW_VECTOR_USING_VTABLE);
    registerHelper(Op_TauMonitorEnter, VM_RT_MONITOR_ENTER_NON_NULL);
    registerHelper(Op_TauMonitorExit, VM_RT_MONITOR_EXIT_NON_NULL);
    registerHelper(Op_TauStRef, VM_RT_GC_HEAP_WRITE_REF);
    registerHelper(Op_TauLdIntfcVTableAddr, VM_RT_GET_INTERFACE_VTABLE_VER0);
    registerHelper(Op_TauCheckCast, VM_RT_CHECKCAST);
    registerHelper(Op_TauInstanceOf, VM_RT_INSTANCEOF);
}

void HelperInlinerAction::registerHelper(Opcode opcode, VM_RT_SUPPORT helperId) {
    MemoryManager& globalMM = getJITInstanceContext().getGlobalMemoryManager();
    assert(flags.opcodeToHelperMapping.find(opcode)==flags.opcodeToHelperMapping.end()); 
    flags.opcodeToHelperMapping[opcode] = helperId; 
    if (flags.helperConfigs.find(helperId)== flags.helperConfigs.end()) {
        std::string helperName = vm_helper_get_name(helperId);
        HelperConfig* h = new (globalMM) HelperConfig(helperId);
        h->doInlining = getBoolArg(helperName.c_str(), false);
        h->hotnessPercentToInline = getIntArg((helperName + "_hotnessPercent").c_str(), 0);
        flags.helperConfigs[helperId] = h; 
    }
}

class HelperInliner {
public:
    HelperInliner(HelperInlinerSession* _sessionAction, MemoryManager& tmpMM, CompilationContext* _cc, Inst* _inst, uint32 _hotness, MethodDesc* _helperMethod, VM_RT_SUPPORT _helperId)  
        : flags(((HelperInlinerAction*)_sessionAction->getAction())->getFlags()), localMM(tmpMM), 
        cc(_cc), inst(_inst), session(_sessionAction), method(_helperMethod), helperId(_helperId)
    {
        hotness=_hotness;
        irm = cc->getHIRManager();
        instFactory = &irm->getInstFactory();
        opndManager = &irm->getOpndManager();
        typeManager = &irm->getTypeManager();
        cfg = &irm->getFlowGraph();
    }

    ~HelperInliner(){};

    void run();

    uint32 hotness;

    static MethodDesc* findHelperMethod(CompilationInterface* ci, VM_RT_SUPPORT helperId);

protected:
    void inlineVMHelper(MethodCallInst* call);
    void finalizeCall(MethodCallInst* call);

    HelperInlinerFlags& flags;
    MemoryManager& localMM;
    CompilationContext* cc;
    Inst* inst;
    HelperInlinerSession* session;
    MethodDesc*  method;
    VM_RT_SUPPORT helperId;

    //these fields used by almost every subclass -> cache them
    IRManager* irm;
    InstFactory* instFactory;
    OpndManager* opndManager;
    TypeManager* typeManager;
    ControlFlowGraph* cfg;
private: 
    void inline_NewObj();
    void inline_NewArray();
    void inline_TauLdIntfcVTableAddr();
    void inline_TauCheckCast();
    void inline_TauInstanceOf();
    void inline_TauStRef();
    void inline_TauMonitorEnter();
    void inline_TauMonitorExit();
};

class HelperInlinerCompare {
public:
    bool operator()(const HelperInliner* hi1, const HelperInliner* hi2) { return hi1->hotness < hi2->hotness; }
};

void HelperInlinerSession::_run(IRManager& irm) {
    CompilationContext* cc = getCompilationContext();
    MemoryManager tmpMM("Inline VM helpers");
    HelperInlinerAction* action = (HelperInlinerAction*)getAction();
    HelperInlinerFlags& flags = action->getFlags();

    if (cc->getVMCompilationInterface()->resolveClassUsingBootstrapClassloader(PRAGMA_INLINE) == NULL) {
        Log::out()<<"Helpers inline pass failed! class not found: "<<PRAGMA_INLINE<<std::endl;
        return;
    }
    //finding all helper calls
    ControlFlowGraph& fg = irm.getFlowGraph();
    double entryExecCount = fg.hasEdgeProfile() ? fg.getEntryNode()->getExecCount(): 1;
    uint32 maxNodeCount = irm.getOptimizerFlags().hir_node_threshold;
    StlPriorityQueue<HelperInliner*, StlVector<HelperInliner*>, HelperInlinerCompare> *helperInlineCandidates = 
        new (tmpMM) StlPriorityQueue<HelperInliner*, StlVector<HelperInliner*>, HelperInlinerCompare>(tmpMM);

    const StlMap<Opcode, VM_RT_SUPPORT>& opcodeToHelper = flags.opcodeToHelperMapping;
    const StlMap<VM_RT_SUPPORT, HelperConfig*>& configs = flags.helperConfigs;

    const Nodes& nodes = fg.getNodesPostOrder();//process checking only reachable nodes.
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        double execCount = node->getExecCount();
        assert (execCount >= 0);
        uint32 nodePercent = fg.hasEdgeProfile() ? (uint32)(execCount*100/entryExecCount) : 0;
        if (node->isBlockNode()) {
            for (Inst* inst = (Inst*)node->getFirstInst(); inst!=NULL; inst = inst->getNextInst()) {
                Opcode opcode = inst->getOpcode();
                StlMap<Opcode, VM_RT_SUPPORT>::const_iterator o2h = opcodeToHelper.find(opcode);
                if (o2h == opcodeToHelper.end()) {
                    continue;
                }
                VM_RT_SUPPORT helperId = o2h->second;
                StlMap<VM_RT_SUPPORT, HelperConfig*>::const_iterator iconf = configs.find(helperId);
                if (iconf == configs.end()) {
                    continue;
                }
                HelperConfig* config = iconf->second;
                if (!config->doInlining || nodePercent < config->hotnessPercentToInline) {
                    continue;
                }
                MethodDesc* md = HelperInliner::findHelperMethod(cc->getVMCompilationInterface(), helperId);
                if (md == NULL) {
                    continue;
                }
                HelperInliner* inliner = new (tmpMM) HelperInliner(this, tmpMM, cc, inst, nodePercent, md, helperId);
                
                helperInlineCandidates->push(inliner);
            }
        }
    }

    //running all inliners
    while(!helperInlineCandidates->empty() && (fg.getNodeCount() < maxNodeCount)) {
        HelperInliner* inliner = helperInlineCandidates->top();
        inliner->run();
        helperInlineCandidates->pop();
    }
}

void HelperInliner::run()  {
    if (Log::isEnabled())  {
        Log::out() << "Processing inst:"; inst->print(Log::out()); Log::out()<<std::endl; 
    }
    assert(method);
    switch(inst->getOpcode()) {
        case Op_NewObj:                 inline_NewObj(); break;
        case Op_NewArray:               inline_NewArray(); break;
        case Op_TauLdIntfcVTableAddr:   inline_TauLdIntfcVTableAddr(); break;
        case Op_TauCheckCast:           inline_TauCheckCast(); break;
        case Op_TauInstanceOf:          inline_TauInstanceOf(); break;
        case Op_TauStRef:               inline_TauStRef(); break;
        case Op_TauMonitorEnter:        inline_TauMonitorEnter(); break;
        case Op_TauMonitorExit:         inline_TauMonitorExit(); break;
        default: assert(0);
    }
}

MethodDesc* HelperInliner::findHelperMethod(CompilationInterface* ci, VM_RT_SUPPORT helperId) 
{
    Method_Handle mh = vm_helper_get_magic_helper(helperId);
    if (mh == NULL) {
        if (Log::isEnabled()) Log::out()<<"WARN: helper's method is not resolved:"<<vm_helper_get_name(helperId)<<std::endl;
        return NULL;
    }
    Class_Handle ch = method_get_class(mh);
    if (!class_is_initialized(ch)) {
        if (Log::isEnabled()) Log::out()<<"WARN: class is not initialized:"<<class_get_name(ch)<<std::endl;
        return NULL;
    }
    MethodDesc* md = ci->getMethodDesc(mh);
    return md;
}

void HelperInliner::inlineVMHelper(MethodCallInst* call) {
    if (Log::isEnabled()) {
        Log::out()<<"Inlining VMHelper:";call->print(Log::out());Log::out()<<std::endl;
    }

    Inliner inliner(session, localMM, *irm, false, false, flags.inlinerPipelineName);
    inliner.runInliner(call);
}

void HelperInliner::finalizeCall(MethodCallInst* callInst) {
    //if call is not last inst -> make it last inst
    if (callInst != callInst->getNode()->getLastInst()) {
        cfg->splitNodeAtInstruction(callInst, true, true, instFactory->makeLabel());
    }

    //every call must have exception edge -> add it
    if (callInst->getNode()->getExceptionEdge() == NULL) {
        Node* node = callInst->getNode();
        Node* dispatchNode = node->getUnconditionalEdgeTarget()->getExceptionEdgeTarget();
        if (dispatchNode == NULL) {
            dispatchNode = cfg->getUnwindNode();
            if (dispatchNode==NULL) {
                dispatchNode = cfg->createDispatchNode(instFactory->makeLabel());
                cfg->setUnwindNode(dispatchNode);
                cfg->addEdge(dispatchNode, cfg->getExitNode());
            }
        }
        cfg->addEdge(node, dispatchNode);
    }
}



void HelperInliner::inline_NewObj() {
#if defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_NewObj);

    Opnd* dstOpnd= inst->getDst();
    Type * type = dstOpnd->getType();
    assert(type->isObject());
    ObjectType* objType = type->asObjectType();

    if (objType->isFinalizable()) {
        if (Log::isEnabled()) Log::out()<<"Skipping as finalizable: "<<objType->getName()<<std::endl;
        return;
    }
    //replace newObj with call to a method


#ifdef _EM64T_
    assert(VMInterface::areReferencesCompressed());
#endif
    //the method signature is (int objSize, int allocationHandle)
    int allocationHandle= (int)(POINTER_SIZE_INT)objType->getAllocationHandle();
    int objSize=objType->getObjectSize();

    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* objSizeOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type());
    Opnd* allocationHandleOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type());
    Opnd* callResOpnd = opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type()));
    instFactory->makeLdConst(objSizeOpnd, objSize)->insertBefore(inst);
    instFactory->makeLdConst(allocationHandleOpnd, allocationHandle)->insertBefore(inst);
    Opnd* args[2] = {objSizeOpnd, allocationHandleOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(callResOpnd, tauSafeOpnd, tauSafeOpnd, 2, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();

    assert(call == call->getNode()->getLastInst());

    //todo: use finalizeCall here!

    //convert address type to managed object type
    Edge* fallEdge= call->getNode()->getUnconditionalEdge();
    assert(fallEdge && fallEdge->getTargetNode()->isBlockNode());
    Modifier mod = Modifier(Overflow_None)|Modifier(Exception_Never)|Modifier(Strict_No);
    Node* fallNode = fallEdge->getTargetNode();
    if (fallNode->getInDegree()>1) {
        fallNode = irm->getFlowGraph().spliceBlockOnEdge(fallEdge, instFactory->makeLabel());
    }
    fallNode->prependInst(instFactory->makeConvUnmanaged(mod, type->tag, dstOpnd, callResOpnd));

    inlineVMHelper(call);
#endif
}

void HelperInliner::inline_NewArray() {
#if defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_NewArray);

    //the method signature is (int objSize, int allocationHandle)
    Opnd* dstOpnd = inst->getDst();
    ArrayType* arrayType = dstOpnd->getType()->asArrayType();
#ifdef _EM64T_
    assert(VMInterface::areReferencesCompressed());
#endif
    int allocationHandle = (int)(POINTER_SIZE_INT)arrayType->getAllocationHandle();
    Type* elemType = arrayType->getElementType();
    int elemSize = 0; //TODO: check if references are compressed!
    if (elemType->isBoolean() || elemType->isInt1()) {
        elemSize = 1;
    } else if (elemType->isInt2() || elemType->isChar()) {
        elemSize = 2;
    } else if (elemType->isInt4() || elemType->isSingle()) {
        elemSize = 4;
    } else if (elemType->isInt8() || elemType->isDouble()) {
        elemSize = 8;
    } else {
        elemSize = 4;
    }

    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* numElements = inst->getSrc(0);
    Opnd* elemSizeOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type());
    Opnd* allocationHandleOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type());
    instFactory->makeLdConst(elemSizeOpnd, elemSize)->insertBefore(inst);
    instFactory->makeLdConst(allocationHandleOpnd, allocationHandle)->insertBefore(inst);
    Opnd* args[3] = {numElements, elemSizeOpnd, allocationHandleOpnd};
    Opnd* callResOpnd = opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type()));
    MethodCallInst* call = instFactory->makeDirectCall(callResOpnd, tauSafeOpnd, tauSafeOpnd, 3, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();
    assert(call == call->getNode()->getLastInst());

    //convert address type to managed array type
    Edge* fallEdge= call->getNode()->getUnconditionalEdge();
    assert(fallEdge && fallEdge->getTargetNode()->isBlockNode());
    Modifier mod = Modifier(Overflow_None)|Modifier(Exception_Never)|Modifier(Strict_No);
    Node* fallNode = fallEdge->getTargetNode();
    if (fallNode->getInDegree()>1) {
        fallNode = irm->getFlowGraph().spliceBlockOnEdge(fallEdge, instFactory->makeLabel());
    }
    fallNode->prependInst(instFactory->makeConvUnmanaged(mod, arrayType->tag, dstOpnd, callResOpnd));
    
    inlineVMHelper(call);
#endif
}


void HelperInliner::inline_TauMonitorEnter() {
#if defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_TauMonitorEnter);

    Opnd* objOpnd = inst->getSrc(0);
    assert(objOpnd->getType()->isObject());
    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* args[1] = {objOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(opndManager->getNullOpnd(), tauSafeOpnd, tauSafeOpnd, 1, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();
    
    
    //if call is not last inst -> make it last inst
    if (call != call->getNode()->getLastInst()) {
        cfg->splitNodeAtInstruction(call, true, true, instFactory->makeLabel());
    }

    //every call must have exception edge -> add it
    if (call->getNode()->getExceptionEdge() == NULL) {
        Node* node = call->getNode();
        //this is fake dispatch edge -> monenter must never throw exceptions
        Node* dispatchNode = cfg->getUnwindNode(); 
        assert(dispatchNode != NULL); //method with monitors must have unwind, so no additional checks is done
        cfg->addEdge(node, dispatchNode);
    }
    
    inlineVMHelper(call);
#endif
}

void HelperInliner::inline_TauMonitorExit() {
#if defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_TauMonitorExit);

    Opnd* objOpnd = inst->getSrc(0);
    assert(objOpnd->getType()->isObject());
    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* args[1] = {objOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(opndManager->getNullOpnd(), tauSafeOpnd, tauSafeOpnd, 1, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();

    assert(call == call->getNode()->getLastInst());
    assert(call->getNode()->getExceptionEdge()!=NULL);

    inlineVMHelper(call);
#endif
}

void HelperInliner::inline_TauStRef() {
#if defined  (_EM64T_) || defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_TauStRef);

    Opnd* srcOpnd = inst->getSrc(0);
    Opnd* ptrOpnd = inst->getSrc(1);
    Opnd* objBaseOpnd = inst->getSrc(2);
    assert(srcOpnd->getType()->isObject());
    assert(ptrOpnd->getType()->isPtr());
    assert(objBaseOpnd->getType()->isObject());
    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* args[3] = {objBaseOpnd, ptrOpnd, srcOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(opndManager->getNullOpnd(), tauSafeOpnd, tauSafeOpnd, 3, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();
    
    if (call != call->getNode()->getLastInst()) {
        cfg->splitNodeAtInstruction(call, true, true, instFactory->makeLabel());
    }

    //every call must have exception edge -> add it
    if (call->getNode()->getExceptionEdge() == NULL) {
        Node* node = call->getNode();
        Node* dispatchNode = node->getUnconditionalEdgeTarget()->getExceptionEdgeTarget();
        if (dispatchNode == NULL) {
            dispatchNode = cfg->getUnwindNode();
            if (dispatchNode == NULL) {
                dispatchNode = cfg->createDispatchNode(instFactory->makeLabel());
                cfg->setUnwindNode(dispatchNode);
                cfg->addEdge(dispatchNode, cfg->getExitNode());
            }
        }
        cfg->addEdge(node, dispatchNode);
    }

    inlineVMHelper(call);
#endif
}

void HelperInliner::inline_TauLdIntfcVTableAddr() {
#if defined  (_EM64T_) || defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_TauLdIntfcVTableAddr);

    Opnd* baseOpnd = inst->getSrc(0);
    assert(baseOpnd->getType()->isObject());
    
    Type* type = inst->asTypeInst()->getTypeInfo();
    Opnd* typeOpnd  = opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type()));
    Opnd* typeValOpnd = opndManager->createSsaTmpOpnd(typeManager->getUIntPtrType());
    void* typeId = type->asNamedType()->getRuntimeIdentifier();
    instFactory->makeLdConst(typeValOpnd, (int)typeId)->insertBefore(inst); //TODO: fix this for EM64T
    Modifier mod = Modifier(Overflow_None)|Modifier(Exception_Never)|Modifier(Strict_No);
    instFactory->makeConv(mod, Type::UnmanagedPtr, typeOpnd, typeValOpnd)->insertBefore(inst);
    
    Opnd* resOpnd = inst->getDst();
    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* args[2] = {baseOpnd, typeOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(resOpnd, tauSafeOpnd, tauSafeOpnd, 2, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();

    finalizeCall(call);
    
    inlineVMHelper(call);
#endif
}



void HelperInliner::inline_TauCheckCast() {
#if defined  (_EM64T_) || defined (_IPF_)
    return;
#else
    //TODO: this is 99% clone of instanceof but result of the slow call's result is handled differently. 
    //to merge these 2 helpers into the one we need to change IA32CG codegen tau_checkcast handling:
    //call VM helper for static_cast opcode but not for tau!

    assert(inst->getOpcode() == Op_TauCheckCast);

    Opnd* objOpnd = inst->getSrc(0);
    assert(objOpnd->getType()->isObject());
    Type* type = inst->asTypeInst()->getTypeInfo();
    assert(type->isObject());
    ObjectType* castType = type->asObjectType();

    void* typeId = castType->getRuntimeIdentifier();
    bool isArrayType = castType->isArrayType();
    bool isInterfaceType = castType->isInterface();
    bool isFastInstanceOf = castType->getFastInstanceOfFlag();
    bool isFinalTypeCast = castType->isFinalClass();
    int  fastCheckDepth = isFastInstanceOf ? castType->getClassDepth() : 0;
    
    Opnd* typeValOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type()); 
    instFactory->makeLdConst(typeValOpnd, (int)typeId)->insertBefore(inst);//TODO: em64t & ipf!

    Opnd* typeOpnd = opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type())); 
    Modifier mod = Modifier(Overflow_None)|Modifier(Exception_Never)|Modifier(Strict_No);
    instFactory->makeConv(mod, Type::UnmanagedPtr, typeOpnd, typeValOpnd)->insertBefore(inst);

    Opnd* isArrayOpnd = opndManager->createSsaTmpOpnd(typeManager->getBooleanType());
    instFactory->makeLdConst(isArrayOpnd, isArrayType)->insertBefore(inst);

    Opnd* isInterfaceOpnd = opndManager->createSsaTmpOpnd(typeManager->getBooleanType());
    instFactory->makeLdConst(isInterfaceOpnd, isInterfaceType)->insertBefore(inst);

    Opnd* isFinalOpnd = opndManager->createSsaTmpOpnd(typeManager->getBooleanType());
    instFactory->makeLdConst(isFinalOpnd, isFinalTypeCast)->insertBefore(inst);

    Opnd* fastCheckDepthOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type());
    instFactory->makeLdConst(fastCheckDepthOpnd, fastCheckDepth)->insertBefore(inst);

    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);

    Opnd* args[6] = {objOpnd, typeOpnd, isArrayOpnd, isInterfaceOpnd, isFinalOpnd, fastCheckDepthOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(opndManager->getNullOpnd(), tauSafeOpnd, tauSafeOpnd, 6, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    
    call->insertBefore(inst);

    // after the call the cast is safe
    Opnd* tauSafeOpnd2 = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd2)->insertBefore(inst);
    instFactory->makeCopy(inst->getDst(), tauSafeOpnd2)->insertBefore(inst);

    inst->unlink();

    finalizeCall(call);

    inlineVMHelper(call);
#endif
}


void HelperInliner::inline_TauInstanceOf() {
#if defined  (_EM64T_) || defined (_IPF_)
    return;
#else
    assert(inst->getOpcode() == Op_TauInstanceOf);

    Opnd* objOpnd = inst->getSrc(0);
    assert(objOpnd->getType()->isObject());
    Type* type = inst->asTypeInst()->getTypeInfo();
    assert(type->isObject());
    ObjectType* castType = type->asObjectType();

    void* typeId = castType->getRuntimeIdentifier();
    bool isArrayType = castType->isArrayType();
    bool isInterfaceType = castType->isInterface();
    bool isFastInstanceOf = castType->getFastInstanceOfFlag();
    bool isFinalTypeCast = castType->isFinalClass();
    int  fastCheckDepth = isFastInstanceOf ? castType->getClassDepth() : 0;

    Opnd* typeValOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type()); 
    instFactory->makeLdConst(typeValOpnd, (int)typeId)->insertBefore(inst);//TODO: em64t & ipf!

    Opnd* typeOpnd = opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type())); 
    Modifier mod = Modifier(Overflow_None)|Modifier(Exception_Never)|Modifier(Strict_No);
    instFactory->makeConv(mod, Type::UnmanagedPtr, typeOpnd, typeValOpnd)->insertBefore(inst);

    Opnd* isArrayOpnd = opndManager->createSsaTmpOpnd(typeManager->getBooleanType());
    instFactory->makeLdConst(isArrayOpnd, isArrayType)->insertBefore(inst);

    Opnd* isInterfaceOpnd = opndManager->createSsaTmpOpnd(typeManager->getBooleanType());
    instFactory->makeLdConst(isInterfaceOpnd, isInterfaceType)->insertBefore(inst);

    Opnd* isFinalOpnd = opndManager->createSsaTmpOpnd(typeManager->getBooleanType());
    instFactory->makeLdConst(isFinalOpnd, isFinalTypeCast)->insertBefore(inst);

    Opnd* fastCheckDepthOpnd = opndManager->createSsaTmpOpnd(typeManager->getInt32Type());
    instFactory->makeLdConst(fastCheckDepthOpnd, fastCheckDepth)->insertBefore(inst);

    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);

    Opnd* args[6] = {objOpnd, typeOpnd, isArrayOpnd, isInterfaceOpnd, isFinalOpnd, fastCheckDepthOpnd};
    MethodCallInst* call = instFactory->makeDirectCall(inst->getDst(), tauSafeOpnd, tauSafeOpnd, 6, args, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());

    call->insertBefore(inst);
    inst->unlink();

    finalizeCall(call);

    inlineVMHelper(call);
#endif
}

}//namespace
