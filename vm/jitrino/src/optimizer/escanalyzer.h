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
 * @author Intel, Natalya V. Golovleva
 * @version $Revision$
 *
 */

#ifndef _ESCANALYSIS_H_
#define _ESCANALYSIS_H_

#include "Stl.h"
#include "optpass.h"
#include "FlowGraph.h"
#include "irmanager.h"
#include "mkernel.h"

namespace Jitrino {

//
// Escape analyzer (synchronization removing)
//
class EscAnalyzer {

public:
    EscAnalyzer(MemoryManager& mm, SessionAction* argSource, IRManager& irm);
    
    void doAnalysis();

// CnG node types
    static const uint32 NT_OBJECT   = 8;  // Op_LdRef,Op_NewObj,Op_NewArray,Op_NewMultiArray    
    static const uint32 NT_DEFARG   = NT_OBJECT+1;  // formal parameter - Op_DefArg
    static const uint32 NT_RETVAL   = 16;           // Op_DirectCall,Op_IndirectMemoryCall-returned by method 
    static const uint32 NT_CATCHVAL = NT_RETVAL+1;  // catched value
    static const uint32 NT_LDOBJ    = 32;           // Op_LdConstant,Op_TauLdInd,Op_LdVar, 
                                                    // Op_TauStaticCast,Op_TauCast 
    static const uint32 NT_INTPTR   = NT_LDOBJ+1;   // Op_SaveRet
    static const uint32 NT_VARVAL   = NT_LDOBJ+2;   // Op_StVar,Op_Phi
    static const uint32 NT_ARRELEM  = NT_LDOBJ+3;   // Op_LdArrayBaseAddr,Op_AddScaledIndex
    static const uint32 NT_REF      = NT_LDOBJ+4;   // reference value - Op_LdFieldAddr,
                                                    // Op_LdStaticAddr
    static const uint32 NT_STFLD    = 64;           // Op_LdStaticAddr
    static const uint32 NT_INSTFLD  = NT_STFLD+1;   // Op_LdFieldAddr
    static const uint32 NT_ACTARG   = 128;          // Op_DirectCall,Op_IndirectMemoryCall
    static const uint32 NT_EXITVAL  = 256;          // returned value - Op_Return
    static const uint32 NT_THRVAL   = NT_EXITVAL+1; // thrown value - Op_Throw
    static const uint32 NT_OBJS     = NT_OBJECT|NT_RETVAL|NT_LDOBJ;  //for findCnGNode_op
// CnG node reference types
    static const uint32 NR_REF = 1;
    static const uint32 NR_ARR = 2;
    static const uint32 NR_REFARR = 3;
// CnG edge types
    static const uint32 ET_POINT = 1;  // Op_TauLdInd (loaded value)
    static const uint32 ET_DEFER = 2;
    static const uint32 ET_FIELD = 3;  // Op_LdFieldAddr (object field), Op_AddScaledIndex
// CG node states
    static const uint32 GLOBAL_ESCAPE = 1;
    static const uint32 ARG_ESCAPE = 2;
    static const uint32 NO_ESCAPE = 3;
    static const uint32 ESC_MASK = 3;
    static const uint32 BIT_MASK = 56;
    static const uint32 LOOP_CREATED = 8;
    static const uint32 CALLEE_ESCAPED = 16;
    static const uint32 VIRTUAL_CALL = 32;

    typedef StlList<uint32> NodeMDs;    

    struct CnGNode;
    struct CnGRef {
        CnGNode* cngNodeTo;
        uint32 edgeType;
        Inst* edgeInst;
    };

    typedef StlList<CnGRef*> CnGRefs;   
    typedef StlList<Inst*> Insts;

    struct CnGNode {
        uint32 cngNodeId;   // CnG node id
        uint32 opndId;      // opnd id  (0 for NT_ACTARG) 
        void* refObj;       // MethodDesc* for NT_ACTARG, Inst* for fields, Opnd* for others
        uint32 nodeType;    // CnG node types
        uint32 nodeRefType; // CnG node reference types
        uint32 instrId;
        CnGNode* lNode;     // ldind from lNode 
        Node* fgNode;
        uint32 state;       // escape state
        NodeMDs* nodeMDs;   // list of NT_ACTARG nodes
        Inst* nInst;        // ref to inst 
        uint32 argNumber;   // number of arg for NT_DEFARG & NT_ACTARG (0 for others)
        CnGRefs* outEdges;  // cngNode out edges
    };

    struct MemberIdent {
        char* parentName;
        char* name;
        char* signature;
    };

    struct InstFld;
    typedef StlList<InstFld*> InstFlds; 

    struct InstFld {
        MemberIdent* fldIdent;
        uint32 state;
        InstFlds* instFlds;   // contained instance fields
    };

