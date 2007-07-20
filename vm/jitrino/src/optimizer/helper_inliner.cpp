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

namespace Jitrino {

struct HelperInlinerFlags {
    const char* inlinerPipelineName;

    bool insertInitilizers;
    bool doInlining;

#define DECLARE_STANDARD_HELPER_FLAGS(name) \
    bool  name##_doInlining;\
    int   name##_hotnessPercentToInline;\
    const char* name##_className;\
    const char* name##_methodName;\
    const char* name##_signature;\

DECLARE_STANDARD_HELPER_FLAGS(newObj);
DECLARE_STANDARD_HELPER_FLAGS(newArray);
DECLARE_STANDARD_HELPER_FLAGS(objMonEnter);
DECLARE_STANDARD_HELPER_FLAGS(objMonExit);
DECLARE_STANDARD_HELPER_FLAGS(wb);
DECLARE_STANDARD_HELPER_FLAGS(ldInterface);
DECLARE_STANDARD_HELPER_FLAGS(checkCast);
DECLARE_STANDARD_HELPER_FLAGS(instanceOf);
    
};

class HelperInlinerAction: public Action {
public:
    HelperInlinerAction() {}
    void init();
    HelperInlinerFlags& getFlags() {return flags;}
protected:
    HelperInlinerFlags flags;
};

DEFINE_SESSION_ACTION_WITH_ACTION(HelperInlinerSession, HelperInlinerAction, inline_helpers, "VM helpers inlining");

void HelperInlinerAction::init() {
    flags.inlinerPipelineName = getStringArg("pipeline", "inliner_pipeline");
    flags.insertInitilizers = getBoolArg("insertInitilizers", false);
    flags.doInlining = true;
    
    
#define READ_STANDARD_HELPER_FLAGS(name)\
    flags.name##_doInlining = getBoolArg(#name, false);\
    if (flags.name##_doInlining) {\
    flags.name##_className = getStringArg(#name"_className", NULL);\
    flags.name##_methodName = getStringArg(#name"_methodName", NULL);\
    flags.name##_hotnessPercentToInline = getIntArg(#name"_hotnessPercent", 0);\
        if (flags.name##_className == NULL || flags.name##_methodName == NULL) {\
            if (Log::isEnabled()) {\
                Log::out()<<"Invalid fast path helper name:"<<flags.name##_className<<"::"<<flags.name##_methodName<<std::endl;\
            }\
            flags.name##_doInlining = false;\
        }\
    }\
    if (!flags.name##_doInlining){\
        flags.name##_className = NULL;\
        flags.name##_methodName = NULL;\
    }\

    READ_STANDARD_HELPER_FLAGS(newObj);
    flags.newObj_signature = "(II)Lorg/vmmagic/unboxed/Address;";

    READ_STANDARD_HELPER_FLAGS(newArray);
    flags.newArray_signature = "(III)Lorg/vmmagic/unboxed/Address;";

    READ_STANDARD_HELPER_FLAGS(objMonEnter);
    flags.objMonEnter_signature = "(Ljava/lang/Object;)V";

    READ_STANDARD_HELPER_FLAGS(objMonExit);
    flags.objMonExit_signature = "(Ljava/lang/Object;)V";

    READ_STANDARD_HELPER_FLAGS(wb);
    flags.wb_signature = "(Lorg/vmmagic/unboxed/Address;Lorg/vmmagic/unboxed/Address;Lorg/vmmagic/unboxed/Address;)V";

    READ_STANDARD_HELPER_FLAGS(ldInterface);
    flags.ldInterface_signature = "(Ljava/lang/Object;Lorg/vmmagic/unboxed/Address;)Lorg/vmmagic/unboxed/Address;";
    
    READ_STANDARD_HELPER_FLAGS(checkCast);
    flags.checkCast_signature = "(Ljava/lang/Object;Lorg/vmmagic/unboxed/Address;ZZZI)Ljava/lang/Object;";
    
    READ_STANDARD_HELPER_FLAGS(instanceOf);
    flags.instanceOf_signature = "(Ljava/lang/Object;Lorg/vmmagic/unboxed/Address;ZZZI)Z";
}


class HelperInliner {
public:
    HelperInliner(HelperInlinerSession* _sessionAction, MemoryManager& tmpMM, CompilationContext* _cc, Inst* _inst)  
        : flags(((HelperInlinerAction*)_sessionAction->getAction())->getFlags()), localMM(tmpMM), 
        cc(_cc), inst(_inst), session(_sessionAction), method(NULL)
    {
        irm = cc->getHIRManager();
        instFactory = &irm->getInstFactory();
        opndManager = &irm->getOpndManager();
        typeManager = &irm->getTypeManager();
        cfg = &irm->getFlowGraph();
    }

    virtual ~HelperInliner(){};
    
    virtual void run()=0;
protected:
    MethodDesc* ensureClassIsResolvedAndInitialized(const char* className,  const char* methodName, const char* signature);
    virtual void doInline() = 0;
    void inlineVMHelper(MethodCallInst* call);
    void finalizeCall(MethodCallInst* call);

