/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @version $Revision: 1.7.8.1.4.4 $
 *
 */

#include "lazyexceptionopt.h"
#include "FlowGraph.h"
#include "irmanager.h"
#include "Opnd.h"
#include "Inst.h"
#include "Stl.h"
#include "Log.h"
#include "Dominator.h"
#include "optimizer.h"

namespace Jitrino {

DEFINE_SESSION_ACTION(LazyExceptionOptPass, lazyexc, "Lazy Exception Throwing Optimization")

void LazyExceptionOptPass::_run(IRManager& irm) {
    LazyExceptionOpt le(irm, irm.getMemoryManager());
    le.doLazyExceptionOpt();
}

int LazyExceptionOpt::level=0;

LazyExceptionOpt::LazyExceptionOpt(IRManager &ir_manager, MemoryManager& mem_manager) :
    irManager(ir_manager), memManager(mem_manager), 
    leMemManager(1024,"LazyExceptionOpt::doLazyExceptionOpt"),
    compInterface(ir_manager.getCompilationInterface()),nodeSet(NULL)
{
    if (compInterface.isBCMapInfoRequired()) {
        isBCmapRequired = true;
        MethodDesc* meth = compInterface.getMethodToCompile();
        bc2HIRMapHandler = new VectorHandler(bcOffset2HIRHandlerName, meth);
    } else {
        isBCmapRequired = false;
        bc2HIRMapHandler = NULL;
    }
}

void 
LazyExceptionOpt::doLazyExceptionOpt() {
    MethodDesc &md = irManager.getMethodDesc();
    BitSet excOpnds(leMemManager,irManager.getOpndManager().getNumSsaOpnds());
    StlDeque<Inst*> candidateSet(leMemManager);
    optCandidates = new (leMemManager) OptCandidates(leMemManager);
    CompilationInterface::MethodSideEffect m_sideEff = compInterface.getMethodHasSideEffect(&md); 

    const Nodes& nodes = irManager.getFlowGraph().getNodes();
    Nodes::const_iterator niter;

#ifdef _DEBUG
    mtdDesc=&md;
#endif

#ifdef _DEBUG
    if (Log::isEnabled()) {
        Log::out() << std::endl;
        for (int i=0; i<level; i++) Log::out() << " "; 
        Log::out() << "doLE "; md.printFullName(Log::out()); 
        Log::out() << " SideEff " << (int)m_sideEff << std::endl; 
    }
#endif

    level++;
    uint32 opndId = 0;
    isArgCheckNull = false;
    isExceptionInit = md.isInstanceInitializer() && 
            md.getParentType()->isLikelyExceptionType();
//  core api exception init
    if (m_sideEff == CompilationInterface::MSE_UNKNOWN && isExceptionInit 
            && strncmp(md.getParentType()->getName(),"java/lang/",10) == 0) {
        m_sideEff = CompilationInterface::MSE_NO;
        compInterface.setMethodHasSideEffect(&md,m_sideEff);
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "      core api exc "; md.printFullName(Log::out()); 
            Log::out() << " SideEff " << (int)m_sideEff << std::endl;
        }
#endif
    }

    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        Node* node = *niter;
        Inst *headInst = (Inst*)node->getFirstInst();
        for (Inst* inst=headInst->getNextInst();inst!=NULL;inst=inst->getNextInst()) {
#ifdef _DEBUG
            if (inst->getOpcode()==Op_DefArg && isExceptionInit) {
                if (Log::isEnabled()) {
                    Log::out() << "    defarg: "; 
                    inst->print(Log::out()); Log::out()  << std::endl;
                    Log::out() << "            "; 
                    Log::out() << (int)(inst->getDefArgModifier()) << " " <<
                    (inst->getDefArgModifier()==DefArgNoModifier) << " " <<
                    (inst->getDefArgModifier()==NonNullThisArg) << " " <<
                    (inst->getDefArgModifier()==SpecializedToExactType) << " " <<
                    (inst->getDefArgModifier()==DefArgBothModifiers) << std::endl;
                }
            }
#endif
            if (inst->getOpcode()==Op_Throw) {
                if (inst->getSrc(0)->getInst()->getOpcode()==Op_NewObj) {
                    excOpnds.setBit(opndId=inst->getSrc(0)->getId(),true);
                    if (addOptCandidates(opndId,inst))
                        excOpnds.setBit(opndId,false); // different exc. edges
#ifdef _DEBUG
                    if (excOpnds.getBit(opndId)==1) {
                        if (Log::isEnabled()) {
                            Log::out() << "      add opnd: "; 
                            inst->print(Log::out()); Log::out() << std::endl; 
                            Log::out() << "      add  obj: "; 
                            inst->getSrc(0)->getInst()->print(Log::out()); Log::out() << std::endl; 
                        }
                    }
#endif
                }
            }
            if (m_sideEff==0)
                if (instSideEffect(inst)) {
                    m_sideEff=compInterface.MSE_YES;
#ifdef _DEBUG
                    if (Log::isEnabled()) {
                        Log::out() << "~~~~~~inst sideEff "; 
                        inst->print(Log::out()); Log::out() << std::endl; 
                    }
#endif
                }
        }
    }
    if (compInterface.getMethodHasSideEffect(&md)==CompilationInterface::MSE_UNKNOWN) {
        if (m_sideEff == CompilationInterface::MSE_UNKNOWN)
            if (isExceptionInit && isArgCheckNull) {
#ifdef _DEBUG
                if (Log::isEnabled()) {
                    Log::out() << "~~~~~~init sideEff reset: " << m_sideEff << " 3 "; 
                    md.printFullName(Log::out()); Log::out() << std::endl; 
                }
#endif
                m_sideEff = CompilationInterface::MSE_NULL_PARAM;
            } else
                m_sideEff = CompilationInterface::MSE_NO;
        compInterface.setMethodHasSideEffect(&md,m_sideEff);
    } 

    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        Node* node = *niter;
        Inst *headInst = (Inst*)node->getFirstInst();
        Opnd* opnd;
        for (Inst* inst=headInst->getNextInst();inst!=NULL;inst=inst->getNextInst()) {
            uint32 nsrc = inst->getNumSrcOperands();
            for (uint32 i=0; i<nsrc; i++) {
                if (!(opnd=inst->getSrc(i))->isSsaOpnd())  // check ssa operands
                    continue;
                if (excOpnds.getBit(opndId=opnd->getId())==0) 
                    continue;
                if (inst->getOpcode()==Op_DirectCall) {
                    MethodDesc* md = inst->asMethodInst()->getMethodDesc();
                    if (md->isInstanceInitializer() &&
                        md->getParentType()->isLikelyExceptionType()) {
                        if (addOptCandidates(opndId,inst)) {
                            excOpnds.setBit(opndId,false);
#ifdef _DEBUG
                            if (Log::isEnabled()) {
                                Log::out() << "    - rem opnd " << opnd->getId() << " "; 
                                inst->print(Log::out()); Log::out() << std::endl; 
                            }
#endif
                        } 
                    } else {
                        excOpnds.setBit(opndId,false);
#ifdef _DEBUG
                        if (Log::isEnabled()) {
                            Log::out() << "   -- rem opnd " << opnd->getId() << " "; 
                            inst->print(Log::out()); Log::out() << std::endl; 
                        }
#endif
                    }
                } else {
                    if (inst->getOpcode()!=Op_Throw) {
                        excOpnds.setBit(opndId,false);
#ifdef _DEBUG
                        if (Log::isEnabled()) {
                            Log::out() << "      rem opnd " << opnd->getId() << " "; 
                            inst->print(Log::out()); Log::out() << std::endl; 
                        }
#endif
                    }
                }
            }
        }
    }
    if (!excOpnds.isEmpty()) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "------LE: "; 
            md.printFullName(Log::out()); Log::out() << std::endl;
        }
