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

#include "escanalyzer.h"
#include "FlowGraph.h"
#include "Inst.h"
#include "Stl.h"
#include "BitSet.h"
#include "Dominator.h"
#include "Loop.h"
#include "Log.h"
#include "Type.h"
#include "ssa/SSA.h"
#include "optpass.h"
#include "devirtualizer.h"
#include "DrlVMInterface.h"

namespace Jitrino {


DEFINE_SESSION_ACTION(EscapeAnalysisPass, escape, "Escape Analysis")

struct ComObjStat {
    ComObjStat()    :_n0(0), _n_ge(0), _n_ae(0), _n_ne(0), _n_lo(0) {};
    uint32 _n0;
    uint32 _n_ge;
    uint32 _n_ae;
    uint32 _n_ne;
    uint32 _n_lo;
};
static ComObjStat comObjStat;
EscAnalyzer::CalledMethodInfos* EscAnalyzer::calledMethodInfos=NULL;
Mutex EscAnalyzer::calledMethodInfosLock;


void
EscapeAnalysisPass::_run(IRManager& irm) {

    MemoryManager escMemManager(1024, "EscapeAnalyzer:tmp_mm");
    EscAnalyzer ea(escMemManager, this, irm);

    if (Log::isEnabled() && (ea.allProps!=0)) {
        Log::out() << "E s c a p e   A n a l y s i s " << std::endl;
    }

    //if (Log::isEnabled()) {
    //    const char* i1=Log::getDotFileDirName();
    //    if (strlen(i1)!=0) {
    //        ControlFlowGraph& flowGraph = irm.getFlowGraph();
    //        FlowGraph::printDotFile(flowGraph, irm.getMethodDesc(), "ea1");
    //    }
    //}
    ea.doAnalysis();
    //if (Log::isEnabled()) {
    //    const char* i1=Log::getDotFileDirName();
    //    if (strlen(i1)!=0) {
    //        ControlFlowGraph& flowGraph = irm.getFlowGraph();
    //        FlowGraph::printDotFile(flowGraph, irm.getMethodDesc(), "ea2"); 
    //    }
    //}
}  // run(IRManager& irm) 


EscAnalyzer::EscAnalyzer(MemoryManager& mm, SessionAction* argSource, IRManager& irm)
: eaMemManager(mm), irManager(irm), mh(irm.getMethodDesc()) 
{
    maxMethodExamLevel = (uint32)argSource->getIntArg("max_level",maxMethodExamLevel_default);
    allProps = (uint32)argSource->getIntArg("d_prop",0);
    debug_method = argSource->getStringArg("d_method", NULL);
    method_ea_level = 0;  // determines level of method scan
    
    const char* translatorName = argSource->getStringArg("translatorActionName", "translator");
    translatorAction = (TranslatorAction*)PMF::getAction(argSource->getPipeline(), translatorName);
    assert(translatorAction);

    init();
}

EscAnalyzer::EscAnalyzer(EscAnalyzer* parent, IRManager& irm) 
: eaMemManager(parent->eaMemManager), irManager(irm), mh(irm.getMethodDesc())
{
    maxMethodExamLevel = parent->maxMethodExamLevel;
    allProps = parent->allProps;
    debug_method = parent->debug_method;
    translatorAction  = parent->translatorAction;
    method_ea_level = parent->method_ea_level + 1;
    
    init();
}

void EscAnalyzer::init() {
    i32_0 = NULL;
    i32_1 = NULL;

    initNodeType = 0;     // type of initial scanned node

    scannedObjs = new (eaMemManager) ObjIds(eaMemManager);
    scannedInsts = new (eaMemManager) ObjIds(eaMemManager);
    scannedSucNodes = new (eaMemManager) ObjIds(eaMemManager);
    monitorInstUnits = new (eaMemManager) MonInstUnits(eaMemManager);    

    _cfgirun=0;
    _instrInfo=0;
    _instrInfo2=0;
    _cngnodes=0;
    _cngedges=0;
    _scanMtds=0;
    _setState=0;
    _printstat=0;
    _eainfo=0;
    _seinfo=0;
    std::fill(prsArr, prsArr + prsNum, 0);


    int pr = allProps;
    int prsBound[] = {1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1};
    if (pr-prsBound[0]-prsBound[0]<0 && pr!= 0) {
        for (int i=0; i<prsNum; i++) {
            if (pr >= prsBound[i] && pr-prsBound[i] >= 0) {
                prsArr[i] = 1;
                pr -= prsBound[i];
            }
        }

        _eainfo     = prsArr[8];
        _seinfo     = prsArr[9];
        if (debug_method==NULL || strcmp(debug_method,"all")==0) {
            _cfgirun    = prsArr[0];
            _instrInfo  = prsArr[1];
            _instrInfo2 = prsArr[2];
            _cngnodes   = prsArr[3];
            _cngedges   = prsArr[4];
            _scanMtds   = prsArr[5];
            _setState   = prsArr[6];
            _printstat  = prsArr[7];
        }
    }
}

void
EscAnalyzer::doAnalysis() {
    const char* mn = mh.getName();
    const Nodes& nodes = irManager.getFlowGraph().getNodes();
    ControlFlowGraph& fg = irManager.getFlowGraph();
    uint32 nodeNum = (uint32)nodes.size();
    uint32 num2 = fg.getNodeCount();
    uint32 num3 = fg.getMaxNodeId();
    Nodes::const_iterator niter;

    lastCnGNodeId = 0;    // initialization of private field
    defArgNumber = -1;   // initialization of private field

    if (Log::isEnabled() && debug_method!=NULL) {
        if (strcmp(mn,debug_method)==0) {
            _cfgirun    = prsArr[0];
            _instrInfo  = prsArr[1];
            _instrInfo2 = prsArr[2];
            _cngnodes   = prsArr[3];
            _cngedges   = prsArr[4];
            _scanMtds   = prsArr[5];
            _setState   = prsArr[6];
            _printstat  = prsArr[7];
            Log::out() << "_cfgirun     " << _cfgirun    << std::endl;
            Log::out() << "_instrInfo   " << _instrInfo  << std::endl;
            Log::out() << "_instrInfo2  " << _instrInfo2 << std::endl;
            Log::out() << "_cngnodes    " << _cngnodes   << std::endl;
            Log::out() << "_cngedges    " << _cngedges   << std::endl;
            Log::out() << "_scanMtds    " << _scanMtds   << std::endl;
            Log::out() << "_setState    " << _setState   << std::endl;
            Log::out() << "_printstat   " << _printstat  << std::endl;
            Log::out() << "_eainfo      " << _eainfo     << std::endl;
            Log::out() << "_seinfo      " << _seinfo     << std::endl;
       }
    }
    if (_eainfo || _seinfo) {
        Log::out()<<"======  doAnalysis  ====== "<<mn<<"   level: ";
        Log::out()<<method_ea_level<<"   "; 
        if (mh.isSynchronized())
            Log::out()<<"sync   "; 
        if (mh.isStatic())
            Log::out()<<"stat   "; 
        Log::out()<<nodeNum<<" "<<num2<<" "<<num3<< "   ";
        if (Log::isEnabled())
            mh.printFullName(Log::out()); 
        Log::out()<< std::endl;
    }
    cfgNode* pr_n = new (eaMemManager)cfgNode[nodeNum];           // passed nodes
    int maxInd = 0;
    int cur = -1;
    Node* node;
    cngNodes=new (eaMemManager) CnGNodes(eaMemManager);   // Common part of connection graph (nodes)

    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        node = *niter;
        cur = maxInd++;
        (pr_n[cur]).nodeId = node->getId(); 
        (pr_n[cur]).fgNode = node;
        (pr_n[cur]).instructions = NULL;

//  exam instructions

#ifdef _DEBUG
        if (_cfgirun) {
            Log::out() <<std::endl;
            Log::out() <<"pr_n["<<cur<<"]  nId "<<(pr_n[cur]).nodeId;
            Log::out() <<"  Node "<<(pr_n[cur]).fgNode->getId();
            Log::out() <<std::endl;
        }
#endif

        instrExam(&pr_n[cur]);

    };

#ifdef _DEBUG
    if (_cngnodes) {
        Log::out() <<"printCnGNodes: "; 
        if (Log::isEnabled()) 
            mh.printFullName(Log::out()); 
        Log::out() << std::endl;
        if (Log::isEnabled()) 
            printCnGNodes("First run result: nodes",Log::out());
    }
#endif

    cngEdges=new (eaMemManager) CnGEdges(eaMemManager);   // Common part of connection graph (edges)
    for (int i=0; i<maxInd; i++) {
        instrExam2(&pr_n[i]);
    }

#ifdef _DEBUG
    if (_cngedges) {
        Log::out() <<"printCnGEdges: "; 
        if (Log::isEnabled()) 
            mh.printFullName(Log::out()); 
        Log::out() << std::endl;
        if (Log::isEnabled()) 
            printCnGEdges("resulting OUT CnGEdges",Log::out());
    }
#endif

    
    setCreatedObjectStates();

#ifdef _DEBUG
    if (_eainfo) {
        if (Log::isEnabled()) 
            printCreatedObjectsInfo(Log::out());
    }
#endif

    saveScannedMethodInfo();    //??? to save states of contained obj, if needed

#ifdef _DEBUG
    if (_cngedges) {
        Log::out() << "++++++++++++++ printRefInfo()    ";
        if (Log::isEnabled())  
            mh.printFullName(Log::out());
        Log::out() << std::endl;
        if (Log::isEnabled()) 
            printRefInfo(Log::out());
        Log::out() << "++++++++++++++ end               "; 
        if (Log::isEnabled()) 
            mh.printFullName(Log::out());
        Log::out() << std::endl;
    }
#endif

    if (method_ea_level == 0) {   // mark inst that may be optimized
#ifdef _DEBUG
        if (_cngedges)
            Log::out() <<"+++++++++++++++++ markNotEscInsts()"<< std::endl;
#endif
        markNotEscInsts();    
    }

#ifdef _DEBUG
    if (_cngnodes) {
        Log::out() <<"printCnGNodes: "; 
        if (Log::isEnabled()) 
            mh.printFullName(Log::out()); 
        Log::out() << std::endl;
        if (Log::isEnabled()) 
            printCnGNodes("Marked nodes",Log::out());
    }
#endif

    if (method_ea_level == 0) {  
        createdObjectInfo();     // prints if _printstat==1
    }

    if (method_ea_level == 0) {
        scanSyncInsts();

#ifdef _DEBUG
        if (_seinfo && Log::isEnabled()) {
            printCreatedObjectsInfo(Log::out());
            Log::out() << "================ this agrument of sync method" 
                    << std::endl;
            CnGNodes::iterator it;
            for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
                CnGNode* n = (*it);
                if (n->nodeType==NT_ACTARG && n->argNumber==0 && 
                        ((MethodDesc*)(n->refObj))->isSynchronized()) {
                    ((MethodDesc*)(n->refObj))->printFullName(Log::out());
                    Log::out()<<std::endl;
                    printCnGNodeRefs(n, "  ", Log::out());
                }
            }

            scannedObjs->clear();
            Log::out() << "================ Return Values 2" << std::endl;
            for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
                if ((*it)->nodeType==NT_EXITVAL) {
                    printCnGNodeRefs(*it, "", Log::out());
                }
            }
            scannedObjs->clear();

        }
#endif
    }

    if (_eainfo || _seinfo) {
        Log::out()<<"======  doAnalysis  ====== "<<mn<<"   level: ";
        Log::out()<<method_ea_level<<"   end   "; 
        if (mh.isSynchronized())
            Log::out()<<"sync   "; 
        if (mh.isStatic())
            Log::out()<<"stat   "; 
        if (Log::isEnabled())
            mh.printFullName(Log::out()); 
        Log::out()<< std::endl;
    }

    if (Log::isEnabled() && debug_method!=NULL) {
        if (strcmp(mn,debug_method)==0) {
            _cfgirun    = 0;
            _instrInfo  = 0;
            _instrInfo2 = 0;
            _cngnodes   = 0;
            _cngedges   = 0;
            _scanMtds   = 0;
            _setState   = 0;
            _printstat  = 0;
        }
    }

    return;
}  // doAnalysis() 


void
EscAnalyzer::scanSyncInsts() {
    MonInstUnits::iterator it;
    Insts::iterator it1;
    Insts* syncInsts;
    CnGNode* node;
    uint32 checkedState = 0;
    uint32 nState = 0;
    bool to_fix_ssa = false;

    if (monitorInstUnits == NULL)
        return;

#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "Synchronized units: " << std::endl;
    }
#endif
    for (it = monitorInstUnits->begin( ); it != monitorInstUnits->end( ); it++ ) {
        node = findCnGNode_op((*it)->opndId);
        checkedState = 0;
        nState = getEscState(node);
        if (node->nodeType == NT_OBJECT || nState == GLOBAL_ESCAPE)
            checkedState = nState;
        if (nState != GLOBAL_ESCAPE && (node->nodeType != NT_OBJECT)) {
            checkedState = checkState(node->nInst,nState);
            scannedObjs->clear();
            if (checkedState != nState && checkedState != 0) {
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--scanSyncInsts 1: node "
                            <<node->cngNodeId<<" opndId "<<node->opndId
                            <<" state ";
                    if (Log::isEnabled()) 
                        printState(node); 
                    Log::out()<<" to esc.state "<< checkedState << std::endl;
                }
#endif
                setEscState(node,checkedState);
            }
        }
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() <<"--sync: state "; 
            Log::out() << checkedState << "  "; 
            if (checkedState != nState)
                Log::out() << "initState " << nState << "  "; 
            Log::out() << node->cngNodeId << " - " << node->opndId << "  "; 
            if (Log::isEnabled()) 
                printCnGNode(node,Log::out()); 
            Log::out() << std::endl;
        }
#endif
        if ((node->nodeType&NT_OBJECT)==0 && (getEscState(node) != GLOBAL_ESCAPE)) {
            findObject(node->nInst);  
            scannedObjs->clear();
        }
        syncInsts = (*it)->monInsts;
#ifdef _DEBUG
        if (_seinfo) {
            for (it1 = syncInsts->begin( ); it1 != syncInsts->end( ); it1++ ) {
                Log::out() << "   -"; 
                if (Log::isEnabled()) 
                    (*it1)->print(Log::out());
                Log::out() << "  // node " 
                    << (*it1)->getNode()->getId() << std::endl;
            }
        }
#endif
        if (getEscState(node) == GLOBAL_ESCAPE)
            setSencEscState(node,syncInsts);
        if (getVirtualCall(node)!=0) {
            if (checkedState > GLOBAL_ESCAPE) {
#ifdef _DEBUG
               if (_seinfo) {
                    Log::out() << "=-=- vc loc.esc."  << std::endl;
                    findObject(node->nInst);
                    scannedObjs->clear();
                }
#endif
                if (node->nodeType==NT_OBJECT) {
#ifdef _DEBUG
                    if (_seinfo) {
                        Log::out() << "=-=- vc to optimize object"  << std::endl;
                    }
#endif
                    fixMonitorInstsVCalls(*it);
                }
                if (node->nodeType==NT_RETVAL) {
#ifdef _DEBUG
                    if (_seinfo) {
                        Log::out() << "=-=- vc to optimize retval"  << std::endl;
                    }
#endif
                    fixMonitorInstsVCalls(*it);
                }
                if (!to_fix_ssa)
                    to_fix_ssa = true;
            } else {
#ifdef _DEBUG
               if (_seinfo) {
                    Log::out() << "=-=- vc gl.esc."  << std::endl;
                    findObject(node->nInst);
                    scannedObjs->clear();
                }
#endif
            }
        } else {
            if (node->nodeType==NT_OBJECT && getEscState(node) != GLOBAL_ESCAPE) {
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "++++ to optimize (remove) object"  << std::endl;
                }
#endif
                removeMonitorInsts(syncInsts);
            }

            if (node->nodeType==NT_DEFARG && getEscState(node) != GLOBAL_ESCAPE
                    && node->nInst->getDefArgModifier()==NonNullThisArg && mh.isSynchronized()) {
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "++++ to optimize (fix) defarg.ths"  << std::endl;
                }
#endif
#ifndef PLATFORM_POSIX
                fixSyncMethodMonitorInsts(syncInsts);
#endif
            }
//        if (opndInst->getOpcode()==Op_DefArg && opndInst->getDefArgModifier()==NonNullThisArg) {

        }
    }
#ifndef PLATFORM_POSIX
    checkCallSyncMethod();
#endif
    // to fix var operand inserted by fixMonitorInstsVCalls method
    if (to_fix_ssa) { 
        OptPass::fixupSsa(irManager);
    }
    return;
}  // scanSyncInsts() 


void
EscAnalyzer::fixSyncMethodMonitorInsts(Insts* syncInsts) {
    SsaTmpOpnd* stThis = (SsaTmpOpnd*)insertReadJitHelperCall();
    insertFlagCheck(syncInsts,stThis);
}  // fixSyncMethodMonitorInsts(Insts* syncInsts) 


