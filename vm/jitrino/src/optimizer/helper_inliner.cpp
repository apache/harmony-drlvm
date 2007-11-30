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
#include "VMMagic.h"

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
        : inlinerPipelineName(NULL), pragmaInlineType(NULL), opcodeToHelperMapping(mm), helperConfigs(mm){}

    const char* inlinerPipelineName;
    Type* pragmaInlineType;

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
    HelperInliner(HelperInlinerSession* _sessionAction, MemoryManager& tmpMM, CompilationContext* _cc, Inst* _inst, 
        uint32 _hotness, MethodDesc* _helperMethod, VM_RT_SUPPORT _helperId)  
        : flags(((HelperInlinerAction*)_sessionAction->getAction())->getFlags()), localMM(tmpMM), 
        cc(_cc), inst(_inst), session(_sessionAction), method(_helperMethod),  helperId(_helperId)
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

    //cache these values for convenience of use
    IRManager* irm;
    InstFactory* instFactory;
    OpndManager* opndManager;
    TypeManager* typeManager;
    ControlFlowGraph* cfg;
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

    if (flags.pragmaInlineType== NULL) {
        flags.pragmaInlineType= cc->getVMCompilationInterface()->resolveClassUsingBootstrapClassloader(PRAGMA_INLINE_TYPE_NAME);
        if (flags.pragmaInlineType == NULL) {
            Log::out()<<"Helpers inline pass failed! class not found: "<<PRAGMA_INLINE_TYPE_NAME<<std::endl;
            return;
        }
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
#ifdef _EM64T_
    assert(VMInterface::areReferencesCompressed());
#endif

    if (Log::isEnabled())  {
        Log::out() << "Processing inst:"; inst->print(Log::out()); Log::out()<<std::endl; 
    }
    assert(method);

    //Convert all inst params info helper params
    uint32 numHelperArgs = method->getNumParams();
    uint32 numInstArgs = inst->getNumSrcOperands();
    Opnd** helperArgs =new (irm->getMemoryManager()) Opnd*[numHelperArgs];
#ifdef _DEBUG
    std::fill(helperArgs, helperArgs + numHelperArgs, (Opnd*)NULL);
#endif
    uint32 currentHelperArg = 0;
    if (inst->isType()) {
        Type* type = inst->asTypeInst()->getTypeInfo();
        assert(type->isNamedType());
        Opnd* typeOpnd = opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type()));
        Inst* ldconst = instFactory->makeLdConst(typeOpnd, (POINTER_SIZE_SINT)type->asNamedType()->getVMTypeHandle());
        ldconst->insertBefore(inst);
        helperArgs[currentHelperArg] = typeOpnd;
        currentHelperArg++;
    }
    for (uint32 i = 0; i < numInstArgs; i++) {
        Opnd* instArg = inst->getSrc(i);
        if (instArg->getType()->tag == Type::Tau) { //TODO: what to do with taus?
            continue;
        }
        assert(currentHelperArg < numHelperArgs);
        helperArgs[currentHelperArg] = instArg;
    }
    assert(helperArgs[numHelperArgs-1]!=NULL);


    //Prepare res opnd
    Opnd* instResOpnd = inst->getDst();
    Type* helperRetType = method->getReturnType();
    Type* instResType = instResOpnd->getType();
    bool resIsTau = instResType && Type::isTau(instResType->tag);
    if (resIsTau) {
        assert(helperRetType->isVoid());
        instResType = NULL;
        instResOpnd = opndManager->getNullOpnd();
    }
    bool needResConv = instResType && instResType->isObject() && helperRetType->isObject() && VMMagicUtils::isVMMagicClass(helperRetType->asObjectType()->getName());
    Opnd* helperResOpnd = !needResConv ? instResOpnd : opndManager->createSsaTmpOpnd(typeManager->getUnmanagedPtrType(typeManager->getInt8Type()));
    
    //Make a call inst
    Opnd* tauSafeOpnd = opndManager->createSsaTmpOpnd(typeManager->getTauType());
    instFactory->makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    MethodCallInst* call = instFactory->makeDirectCall(helperResOpnd, tauSafeOpnd, tauSafeOpnd, numHelperArgs, helperArgs, method)->asMethodCallInst();
    assert(inst->getBCOffset()!=ILLEGAL_BC_MAPPING_VALUE);
    call->setBCOffset(inst->getBCOffset());
    call->insertBefore(inst);
    inst->unlink();

    finalizeCall(call); //make call last inst in a block

    if (needResConv || resIsTau) {
        //convert address type to managed object type
        Edge* fallEdge = call->getNode()->getUnconditionalEdge();
        assert(fallEdge && fallEdge->getTargetNode()->isBlockNode());
        Node* fallNode = fallEdge->getTargetNode();
        if (fallNode->getInDegree()>1) {
            fallNode = irm->getFlowGraph().spliceBlockOnEdge(fallEdge, instFactory->makeLabel());
        }
        if (needResConv) {
            Modifier mod = Modifier(Overflow_None)|Modifier(Exception_Never)|Modifier(Strict_No);
            fallNode->prependInst(instFactory->makeConvUnmanaged(mod, Type::Object, instResOpnd, helperResOpnd));
        } else {
            assert(resIsTau);
            Opnd* tauSafeOpnd2 = opndManager->createSsaTmpOpnd(typeManager->getTauType());
            Inst* makeTau = instFactory->makeTauSafe(tauSafeOpnd2);
            fallNode->prependInst(makeTau);
            instFactory->makeCopy(inst->getDst(), tauSafeOpnd2)->insertAfter(makeTau);
            
        }
    }

    //Inline the call
    inlineVMHelper(call);
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
}//namespace