#endif
        fixOptCandidates(&excOpnds);
    }

level--;
#ifdef _DEBUG
    if (Log::isEnabled()) {
        for (int i=0; i<level; i++) Log::out() << " "; 
        Log::out() << "done "; md.printFullName(Log::out()); 
        Log::out() << " SideEff " << (int)m_sideEff << std::endl; 
    }
#endif
};

/*
*  Returns:
*      true, if throw cannot be optimized
*      false, otherwise
*/
bool 
LazyExceptionOpt::addOptCandidates(uint32 id, Inst* inst) {
    OptCandidate* oc = NULL;
    ThrowInsts* thrinst = NULL;
    OptCandidates::iterator it;
    if (optCandidates == NULL)
        optCandidates = new (leMemManager) OptCandidates(leMemManager);
    for (it = optCandidates->begin( ); it != optCandidates->end( ); it++ ) {
        if ((*it)->opndId==id) {
            oc = *it;
            break;
        }
    }
#ifdef _DEBUG
    if (Log::isEnabled()) {
        Log::out() << "    addOptCandidates: "; 
        inst->print(Log::out()); Log::out()  << std::endl;
    }
#endif
    if (oc == NULL) {
        if (inst->getOpcode()==Op_Throw) {
            bool hasFinalize =
                ((NamedType*)inst->getSrc(0)->getType())->isFinalizable();
            if (hasFinalize) {
#ifdef _DEBUG
                Log::out() << "    isFinalizable: "
                 << hasFinalize << std::endl;
#endif
                return true;
            }
        }
        oc = new (leMemManager) OptCandidate;
        oc->opndId = id;
        oc->objInst = inst->getSrc(0)->getInst();
        oc->initInst=NULL;
        thrinst = new (leMemManager) ThrowInsts(leMemManager);
        thrinst->push_back(inst);
        oc->throwInsts = thrinst;
        optCandidates->push_back(oc);
        if (!isEqualExceptionNodes(oc->objInst,inst)) {
            return true;
        }
    } else {
        if (inst->getOpcode()==Op_Throw) {
            oc->throwInsts->push_back(inst);
            return false;
        } else {
            assert(inst->getOpcode()==Op_DirectCall);
            assert(oc->initInst==NULL);
            oc->initInst = inst;
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "    addOptCandidates: call checkMC "; 
                inst->print(Log::out()); Log::out()  << std::endl;
            }
#endif
            uint32 nii_id=inst->getId()+1;
            ThrowInsts::iterator it1;
            for (it1 = oc->throwInsts->begin(); it1 !=oc->throwInsts->end(); it1++) {
                if ((*it1)->getId() != nii_id) {
#ifdef _DEBUG
                    if (Log::isEnabled()) {
                        Log::out() << "??  addOptCandidates: throw "; 
                        (*it1)->print(Log::out()); Log::out()  << std::endl;
                    }
#endif
                    if (checkInSideEff((*it1),inst))
                        return true;
                }
            }
            if (checkMethodCall(inst)) {
                return true;
            }
        }
    }
    return false;
};