void
EscAnalyzer::checkCallSyncMethod() {
    CnGNodes::iterator it;
    CnGRefs::iterator it2;
    CnGNode* node;
    CnGNode* aanode;
    MethodDesc* md;
    CalledMethodInfo* mtdInfo;
    uint32 callee_state;

    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        node = (*it);
        if (node->nodeType == NT_ACTARG && node->argNumber == 0) {
            md = (MethodDesc*)(node->refObj);
            if (md->isSynchronized() && node->nInst->getOpcode()==Op_DirectCall) {
                const char* ch1 = md->getParentType()->getName(); 
                const char* ch2 = md->getName(); 
                const char* ch3 = md->getSignatureString();
                mtdInfo = getMethodInfo(ch1,ch2,ch3);
                if (mtdInfo==NULL) {
                    callee_state = 0;
#ifdef _DEBUG
                    if (_seinfo) {
                        Log::out() << "=-   Methodinfo is NULL"; 
                        Log::out() << std::endl;
                    }
#endif
                } else {
                    callee_state = getMethodParamState(mtdInfo,0);
                }
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "---- checkCallSyncMethod:"  << std::endl;
                    if (Log::isEnabled()) 
                        node->nInst->print(Log::out()); 
                    Log::out() << std::endl;
                    if (Log::isEnabled()) 
                        printCnGNode(node,Log::out()); 
                    Log::out() << std::endl;
                    if (node->outEdges != NULL) {
                        for (it2 = node->outEdges->begin(); it2 != node->outEdges->end(); it2++ ) {
                            Log::out() << "  ccsm: ";
                            if (Log::isEnabled())  
                                printState(callee_state);
                            Log::out() << " ";
                            if (Log::isEnabled()) 
                                printCnGNode((*it2)->cngNodeTo,Log::out());
                            Log::out() << std::endl;
                        }
                    } 
                    Log::out() << "++++ checkCallSyncMethod: ";
                    Log::out() << " instance: " << md->isInstance() <<
                        " initializer: " << md->isInstanceInitializer() << " end" << std::endl;
                }
#endif
                if (callee_state == 0)
                    callee_state = GLOBAL_ESCAPE;
                if (!isGlobalState(callee_state)) {
                    if (node->outEdges != NULL) {
                        for (it2 = node->outEdges->begin(); it2 != node->outEdges->end(); it2++ ) {
                            aanode = (*it2)->cngNodeTo;
                            if (!isGlobalState(aanode->state)&&
                                    (aanode->nodeType==NT_OBJECT||aanode->nodeType==NT_RETVAL)) {
#ifdef _DEBUG
                                if (_seinfo) {
                                    Log::out() 
                                        << "=-=- sm this.agr.saving"  << std::endl;
                                }
#endif
                                insertSaveJitHelperCall(node);
#ifdef _DEBUG
                                if (_seinfo) {
                                    Log::out() 
                                        << "  checkCSM: this was saved"   
                                        << std::endl;
                                }
                            } else {
                                if (_seinfo) {
                                    Log::out() 
                                        << "  checkCSM: this wasn't saved"   
                                        << std::endl;
                                }
#endif
                            }
                        }
                    }
                }
            }
        }
    }
}  // checkCallSyncMethod() 


/*
*  Inserts ldc0, if it was not inserted previously,
*          jit helper call to save this arg of direct call method.
*  Parameter:
*      CnGNode corresponding to this arg of direct call sync method.
*/
void 
EscAnalyzer::insertSaveJitHelperCall(CnGNode* node) {
    SsaTmpOpnd* stVal;
    Inst* inst_before = node->nInst;
    Node* oldBlock = inst_before->getNode();
    ControlFlowGraph& fg = irManager.getFlowGraph();

#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "++++ insertSJHC: before"  << std::endl;
        if (Log::isEnabled()) {
            printNode(fg.getEntryNode(),Log::out());
            printNode(oldBlock,Log::out());
        }
        Log::out() << "++++ insertSJHC: before end"  << std::endl;
    }
#endif
    stVal = insertLdConst(0);
        // insert jit helper call
    Opnd* args[1] = {stVal};
    InstFactory& instfactory = irManager.getInstFactory();
    Inst* jhcinst = instfactory.makeJitHelperCall(
            OpndManager::getNullOpnd(), SaveThisState, 1, args);
       // add jit helper
    jhcinst->insertBefore(inst_before);
    Node* newBlock = fg.splitNodeAtInstruction(jhcinst, true, false, instfactory.makeLabel());
        // add dispatch edge to oldBlock
    fg.addEdge(oldBlock,newBlock->getExceptionEdgeTarget());
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "++++ insertSJHC: after"  << std::endl;
        if (Log::isEnabled()) {
            printNode(fg.getEntryNode(),Log::out());
            printNode(oldBlock,Log::out());
            printNode(newBlock,Log::out());
        }
        Log::out() << "++++ insertSJHC: after end"  << std::endl;
    }
#endif
}  // insertSaveJitHelperCall(CnGNode* node) 


/*
*  Inserts ldc0, if it was not inserted previously,
*          jit helper call to read this arg of the method.
*  Parameter:
*      CnGNode corresponding to this arg of direct call sync method.
*  Returns:
*      Opnd* stored_value
*          0 - this argument is thread local, 1 - this argument is thread global
*/
Opnd* 
EscAnalyzer::insertReadJitHelperCall() {
    ControlFlowGraph& fg = irManager.getFlowGraph();
    Node* oldBlock = fg.getEntryNode();
    Inst* inst_after = (Inst*)oldBlock->getFirstInst();
    TypeManager& _typeManager = irManager.getTypeManager();
    Type* typeInt32 = _typeManager.getInt32Type();
    OpndManager& _opndManager = irManager.getOpndManager();
    SsaTmpOpnd* stThis = _opndManager.createSsaTmpOpnd(typeInt32);

#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "++++ insertRJHC: before"  << std::endl;
        if (Log::isEnabled()) 
            printNode(oldBlock,Log::out());
        Log::out() << "++++ insertRJHC: before end"  << std::endl;
    }
#endif
    insertLdConst(0);
        // insert jit helper call
    Opnd** args = NULL;
    InstFactory& instfactory = irManager.getInstFactory();
    Inst* jhcinst = instfactory.makeJitHelperCall(
            stThis, ReadThisState, 0, args);
    jhcinst->insertAfter(inst_after);
    fg.splitNodeAtInstruction(jhcinst,true, false,instfactory.makeLabel());
    fg.addEdge(oldBlock,fg.getUnwindNode());
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "++++ insertRJHC: after"  << std::endl;
        if (Log::isEnabled()) {
            printNode(oldBlock,Log::out());
        }
        Log::out() << "++++ insertRJHC: after end"  << std::endl;
    }
#endif
    return stThis;
}  // insertReadJitHelperCall() 


/*
*  Checks, that method contains monitor instructions with parameter 
*  which is this or subobject of this.
*    Returns: true, if such monitors exist, false overwise.
*/
bool
EscAnalyzer::checkMonitorsOnThis() {
    MonInstUnits::iterator it;
    Insts::iterator it1;
    CnGNode* node;

    if (monitorInstUnits==NULL)
        return false;
    for (it = monitorInstUnits->begin( ); it != monitorInstUnits->end( ); it++ ) { //540
        node = findCnGNode_op((*it)->opndId);

        Inst* opndInst = node->nInst;
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() << "    checkMOT: ";
            if (Log::isEnabled())  
                opndInst->print(Log::out()); 
            Log::out()  << std::endl;
        }
#endif
        if (opndInst->getOpcode()==Op_DefArg && opndInst->getDefArgModifier()==NonNullThisArg) {
#ifdef _DEBUG
            if (_seinfo) {
                Log::out() << "    checkMOT: "; 
                Log::out() << (int)(opndInst->getDefArgModifier()) << " " <<
                (opndInst->getDefArgModifier()==DefArgNoModifier) << " " <<
                (opndInst->getDefArgModifier()==NonNullThisArg) << " " <<
                (opndInst->getDefArgModifier()==DefArgBothModifiers) << "  state: ";
                if (Log::isEnabled()) 
                    printState(node,Log::out());
                Log::out() << std::endl;
                if (getEscState(node) != GLOBAL_ESCAPE) {
                    Log::out() << "    defarg.ths isn't global "<< std::endl; 
                }
            }
#endif
            return true;
        }
    }
    return false;
} // checkMonitorsOnThis()


void
EscAnalyzer::setSencEscState(CnGNode* node,Insts* syncInsts) {
    CnGEdges::iterator it;
    Insts::iterator it1;
    CnGRefs::iterator it2;
    Inst* einst;
    bool pLocalObj = true;
    bool pLocalCurObj = false;
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=- to node " << node->cngNodeId 
            << std::endl; 
    }
#endif
    scannedSucNodes->clear();      // prepared for successor collection
    for (it = cngEdges->begin( ); it != cngEdges->end( ); it++ ) {
        for (it2 = (*it)->refList->begin( ); it2 != (*it)->refList->end( ); it2++ ) {
            if ((*it2)->cngNodeTo == node) {
                pLocalCurObj = false;
                einst = (*it2)->edgeInst;
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "=- "; 
                    Log::out() << einst->getNode()->getId()
                        << " ";
                    if (Log::isEnabled()) 
                        einst->print(Log::out());
                    Log::out() << std::endl;
                }
#endif
                if (einst->getOpcode() == Op_DirectCall) {
                    MethodDesc* md=einst->asMethodInst()->getMethodDesc();
                    const char* ch1 = md->getParentType()->getName(); 
                    const char* ch2 = md->getName(); 
                    const char* ch3 = md->getSignatureString();
                    CalledMethodInfo* mtdInfo = getMethodInfo(ch1,ch2,ch3);
                    if (mtdInfo==NULL) {
#ifdef _DEBUG
                        if (_seinfo) {
                            Log::out() << "=-   Methodinfo is NULL"; 
                            Log::out() << std::endl;
                        }
#endif
                    } else {
                        uint32 st = getMethodParamState(mtdInfo,(*it)->cngNodeFrom->argNumber);
#ifdef _DEBUG
                        if (_seinfo) {
                            Log::out() << "=-=- Method param "
                                << (*it)->cngNodeFrom->argNumber << " state is ";
                            if (Log::isEnabled()) 
                                printState(st);
                            Log::out() << std::endl;
                        }
#endif
                        if ((st&ESC_MASK)>GLOBAL_ESCAPE) {
                            pLocalCurObj = true;
#ifdef _DEBUG
                            if (_seinfo) {
                                Log::out() << "=-   Method param "
                                    << (*it)->cngNodeFrom->argNumber << " state is ";
                                if (Log::isEnabled()) 
                                    printState(st);
                                Log::out() << std::endl;
                            }
#endif
                        } else {
#ifdef _DEBUG
                            if (_seinfo) {
                                if (Log::isEnabled()) 
                                    printMethodInfo(mtdInfo);
                            }
#endif
                         }
                     }
                }
                findObject1(einst);
                collectSuccessors(einst->getNode());
            }
        }
        for (it1 = syncInsts->begin(); it1 != syncInsts->end( ); it1++ ) {
            if (scannedSucNodes->size()!=0) {
                if (checkScannedSucNodes((*it1)->getNode()->getId())!=0) {
#ifdef _DEBUG
                    if (_seinfo) {
                        if (pLocalCurObj)
                            Log::out() << "     "; 
                        Log::out() << "=-     contains " 
                            << (*it1)->getNode()->getId()
                            << std::endl;
                    }
#endif
                    if (!pLocalCurObj)
                        pLocalObj = false;
                }
            }
        }
        scannedSucNodes->clear();
    }
    if (pLocalObj) {
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() << "=-     may be optimized" 
                << std::endl;
        }
#endif
    }
}  // setSencEscState(CnGNode* node,Insts* syncInsts)


/*
*  Collect all reachable from specified node nodes in FlowGraph.
*  scannedSucNodes - result of collection.
*/
void 
EscAnalyzer::collectSuccessors(Node* node) {
    Node* n;
    Edges::const_iterator eit;
    const Edges& out_edges = node->getOutEdges();
    if (scannedSucNodes->size()!=0) {
        if (checkScannedSucNodes(node->getId())!=0) {
            return;
        }
    }
    scannedSucNodes->push_back(node->getId());
    for (eit = out_edges.begin(); eit != out_edges.end(); ++eit) {
        n = (*eit)->getTargetNode();
        collectSuccessors(n);
    }
}  // collectSuccessors(Node* node) 


void
EscAnalyzer::collectGlobalNodeSuccessors(CnGNode* node) {
    CnGEdges::iterator it;
     CnGRefs::iterator it2;
    Inst* einst;
    bool pGlobalObj = false;
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=- to node " << node->cngNodeId 
            << std::endl; 
    }
#endif
    scannedSucNodes->clear();      // prepared for successor collection
    for (it = cngEdges->begin( ); it != cngEdges->end( ); it++ ) {
        for (it2 = (*it)->refList->begin( ); it2 != (*it)->refList->end( ); it2++ ) {
            if ((*it2)->cngNodeTo == node) {
                pGlobalObj = false;
                einst = (*it2)->edgeInst;
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "=-ggns "; 
                    Log::out() << einst->getNode()->getId()
                        << " ";
                    if (Log::isEnabled()) 
                        einst->print(Log::out());
                    Log::out() << std::endl;
                }
#endif
                if (einst->getOpcode() == Op_DirectCall) {
                    MethodDesc* md=einst->asMethodInst()->getMethodDesc();
                    const char* ch1 = md->getParentType()->getName(); 
                    const char* ch2 = md->getName(); 
                    const char* ch3 = md->getSignatureString();
                    CalledMethodInfo* mtdInfo = getMethodInfo(ch1,ch2,ch3);
                    if (mtdInfo==NULL) {
                        pGlobalObj = true;
#ifdef _DEBUG
                        if (_seinfo) {
                            Log::out() << "ggns=-   Methodinfo is NULL"; 
                            Log::out() << std::endl;
                        }
#endif
                    } else {
                        uint32 st = getMethodParamState(mtdInfo,(*it)->cngNodeFrom->argNumber);
#ifdef _DEBUG
                        if (_seinfo) {
                            Log::out() << "ggns=-=- Method param "
                                << (*it)->cngNodeFrom->argNumber << " state is ";
                            if (Log::isEnabled()) 
                                printState(st);
                            Log::out() << std::endl;
                        }
#endif
                        if ((st&ESC_MASK)<=GLOBAL_ESCAPE) {
                            pGlobalObj = true;
#ifdef _DEBUG
                            if (_seinfo) {
                                Log::out() << "ggns=-   Method param "
                                    << (*it)->cngNodeFrom->argNumber << " state is ";
                                if (Log::isEnabled()) 
                                    printState(st);
                                Log::out() << std::endl;
                            }
#endif
                        } 
                     }
                }
                if (einst->getOpcode() == Op_IndirectMemoryCall) 
                    pGlobalObj = true;
                if (einst->getOpcode() == Op_TauStInd) {
                    Inst* i1 = einst->getSrc(0)->getInst();
                    if (i1->getOpcode()==Op_LdStaticAddr)
                        pGlobalObj = true;
                    uint32 st = getContainingObjState(i1);
                    if ((st&ESC_MASK)<=GLOBAL_ESCAPE) {
                        pGlobalObj = true;
                    }
                }
                findObject1(einst);
                if (pGlobalObj)
                    collectSuccessors(einst->getNode());
            }
        }
    }
}  // collectGlobalNodeSuccessors(CnGNode* node)


uint32 
EscAnalyzer::getContainingObjState(Inst* linst) {
    Opnd* cop = NULL;
    if (linst->getOpcode()==Op_LdStaticAddr)
        return GLOBAL_ESCAPE;
    if (linst->getOpcode()==Op_LdFieldAddr || linst->getOpcode()==Op_LdArrayBaseAddr)
        cop = linst->getSrc(0);
    if (linst->getOpcode()==Op_AddScaledIndex)
        cop = linst->getSrc(0)->getInst()->getSrc(0);
    else 
        return GLOBAL_ESCAPE;
    CnGNode* n = findCnGNode_op(cop->getId());
    return n->state;
}  // getContainingObjState(Inst* linst)


void
EscAnalyzer::removeMonitorInsts(Insts* syncInsts) {
    ControlFlowGraph& fg = irManager.getFlowGraph();
    Insts::iterator it1;
    Edge* excedge = NULL;
    Node* targetnode = NULL;
    Inst* reminst;
#ifdef _DEBUG
    bool canthrow = false;
#endif

    for (it1 = syncInsts->begin( ); it1 != syncInsts->end( ); it1++ ) {
        reminst = *it1;
        if (reminst->getOperation().canThrow()==true) {
            excedge = (Edge*)reminst->getNode()->getExceptionEdge();
            if (excedge != NULL)
                targetnode = excedge->getTargetNode();
            else 
                targetnode = NULL;
        } else {
            excedge = NULL;
            targetnode = NULL;
        }
#ifdef _DEBUG
        if (_seinfo) {
            canthrow = reminst->getOperation().canThrow();
            Log::out() << "  ";
            if (Log::isEnabled()) 
                reminst->print(Log::out());
            Log::out() << std::endl;
            Log::out() << "    canThrow "<< canthrow;
            Log::out() << std::endl;
            if (excedge==NULL) {
                Log::out() << "    exception edge is NULL " << std::endl;
            } else {
                Log::out() << "    target node is " 
                    << targetnode->getId() << std::endl;
            }
            if (canthrow && (excedge==NULL)) {
                const Edges& out_edges = reminst->getNode()->getOutEdges();
                Edges::const_iterator eit;
                Node* n; 
                for (eit = out_edges.begin(); eit != out_edges.end(); ++eit) {
                    n = (*eit)->getTargetNode();
                    Log::out() << "    edge to node " 
                        << n->getId() << " kind " << (*eit)->getKind() << std::endl;
                }
            }
        }
#endif

        reminst->unlink();
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() << "    unlinked: "; 
            if (Log::isEnabled()) 
                reminst->print(Log::out());
            Log::out() << std::endl;
        }
#endif
        if (targetnode != NULL) {
            if (targetnode->getInEdges().size() > 1) {
                fg.removeEdge(excedge);
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "    removed edge: " 
                        << excedge->getSourceNode()->getId() << " -> "
                        << excedge->getTargetNode()->getId() << " kind " << excedge->getKind(); 
                    Log::out() << std::endl;
                }
#endif
            } else {
                scannedObjs->clear();
                removeNode(targetnode);
                scannedObjs->clear();
            }
        }

    }

} // removeMonitorInsts(Insts* syncInsts)


void 
EscAnalyzer::removeNode(Node* node) {
    const Edges& out_edges = node->getOutEdges();
    Edges::const_iterator eit;
    Node* n; 

#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "    to remove node " 
            << node->getId() << std::endl;
   }
#endif
    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(node->getId())) {
            return;
        }
    }
    scannedObjs->push_back(node->getId());
    Nodes nodes2delete(irManager.getMemoryManager());
    for (eit = out_edges.begin(); eit != out_edges.end(); ++eit) {
        n = (*eit)->getTargetNode();
        if (n->getInEdges().size() == 1) {
            nodes2delete.push_back(n);
        }
    }
    Nodes::iterator iter = nodes2delete.begin(), end = nodes2delete.end();
    for (; iter != end; ++iter) {
        n = (*iter);
        removeNode(n);
    }
    irManager.getFlowGraph().removeNode(node);
    scannedObjs->pop_back();
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "    removed node " 
            << node->getId() << std::endl;
    }
#endif
} // removeNode(Node* node) 