    struct ParamInfo {
        uint32 paramNumber;
        uint32 state;
        InstFlds* instFlds;   // contained instance fields
    }; 

    typedef StlList<ParamInfo*> ParamInfos; 

    struct CalledMethodInfo {
        MemberIdent* methodIdent;
        uint32 numberOfArgs;
        uint32 properties;     // native, final, virtual ...
        ParamInfos* paramInfos;
        uint32 retValueState;
        bool mon_on_this;
    };

    typedef StlList<CalledMethodInfo*> CalledMethodInfos;

    struct MonUnit {
        uint32 opndId;
        Insts* monInsts;
        Insts* icallInsts;
    };

    MemoryManager& eaMemManager;

    static CalledMethodInfos* calledMethodInfos;
    static Mutex calledMethodInfosLock;
    
    uint32 allProps;
    const char* debug_method;

private:
    static const int maxMethodExamLevel_default = 5;

    struct CnGEdge {
        CnGNode* cngNodeFrom;
        CnGRefs* refList;
    };

    typedef StlList<CnGNode*> CnGNodes; 
    typedef StlList<CnGEdge*> CnGEdges; 
    
    struct cfgNode {
        uint32 nodeId;
        Node* fgNode;
        Insts* instructions;
    };

    typedef StlList<uint32> ObjIds; 

    typedef StlList<MonUnit*> MonInstUnits; 

    EscAnalyzer(EscAnalyzer* parent, IRManager& irm);
    
    uint32 maxMethodExamLevel;
    IRManager& irManager;
    MethodDesc& mh;            // analyzed method header
    CnGNodes* cngNodes;
    CnGEdges* cngEdges;
    uint32 lastCnGNodeId;
    uint32 curMDNode;
    int defArgNumber;
    uint32 method_ea_level;
    ObjIds *scannedObjs;
    ObjIds *scannedInsts;
    ObjIds *scannedSucNodes;
    uint32 initNodeType;  // type of initial scanned node
    MonInstUnits* monitorInstUnits ;
    SsaTmpOpnd* i32_0;
    SsaTmpOpnd* i32_1;
    TranslatorAction* translatorAction;


#ifdef _DEBUG
    void prPrN(cfgNode pr_n[],int maxInd) {
        Log::out()<<"--------------------"<<std::endl;
        Log::out() <<"    pr_n contains"<<std::endl;
        for (int i=0; i<maxInd; i++) {
            Log::out() <<i<<"  nId "<<(pr_n[i]).nodeId;
            Log::out() <<"  Node "<<(pr_n[i]).fgNode->getId();
            Log::out() <<std::endl;
        }
        Log::out() <<"--------------------"<<std::endl;
    }
#endif
    //common method for both EscAnalyzer constructors
    void init();
    void instrExam(cfgNode* node);
    void instrExam2(cfgNode* node);
    void addEdge(CnGNode* cgnfrom, CnGNode* cgnto, uint32 etype, Inst* inst);
    void printCnGEdges(char* text,::std::ostream& os); 
    void what_inst(Inst* inst,::std::ostream& os); 
    void debug_inst_info(Inst* inst,::std::ostream& os);
    void debug_opnd_info(Opnd* opnd,::std::ostream& os);
    void ref_type_info(Type* type,::std::ostream& os);
    CnGNode* addCnGNode(Inst* inst, Type* type, uint32 ntype);
    CnGNode* addCnGNode_op(Inst* inst, Type* type, uint32 ntype);
    CnGNode* addCnGNode_mp(Inst* inst, MethodDesc* md, uint32 ntype, uint32 narg);
    CnGNode* addCnGNode_ex(Inst* inst, uint32 ntype);
    CnGNode* addCnGNode_fl(Inst* inst, uint32 ntype);
    void printCnGNodes(char* text,::std::ostream& os);
    void printCnGNode(CnGNode* cgn,::std::ostream& os);
    std::string nodeTypeToString(CnGNode* cgn);
    std::string edgeTypeToString(CnGRef* edr);
    CnGNode* findCnGNode_id(uint32 nId);
    CnGNode* findCnGNode_op(uint32 nId);
    CnGNode* findCnGNode_in(uint32 nId);
    CnGNode* findCnGNode_mp(uint32 iId, uint32 aId);
    CnGNode* findCnGNode_fl(Inst* inst, uint32 ntype);
    void printCnGNodeRefs(CnGNode* cgn, std::string text,::std::ostream& os);
    void printRefInfo(::std::ostream& os); 
    void addInst(cfgNode* cfgn, Inst* inst);
    void scanCnGNodeRefsGE(CnGNode* cgn);
    void scanCnGNodeRefsEV(CnGNode* cgn);
    void scanCnGNodeRefsDA(CnGNode* cgn);
    void scanCnGNodeRefsAE(CnGNode* cgn);
    void scanCalleeMethod(Inst* call);
    void optimizeTranslatedCode(IRManager& irManager);
    void setCreatedObjectStates();
    void printCreatedObjectsInfo(::std::ostream& os);
    void printMethodInfos();        //?
    void printMethodInfo(CalledMethodInfo* mi);
    CalledMethodInfo* getMethodInfo(const char* ch1,const char* ch2,const char* ch3);
    CalledMethodInfo* findMethodInfo(MethodDesc* md,Inst* inst);
    void saveScannedMethodInfo();
    uint32 getMethodParamState(CalledMethodInfo* mi, uint32 np);
    void markNotEscInsts();
    bool checkScannedObjs(uint32 id);
    bool checkScannedInsts(uint32 id);
    bool checkScannedSucNodes(uint32 id);
    void createdObjectInfo();
    void addMonInst(Inst* inst);
    void addMonUnitVCall(MonUnit* mu, Inst* inst); 
    MonUnit* findMonUnit(uint32 opndId);
    void scanSyncInsts();
    void fixSyncMethodMonitorInsts(Insts* syncInsts);
    void checkCallSyncMethod();
    void insertSaveJitHelperCall(CnGNode* node);
    Opnd* insertReadJitHelperCall();
    bool checkMonitorsOnThis();
    void setSencEscState(CnGNode* node,Insts* syncInsts);
    void collectSuccessors(Node* node);
    void collectGlobalNodeSuccessors(CnGNode* node);
    uint32 getContainingObjState(Inst* linst);
    void removeMonitorInsts(Insts* syncInsts);
    void removeNode(Node* node);
    SsaTmpOpnd* insertLdConst(uint32 value);
    void fixMonitorInstsVCalls(MonUnit* mu);
    void insertFlagCheck(Insts* syncInsts, Opnd* muflag);
    void printNode(Node* n,::std::ostream& os);
    uint32 checkState(Inst* inst,uint32 st);
    void findObject(Inst* inst,std::string text="  ");
    void findObject1(Inst* inst,std::string text="  ");
    void lObjectHistory(Inst* inst,std::string text,::std::ostream& os);
    uint32 getSubobjectStates(CnGNode* node);