bool 
LazyExceptionOpt::checkInSideEff(Inst* throw_inst, Inst* init_inst) {
    Node* node = throw_inst->getNode();
    Inst* instfirst = (Inst*)node->getFirstInst();;
    Inst* instlast = throw_inst;
    Inst* inst;
    bool dofind = true;
    bool inSE = false;
    if (throw_inst!=instfirst)
        instlast=throw_inst->getPrevInst();
    else {
        node = node->getInEdges().front()->getSourceNode();
        instlast = (Inst*)node->getLastInst();    
    } 
    while (dofind && node!=NULL) {
        instfirst = (Inst*)node->getFirstInst();
        for (inst = instlast; inst!=instfirst; inst=inst->getPrevInst()) {
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "      checkInSE: see "; 
                inst->print(Log::out()); Log::out() << std::endl; 
            }
#endif
            if (inst==init_inst) {
                dofind=false;
                break;
            }
            if (!inSE) {
                if (instSideEffect(inst)) {
                    inSE=true;
#ifdef _DEBUG
                    if (Log::isEnabled()) {
                        Log::out() << "      checkInSE: sideEff "; 
                        inst->print(Log::out()); Log::out() << std::endl; 
                    }
#endif
                    break;
                }
            }
        }
        if (dofind){
            node = node->getInEdges().front()->getSourceNode();
            instlast = (Inst*)node->getLastInst();
        }
    }
    if (dofind)
        return true; // call init wasn't found
    return inSE;
}

bool
LazyExceptionOpt::isEqualExceptionNodes(Inst* oi, Inst* ti) {
    Edge* oedge = oi->getNode()->getExceptionEdge();
    Edge* tedge = ti->getNode()->getExceptionEdge();
    if (oedge->getTargetNode()!=tedge->getTargetNode()) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    addOptCandidates: diff.exc.edges for obj&throw "; 
            Log::out() << oedge->getTargetNode()->getId() << "  ";
            Log::out() << tedge->getTargetNode()->getId() << std::endl;
        }
#endif
        return false;
    }
    return true;
}

void 
LazyExceptionOpt::printOptCandidates(::std::ostream& os) {
    OptCandidates::iterator it;
    Inst* oinst;
    Inst* iinst;
    Inst* tinst;

    if (optCandidates == NULL) {
        return;
    }
    for (it = optCandidates->begin( ); it != optCandidates->end( ); it++ ) {
        os << "~~  opndId " << (*it)->opndId << std::endl;
        oinst = (*it)->objInst; 
        os << "  obj       ";
        if (oinst != NULL)
            oinst->print(os);
        else
            os << "newobj NULL";
        os << std::endl;
        iinst = (*it)->initInst; 
        os << "  init      ";
        if (iinst != NULL)
            iinst->print(os);
        else
            os << "call init NULL";
        os << std::endl;
        if ((*it)->throwInsts == NULL) {
            os << "  thr        throw NULL";
            os << std::endl;
            continue;
        }
        ThrowInsts::iterator it1;
        for (it1 = (*it)->throwInsts->begin(); it1 !=(*it)->throwInsts->end(); it1++) {
            tinst = *it1;
            assert(tinst != NULL);
            os << "  thr       ";
            tinst->print(os);
            os << std::endl;
        }
    }
    os << "end~~" << std::endl;
}