/*
*  Creates i32_0 or i32_1 SsaTmpOpnd (in accordance with value: 0 or 1)
*  if it was'n created before.
*  Inserts ldc0 or ldc1 instruction after first instruction in entry Node, 
*  if SsaTmpOpnd was created.
*  Returns: 
*      i32_0, if value = 0
*      i32_1, if value = 1
*/
SsaTmpOpnd* 
EscAnalyzer::insertLdConst(uint32 value) {
    TypeManager& _typeManager  = irManager.getTypeManager();
    Type* typeInt32 = _typeManager.getInt32Type();
    OpndManager& _opndManager = irManager.getOpndManager();
    InstFactory& _instFactory = irManager.getInstFactory();
    Inst* ildc = NULL;
    if (value == 0)
        if (i32_0 == NULL) {
            i32_0 = _opndManager.createSsaTmpOpnd(typeInt32);
            ildc = _instFactory.makeLdConst(i32_0, 0);
        }
    if (value == 1)
        if (i32_1 == NULL) {
            i32_1 = _opndManager.createSsaTmpOpnd(typeInt32);
            ildc = _instFactory.makeLdConst(i32_1, 1);
        }
    if (ildc != NULL) {
        ildc->insertAfter(irManager.getFlowGraph().getEntryNode()->getFirstInst());
    }
    if (value == 0)
        return i32_0;
    return i32_1;
} // insertLdConst(uint32 value) 


void 
EscAnalyzer::fixMonitorInstsVCalls(MonUnit* mu) {
    Inst* opi = findCnGNode_op(mu->opndId)->nInst;
    OpndManager& _opndManager = irManager.getOpndManager();
    InstFactory& _instFactory = irManager.getInstFactory();
    TypeManager& _typeManager  = irManager.getTypeManager();
    Type* typeInt32 = _typeManager.getInt32Type();
    VarOpnd* muflag = _opndManager.createVarOpnd(typeInt32, false);
    Inst* stvar0; // = _instFactory.makeStVar(muflag, i32_0);
    Inst* stvar1;
    Insts::iterator inst_it;
    Insts* vcInsts = mu->icallInsts;;
    Insts* syncInsts = mu->monInsts;
#ifdef _DEBUG
    ControlFlowGraph& fg = irManager.getFlowGraph();
    Node* entry_node = fg.getEntryNode();
    Node* muo_node = opi->getNode();
    Node* node;
#endif

    // values 0 and 1 to set flag variable
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=- w0 Before " << std::endl;
        if (Log::isEnabled()) 
            printNode(entry_node,Log::out());
    }
#endif
    insertLdConst(1);
    insertLdConst(0);
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=- w0 After " << std::endl;
        if (Log::isEnabled()) 
            printNode(entry_node,Log::out());
    }
#endif

    // insert flag=0 after monitor instruction opnd creation instruction
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=- w1 Before " << std::endl;
        if (Log::isEnabled()) 
            printNode(muo_node,Log::out());
    }
#endif
    stvar0 = _instFactory.makeStVar(muflag, i32_0);
    stvar0->insertAfter(opi);
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=- w1 After " << std::endl;
        if (Log::isEnabled()) 
            printNode(muo_node,Log::out());
    }
#endif

    // insert flag=1 before virtual call instructions
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=-=-=- Start w2" << std::endl;
    }
#endif
    for (inst_it = vcInsts->begin( ); inst_it != vcInsts->end( ); inst_it++ ) {
#ifdef _DEBUG
        node = (*inst_it)->getNode();
        if (_seinfo) {
            Log::out() << "=-=- w2 Before " << std::endl;
            if (Log::isEnabled()) 
                printNode(node,Log::out());
        }
#endif
        stvar1 = _instFactory.makeStVar(muflag, i32_1);
        stvar1->insertBefore(*inst_it);
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() << "=-=- w2 After " << std::endl;
            if (Log::isEnabled()) 
                printNode(node,Log::out());
        }
#endif
    }
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=-=-=- Finish w2" << std::endl;
    }
#endif

    insertFlagCheck(syncInsts,muflag);

} // fixMonitorInstsVCalls(MonUnit* mu)


/*
*  Insers flag check before monitor instruction.
*  If flag = 0 monitor instruction isn't executed.
*  Operand flag may be VarOpnd* or SsaTmpOpnd* type.
*/
void 
EscAnalyzer::insertFlagCheck(Insts* syncInsts, Opnd* muflag) {
    Insts::iterator inst_it;
    OpndManager& _opndManager = irManager.getOpndManager();
    InstFactory& _instFactory = irManager.getInstFactory();
    ControlFlowGraph& fg = irManager.getFlowGraph();
    TypeManager& _typeManager  = irManager.getTypeManager();
    Type* typeInt32 = _typeManager.getInt32Type();

    // check flag before monitor instructions
    assert(muflag->isVarOpnd()||muflag->isSsaTmpOpnd());
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=-=-=- Start w3" << std::endl;
    }
#endif
    for (inst_it = syncInsts->begin( ); inst_it != syncInsts->end( ); inst_it++ ) {
        Inst* curMonInst = (*inst_it);
        Node* oldnode = curMonInst->getNode();
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() << "=-=- w3 Before " << std::endl;
            if (Log::isEnabled()) 
                printNode(oldnode,Log::out());
        }
#endif
        Node* afterMonInstBlock = NULL;
        Node* tiInstBlock = NULL;
        if ((*inst_it)->getNextInst()!=NULL) {
            // monitor inst isn'n last
            tiInstBlock = fg.splitNodeAtInstruction(curMonInst, true, false, _instFactory.makeLabel());
            afterMonInstBlock = tiInstBlock;
        } else {
            // monitor inst is last
            afterMonInstBlock = (Node*)(oldnode->getUnconditionalEdge()->getTargetNode());
        }
        SsaTmpOpnd* i32_flag;
        if (muflag->isVarOpnd()) {
            i32_flag = _opndManager.createSsaTmpOpnd(typeInt32);
            _instFactory.makeLdVar(i32_flag,(VarOpnd*)muflag)->insertBefore(curMonInst);
        } else
            i32_flag = (SsaTmpOpnd*)muflag;
        Inst* branch_inst = _instFactory.makeBranch(Cmp_EQ, Type::Int32, 
                i32_flag, i32_0, (LabelInst*)(afterMonInstBlock->getFirstInst()));
        // insert flag check
        branch_inst->insertBefore(curMonInst);
#ifdef _DEBUG
        Node* monInstBlock = 
#endif
        fg.splitNodeAtInstruction(branch_inst,true, false, _instFactory.makeLabel());
        fg.addEdge(oldnode,afterMonInstBlock);
#ifdef _DEBUG
        if (_seinfo) {
            Log::out() << "=-=- w3 After " << std::endl;
            if (Log::isEnabled()) {
                printNode(oldnode,Log::out());
                printNode(monInstBlock,Log::out());
                if (tiInstBlock != NULL) {
                    printNode(tiInstBlock,Log::out());
                }
            }
        }
#endif
    }
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << "=-=-=-=- Finish w3" << std::endl;
    }
#endif
} // insertFlagCheck(Insts* syncInsts, VarOpnd* muflag)


void 
EscAnalyzer::printNode(Node* n,::std::ostream& os) {
    const Edges& ie = n->getInEdges();
    const Edges& oe = n->getOutEdges();  
    Edges::const_iterator i;
    os << "=- Node Id." << n->getId() << std::endl;
    FlowGraph::printInsts(os,n,2);
    os << "=- pred: ";
    for(i = ie.begin(); i != ie.end(); ++i) {
        os << (*i)->getSourceNode()->getId() << " ";
    }
    os << std::endl;
    os << "=- succ: ";
    for(i = oe.begin(); i != oe.end(); ++i) {
        os << (*i)->getTargetNode()->getId() << " ";
    }
    os << std::endl;
} // printNode(Node* n,::std::ostream& os) 


/*
*   Checks state for NT_LDOBJ nodes.
*   Returns 
*     GLOBAL_ESCAPE  - for global escaped NT_OBJECT, NT_RETVAL, NT_DEFARG
*     node state     - for not global escaped NT_OBJECT 
*     0              - for not global escaped ?NT_RETVAL, NT_DEFARG
*/
uint32 
EscAnalyzer::checkState(Inst* inst,uint32 st) {
    uint32 st1;
    Inst* inst1;
    Opnd* opnd1;
    uint32 nsrc=inst->getNumSrcOperands();

    if (st <= GLOBAL_ESCAPE)
        return st;
    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(inst->getId())) {
            return st;
        }
    }
    if (inst->getOpcode()==Op_DirectCall || inst->getOpcode()==Op_IndirectMemoryCall) {
        Opnd *returnOpnd = inst->getDst(); 
        if (returnOpnd != NULL) {
            CnGNode* n = findCnGNode_op(returnOpnd->getId());
            if (n != NULL) {
                st1 = getEscState(n);
                if (st > st1)
                    st=st1;
            }
        }
        return st;
    }
    if (st <= GLOBAL_ESCAPE)
        return st;
    if (inst->getOpcode()==Op_TauLdInd || inst->getOpcode()==Op_LdVar) {  // ldind, ldvar
        Opnd *dst = inst->getDst(); 
        CnGNode* n = findCnGNode_op(dst->getId());
        if (n != NULL) {
            st1 = getEscState(n);
            if (st > st1)
                st=st1;
        }
    }
    if (st <= GLOBAL_ESCAPE)
        return st;
    switch (inst->getOpcode()) {
        case Op_LdRef:        // ldstr
        case Op_NewObj:          // newobj
        case Op_NewArray:        // newarray
        case Op_NewMultiArray:   // newmultiarray
        case Op_DefArg:          // defarg
        {
            CnGNode* n = findCnGNode_in(inst->getId());
            if (n != NULL) {
                st1 = getEscState(n);
                if (st > st1)
                    st=st1;
            }
            break;
        }
        default:
            break;
    }
    if (st <= GLOBAL_ESCAPE)
        return st;
    scannedObjs->push_back(inst->getId());
    for (uint32 i=0; i<nsrc; i++) {
        opnd1 = inst->getSrc(i);
        if (opnd1->isVarOpnd()) {
            inst1 = opnd1->asVarOpnd()->getVarAccessInsts();
        } else {
            inst1 = opnd1->getInst();
        }
        st1 = checkState(inst1,st);
        if (st > st1)
            st=st1;
        if (st<=GLOBAL_ESCAPE)
            break;
    }
    scannedObjs->pop_back();
    return st;
} // checkState(Inst* inst,uint32 st) 


void
EscAnalyzer::findObject(Inst* inst,std::string text) {
    Inst* inst1 = NULL;
    uint32 nsrc=inst->getNumSrcOperands();

    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(inst->getId())) {
#ifdef _DEBUG
            if (_seinfo) {
                Log::out() << "instId " << inst->getId() 
                    << "  .  .  . " << std::endl;
            }
#endif
            return;
        }
    }
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << text; 
        if (Log::isEnabled()) 
            inst->print(Log::out()); 
        Log::out() << std::endl;
    }
#endif
    if (inst->getOpcode()==Op_DirectCall || inst->getOpcode()==Op_IndirectMemoryCall) {
        Opnd *returnOpnd = inst->getDst(); 
        if (returnOpnd != NULL) {
            CnGNode* n = findCnGNode_op(returnOpnd->getId());
            if (n != NULL) {
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out()<< text << "  ";
                    if (Log::isEnabled())          
                        printCnGNode(n,Log::out()); 
                    Log::out()<< std::endl;
                }
#endif
            }
        }
        if (inst->getOpcode()==Op_IndirectMemoryCall) {
#ifdef _DEBUG
            MethodDesc* md = inst->getSrc(0)->getInst()->asMethodInst()->getMethodDesc();
            if (_seinfo) {
                Log::out() << text << "  "; 
                if (Log::isEnabled())         
                    md->printFullName(Log::out() ); 
                Log::out() << std::endl;
            }
#endif
        }
        return;
    }
    if (inst->getOpcode()==Op_TauLdInd || inst->getOpcode()==Op_LdVar) { // ldind,ldvar 
        Opnd *dst = inst->getDst(); 
        CnGNode* n = findCnGNode_op(dst->getId());
        if (n != NULL) {
#ifdef _DEBUG
            if (_seinfo) {
                Log::out()<< text << "  "; 
                if (Log::isEnabled())         
                    printCnGNode(n,Log::out()); 
                Log::out()<< std::endl;
            }
#endif
        }
    }
    switch (inst->getOpcode()) {
        case Op_LdRef:        // ldstr
        case Op_NewObj:          // newobj
        case Op_NewArray:        // newarray
        case Op_NewMultiArray:   // newmultiarray
        case Op_DefArg:          // defarg
        {
            CnGNode* n = findCnGNode_in(inst->getId());
            if (n != NULL) {
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << text << "  "; 
                    if (Log::isEnabled())         
                        printCnGNode(n,Log::out() ); 
                    Log::out() << std::endl;
            //        if (getEscState(n) == GLOBAL_ESCAPE)
            //            return;
                }
#endif
            }
            break;
        }
        default:
            break;
    }
    scannedObjs->push_back(inst->getId());
    for (uint32 i=0; i<nsrc; i++) {
        inst1 = inst->getSrc(i)->getInst();
        findObject(inst1,text+" ");
    }
    scannedObjs->pop_back();
}  // findObject(Inst* inst,std::string text) 


/*
*  Finds (prints) origin objects of monitor instruction operand.
*/
void
EscAnalyzer::findObject1(Inst* inst,std::string text) {
    Inst* inst1;
    uint32 nsrc=inst->getNumSrcOperands();

    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(inst->getId())) {
#ifdef _DEBUG
            if (_seinfo) {
                Log::out() << "instId " << inst->getId() 
                    << "  .  .  . " << std::endl;
            }
#endif
            return;
        }
    }
#ifdef _DEBUG
    if (_seinfo) {
        Log::out() << text; 
        if (Log::isEnabled()) 
            inst->print(Log::out()); 
        Log::out() << std::endl;
    }
#endif
    if (inst->getOpcode()==Op_DirectCall || inst->getOpcode()==Op_IndirectMemoryCall) {
        Opnd *returnOpnd = inst->getDst(); 
        if (returnOpnd != NULL) {
            CnGNode* n = findCnGNode_op(returnOpnd->getId());
            if (n != NULL) {
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out()<< text << "  ";
                    if (Log::isEnabled())          
                        printCnGNode(n,Log::out()); 
                    Log::out()<< std::endl;
                }
#endif
            }
        }
        if (inst->getOpcode()==Op_IndirectMemoryCall) {
#ifdef _DEBUG
            MethodDesc* md = inst->getSrc(0)->getInst()->asMethodInst()->getMethodDesc();
            if (_seinfo) {
                Log::out() << text << "  "; 
                if (Log::isEnabled())         
                    md->printFullName(Log::out() ); 
                Log::out() << std::endl;
            }
#endif
        }
        return;
    }
    if (inst->getOpcode()==Op_TauLdInd || inst->getOpcode()==Op_LdVar) { // ldind,ldvar 
        Opnd *dst = inst->getDst(); 
        CnGNode* n = findCnGNode_op(dst->getId());
        if (n != NULL) {
#ifdef _DEBUG
            if (_seinfo) {
                Log::out()<< text << "  "; 
                if (Log::isEnabled())         
                    printCnGNode(n,Log::out()); 
                Log::out()<< std::endl;
            }
#endif
        }
    }
    switch (inst->getOpcode()) {
        case Op_LdRef:        // ldstr
        case Op_NewObj:          // newobj
        case Op_NewArray:        // newarray
        case Op_NewMultiArray:   // newmultiarray
        case Op_DefArg:          // defarg
        {
            CnGNode* n = findCnGNode_in(inst->getId());
            if (n != NULL) {
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << text << "  "; 
                    if (Log::isEnabled())         
                        printCnGNode(n,Log::out() ); 
                    Log::out() << std::endl;
                }
#endif
            }
            break;
        }
        default:
            break;
    }
    scannedObjs->push_back(inst->getId());
    switch (inst->getOpcode()) {
        case Op_TauLdInd:        // ldind
        case Op_AddScaledIndex:  // addindex
            inst1 = inst->getSrc(0)->getInst();
            findObject1(inst1,text+" ");
            break;
        case Op_TauStInd:        // stind
            for (uint32 i=0; i<2; i++) {
                inst1 = inst->getSrc(i)->getInst();
                findObject1(inst1,text+" ");
            }
            break;
        default:
            for (uint32 i=0; i<nsrc; i++) {
                inst1 = inst->getSrc(i)->getInst();
                findObject1(inst1,text+" ");
            }
    }
    scannedObjs->pop_back();
}  // findObject1(Inst* inst,std::string text) 


void
EscAnalyzer::markNotEscInsts() {
    CnGNodes::iterator it;
    bool p2 = false;
    StlMap<uint32, uint32> nonEscInsts(eaMemManager);
    typedef ::std::pair <uint32,uint32> intPair;

    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType == NT_OBJECT && getEscState(*it) > GLOBAL_ESCAPE) {
            nonEscInsts.insert(intPair((*it)->instrId,getFullState(*it))); 
        }
    }
    if (p2 && Log::isEnabled()) {
        Log::out() << "================                       > "; 
        irManager.getMethodDesc().printFullName(Log::out()); 
        Log::out() << std::endl;
    }
}  // markNotEscInsts() 