    HelperInlinerFlags& flags;
    MemoryManager& localMM;
    CompilationContext* cc;
    Inst* inst;
    HelperInlinerSession* session;
    MethodDesc*  method;
    
//these fields used by almost every subclass -> cache them
    IRManager* irm;
    InstFactory* instFactory;
    OpndManager* opndManager;
    TypeManager* typeManager;
    ControlFlowGraph* cfg;

};
#define DECLARE_HELPER_INLINER(name, flagPrefix)\
class name : public HelperInliner {\
public:\
    name (HelperInlinerSession* session, MemoryManager& tmpMM, CompilationContext* cc, Inst* inst)\
        : HelperInliner(session, tmpMM, cc, inst){}\
    \
    virtual void run() { \
        if (Log::isEnabled())  {\
            Log::out() << "Processing inst:"; inst->print(Log::out()); Log::out()<<std::endl; \
        }\
        method = ensureClassIsResolvedAndInitialized(flags.flagPrefix##_className, flags.flagPrefix##_methodName, flags.flagPrefix##_signature);\
        if (!method) return;\
        doInline();\
    }\
    virtual void doInline();\
};\

DECLARE_HELPER_INLINER(NewObjHelperInliner, newObj)
DECLARE_HELPER_INLINER(NewArrayHelperInliner, newArray)
DECLARE_HELPER_INLINER(ObjMonitorEnterHelperInliner, objMonEnter)
DECLARE_HELPER_INLINER(ObjMonitorExitHelperInliner, objMonExit)
DECLARE_HELPER_INLINER(WriteBarrierHelperInliner, wb)
DECLARE_HELPER_INLINER(LdInterfaceHelperInliner, ldInterface)
DECLARE_HELPER_INLINER(CheckCastHelperInliner, checkCast)
DECLARE_HELPER_INLINER(InstanceOfHelperInliner, instanceOf)

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
    StlVector<HelperInliner*> helperInliners(tmpMM);
    const Nodes& nodes = fg.getNodesPostOrder();//process checking only reachable nodes.
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        int nodePercent = fg.hasEdgeProfile() ? (int)(node->getExecCount()*100/entryExecCount) : 0;
        if (node->isBlockNode()) {
            for (Inst* inst = (Inst*)node->getFirstInst(); inst!=NULL; inst = inst->getNextInst()) {
                Opcode opcode = inst->getOpcode();
                switch(opcode) {
                    case Op_NewObj:
                        if (flags.newObj_doInlining && nodePercent >= flags.newObj_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) NewObjHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_NewArray:
                        if (flags.newArray_doInlining && nodePercent >= flags.newArray_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) NewArrayHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_TauMonitorEnter:
                        if (flags.objMonEnter_doInlining && nodePercent >= flags.objMonEnter_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) ObjMonitorEnterHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_TauMonitorExit:
                        if (flags.objMonExit_doInlining && nodePercent >= flags.objMonExit_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) ObjMonitorExitHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_TauStRef:
                        if (flags.wb_doInlining && nodePercent >= flags.wb_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) WriteBarrierHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_TauLdIntfcVTableAddr:
                        if (flags.ldInterface_doInlining && nodePercent >= flags.ldInterface_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) LdInterfaceHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_TauCheckCast:
                        if (flags.checkCast_doInlining && nodePercent >= flags.checkCast_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) CheckCastHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    case Op_TauInstanceOf:
                        if (flags.instanceOf_doInlining && nodePercent >= flags.instanceOf_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) InstanceOfHelperInliner(this, tmpMM, cc, inst));
                        }
                        break;
                    default: break;
                }
            }
        }
    }

    //running all inliners
    //TODO: set inline limit!
    for (StlVector<HelperInliner*>::const_iterator it = helperInliners.begin(), end = helperInliners.end(); it!=end; ++it) {
        HelperInliner* inliner = *it;
        inliner->run();
    }
}


MethodDesc* HelperInliner::ensureClassIsResolvedAndInitialized(const char* className, const char* methodName, const char* signature) 
{
    CompilationInterface* ci = cc->getVMCompilationInterface();
    ObjectType* clazz = ci->resolveClassUsingBootstrapClassloader(className);
    if (!clazz) {
        if (Log::isEnabled()) Log::out()<<"Error: class not found:"<<className<<std::endl;
        return NULL;
    }
    //helper class is resolved here -> check if initialized
    if (clazz->needsInitialization()) {
        if (flags.insertInitilizers) {
            instFactory->makeInitType(clazz)->insertBefore(inst);
        }
        return NULL;
    }
    //helper class is initialized here -> inline it.
    MethodDesc* method = ci->resolveMethod(clazz, methodName, signature);
    if (!method) {
        if (Log::isEnabled()) Log::out()<<"Error: method not found:"<<className<<"::"<<methodName<<signature<<std::endl;;
        return NULL;
    }
    assert (method->isStatic());
    return method;

}

typedef StlVector<MethodCallInst*> InlineVector;


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



void NewObjHelperInliner::doInline() {
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

void NewArrayHelperInliner::doInline() {
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


void ObjMonitorEnterHelperInliner::doInline() {
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

void ObjMonitorExitHelperInliner::doInline() {
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
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();

    assert(call == call->getNode()->getLastInst());
    assert(call->getNode()->getExceptionEdge()!=NULL);

    inlineVMHelper(call);
#endif
}

void WriteBarrierHelperInliner::doInline() {
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

void LdInterfaceHelperInliner::doInline() {
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
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();

    finalizeCall(call);
    
    inlineVMHelper(call);
#endif
}



void CheckCastHelperInliner::doInline() {
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


void InstanceOfHelperInliner::doInline() {
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
    call->setBCOffset(inst->getBCOffset());

    call->insertBefore(inst);
    inst->unlink();

    finalizeCall(call);

    inlineVMHelper(call);
#endif
}

}//namespace