void 
LazyExceptionOpt::fixOptCandidates(BitSet* bs) {
    OptCandidates::iterator it;
    Inst* oinst;
    MethodCallInst* iinst;
    Inst* tinst;
    Inst* tlinst;
    uint32 opcount;
    Opnd **opnds = NULL;

    if (optCandidates == NULL) {
        return;
    }
    for (it = optCandidates->begin( ); it != optCandidates->end( ); it++ ) {
        if (bs->getBit((*it)->opndId)) {
            oinst = (*it)->objInst; 
            assert(oinst != NULL);
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "  to remove ";
                oinst->print(Log::out());
                Log::out() << std::endl;
            }
#endif
            iinst = (*it)->initInst->asMethodCallInst(); 
            assert(iinst != NULL);
            // inline info from constructor should be propogated to lazy
            // exception if any
            InlineInfo* constrInlineInfo = iinst->getInlineInfoPtr();
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "  to remove ";
                iinst->print(Log::out());
                Log::out() << std::endl;
            }
#endif
            assert((*it)->throwInsts != NULL);
            assert(iinst->getNumSrcOperands() >= 3);
            if (!removeInsts(oinst,iinst))
                continue;   // to null bitset?
            TypeManager& tm = irManager.getTypeManager();
            Opnd* mpt = irManager.getOpndManager().createSsaTmpOpnd(
                        tm.getMethodPtrType(iinst->getMethodDesc()));
            opcount = iinst->getNumSrcOperands()-2;  //numSrc-3+1 
            if (opcount >0) {
                opnds = new (leMemManager) Opnd*[opcount];   //local mem should be used
                opnds[0] = mpt;
                for (uint32 i = 0; i < opcount-1; i++)
                    opnds[i+1] = iinst->getSrc(i+3);
            }
            Inst* mptinst = irManager.getInstFactory().makeLdFunAddr(mpt,iinst->getMethodDesc());
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "  1st      ";
                mptinst->print(Log::out());
                Log::out() << std::endl;
            }
#endif
            ThrowInsts::iterator it1;
            for (it1 = (*it)->throwInsts->begin(); it1 !=(*it)->throwInsts->end(); it1++) {
                tinst = *it1;
                assert(tinst != NULL);
                tlinst=irManager.getInstFactory().makeVMHelperCall(  
                        OpndManager::getNullOpnd(), ThrowLazy, opcount, 
                        opnds, constrInlineInfo);
#ifdef _DEBUG
                if (Log::isEnabled()) {
                    Log::out() << "  2nd      ";
                    tlinst->print(Log::out());
                    Log::out() << std::endl;
                }
                if (Log::isEnabled()) {
                    Log::out() << "  to change ";
                    tinst->print(Log::out());
                    Log::out() << std::endl;
                }
#endif
                mptinst->insertBefore(tinst); 
                tlinst->insertBefore(tinst);
                tinst->unlink();

                if (isBCmapRequired) {
                    uint64 bcOffset = ILLEGAL_VALUE;
                    uint64 instID = iinst->getId();
                    bcOffset = bc2HIRMapHandler->getVectorEntry(instID);
                    if (bcOffset != ILLEGAL_VALUE) {
                        bc2HIRMapHandler->setVectorEntry(mptinst->getId(), bcOffset);
                        bc2HIRMapHandler->setVectorEntry(tlinst->getId(), bcOffset);
                    }
                }
            }
            irManager.getFlowGraph().purgeEmptyNodes();
        }
    }
}

bool
LazyExceptionOpt::removeInsts(Inst* oinst,Inst* iinst) {
    ControlFlowGraph& fg = irManager.getFlowGraph();
    Edge* oedge = oinst->getNode()->getExceptionEdge();
    Edge* iedge = iinst->getNode()->getExceptionEdge();
    Node* otn = oedge->getTargetNode();
    Node* itn = iedge->getTargetNode();

    if (otn!=itn) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    removeInsts: diff.exc.edges for obj&init "; 
            Log::out() << otn->getId() << "  "  << itn->getId() << std::endl;
            Log::out() << "   "; oinst->print(Log::out()); 
            Log::out() << std::endl;
            Log::out() << "   "; iinst->print(Log::out()); 
            Log::out() << std::endl;
        }
#endif
        return false;
    }
    oinst->unlink();
    iinst->unlink();
    if (otn->getInEdges().size() > 1) {
        fg.removeEdge(oedge);
    } else
        removeNode(otn);
    if (itn->getInEdges().size() > 1) {
        fg.removeEdge(iedge);
    } else
        removeNode(itn);
    return true;
}

