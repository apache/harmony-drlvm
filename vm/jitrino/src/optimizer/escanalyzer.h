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
    static const uint32 NT_LDVAL    = 512;          // Op_TauLdInd, Op_TauStInd
	static const uint32 NT_OBJS     = NT_OBJECT|NT_RETVAL|NT_LDOBJ;  //for findCnGNode_op
// CnG node reference types
    static const uint32 NR_PRIM = 0;
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
    bool do_sync_removal;
    bool do_scalar_repl;
    bool do_esc_scalar_repl;
    bool do_scalar_repl_only_final_fields;
    bool scalarize_final_fields;
    const char* execCountMultiplier_string;
    double ec_mult;
    bool compressedReferencesArg; // for makeTauLdInd 

private:
    static const int maxMethodExamLevel_default = 0;

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
    ObjIds *scannedObjsRev;
    ObjIds *scannedInsts;
    ObjIds *scannedSucNodes;
    uint32 initNodeType;  // type of initial scanned node
    MonInstUnits* monitorInstUnits ;
    SsaTmpOpnd* i32_0;
    SsaTmpOpnd* i32_1;
    TranslatorAction* translatorAction;
    Insts* methodEndInsts;
    Insts* checkInsts;

    std::ostream& os_sc;
    bool print_scinfo;

    struct ScObjFld {
        VarOpnd* fldVarOpnd;
        Insts* ls_insts;
        FieldDesc* fd;
        bool isFinalFld;
    };

    typedef StlList<ScObjFld*> ScObjFlds; 

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
    void showFlags(std::ostream& os);
    void eaFixupVars(IRManager& irm);
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
    void scanCnGNodeRefsGE(CnGNode* cgn, bool check_var_src, bool check_field_elem=true);
//    void scanCnGNodeRefsEV(CnGNode* cgn);
//    void scanCnGNodeRefsDA(CnGNode* cgn);
    void scanCnGNodeRefsAE(CnGNode* cgn, bool check_var_src, bool check_field_elem=true);
    void scanCalleeMethod(Inst* call);
    void optimizeTranslatedCode(IRManager& irManager);
    void setCreatedObjectStates();
    void printCreatedObjectsInfo(::std::ostream& os);
//    void printLocalObjectsInfo(::std::ostream& os);  //nvg
    void printMethodInfos();        //?
    void printMethodInfo(CalledMethodInfo* mi);
    CalledMethodInfo* getMethodInfo(const char* ch1,const char* ch2,const char* ch3);
    CalledMethodInfo* findMethodInfo(MethodDesc* md,Inst* inst);
    void saveScannedMethodInfo();
    uint32 getMethodParamState(CalledMethodInfo* mi, uint32 np);
    void markNotEscInsts();

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

    bool checkScanned(ObjIds* ids, uint32 id) {
        ObjIds::iterator it;
        if (ids == NULL) {
            return false;
        }
        for (it = ids->begin( ); it != ids->end( ); it++ ) {
            if ((*it)==id) {
                return true;
            }
        }
        return false;
    }
    bool checkScannedObjs(uint32 id) {return checkScanned(scannedObjs, id);}
    bool checkScannedObjsRev(uint32 id) {return checkScanned(scannedObjsRev, id);}
    bool checkScannedInsts(uint32 id) {return checkScanned(scannedInsts, id);}
    bool checkScannedSucNodes(uint32 id) {return checkScanned(scannedSucNodes, id);}

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


    // Scalar replacement optimization
/**
 * Performs scalar replacement optimization for local objects 
 * (class instances and arrays).
 */
    void scanLocalObjects();

/**
 * Performs scalar replacement optimization for method escaped class instances.
 */
    void scanEscapedObjects();

/**
 * Performs scalar replacement optimization for local objects from the specified list.
 * @param loids - list of local objects CnG nodes Ids
 * @param os - output stream
 */
    void doLOScalarReplacement(ObjIds* loids);

/**
 * Performs scalar replacement optimization for method escaped objects from the specified list.
 * @param loids - list of local objects CnG nodes Ids
 */
    void doEOScalarReplacement(ObjIds* loids);

/**
 * Collects (using connection graph) information of onode object fields usage.
 * @param onode - connection graph node fields usage of which is collected
 * @param scObjFlds - list to collect onode fields usage
 */
    void collectStLdInsts(CnGNode* onode, ScObjFlds* scObjFlds);

/**
 * Collects (using connection graph) call instructions which use optimized object 
 * as a parameter.
 * @param n - optimized obgect connection graph node Id;
 * @param vc_insts - list of call instructions;
 * @param vcids - list of call instructions ids.
 */
    void collectCallInsts(uint32 n, Insts* vc_insts, ObjIds* vcids);

/**
 * Performs scalar replacement optimization for optimized object field usage.
 * @param scfld       - optimized object scalarizable field
 */
    void scalarizeOFldUsage(ScObjFld* scfld);

/**
 * Checks if an object from the specified list can be removed and its fields/elements scalarized.
 * If an object cannot be optimized it is removed from the list.
 * @param loids - list of local object CnG nodes Ids
 * @param check_loc - if <code>true</code> checks for local objects,
 *                    if <code>false</code> checks for virtual call escaped objects.
 */
    void checkOpndUsage(ObjIds* lnoids, ObjIds* lloids, bool check_loc);

