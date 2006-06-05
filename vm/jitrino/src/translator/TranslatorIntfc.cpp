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
#include "PropertyTable.h"
#include "Log.h"
#include "methodtable.h"
#include "CompilationContext.h"

namespace Jitrino {

void TranslatorIntfc::readFlagsFromCommandLine(CompilationContext* cs, bool ia32Cg)
{
    JitrinoParameterTable* params = cs->getThisParameterTable();
    TranslatorFlags& flags = *cs->getTranslatorFlags();
    flags.propValues = params->lookupBool("tra::propValues", true);
    bool defaultGenCharArrayCopy = ia32Cg ? false : true;
    flags.genCharArrayCopy = params->lookupBool("tra::genCharArrayCopy", 
                                                 defaultGenCharArrayCopy); 
    flags.onlyBalancedSync = params->lookupBool("tra::balancedSync", false);
                          
#ifdef _IPF_            
    flags.optArrayInit = params->lookupBool("tra::optArrayInit", false);
#else
    flags.optArrayInit = params->lookupBool("tra::optArrayInit", true);
#endif
    flags.ignoreSync       = params->lookupBool("tra::ignoreSync",false);
    flags.syncAsEnterFence = params->lookupBool("tra::syncAsEnterFence",false);
    flags.newCatchHandling = params->lookupBool("tra::newCatchHandling",true);
	flags.inlineMethods = params->lookupBool("tra::inlineMethods", true);
    
    flags.guardedInlining = params->lookupBool("tra::guardedInlining", true);
    //
    // see optimizer.cpp for additional info about the "client" option
    //
    if ( params->lookupBool("client", false) ) {
        flags.guardedInlining = false;
    }

    flags.magicMinMaxAbs = params->lookupBool("tra::magicMinMaxAbs", false);
    flags.genMinMaxAbs = params->lookupBool("tra::genMinMaxAbs", false);
    flags.genFMinMaxAbs = params->lookupBool("tra::genFMinMaxAbs", false);
    const char* skipMethods = params->lookup("opt::inline::skip_methods");
    if(skipMethods == NULL) {
        flags.inlineSkipTable = NULL;
    } else {
        flags.inlineSkipTable = new Method_Table(strdup(skipMethods), "SKIP_METHODS", true);
	}

    flags.magicClass=(char *)params->lookup("tra::useMagicMethods"); //NULL if not specified

}

void TranslatorIntfc::showFlagsFromCommandLine()
{
    Log::out() << "    tra::inlineMethods[={ON|off}] = bytecode inlining" << ::std::endl;
    Log::out() << "    tra::propValues[={ON|off}] =  propagate values during translation" << ::std::endl;
    Log::out() << "    tra::guardedInlining[={on|OFF}] = do guarded inlining during translation" << ::std::endl;
    Log::out() << "    tra::genCharArrayCopy[={on|off}] = generate intrinsic calls to char array copy" << ::std::endl;
    Log::out() << "    tra::useMagicMethods[={MagicClass name}] = use magic methods with magic class named MagicClass"<< ::std::endl;
    Log::out() << "    tra::balancedSync[={on|OFF}] = treat all synchronization as balanced" << ::std::endl;
    Log::out() << "    tra::ignoreSync[={on|OFF}] = do not generate synchronization" << ::std::endl;
    Log::out() << "    tra::syncAsEnterFence[={on|OFF}] = implement synchronization as monitor enter fence" << ::std::endl;
    Log::out() << "    tra::magicMinMaxAbs[={on|OFF}] = treat java/lang/Math::min and max as magic"<< ::std::endl;
    Log::out() << "    tra::genMinMaxAbs[={on|OFF}] = use special opcodes for Min/Max/Abs, rather than using Select"<< ::std::endl;
    Log::out() << "    tra::genFMinMaxAbs[={on|OFF}] = also use opcodes for float Min/Max/Abs, rather than using Select"<< ::std::endl;
}


void IRBuilder::readFlagsFromCommandLine(CompilationContext* cs)
{
    JitrinoParameterTable* params = cs->getThisParameterTable();
    IRBuilderFlags &irBuilderFlags = *cs->getIRBuilderFlags();
    
    //
    // IRBuilder expansion flags
    //
    irBuilderFlags.expandMemAddrs         = params->lookupBool("irb::expandMemAddrs", true);
    irBuilderFlags.expandElemAddrs        = params->lookupBool("irb::expandElemAddrs", true);
    irBuilderFlags.expandCallAddrs        = params->lookupBool("irb::expandCallAddrs", false);
    irBuilderFlags.expandVirtualCallAddrs = params->lookupBool("irb::expandVirtualCallAddrs", true);
    irBuilderFlags.expandNullChecks       = params->lookupBool("irb::expandNullChecks", true);
    irBuilderFlags.expandElemTypeChecks   = params->lookupBool("irb::expandElemTypeChecks", true);

    //
    // IRBuilder translation-time optimizations
    //
    irBuilderFlags.doCSE                  = params->lookupBool("irb::doCSE", true);
    irBuilderFlags.doSimplify             = params->lookupBool("irb::doSimplify", true);

    irBuilderFlags.suppressCheckBounds = params->lookupBool("irb::suppressCheckBounds", false);

	irBuilderFlags.insertMethodLabels     = params->lookupBool("irb::insertMethodLabels", true);
    irBuilderFlags.compressedReferences = params->lookupBool("irb::compressedReferences", false);

    irBuilderFlags.genMinMaxAbs = params->lookupBool("tra::genMinMaxAbs", false);
    irBuilderFlags.genFMinMaxAbs = params->lookupBool("tra::genFMinMaxAbs", false);

    irBuilderFlags.useNewTypeSystem = params->lookupBool("tra::useNewTypeSystem", false);
}

void IRBuilder::showFlagsFromCommandLine()
{
    //
    // IRBuilder expansion flags
    //
    Log::out() << "    irb::expandMemAddrs[={ON|off}] = ?" << ::std::endl;
    Log::out() << "    irb::expandElemAddrs[={ON|off}] = ?" << ::std::endl;
    Log::out() << "    irb::expandCallAddrs[={on|OFF}]  = ?" << ::std::endl;
    Log::out() << "    irb::expandVirtualCallAddrs[={ON|off}] = ?" << ::std::endl;
    Log::out() << "    irb::expandNullChecks[={ON|off}] = ?" << ::std::endl;
    Log::out() << "    irb::expandElemTypeChecks[={ON|off}] = ?" << ::std::endl;

    //
    // IRBuilder translation-time optimizations
    //
    Log::out() << "    irb::doCSE[={ON|off}] = ?" << ::std::endl;
    Log::out() << "    irb::doSimplify[={ON|off}] = ?" << ::std::endl;

    Log::out() << "    irb::suppressCheckBounds[={on|OFF}] = omit all bounds checks" << ::std::endl;

    Log::out() << "    irb::insertMethodLabels[={on|OFF}]  = ?" << ::std::endl;
    Log::out() << "    irb::compressedReferences[={on|OFF}] = force compressed references\n";
}

static void 
InitIRBuilderFlags(CompilationInterface& compilationInterface,
                   MethodDesc&  methodDesc) {
    //
    // Flags from compilation environment
    //
    IRBuilderFlags& irBuilderFlags = *compilationInterface.getCompilationContext()->getIRBuilderFlags();
    irBuilderFlags.insertWriteBarriers    = compilationInterface.insertWriteBarriers();
    irBuilderFlags.compressedReferences   = irBuilderFlags.compressedReferences || compilationInterface.areReferencesCompressed();
}

//
// this is the regular routine to be used to generate IR for a method
//
void
TranslatorIntfc::translateByteCodes(IRManager& irManager) {
    CompilationInterface& compilationInterface = irManager.getCompilationInterface();
    MethodDesc &methodDesc = irManager.getMethodDesc();

    //assert(flags);
    //assert(IRBuilder::flagsFromCommandLine);
    InitIRBuilderFlags(compilationInterface,irManager.getMethodDesc());

    IRBuilder irBuilder(irManager);
    if (methodDesc.isJavaByteCodes()) {
        JavaTranslator javaTranslator;
        javaTranslator.translateMethod(compilationInterface,
                                       methodDesc,
                                       irBuilder);
    } else {
		assert(false);
	}
}

//
// this is the routine to be used to generate IR for an inlined method
//
void
TranslatorIntfc::translateByteCodesInline(IRManager& irManager,
                                          uint32 numArgs, Opnd** argOpnds, uint32 inlineDepth) {
    CompilationInterface& compilationInterface = irManager.getCompilationInterface();
    MethodDesc& methodDesc = irManager.getMethodDesc();

    InitIRBuilderFlags(compilationInterface,methodDesc);

    IRBuilder irBuilder(irManager);

    if (methodDesc.isJavaByteCodes()) {
        Opnd* returnOpnd = irManager.getReturnOpnd();
        JavaTranslateMethodForIRInlining(
            compilationInterface,
            methodDesc,
            irBuilder,
            numArgs,
            argOpnds,
            returnOpnd ? &returnOpnd : 0,
            NULL,
            NULL,
            inlineDepth);
        if (returnOpnd != irManager.getReturnOpnd())
            irManager.setReturnOpnd(returnOpnd);
    } else {
        assert(0);
    }
}

//
// this routine is used for IR inlining, generating IR for a particular method
//
void
TranslatorIntfc::generateMethodIRForInlining(
         CompilationInterface& compilationInterface,
         MethodDesc& methodDesc,
         IRBuilder&        irBuilder,
         uint32            numActualArgs,
         Opnd**            actualArgs,
         Opnd**            returnOpnd,
         CFGNode**         returnNode,
         Inst*             inlineSite,
         uint32 inlineDepth) {

    if (methodDesc.isJavaByteCodes()) {
        JavaTranslateMethodForIRInlining(
                                         compilationInterface,
                                         methodDesc,
                                         irBuilder,
                                         numActualArgs,
                                         actualArgs,
                                         returnOpnd,
                                         returnNode,
                                         inlineSite,
                                         inlineDepth);
    } else {
        assert(0); 
    }
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