void 
LazyExceptionOpt::removeNode(Node* node) {
    const Edges &out_edges = node->getOutEdges();
    Edges::const_iterator eit;
    Node* n; 
    for (eit = out_edges.begin(); eit != out_edges.end(); ++eit) {
        n = (*eit)->getTargetNode();
        if (n->getInEdges().size() == 1)
            removeNode(n);
    }
    irManager.getFlowGraph().removeNode(node);
}

void 
LazyExceptionOpt::printInst1(::std::ostream& os, Inst* inst, std::string txt) {
    uint32 nsrc = inst->getNumSrcOperands();
    os << txt;
    inst->print(os);
    os << std::endl;
    for (uint32 i=0; i<nsrc; i++) {
        printInst1(os, inst->getSrc(i)->getInst(),txt+"  ");
    }

}

bool 
LazyExceptionOpt::checkMethodCall(Inst* inst) {
    uint32 opcode = inst->getOpcode();
    MethodDesc* cmd;
    CompilationInterface::MethodSideEffect mse;

    if (opcode==Op_DirectCall || opcode==Op_TauVirtualCall) {
        cmd = inst->asMethodCallInst()->getMethodDesc();
    } else {
        if (opcode==Op_IndirectCall || opcode==Op_IndirectMemoryCall) {
            cmd = inst->asCallInst()->getFunPtr()->getType()->asMethodPtrType()->getMethodDesc();
        } else {
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "    checkMC: no check "; 
                inst->print(Log::out()); Log::out()  << std::endl;
            }
#endif
            return true;
        }
    }
#ifdef _DEBUG
    if (Log::isEnabled()) {
        Log::out() << "    checkMC: "; 
        cmd->printFullName(Log::out()); Log::out() << std::endl;
    }
#endif
    
    mse=compInterface.getMethodHasSideEffect(cmd);
#ifdef _DEBUG
    if (mse!=CompilationInterface::MSE_UNKNOWN) {
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: prev.set sideEff " << mse << "  "; 
            inst->print(Log::out()); Log::out() << std::endl;
        }
    }
#endif
    if (mse==CompilationInterface::MSE_YES) {
        return true;
    }
    if (mse==CompilationInterface::MSE_NO) {
        return false;
    }
//  core api exception init
    if (cmd->isInstanceInitializer() && cmd->getParentType()->isLikelyExceptionType()
            && strncmp(cmd->getParentType()->getName(),"java/lang/",10) == 0) {
        compInterface.setMethodHasSideEffect(cmd,CompilationInterface::MSE_NO);
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: core api exc "; 
            inst->print(Log::out()); Log::out() << std::endl;
        }
#endif
        return false;
    }

    if ( opcode!=Op_DirectCall && !cmd->isFinal() ) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: not DirCall not final "; 
            inst->print(Log::out()); Log::out() << std::endl;
        }
#endif
        return true;
    }

    if (!isExceptionInit && 
        !(cmd->isInstanceInitializer()&&cmd->getParentType()->isLikelyExceptionType())) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: no init "; 
            Log::out() << isExceptionInit << " ";
            Log::out() << cmd->isInstanceInitializer() << " ";
            Log::out() << cmd->getParentType()->isLikelyExceptionType() << " ";
            inst->print(Log::out()); Log::out() << std::endl;
        }
#endif
        return true;
    }

    if (cmd->getParentType()->needsInitialization()) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: need cinit "; 
            inst->print(Log::out()); Log::out() << std::endl;
        }
#endif
        return true;  // cannot compile <init> before <clinit> (to fix vm)
    }
    if (compInterface.compileMethod(cmd)) {
        mse = compInterface.getMethodHasSideEffect(cmd);
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: method was compiled, sideEff " 
                << mse << std::endl;
        }
#endif
        if (mse==CompilationInterface::MSE_YES)
            return true;
        else {
            if (mse==CompilationInterface::MSE_NULL_PARAM) {
                uint32 nsrc=inst->getNumSrcOperands();
                Inst* src_inst;
                bool mayBeNull;
                if (nsrc>3) {
#ifdef _DEBUG
                    if (Log::isEnabled()) {
                        Log::out() << "    checkMC: exc.init "; 
                        inst->print(Log::out()); Log::out() << std::endl;
                    }
#endif
                    mayBeNull=false;
                    for (uint32 i=3; i<nsrc; i++) {
                        if (inst->getSrc(i)->getType()->isReference()) {
                            src_inst=inst->getSrc(i)->getInst();
                            if (mayBeNullArg(inst,src_inst))
                                mayBeNull=true;
                         }
                    }
                    if (!mayBeNull)
                        return false;
#ifdef _DEBUG
                    for (uint32 i=0; i<nsrc; i++) {
                        if (Log::isEnabled()) {
                            Log::out() << "        "<<i<<" isRef: "<<
                            inst->getSrc(i)->getType()->isReference()<<" "; 
                            inst->getSrc(i)->getInst()->print(Log::out()); 
                            Log::out() << std::endl;
                        }
                    }
#endif
                    return true;
                } 
#ifdef _DEBUG
                else {
                    if (Log::isEnabled()) {
                        Log::out() << " ?????? MSE_NULL_PARAM & nsrc "<<
                        nsrc << std::endl;
                    }
                }
#endif
            }
            return false;
        }
    } else {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkMC: method was not compiled " << std::endl;
        }
