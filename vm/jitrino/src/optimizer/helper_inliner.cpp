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

namespace Jitrino {

struct HelperInlinerFlags {
    const char* inlinerPipelineName;

    bool insertInitilizers;
    bool doInlining;

    bool  newObj_doInlining;
    int   newObj_hotnessPercentToInline;
    const char* newObj_className;
    const char* newObj_methodName;
    const char* newObj_signature;
};

class HelperInlinerAction: public Action {
public:
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
    
    
    //new obj inlining params;
    flags.newObj_doInlining = getBoolArg("newObj", false);

    flags.newObj_signature = "(II)Ljava/lang/Object;";
    if (flags.newObj_doInlining) {
        flags.newObj_className = getStringArg("newObj_className", NULL);
        flags.newObj_methodName = getStringArg("newObj_methodName", NULL);
        flags.newObj_hotnessPercentToInline = getIntArg("newObj_hotnessPercent", 0);
        if (flags.newObj_className == NULL || flags.newObj_methodName == NULL) {
            //TODO:? crash("Invalid newObj fast path helper name: %s::%s\n", flags.newObj_className, flags.newObj_methodName);
            flags.newObj_doInlining = false;
        }
    }
    
    if (!flags.newObj_doInlining){
        flags.newObj_className = NULL;
        flags.newObj_methodName = NULL;
    }

}


class HelperInliner {
public:
    HelperInliner(HelperInlinerSession* _sessionAction, MemoryManager& tmpMM, CompilationContext* _cc, Inst* _inst)  
        : flags(((HelperInlinerAction*)_sessionAction->getAction())->getFlags()), localMM(tmpMM), 
        cc(_cc), inst(_inst), action(_sessionAction)
    {}
    virtual ~HelperInliner(){};

    virtual void doInline() = 0;
protected:
    MethodDesc* ensureClassIsResolvedAndInitialized(const char* className,  const char* methodName, const char* signature);
    void inlineVMHelper(MethodCallInst* call);

    HelperInlinerFlags& flags;
    MemoryManager& localMM;
    CompilationContext* cc;
    Inst* inst;
    HelperInlinerSession* action;
};

class NewObjHelperInliner : public HelperInliner {
public:
    NewObjHelperInliner(HelperInlinerSession* session, MemoryManager& tmpMM, CompilationContext* cc, Inst* inst) 
        : HelperInliner(session, tmpMM, cc, inst){}
        virtual void doInline();
};


void HelperInlinerSession::_run(IRManager& irm) {
    CompilationContext* cc = getCompilationContext();
    MemoryManager tmpMM(1024, "Inline VM helpers");
    HelperInlinerAction* action = (HelperInlinerAction*)getAction();
    HelperInlinerFlags& flags = action->getFlags();
    if (!flags.doInlining) {
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
        if (node->isBlockNode()) { //only block nodes can have helper calls today
            for (Inst* inst = (Inst*)node->getFirstInst(); inst!=NULL; inst = inst->getNextInst()) {
                Opcode opcode = inst->getOpcode();
                switch(opcode) {
                    case Op_NewObj:
                        if (flags.newObj_doInlining && nodePercent >= flags.newObj_hotnessPercentToInline) {
                            helperInliners.push_back(new (tmpMM) NewObjHelperInliner(this, tmpMM, cc, inst));
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
        inliner->doInline();
    }
}


MethodDesc* HelperInliner::ensureClassIsResolvedAndInitialized(const char* className, const char* methodName, const char* signature) 
{
    CompilationInterface* ci = cc->getVMCompilationInterface();
    ObjectType* clazz = ci->resolveClassUsingBootstrapClassloader(className);
    if (!clazz) {
        if (Log::isEnabled()) Log::out()<<"Error: class not found:"<<className<<std::endl;
        flags.doInlining=false;
        return NULL;
    }
    //helper class is resolved here -> check if initialized
    IRManager* irm = cc->getHIRManager();
    InstFactory& instFactory = irm->getInstFactory();
    if (clazz->needsInitialization()) {
        if (flags.insertInitilizers) {
            instFactory.makeInitType(clazz)->insertBefore(inst);
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

void HelperInliner::inlineVMHelper(MethodCallInst* call) {
    CompilationInterface* ci = cc->getVMCompilationInterface();
    IRManager* irm = cc->getHIRManager();

    //now inline the call
    CompilationContext inlineCC(cc->getCompilationLevelMemoryManager(), ci, cc->getCurrentJITContext());
    inlineCC.setPipeline(cc->getPipeline());

    Inliner inliner(action, localMM, *irm, false);
    InlineNode* regionToInline = inliner.createInlineNode(inlineCC, call);

    inliner.connectRegion(regionToInline);

    // Optimize inlined region before splicing
    inlineCC.stageId = cc->stageId;
    Inliner::runInlinerPipeline(inlineCC, flags.inlinerPipelineName);
    cc->stageId = inlineCC.stageId;

    inliner.inlineRegion(regionToInline, false);
}

void NewObjHelperInliner::doInline() {
#ifdef _EM64T_
    return;
#else
    if (Log::isEnabled())  {
        Log::out() << "Processing inst:"; inst->print(Log::out()); Log::out()<<std::endl;
    }
    assert(inst->getOpcode() == Op_NewObj);

    //find the method
    MethodDesc* method = ensureClassIsResolvedAndInitialized(flags.newObj_className, flags.newObj_methodName, flags.newObj_signature);
    if (!method) {
        return;
    }
    
    TypeInst *typeInst = (TypeInst*)inst;
    Type * type = typeInst->getTypeInfo();
    assert(type->isObject());
    ObjectType* objType = type->asObjectType();

    if (objType->isFinalizable()) {
        if (Log::isEnabled()) Log::out()<<"Skipping as finalizable: "<<objType->getName()<<std::endl;
        return;
    }
    //replace newObj with call to a method

    //the method signature is (int objSize, int allocationHandle)
    int allocationHandle= (int)objType->getAllocationHandle();
    int objSize=objType->getObjectSize();

    IRManager* irm = cc->getHIRManager();
    InstFactory& instFactory = irm->getInstFactory();
    OpndManager& opndManager = irm->getOpndManager();
    TypeManager& typeManager = irm->getTypeManager();

    Opnd* tauSafeOpnd = opndManager.createSsaTmpOpnd(typeManager.getTauType());
    instFactory.makeTauSafe(tauSafeOpnd)->insertBefore(inst);
    Opnd* res = inst->getDst();
    Opnd* objSizeOpnd = opndManager.createSsaTmpOpnd(typeManager.getInt32Type());
    Opnd* allocationHandleOpnd = opndManager.createSsaTmpOpnd(typeManager.getInt32Type());
    instFactory.makeLdConst(objSizeOpnd, objSize)->insertBefore(inst);
    instFactory.makeLdConst(allocationHandleOpnd, allocationHandle)->insertBefore(inst);
    Opnd* args[2] = {objSizeOpnd, allocationHandleOpnd};
    MethodCallInst* call = instFactory.makeDirectCall(res, tauSafeOpnd, tauSafeOpnd, 2, args, method)->asMethodCallInst();
    call->insertBefore(inst);
    inst->unlink();
    assert(call == call->getNode()->getLastInst());

    //inline the method
    inlineVMHelper(call);
#endif
}

}//namespace