/*
*  Summarizes common number of objects with the same state.
*  Prints statictics if required.
*/
void
EscAnalyzer::createdObjectInfo() {
    CnGNodes::iterator it;
    int n0 = 0;
    int n_ge = 0;
    int n_ae = 0;
    int n_ne = 0;
    uint32 state = 0;

    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType == NT_OBJECT) {
            n0++;
            state = getEscState(*it);
            if (state == GLOBAL_ESCAPE)
                n_ge++;
            if (state == ARG_ESCAPE)
                n_ae++;
            if (state == NO_ESCAPE)
                n_ne++;
        }
    }
    if (_printstat==1) {
        Log::out() << "**************  Created object info: ";
        if (Log::isEnabled())  
            mh.printFullName(Log::out()); 
        Log::out() << std::endl; 
        Log::out() << "  Number of created objects: " << n0 << std::endl; 
        if (n0>0) {
            Log::out() << "    Global escaped objects: " << n_ge << std::endl; 
            Log::out() << "    Arg. escaped objects: " << n_ae << std::endl; 
            Log::out() << "    Non escaped objects: " << n_ne << std::endl; 
        }
        Log::out() << "**************  " << std::endl; 
    }

    comObjStat._n0+=n0;
    comObjStat._n_ge+=n_ge;
    comObjStat._n_ae+=n_ae;
    comObjStat._n_ne+=n_ne;

    if (_printstat==1) {
        Log::out() << "**************  Common created object info " << std::endl; 
        Log::out() << "  Number of created objects: " << comObjStat._n0 << std::endl; 
        Log::out() << "    Global escaped objects: " << comObjStat._n_ge << std::endl; 
        Log::out() << "    Arg. escaped objects: " << comObjStat._n_ae << std::endl; 
        Log::out() << "    Non escaped objects: " << comObjStat._n_ne << std::endl; 
        Log::out() << "    Objects in loop: " << comObjStat._n_lo << std::endl; 
        Log::out() << "**************  " << std::endl; 
    }
}  // createdObjectInfo() 


void
EscAnalyzer::saveScannedMethodInfo() {
    CnGNodes::iterator it;
    MethodDesc* mdesc = &irManager.getMethodDesc();
    MemoryManager& globalMM = irManager.getCurrentJITContext()->getGlobalMemoryManager();
    const char* ch1 = mdesc->getParentType()->getName();
    const char* ch2 = mdesc->getName(); 
    const char* ch3 = mdesc->getSignatureString();
    if (calledMethodInfos==NULL) {
        calledMethodInfosLock.lock();
        if (calledMethodInfos==NULL)
            calledMethodInfos = new (globalMM) CalledMethodInfos(globalMM);
        calledMethodInfosLock.unlock();
    } else {
        CalledMethodInfo* mtdInfo = getMethodInfo(ch1,ch2,ch3);
        if (mtdInfo!=NULL)
            return;    // already saved for global analyzed method
    }
    if (getMethodInfo(ch1,ch2,ch3)!=NULL) // info was saved by another jit
        return;

    calledMethodInfosLock.lock();  // Lock to save method info in common memory

    if (getMethodInfo(ch1,ch2,ch3)!=NULL) {   // already saved by another jit
        calledMethodInfosLock.unlock();  // Unlock 
        return;
    }

    CalledMethodInfo* minfo = new (globalMM) CalledMethodInfo;
    char* mpname=new (globalMM) char[strlen(ch1)+1];
    strcpy(mpname,ch1);
    char* mname=new (globalMM) char[strlen(ch2)+1];
    strcpy(mname,ch2);
    char* msig=new (globalMM) char[strlen(ch3)+1];
    strcpy(msig,ch3);
    MemberIdent* mident = new (globalMM) MemberIdent;
    mident->parentName=mpname;
    mident->name=mname;
    mident->signature=msig;
    minfo->methodIdent=mident;
    uint32 numpar = mdesc->getMethodSig()->getNumParams();
    minfo->numberOfArgs=numpar;
    ParamInfos* prminfos = new (globalMM) ParamInfos(globalMM);
    minfo->paramInfos=prminfos;
    minfo->retValueState=0;
    if (mdesc->getMethodSig()->getReturnType()->isReference()) {
        uint32 escstate = 3, bitstate = 0;
        for (it = cngNodes->begin( ); it != cngNodes->end( ); it++) {
            if ((*it)->nodeType==NT_EXITVAL) {
                bitstate = bitstate|getCalleeEscaped(*it);
                if (escstate > getEscState(*it))
                    escstate=getEscState(*it);
            }
        } 
        minfo->retValueState=escstate|bitstate;
    }
    bool pmt = checkMonitorsOnThis();
    minfo->mon_on_this = pmt;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++) {
        if ((*it)->nodeType==NT_DEFARG) {
            ParamInfo* prminfo = new (globalMM) ParamInfo;
            prminfo->state=getFullState(*it);
            prminfo->paramNumber=(*it)->argNumber;
            prminfos->push_back(prminfo);
        }
    }
    calledMethodInfos->push_back(minfo);

    calledMethodInfosLock.unlock();  // Unlock 

#ifdef _DEBUG
    if (_scanMtds==1) {
        ParamInfos::iterator it2;
        Log::out() << "====     ===== calledMethodInfo  " << std::endl;
        Log::out() << minfo->methodIdent->parentName << "  ";
        Log::out() << minfo->methodIdent->name << "  ";
        Log::out() << minfo->methodIdent->signature << "  ";
        Log::out() << minfo->numberOfArgs<< "  " << std::endl;
        for (it2 = minfo->paramInfos->begin( ); it2 != minfo->paramInfos->end( ); it2++) {
            Log::out() << (*it2)->paramNumber << "  st." 
                << (*it2)->state << std::endl;
        }
        Log::out() << "==== end ===== calledMethodInfo  " << std::endl;
    }
#endif
}  // saveScannedMethodInfo(MemoryManager& mm)


void
EscAnalyzer::printMethodInfos() {
    CalledMethodInfos::iterator it1;
    ParamInfos::iterator it2;
    Log::out() << "==== debug ===== calledMethodInfos;  " << std::endl;
    if (calledMethodInfos==NULL)
        Log::out() << "  calledMethodInfos is NULL " << std::endl;
    if (calledMethodInfos!=NULL)
        for (it1 = calledMethodInfos->begin( ); it1 != calledMethodInfos->end( ); it1++) {
            Log::out() << (*it1)->methodIdent->parentName << "  ";
            Log::out() << (*it1)->methodIdent->name << "  ";
            Log::out() << (*it1)->methodIdent->signature << "  ";
            Log::out() << (*it1)->numberOfArgs<< "  " << std::endl;
            for (it2 = (*it1)->paramInfos->begin( ); 
                    it2 != (*it1)->paramInfos->end( ); it2++) {
                Log::out() << (*it2)->paramNumber << "  st." 
                    << (*it2)->state << std::endl;
            }
        }
    Log::out() << "====  end  ===== calledMethodInfos;  " << std::endl;
}  // printMethodInfos()


void
EscAnalyzer::printMethodInfo(CalledMethodInfo* mi) {
    ParamInfos::iterator it2;
    Log::out() << "==== debug ===== calledMethodInfo  " << std::endl;
    if (mi==NULL) 
        Log::out() << "  calledMethodInfo is NULL " << std::endl;
    else {
        Log::out() << mi->methodIdent->parentName << "  ";
        Log::out() << mi->methodIdent->name << "  ";
        Log::out() << mi->methodIdent->signature << "  ";
        Log::out() << "Number of parameters: " << mi->numberOfArgs<< "  " << std::endl;
        for (it2 = mi->paramInfos->begin( ); it2 != mi->paramInfos->end( ); it2++) {
            Log::out() << (*it2)->paramNumber << "  st.";
            if (Log::isEnabled()) 
                printState((*it2)->state);
            Log::out() << std::endl;
        }
        Log::out() << "Return value state: ";
        printState(mi->retValueState);
        Log::out() << "  " << std::endl;
    }
    Log::out() << "====  end  ===== calledMethodInfo  " << std::endl;
}  // printMethodInfos(CalledMethodInfo* mi) 


void
EscAnalyzer::scanCalleeMethod(Inst* call) {
    MethodDesc* methodDesc = 0;
    
#ifdef _DEBUG
    if (_scanMtds==1) {
        Log::out() << "=="; 
        if (Log::isEnabled()) 
            call->print(Log::out()); 
        Log::out() << std::endl;
    }
#endif
    if (call == NULL) {  // scanned Op_DirectCall, not scanned Op_IndirectMemoryCall
        Log::out() << "scanMethod: NULL" << std::endl;
        return;
    }
    methodDesc = call->asMethodCallInst()->getMethodDesc();

#ifdef _DEBUG
    if (_scanMtds==1) {
        Log::out() << std::endl; 
        Log::out() << "scanMethod: " << methodDesc->getParentType()->getName() 
            << "." << methodDesc->getName() << methodDesc->getSignatureString() 
            << "  " << methodDesc << std::endl;
        Log::out() << "    NumParams: " 
            << methodDesc->getMethodSig()->getNumParams()<< std::endl;
        Log::out() << "    isNative: " << methodDesc->isNative() << std::endl;
        Log::out() << "    isStatic: " << methodDesc->isStatic() << std::endl;
        Log::out() << "    isInstance: " << methodDesc->isInstance() << std::endl;
        Log::out() << "    isFinal: " << methodDesc->isFinal() << std::endl;
        Log::out() << "    isVirtual: " << methodDesc->isVirtual() << std::endl;
        Log::out() << "    isAbstract: " << methodDesc->isAbstract() << std::endl;
        Log::out() << "    isInstanceInitializer: " 
            << methodDesc->isInstanceInitializer() << std::endl;
        Log::out() << "    isOverridden: " << methodDesc->isOverridden() << std::endl;
        Log::out() << "    isJavaByteCodes: " << methodDesc->isJavaByteCodes() << std::endl;
   }
#endif

    OpndManager& _opndManager(irManager.getOpndManager());
    Opnd *returnOpnd = 0;
    if(call->getDst()->isNull())
        returnOpnd = _opndManager.getNullOpnd();
    else 
        returnOpnd = _opndManager.createSsaTmpOpnd(call->getDst()->getType());

    IRManager* inlinedIRM = new (eaMemManager) IRManager(irManager.getMemoryManager(), irManager, *methodDesc, returnOpnd);
    DrlVMCompilationInterface& ci= (DrlVMCompilationInterface&)inlinedIRM->getCompilationInterface();
    bool cibcmap = ci.isBCMapInfoRequired();
    if (cibcmap) {
        ci.setBCMapInfoRequired(false);
    }
    
    CompilationContext inlineCC(irManager.getMemoryManager(), &ci, ci.getCompilationContext());
    inlineCC.setHIRManager(inlinedIRM);
    runTranslatorSession(inlineCC);
    
    optimizeTranslatedCode(*inlinedIRM);

    if (cibcmap) {
        ci.setBCMapInfoRequired(true);
    }
    
    EscAnalyzer ea1(this, *inlinedIRM);
    ea1.doAnalysis();

}  // scanCalleeMethod(Inst* call, MemoryManager& mm) 

void EscAnalyzer::runTranslatorSession(CompilationContext& inlineCC) {
    TranslatorSession* traSession = (TranslatorSession*)translatorAction->createSession(inlineCC.getCompilationLevelMemoryManager());
    traSession->setCompilationContext(&inlineCC);
    inlineCC.setCurrentSessionAction(traSession);
    traSession->run();
    inlineCC.setCurrentSessionAction(NULL);
}


void
EscAnalyzer::optimizeTranslatedCode(IRManager& irm) {
    // run ssa pass
    OptPass::computeDominators(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    ControlFlowGraph& flowGraph = irm.getFlowGraph();
   
    DomFrontier frontier(irm.getNestedMemoryManager(),*dominatorTree,&flowGraph);
    SSABuilder ssaBuilder(irm.getOpndManager(),irm.getInstFactory(),frontier,&flowGraph, 
                          irm.getOptimizerFlags());
    ssaBuilder.convertSSA(irm.getMethodDesc());
    irm.setInSsa(true);
    irm.setSsaUpdated();

    // run devirt pass
    Devirtualizer pass(irm);
    pass.guardCallsInRegion(irm, dominatorTree);

}  // optimizeTranslatedCode(IRManager& irManager) 


void
EscAnalyzer::setCreatedObjectStates() {
    CnGNodes::iterator it;
    NodeMDs::iterator it1;

    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_STFLD) {
            initNodeType = NT_STFLD;
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"-- before scanGE: node "
                    <<(*it)->cngNodeId<<" opnd "<<(*it)->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(*it); 
                Log::out() << std::endl;
            }
#endif
            scanCnGNodeRefsGE(*it);
        }
    }
    scannedObjs->clear();
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType&NT_EXITVAL) {  // returned, thrown
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"-- before scanEV: node "
                    <<(*it)->cngNodeId<<" opnd "<<(*it)->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(*it); 
                Log::out() << std::endl;
            }
#endif
            initNodeType = (*it)->nodeType;
            scanCnGNodeRefsGE(*it);
        }
    }
    scannedObjs->clear();
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_DEFARG) {
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"-- before scanDA: node "
                    <<(*it)->cngNodeId<<" opnd "<<(*it)->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(*it); 
                Log::out() << std::endl;
            }
#endif
            initNodeType = NT_DEFARG;
            scanCnGNodeRefsGE(*it);
            scannedObjs->clear(); // temporary
        }
    }
    scannedObjs->clear();
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_ACTARG) {
            curMDNode=(*it)->cngNodeId;
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"-- before scanAE: node "
                    <<(*it)->cngNodeId<<" opnd "<<(*it)->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(*it); 
                Log::out() << std::endl;
            }
#endif
            initNodeType = NT_ACTARG;
            scanCnGNodeRefsAE(*it);
        }
    }
    scannedObjs->clear();
    if (method_ea_level==0) {
        DominatorTree* dominatorTree = irManager.getDominatorTree();
        if (!(dominatorTree && dominatorTree->isValid())) {
             OptPass::computeDominators(irManager);
             dominatorTree = irManager.getDominatorTree();
        }
        if (dominatorTree && dominatorTree->isValid()) {
            OptPass::computeLoops(irManager);
            LoopTree* ltree = irManager.getLoopTree();
            if (ltree->isValid())
                for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
                    if ( (*it)->nodeType==NT_OBJECT) {
                        if (ltree->getLoopHeader((*it)->fgNode,false)) {
                            if ((*it)->state > GLOBAL_ESCAPE) {
#ifdef _DEBUG
                                if (_setState) {
                                    Log::out() 
                                        <<"--setSt loop: node "
                                        <<(*it)->cngNodeId<<" opndId "
                                        <<(*it)->opndId<<" state ";
                                    if (Log::isEnabled()) 
                                        printState(*it);
                                    Log::out() 
                                        <<" to set loop" << std::endl;
                                }
#endif
                                setLoopCreated(*it);
                                comObjStat._n_lo++;
                            }
                        }
                    }
                }
        } else {
#ifdef _DEBUG
            mh.printFullName(Log::out()); Log::out() << std::endl;
            Log::out() << "DominatorTree isn't valid  " << std::endl;
#endif
        }
    }
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        uint32 nt = (*it)->nodeType;
        if ((nt&NT_OBJECT || nt==NT_LDOBJ) && (getEscState(*it)==ARG_ESCAPE)) {
            for (it1 = (*it)->nodeMDs->begin(); it1 != (*it)->nodeMDs->end(); 
                    it1++) {
                CnGNode* n=findCnGNode_id(*it1);   // method argument node
                assert(n!=NULL);
                MethodDesc* mdesc = (MethodDesc*)n->refObj;
                Inst* callInst = n->nInst;
                if (mdesc->isNative()) {  // not scanned native methods 
                    if (strcmp(mdesc->getParentType()->getName(),"java/lang/VMMemoryManager")==0
                        &&strcmp(mdesc->getName(),"arrayCopy")==0) {
                        break;  // objects don't escape java/lang/VMMemoryManager::arrayCopy
                    }
                    if (strcmp(mdesc->getParentType()->getName(),"java/lang/VMClassRegistry")==0
                        &&strcmp(mdesc->getName(),"getName")==0) {
                        break;  // objects don't escape java/lang/VMClassRegistry::getName
                    }
                    if (strcmp(mdesc->getParentType()->getName(),"java/lang/VMThreadManager")==0
                        &&strcmp(mdesc->getName(),"notify")==0) {
                        break;  // objects don't escape java/lang/VMThreadManager::notify
                    }
                    setEscState(*it,GLOBAL_ESCAPE);
#ifdef _DEBUG
                    if (_scanMtds==1) {
                        if (Log::isEnabled()) 
                            mdesc->printFullName(Log::out());
                        Log::out() << std::endl;
                        Log::out() << "    isNative: " 
                            << mdesc->isNative() << std::endl;
                    }
                    if (_setState) {
                        Log::out() <<"--setSt 1: node "
                            <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId
                            <<" state ";
                        if (Log::isEnabled()) 
                            printState(*it); 
                        Log::out()<<" to gl.esc."<< std::endl;
                    }
#endif
                    break;
                }
                if (callInst->getOpcode()!=Op_DirectCall) {   // not direct call
                    if (method_ea_level == 0) {
                        MonUnit* mu = NULL;
                        if (monitorInstUnits!=NULL)
                            mu = findMonUnit((*it)->opndId);
                        if (mu != NULL) {
                            setVirtualCall(*it);
                            addMonUnitVCall(mu,callInst);
#ifdef _DEBUG
                            if (Log::isEnabled()) {
                                Log::out() << "=-=-=-=- callimem for this ";
                                Log::out() << std::endl;
                                Log::out() << "=-=- ";
                                printCnGNode(*it,Log::out());
                                Log::out() << std::endl;
                                printCnGNode(n,Log::out());
                                Log::out() << std::endl;
                                callInst->print(Log::out());
                                Log::out() << std::endl;
                                mdesc->printFullName(Log::out());
                                Log::out() << std::endl;
                                Log::out() << "=-=-=-=- end " << std::endl;
                            }
#endif
                            continue;
                        }
                    }

                    setEscState(*it,GLOBAL_ESCAPE);
#ifdef _DEBUG
                    if (_scanMtds==1) {
                        if (Log::isEnabled()) {
                            callInst->print(Log::out());
                            Log::out() << std::endl;
                            mdesc->printFullName(Log::out());
                        }
                        Log::out() << std::endl;
                        Log::out() << "    isStatic: " 
                            << mdesc->isStatic() << std::endl;
                        Log::out() << "    isFinal: " 
                            << mdesc->isFinal() << std::endl;

                        Log::out() << "---------------" << std::endl;
                        Log::out() << "    NumParams: " 
                            << mdesc->getMethodSig()->getNumParams()<< std::endl;
                        Log::out() << "    isNative: " 
                            << mdesc->isNative() << std::endl;
                        Log::out() << "    isStatic: " 
                            << mdesc->isStatic() << std::endl;
                        Log::out() << "    isInstance: " 
                            << mdesc->isInstance() << std::endl;
                        Log::out() << "    isFinal: " 
                            << mdesc->isFinal() << std::endl;
                        Log::out() << "    isVirtual: " 
                            << mdesc->isVirtual() << std::endl;
                        Log::out() << "    isAbstract: " 
                            << mdesc->isAbstract() << std::endl;
                        Log::out() << "    isInstanceInitializer: " 
                            << mdesc->isInstanceInitializer() << std::endl;
                        Log::out() << "    isOverridden: " 
                            << mdesc->isOverridden() << std::endl;
                        Log::out() << "    isJavaByteCodes: " 
                            << mdesc->isJavaByteCodes() << std::endl;

                    }
                    if (_setState) {
                        Log::out() <<"--setSt 2: node "
                            <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId
                            <<" state ";
                        if (Log::isEnabled()) 
                            printState(*it);
                        Log::out() <<" to gl.esc."
                            << std::endl;
                    }
#endif
                    break;
                }

                CalledMethodInfo* mtdInfo = findMethodInfo(mdesc,callInst);
                if (mtdInfo == NULL) {    // no info about called method
                    setEscState(*it,GLOBAL_ESCAPE);
#ifdef _DEBUG
                    if (_setState) {
                        Log::out() <<"--setSt 3: node "
                            <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId
                            <<" state ";
                        if (Log::isEnabled()) 
                            printState(*it);
                        Log::out() <<" to gl.esc." 
                            << std::endl;
                    }
#endif
                    break;
                } else {  // to use info about scanned method
                    uint32 narg = n->argNumber;
                    ParamInfos::iterator it2;
                    uint32 state = 0;
#ifdef _DEBUG
                    if (_setState) {
                        Log::out() << "--setSt cmi: method " 
                            << mtdInfo->methodIdent->parentName << "::"
                            << mtdInfo->methodIdent->name << " "
                            << mtdInfo->methodIdent->signature << std::endl;
                    }
#endif
                    for (it2 = mtdInfo->paramInfos->begin( ); 
                            it2 != mtdInfo->paramInfos->end( ); it2++) {
#ifdef _DEBUG
                        if (_setState) {
                            Log::out() 
                                <<(*it2)->paramNumber<<" == "<<narg<<" state "
                                <<(*it2)->state <<" < "<< getEscState(*it)<<"  ";
                            if (Log::isEnabled()) 
                                printState(*it);
                            Log::out() << std::endl;
                        }
#endif
                        if ((*it2)->paramNumber == narg) { //???to add scanning of contained obj
                            if ((state=(*it2)->state&ESC_MASK) < getEscState(*it)) {
#ifdef _DEBUG
                                if (_setState) {
                                    Log::out()<<"--setSt cmi1: node " 
                                        <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId
                                        <<" state "; 
                                    if (Log::isEnabled()) 
                                        printState(*it);
                                    Log::out() <<" to state "
                                        <<state<< std::endl;
                                }
#endif
                                setEscState(*it,state);
                            }
                        }
                    }
                }
            }
            if (getEscState(*it)==GLOBAL_ESCAPE) { // to set gl.escape for contained objects
                scanCnGNodeRefsGE(*it);    //??? to set initNodeType?
                scannedObjs->clear();
            }
        }
    }
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        uint32 nt = (*it)->nodeType;
        if ((nt&NT_RETVAL) && getEscState(*it)>GLOBAL_ESCAPE) {
            if (nt==NT_CATCHVAL || ((*it)->nInst->getOpcode())==Op_IndirectMemoryCall) {
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--setCOS 1: node "
                        <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId <<" state ";
                    if (Log::isEnabled()) 
                        printState(*it);
                    Log::out() <<" to gl.esc."<< std::endl;
                }
#endif
                setEscState(*it,GLOBAL_ESCAPE);
                continue;
            }
            MethodDesc* mdesc = (*it)->nInst->asMethodInst()->getMethodDesc();
            if (mdesc->isNative()) {  // not scanned native methods
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--setCOS 2: node "
                        <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId <<" state ";
                    if (Log::isEnabled()) 
                        printState(*it);
                    Log::out() <<" to gl.esc."<< std::endl;
                }