/**
 * Checks if an object can be removed and its fields/elements scalarized.
 * @param lobjid - object CnG nodes Ids
 * @return <code>true</code> if an object is used only in ldflda or ldbase instructions; 
 *         <code>false<code> otherwise.
 */
    bool checkOpndUsage(uint32 lobjid);

/**
 * Performs checks for CnGNode operand using connection graph.
 * @param scnode - CnG node of optimized operand
 * @param check_loc - <true> * @param check_loc - if <code>true</code> checks for local objects,
 *                    if <code>false</code> checks for virtual call escaped objects.
 * @return CnGNode* for operand that may be optimized; 
 *         <code>NULL<code> otherwise.
 */
    CnGNode* checkCnG(CnGNode* scnode, bool check_loc);

/**
 * Checks if there is a path in CFG from node where object created by a nob_inst instruction
 * to EXIT node and this object is not escaped to any method call.
 * @param nob_inst - object creation instruction.
 * @return <code>execCount</code> of this path execution; 
 *         <code>0<code> otherwise.
 */
    double checkLocalPath(Inst* nob_inst);

/**
 * Checks if there is a path in CFG from node where object created by a nob_inst instruction
 * to EXIT node and this object is not escaped to any method call.
 * @param n - CFG node to scan
 * @param obId - escaped optimized object Id
 * @param cExecCount - current execCount
 * @return <code>execCount</code> the most value of <code>execCount</code> and 
 *                                checkNextNodes execution for next after n node; 
 */
    double checkNextNodes(Node* n, uint32 obId, double cExecCount, std::string text="");

/**
 * Checks flag and creates object before call instruction (if it was not created yet).
 * @param vc_insts    - list of call instructions optimized object is escaped to
 * @param objs        - list of optimized object fields
 * @param ob_var_opnd -  varOpnd replacing optimized object
 * @param ob_flag_var_opnd - sign if optimized object was created
 * @param tnode       - target CFG node for newobj instruction exception edge
 * @param oid         - escaped optimized object Id 
 */
    void restoreEOCreation(Insts* vc_insts, ScObjFlds* objs, VarOpnd* ob_var_opnd, 
        VarOpnd* ob_flag_var_opnd, Node* tnode, uint32 oid);

/**
 * Removes specified instruction from ControlFlowGraph.
 * If instruction can throw exception removes corresponding CFGEdge.
 * @param reminst - removed instruction
 */
    void removeInst(Inst* reminst);

/**
 * Returns MethodDesc* for Op_IndirectMemoryCall and Op_DirectCall instructions.
 * @param inst - call instruction.
 * @return MethodDesc for <code>Op_IndirectMemoryCall</code> and <code>Op_DirectCall</code>; 
 *         <code>NULL<code> otherwise.
 */
    MethodDesc* getMD(Inst* inst);

/**
 * Replaces first source operand of Op_MethodEnd instruction by NULL
 * for scalar replacement optimized object.
 * @param ob_id - optimized object Id
 */
    void fixMethodEndInsts(uint32 ob_id);

/**
 * Finds (using connection graph) load varOpnd that should be optimized with 
 * new object operand.
 * @param vval - CnG node of target stvar instruction varOpnd 
 * @return CnGNode* - found optimized load varOpnd CnG node
 *         <code>NULL</code> otherwise.
 */
    CnGNode* getLObj(CnGNode* vval);

/**
 * Checks that all sources of optimized load varOpnd aren't null and
 * satisfy to specified conditions.
 * @param inst - ldvar instruction created optimized load varOpnd.
 * @return <code>true</code> if satisfied; 
 *         <code>false<code> otherwise.
 */
    bool checkVVarSrcs(Inst* inst);

/**
 * Checks that optimized object type satisfied to specified types.
 * @param otn - object type name.
 * @return <code>true</code> if satisfied; 
 *         <code>false<code> otherwise.
 */
    bool checkObjectType(const char* otn);

/**
 * Checks that all load varOpnd fields are in new object field usage list.
 * @param nscObjFlds - list of used fields of optimized new object
 * @param lscObjFlds - list of used fields of optimized load varOpnd
 * @return <code>true</code> if list of new object used field contains all 
 *                           load varOpnd used field; 
 *         <code>false<code> otherwise.
 */
    bool checkObjFlds(ScObjFlds* nscObjFlds, ScObjFlds* lscObjFlds);

/**
 * Removes check instructions for optimized load varOpnd.
 * @param ob_id - optimized load variable operand Id
 */
    void fixCheckInsts(uint32 opId);

/**
 * Checks (using connection graph) if CnGNode operand has final fields and adds it to
 * the list of posible optimized final fields operands.
 * @param onode - CnG node of optimized operand
 * @param scObjFlds - list to collect onode operand field usage
 */
    void checkToScalarizeFinalFiels(CnGNode* onode, ScObjFlds* scObjFlds);


    // BCMap support
/**
 * Sets bcmap offset in bc2HIRMapHandler.
 * @param new_i - instruction to set offset
 * @param old_i - offset of old_i instruction is set to new_i instruction
 */
    void setNewBCMap(Inst* new_i, Inst* old_i);

/**
 * Removes bcmap offset in bc2HIRMapHandler.
 * @param inst - instruction to remove offset
 */
    void remBCMap(Inst* inst);


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
    int _scinfo;
#define  prsNum  10
    int prsArr[prsNum];

    CompilationInterface &compInterface;
    // Byte code map info
    bool isBCmapRequired;
    VectorHandler* bc2HIRMapHandler;
};

} //namespace Jitrino 

#endif // _ESCANALYSIS_H_