#endif
        return true;
    }
}

bool 
LazyExceptionOpt::mayBeNullArg(Inst* call_inst, Inst* src_inst) {
    uint32 mnid = irManager.getFlowGraph().getMaxNodeId();
    Node* node = call_inst->getNode();
    bool done = true;

    if (nodeSet == NULL) {
        nodeSet = new (leMemManager) NodeSet;
        nodeSet->nodes=new (leMemManager) BitSet(leMemManager,mnid);
    } else {
        nodeSet->nodes->clear();
    }
    nodeSet->arg_src_inst = src_inst;
    nodeSet->call_inst = call_inst;
    nodeSet->check_inst = NULL;
    nodeSet->reset_inst = NULL;

    done = checkArg(node);
#ifdef _DEBUG
    if (Log::isEnabled()) {
        Log::out() << "        mb0 done " << done << " nodes: " << std::endl; 
        for(uint32 i = 0; i < mnid; i++) {
            if (nodeSet->nodes->getBit(i)) {
                Log::out() << " " << i;
            }
        }
        Log::out() << std::endl; 
        Log::out() << "   arg   node: " << nodeSet->arg_src_inst->getNode()->getId() << std::endl; 
        Log::out() << "   call  node: " << nodeSet->call_inst->getNode()->getId() << std::endl; 
        if (nodeSet->check_inst)
        Log::out() << "   check node: " << nodeSet->check_inst->getNode()->getId() << std::endl; 
        if (nodeSet->reset_inst)
        Log::out() << "   reset node: " << nodeSet->reset_inst->getNode()->getId() << std::endl; 
    }
#endif 
    if (!done)
        return true;
    if (nodeSet->reset_inst)
        return true;
    if (nodeSet->check_inst==NULL && src_inst->getOpcode()==Op_Catch)
        return false;
    if (nodeSet->check_inst!=NULL && (nodeSet->check_inst->getNode()->getId() == 
            nodeSet->arg_src_inst->getNode()->getId()))
        return false;
    return true;
}

bool 
LazyExceptionOpt::checkArg(Node* nodeS) {
    Node* node = nodeS;
    Inst* instfirst = (Inst*)node->getFirstInst();
    Inst* instlast = (Inst*)node->getLastInst();
    Inst* inst;
    Opnd* arg_opnd = nodeSet->arg_src_inst->getDst();
    bool doneOK = true;
    bool dofind = true;

#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkArg: first node " << node->getId()
            << "  inEdges " << node->getInDegree() << "  " << std::endl;
        }
#endif 
 
    while (dofind && node!=NULL) {
        if ( nodeSet->nodes->getBit(node->getId()) ) {
            if (nodeSet->call_inst->getNode() == node) {
#ifdef _DEBUG
                if (Log::isEnabled()) {
                    Log::out() << "        node " << node->getId()
                    << " again in call_inst node " << std::endl; 
                }
#endif 
                doneOK = false;
            }
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "        node " << node->getId()
                << "  inEdges " << node->getInDegree() << " was scanned " << std::endl; 
            }
#endif 
            break;
        }
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "        node " << node->getId()
            << "  inEdges " << node->getInDegree() << std::endl; 
        }
#endif 
        for (inst = instlast; inst!=instfirst; inst=inst->getPrevInst()) {
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "          "; 
                inst->print(Log::out()); Log::out() << std::endl; 
            }