#endif
                setEscState(*it,GLOBAL_ESCAPE);
                continue;
            }
            if (((*it)->nInst->getOpcode())!=Op_DirectCall) {
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--setCOS 3: node "
                        <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId <<" state ";
                    if (Log::isEnabled()) 
                        printState(*it);
                    Log::out() <<" to gl.esc."<< std::endl;
                }
#endif
                setEscState(*it,GLOBAL_ESCAPE);
                continue;
            }
            CalledMethodInfo* mthInfo = findMethodInfo(mdesc,(*it)->nInst);
            if (mthInfo == NULL) {
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--setCOS 4: node "
                        <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId <<" state ";
                    if (Log::isEnabled()) 
                        printState(*it);
                    Log::out() <<" to gl.esc."<< std::endl;
                }
#endif
                setEscState(*it,GLOBAL_ESCAPE);
            } else {
                if (getEscState(*it)>((mthInfo->retValueState)&ESC_MASK)) {
#ifdef _DEBUG
                    if (_setState) {
                        Log::out() <<"--setCOS 5: node "
                            <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId <<" state ";
                        if (Log::isEnabled()) 
                            printState(*it);
                        Log::out() <<" to "<< mthInfo->retValueState<< std::endl;
                    }
#endif
                    setEscState(*it,(mthInfo->retValueState)&ESC_MASK);
                }
            }
        }
    }

    uint32 rs = NO_ESCAPE;  
    CnGRefs::iterator it2;
    CnGNode* cgn;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++) {
        if ((*it)->nodeType==NT_EXITVAL) {
            if ((*it)->outEdges ==NULL)
                continue;
            for (it2 = (*it)->outEdges->begin( ); it2 != (*it)->outEdges->end( ); it2++ ) {
                cgn = (*it2)->cngNodeTo;
                rs = getSubobjectStates(cgn);
                if (getEscState(cgn)<=(rs&ESC_MASK)) 
                    rs = getFullState(cgn);
            } 
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"--setCOS 6: node "
                    <<(*it)->cngNodeId<<" opndId "<<(*it)->opndId <<" state ";
                if (Log::isEnabled()) 
                    printState(*it);
                Log::out() <<" to "<< rs << std::endl;
            }
#endif
            setFullState(*it,rs);

        }
        scannedObjs->clear();
    }


}  // setCreatedObjectStates(MemoryManager& mm) 


uint32 
EscAnalyzer::getSubobjectStates(CnGNode* node) {
    uint32 st = NO_ESCAPE, rs;
    CnGRefs::iterator it;
    CnGNode* cgn;

    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(node->cngNodeId))
            return st;
    }
    if (node->outEdges==NULL)
        return st;
    for (it = node->outEdges->begin( ); it != node->outEdges->end( ); it++ ) {
        cgn = (*it)->cngNodeTo;
#ifdef _DEBUG
        if (_setState) {
            Log::out() <<"--getSOS : node "
                <<cgn->cngNodeId<<" opndId "<<cgn->opndId <<" state ";
            if (Log::isEnabled()) 
                printState(cgn);
            Log::out()  << std::endl;
        }
#endif
        if (st > getEscState(cgn))
            st = getEscState(cgn);
        if (st == GLOBAL_ESCAPE)
            return st;
        scannedObjs->push_back(node->cngNodeId);
        rs = getSubobjectStates(cgn);
        scannedObjs->pop_back();
        if (st > rs)
            st = rs;
        if (st == GLOBAL_ESCAPE)
            return st;
    } 
    return st;
}


EscAnalyzer::CalledMethodInfo*
EscAnalyzer::getMethodInfo(const char* ch1,const char* ch2,const char* ch3) {
    CalledMethodInfos::iterator it;
    if (calledMethodInfos==NULL)
        return NULL;
    for (it = calledMethodInfos->begin( ); it != calledMethodInfos->end( ); it++ ) {
        const char* c1 = (*it)->methodIdent->parentName; 
        const char* c2 = (*it)->methodIdent->name; 
        const char* c3 = (*it)->methodIdent->signature;
        if (strcmp(c1,ch1)==0 && strcmp(c2,ch2)==0 && strcmp(c3,ch3)==0)
            return (*it);
    }
    return NULL;
}  // getMethodInfo(const char* ch1,const char* ch2,const char* ch3) 


EscAnalyzer::CalledMethodInfo*
EscAnalyzer::findMethodInfo(MethodDesc* mdesc,Inst* callInst) {
    const char* ch1 = mdesc->getParentType()->getName(); 
    const char* ch2 = mdesc->getName(); 
    const char* ch3 = mdesc->getSignatureString();
    CalledMethodInfo* mtdInfo = getMethodInfo(ch1,ch2,ch3);
    if (mtdInfo == NULL) {
#ifdef _DEBUG
        if (_scanMtds==1) {
            Log::out() << "      = = = = = = = =  To scan method " << std::endl;
            if (Log::isEnabled()) {
                mdesc->printFullName(Log::out());
                Log::out() << std::endl;
                callInst->print(Log::out());
            }
            Log::out() << std::endl;
        }
#endif
        if (method_ea_level < maxMethodExamLevel) {
            scanCalleeMethod(callInst);
            mtdInfo=getMethodInfo(ch1,ch2,ch3);
        }
    }
    return mtdInfo;
} // findMethodInfo(MethodDesc* mdesc) 


/*
*   Finds specified parameter in specified method info.
*   Returns: 
*       state - for existent parameter,
*       0     - for another cases.
*/
uint32 
EscAnalyzer::getMethodParamState(CalledMethodInfo* mi, uint32 np) {
    ParamInfos::iterator it;
    uint32 st = 0;
    if (mi==NULL)
        return 0; 
    for (it = mi->paramInfos->begin( ); it != mi->paramInfos->end( ); it++) {
        if ((*it)->paramNumber == np) {
             st = (*it)->state;
             return st;
        }
    }
    return st;
}  // getMethodParamState(CalledMethodInfo* mi, uint32 np) 


void
EscAnalyzer::printCreatedObjectsInfo(::std::ostream& os) {
    CnGNodes::iterator it;
    NodeMDs::iterator it1;
    os << "================ Created Object States   < "; 
    irManager.getMethodDesc().printFullName(os); 
    os << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType & NT_OBJECT || (*it)->nodeType==NT_LDOBJ) {
            printCnGNode(*it,os);
            ((Opnd*)(*it)->refObj)->printWithType(os);
            if (getEscState(*it)==ARG_ESCAPE) {
                for (it1 = (*it)->nodeMDs->begin(); it1 != (*it)->nodeMDs->end(); it1++) {
                    CnGNode* n=findCnGNode_id(*it1);
                    assert(n!=NULL);
                    os << std::endl; os <<"    ";
                    printCnGNode(n,os);
                    os << std::endl; os <<"      ";
                    ((MethodDesc*)n->refObj)->printFullName(os);
                }
            }
            os << std::endl;
            os << "  =="; ((Opnd*)(*it)->refObj)->getInst()->print(os); 
            os << std::endl;
        }
    }
    os << "================                         > " ;
    irManager.getMethodDesc().printFullName(os); 
    os << std::endl;
}  // printCreatedObjectsInfo(::std::ostream& os) 


void
EscAnalyzer::scanCnGNodeRefsGE(CnGNode* cgn) {
//    CnGEdges* cge = cngEdges;
    CnGEdges::iterator it;
    CnGRefs::iterator it1;
    uint32 nscan=0;
    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(cgn->cngNodeId))
            return;
    }
#ifdef _DEBUG
    if (_setState) {
        Log::out() <<"--scanGE 1: node "<<cgn->cngNodeId
            <<" opndId "<<cgn->opndId<<" state ";
        if (Log::isEnabled()) 
            printState(cgn); 
        Log::out() <<" " 
            << nodeTypeToString(cgn) << cgn->nodeType << std::endl;
    }
#endif
    if (cgn->nodeType&NT_OBJS) {
        if (initNodeType!=NT_EXITVAL && initNodeType!=NT_DEFARG) { // 
            if (getEscState(cgn) > GLOBAL_ESCAPE) {
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--scanGE 2: node "
                        <<cgn->cngNodeId<<" opndId "<<cgn->opndId <<" state ";
                    if (Log::isEnabled()) 
                        printState(cgn);
                    Log::out() <<" to gl.esc."<< std::endl;
                    Log::out() <<"--scanGE 2: "<< nodeTypeToString(cgn) 
                        << cgn->nodeType <<" initNode "<<initNodeType<< std::endl;
                }
#endif
                setEscState(cgn,GLOBAL_ESCAPE);
            }
        } 
        if (initNodeType==NT_EXITVAL) {
            if (getCalleeEscaped(cgn) == 0) {
#ifdef _DEBUG
                if (_setState) {
                    Log::out() <<"--scanGE 3: node "
                        <<cgn->cngNodeId<<" opndId "<<cgn->opndId <<" state ";
                    if (Log::isEnabled()) 
                        printState(cgn);
                    Log::out() <<" to callee.esc."<< std::endl;
                    Log::out() <<"--scanGE 3: "<< nodeTypeToString(cgn) 
                        << cgn->nodeType <<" initNode "<<initNodeType<< std::endl;
                }
#endif
                setCalleeEscaped(cgn);
            }
            // The objects created in the method are not global escaped through return
        }
        if (initNodeType==NT_DEFARG) {
             if (getCalleeEscaped(cgn) == 0) {
#ifdef _DEBUG
                 if (_setState) {
                     Log::out() <<"--scanGE 5: node "
                         <<cgn->cngNodeId<<" opndId "<<cgn->opndId <<" state ";
                     if (Log::isEnabled()) 
                         printState(cgn);
                     Log::out() <<" to callee.esc."<< std::endl;
                     Log::out() <<"--scanGE 5: "<< nodeTypeToString(cgn) 
                         << cgn->nodeType <<" initNode "<<initNodeType<< std::endl;
                 }
#endif
                 setCalleeEscaped(cgn);
             }
        }
    }
    scannedObjs->push_back(cgn->cngNodeId);
    if (cgn->outEdges != NULL) {
        for (it1 = cgn->outEdges->begin( ); it1 != cgn->outEdges->end( ); it1++ ) {
            scanCnGNodeRefsGE(findCnGNode_id((*it1)->cngNodeTo->cngNodeId));
            if ((*it1)->edgeType!=ET_POINT)
                nscan++;
        }
    }
#ifdef _DEBUG
    if (_setState) {
        Log::out()<<"--scanGE 6: nscan "<<nscan<<" opndId "<<cgn->opndId<<std::endl;
    }
#endif
    if (cgn->nodeType==NT_ARRELEM && nscan==0) {
        Inst* inst_ldbase= ((Opnd*)(cgn->refObj))->getInst()->getSrc(0)->getInst();
        CnGNode* cgnode=findCnGNode_op(inst_ldbase->getSrc(0)->getId());
#ifdef _DEBUG
        if (_setState) {
            Log::out() <<"--scanGE arrElem: "; 
            if (Log::isEnabled()) 
                inst_ldbase->print(Log::out()); 
            Log::out() << std::endl;
            Log::out() <<"--scanGE next: " << cgnode->cngNodeId << " - " 
                << cgnode->opndId << std::endl;
        }
#endif
        scanCnGNodeRefsGE(cgnode);
    }  
    if (cgn->nodeType==NT_LDOBJ && nscan==0) {
        Inst* ldinst =((Opnd*)(cgn->refObj))->getInst();
        if (ldinst->getOpcode()==Op_TauLdInd) { //tttt
            Inst* inst_fldaddr= ldinst->getSrc(0)->getInst();
            if (inst_fldaddr->getOpcode()==Op_LdFieldAddr) {
                CnGNode* cgnode=findCnGNode_fl(inst_fldaddr,NT_INSTFLD);
#ifdef _DEBUG
                if (_setState) {
                    Log::out() << "TO FIND 1 for opnd "
                        << cgn->opndId << std::endl;
                    if (Log::isEnabled()) 
                        ldinst->print(Log::out());
                    Log::out() << std::endl;
                    if (Log::isEnabled())  
                        inst_fldaddr->print(Log::out());
                    Log::out() << std::endl;
                    Log::out() << cgn->cngNodeId << "-"
                        << cgn->opndId << " -> ";
                    Log::out() << cgnode->cngNodeId << "-"
                        << cgnode->opndId << std::endl;
                }
#endif
                scanCnGNodeRefsGE(cgnode);
            } else {
                if (inst_fldaddr->getOpcode()==Op_AddScaledIndex) {
                    CnGNode* cgnode=findCnGNode_op(ldinst->getSrc(0)->getId());
#ifdef _DEBUG
                    if (_setState) {
                        Log::out() << "TO FIND 2 for opnd "
                            << cgn->opndId << std::endl;
                        if (Log::isEnabled()) 
                            ldinst->print(Log::out());
                        Log::out() << std::endl;
                        if (Log::isEnabled()) 
                            inst_fldaddr->print(Log::out());
                        Log::out() << std::endl;
                        Log::out() << cgn->cngNodeId << "-"
                            << cgn->opndId << " -> ";
                        Log::out() << cgnode->cngNodeId << "-"
                            << cgnode->opndId << std::endl;
                    }
#endif
                    scanCnGNodeRefsGE(cgnode);
                }
            }
        }
    }
}  // scanCnGNodeRefsGE(CnGNode* cgn) 


void
EscAnalyzer::scanCnGNodeRefsEV(CnGNode* cgn) {
    CnGRefs::iterator it1;
    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(cgn->cngNodeId))
            return;
    }
#ifdef _DEBUG
    if (_setState) {
        Log::out() <<"--scanEV: node "<<cgn->cngNodeId
            <<" opndId "<<cgn->opndId <<" state ";
        if (Log::isEnabled()) 
            printState(cgn); 
        Log::out() << " "
            << nodeTypeToString(cgn) << cgn->nodeType << std::endl;
    }
#endif
    if (cgn->nodeType==NT_OBJECT)       // object is escaped if it is returned 
        if (getEscState(cgn) > GLOBAL_ESCAPE) {
            setEscState(cgn,GLOBAL_ESCAPE);
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"--scanEV: node "<<cgn->cngNodeId
                    <<" opndId "<<cgn->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(cgn);
                Log::out() <<" to gl.esc."<< std::endl;
            }
