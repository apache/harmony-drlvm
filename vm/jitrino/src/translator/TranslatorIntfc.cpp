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
 * @author Intel, George A. Timoshenko
 * @version $Revision: 1.26.8.2.4.4 $
 *
 */

#include <assert.h>

#include "MemoryManager.h"
#include "TranslatorIntfc.h"
#include "JavaTranslator.h"
#include "irmanager.h"
#include "IRBuilder.h"
#include "simplifier.h"
#include "Log.h"
#include "methodtable.h"
#include "CompilationContext.h"
#include "FlowGraph.h"

namespace Jitrino {


void TranslatorSession::run () {
    TranslatorAction* action = (TranslatorAction*)getAction();
    flags = action->getFlags();

    translate();
    postTranslatorCleanup();
}

// this is the regular routine to be used to generate IR for a method
void TranslatorSession::translate() {
    CompilationContext* cc = getCompilationContext();
    IRManager* irm = cc->getHIRManager();
    assert(irm);
    MethodDesc& methodDesc = irm->getMethodDesc();
    if (methodDesc.isJavaByteCodes()) {
        //create IRBuilder
        MemoryManager& mm = cc->getCompilationLevelMemoryManager();
        TranslatorAction* myAction = (TranslatorAction*)getAction();
        IRBuilder* irb = (IRBuilder*)myAction->getIRBuilderAction()->createSession(mm);
        irb->setCompilationContext(cc);
        MemoryManager tmpMM(1024, "IRBuilder::tmpMM");
        irb->init(irm, &flags, tmpMM);
        JavaTranslator::translateMethod(*cc->getVMCompilationInterface(), methodDesc, *irb);
    } else {
        assert(false);
    }
}


void TranslatorSession::postTranslatorCleanup() {
    IRManager* irm = getCompilationContext()->getHIRManager();
    ControlFlowGraph& flowGraph = irm->getFlowGraph();
    MethodDesc& methodDesc = irm->getMethodDesc();
    if (Log::isEnabled()) {
        Log::out() << "PRINTING LOG: After Translator" << std::endl;
        FlowGraph::printHIR(Log::out(), flowGraph, methodDesc);
    }
    FlowGraph::doTranslatorCleanupPhase(*irm);
    if (Log::isEnabled()) {
        Log::out() << "PRINTING LOG: After Cleanup" << std::endl;
        FlowGraph::printHIR(Log::out(), flowGraph,  methodDesc);
    }
}


static const char* help = \
    "  inlineMethods[={ON|off}] - bytecode inlining\n"\
    "  propValues[={ON|off}]    -  propagate values during translation\n"\
    "  guardedInlining[={on|OFF}]  - do guarded inlining during translation\n"\
    "  genCharArrayCopy[={on|off}] - generate intrinsic calls to char array copy\n"\
    "  genArrayCopy[={ON|off}] - inline java/lang/System::arraycopy call\n"\
    "  useMagicMethods[={MagicClass name}] - use magic methods with magic class named MagicClass\n"\
    "  balancedSync[={on|OFF}] - treat all synchronization as balanced\n"\
    "  ignoreSync[={on|OFF}]   - do not generate synchronization\n"\
    "  syncAsEnterFence[={on|OFF}] - implement synchronization as monitor enter fence\n"\
    "  magicMinMaxAbs[={on|OFF}] - treat java/lang/Math::min and max as magic\n"\
    "  genMinMaxAbs[={on|OFF}]   - use special opcodes for Min/Max/Abs\n"\
    "  genFMinMaxAbs[={on|OFF}]  - also use opcodes for float Min/Max/Abs\n";




static ActionFactory<TranslatorSession, TranslatorAction> _translator("translator", help);

void TranslatorAction::init() {
    readFlags();
    MemoryManager& mm = getJITInstanceContext().getGlobalMemoryManager();
    irbAction = (IRBuilderAction*)createAuxAction(mm, IRBUILDER_ACTION_NAME, "irbuilder");
    irbAction->init();
}    

void TranslatorAction::readFlags() {
    flags.propValues = getBoolArg("propValues", true);
#if defined(_IPF_)
    flags.genCharArrayCopy = getBoolArg("genCharArrayCopy", true); 
    flags.optArrayInit = getBoolArg("optArrayInit", false);
#else
    flags.genCharArrayCopy = getBoolArg("genCharArrayCopy", false); 
    flags.optArrayInit = getBoolArg("optArrayInit", true);
#endif
    flags.genArrayCopy = getBoolArg("genArrayCopy", true); 
    flags.onlyBalancedSync = getBoolArg("balancedSync", false);

    flags.ignoreSync       = getBoolArg("ignoreSync",false);
    flags.syncAsEnterFence = getBoolArg("syncAsEnterFence",false);
    flags.newCatchHandling = getBoolArg("newCatchHandling",true);

    flags.inlineMethods  = getBoolArg("inlineMethods", false);
    flags.guardedInlining = getBoolArg("guardedInlining", false);

    flags.magicMinMaxAbs = getBoolArg("magicMinMaxAbs", false);
    flags.genMinMaxAbs = getBoolArg("genMinMaxAbs", false);
    flags.genFMinMaxAbs = getBoolArg("genFMinMaxAbs", false);
 
    const char* skipMethods = getStringArg("skipMethods", NULL);
    if(skipMethods == NULL) {
        flags.inlineSkipTable = NULL;
    } else {
        flags.inlineSkipTable = new Method_Table(strdup(skipMethods), "SKIP_METHODS", true);
    }

    flags.magicClass=(char *)getStringArg("useMagicMethods", NULL);
}


OpndStack::OpndStack(MemoryManager& memManager,uint32 slots) 
    : maxSlots(slots) 
{
    opnds = new (memManager) Opnd*[maxSlots];
    tos = 0;
}


enum {
    IsNonNull      = 0x02,
    IsExactType    = 0x04,
    StackOpndAlive = 0x10,  // to get rid of phi nodes in the translator
    StackOpndSaved = 0x20   // to get rid of phi nodes in the translator
};
static bool isNonNull(uint32 flags)   {
    return (flags & IsNonNull) != 0; 
}
static bool isExactType(uint32 flags) {
    return (flags & IsExactType) != 0; 
}
static uint32 setNonNull(uint32 flags,bool val) { 
    return (val ? (flags | IsNonNull) : (flags & ~IsNonNull));
}
static uint32 setExactType(uint32 flags,bool val){ 
    return (val ? (flags | IsExactType) : (flags & ~IsExactType));
}
static bool isStackOpndAlive(uint32 flags) {
    return (flags & StackOpndAlive) != 0;
}
static bool isStackOpndSaved(uint32 flags) {
    return (flags & StackOpndSaved) != 0;
}
static uint32 setStackOpndAlive(uint32 flags,bool val) {
    return (val ? (flags | StackOpndAlive) : (flags & ~StackOpndAlive));
}

static uint32 setStackOpndSaved(uint32 flags,bool val) {
    return (val ? (flags | StackOpndSaved) : (flags & ~StackOpndSaved));
}


//
// utility methods to allow refactoring of Opnd.h
bool 
isNonNullOpnd(Opnd* opnd) {
    if (opnd->isVarOpnd()) {
        //
        // use the properties in Opnd
        //
        return isNonNull(opnd->getProperties());
    }
    return (Simplifier::isNonNullObject(opnd) ||
            Simplifier::isNonNullParameter(opnd));
}

bool 
isExactTypeOpnd(Opnd* opnd) {
    if (opnd->isVarOpnd()) {
        //
        // use the properties in Opnd
        //
        return isExactType(opnd->getProperties());
    }
    return Simplifier::isExactType(opnd);
}

bool
isStackOpndAliveOpnd(Opnd* opnd) {
    return isStackOpndAlive(opnd->getProperties());
}

bool
isStackOpndSavedOpnd(Opnd* opnd) {
    return isStackOpndSaved(opnd->getProperties());
}

void
setNonNullOpnd(Opnd* opnd,bool val) {
    if (opnd->isVarOpnd()) {
        //
        // use the properties in Opnd
        //
        uint32 props = opnd->getProperties();
        opnd->setProperties(setNonNull(props,val));
        return;
    }
}

void
setExactTypeOpnd(Opnd* opnd,bool val) {
    if (opnd->isVarOpnd()) {
        //
        // use the properties in Opnd
        //
        uint32 props = opnd->getProperties();
        opnd->setProperties(setExactType(props,val));
        return;
    }
}

void
setStackOpndAliveOpnd(Opnd* opnd,bool val) {
    //
    // use the properties in Opnd
    //
    uint32 props = opnd->getProperties();
    opnd->setProperties(setStackOpndAlive(props,val));
    return;
}

void
setStackOpndSavedOpnd(Opnd* opnd,bool val) {
    //
    // use the properties in Opnd
    //
    uint32 props = opnd->getProperties();
    opnd->setProperties(setStackOpndSaved(props,val));
    return;
}

} //namespace Jitrino 