#endif
            if (inst==nodeSet->arg_src_inst) {
                dofind=false;
                break;
            }
            if (inst->getOpcode()==Op_TauCheckNull && inst->getSrc(0)==arg_opnd) {
                if (nodeSet->check_inst != NULL) {
                    dofind = false; 
                    doneOK = false; 
#ifdef _DEBUG
                    if (Log::isEnabled()) {
                        Log::out() << "  check_inst is not NULL" << std::endl; 
                    }
#endif
                }
                nodeSet->check_inst = inst;
                break;
            }
            if (inst->getDst()==arg_opnd) {
#ifdef _DEBUG
                if (nodeSet->reset_inst != NULL) {
                    if (Log::isEnabled()) {
                        Log::out() << "  reset_inst is not NULL" << std::endl; 
                    }
                }
#endif
                nodeSet->reset_inst=inst;
                dofind = false; 
                doneOK = false; 
            }
        }
        if (nodeSet->nodes->getBit(node->getId()) == 0) {
            nodeSet->nodes->setBit(node->getId(),true); 
        } 
        if (dofind) {
            if (node->getInDegree()==0) {
                dofind = false;
                break;
            }
            if (node->getInDegree()==1) {
                node = node->getInEdges().front()->getSourceNode();
                instfirst = (Inst*)node->getFirstInst();
                instlast = (Inst*)node->getLastInst();
            } else {
                const Edges &in_edges = node->getInEdges();
                Edges::const_iterator eit;
                for (eit = in_edges.begin(); eit != in_edges.end(); ++eit) {
                     if ( !(checkArg((*eit)->getSourceNode())) ) {
                         doneOK = false;
                         break;
                     }
                }
                dofind = false;
            }
        }
    }
//    if (nodeSet->reset_inst != NULL)
//        return false;
    return doneOK;
}

bool
LazyExceptionOpt::checkField(Inst* inst) {
    Opnd* insOp = inst->getSrc(0);
    Inst* instDef = insOp->getInst();
    if (instDef->getOpcode() == Op_DefArg) {
#ifdef _DEBUG
        if (Log::isEnabled()) {
            Log::out() << "    checkField: "; 
            inst->print(Log::out()); Log::out()  << std::endl;
            Log::out() << "    checkField: "; 
            instDef->print(Log::out()); Log::out()  << std::endl;
            Log::out() << "    checkField: "; 
            Log::out() << (int)(instDef->getDefArgModifier()) << " " <<
            (instDef->getDefArgModifier()==DefArgNoModifier) << " " <<
            (instDef->getDefArgModifier()==NonNullThisArg) << " " <<
            (instDef->getDefArgModifier()==DefArgBothModifiers) << std::endl;
        }
#endif
        if (instDef->getDefArgModifier()==NonNullThisArg && isExceptionInit)
            return false;
    }
    return true;
   
}