#endif
        }
    scannedObjs->push_back(cgn->cngNodeId);
    if (cgn->outEdges != NULL) {
        for (it1 = cgn->outEdges->begin( ); it1 != cgn->outEdges->end( ); it1++ ) {
            scanCnGNodeRefsGE(findCnGNode_id((*it1)->cngNodeTo->cngNodeId));
        }
    }
}  // scanCnGNodeRefsEV(CnGNode* cgn) 


void
EscAnalyzer::scanCnGNodeRefsDA(CnGNode* cgn) {
    CnGEdges* cge = cngEdges;
    CnGEdges::iterator it;
    CnGRefs::iterator it1;
    uint32 cgntype=cgn->nodeType;
    bool first=true;
    if (scannedObjs->size()!=0) {
        first=false;
        if (checkScannedObjs(cgn->cngNodeId))
            return;
    }
#ifdef _DEBUG
    if (_setState) {
        Log::out() <<"--scanDA: node "<<cgn->cngNodeId
            <<" opndId "<<cgn->opndId<<" state ";
        if (Log::isEnabled()) 
            printState(cgn);
        Log::out() <<" "<<nodeTypeToString(cgn)<<cgn->nodeType<< std::endl;
    }
#endif
    if ((cgntype&NT_OBJECT && !first)||(cgntype==NT_OBJECT && first))
        scannedObjs->push_back(cgn->cngNodeId);
        for (it = cge->begin( ); it != cge->end( ); it++ ) {
            if ((*it)->cngNodeFrom == cgn) {
                for (it1 = (*it)->refList->begin( ); it1 != (*it)->refList->end( ); it1++ ) {
                    scanCnGNodeRefsGE(findCnGNode_id((*it1)->cngNodeTo->cngNodeId));
                }
            }
        }           
}  // scanCnGNodeRefsDA(CnGNode* cgn) 


void
EscAnalyzer::scanCnGNodeRefsAE(CnGNode* cgn) {
    CnGEdges* cge = cngEdges;
    CnGEdges::iterator it;
    CnGRefs::iterator it1;
    uint32 nscan=0;
    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(cgn->cngNodeId))
            return;
    }
#ifdef _DEBUG
    if (_setState) {
        Log::out() <<"--scanAE: node "<<cgn->cngNodeId
            <<" opndId "<<cgn->opndId<<" state ";
        if (Log::isEnabled()) 
            printState(cgn);
        Log::out() <<" " << nodeTypeToString(cgn) <<cgn->nodeType<< std::endl;
    }
#endif
    if (cgn->nodeType&NT_OBJS)
        if (getEscState(cgn) >= ARG_ESCAPE) {
            setEscState(cgn,ARG_ESCAPE);
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"--scanAE: node "<<cgn->cngNodeId
                    <<" opndId "<<cgn->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(cgn);
                Log::out() <<" to arg.esc."<< std::endl;
            }
#endif
            if (cgn->nodeMDs==NULL) {
                Log::out() <<"--scanAE: node "<<cgn->cngNodeId
                    <<" opndId "<<cgn->opndId<<" state ";
                if (Log::isEnabled()) 
                    printState(cgn);
                Log::out() <<" " << nodeTypeToString(cgn) 
                    << cgn->nodeType << std::endl;
            }
            cgn->nodeMDs->push_back(curMDNode);
        }
    scannedObjs->push_back(cgn->cngNodeId);
    for (it = cge->begin( ); it != cge->end( ); it++ ) {
        if ((*it)->cngNodeFrom == cgn) {
            for (it1 = (*it)->refList->begin( ); it1 != (*it)->refList->end( ); it1++ ) {
                if ((*it1)->edgeType==ET_FIELD)  // do not scan field edge
                    continue;
                scanCnGNodeRefsAE(findCnGNode_id((*it1)->cngNodeTo->cngNodeId));
                if ((*it1)->edgeType!=ET_POINT)
                    nscan++;
            }
        }
    }
    if (cgn->nodeType==NT_ARRELEM && nscan==0) {
        Inst* inst_addindex = ((Opnd*)(cgn->refObj))->getInst();
        Inst* inst_ldbase = inst_addindex->getSrc(0)->getInst();
        if (inst_addindex->getOpcode() == Op_LdArrayBaseAddr)
            inst_ldbase = inst_addindex;
        CnGNode* cgnode=findCnGNode_op(inst_ldbase->getSrc(0)->getId());
#ifdef _DEBUG
        if (_setState) {
            Log::out() <<"--scanAE check1: ";
            if (Log::isEnabled())  
                inst_ldbase->print(Log::out()); 
            Log::out() << std::endl;
            Log::out() <<"--scanAE check2: ";
            if (cgnode==NULL)  
                Log::out() <<"cngnode==null "<< std::endl;
            Inst* inst_addindex= ((Opnd*)(cgn->refObj))->getInst();
            Log::out() <<"--scanAE arrElem: ";
            if (Log::isEnabled())  
                inst_addindex->print(Log::out()); 
            Log::out() << std::endl;
            Log::out() <<"--scanAE arrElem: ";
            if (Log::isEnabled())  
                inst_ldbase->print(Log::out()); 
            Log::out() << std::endl;
            Log::out() <<"--scanAE next: " << cgnode->cngNodeId << " - " 
                << cgnode->opndId << std::endl;
        }
#endif
        scanCnGNodeRefsAE(cgnode);
    }        
    if (cgn->nodeType==NT_LDOBJ && nscan==0) {
        Inst* ldinst =((Opnd*)(cgn->refObj))->getInst();
        if (ldinst->getOpcode()==Op_TauLdInd) {
            CnGNode* cgnode=findCnGNode_op(ldinst->getSrc(0)->getId());
#ifdef _DEBUG
            if (_setState) {
                Log::out() <<"TO FIND for opnd "
                    << cgn->opndId << std::endl;
                if (Log::isEnabled()) 
                    ldinst->print(Log::out());
                Log::out() << std::endl;
                Log::out() << cgn->cngNodeId << "-"
                    << cgn->opndId << " -> ";
                Log::out() << cgnode->cngNodeId << "-"
                    << cgnode->opndId << std::endl;
            }
#endif
            scanCnGNodeRefsAE(cgnode);
        }
    }
    if ((cgn->nodeType==NT_INSTFLD) && nscan==0) { //????????
        if (getEscState(cgn) > ARG_ESCAPE) 
            setEscState(cgn,ARG_ESCAPE);  // for instance fields of method parameters & array elements
    }
    scannedObjs->pop_back();
}  // scanCnGNodeRefsAE(CnGNode* cgn) 


bool 
EscAnalyzer::checkScannedObjs(uint32 id) {
    ObjIds::iterator it;
    for (it = scannedObjs->begin( ); it != scannedObjs->end( ); it++ ) {
        if ((*it)==id) {
            return true;
        }
    }
    return false;
}  // checkScannedObjs(CnGNode* cgn)


bool 
EscAnalyzer::checkScannedInsts(uint32 id) {
    ObjIds::iterator it;
    for (it = scannedInsts->begin( ); it != scannedInsts->end( ); it++ ) {
        if ((*it)==id) {
            return true;
        }
    }
    return false;
}  // checkScannedInsts(CnGNode* cgn)


bool 
EscAnalyzer::checkScannedSucNodes(uint32 id) {
    ObjIds::iterator it;
    for (it = scannedSucNodes->begin( ); it != scannedSucNodes->end( ); it++ ) {
        if ((*it)==id) {
            return true;
        }
    }
    return false;
}  // checkScannedSucNodes(CnGNode* cgn)


void
EscAnalyzer::printRefInfo(::std::ostream& os) {
    CnGNodes::iterator it;
    os << "================ Static Fields" << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_STFLD) {
            printCnGNodeRefs(*it, "", os);
        }
    }
    scannedObjs->clear();          
    os << "================ Method Agruments" << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_ACTARG) {
            printCnGNodeRefs(*it, "", os);
        }
    }
    scannedObjs->clear();          
    os << "================ Return Values" << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_EXITVAL) {
            printCnGNodeRefs(*it, "", os);
        }
    }
    scannedObjs->clear();          
    os << "================ Thrown Values" << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_THRVAL) {
            printCnGNodeRefs(*it, "", os);
        }
    }
    scannedObjs->clear();          
    os << "================ Instsnce Fields" << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==NT_INSTFLD) {
            printCnGNodeRefs(*it, "", os);
        }
    }
    scannedObjs->clear();          
}  // printRefInfo(::std::ostream& os)


void
EscAnalyzer::what_inst(Inst* inst,::std::ostream& os) {
    if (inst->asCallInst())
        os << "        CallInst" << std::endl;
    if (inst->asFieldAccessInst()) {
        os << "        FieldAccessInst" << std::endl;
        FieldAccessInst* fai=inst->asFieldAccessInst();
        FieldDesc* fd=fai->getFieldDesc();
        Type* tt=fd->getFieldType();
        os << "      isInitOnly        " << fd->isInitOnly() << std::endl;
        os << "      isVolatile        " << fd->isVolatile() << std::endl;
        os << "      isLiteral         " << fd->isLiteral() << std::endl;
        os << "      isUnmanagedStatic " << fd->isUnmanagedStatic() << std::endl;
        os << "      fldT      " << tt->getName() <<" "<< tt->tag<< std::endl;
        os << "      isObject  " << tt->isObject() << std::endl;
        os << "      isRef     " << tt->isReference()<< std::endl;
        if (tt->isReference()) {
            ref_type_info(tt,os);
        }
        os << "      getName    " << fd->getName() << std::endl;
        os << "      signature  " << fd->getSignatureString() << std::endl;
        os << "      parentType " << fd->getParentType()->getName() << std::endl;
        os << "      getId      " << fd->getId() << std::endl;
        os << "      isPrivate  " << fd->isPrivate() << std::endl;
        os << "      isStatic   " << fd->isStatic() << std::endl;
        os << "      ";
        fd->printFullName(os);
        os << std::endl;
    }
    if (inst->asIntrinsicCallInst())
        os << "        IntrinsicCallInst" << std::endl;
    if (inst->asMethodCallInst())
        os << "        MethodCallInst" << std::endl;
    if (inst->asMultiSrcInst())
        os << "        MultiSrcInst" << std::endl;
    if (inst->asVarAccessInst()) {
        os << "        VarAccessInst" << std::endl;
        if (inst->asVarAccessInst()->getVar()!=NULL) {
            os << "          Var:  ";
            inst->asVarAccessInst()->getVar()->print(os);os << std::endl;
            os << "          ";
            inst->asVarAccessInst()->getVar()->printWithType(os);os<< std::endl;
        }
        if (inst->asVarAccessInst()->getBaseVar()!=NULL) {
            os << "          BaseVar:  ";
            inst->asVarAccessInst()->getBaseVar()->print(os);os << std::endl;
            os << "          ";
            inst->asVarAccessInst()->getBaseVar()->printWithType(os);os << std::endl;
        }
    }
    if (inst->asBranchInst())
        os << "        BranchInst" << std::endl;
    if (inst->asCatchLabelInst())
        os << "        CatchLabelInst" << std::endl;
    if (inst->asConstInst())
        os << "        ConstInst" << std::endl;
    if (inst->asDispatchLabelInst())
        os << "        DispatchLabelInst" << std::endl;
    if (inst->asLabelInst())
        os << "        LabelInst" << std::endl;
    if (inst->asMethodEntryInst())
        os << "        MethodEntryInst" << std::endl;
    if (inst->asMethodInst()) {
        os << "        MethodInst" << std::endl;
        MethodDesc* md=inst->asMethodInst()->getMethodDesc();
        os << "          isNative " << md->isNative() << std::endl;
        os << "          isSynchronized " << md->isSynchronized() << std::endl;
        os << "          isStatic " << md->isStatic() << std::endl;
        os << "          isInstsnce " << md->isInstance() << std::endl;
        os << "          isFinal " << md->isFinal() << std::endl;
        os << "          isVirtual " << md->isVirtual() << std::endl;
        os << "          isAbstract " << md->isAbstract() << std::endl;
        os << "          isClassInitializer " << md->isClassInitializer() << std::endl;
        os << "          isInstanceInitializer " << md->isInstanceInitializer() << std::endl;
        os << "          isInitLocals " << md->isInitLocals() << std::endl;
        os << "          isOverridden " << md->isOverridden() << std::endl;
        os << "          isJavaByteCodes " << md->isJavaByteCodes() << std::endl;
        os << "          Name " << md->getName() << std::endl;
        os << "          Signature " << md->getSignatureString() << std::endl;
        MethodSignatureDesc* msd = md->getMethodSig();
        uint32 n=msd->getNumParams();
        os << "          Params " << n << std::endl;
        for (uint32 i = 0; i < n; i++) {
            Type* tt = msd->getParamType(i);
            os << "          << "<<i<<" >> " << tt->getName() <<" "<< tt->tag<< std::endl;
            os << "              isObject  " << tt->isObject();
            os << "  isRef  " << tt->isReference()<< std::endl;
            if (tt->isReference()) {
                ref_type_info(tt,os);
            }
        }
        os << "          Id " << md->getId() << std::endl;
        os << "          isPrivate " << md->isPrivate() << std::endl;
        os << "          ParentName " << md->getParentType()->getName();os << std::endl;
        md->printFullName(os);os << std::endl;
    }
    if (inst->asMethodMarkerInst())
       os << "        MethodMarkerInst" << std::endl;
    if (inst->asPhiInst())
       os << "        PhiInst" << std::endl;
    if (inst->asTauPiInst())
       os << "        TauPiInst" << std::endl;
    if (inst->asSwitchInst())
       os << "        SwitchInst" << std::endl;
    if (inst->asTokenInst())
       os << "        TokenInst" << std::endl;
    if (inst->asTypeInst()) {
       Type* tt = inst->asTypeInst()->getTypeInfo();
       os << "        TypeInst" << std::endl;
       os << "          "<< tt->getName() <<" "<< tt->tag<< std::endl;
    }
}  // what_inst(Inst* inst,::std::ostream& os) 


void
EscAnalyzer::ref_type_info(Type* type,::std::ostream& os) {
    NamedType* arreltype;
    os << "           isArr " << type->isArray();
    if (type->asArrayType()) {
        arreltype=type->asArrayType()->getElementType();
        os << " elmT " << arreltype->getName() << " " << arreltype->tag<<" ";
        os << " isRef " << arreltype->isReference();
    }
    os << " isArrElem " << type->isArrayElement()<< std::endl;
}  // ref_type_info(Type* type,::std::ostream& os) 


void
EscAnalyzer::debug_inst_info(Inst* inst,::std::ostream& os) {
    Opnd* dst;
    Opnd* src;
    uint32 nsrc;

    os << "  =="; inst->print(os); os << std::endl;
    os << "  Inst Info:" << std::endl;
    what_inst(inst,os);
    os << "  Dst & Src Info:" << std::endl;
    dst=inst->getDst();
    nsrc=inst->getNumSrcOperands();
    os << "  ";
    if (!dst->isNull())
        dst->print(os);
    else
        os << "dst NULL";
    os << "  --srcnum " << nsrc << std::endl;
    if (!dst->isNull()) {
        os << "  dst ";
        debug_opnd_info(dst, os);
    } else
        os << std::endl;
    if ( nsrc != 0 ) {
        os << "  sources" << std::endl;
        for (uint32 i=0; i<nsrc; i++) {
            src=inst->getSrc(i);
            os << "  <<" <<i<<">> ";
            debug_opnd_info(src, os);
        }
    }
}  // debug_inst_info(Inst* inst,::std::ostream& os) 


void
EscAnalyzer::debug_opnd_info(Opnd* opnd,::std::ostream& os) {
    Type* type=opnd->getType();

    opnd->print(os);
    os << " id. " <<opnd->getId();
    os << " type " << type->getName() << " " << type->tag<<" ";
    os << " isRef " << type->isReference();
    os << " isObj " << type->isObject() << std::endl;
    if (type->isReference()) 
        ref_type_info(type,os);
    os << "        ";
    opnd->printWithType(os );
    os << std::endl;
    os << "        prop " << opnd->getProperties();
    os << " isVar " << opnd->isVarOpnd();
    os << " isSsa " << opnd->isSsaOpnd();
    os << " isSsaVar " << opnd->isSsaVarOpnd();
    os << " isSsaTmp " << opnd->isSsaTmpOpnd();
    os << " isPi " << opnd->isPiOpnd() << std::endl;
    if (!opnd->isVarOpnd()) {
        os << "        ";
        opnd->getInst()->print(os);
        os << std::endl;
        what_inst(opnd->getInst(),os);
    }
}  // debug_opnd_info(Opnd* opnd,::std::ostream& os) 


EscAnalyzer::CnGNode*
EscAnalyzer::addCnGNode(Inst* inst, Type* type, uint32 ntype) {
    CnGNode* cgnode = new (eaMemManager) CnGNode; // new CG node

    cgnode->cngNodeId = ++lastCnGNodeId;
    cgnode->fgNode = inst->getNode();
    cgnode->instrId = inst->getId();
    cgnode->nodeType = ntype;
    cgnode->lNode = NULL;
    cgnode->nInst = inst;
    cgnode->outEdges = NULL;
    if (cgnode->nodeType==NT_DEFARG)
        cgnode->argNumber = defArgNumber;   // number of formal parameter
    else
        cgnode->argNumber = 0; 
    if ((ntype==NT_STFLD)||ntype==NT_THRVAL)
        setFullState(cgnode,GLOBAL_ESCAPE);
    else {
        if (ntype==NT_ACTARG)
            setFullState(cgnode,ARG_ESCAPE);
        else
            setFullState(cgnode,NO_ESCAPE);
    }
    if (ntype==NT_EXITVAL)
        setCalleeEscaped(cgnode);
    if (type->isArray()) {
        if (type->asArrayType()->isReference()) {
            cgnode->nodeRefType = NR_REFARR;
        } else
            cgnode->nodeRefType = NR_ARR;
    } else
        cgnode->nodeRefType = NR_REF;
    if (ntype&(NT_OBJECT|NT_RETVAL)||ntype==NT_LDOBJ||ntype==NT_VARVAL||ntype==NT_REF||ntype==NT_ARRELEM) { 
        cgnode->nodeMDs = new (eaMemManager) NodeMDs(eaMemManager);  // to collect methods receiving object
    } else 
        cgnode->nodeMDs = NULL;
    cngNodes->push_back(cgnode);
    return cgnode;
}  // addCnGNode(MemoryManager& mm, Type* type, Inst* inst, uint32 ntype) 