    uint32 getEscState(CnGNode* n) {
        return (n->state)&ESC_MASK;
    }
    void setEscState(CnGNode* n, uint32 st) {
        n->state = ((n->state)&BIT_MASK)+st;
    }
    uint32 getFullState(CnGNode* n) {
        return n->state;
    }
    void setFullState(CnGNode* n, uint32 st) {
        n->state = st;
    }
    uint32 getLoopCreated(CnGNode* n) {
        return (n->state)&LOOP_CREATED;
    }
    void setLoopCreated(CnGNode* n) {
        n->state = n->state|LOOP_CREATED;
    }
    void remLoopCreated(CnGNode* n) {
        n->state = (n->state|LOOP_CREATED)^LOOP_CREATED;
    }
    uint32 getCalleeEscaped(CnGNode* n) {
        return (n->state)&CALLEE_ESCAPED;
    }
    void setCalleeEscaped(CnGNode* n) {
        n->state = n->state|CALLEE_ESCAPED;
    }
    void remCalleeEscaped(CnGNode* n) {
        n->state = (n->state|CALLEE_ESCAPED)^CALLEE_ESCAPED;
    }
    uint32 getVirtualCall(CnGNode* n) {
        return (n->state)&VIRTUAL_CALL;
    }
    void setVirtualCall(CnGNode* n) {
        n->state = n->state|VIRTUAL_CALL;
    }
    void remVirtualCall(CnGNode* n) {
        n->state = (n->state|VIRTUAL_CALL)^VIRTUAL_CALL;
    }
    void printState(CnGNode* n,::std::ostream& os=Log::out()) {
        os << getEscState(n) << " (" << (getFullState(n)>>3) << ")";
    }
    void printState(uint32 st,::std::ostream& os=Log::out()) {
        os << (st&ESC_MASK) << " (" << (st>>3) << ")";
    }
    bool isGlobalState(uint32 state) {
        if ((state&ESC_MASK)==GLOBAL_ESCAPE||(state&VIRTUAL_CALL)!=0)
            return true;
        return false;
    }
    void runTranslatorSession(CompilationContext& inlineCC);

    int _cfgirun;
    int _instrInfo;
    int _instrInfo2;
    int _cngnodes;
    int _cngedges;
    int _scanMtds;
    int _setState;
    int _printstat;
    int _eainfo;
    int _seinfo;
#define  prsNum  10
    int prsArr[prsNum];
};

} //namespace Jitrino 

#endif // _ESCANALYSIS_H_