bool 
LazyExceptionOpt::instSideEffect(Inst* inst) {
    switch (inst->getOpcode()) {
        case Op_Add:
        case Op_Mul:
        case Op_Sub:
        case Op_TauDiv:
        case Op_TauRem:
        case Op_Neg:
        case Op_MulHi:
        case Op_Min:
        case Op_Max:
        case Op_Abs:
        case Op_And:
        case Op_Or:
        case Op_Xor:
        case Op_Not:
        case Op_Select:
            return false;
        case Op_Conv:
            return true;
        case Op_Shladd:
        case Op_Shl:
        case Op_Shr:
        case Op_Cmp:
        case Op_Cmp3:
        case Op_Branch:
        case Op_Jump:
        case Op_Switch:
            return false;
        case Op_DirectCall:
        case Op_TauVirtualCall:
        case Op_IndirectCall:
        case Op_IndirectMemoryCall:
#ifdef _DEBUG
            if (Log::isEnabled()) {
                Log::out() << "    instSideEffect: call checkMC "; 
                inst->print(Log::out()); Log::out()  << std::endl;
            }
#endif
            return checkMethodCall(inst);  
        case Op_IntrinsicCall:
        case Op_JitHelperCall:
        case Op_VMHelperCall:
            return true;
        case Op_Return:
        case Op_Catch:
            return false;
        case Op_Throw:
        case Op_ThrowSystemException:
        case Op_ThrowLinkingException:
            return true;
        case Op_Leave:            // deleted
        case Op_EndFinally:
        case Op_EndFilter:
        case Op_EndCatch:
            return true;
        case Op_JSR:              // deleted
        case Op_Ret:
        case Op_SaveRet:
            return true;
        case Op_Copy:
            return true;
        case Op_DefArg:
        case Op_LdConstant:
        case Op_LdRef:
        case Op_LdVar:    
        case Op_LdVarAddr:
        case Op_TauLdInd:
            return false;
        case Op_TauLdField:
            return true; 
        case Op_LdStatic:
            return true;
        case Op_TauLdElem:
             return false;
        case Op_LdFieldAddr: 
             return false;
        case Op_LdStaticAddr:
            return true;
        case Op_LdElemAddr:
            return false; //
        case Op_TauLdVTableAddr:
        case Op_TauLdIntfcVTableAddr:
        case Op_TauLdVirtFunAddr:
        case Op_TauLdVirtFunAddrSlot:
        case Op_LdFunAddr:
        case Op_LdFunAddrSlot:
        case Op_GetVTableAddr:
            return false;
        case Op_TauArrayLen:
        case Op_LdArrayBaseAddr:
        case Op_AddScaledIndex:
        case Op_ScaledDiffRef:
            return true;
        case Op_StVar:
            return true;
        case Op_TauStInd:
            {
                Inst* inst_src1 = inst->getSrc(1)->getInst();
#ifdef _DEBUG
                if (Log::isEnabled()) {
                    Log::out() << "    stind: "; 
                    inst->print(Log::out()); Log::out()  << std::endl;
                    Log::out() << "           "; 
                    inst_src1->print(Log::out()); Log::out()  << std::endl;
                }
#endif
                if (inst_src1->getOpcode()==Op_LdFieldAddr ) 
                    return checkField(inst_src1);
            }
            return true; 
        case Op_TauStField:
            return true; // 
        case Op_TauStElem:
        case Op_TauStStatic:
        case Op_TauStRef:
        case Op_TauCheckBounds:
        case Op_TauCheckLowerBound:
        case Op_TauCheckUpperBound:
            return true;
        case Op_TauCheckNull:
            {
                Inst* inst_src = inst->getSrc(0)->getInst();
#ifdef _DEBUG
                if (Log::isEnabled()) {
                    Log::out() << "    checknull: "; 
                    inst->print(Log::out()); Log::out()  << std::endl;
                    Log::out() << "               "; 
                    inst_src->print(Log::out()); Log::out()  << std::endl;
                }
#endif
                if (inst_src->getOpcode()==Op_DefArg && isExceptionInit) {
                    isArgCheckNull = true;
                    return false;
                }
            }
            return true; // 
        case Op_TauCheckZero:
        case Op_TauCheckDivOpnds:
        case Op_TauCheckElemType:
        case Op_TauCheckFinite:
            return true;
        case Op_NewObj:
// core api
            {
                NamedType* nt = inst->getDst()->getType()->asNamedType();
                if (strncmp(nt->getName(),"java/lang/",10)==0 && nt->isLikelyExceptionType()) {
#ifdef _DEBUG
                    if (Log::isEnabled()) {
                        Log::out() << "====newobj "; 
                        inst->print(Log::out()); Log::out()  << std::endl;
                        Log::out() << "core api exc " << nt->getName() << " "
                            << strncmp(nt->getName(),"java/lang/",10)
                            << " excType: " << nt->isLikelyExceptionType() << std::endl; 
                    }
#endif
                    return false;
                }
            }
            return true;
        case Op_NewArray:
        case Op_NewMultiArray:
        case Op_TauMonitorEnter:
        case Op_TauMonitorExit:
        case Op_TypeMonitorEnter:
        case Op_TypeMonitorExit:
        case Op_LdLockAddr:
        case Op_IncRecCount:
        case Op_TauBalancedMonitorEnter:
        case Op_BalancedMonitorExit:
        case Op_TauOptimisticBalancedMonitorEnter:
        case Op_OptimisticBalancedMonitorExit:
        case Op_MonitorEnterFence:
        case Op_MonitorExitFence:
            return true;
        case Op_TauStaticCast:
        case Op_TauCast:
        case Op_TauAsType:
        case Op_TauInstanceOf:
        case Op_InitType:
            return true;
        case Op_Label:
        case Op_MethodEntry:
        case Op_MethodEnd:
        case Op_SourceLineNumber:
            return false;                
        case Op_LdObj:
        case Op_StObj:
        case Op_CopyObj:
        case Op_InitObj:
        case Op_Sizeof:
        case Op_Box:
        case Op_Unbox:
        case Op_LdToken:
        case Op_MkRefAny:
        case Op_RefAnyVal:
        case Op_RefAnyType:
        case Op_InitBlock:
        case Op_CopyBlock:
        case Op_Alloca:
        case Op_ArgList:
            return true;
        case Op_Phi:
        case Op_TauPi:
            return false;
        case Op_IncCounter:
        case Op_Prefetch:
            return false;
        case Op_UncompressRef:
        case Op_CompressRef:
        case Op_LdFieldOffset:
        case Op_LdFieldOffsetPlusHeapbase:
        case Op_LdArrayBaseOffset:
        case Op_LdArrayBaseOffsetPlusHeapbase:
        case Op_LdArrayLenOffset:
        case Op_LdArrayLenOffsetPlusHeapbase:
        case Op_AddOffset:
        case Op_AddOffsetPlusHeapbase:
            return true;
        case Op_TauPoint:
        case Op_TauEdge:
        case Op_TauAnd:
        case Op_TauUnsafe:
        case Op_TauSafe:
            return false;
        case Op_TauCheckCast:
            return true;
        case Op_TauHasType:
        case Op_TauHasExactType:
        case Op_TauIsNonNull:
            return false;
        case Op_PredCmp:
        case Op_PredBranch:
            return false;
        default:
            return true;
    }
}

} //namespace Jitrino 