EscAnalyzer::CnGNode*
EscAnalyzer::addCnGNode_op(Inst* inst, Type* type, uint32 ntype) {
    CnGNode* cgnode = addCnGNode(inst, type, ntype); // new CG node
    Opnd* opnd = inst->getDst();

    cgnode->opndId = opnd->getId();
    cgnode->refObj = opnd;
    return cgnode;
}  // addCnGNode_op(MemoryManager& mm, Inst* inst, Type* type, uint32 ntype) 


EscAnalyzer::CnGNode*
EscAnalyzer::addCnGNode_mp(Inst* inst, MethodDesc* md, uint32 ntype, uint32 narg) {
    Type* type = md->getMethodSig()->getParamType(narg);
    CnGNode* cgnode = addCnGNode(inst, type, ntype); // new CG node

    cgnode->opndId = 0;
    cgnode->argNumber = narg;
    cgnode->refObj = md;
    return cgnode;
}  // addCnGNode_mp(MemoryManager& mm, Inst* inst, MethodDesc* md, uint32 ntype, uint32 narg) 


EscAnalyzer::CnGNode*
EscAnalyzer::addCnGNode_ex( Inst* inst, uint32 ntype) {
    Type* type = inst->getSrc(0)->getType();
    CnGNode* cgnode = addCnGNode(inst, type, ntype); // new CG node

    cgnode->opndId = 0;
    cgnode->refObj = inst->getSrc(0);   // returned or thrown operand
    return cgnode;
}  // addCnGNode_ex(MemoryManager& mm, Inst* inst, uint32 ntype) 


EscAnalyzer::CnGNode*
EscAnalyzer::addCnGNode_fl(Inst* inst, uint32 ntype) {
    Type* type = inst->getDst()->getType();
    CnGNode* cgnode = addCnGNode(inst, type, ntype); // new CG node

    cgnode->opndId = 0;
    cgnode->refObj = inst;   // returned or thrown operand
    return cgnode;
}  // addCnGNode_fl(MemoryManager& mm, Inst* inst, uint32 ntype) 


void 
EscAnalyzer::addInst(cfgNode* cfgn, Inst* inst) {
    cfgn->instructions->push_back(inst);
}  // addInst(cfgNode* cfgn, Inst* inst) 


void 
EscAnalyzer::addMonInst(Inst* inst) {
    uint32 monOpndId = inst->getSrc(0)->getId();
    MonUnit* monUnit = NULL;
    monUnit = findMonUnit(monOpndId);
    if (monUnit == NULL) {
        monUnit = new (eaMemManager) MonUnit;  // new monitor unit
        monUnit->opndId = monOpndId;
        monUnit->monInsts = new (eaMemManager) Insts(eaMemManager);
        monitorInstUnits->push_back(monUnit);
        monUnit->icallInsts = NULL;
    }
    monUnit->monInsts->push_back(inst);
}  // addMonInst(MemoryManager& memManager, Inst* inst) 


void 
EscAnalyzer::addMonUnitVCall(MonUnit* mu, Inst* inst) {
    if (mu->icallInsts == NULL) {
        mu->icallInsts = new (eaMemManager) Insts(eaMemManager);
    }
    mu->icallInsts->push_back(inst);
}  // addMonUnitVCall(MemoryManager& memManager, MonUnit* mu, Inst* inst) 


EscAnalyzer::MonUnit* 
EscAnalyzer::findMonUnit(uint32 opndId) {
    MonInstUnits::iterator it;
    assert(monitorInstUnits != NULL);
    for (it = monitorInstUnits->begin( ); it != monitorInstUnits->end( ); it++ ) {
        if ((*it)->opndId == opndId)
            return *it;
    }
    return NULL;
}


void 
EscAnalyzer::instrExam(cfgNode* cfgnode) {
    Node* node = cfgnode->fgNode;
    Inst *headInst = (Inst*)node->getFirstInst();
    int insnum=0;
    Type* type;
    CnGNode* cgnode;
    uint32 ntype=0;
    MethodDesc* md;
    MethodSignatureDesc* msd;
    uint32 n;
    bool addinst;
    Inst* method_inst;

    cfgnode->instructions=new (eaMemManager) Insts(eaMemManager);
    for (Inst* inst=headInst->getNextInst();inst!=NULL;inst=inst->getNextInst()) {
        insnum++;
        ntype=0;
        if (_instrInfo) {
            Log::out() <<"++Node Id."<<node->getId()<<"  "<<node->getDfNum()<<std::endl;
            Log::out() << "==";
            if (Log::isEnabled())  
                inst->print(Log::out()); 
            Log::out() << std::endl;
        }

        switch (inst->getOpcode()) {
            case Op_LdRef:        // ldstr
            case Op_NewObj:          // newobj
            case Op_NewArray:        // newarray
            case Op_NewMultiArray:   // newmultiarray
                ntype=NT_OBJECT;        // for 4 cases above
            case Op_LdConstant:      // ldc
                if (ntype==0) 
                    ntype=NT_LDOBJ;         // loads only ref null
            case Op_DefArg:          // defarg
                if (ntype==0) {
                    ntype=NT_DEFARG;    // for Op_DefArg
                    defArgNumber++;
                }
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (inst->getDst()->getType()->isObject()) {
                    type=inst->getDst()->getType();
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,ntype);
                }
                break;

            case Op_LdFieldAddr:     // ldflda
                ntype=NT_INSTFLD;
            case Op_LdStaticAddr:    // ldsflda
                if (ntype==0)
                    ntype=NT_STFLD;    // for LdStaticAddr
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                FieldAccessInst* fainst;
                if ((fainst=inst->asFieldAccessInst())!=NULL) {
                    type=fainst->getFieldDesc()->getFieldType();  //field type
                    if (type->isReference()) {
                        assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                        cgnode = addCnGNode_op(inst,type,NT_REF);
                        if (findCnGNode_fl(inst,ntype)==NULL) {
                            CnGNode* n = addCnGNode_fl(inst,ntype);
                            cgnode->lNode=n;   // stick nodes         // and if fl node exists ? bug?
                        }
                        addInst(cfgnode, inst);
                    }
                }
                break;

            case Op_TauLdInd:        // ldind
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (inst->getDst()->getType()->isObject()) {
                    type=inst->getDst()->getType();
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_LDOBJ);
                    addInst(cfgnode, inst);
                }
                break;

            case Op_LdArrayBaseAddr: // ldbase
            case Op_AddScaledIndex:  // addindex
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (inst->getDst()->getType()->isReference()) {
                    type=inst->getDst()->getType();
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_ARRELEM);
                    addInst(cfgnode, inst);
                }
                break;

            case Op_TauStInd:        // stind    
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (inst->getSrc(0)->getType()->isObject()) {
                    addInst(cfgnode, inst);
                }
                break;

            case Op_DirectCall:      // call
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (!inst->getDst()->isNull()) {
                    if (inst->getDst()->getType()->isObject()) {
                        type=inst->getDst()->getType();
                        assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                        cgnode = addCnGNode_op(inst,type,NT_RETVAL);
                    }
                }
                md=inst->asMethodInst()->getMethodDesc();
                msd = md->getMethodSig();
                n=msd->getNumParams();
                addinst=false;
                for (uint32 i = 0; i < n; i++) {
                    Type* tt = msd->getParamType(i);
                    if (!tt->isReference()) {
                        continue;
                    }
                    addinst=true;
                    assert(findCnGNode_mp(inst->getId(),i)==NULL);
                    cgnode = addCnGNode_mp(inst,md,NT_ACTARG, i);
                }
                if (addinst) {
                    addInst(cfgnode, inst);
                }
#ifdef _DEBUG  
                if (_seinfo) {
                    if (method_ea_level == 0) {
                        Log::out() <<"iE:        call        ";
                        if (md->isSynchronized())
                            Log::out() << "sync   "; 
                        if (md->isStatic())
                            Log::out() << "stat   ";
                        if (Log::isEnabled())  
                            inst->print(Log::out()); 
                        Log::out()<<std::endl;
                        Log::out() << "           ";
                        if (Log::isEnabled()) 
                            md->printFullName(Log::out()); 
                        Log::out() << std::endl;
                    }
                }
#endif
                break;

            case Op_IndirectMemoryCall:  //callimem
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (!inst->getDst()->isNull()) {
                    if (inst->getDst()->getType()->isObject()) {
                        type=inst->getDst()->getType();
                        assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                        cgnode = addCnGNode_op(inst,type,NT_RETVAL);
                    }
                }
                method_inst=inst->getSrc(0)->getInst();
                md=method_inst->asMethodInst()->getMethodDesc();
                msd = md->getMethodSig();
                n=msd->getNumParams();
                addinst=false;
                for (uint32 i = 0; i < n; i++) {
                    Type* tt = msd->getParamType(i);
                    if (!tt->isReference()) {
                        continue;
                    }
                    addinst=true;
                    assert(findCnGNode_mp(inst->getId(),i)==NULL);
                    cgnode = addCnGNode_mp(inst,md,NT_ACTARG,i);
                }
                if (addinst) {
                    addInst(cfgnode, inst);
                }
#ifdef _DEBUG 
                if (_seinfo) {
                    if (method_ea_level == 0) {
                        Log::out() <<"iE:        callimem    ";
                        if (md->isSynchronized())
                            Log::out() << "sync   "; 
                        if (md->isStatic())
                            Log::out() << "stat   ";
                        if (Log::isEnabled())  
                            inst->print(Log::out()); 
                        Log::out()<<std::endl;
                        Log::out() << "           ";
                        if (Log::isEnabled()) 
                            md->printFullName(Log::out()); 
                        Log::out() << std::endl;
                    }
                }
#endif
                break;

            case Op_Catch:           // catch
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (inst->getDst()->getType()->isObject()) {
                    type=inst->getDst()->getType();
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_CATCHVAL);
                }
                break;

            case Op_StVar:           // stvar
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                type = inst->getDst()->getType();
                if (type->isObject()) {
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_VARVAL);
                    addInst(cfgnode, inst);
                }    
                break;

            case Op_Phi:             // phi
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                type = inst->getDst()->getType();
                if (type->isReference()) {
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_VARVAL);
                    addInst(cfgnode, inst);
                }
                break;

            case Op_LdVar:             // ldvar
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                type = inst->getDst()->getType();
                if (type->isReference()) {
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_LDOBJ);
                    addInst(cfgnode, inst);
                }
                break;

            case Op_Return:          // return     
                ntype=NT_EXITVAL;
            case Op_Throw:           // throw
                if (ntype==0)
                    ntype=NT_THRVAL;
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                if (inst->getNumSrcOperands()>0) {
                    if (inst->getSrc(0)->getType()->isObject()) {
                        assert(findCnGNode_in(inst->getId())==NULL);
                        cgnode = addCnGNode_ex(inst,ntype);
                        addInst(cfgnode, inst);
                    }
                }
                break;

            case Op_TauStaticCast:   // staticcast
            case Op_TauCast:         // cast
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                type = inst->getDst()->getType();
                if (type->isObject()) {
                    assert(findCnGNode_op(inst->getDst()->getId())==NULL);
                    cgnode = addCnGNode_op(inst,type,NT_LDOBJ);
                    addInst(cfgnode, inst);
                }
                break;

            case Op_SaveRet:           // saveret
#ifdef _DEBUG
                if (_instrInfo) {
                    Log::out() <<"Node Id."<<node->getId()<<std::endl;
                    if (Log::isEnabled()) 
                        debug_inst_info(inst,Log::out()); 
                }
#endif
                type = inst->getDst()->getType();
                if (type->isIntPtr()) {
                    cgnode = findCnGNode_op(inst->getDst()->getId());
                    if (cgnode == NULL)
                        cgnode = addCnGNode_op(inst,type,NT_INTPTR);
                }
                break;

            case Op_TauMonitorEnter: // monenter
            case Op_TauMonitorExit:  // monexit
                addMonInst(inst);
#ifdef _DEBUG
                if (_seinfo) {
                    Log::out() << "iE: monX  Node Id."
                            << node->getId() << "  ";
                    if (Log::isEnabled()) 
                        inst->print(Log::out()); 
                    Log::out()<<std::endl;
                }
#endif
                break;

            case Op_TypeMonitorEnter:// tmonenter
            case Op_TypeMonitorExit: // tmonexit
                break;

            case Op_TauVirtualCall:  // callvirt
            case Op_IndirectCall:    // calli
            case Op_IntrinsicCall:   // callintr

            case Op_TauStRef:  
            case Op_TauStField:     
            case Op_TauStElem:     
            case Op_TauStStatic:
            case Op_Copy:       

            case Op_Box:

                if (_instrInfo) {
                    Log::out() <<"--Node Id."<<node->getId()<<std::endl;
                    Log::out() << "    =="; 
                    if (Log::isEnabled()) 
                        inst->print(Log::out()); 
                    Log::out() << std::endl;
                }
                break;

            default:

                if (_instrInfo) {
                    Log::out() <<"~~Node Id."<<node->getId()<<std::endl;
                    Log::out() << "    =="; 
                    if (Log::isEnabled()) 
                        inst->print(Log::out()); 
                    Log::out() << std::endl;
                }
        }
    }
    return;
}  // instrExam(cfgNode* cfgnode) 


void 
EscAnalyzer::instrExam2(cfgNode* node) {
    Insts *instrs = node->instructions;
    int insnum=0;
    Insts::iterator it;
    Inst* inst;
    Type* type;
    CnGNode* cgnode;
    CnGNode* cgn_src;
    uint32 ntype=0;
    MethodDesc* md;
    MethodSignatureDesc* msd;
    uint32 n;
    Inst* method_inst;
    bool not_exam = false;

    for (it = instrs->begin( ); it != instrs->end( ); it++ ) {
        inst=*it;
        insnum++;
        ntype=0;

#ifdef _DEBUG
        if (_instrInfo2) {
            Log::out() <<"instrExam2:  Node Id."<<node->fgNode->getId()<<std::endl;
            if (Log::isEnabled()) 
                debug_inst_info(inst,Log::out()); 
        }
#endif

        switch (inst->getOpcode()) {
            case Op_LdFieldAddr:     // ldflda
                ntype=NT_INSTFLD;
            case Op_LdStaticAddr:    // ldsflda
                if (ntype==0)
                    ntype=NT_STFLD;    // for LdStaticAddr
                FieldAccessInst* fainst;
                if ((fainst=inst->asFieldAccessInst())!=NULL) {
                    if (fainst->getFieldDesc()->getFieldType()->isReference()) {
                        cgn_src=findCnGNode_op(inst->getDst()->getId());  //  field address node
                        assert(cgn_src!=NULL);
                        cgnode=findCnGNode_fl(inst,ntype);    //  field node
                        assert(cgnode!=NULL);
                        //  adding edge from fld node to address node
                        if (ntype == NT_INSTFLD) {
                            cgn_src=findCnGNode_op(inst->getSrc(0)->getId());  // instance node
                            //  adding fld edge for ldflda
                            addEdge(cgn_src,cgnode,ET_FIELD,inst);
                            // special for java/lang/String::value
                            FieldDesc* fd=inst->asFieldAccessInst()->getFieldDesc();
                            if (fd->getParentType()->isSystemString()&&strcmp(fd->getName(),"value")==0) {
                                addEdge(cgnode,cgn_src,ET_DEFER,inst);
                            }
                        }
                    }
                }
                break;

            case Op_TauLdInd:        // ldind
                if (inst->getDst()->getType()->isObject()) {
                    cgnode=findCnGNode_op(inst->getDst()->getId());
                    assert(cgnode!=NULL);
                    // ref to loaded object
                    cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                    assert(cgn_src!=NULL);
                    addEdge(cgn_src,cgnode,ET_POINT,inst);  
                }
                break;

            case Op_LdArrayBaseAddr: // ldbase
            case Op_AddScaledIndex:  // addindex
                if (inst->getDst()->getType()->isReference()) {
                    cgnode=findCnGNode_op(inst->getDst()->getId());
                    assert(cgnode!=NULL);
                   // ref to loaded address
                    cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                    assert(cgn_src!=NULL);
                    if (inst->getOpcode()==Op_LdArrayBaseAddr) {
                        cgnode->lNode=cgn_src;  //stick array object & array base address nodes
                    } 
                    if (inst->getOpcode()==Op_AddScaledIndex) {
                        addEdge(cgn_src,cgnode,ET_FIELD,inst); // ref from array object to inner objects
                    }
                }
                break;

            case Op_TauStInd:        // stind    
                if (inst->getSrc(0)->getType()->isObject()) {
                    // ref to loaded address
                    cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                    assert(cgn_src!=NULL);
                    cgnode=findCnGNode_op(inst->getSrc(1)->getId());
                    assert(cgnode!=NULL);
                    addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                }
                break;

            case Op_DirectCall:      // call
                md=inst->asMethodInst()->getMethodDesc();
                msd = md->getMethodSig();
                n=msd->getNumParams();
                for (uint32 i = 0; i < n; i++) {
                    Type* tt = msd->getParamType(i);
                    if (!tt->isReference()) 
                        continue;
                    cgnode=findCnGNode_mp(inst->getId(),i);
                    assert(cgnode!=NULL);
                    cgn_src=findCnGNode_op(inst->getSrc(2+i)->getId());
                    assert(cgn_src!=NULL);
                    addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                }
                break;

            case Op_IndirectMemoryCall:  //callimem
                method_inst=inst->getSrc(0)->getInst();
                md=method_inst->asMethodInst()->getMethodDesc();
                msd = md->getMethodSig();
                n=msd->getNumParams();
                for (uint32 i = 0; i < n; i++) {
                    Type* tt = msd->getParamType(i);
                    if (!tt->isReference()) 
                        continue;
                    cgnode = findCnGNode_mp(inst->getId(),i);
                    assert(cgnode!=NULL);
                    cgn_src=findCnGNode_op(inst->getSrc(3+i)->getId());
                    assert(cgn_src!=NULL);
                    addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                }
                break;

            case Op_StVar:           // stvar
                if (inst->getDst()->getType()->isObject()) {
                    cgnode = findCnGNode_op(inst->getDst()->getId());
                    assert(cgnode!=NULL);
                    cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                    assert(cgn_src!=NULL);
                    addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                }
                break;

            case Op_Phi:             // phi
                if (inst->getDst()->getType()->isObject()) {
                    uint32 nsrc=inst->getNumSrcOperands();
                    cgnode = findCnGNode_op(inst->getDst()->getId());
                    assert(cgnode!=NULL);
                    for (uint32 i=0; i<nsrc; i++) {
                        cgn_src=findCnGNode_op(inst->getSrc(i)->getId());
                        assert(cgn_src!=NULL);
                        addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                    }
                }
                break;

            case Op_LdVar:             // ldvar
                type = inst->getDst()->getType();
                if (type->isReference()) {
                    cgnode = findCnGNode_op(inst->getDst()->getId());
                    assert(cgnode!=NULL);
                    cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                    assert(cgn_src!=NULL);
                    addEdge(cgn_src,cgnode,ET_DEFER,inst);   // load ldobj
                }
                break;

            case Op_Return:          // return     
                ntype=NT_EXITVAL;
            case Op_Throw:           // throw
                if (ntype==0)
                    ntype=NT_THRVAL;
                if (inst->getNumSrcOperands()>0) {
                    cgnode = findCnGNode_in(inst->getId());
                    assert(cgnode!=NULL);
                    cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                    assert(cgn_src!=NULL);
                    addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                }
                break;

            case Op_TauStaticCast:   // staticcast
            case Op_TauCast:         // cast
                type = inst->getDst()->getType();
                cgnode = findCnGNode_op(inst->getDst()->getId());
                assert(cgnode!=NULL);
                cgn_src=findCnGNode_op(inst->getSrc(0)->getId());
                assert(cgn_src!=NULL);
                addEdge(cgnode,cgn_src,ET_DEFER,inst); 
                break;

            case Op_TauMonitorEnter: // monenter
            case Op_TauMonitorExit:  // monexit
            case Op_TypeMonitorEnter:// tmonenter
            case Op_TypeMonitorExit: // tmonexit
                break;

            default:
                if (_instrInfo2) {
                    not_exam = true;
                }
        }
        if (_instrInfo2) {
            if (not_exam) {
                Log::out() <<"!!! Not examined."<<std::endl;
                not_exam = false;
            }
        }
    }
    return;
}  // instrExam2(cfgNode* node) 


