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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.21.8.1.4.4 $
 *
 */

#include "Log.h"
#include "optpass.h"
#include "irmanager.h"
#include "Timer.h"
#include "Dominator.h"
#include "Loop.h"
#include "./ssa/SSA.h"
#include "Profiler.h"
#include "CompilationContext.h"
#include "optimizer.h"

namespace Jitrino {

void 
OptPass::run(IRManager& irm) 
{
	id=Log::getNextStageId();

	if (Log::cat_opt()->isIREnabled()){
		Log::printStageBegin(id, "HLO", getName(), getTagName());
	}

	Log::cat_opt()->info3 << indent(irm) << "Opt:   Running " << getName() << ::std::endl;
    printHIR(irm, genIRBefore(irm), "before");
    printDotFile(irm, genDotFileBefore(irm), "before");
    {
        CompilationMode mode = getCompilationMode(irm);
        PhaseTimer modeTimer(getModeTimer(mode), getModeTimerName(mode));
        PhaseTimer timer(getTimer(), getTimerName());
        _run(irm);
    }
    printHIR(irm, genIRAfter(irm), "after");
    printDotFile(irm, genDotFileAfter(irm), "after");

	if (Log::cat_opt()->isIREnabled()){
		Log::printStageEnd(id, "HLO", getName(), getTagName());
	}
}

void
OptPass::computeDominators(IRManager& irm) {
    DominatorTree* dominatorTree = irm.getDominatorTree();
    if(dominatorTree != NULL && dominatorTree->isValid()) {
        // Already valid.
        return;
    }
    Log::cat_opt()->info3 << indent(irm) << "Opt:   Compute Dominators" << ::std::endl;
    static Timer *computeDominatorsTimer = 0; // not thread-safe
    PhaseTimer t(computeDominatorsTimer, "opt::helper::computeDominators"); 
    DominatorBuilder db;
    dominatorTree = db.computeDominators(irm.getNestedMemoryManager(), &(irm.getFlowGraph()),false,true);
    irm.setDominatorTree(dominatorTree);
}


void
OptPass::computeLoops(IRManager& irm) {
    LoopTree* loopTree = irm.getLoopTree();
    if(loopTree != NULL && loopTree->isValid())
        // Already valid.
        return;
    Log::cat_opt()->info3 << indent(irm) << "Opt:   Compute Loop Tree" << ::std::endl;
    static Timer *computeLoopsTimer = 0; // not thread-safe
    PhaseTimer t(computeLoopsTimer, "opt::helper::computeLoops"); 
    assert(irm.getDominatorTree()->isValid());
    LoopBuilder lb(irm.getNestedMemoryManager(),
                   irm, *(irm.getDominatorTree()),false);
    loopTree = lb.computeAndNormalizeLoops();
    if (lb.needSsaFixup()) {
        fixupSsa(irm);
    }
    irm.setLoopTree(loopTree);
}

void
OptPass::computeDominatorsAndLoops(IRManager& irm) {
    computeDominators(irm);
    computeLoops(irm);
    computeDominators(irm);
}

void
OptPass::fixupSsa(IRManager& irm) {
    static Timer* fixupSsaTimer = 0;
    static uint32 globalSsaFixupCounter = 0;

    if(!irm.isSsaUpdated()) {
        PhaseTimer t(fixupSsaTimer, "opt::helper::fixupSsa");
        Log::cat_opt()->info3 << indent(irm) << "Opt:   SSA Fixup" << ::std::endl;
    
        computeDominators(irm);
        DominatorTree* dominatorTree = irm.getDominatorTree();
        FlowGraph& flowGraph = irm.getFlowGraph();
        MemoryManager& memoryManager = irm.getNestedMemoryManager();
        DomFrontier frontier(memoryManager,*dominatorTree,&flowGraph);
        SSABuilder ssaBuilder(irm.getOpndManager(),irm.getInstFactory(),frontier,&flowGraph, *irm.getCompilationContext()->getOptimizerFlags());
        bool better_ssa_fixup = irm.getParameterTable().lookupBool("opt::better_ssa_fixup", false);
 
        ssaBuilder.fixupSSA(irm.getMethodDesc(), better_ssa_fixup);
        globalSsaFixupCounter += 1;
        irm.setSsaUpdated();
    }

}

void
OptPass::splitCriticalEdges(IRManager& irm) {
    if(!irm.areCriticalEdgesSplit()) {
        irm.getFlowGraph().splitCriticalEdges(false);
        irm.setCriticalEdgesSplit();
    }
}

bool
OptPass::inDPGOMode(IRManager& irm) {
    return irm.getParameterTable().lookupBool("opt::use_profile", irm.getCompilationInterface().isDynamicProfiling() 
        || profileCtrl.useProf() || profileCtrl.getInstrumentGen());
}

CompilationMode
OptPass::getCompilationMode(IRManager& irm) {
    if ( irm.getCompilationContext()->getOptimizerFlags()->skip ) {
        return CM_NO_OPT;
    }
    MethodDesc& md = irm.getMethodDesc();

    switch(irm.getCompilationInterface().getOptimizationLevel()) {
    case 0:
		if (md.isClassInitializer()) {
			return CM_NO_OPT;
		} else {
			return CM_STATIC;
		}
    case 1:
        return inDPGOMode(irm) ? (profileCtrl.useProf() ? CM_DPGO2 : CM_DPGO1) : CM_STATIC;
    case 2:
        return CM_DPGO2;    
    default:
        assert(0);
        return CM_NUM_MODES;
    }
}

const char* 
OptPass::getCompilationModeName(CompilationMode mode) {
    switch(mode) {
    case CM_NO_OPT: return "noopt";
    case CM_STATIC: return "static";
    case CM_DPGO1: return "dpgo1";
    case CM_DPGO2: return "dpgo2";
    default: break;
    }
    assert(0);
    return NULL;
}

void
OptPass::initialize() {
    timer = NULL;
    timerName = ::std::string("opt::")+getTagName();
    for(uint32 i = 0; i < CM_NUM_MODES; ++i) {
        modeTimers[i] = NULL;
        modeTimerNames[i] = ::std::string("opt::")+getCompilationModeName((CompilationMode) i)+"::"+getTagName();
    }
}

const char*
OptPass::getModeTimerName(CompilationMode mode) {
    return modeTimerNames[mode].c_str();
}

Timer*&
OptPass::getTimer() {
    return timer;
}

Timer*&
OptPass::getModeTimer(CompilationMode mode) {
    return modeTimers[mode];
}

bool 
OptPass::isProfileConsistent(IRManager& irm) {
    char    methodStr[MaxMethodStringLength];
    genMethodString(irm.getMethodDesc(), methodStr, MaxMethodStringLength);
    return irm.getFlowGraph().isProfileConsistent(methodStr, profileCtrl.getDebugCtrl()>0);
}

void 
OptPass::smoothProfile(IRManager& irm) { 
    if (isProfileConsistent(irm) == false) {
        if (profileCtrl.getDebugCtrl() > 0)
            ::std::cerr << "Optimizer detects profile inconsistency! Ready for smoothing!" << ::std::endl;
        irm.getFlowGraph().smoothProfile(irm.getMethodDesc());
    }
}

bool OptPass::genIRBefore(IRManager& irm) {
    return Log::cat_opt()->isDebugEnabled();
}
bool OptPass::genIRAfter(IRManager& irm) {
    return Log::cat_opt()->isIR2Enabled();
}
bool OptPass::genDotFileBefore(IRManager& irm) {
    return false; 
}
bool OptPass::genDotFileAfter(IRManager& irm) {
    return false; 
}

void
OptPass::printHIR(IRManager& irm) {
    FlowGraph& flowGraph = irm.getFlowGraph();
    DominatorTree* dominatorTree = irm.getDominatorTree();
    LoopTree* loopTree = irm.getLoopTree();
    
    CFGNode::ChainedAnnotator annotator;
    CFGNode::ProfileAnnotator profileAnnotator;
    if(dominatorTree && dominatorTree->isValid())
        annotator.add(dominatorTree);
    if(loopTree && loopTree->isValid())
        annotator.add(loopTree);
    if(flowGraph.hasEdgeProfile()) {
        annotator.add(&profileAnnotator);
    }
    flowGraph.printInsts(Log::out(),irm.getMethodDesc(), &annotator);
}

void
OptPass::printHIR(IRManager& irm, bool condition, const char* when) {
    if(condition) {
		Log::printIRDumpBegin(id, getName(), when);
        printHIR(irm);
		Log::printIRDumpEnd(id, getName(), when);
    }
}

void
OptPass::printDotFile(IRManager& irm, const char* name) {
    FlowGraph& flowGraph = irm.getFlowGraph();
    DominatorTree* dominatorTree = irm.getDominatorTree();
    flowGraph.printDotFile(irm.getMethodDesc(), name, (dominatorTree && dominatorTree->isValid()) ? dominatorTree : NULL);
}

void OptPass::composeDotFileName(char * name, const char * suffix)
{
	name[0] = 0;
	strcat(name, getTagName());
	strcat(name, ".");
	char idString[10];
	sprintf(idString, "%d", (int)id);
	strcat(name, idString);
	strcat(name, ".");
	strcat(name, suffix);
}

void
OptPass::printDotFile(IRManager& irm, bool condition, const char* suffix) {
    if(condition) {
        assert(strlen(getTagName()) < 30);
        assert(strlen(suffix) < 30);
		char name[128];
		composeDotFileName(name, suffix);
        printDotFile(irm, name);
    }
}

const char*
OptPass::indent(IRManager& irm) { 
    return irm.getParent() == NULL ? "" : "    "; 
}

} //namespace Jitrino 