void
EscAnalyzer::printCnGNode(CnGNode* cgn,::std::ostream& os) {
    std::string t2;

    os << "nodeId "<<cgn->cngNodeId<<"  ";
    if (cgn->nodeType & NT_OBJS) {   //node of object created in the method
        os << "opId "<<cgn->opndId<<"  ";
    } else {
        if (cgn->nodeType & NT_ACTARG) {   //node of actual method parameter
            os << "nArg "<<cgn->argNumber<<"  ";
            os << "method "<<cgn->refObj<<"  ";
        } 
    }
    if (cgn->nodeType==NT_DEFARG)
        os << "  nArg "<<cgn->argNumber<<"  ";    //Arg number for defarg
    os << "  inst "<<cgn->instrId<<"  ("<<nodeTypeToString(cgn)<<cgn->nodeType<<",  ";
    switch (cgn->nodeRefType) {
        case NR_REF    : t2="Ref -"; break;
        case NR_ARR    : t2="Arr -"; break;
        case NR_REFARR : t2="RArr-"; break;
        default        : t2="    -";
    }
    os <<t2<<cgn->nodeRefType<<")  ";
    if (cgn->lNode)
        os << "( " << cgn->lNode->cngNodeId << "-" << cgn->lNode->opndId << " )  ";
    os << "st. ";
    printState(cgn); os << "  ";
}  // printCnGNode(CnGNode* cgn,::std::ostream& os) 


std::string 
EscAnalyzer::nodeTypeToString(CnGNode* cgn) {
    std::string t1;
    switch (cgn->nodeType) {
        case NT_OBJECT  : t1="Obj -"; break;
        case NT_DEFARG  : t1="DArg-"; break;
        case NT_RETVAL  : t1="RVal-"; break;
        case NT_CATCHVAL: t1="CVal-"; break;
        case NT_STFLD   : t1="SFld-"; break;
        case NT_INSTFLD : t1="IFld-"; break;
        case NT_LDOBJ   : t1="LObj-"; break;
        case NT_INTPTR  : t1="IPtr-"; break;
        case NT_VARVAL  : t1="VVal-"; break;
        case NT_ARRELEM : t1="ArEl-"; break;
        case NT_REF     : t1="REF -"; break;
        case NT_ACTARG  : t1="AArg-"; break;
        case NT_EXITVAL : t1="EVal-"; break;
        case NT_THRVAL  : t1="TVal-"; break;
        default         : t1="    -";
    }
    return t1;
}  // nodeTypeToString(CnGNode* cgn) 


std::string 
EscAnalyzer::edgeTypeToString(CnGRef* edr) {
    std::string t1;
    switch (edr->edgeType) {
        case ET_POINT : t1="poi-"; break;
        case ET_DEFER : t1="ref-"; break;
        case ET_FIELD : t1="fld-"; break;
        default       : t1="   -";
    }
    return t1;
}  // edgeTypeToString(CnGRef* edr) 


void
EscAnalyzer::printCnGNodes(char* text,::std::ostream& os) {
    CnGNodes::const_iterator it;
    FieldDesc* fd;
    std::string t1;
    std::string t2;
    os << "    "<< text << std::endl;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        os <<"    ";
        printCnGNode(*it,os);
        os << std::endl;
        os << "                                ";
        if ((*it)->nodeType & (NT_OBJS|NT_EXITVAL)) {  //node of created or exit object
            ((Opnd*)(*it)->refObj)->printWithType(os);
        } 
        if ((*it)->nodeType == NT_RETVAL) {
            os << std::endl; os << "                                ";
            Inst* inst = ((Opnd*)(*it)->refObj)->getInst();
            inst->print(os);
            if (inst->getOpcode()==Op_IndirectMemoryCall) {
                MethodDesc* md = inst->getSrc(0)->getInst()->asMethodInst()->getMethodDesc();
                os << std::endl; os << "                                ";
                md->printFullName(os);
            }
        }
        if ((*it)->nodeType & NT_ACTARG) {    //node of actual method parameter
            os << ((MethodDesc*)(*it)->refObj)->getParentType()->getName() << "::";
            os << ((MethodDesc*)(*it)->refObj)->getName() << std::endl;
            os << "                                ";
            os << ((MethodDesc*)(*it)->refObj)->getMethodSig()->getParamType((*it)->opndId)->getName();
        }
        if ((*it)->nodeType & NT_STFLD) {    //field node 
            fd = ((Inst*)(*it)->refObj)->asFieldAccessInst()->getFieldDesc();
            os << fd->getParentType()->getName() << "::"<< fd->getName() << std::endl;
            os << "                                "<<fd->getFieldType()->getName();
        }
        os << std::endl;
    }
}  // printCnGNodes(char* text,::std::ostream& os) 


void
EscAnalyzer::printCnGNodeRefs(CnGNode* cgn, std::string text,::std::ostream& os) {
    CnGNode* node;
    CnGRefs::iterator it1;
    Inst* inst; 
    os << text;
    if (scannedObjs->size()!=0) {
        if (checkScannedObjs(cgn->cngNodeId)) {
            os << "nodeId " << cgn->cngNodeId << "  .  .  . " << std::endl;
            return;
        }
    }
    printCnGNode(cgn,os);
    os << std::endl;
    os << text; cgn->nInst->print(os); os << std::endl; 
    scannedObjs->push_back(cgn->cngNodeId);
    if (cgn->outEdges != NULL)
        for (it1 = cgn->outEdges->begin( ); it1 != cgn->outEdges->end( ); it1++ ) {
            os << text << edgeTypeToString(*it1) << std::endl;
            if ((node=findCnGNode_id((*it1)->cngNodeTo->cngNodeId))!=NULL)
                printCnGNodeRefs(node,text+"  ",os);
        } 
    scannedObjs->pop_back();
    if (cgn->nodeType==NT_RETVAL) {
        inst = cgn->nInst;
        if (inst->getOpcode()==Op_IndirectMemoryCall) {
            MethodDesc* md = inst->getSrc(0)->getInst()->asMethodInst()->getMethodDesc();
            os << text << "  ";         
            md->printFullName(os); os << std::endl;
        }
    }
    if (cgn->nodeType==NT_LDOBJ && getEscState(cgn)!=GLOBAL_ESCAPE) {
        inst = cgn->nInst;
        lObjectHistory(inst,text,os);
        scannedInsts->clear();
    }
}  // printCnGNodeRefs(CnGNode* cgn, std::string text,::std::ostream& os) 


void
EscAnalyzer::lObjectHistory(Inst* inst,std::string text,::std::ostream& os) {
    Inst* inst1;
    uint32 nsrc=inst->getNumSrcOperands();

    if (scannedInsts->size()!=0) {
        if (checkScannedInsts(inst->getId())) {
            os << text << "instId " << inst->getId() << "  .  .  . " << std::endl;
            return;
        }
    }
    os << text; inst->print(os); os << std::endl;
    if (inst->getOpcode()==Op_DirectCall || inst->getOpcode()==Op_IndirectMemoryCall) {
        Opnd *returnOpnd = inst->getDst(); 
        if (returnOpnd != NULL) {
            CnGNode* n = findCnGNode_op(returnOpnd->getId());
            if (n != NULL) {
                os<< text << "  "; printCnGNode(n,os); os<< std::endl;
            }
        }
        if (inst->getOpcode()==Op_IndirectMemoryCall) {
            MethodDesc* md = inst->getSrc(0)->getInst()->asMethodInst()->getMethodDesc();
            os << text << "  "; md->printFullName(os); os << std::endl;
        }
        return;
    }
    scannedInsts->push_back(inst->getId());
    for (uint32 i=0; i<nsrc; i++) {
        inst1 = inst->getSrc(i)->getInst();
        if (!(Type::isTau(inst->getSrc(i)->getType()->tag)))
            lObjectHistory(inst1,text+" ",os);
    }
}  // lObjectHistory(Inst* inst,std::string text,::std::ostream& os) 


void
EscAnalyzer::printCnGEdges(char* text,::std::ostream& os) {
    CnGEdges* cge = cngEdges;
    CnGEdges::iterator it;
    CnGRefs::iterator it1;
    std::string t1;
    std::string t2;
    os << "    "<< text << std::endl;
    if (cge==NULL) {
        os <<"    NULL"<< std::endl;
        return;
    }
    for (it = cge->begin( ); it != cge->end( ); it++ ) {
        os << "    from ";
        printCnGNode((*it)->cngNodeFrom,os);
        os << "  to " << std::endl;
        for (it1 = (*it)->refList->begin( ); it1 != (*it)->refList->end( ); it1++ ) {
            os << "      ";
            os <<(*it1)->cngNodeTo->cngNodeId<<" (";
            switch ((*it1)->edgeType) {
                case ET_POINT : t1="poi-"; break;
                case ET_DEFER : t1="ref-"; break;
                case ET_FIELD : t1="fld-"; break;
                default       : t1="   -";
            }
            os <<t1<<(*it1)->edgeType<<"), ";
            printCnGNode(findCnGNode_id((*it1)->cngNodeTo->cngNodeId),os);
            os << std::endl;
        }
    }
}  // printCnGEdges(char* text,::std::ostream& os) 


EscAnalyzer::CnGNode*
EscAnalyzer::findCnGNode_op(uint32 nId) {
    CnGNodes::iterator it;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->opndId==nId && ((*it)->nodeType & NT_OBJS))
            return (*it);
    }
    return(NULL);
}  // findCnGNode_op(uint32 nId) 


EscAnalyzer::CnGNode*
EscAnalyzer::findCnGNode_id(uint32 nId) {
    CnGNodes::iterator it;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->cngNodeId==nId)
            return (*it);
    }
    return(NULL);
}  // findCnGNode_id(uint32 nId) 


EscAnalyzer::CnGNode*
EscAnalyzer::findCnGNode_in(uint32 nId) {
    CnGNodes::iterator it;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->instrId==nId)
            return (*it);
    }
    return(NULL);
}  // findCnGNode_in(uint32 nId) 


EscAnalyzer::CnGNode*
//EscAnalyzer::findCnGNode_mp(MethodDesc* md, uint32 nId) {
EscAnalyzer::findCnGNode_mp(uint32 iId, uint32 aId) {
    CnGNodes::iterator it;
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->instrId==iId && (*it)->argNumber==aId &&
                (*it)->nodeType == NT_ACTARG)
            return (*it);
    }
    return(NULL);
}  // findCnGNode_mp(uint32 iId, uint32 aId) 


EscAnalyzer::CnGNode*
EscAnalyzer::findCnGNode_fl(Inst* inst, uint32 ntype) {
    CnGNodes::iterator it;
    FieldDesc* fd1;
    FieldDesc* fd2;
    uint32 idr = 0;
    if (ntype==NT_INSTFLD)
        idr=inst->getSrc(0)->getId();
    assert(inst->asFieldAccessInst()!=NULL);
    fd1 = inst->asFieldAccessInst()->getFieldDesc();
    for (it = cngNodes->begin( ); it != cngNodes->end( ); it++ ) {
        if ((*it)->nodeType==ntype) {
            assert(((Inst*)((*it)->refObj))->asFieldAccessInst()!=NULL);
            fd2 = ((Inst*)((*it)->refObj))->asFieldAccessInst()->getFieldDesc();
            if ( fd1->getParentType()==fd2->getParentType() && 
                    strcmp(fd1->getName(),fd2->getName())==0) {
                if (ntype==NT_INSTFLD) {
                    uint32 idf=((Inst*)((*it)->refObj))->getSrc(0)->getId();
                    if (idr!=idf) {
#ifdef _DEBUG
                        if (_cngedges) {
                            Log::out()
                                << "++++ findCnGNode_fl: required " << idr 
                                << " - found  " << idf << " not found" << std::endl;
                        }
#endif
                        continue;
                    }
#ifdef _DEBUG
                    if (_cngedges) {
                        Log::out()
                            << "++++ findCnGNode_fl: required " << idr
                            << " - found  " << idf << std::endl;
                    }
#endif
                }
                return (*it);
            }
        }
    }
    return(NULL);
}  // findCnGNode_fl(Opnd* opnd, uint32 ntype) 


/*
*   Creates edge if it doesn't exist yet.
*/
void
EscAnalyzer::addEdge(CnGNode* cgnfrom, CnGNode* cgnto, 
                                    uint32 etype, Inst* inst) {
    CnGEdges::iterator it;
    CnGRefs* el;
    CnGRef* ref;
    CnGNode* cgn1=cgnfrom;
    CnGNode* cgn2=cgnto;
    bool done = false;

    if (cgnfrom->lNode) {
        cgn1=findCnGNode_id(cgnfrom->lNode->cngNodeId);
        assert(cgn1!=NULL);
    }

    if (cgnto->lNode) {
        cgn2=findCnGNode_id(cgnto->lNode->cngNodeId);
        assert(cgn2!=NULL);
    }
#ifdef _DEBUG
    if (_cngedges) {
        Log::out() 
            << "++++ addEdge: " << cgnfrom->cngNodeId << "-" << cgnfrom->opndId
            << " ( "<<cgn1->cngNodeId << "-" << cgn1->opndId << " ) to "
            << cgnto->cngNodeId << "-" << cgnto->opndId << " ( "
            << cgn2->cngNodeId << "-" << cgn2->opndId << " )" << std::endl;
    }
#endif
    if (cgn1==cgn2) {
#ifdef _DEBUG
       if (_cngedges) {
            Log::out() << "+++++++ equal " 
                << cgnfrom->cngNodeId<< "-" << cgnfrom->opndId
                << " ( "<<cgn1->cngNodeId << "-" << cgn1->opndId << " ) to "
                << cgnto->cngNodeId << "-" << cgnto->opndId << " ( "
                << cgn2->cngNodeId << "-" << cgn2->opndId << " )" << std::endl;
        }
#endif
        return;
    }
    for ( it = cngEdges->begin( ); it != cngEdges->end( ); it++ ) {
        if ((*it)->cngNodeFrom == cgn1) {
            CnGRefs::iterator itr;
            if (etype==ET_FIELD || cgn1->nodeType==NT_ACTARG) {
                for ( itr = (*it)->refList->begin( ); itr != (*it)->refList->end( ); itr++ ) {
                    if ((*itr)->cngNodeTo == cgn2) {
#ifdef _DEBUG
                        if (_cngedges) {
                            Log::out() << "++++ addEdge: ET_FIELD || cgn1-> ==NT_ACTARG && *->cngNodeTo == cgn2" <<  std::endl;
                        }
#endif
                        return;
                    }
                }
            } else {
                for ( itr = (*it)->refList->begin( ); itr != (*it)->refList->end( ); itr++ ) 
                    if ((*itr)->cngNodeTo == cgn2) { // needed edge exists
                       done = true;
#ifdef _DEBUG
                       if (_cngedges) {
                           Log::out() << "++++ addEdge: needed edge exists" <<  std::endl;
                       }
#endif
                    }
            }
            if (!done) {
                ref = new (eaMemManager) CnGRef;
                ref->cngNodeTo=cgn2;
                ref->edgeType=etype;
                ref->edgeInst=inst;
                (*it)->refList->push_back(ref);
#ifdef _DEBUG
                if (_cngedges) {
                    Log::out() << "++++ addEdge: added CnGRef" <<  std::endl;
                }
#endif
            }
            return;
        } 
    }
    ref = new (eaMemManager) CnGRef;
    ref->cngNodeTo=cgn2;
    ref->edgeType=etype;
    ref->edgeInst=inst;
    CnGEdge* cgedge=new (eaMemManager) CnGEdge;
    el=new CnGRefs(eaMemManager);
    cgedge->cngNodeFrom=cgn1;
    el->push_back(ref);
    cgedge->refList=el;
    cngEdges->push_back(cgedge);
    cgn1->outEdges=el;
#ifdef _DEBUG
    if (_cngedges) {
        Log::out() << "++++ addEdge: added edge" <<  std::endl;
    }
#endif

}  // addEdge(CnGNode* cgnfrom, CnGNode* cgnto, uint32 etype, Inst* inst) 

} //namespace Jitrino 

